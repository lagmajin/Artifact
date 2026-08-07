module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <memory>
#include <limits>
#include <mutex>
#include <QVariant>
#include <QDebug>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module InvertEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

namespace Artifact {

namespace {
const char* kInvertHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer InvertParams : register(b0)
{
    float g_Channel;
    float g_Strength;
    float2 g_Pad;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = g_InputTexture[dtid.xy];
    const float strength = saturate(g_Strength);
    const int channel = (int)(g_Channel + 0.5f);
    if (channel == 0) {
        c.rgb = lerp(c.rgb, 1.0f - c.rgb, strength);
    } else if (channel == 1) {
        c.r = lerp(c.r, 1.0f - c.r, strength);
    } else if (channel == 2) {
        c.g = lerp(c.g, 1.0f - c.g, strength);
    } else if (channel == 3) {
        c.b = lerp(c.b, 1.0f - c.b, strength);
    } else {
        c.a = lerp(c.a, 1.0f - c.a, strength);
    }
    g_OutputTexture[dtid.xy] = c;
}

[numthreads(256, 1, 1)]
void main1D(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    const uint pixelCount = width * height;
    if (dtid.x >= pixelCount || width == 0) return;

    const uint2 pixel = uint2(dtid.x % width, dtid.x / width);
    float4 c = g_InputTexture[pixel];
    const float strength = saturate(g_Strength);
    const int channel = (int)(g_Channel + 0.5f);
    if (channel == 0) {
        c.rgb = lerp(c.rgb, 1.0f - c.rgb, strength);
    } else if (channel == 1) {
        c.r = lerp(c.r, 1.0f - c.r, strength);
    } else if (channel == 2) {
        c.g = lerp(c.g, 1.0f - c.g, strength);
    } else if (channel == 3) {
        c.b = lerp(c.b, 1.0f - c.b, strength);
    } else {
        c.a = lerp(c.a, 1.0f - c.a, strength);
    }
    g_OutputTexture[pixel] = c;
}
)";
} // namespace

class InvertEffectCPUImpl : public ArtifactEffectImplBase {
public:
    int channel_ = 0;
    float strength_ = 1.0f;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const int channel = std::clamp(channel_, 0, 4);
        const float strength = std::clamp(strength_, 0.0f, 1.0f);

        Parallel::For(0, height, width * height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                auto invertChannel = [strength](float v) {
                    return std::lerp(v, 1.0f - v, strength);
                };
                if (channel == 0) {
                    pixel[0] = invertChannel(pixel[0]);
                    pixel[1] = invertChannel(pixel[1]);
                    pixel[2] = invertChannel(pixel[2]);
                } else if (channel == 1) {
                    pixel[0] = invertChannel(pixel[0]);
                } else if (channel == 2) {
                    pixel[1] = invertChannel(pixel[1]);
                } else if (channel == 3) {
                    pixel[2] = invertChannel(pixel[2]);
                } else {
                    pixel[3] = invertChannel(pixel[3]);
                }
            }
        });
    }
};

