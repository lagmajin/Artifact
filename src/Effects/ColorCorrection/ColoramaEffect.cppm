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

module ColoramaEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Colorama;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class ColoramaEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ColoramaProcessor processor_;

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

class ColoramaEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ColoramaProcessor processor_;
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
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Colorama/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "ColoramaParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Colorama/PSO"; desc.shaderSource=kColoramaHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("ColoramaParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_ = true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Colorama/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "Colorama/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        const auto &settings = processor_.settings();
        ParamsCB params{}; params.sourceMode=(int)settings.sourceMode; params.palette=(int)settings.palette; params.phase=settings.phase; params.spread=settings.spread; params.strength=settings.strength; params.saturationBoost=settings.saturationBoost; params.contrast=settings.contrast; params.preserveLuma=settings.preserveLuma ? 1.0f : 0.0f; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "Colorama/StagingTexture")) { applyCPU(src,dst); return; }
    }

private:
    struct ParamsCB { int sourceMode=0; int palette=0; float phase=0; float spread=1; float strength=1; float saturationBoost=1; float contrast=1; float preserveLuma=1; };
    static constexpr const char* kColoramaHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ColoramaParams : register(b0){ int g_SourceMode; int g_Palette; float g_Phase; float g_Spread; float g_Strength; float g_SaturationBoost; float g_Contrast; float g_PreserveLuma; };
float luma(float3 c){ return dot(c,float3(0.299f,0.587f,0.114f)); }
float3 paletteColor(int palette, float t){ t = frac(t); if(palette==1) return lerp(float3(0.25,0.0,0.0), float3(1.0,0.92,0.25), t); if(palette==2) return lerp(float3(0.0,0.12,0.28), float3(0.20,0.78,1.0), t); if(palette==3) return lerp(float3(0.0,0.8,0.55), float3(1.0,0.15,0.9), t); if(palette==4) return lerp(float3(0.28,0.08,0.18), float3(1.0,0.86,0.60), t); return float3(t, frac(t+0.33), frac(t+0.66)); }
float3 rgbToHsl(float3 c){ float maxc=max(c.r,max(c.g,c.b)); float minc=min(c.r,min(c.g,c.b)); float l=(maxc+minc)*0.5f; float d=maxc-minc; float s=(d<1e-5f)?0.0f:d/(1.0f-abs(2.0f*l-1.0f)); float h=0.0f; if(d>1e-5f){ if(maxc==c.r) h=fmod((c.g-c.b)/d,6.0f); else if(maxc==c.g) h=((c.b-c.r)/d)+2.0f; else h=((c.r-c.g)/d)+4.0f; h/=6.0f; if(h<0) h+=1.0f; } return float3(h,s,l); }
float3 hslToRgb(float3 hsl){ float h=hsl.x, s=hsl.y, l=hsl.z; float c=(1.0f-abs(2.0f*l-1.0f))*s; float x=c*(1.0f-abs(fmod(h*6.0f,2.0f)-1.0f)); float m=l-c*0.5f; float3 rgb; if(h<1.0/6.0) rgb=float3(c,x,0); else if(h<2.0/6.0) rgb=float3(x,c,0); else if(h<3.0/6.0) rgb=float3(0,c,x); else if(h<4.0/6.0) rgb=float3(0,x,c); else if(h<5.0/6.0) rgb=float3(x,0,c); else rgb=float3(c,0,x); return rgb+m; }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 c=px.rgb; float lum=luma(c); float3 hsl=rgbToHsl(c); float t=(g_SourceMode==0)?lum:hsl.x; float mapped=paletteColor(g_Palette, t*g_Spread + g_Phase).r; float3 paletteRGB=paletteColor(g_Palette, t*g_Spread + g_Phase); float3 outRgb = lerp(c, paletteRGB, saturate(g_Strength)); if(g_SourceMode==1){ hsl.x = frac(hsl.x + g_Phase); hsl.y = saturate(hsl.y * g_SaturationBoost); hsl.z = saturate(hsl.z * g_Contrast); outRgb = hslToRgb(hsl); outRgb = lerp(c, outRgb, saturate(g_Strength)); } if(g_PreserveLuma > 0.5f){ float outLum=luma(outRgb); outRgb += (lum-outLum).xxx; } px.rgb=saturate(outRgb); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

