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

module SelectiveColorEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.SelectiveColor;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

class SelectiveColorEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SelectiveColorSettings settings_;
    ArtifactCore::SelectiveColorProcessor processor_;

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

class SelectiveColorEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SelectiveColorSettings settings_;
    ArtifactCore::SelectiveColorProcessor processor_;
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
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="SelectiveColor/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "SelectiveColorParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="SelectiveColor/PSO"; desc.shaderSource=kSelectiveColorHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("SelectiveColorParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_ = true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "SelectiveColor/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "SelectiveColor/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.strength=settings_.strength; params.relativeMode=settings_.relativeMode?1.0f:0.0f; params.preserveLuma=settings_.preserveLuma?1.0f:0.0f; for (int i=0;i<9;++i){ params.groups[i][0]=settings_.groups[i].cyan; params.groups[i][1]=settings_.groups[i].magenta; params.groups[i][2]=settings_.groups[i].yellow; params.groups[i][3]=settings_.groups[i].black; } std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "SelectiveColor/StagingTexture")) { applyCPU(src,dst); return; }
    }

private:
    struct ParamsCB { float strength=1.0f; float relativeMode=0.0f; float preserveLuma=1.0f; float pad0=0.0f; float groups[9][4]{}; };
    static constexpr const char* kSelectiveColorHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer SelectiveColorParams : register(b0){ float g_Strength; float g_RelativeMode; float g_PreserveLuma; float g_Pad0; float4 g_Groups[9]; };
float luma(float3 c){ return dot(c,float3(0.299f,0.587f,0.114f)); }
float3 adjustGroup(float3 c, float4 adj){ float3 rgb = c + adj.rgb * adj.a; return rgb; }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 c=px.rgb; float lum=luma(c); int groupIdx=8; if(lum < 0.11f) groupIdx=8; else if(lum < 0.22f) groupIdx=0; else if(lum < 0.33f) groupIdx=1; else if(lum < 0.44f) groupIdx=2; else if(lum < 0.55f) groupIdx=3; else if(lum < 0.66f) groupIdx=4; else if(lum < 0.77f) groupIdx=5; else if(lum < 0.88f) groupIdx=6; else groupIdx=7; float4 adj = g_Groups[groupIdx]; float3 outRgb = c + adj.rgb * adj.a * g_Strength; if(g_PreserveLuma > 0.5f){ float outLum=luma(outRgb); outRgb += (lum-outLum).xxx; } px.rgb=saturate(outRgb); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

namespace {
constexpr std::pair<const char*, ArtifactCore::SelectiveColorGroup> kGroupNames[] = {
    {"Reds", ArtifactCore::SelectiveColorGroup::Reds},
    {"Yellows", ArtifactCore::SelectiveColorGroup::Yellows},
    {"Greens", ArtifactCore::SelectiveColorGroup::Greens},
    {"Cyans", ArtifactCore::SelectiveColorGroup::Cyans},
    {"Blues", ArtifactCore::SelectiveColorGroup::Blues},
    {"Magentas", ArtifactCore::SelectiveColorGroup::Magentas},
    {"Whites", ArtifactCore::SelectiveColorGroup::Whites},
    {"Neutrals", ArtifactCore::SelectiveColorGroup::Neutrals},
    {"Blacks", ArtifactCore::SelectiveColorGroup::Blacks},
};
}

SelectiveColorEffect::SelectiveColorEffect() {
    setEffectID(UniString("effect.colorcorrection.selectivecolor"));
    setDisplayName(UniString("Selective Color"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SelectiveColorEffectCPUImpl>());
    setGPUImpl(std::make_shared<SelectiveColorEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

SelectiveColorEffect::~SelectiveColorEffect() = default;

void SelectiveColorEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Neutral:
        settings_ = ArtifactCore::SelectiveColorSettings::neutral();
        break;
    case Preset::Warm:
        settings_ = ArtifactCore::SelectiveColorSettings::warm();
        break;
    case Preset::Cool:
        settings_ = ArtifactCore::SelectiveColorSettings::cool();
        break;
    case Preset::Vivid:
        settings_ = ArtifactCore::SelectiveColorSettings::vivid();
        break;
    case Preset::Film:
        settings_ = ArtifactCore::SelectiveColorSettings::film();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void SelectiveColorEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void SelectiveColorEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void SelectiveColorEffect::setRelativeMode(bool value) {
    preset_ = Preset::Custom;
    settings_.relativeMode = value;
    syncImpls();
}

void SelectiveColorEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void SelectiveColorEffect::setAdjustment(ArtifactCore::SelectiveColorGroup group, float c, float m, float y, float k) {
    preset_ = Preset::Custom;
    auto& adj = settings_.groups[static_cast<size_t>(group)];
    adj.cyan = std::clamp(c, -1.0f, 1.0f);
    adj.magenta = std::clamp(m, -1.0f, 1.0f);
    adj.yellow = std::clamp(y, -1.0f, 1.0f);
    adj.black = std::clamp(k, -1.0f, 1.0f);
    syncImpls();
}

void SelectiveColorEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<SelectiveColorEffectCPUImpl*>(cpuImpl().get())) {
        cpu->settings_ = settings_;
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<SelectiveColorEffectGPUImpl*>(gpuImpl().get())) {
        gpu->settings_ = settings_;
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> SelectiveColorEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(40);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-40);
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(settings_.strength)));
    strengthProp.setDisplayPriority(-30);
    props.push_back(strengthProp);

    AbstractProperty relativeProp;
    relativeProp.setName("Relative Mode");
    relativeProp.setType(PropertyType::Boolean);
    relativeProp.setValue(settings_.relativeMode);
    relativeProp.setDisplayPriority(-20);
    props.push_back(relativeProp);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(-10);
    props.push_back(preserveProp);

    auto addFloat = [&props](const QString& name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    int basePriority = 0;
    for (const auto& [groupName, group] : kGroupNames) {
        const auto& adj = settings_.groups[static_cast<size_t>(group)];
        addFloat(QStringLiteral("%1 Cyan").arg(groupName), adj.cyan, basePriority + 0);
        addFloat(QStringLiteral("%1 Magenta").arg(groupName), adj.magenta, basePriority + 1);
        addFloat(QStringLiteral("%1 Yellow").arg(groupName), adj.yellow, basePriority + 2);
        addFloat(QStringLiteral("%1 Black").arg(groupName), adj.black, basePriority + 3);
        basePriority += 10;
    }

    return props;
}

void SelectiveColorEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
        return;
    }
    if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
        return;
    }
    if (key == QStringLiteral("Relative Mode")) {
        setRelativeMode(value.toBool());
        return;
    }
    if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
        return;
    }

    for (const auto& [groupName, group] : kGroupNames) {
        if (key == QStringLiteral("%1 Cyan").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, value.toFloat(), adj.magenta, adj.yellow, adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Magenta").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, value.toFloat(), adj.yellow, adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Yellow").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, adj.magenta, value.toFloat(), adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Black").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, adj.magenta, adj.yellow, value.toFloat());
            return;
        }
    }
}

} // namespace Artifact