class InvertEffectGPUImpl : public ArtifactEffectImplBase {
public:
    int channel_ = 0;
    float strength_ = 1.0f;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.channel_ = channel_;
        cpuImpl_.strength_ = strength_;
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src, dst); return; }
        struct SharedDeviceLease {
            ~SharedDeviceLease() { releaseSharedRenderDevice(); }
        } sharedDeviceLease;
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        if (!pixels || image.width() <= 0 || image.height() <= 0) { applyCPU(src, dst); return; }

        Diligent::TextureDesc desc{};
        desc.Name = "Invert/Input";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(image.width());
        desc.Height = static_cast<Diligent::Uint32>(image.height());
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData sub{};
        sub.pData = pixels;
        sub.Stride = static_cast<Diligent::Uint64>(image.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        Diligent::RefCntAutoPtr<Diligent::ITexture> input;
        device->CreateTexture(desc, &init, &input);
        if (!input) { applyCPU(src, dst); return; }

        Diligent::TextureDesc outDesc = desc;
        outDesc.Name = "Invert/Output";
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS;
        if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
            outputTex_->GetDesc().Height != outDesc.Height ||
            outputTex_->GetDesc().Format != outDesc.Format ||
            outputTex_->GetDesc().BindFlags != outDesc.BindFlags) {
            outputTex_.Release();
            device->CreateTexture(outDesc, nullptr, &outputTex_);
        }
        if (!outputTex_) { applyCPU(src, dst); return; }

        Diligent::BufferDesc cbDesc{};
        cbDesc.Name = "Invert/Params";
        cbDesc.Size = sizeof(Params);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> params;
        device->CreateBuffer(cbDesc, nullptr, &params);
        if (!params) { applyCPU(src, dst); return; }
        void* mapped = nullptr;
        context->MapBuffer(params, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        Params values{static_cast<float>(channel_), strength_};
        std::memcpy(mapped, &values, sizeof(values));
        context->UnmapBuffer(params, Diligent::MAP_WRITE);

        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "InvertParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

        ArtifactCore::GpuContext gpuContext{device, context};
        ArtifactCore::ComputeExecutor executor{gpuContext};
        const D3D12AgilityCapabilitySnapshot agilityCaps =
            sharedD3D12AgilityCapabilities();
        constexpr Diligent::Uint64 kThreadsPer1DGroup = 256;
        constexpr Diligent::Uint64 kLegacyMax1DGroups = 65535;
        const Diligent::Uint64 pixelCount =
            static_cast<Diligent::Uint64>(outDesc.Width) *
            static_cast<Diligent::Uint64>(outDesc.Height);
        const Diligent::Uint64 oneDimensionalGroupCount =
            (pixelCount + kThreadsPer1DGroup - 1) / kThreadsPer1DGroup;
        bool useExtended1DDispatch =
            agilityCaps.options22Available &&
            agilityCaps.deviceShaderModel69Supported &&
            agilityCaps.dxcShaderModel69Supported &&
            oneDimensionalGroupCount > kLegacyMax1DGroups &&
            oneDimensionalGroupCount <= agilityCaps.max1DDispatchSize &&
            oneDimensionalGroupCount <=
                static_cast<Diligent::Uint64>((std::numeric_limits<Diligent::Uint32>::max)());
        ArtifactCore::ComputePipelineDesc pipeline{};
        pipeline.name = useExtended1DDispatch ? "Invert/PSO-1D-SM69"
                                              : "Invert/PSO-2D";
        pipeline.shaderSource = kInvertHlsl;
        pipeline.entryPoint = useExtended1DDispatch ? "main1D" : "main";
        pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        pipeline.shaderCompiler = useExtended1DDispatch
            ? Diligent::SHADER_COMPILER_DXC
            : Diligent::SHADER_COMPILER_DEFAULT;
        pipeline.hlslVersion = useExtended1DDispatch
            ? Diligent::ShaderVersion{6, 9}
            : Diligent::ShaderVersion{};
        pipeline.variables = vars;
        pipeline.variableCount = 3;
        pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        bool pipelineBuilt = executor.build(pipeline);
        if (!pipelineBuilt && useExtended1DDispatch) {
            useExtended1DDispatch = false;
            pipeline.name = "Invert/PSO-2D-Fallback";
            pipeline.entryPoint = "main";
            pipeline.shaderCompiler = Diligent::SHADER_COMPILER_DEFAULT;
            pipeline.hlslVersion = {};
            pipelineBuilt = executor.build(pipeline);
        }
        if (!pipelineBuilt ||
            !executor.createShaderResourceBinding(true) ||
            !executor.setBuffer("InvertParams", params) ||
            !executor.setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor.setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        const Diligent::DispatchComputeAttribs dispatch = useExtended1DDispatch
            ? Diligent::DispatchComputeAttribs{
                  static_cast<Diligent::Uint32>(oneDimensionalGroupCount), 1, 1}
            : ArtifactCore::ComputeExecutor::makeDispatchAttribs(
                  outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        if (useExtended1DDispatch) {
            static std::once_flag extendedDispatchLogFlag;
            std::call_once(extendedDispatchLogFlag, [&]() {
                qInfo() << "[InvertEffect] Agility extended 1D dispatch enabled"
                        << "groups=" << dispatch.ThreadGroupCountX
                        << "pixels=" << pixelCount
                        << "limit=" << agilityCaps.max1DDispatchSize;
            });
        }
        executor.dispatch(context, dispatch,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::TextureDesc stagingDesc = outDesc;
        stagingDesc.Name = "Invert/Readback";
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.BindFlags = Diligent::BIND_NONE;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        if (!stagingTex_ || stagingTex_->GetDesc().Width != stagingDesc.Width ||
            stagingTex_->GetDesc().Height != stagingDesc.Height ||
            stagingTex_->GetDesc().Format != stagingDesc.Format) {
            stagingTex_.Release();
            device->CreateTexture(stagingDesc, nullptr, &stagingTex_);
        }
        if (!stagingTex_) { applyCPU(src, dst); return; }
        Diligent::CopyTextureAttribs copy(outputTex_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          stagingTex_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CopyTexture(copy);
        context->Flush();
        context->WaitForIdle();
        Diligent::MappedTextureSubresource read{};
        context->MapTextureSubresource(stagingTex_, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, read);
        if (!read.pData || !read.Stride) { applyCPU(src, dst); return; }
        cv::Mat result(static_cast<int>(outDesc.Height), static_cast<int>(outDesc.Width), CV_32FC4, read.pData, read.Stride);
        dst.image().setFromCVMat(result, image.colorDescriptor());
        context->UnmapTextureSubresource(stagingTex_, 0, 0);
    }

private:
    struct Params { float channel, strength, pad0 = 0.0f, pad1 = 0.0f; };
    InvertEffectCPUImpl cpuImpl_;
};


InvertEffect::InvertEffect() {
    setEffectID(UniString("effect.colorcorrection.invert"));
    setDisplayName(UniString("Invert"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<InvertEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<InvertEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

InvertEffect::~InvertEffect() = default;

void InvertEffect::setChannel(int value) {
    channel_ = std::clamp(value, 0, 4);
    syncImpls();
}

void InvertEffect::setStrength(float value) {
    strength_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    syncImpls();
}

void InvertEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<InvertEffectCPUImpl*>(cpuImpl().get())) {
        cpu->channel_ = channel_;
        cpu->strength_ = strength_;
    }
    if (auto* gpu = dynamic_cast<InvertEffectGPUImpl*>(gpuImpl().get())) {
        gpu->channel_ = channel_;
        gpu->strength_ = strength_;
    }
}

std::vector<AbstractProperty> InvertEffect::getProperties() const {
    std::vector<AbstractProperty> props(2);
    props[0].setName("Channel");
    props[0].setType(PropertyType::Integer);
    props[0].setValue(QVariant(channel()));
    props[0].setMinValue(0);
    props[0].setMaxValue(4);
    props[1].setName("Strength");
    props[1].setType(PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(strength_)));
    props[1].setMinValue(0.0);
    props[1].setMaxValue(1.0);
    return props;
}

void InvertEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Channel") {
        setChannel(value.toInt());
    } else if (name == "Strength") {
        setStrength(value.toFloat());
    }
}

} // namespace Artifact

