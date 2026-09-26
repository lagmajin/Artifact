module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <QElapsedTimer>
#include <QSettings>
#include <QDebug>
#include <QtGlobal>
#include <vector>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ThresholdEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

namespace {
constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;
} // namespace

class ThresholdEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float th = threshold_;

        ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
            float* row = pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
            for (int x = 0; x < width; ++x) {
                float* pixel = row + static_cast<size_t>(x) * 4u;
                const float luma = pixel[0] * kLumaR + pixel[1] * kLumaG + pixel[2] * kLumaB;
                const float val = (luma >= th) ? 1.0f : 0.0f;
                pixel[0] = val;
                pixel[1] = val;
                pixel[2] = val;
                // アルファチャンネルはそのまま保持
            }
        });
    }
};

class ThresholdEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto previousDevice = device_;
        const auto previousContext = context_;
        SharedRenderDeviceLease lease;
        if (!lease.acquire(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        if (previousDevice != device_ || previousContext != context_) {
            executor_.reset();
            gpuContext_.reset();
            paramsCB_.Release();
            outputTex_.Release();
            stagingTex_.Release();
            pipelineReady_ = false;
        }

        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) {
            applyCPU(src, dst);
            return;
        }

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "Threshold/ParamsCB";
            cbDesc.Size = sizeof(ParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) {
            applyCPU(src, dst);
            return;
        }

        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "ThresholdParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Threshold/PSO";
            desc.shaderSource = kThresholdHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("ThresholdParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "Threshold/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Threshold/OutputTexture";
        if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
            outputTex_->GetDesc().Height != outDesc.Height ||
            outputTex_->GetDesc().Format != outDesc.Format ||
            outputTex_->GetDesc().BindFlags != outDesc.BindFlags) {
            outputTex_.Release();
            device_->CreateTexture(outDesc, nullptr, &outputTex_);
        }
        if (!outputTex_) {
            applyCPU(src, dst);
            return;
        }

        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        ParamsCB params{};
        params.threshold = threshold_;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex_, stagingTex_, dst, src.image().colorDescriptor(), "Threshold/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float threshold = 0.5f;
        float3 pad = {0.0f, 0.0f, 0.0f};
    };

    static constexpr const char* kThresholdHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ThresholdParams : register(b0) {
    float g_Threshold;
    float3 g_Pad;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = g_InputTexture[dtid.xy];
    float luma = c.r * 0.299f + c.g * 0.587f + c.b * 0.114f;
    float val = (luma >= g_Threshold) ? 1.0f : 0.0f;
    c.rgb = float3(val, val, val);
    g_OutputTexture[dtid.xy] = c;
}
)";

    ThresholdEffectCPUImpl cpuImpl_;

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& img,
                                      Diligent::IRenderDevice* device,
                                      Diligent::ITexture** outTex,
                                      const char* name)
    {
        if (!device || !outTex) return false;
        const auto& image = img.image();
        const int width = image.width();
        const int height = image.height();
        if (width <= 0 || height <= 0) return false;
        const float* data = image.rgba32fData();
        if (!data) return false;

        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(width);
        desc.Height = static_cast<Diligent::Uint32>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = name;

        Diligent::TextureSubResData subres;
        subres.pData = data;
        subres.Stride = static_cast<Diligent::Uint32>(width * 4 * sizeof(float));
        Diligent::TextureData initData;
        initData.pSubResources = &subres;
        initData.NumSubresources = 1;

        device->CreateTexture(desc, &initData, outTex);
        return (*outTex != nullptr);
    }

    static bool readbackTexture(Diligent::IRenderDevice* device,
                               Diligent::IDeviceContext* ctx,
                               Diligent::ITexture* src,
                               Diligent::RefCntAutoPtr<Diligent::ITexture>& staging,
                               ImageF32x4RGBAWithCache& dst,
                               const ArtifactCore::ImageColorDescriptor& colorDescriptor,
                               const char* name)
    {
        if (!device || !ctx || !src) return false;
        const auto desc = src->GetDesc();
        Diligent::TextureDesc stagingDesc;
        stagingDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = desc.Width;
        stagingDesc.Height = desc.Height;
        stagingDesc.Format = desc.Format;
        stagingDesc.ArraySize = 1;
        stagingDesc.MipLevels = 1;
        stagingDesc.SampleCount = 1;
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        stagingDesc.Name = name;
        if (!staging || staging->GetDesc().Width != stagingDesc.Width ||
            staging->GetDesc().Height != stagingDesc.Height ||
            staging->GetDesc().Format != stagingDesc.Format) {
            staging.Release();
            device->CreateTexture(stagingDesc, nullptr, &staging);
        }
        if (!staging) return false;

        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;

        const size_t rowBytes = static_cast<size_t>(desc.Width) * sizeof(float) * 4ull;
        if (mapped.Stride < rowBytes) {
            ctx->UnmapTextureSubresource(staging, 0, 0);
            return false;
        }
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp, colorDescriptor);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

ThresholdEffect::ThresholdEffect() {
    setEffectID(UniString("effect.colorcorrection.threshold"));
    setDisplayName(UniString("Threshold"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ThresholdEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<ThresholdEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ThresholdEffect::~ThresholdEffect() = default;

void ThresholdEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<ThresholdEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
    }
    if (auto gpu = ArtifactCore::dynamicPointerCast<ThresholdEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
    }
}

std::vector<AbstractProperty> ThresholdEffect::getProperties() const {
    std::vector<AbstractProperty> props(1);

    props[0].setName("Threshold");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(threshold_)));

    return props;
}

void ThresholdEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Threshold") setThreshold(value.toFloat());
}

} // namespace Artifact
