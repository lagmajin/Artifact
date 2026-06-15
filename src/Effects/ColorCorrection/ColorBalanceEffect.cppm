module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module ColorBalanceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ColorBalance;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class ColorBalanceEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColorBalanceSettings settings_;
    ColorBalanceProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }
};

class ColorBalanceEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ColorBalanceSettings settings_;
    ColorBalanceProcessor processor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }

        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "ColorBalance/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "ColorBalanceParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "ColorBalance/PSO";
            desc.shaderSource = kColorBalanceHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("ColorBalanceParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "ColorBalance/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "ColorBalance/OutputTexture";
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
        params.shadowR = settings_.shadowR;
        params.shadowG = settings_.shadowG;
        params.shadowB = settings_.shadowB;
        params.midtoneR = settings_.midtoneR;
        params.midtoneG = settings_.midtoneG;
        params.midtoneB = settings_.midtoneB;
        params.highlightR = settings_.highlightR;
        params.highlightG = settings_.highlightG;
        params.highlightB = settings_.highlightB;
        params.shadowRange = settings_.shadowRange;
        params.highlightRange = settings_.highlightRange;
        params.masterStrength = settings_.masterStrength;
        params.preserveLuma = settings_.preserveLuma ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "ColorBalance/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
    }

private:
    struct ParamsCB {
        float shadowR = 0.0f;
        float shadowG = 0.0f;
        float shadowB = 0.0f;
        float midtoneR = 0.0f;
        float midtoneG = 0.0f;
        float midtoneB = 0.0f;
        float highlightR = 0.0f;
        float highlightG = 0.0f;
        float highlightB = 0.0f;
        float shadowRange = 0.33f;
        float highlightRange = 0.66f;
        float masterStrength = 1.0f;
        float preserveLuma = 0.0f;
    };

    static constexpr const char* kColorBalanceHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ColorBalanceParams : register(b0)
{
    float3 g_Shadow;
    float g_ShadowRange;
    float3 g_Midtone;
    float g_HighlightRange;
    float3 g_Highlight;
    float g_MasterStrength;
    float g_PreserveLuma;
    float3 g_Pad;
};

float luma(float3 c) {
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

float smoothstep01(float edge0, float edge1, float x) {
    float t = saturate((x - edge0) / max(0.0001f, edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 px = g_InputTexture[dtid.xy];
    float3 c = px.rgb;
    float lum = luma(c);

    float shadowW = 1.0f - smoothstep01(g_ShadowRange * 0.5f, g_ShadowRange, lum);
    float highlightW = smoothstep01(g_HighlightRange, lerp(g_HighlightRange, 1.0f, 0.5f), lum);
    float midtoneW = saturate(1.0f - shadowW - highlightW);

    float3 delta = g_Shadow * shadowW + g_Midtone * midtoneW + g_Highlight * highlightW;
    float3 mixed = c + delta * g_MasterStrength;

    if (g_PreserveLuma > 0.5f) {
        float mixedLum = luma(mixed);
        mixed += (lum - mixedLum).xxx;
    }

    px.rgb = saturate(mixed);
    g_OutputTexture[dtid.xy] = px;
}
)";

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
};

ColorBalanceEffect::ColorBalanceEffect() {
    setEffectID(UniString("effect.colorcorrection.colorbalance"));
    setDisplayName(UniString("Color Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColorBalanceEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColorBalanceEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ColorBalanceEffect::~ColorBalanceEffect() = default;

void ColorBalanceEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Neutral:
        settings_ = ColorBalanceSettings::neutral();
        break;
    case Preset::CoolShadows:
        settings_ = ColorBalanceSettings::coolShadows();
        break;
    case Preset::WarmHighlights:
        settings_ = ColorBalanceSettings::warmHighlights();
        break;
    case Preset::Cinematic:
        settings_ = ColorBalanceSettings::cinematic();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ColorBalanceEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ColorBalanceEffect::setShadowBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.shadowR = std::clamp(r, -1.0f, 1.0f);
    settings_.shadowG = std::clamp(g, -1.0f, 1.0f);
    settings_.shadowB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setMidtoneBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.midtoneR = std::clamp(r, -1.0f, 1.0f);
    settings_.midtoneG = std::clamp(g, -1.0f, 1.0f);
    settings_.midtoneB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setHighlightBalance(float r, float g, float b) {
    preset_ = Preset::Custom;
    settings_.highlightR = std::clamp(r, -1.0f, 1.0f);
    settings_.highlightG = std::clamp(g, -1.0f, 1.0f);
    settings_.highlightB = std::clamp(b, -1.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setShadowRange(float value) {
    preset_ = Preset::Custom;
    settings_.shadowRange = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setHighlightRange(float value) {
    preset_ = Preset::Custom;
    settings_.highlightRange = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setMasterStrength(float value) {
    preset_ = Preset::Custom;
    settings_.masterStrength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColorBalanceEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ColorBalanceEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColorBalanceEffectCPUImpl*>(cpuImpl().get())) {
        cpu->settings_ = settings_;
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ColorBalanceEffectGPUImpl*>(gpuImpl().get())) {
        gpu->settings_ = settings_;
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ColorBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(14);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Shadow R", settings_.shadowR, -20);
    addFloat("Shadow G", settings_.shadowG, -19);
    addFloat("Shadow B", settings_.shadowB, -18);
    addFloat("Midtone R", settings_.midtoneR, -10);
    addFloat("Midtone G", settings_.midtoneG, -9);
    addFloat("Midtone B", settings_.midtoneB, -8);
    addFloat("Highlight R", settings_.highlightR, 0);
    addFloat("Highlight G", settings_.highlightG, 1);
    addFloat("Highlight B", settings_.highlightB, 2);
    addFloat("Shadow Range", settings_.shadowRange, 10);
    addFloat("Highlight Range", settings_.highlightRange, 11);
    addFloat("Strength", settings_.masterStrength, 20);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(30);
    props.push_back(preserveProp);

    return props;
}

void ColorBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Shadow R")) {
        setShadowBalance(value.toFloat(), settings_.shadowG, settings_.shadowB);
    } else if (key == QStringLiteral("Shadow G")) {
        setShadowBalance(settings_.shadowR, value.toFloat(), settings_.shadowB);
    } else if (key == QStringLiteral("Shadow B")) {
        setShadowBalance(settings_.shadowR, settings_.shadowG, value.toFloat());
    } else if (key == QStringLiteral("Midtone R")) {
        setMidtoneBalance(value.toFloat(), settings_.midtoneG, settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone G")) {
        setMidtoneBalance(settings_.midtoneR, value.toFloat(), settings_.midtoneB);
    } else if (key == QStringLiteral("Midtone B")) {
        setMidtoneBalance(settings_.midtoneR, settings_.midtoneG, value.toFloat());
    } else if (key == QStringLiteral("Highlight R")) {
        setHighlightBalance(value.toFloat(), settings_.highlightG, settings_.highlightB);
    } else if (key == QStringLiteral("Highlight G")) {
        setHighlightBalance(settings_.highlightR, value.toFloat(), settings_.highlightB);
    } else if (key == QStringLiteral("Highlight B")) {
        setHighlightBalance(settings_.highlightR, settings_.highlightG, value.toFloat());
    } else if (key == QStringLiteral("Shadow Range")) {
        setShadowRange(value.toFloat());
    } else if (key == QStringLiteral("Highlight Range")) {
        setHighlightRange(value.toFloat());
    } else if (key == QStringLiteral("Strength")) {
        setMasterStrength(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    }
}

} // namespace Artifact
