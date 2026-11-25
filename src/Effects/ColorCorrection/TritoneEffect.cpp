module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <QString>
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
import Image.GpuImageUpload;
import Image.ImageSurfaceView;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Graphics.SurfaceColorContract;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;
import Memory.SharedPtr;

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
        const bool premultiplied = src.image().colorDescriptor().alphaMode ==
            ArtifactCore::SurfaceAlphaMode::Premultiplied;
        const int width = dst.image().width();
        const int height = dst.image().height();
        (void)ArtifactCore::withMutableColorFloat4View(
            dst.image().surfaceView(), pixels, [&](auto& view) {
                ArtifactCore::Parallel::ForPixels(0, height, width, height, [&](int y) {
                    auto row = view.row(y);
                    for (int x = 0; x < width; ++x) {
                        auto pixel = row[x];
                        if (premultiplied && pixel.a <= 0.0f) continue;
                        const float alpha = premultiplied ? pixel.a : 1.0f;
                        float r = pixel.r / alpha;
                        float g = pixel.g / alpha;
                        float b = pixel.b / alpha;
                        processor_.applyPixel(r, g, b);
                        pixel.r = r * alpha;
                        pixel.g = g * alpha;
                        pixel.b = b * alpha;
                    }
                });
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
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable bool pipelineReady_ = false;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const bool premultiplied = src.image().colorDescriptor().alphaMode ==
            ArtifactCore::SurfaceAlphaMode::Premultiplied;
        const int width = dst.image().width();
        const int height = dst.image().height();
        (void)ArtifactCore::withMutableColorFloat4View(
            dst.image().surfaceView(), pixels, [&](auto& view) {
                ArtifactCore::Parallel::ForPixels(0, height, width, height, [&](int y) {
                    auto row = view.row(y);
                    for (int x = 0; x < width; ++x) {
                        auto pixel = row[x];
                        if (premultiplied && pixel.a <= 0.0f) continue;
                        const float alpha = premultiplied ? pixel.a : 1.0f;
                        float r = pixel.r / alpha;
                        float g = pixel.g / alpha;
                        float b = pixel.b / alpha;
                        processor_.applyPixel(r, g, b);
                        pixel.r = r * alpha;
                        pixel.g = g * alpha;
                        pixel.b = b * alpha;
                    }
                });
        });
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) { applyCPU(src, dst); return; }
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Tritone/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "TritoneParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Tritone/PSO"; desc.shaderSource=kTritoneHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) || !executor_->setBuffer("TritoneParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_ = true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Tritone/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "Tritone/OutputTexture"; if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width || outputTex_->GetDesc().Height != outDesc.Height || outputTex_->GetDesc().Format != outDesc.Format || outputTex_->GetDesc().BindFlags != outDesc.BindFlags) { outputTex_.Release(); device_->CreateTexture(outDesc,nullptr,&outputTex_); } if (!outputTex_) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.shadowColor[0]=settings_.shadowColor.r; params.shadowColor[1]=settings_.shadowColor.g; params.shadowColor[2]=settings_.shadowColor.b; params.midtoneColor[0]=settings_.midtoneColor.r; params.midtoneColor[1]=settings_.midtoneColor.g; params.midtoneColor[2]=settings_.midtoneColor.b; params.highlightColor[0]=settings_.highlightColor.r; params.highlightColor[1]=settings_.highlightColor.g; params.highlightColor[2]=settings_.highlightColor.b; params.balance=settings_.balance; params.softness=settings_.softness; params.shadowStrength=settings_.shadowStrength; params.midtoneStrength=settings_.midtoneStrength; params.highlightStrength=settings_.highlightStrength; params.masterStrength=settings_.masterStrength; params.colorMix=settings_.colorMix; params.preserveLuma=settings_.preserveLuma ? 1.0f : 0.0f; params.premultiplied=src.image().colorDescriptor().alphaMode == ArtifactCore::SurfaceAlphaMode::Premultiplied ? 1.0f : 0.0f; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex_, dst, "Tritone/StagingTexture", src.image().colorDescriptor())) { applyCPU(src,dst); return; }
    }

