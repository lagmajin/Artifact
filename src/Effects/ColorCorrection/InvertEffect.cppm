module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <memory>
#include <QVariant>
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
)";
} // namespace

class InvertEffectCPUImpl : public ArtifactEffectImplBase {
public:
    int channel_ = 0;
    float strength_ = 1.0f;

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

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.channel_ = channel_;
        cpuImpl_.strength_ = strength_;
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
        Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
        if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) { applyCPU(src, dst); return; }
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
        Diligent::RefCntAutoPtr<Diligent::ITexture> output;
        device->CreateTexture(outDesc, nullptr, &output);
        if (!output) { applyCPU(src, dst); return; }

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
        ArtifactCore::ComputePipelineDesc pipeline{};
        pipeline.name = "Invert/PSO";
        pipeline.shaderSource = kInvertHlsl;
        pipeline.entryPoint = "main";
        pipeline.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        pipeline.variables = vars;
        pipeline.variableCount = 3;
        pipeline.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        if (!executor.build(pipeline) || !executor.createShaderResourceBinding(true) ||
            !executor.setBuffer("InvertParams", params) ||
            !executor.setTextureView("g_InputTexture", input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor.setTextureView("g_OutputTexture", output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        executor.dispatch(context, ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1),
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::TextureDesc stagingDesc = outDesc;
        stagingDesc.Name = "Invert/Readback";
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.BindFlags = Diligent::BIND_NONE;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) { applyCPU(src, dst); return; }
        Diligent::CopyTextureAttribs copy(output, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CopyTexture(copy);
        context->Flush();
        context->WaitForIdle();
        Diligent::MappedTextureSubresource read{};
        context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, read);
        if (!read.pData || !read.Stride) { applyCPU(src, dst); return; }
        cv::Mat result(static_cast<int>(outDesc.Height), static_cast<int>(outDesc.Width), CV_32FC4, read.pData, read.Stride);
        dst.image().setFromCVMat(result, image.colorDescriptor());
        context->UnmapTextureSubresource(staging, 0, 0);
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
    strength_ = std::clamp(value, 0.0f, 1.0f);
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

