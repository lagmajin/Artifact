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

module TritoneEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Tritone;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

class TritoneEffectCPUImpl : public ArtifactEffectImplBase {
public:
    TritoneSettings settings_;
    TritoneProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        });
    }
};

class TritoneEffectGPUImpl : public ArtifactEffectImplBase {
public:
    TritoneSettings settings_;
    TritoneProcessor processor_;
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
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Tritone/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "TritoneParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Tritone/PSO"; desc.shaderSource=kTritoneHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("TritoneParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_ = true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Tritone/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "Tritone/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.shadowColor[0]=settings_.shadowColor.r; params.shadowColor[1]=settings_.shadowColor.g; params.shadowColor[2]=settings_.shadowColor.b; params.midtoneColor[0]=settings_.midtoneColor.r; params.midtoneColor[1]=settings_.midtoneColor.g; params.midtoneColor[2]=settings_.midtoneColor.b; params.highlightColor[0]=settings_.highlightColor.r; params.highlightColor[1]=settings_.highlightColor.g; params.highlightColor[2]=settings_.highlightColor.b; params.balance=settings_.balance; params.softness=settings_.softness; params.shadowStrength=settings_.shadowStrength; params.midtoneStrength=settings_.midtoneStrength; params.highlightStrength=settings_.highlightStrength; params.masterStrength=settings_.masterStrength; params.colorMix=settings_.colorMix; params.preserveLuma=settings_.preserveLuma ? 1.0f : 0.0f; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "Tritone/StagingTexture", src.image().colorDescriptor())) { applyCPU(src,dst); return; }
    }

private:
    struct ParamsCB { float shadowColor[3]{}; float balance=0.5f; float midtoneColor[3]{}; float softness=0.55f; float highlightColor[3]{}; float shadowStrength=1.0f; float midtoneStrength=1.0f; float highlightStrength=1.0f; float masterStrength=1.0f; float colorMix=0.85f; float preserveLuma=1.0f; float pad[2]{}; };
    static constexpr const char* kTritoneHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer TritoneParams : register(b0){ float3 g_ShadowColor; float g_Balance; float3 g_MidtoneColor; float g_Softness; float3 g_HighlightColor; float g_ShadowStrength; float g_MidtoneStrength; float g_HighlightStrength; float g_MasterStrength; float g_ColorMix; float g_PreserveLuma; float2 g_Pad; };
float luma(float3 c){ return dot(c,float3(0.299f,0.587f,0.114f)); }
float smoothstep01(float e0,float e1,float x){ float t=saturate((x-e0)/max(0.0001f,e1-e0)); return t*t*(3.0f-2.0f*t); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 c=px.rgb; float lum=luma(c); float s=smoothstep01(g_Balance-g_Softness*0.5f, g_Balance, lum); float hW=smoothstep01(g_Balance, g_Balance+g_Softness*0.5f, lum); float mW=saturate(1.0f-s-hW); float3 tritone=(g_ShadowColor*g_ShadowStrength*s + g_MidtoneColor*g_MidtoneStrength*mW + g_HighlightColor*g_HighlightStrength*hW) * g_MasterStrength; float3 mixed=lerp(c, tritone, saturate(g_ColorMix)); if(g_PreserveLuma>0.5f){ float mixedLum=luma(mixed); mixed += (lum-mixedLum).xxx; } px.rgb=saturate(mixed); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name, const auto& colorDescriptor){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp,colorDescriptor); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

namespace {
QColor toQColor(const TritoneColor& color) {
    return QColor::fromRgbF(color.r, color.g, color.b);
}

TritoneColor toColor(const QColor& color) {
    return {
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF())
    };
}
}