private:
    struct ParamsCB { float shadowColor[3]{}; float balance=0.5f; float midtoneColor[3]{}; float softness=0.55f; float highlightColor[3]{}; float shadowStrength=1.0f; float midtoneStrength=1.0f; float highlightStrength=1.0f; float masterStrength=1.0f; float colorMix=0.85f; float preserveLuma=1.0f; float premultiplied=0.0f; float pad[2]{}; };
    static_assert(sizeof(ParamsCB) == 80);
    static constexpr const char* kTritoneHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer TritoneParams : register(b0){ float3 g_ShadowColor; float g_Balance; float3 g_MidtoneColor; float g_Softness; float3 g_HighlightColor; float g_ShadowStrength; float g_MidtoneStrength; float g_HighlightStrength; float g_MasterStrength; float g_ColorMix; float g_PreserveLuma; float g_Premultiplied; float g_Pad; };
float luma(float3 c){ return dot(c,float3(0.2126f,0.7152f,0.0722f)); }
float smoothstep01(float e0,float e1,float x){ if(e0==e1) return x<e0 ? 0.0f : 1.0f; float t=saturate((x-e0)/(e1-e0)); return t*t*(3.0f-2.0f*t); }
float3 preserveLuma(float sourceLuma,float3 color,float preserveAmount){ float targetLuma=luma(color); if(targetLuma<=1.0e-6f) return color; float scale=sourceLuma/targetLuma; float3 preserved=saturate(color*scale); return lerp(color,preserved,preserveAmount); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=(g_InputTexture[dtid.xy]).bgra; if(g_Premultiplied>0.5f&&px.a<=0.0f){g_OutputTexture[dtid.xy]=(px).bgra;return;} float alpha=g_Premultiplied>0.5f?px.a:1.0f; float3 c=px.rgb/alpha; float lum=luma(c); float shadowWeight=1.0f-smoothstep01(g_Balance-g_Softness,g_Balance,lum); float highlightWeight=smoothstep01(g_Balance,g_Balance+g_Softness,lum); float midWeight=saturate(1.0f-shadowWeight-highlightWeight); float3 target=g_ShadowColor*shadowWeight*g_ShadowStrength + g_MidtoneColor*midWeight*g_MidtoneStrength + g_HighlightColor*highlightWeight*g_HighlightStrength; target=saturate(target); float mixAmount=saturate(g_MasterStrength*g_ColorMix); float3 mixed=lerp(c,target,mixAmount); if(g_PreserveLuma>0.5f) mixed=preserveLuma(lum,mixed,saturate(g_MasterStrength)); px.rgb=saturate(mixed)*alpha; g_OutputTexture[dtid.xy]=(px).bgra; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); if(!device||!outTex||img.width()<=0||img.height()<=0) return false; const auto upload=ArtifactCore::makeGpuImageUploadBuffer(img.surfaceView(), ArtifactCore::GpuImageChannelOrder::Bgra); if(!upload.isValid()) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=upload.data(); sub.Stride=static_cast<Diligent::Uint64>(upload.rowStride); Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name, const auto& colorDescriptor){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; const bool copied=dst.image().setFromGpuBGRA32F(static_cast<const float*>(mapped.pData),static_cast<int>(desc.Width),static_cast<int>(desc.Height),static_cast<std::size_t>(mapped.Stride),colorDescriptor); ctx->UnmapTextureSubresource(staging,0,0); return copied; }
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
    setCPUImpl(ArtifactCore::makeShared<TritoneEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<TritoneEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(Preset::Cinematic);
    syncImpls();
}

TritoneEffect::~TritoneEffect() = default;

