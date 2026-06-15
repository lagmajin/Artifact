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

module FillEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Fill;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class FillEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SolidFillProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        processor_.apply(pixels, dst.image().width(), dst.image().height());
    }
};

class FillEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SolidFillProcessor processor_;
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
            cbDesc.Name = "Fill/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "FillParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Fill/PSO";
            desc.shaderSource = kFillHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) ||
                !executor->setBuffer("FillParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "Fill/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Fill/OutputTexture";
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
        params.r = settings_.color.redF();
        params.g = settings_.color.greenF();
        params.b = settings_.color.blueF();
        params.opacity = settings_.opacity;
        params.preserveAlpha = settings_.preserveAlpha ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex, dst, "Fill/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
    }

private:
    struct ParamsCB {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float opacity = 1.0f;
        float preserveAlpha = 1.0f;
        float pad[3]{};
    };

    static constexpr const char* kFillHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer FillParams : register(b0)
{
    float g_R;
    float g_G;
    float g_B;
    float g_Opacity;
    float g_PreserveAlpha;
    float3 g_Pad;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 px = g_InputTexture[dtid.xy];
    float3 fillColor = float3(g_R, g_G, g_B);
    px.rgb = lerp(px.rgb, fillColor, saturate(g_Opacity));
    if (g_PreserveAlpha < 0.5f) {
        px.a = 1.0f;
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

FillEffect::FillEffect() {
    setEffectID(UniString("effect.colorcorrection.fill"));
    setDisplayName(UniString("Fill"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<FillEffectCPUImpl>());
    setGPUImpl(std::make_shared<FillEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

FillEffect::~FillEffect() = default;

void FillEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::White:
        settings_ = ArtifactCore::SolidFillSettings::white();
        break;
    case Preset::Black:
        settings_ = ArtifactCore::SolidFillSettings::black();
        break;
    case Preset::Red:
        settings_ = ArtifactCore::SolidFillSettings::red();
        break;
    case Preset::Blue:
        settings_ = ArtifactCore::SolidFillSettings::blue();
        break;
    case Preset::Green:
        settings_ = ArtifactCore::SolidFillSettings::green();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void FillEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void FillEffect::setColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.color = color;
    syncImpls();
}

void FillEffect::setOpacity(float value) {
    preset_ = Preset::Custom;
    settings_.opacity = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void FillEffect::setPreserveAlpha(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveAlpha = value;
    syncImpls();
}

void FillEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<FillEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<FillEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> FillEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(5);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty colorProp;
    colorProp.setName("Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setColorValue(settings_.color);
    colorProp.setValue(settings_.color);
    colorProp.setDisplayPriority(-20);
    props.push_back(colorProp);

    AbstractProperty opacityProp;
    opacityProp.setName("Opacity");
    opacityProp.setType(PropertyType::Float);
    opacityProp.setValue(QVariant(static_cast<double>(settings_.opacity)));
    opacityProp.setDisplayPriority(0);
    props.push_back(opacityProp);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Alpha");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveAlpha);
    preserveProp.setDisplayPriority(10);
    props.push_back(preserveProp);

    return props;
}

void FillEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Color")) {
        setColor(value.value<QColor>());
    } else if (key == QStringLiteral("Opacity")) {
        setOpacity(value.toFloat());
    } else if (key == QStringLiteral("Preserve Alpha")) {
        setPreserveAlpha(value.toBool());
    }
}

} // namespace Artifact