TritoneEffect::TritoneEffect() {
    setEffectID(UniString("effect.colorcorrection.tritone"));
    setDisplayName(UniString("Tritone"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<TritoneEffectCPUImpl>());
    setGPUImpl(std::make_shared<TritoneEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

TritoneEffect::~TritoneEffect() = default;

void TritoneEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Cinematic:
        settings_ = TritoneSettings::cinematic();
        break;
    case Preset::TealAndOrange:
        settings_ = TritoneSettings::tealAndOrange();
        break;
    case Preset::WarmGold:
        settings_.shadowColor = {0.12f, 0.18f, 0.24f};
        settings_.midtoneColor = {0.52f, 0.47f, 0.38f};
        settings_.highlightColor = {0.98f, 0.86f, 0.56f};
        settings_.balance = 0.50f;
        settings_.softness = 0.60f;
        settings_.shadowStrength = 0.85f;
        settings_.midtoneStrength = 0.90f;
        settings_.highlightStrength = 1.00f;
        settings_.masterStrength = 1.0f;
        settings_.colorMix = 0.88f;
        settings_.preserveLuma = true;
        break;
    case Preset::ColdBlue:
        settings_.shadowColor = {0.06f, 0.14f, 0.28f};
        settings_.midtoneColor = {0.42f, 0.50f, 0.58f};
        settings_.highlightColor = {0.82f, 0.92f, 1.00f};
        settings_.balance = 0.47f;
        settings_.softness = 0.58f;
        settings_.shadowStrength = 1.00f;
        settings_.midtoneStrength = 0.88f;
        settings_.highlightStrength = 0.92f;
        settings_.masterStrength = 1.0f;
        settings_.colorMix = 0.82f;
        settings_.preserveLuma = true;
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void TritoneEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void TritoneEffect::setShadowColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.shadowColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setMidtoneColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.midtoneColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setHighlightColor(const QColor& color) {
    preset_ = Preset::Custom;
    settings_.highlightColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setBalance(float value) {
    preset_ = Preset::Custom;
    settings_.balance = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setSoftness(float value) {
    preset_ = Preset::Custom;
    settings_.softness = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setMasterStrength(float value) {
    preset_ = Preset::Custom;
    settings_.masterStrength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setColorMix(float value) {
    preset_ = Preset::Custom;
    settings_.colorMix = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void TritoneEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void TritoneEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<TritoneEffectCPUImpl*>(cpuImpl().get())) {
        cpu->settings_ = settings_;
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<TritoneEffectGPUImpl*>(gpuImpl().get())) {
        gpu->settings_ = settings_;
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> TritoneEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(8);

    auto addColor = [&props](const char* name, const QColor& color) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        props.push_back(prop);
    };

    auto addFloat = [&props](const char* name, float value) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        props.push_back(prop);
    };

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-20);
    props.push_back(presetProp);

    addColor("Shadow Color", toQColor(settings_.shadowColor));
    addColor("Midtone Color", toQColor(settings_.midtoneColor));
    addColor("Highlight Color", toQColor(settings_.highlightColor));
    if (!props.empty()) {
        props[1].setDisplayPriority(-10);
        props[2].setDisplayPriority(-9);
        props[3].setDisplayPriority(-8);
    }
    addFloat("Balance", settings_.balance);
    addFloat("Softness", settings_.softness);
    addFloat("Master Strength", settings_.masterStrength);
    addFloat("Color Mix", settings_.colorMix);

    if (props.size() >= 8) {
        props[4].setDisplayPriority(0);
        props[5].setDisplayPriority(5);
        props[6].setDisplayPriority(10);
        props[7].setDisplayPriority(15);
    }

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(20);
    props.push_back(preserveProp);

    return props;
}

void TritoneEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Shadow Color")) {
        setShadowColor(value.value<QColor>());
    } else if (key == QStringLiteral("Midtone Color")) {
        setMidtoneColor(value.value<QColor>());
    } else if (key == QStringLiteral("Highlight Color")) {
        setHighlightColor(value.value<QColor>());
    } else if (key == QStringLiteral("Balance")) {
        setBalance(value.toFloat());
    } else if (key == QStringLiteral("Softness")) {
        setSoftness(value.toFloat());
    } else if (key == QStringLiteral("Master Strength")) {
        setMasterStrength(value.toFloat());
    } else if (key == QStringLiteral("Color Mix")) {
        setColorMix(value.toFloat());
    } else if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
    } else if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
    }
}

} // namespace Artifact
