module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QColor>
#include <QVariant>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module PhotoFilterEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.PhotoFilter;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class PhotoFilterEffectCPUImpl : public ArtifactEffectImplBase {
public:
    PhotoFilterProcessor processor_;

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

class PhotoFilterEffectGPUImpl : public ArtifactEffectImplBase {
public:
    PhotoFilterProcessor processor_;
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
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc; cbDesc.Name = "PhotoFilter/ParamsCB"; cbDesc.Size = sizeof(ParamsCB); cbDesc.Usage = Diligent::USAGE_DYNAMIC; cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "PhotoFilterParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc; desc.name="PhotoFilter/PSO"; desc.shaderSource=kPhotoFilterHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("PhotoFilterParams", paramsCB_)) { applyCPU(src, dst); return; }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "PhotoFilter/InputTexture")) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "PhotoFilter/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc, nullptr, &outputTex); if (!outputTex) { applyCPU(src, dst); return; }
        void* mapped = nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src, dst); return; }
        const auto &settings = processor_.settings();
        ParamsCB params{}; params.r=settings.color.r; params.g=settings.color.g; params.b=settings.color.b; params.density=settings.density; params.brightness=settings.brightness; params.contrast=settings.contrast; params.saturationBoost=settings.saturationBoost; params.preserveLuma=settings.preserveLuma ? 1.0f : 0.0f; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "PhotoFilter/StagingTexture")) { applyCPU(src, dst); return; }
    }

private:
    struct ParamsCB { float r=1,g=0.9f,b=0.7f,density=0.35f,brightness=0,contrast=1,saturationBoost=1,preserveLuma=1; };
    static constexpr const char* kPhotoFilterHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer PhotoFilterParams : register(b0){ float g_R; float g_G; float g_B; float g_Density; float g_Brightness; float g_Contrast; float g_SaturationBoost; float g_PreserveLuma; };
float luma(float3 c){ return dot(c,float3(0.299f,0.587f,0.114f)); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 c=px.rgb; float gray=luma(c); float3 filter=float3(g_R,g_G,g_B); c = lerp(c, c*filter, saturate(g_Density)); c += g_Brightness.xxx; c = (c - 0.5f) * g_Contrast + 0.5f; if(g_SaturationBoost != 1.0f){ float g=luma(c); c = lerp(float3(g,g,g), c, g_SaturationBoost);} if(g_PreserveLuma > 0.5f){ float gl=luma(c); c += (gray-gl).xxx; } px.rgb = saturate(c); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name) { const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

namespace {
QColor toQColor(const PhotoFilterColor& color) {
    return QColor::fromRgbF(color.r, color.g, color.b);
}

PhotoFilterColor toColor(const QColor& color) {
    return {
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF())
    };
}
}

PhotoFilterEffect::PhotoFilterEffect() {
    setEffectID(UniString("effect.colorcorrection.photofilter"));
    setDisplayName(UniString("Photo Filter"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<PhotoFilterEffectCPUImpl>());
    setGPUImpl(std::make_shared<PhotoFilterEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

PhotoFilterEffect::~PhotoFilterEffect() = default;

void PhotoFilterEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Warm:
        settings_ = PhotoFilterSettings::warm();
        break;
    case Preset::Cool:
        settings_ = PhotoFilterSettings::cool();
        break;
    case Preset::Sepia:
        settings_ = PhotoFilterSettings::sepia();
        break;
    case Preset::Cyan:
        settings_ = PhotoFilterSettings::cyan();
        break;
    case Preset::Rose:
        settings_ = PhotoFilterSettings::rose();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void PhotoFilterEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void PhotoFilterEffect::setColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.color = toColor(color);
    syncImpls();
}

void PhotoFilterEffect::setDensity(float value) {
    preset_ = Preset::Custom;
    settings_.density = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void PhotoFilterEffect::setBrightness(float value) {
    preset_ = Preset::Custom;
    settings_.brightness = std::clamp(value, -1.0f, 1.0f);
    syncImpls();
}

void PhotoFilterEffect::setContrast(float value) {
    preset_ = Preset::Custom;
    settings_.contrast = std::clamp(value, 0.0f, 2.0f);
    syncImpls();
}

void PhotoFilterEffect::setSaturationBoost(float value) {
    preset_ = Preset::Custom;
    settings_.saturationBoost = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void PhotoFilterEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void PhotoFilterEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<PhotoFilterEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<PhotoFilterEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> PhotoFilterEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    AbstractProperty colorProp;
    colorProp.setName("Filter Color");
    colorProp.setType(PropertyType::Color);
    colorProp.setColorValue(toQColor(settings_.color));
    colorProp.setValue(toQColor(settings_.color));
    colorProp.setDisplayPriority(-20);
    props.push_back(colorProp);

    auto addFloat = [&props](const char* name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    addFloat("Density", settings_.density, 0);
    addFloat("Brightness", settings_.brightness, 10);
    addFloat("Contrast", settings_.contrast, 20);
    addFloat("Saturation Boost", settings_.saturationBoost, 30);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(40);
    props.push_back(preserveProp);

    return props;
}

void PhotoFilterEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    } else if (key == QStringLiteral("Filter Color")) {
        setColor(value.value<QColor>());
    } else if (key == QStringLiteral("Density")) {
        setDensity(value.toFloat());
    } else if (key == QStringLiteral("Brightness")) {
        setBrightness(value.toFloat());
    } else if (key == QStringLiteral("Contrast")) {
        setContrast(value.toFloat());
    } else if (key == QStringLiteral("Saturation Boost")) {
        setSaturationBoost(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    }
}

} // namespace Artifact