ColoramaEffect::ColoramaEffect() {
    setEffectID(UniString("effect.colorcorrection.colorama"));
    setDisplayName(UniString("Colorama"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ColoramaEffectCPUImpl>());
    setGPUImpl(std::make_shared<ColoramaEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

ColoramaEffect::~ColoramaEffect() = default;

void ColoramaEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Rainbow:
        settings_ = ColoramaSettings::rainbow();
        break;
    case Preset::Fire:
        settings_ = ColoramaSettings::fire();
        break;
    case Preset::Ocean:
        settings_ = ColoramaSettings::ocean();
        break;
    case Preset::Neon:
        settings_.sourceMode = ColoramaSourceMode::Hue;
        settings_.palette = ColoramaPalette::Neon;
        settings_.phase = 0.0f;
        settings_.spread = 1.0f;
        settings_.strength = 1.0f;
        settings_.saturationBoost = 1.4f;
        settings_.contrast = 1.15f;
        settings_.preserveLuma = false;
        break;
    case Preset::Sunset:
        settings_.sourceMode = ColoramaSourceMode::Luma;
        settings_.palette = ColoramaPalette::Sunset;
        settings_.phase = 0.05f;
        settings_.spread = 1.0f;
        settings_.strength = 1.0f;
        settings_.saturationBoost = 1.1f;
        settings_.contrast = 1.05f;
        settings_.preserveLuma = true;
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void ColoramaEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void ColoramaEffect::setSourceMode(ColoramaSourceMode mode) {
    preset_ = Preset::Custom;
    settings_.sourceMode = mode;
    syncImpls();
}

void ColoramaEffect::setPalette(ColoramaPalette palette) {
    preset_ = Preset::Custom;
    settings_.palette = palette;
    syncImpls();
}

void ColoramaEffect::setPhase(float value) {
    preset_ = Preset::Custom;
    settings_.phase = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColoramaEffect::setSpread(float value) {
    preset_ = Preset::Custom;
    settings_.spread = std::max(0.0f, value);
    syncImpls();
}

void ColoramaEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ColoramaEffect::setSaturationBoost(float value) {
    preset_ = Preset::Custom;
    settings_.saturationBoost = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void ColoramaEffect::setContrast(float value) {
    preset_ = Preset::Custom;
    settings_.contrast = std::clamp(value, 0.0f, 2.5f);
    syncImpls();
}

void ColoramaEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void ColoramaEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ColoramaEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<ColoramaEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> ColoramaEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    AbstractProperty sourceModeProp;
    sourceModeProp.setName("Source Mode");
    sourceModeProp.setType(PropertyType::Integer);
    sourceModeProp.setValue(static_cast<int>(settings_.sourceMode));
    sourceModeProp.setDisplayPriority(-20);
    props.push_back(sourceModeProp);

    AbstractProperty paletteProp;
    paletteProp.setName("Palette");
    paletteProp.setType(PropertyType::Integer);
    paletteProp.setValue(static_cast<int>(settings_.palette));
    paletteProp.setDisplayPriority(-15);
    props.push_back(paletteProp);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-30);
    props.push_back(presetProp);

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    addFloat("Phase", settings_.phase);
    addFloat("Spread", settings_.spread);
    addFloat("Strength", settings_.strength);
    addFloat("Saturation Boost", settings_.saturationBoost);
    addFloat("Contrast", settings_.contrast);

    if (props.size() >= 8) {
        props[3].setDisplayPriority(0);
        props[4].setDisplayPriority(5);
        props[5].setDisplayPriority(10);
        props[6].setDisplayPriority(15);
        props[7].setDisplayPriority(20);
    }

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(25);
    props.push_back(preserveProp);

    return props;
}

void ColoramaEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Source Mode")) {
        setSourceMode(static_cast<ColoramaSourceMode>(value.toInt()));
    } else if (key == QStringLiteral("Palette")) {
        setPalette(static_cast<ColoramaPalette>(value.toInt()));
    } else if (key == QStringLiteral("Phase")) {
        setPhase(value.toFloat());
    } else if (key == QStringLiteral("Spread")) {
        setSpread(value.toFloat());
    } else if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
    } else if (key == QStringLiteral("Saturation Boost")) {
        setSaturationBoost(value.toFloat());
    } else if (key == QStringLiteral("Contrast")) {
        setContrast(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    }
}

} // namespace Artifact
