module;
#include <utility>

#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <cstring>
#include <vector>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module BrightnessEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class BrightnessEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float contrastFactor = (contrast_ != 1.0f) ? (1.0f + contrast_) / (1.0f - contrast_) : 100.0f;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                for (int c = 0; c < 3; ++c) {
                    float val = pixel[c];
                    val += brightness_;
                    val = contrastFactor * (val - 0.5f) + 0.5f;
                    if (val > 0.5f) {
                        const float highlightWeight = (val - 0.5f) * 2.0f;
                        val += highlights_ * highlightWeight * 0.5f;
                    }
                    if (val < 0.5f) {
                        const float shadowWeight = (0.5f - val) * 2.0f;
                        val += shadows_ * shadowWeight * 0.5f;
                    }
                    pixel[c] = std::clamp(val, 0.0f, 1.0f);
                }
            }
        }
    }
};

class BrightnessEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;
    BrightnessEffectCPUImpl cpuFallback_;

    ~BrightnessEffectGPUImpl() override = default;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuFallback_.brightness_ = brightness_;
        cpuFallback_.contrast_ = contrast_;
        cpuFallback_.highlights_ = highlights_;
        cpuFallback_.shadows_ = shadows_;
        cpuFallback_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        const auto lease = [this]() {
            struct Guard {
                BrightnessEffectGPUImpl* self;
                ~Guard() {
                    if (self) {
                        self->context_.Release();
                        self->device_.Release();
                        releaseSharedRenderDevice();
                    }
                }
            };
            return Guard{this};
        }();

        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "BrightnessContrast/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "BrightnessContrastParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "BrightnessContrast/PSO";
            desc.shaderSource = kBrightnessContrastHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("BrightnessContrastParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "BrightnessContrast/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "BrightnessContrast/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) {
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
        params.brightness = brightness_;
        params.contrast = contrast_;
        params.highlights = highlights_;
        params.shadows = shadows_;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "BrightnessContrast/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
    }

private:
    struct ParamsCB {
        float brightness = 0.0f;
        float contrast = 0.0f;
        float highlights = 0.0f;
        float shadows = 0.0f;
    };

    static const char* kBrightnessContrastHlsl;

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src,
                                       Diligent::IRenderDevice* device,
                                       Diligent::ITexture** outTex,
                                       const char* name)
    {
        if (!device || !outTex) {
            return false;
        }
        const auto& img = src.image();
        const float* data = img.rgba32fData();
        if (!data || img.width() <= 0 || img.height() <= 0) {
            return false;
        }
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(img.width());
        desc.Height = static_cast<Diligent::Uint32>(img.height());
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = name;
        Diligent::TextureSubResData sub{};
        sub.pData = data;
        sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device->CreateTexture(desc, &init, outTex);
        return *outTex != nullptr;
    }

    static bool readbackTexture(Diligent::IRenderDevice* device,
                                Diligent::IDeviceContext* ctx,
                                Diligent::ITexture* src,
                                ImageF32x4RGBAWithCache& dst,
                                const char* name)
    {
        if (!device || !ctx || !src) {
            return false;
        }
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
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) {
            return false;
        }
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) {
            return false;
        }
        const size_t rowBytes = static_cast<size_t>(desc.Width) * sizeof(float) * 4ull;
        if (mapped.Stride < rowBytes) {
            ctx->UnmapTextureSubresource(staging, 0, 0);
            return false;
        }
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    bool pipelineReady_ = false;
    static constexpr const char* kBrightnessContrastHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer BrightnessContrastParams : register(b0) {
    float g_Brightness;
    float g_Contrast;
    float g_Highlights;
    float g_Shadows;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 px = g_InputTexture[dtid.xy];
    float contrastFactor = (abs(g_Contrast - 1.0f) > 0.0001f)
        ? (1.0f + g_Contrast) / max(0.0001f, 1.0f - g_Contrast)
        : 100.0f;

    for (int c = 0; c < 3; ++c) {
        float val = px[c];
        val += g_Brightness;
        val = contrastFactor * (val - 0.5f) + 0.5f;
        if (val > 0.5f) {
            float highlightWeight = (val - 0.5f) * 2.0f;
            val += g_Highlights * highlightWeight * 0.5f;
        }
        if (val < 0.5f) {
            float shadowWeight = (0.5f - val) * 2.0f;
            val += g_Shadows * shadowWeight * 0.5f;
        }
        px[c] = clamp(val, 0.0f, 1.0f);
    }
    g_OutputTexture[dtid.xy] = px;
}
)";
    BrightnessEffectCPUImpl cpuImpl_;
};

BrightnessEffect::BrightnessEffect() {
    setEffectID(UniString("effect.colorcorrection.brightness"));
    setDisplayName(UniString("Brightness / Contrast"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BrightnessEffectCPUImpl>());
    setGPUImpl(std::make_shared<BrightnessEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

BrightnessEffect::~BrightnessEffect() = default;

void BrightnessEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<BrightnessEffectCPUImpl>(cpuImpl())) {
        cpu->brightness_ = brightness_;
        cpu->contrast_ = contrast_;
        cpu->highlights_ = highlights_;
        cpu->shadows_ = shadows_;
    }
    if (auto gpu = std::dynamic_pointer_cast<BrightnessEffectGPUImpl>(gpuImpl())) {
        gpu->brightness_ = brightness_;
        gpu->contrast_ = contrast_;
        gpu->highlights_ = highlights_;
        gpu->shadows_ = shadows_;
    }
}

std::vector<AbstractProperty> BrightnessEffect::getProperties() const {
    std::vector<AbstractProperty> props(4);

    props[0].setName("Brightness");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(brightness_)));
    props[0].setHardRange(-1.0, 1.0);
    props[0].setSoftRange(-0.5, 0.5);
    props[0].setStep(0.01);

    props[1].setName("Contrast");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(contrast_)));
    props[1].setHardRange(-1.0, 1.0);
    props[1].setSoftRange(-0.5, 0.5);
    props[1].setStep(0.01);

    props[2].setName("Highlights");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(highlights_)));
    props[2].setHardRange(-1.0, 1.0);
    props[2].setSoftRange(-0.5, 0.5);
    props[2].setStep(0.01);

    props[3].setName("Shadows");
    props[3].setType(ArtifactCore::PropertyType::Float);
    props[3].setValue(QVariant(static_cast<double>(shadows_)));
    props[3].setHardRange(-1.0, 1.0);
    props[3].setSoftRange(-0.5, 0.5);
    props[3].setStep(0.01);

    return props;
}

void BrightnessEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Brightness") setBrightness(value.toFloat());
    else if (name == "Contrast") setContrast(value.toFloat());
    else if (name == "Highlights") setHighlights(value.toFloat());
    else if (name == "Shadows") setShadows(value.toFloat());
}

} // namespace Artifact