TritoneSettings TritoneEffect::settingsForPreset(Preset preset) {
    switch (preset) {
    case Preset::Cinematic:
        return TritoneSettings::cinematic();
    case Preset::TealAndOrange:
        return TritoneSettings::tealAndOrange();
    case Preset::WarmGold:
    {
        TritoneSettings settings;
        settings.shadowColor = {0.12f, 0.18f, 0.24f};
        settings.midtoneColor = {0.52f, 0.47f, 0.38f};
        settings.highlightColor = {0.98f, 0.86f, 0.56f};
        settings.balance = 0.50f;
        settings.softness = 0.60f;
        settings.shadowStrength = 0.85f;
        settings.midtoneStrength = 0.90f;
        settings.highlightStrength = 1.00f;
        settings.masterStrength = 1.0f;
        settings.colorMix = 0.88f;
        settings.preserveLuma = true;
        return settings;
    }
    case Preset::ColdBlue:
    {
        TritoneSettings settings;
        settings.shadowColor = {0.06f, 0.14f, 0.28f};
        settings.midtoneColor = {0.42f, 0.50f, 0.58f};
        settings.highlightColor = {0.82f, 0.92f, 1.00f};
        settings.balance = 0.47f;
        settings.softness = 0.58f;
        settings.shadowStrength = 1.00f;
        settings.midtoneStrength = 0.88f;
        settings.highlightStrength = 0.92f;
        settings.masterStrength = 1.0f;
        settings.colorMix = 0.82f;
        settings.preserveLuma = true;
        return settings;
    }
    case Preset::Custom:
    default:
        return TritoneSettings{};
    }
}

void TritoneEffect::applyPreset(Preset preset) {
    customPresetPinned_ = preset == Preset::Custom;
    if (customPresetPinned_) {
        return;
    }
    settings_ = settingsForPreset(preset);
}

int TritoneEffect::preset() const {
    if (customPresetPinned_) {
        return static_cast<int>(Preset::Custom);
    }

    constexpr float kTolerance = 0.00002f;
    const auto close = [kTolerance](float lhs, float rhs) {
        return std::abs(lhs - rhs) <= kTolerance;
    };
    const auto matches = [this, &close](const TritoneSettings& candidate) {
        return close(settings_.shadowColor.r, candidate.shadowColor.r) &&
               close(settings_.shadowColor.g, candidate.shadowColor.g) &&
               close(settings_.shadowColor.b, candidate.shadowColor.b) &&
               close(settings_.midtoneColor.r, candidate.midtoneColor.r) &&
               close(settings_.midtoneColor.g, candidate.midtoneColor.g) &&
               close(settings_.midtoneColor.b, candidate.midtoneColor.b) &&
               close(settings_.highlightColor.r, candidate.highlightColor.r) &&
               close(settings_.highlightColor.g, candidate.highlightColor.g) &&
               close(settings_.highlightColor.b, candidate.highlightColor.b) &&
               close(settings_.balance, candidate.balance) &&
               close(settings_.softness, candidate.softness) &&
               close(settings_.shadowStrength, candidate.shadowStrength) &&
               close(settings_.midtoneStrength, candidate.midtoneStrength) &&
               close(settings_.highlightStrength, candidate.highlightStrength) &&
               close(settings_.masterStrength, candidate.masterStrength) &&
               close(settings_.colorMix, candidate.colorMix) &&
               settings_.preserveLuma == candidate.preserveLuma;
    };

    for (int presetValue = static_cast<int>(Preset::Cinematic);
         presetValue <= static_cast<int>(Preset::ColdBlue); ++presetValue) {
        const auto candidate = settingsForPreset(static_cast<Preset>(presetValue));
        if (matches(candidate)) {
            return presetValue;
        }
    }
    return static_cast<int>(Preset::Custom);
}

void TritoneEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 4);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void TritoneEffect::setShadowColor(const QColor& color) {
    settings_.shadowColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setMidtoneColor(const QColor& color) {
    settings_.midtoneColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setHighlightColor(const QColor& color) {
    settings_.highlightColor = toColor(color);
    syncImpls();
}

void TritoneEffect::setBalance(float value) {
    settings_.balance = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}

void TritoneEffect::setSoftness(float value) {
    settings_.softness = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.55f;
    syncImpls();
}

void TritoneEffect::setMasterStrength(float value) {
    settings_.masterStrength = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    syncImpls();
}

void TritoneEffect::setColorMix(float value) {
    settings_.colorMix = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.85f;
    syncImpls();
}

void TritoneEffect::setPreserveLuma(bool value) {
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
    props.reserve(9);
    const TritoneSettings defaultSettings = TritoneSettings::cinematic();

    auto addColor = [&props](const char* name, const QColor& color,
                             const QColor& defaultColor) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setDisplayLabel(QString::fromLatin1(name));
        prop.setType(PropertyType::Color);
        prop.setColorValue(color);
        prop.setValue(color);
        prop.setDefaultValue(defaultColor);
        prop.setTooltip(QStringLiteral("Target color for this tonal range."));
        props.push_back(prop);
    };

    auto addFloat = [&props](const char* name, float value, float defaultValue,
                             const QString& tooltip) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setDisplayLabel(QString::fromLatin1(name));
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDefaultValue(static_cast<double>(defaultValue));
        prop.setMinValue(QVariant(0.0));
        prop.setMaxValue(QVariant(1.0));
        prop.setHardRange(0.0, 1.0);
        prop.setSoftRange(0.0, 1.0);
        prop.setStep(0.01);
        prop.setUnit(QStringLiteral("%"));
        prop.setTooltip(tooltip);
        prop.setAnimatable(true);
        props.push_back(prop);
    };

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDefaultValue(static_cast<int>(Preset::Cinematic));
    presetProp.setDisplayLabel(QStringLiteral("Preset"));
    presetProp.setHardRange(0.0, 4.0);
    presetProp.setSoftRange(0.0, 4.0);
    presetProp.setStep(1.0);
    presetProp.setDisplayPriority(-30);
    presetProp.setAnimatable(false);
    presetProp.setTooltip(QStringLiteral("Choose a built-in Tritone preset."));
    props.push_back(presetProp);

    addColor("Shadow Color", toQColor(settings_.shadowColor),
             toQColor(defaultSettings.shadowColor));
    addColor("Midtone Color", toQColor(settings_.midtoneColor),
             toQColor(defaultSettings.midtoneColor));
    addColor("Highlight Color", toQColor(settings_.highlightColor),
             toQColor(defaultSettings.highlightColor));
    if (!props.empty()) {
        props[1].setDisplayPriority(-10);
        props[2].setDisplayPriority(-9);
        props[3].setDisplayPriority(-8);
    }
    addFloat("Balance", settings_.balance, defaultSettings.balance,
             QStringLiteral("Position of the tonal balance point."));
    addFloat("Softness", settings_.softness, defaultSettings.softness,
             QStringLiteral("Blend width between shadow, midtone, and highlight ranges."));
    addFloat("Master Strength", settings_.masterStrength,
             defaultSettings.masterStrength,
             QStringLiteral("Overall strength of the Tritone grade."));
    addFloat("Color Mix", settings_.colorMix, defaultSettings.colorMix,
             QStringLiteral("Blend between the original and colorized result."));

    if (props.size() >= 8) {
        props[4].setDisplayPriority(0);
        props[5].setDisplayPriority(5);
        props[6].setDisplayPriority(10);
        props[7].setDisplayPriority(15);
    }

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setDisplayLabel(QStringLiteral("Preserve Luma"));
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDefaultValue(defaultSettings.preserveLuma);
    preserveProp.setTooltip(QStringLiteral(
        "Preserve source luminance while applying the Tritone colors."));
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
