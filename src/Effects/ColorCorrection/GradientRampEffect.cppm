module;
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module GradientRampEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.GradientRamp;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class GradientRampEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        processor_.apply(pixels, dst.image().width(), dst.image().height());
    }
};

class GradientRampEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::GradientRampProcessor processor_;
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
        processor_.apply(pixels, dst.image().width(), dst.image().height());
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
            cbDesc.Name = "GradientRamp/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "GradientRampParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "GradientRamp/PSO";
            desc.shaderSource = kGradientRampHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("GradientRampParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "GradientRamp/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "GradientRamp/OutputTexture";
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

        const auto& settings = processor_.settings();
        ParamsCB params{};
        params.startColor[0] = settings.startColor.redF();
        params.startColor[1] = settings.startColor.greenF();
        params.startColor[2] = settings.startColor.blueF();
        params.startColor[3] = 1.0f;
        params.endColor[0] = settings.endColor.redF();
        params.endColor[1] = settings.endColor.greenF();
        params.endColor[2] = settings.endColor.blueF();
        params.endColor[3] = 1.0f;
        params.startX = settings.startX;
        params.startY = settings.startY;
        params.endX = settings.endX;
        params.endY = settings.endY;
        params.opacity = settings.opacity;
        params.preserveAlpha = settings.preserveAlpha ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "GradientRamp/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
    }

private:
    struct ParamsCB {
        float startColor[4]{};
        float endColor[4]{};
        float startX = 0.0f;
        float startY = 0.0f;
        float endX = 1.0f;
        float endY = 1.0f;
        float opacity = 1.0f;
        float preserveAlpha = 1.0f;
    };

    static constexpr const char* kGradientRampHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GradientRampParams : register(b0)
{
    float4 g_StartColor;
    float4 g_EndColor;
    float g_StartX;
    float g_StartY;
    float g_EndX;
    float g_EndY;
    float g_Opacity;
    float g_PreserveAlpha;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) {
        return;
    }

    float4 px = g_InputTexture[dtid.xy];
    float sx = g_StartX * max(1.0f, (float)width - 1.0f);
    float sy = g_StartY * max(1.0f, (float)height - 1.0f);
    float ex = g_EndX * max(1.0f, (float)width - 1.0f);
    float ey = g_EndY * max(1.0f, (float)height - 1.0f);
    float dx = ex - sx;
    float dy = ey - sy;
    float lenSq = max(1e-6f, dx * dx + dy * dy);
    float t = saturate((((float)dtid.x - sx) * dx + ((float)dtid.y - sy) * dy) / lenSq);

    float3 ramp = lerp(g_StartColor.rgb, g_EndColor.rgb, t);
    px.rgb = lerp(px.rgb, ramp, saturate(g_Opacity));
    if (g_PreserveAlpha < 0.5f) {
        px.a = lerp(px.a, 1.0f, saturate(g_Opacity));
    }
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

        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

GradientRampEffect::GradientRampEffect() {
    setEffectID(UniString("effect.colorcorrection.gradientramp"));
    setDisplayName(UniString("Gradient Ramp"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<GradientRampEffectCPUImpl>());
    setGPUImpl(std::make_shared<GradientRampEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

GradientRampEffect::~GradientRampEffect() = default;

void GradientRampEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Sunrise:
        settings_ = ArtifactCore::GradientRampSettings::sunrise();
        break;
    case Preset::Ocean:
        settings_ = ArtifactCore::GradientRampSettings::ocean();
        break;
    case Preset::Neon:
        settings_ = ArtifactCore::GradientRampSettings::neon();
        break;
    case Preset::Mono:
        settings_ = ArtifactCore::GradientRampSettings::mono();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void GradientRampEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 3);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void GradientRampEffect::setStartColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.startColor = color;
    syncImpls();
}

void GradientRampEffect::setEndColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.endColor = color;
    syncImpls();
}

void GradientRampEffect::setStartPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.startX = std::clamp(x, 0.0f, 1.0f);
    settings_.startY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setEndPoint(float x, float y) {
    preset_ = Preset::Custom;
    settings_.endX = std::clamp(x, 0.0f, 1.0f);
    settings_.endY = std::clamp(y, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setOpacity(float value) {
    preset_ = Preset::Custom;
    settings_.opacity = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void GradientRampEffect::setPreserveAlpha(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveAlpha = value;
    syncImpls();
}

void GradientRampEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<GradientRampEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<GradientRampEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> GradientRampEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addColor = [&props](const char* name, const QColor& color, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };
    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addColor("Start Color", settings_.startColor, -20);
    addColor("End Color", settings_.endColor, -19);
    addFloat("Start X", settings_.startX, -10);
    addFloat("Start Y", settings_.startY, -9);
    addFloat("End X", settings_.endX, -8);
    addFloat("End Y", settings_.endY, -7);
    addFloat("Opacity", settings_.opacity, 0);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Alpha");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveAlpha);
    preserveProp.setDisplayPriority(10);
    props.push_back(preserveProp);

    return props;
}

void GradientRampEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Start Color")) {
        setStartColor(value.value<QColor>());
    } else if (key == QStringLiteral("End Color")) {
        setEndColor(value.value<QColor>());
    } else if (key == QStringLiteral("Start X")) {
        setStartPoint(value.toFloat(), settings_.startY);
    } else if (key == QStringLiteral("Start Y")) {
        setStartPoint(settings_.startX, value.toFloat());
    } else if (key == QStringLiteral("End X")) {
        setEndPoint(value.toFloat(), settings_.endY);
    } else if (key == QStringLiteral("End Y")) {
        setEndPoint(settings_.endX, value.toFloat());
    } else if (key == QStringLiteral("Opacity")) {
        setOpacity(value.toFloat());
    } else if (key == QStringLiteral("Preserve Alpha")) {
        setPreserveAlpha(value.toBool());
    }
}

} // namespace Artifact
