module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QStringList>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.WhiteBalance;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

static void kelvinToRGB(float kelvin, float& r, float& g, float& b) {
    const float temp = kelvin / 100.0f;

    if (temp <= 66.0f) r = 1.0f;
    else r = std::clamp(1.2929361860603f * std::pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

    if (temp <= 66.0f) g = std::clamp(0.39008157876902f * std::log(temp) - 0.63184144378863f, 0.0f, 1.0f);
    else g = std::clamp(1.1298908608953f * std::pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

    if (temp >= 66.0f) b = 1.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = std::clamp(0.54320678911019f * std::log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);
}

class WhiteBalanceCPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        float refR = 1.0f;
        float refG = 1.0f;
        float refB = 1.0f;
        kelvinToRGB(6500.0f, refR, refG, refB);

        float targetR = 1.0f;
        float targetG = 1.0f;
        float targetB = 1.0f;
        kelvinToRGB(temperature_, targetR, targetG, targetB);

        const float corrR = targetR / std::max(refR, 0.001f);
        const float corrG = targetG / std::max(refG, 0.001f);
        const float corrB = targetB / std::max(refB, 0.001f);
        const float tintG = 1.0f + tint_ * 0.5f;
        const float tintM = 1.0f - tint_ * 0.5f;
        const float brightMul = std::pow(2.0f, brightness_);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* p = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                p[2] = std::clamp(p[2] * corrR * tintM * brightMul, 0.0f, 1.0f);
                p[1] = std::clamp(p[1] * corrG * tintG * brightMul, 0.0f, 1.0f);
                p[0] = std::clamp(p[0] * corrB * tintM * brightMul, 0.0f, 1.0f);
            }
        }
    }
};

class WhiteBalanceGPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="WhiteBalance/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "WhiteBalanceParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="WhiteBalance/PSO"; desc.shaderSource=kWhiteBalanceHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("WhiteBalanceParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "WhiteBalance/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="WhiteBalance/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.temperature=temperature_; params.tint=tint_; params.brightness=brightness_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "WhiteBalance/StagingTexture")) { applyCPU(src,dst); return; }
    }

private:
    WhiteBalanceCPUImpl cpuImpl_;
    struct ParamsCB { float temperature=6500.0f; float tint=0.0f; float brightness=0.0f; float pad=0.0f; };
    static constexpr const char* kWhiteBalanceHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer WhiteBalanceParams : register(b0){ float g_Temperature; float g_Tint; float g_Brightness; float g_Pad; };
float3 kelvinToRGB(float kelvin){ float temp=kelvin/100.0f; float r = (temp<=66.0f) ? 1.0f : saturate(1.2929361860603f * pow(max(temp-60.0f, 0.0001f), -0.1332047592f)); float g = (temp<=66.0f) ? saturate(0.39008157876902f * log(max(temp, 0.0001f)) - 0.63184144378863f) : saturate(1.1298908608953f * pow(max(temp-60.0f,0.0001f), -0.0755148492f)); float b = (temp>=66.0f) ? 1.0f : ((temp<=19.0f) ? 0.0f : saturate(0.54320678911019f * log(max(temp-10.0f, 0.0001f)) - 1.19625408914f)); return float3(r,g,b); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 ref=kelvinToRGB(6500.0f); float3 tgt=kelvinToRGB(g_Temperature); float3 corr=tgt / max(ref, float3(0.001f,0.001f,0.001f)); float tintG=1.0f + g_Tint * 0.5f; float tintM=1.0f - g_Tint * 0.5f; float brightMul = exp2(g_Brightness); px.rgb = saturate(px.rgb * float3(corr.r * tintM * brightMul, corr.g * tintG * brightMul, corr.b * tintM * brightMul)); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

WhiteBalanceEffect::WhiteBalanceEffect() {
    setDisplayName(UniString("White Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<WhiteBalanceCPUImpl>());
    setGPUImpl(std::make_shared<WhiteBalanceGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

WhiteBalanceEffect::~WhiteBalanceEffect() = default;

void WhiteBalanceEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<WhiteBalanceCPUImpl>(cpuImpl())) {
        cpu->temperature_ = temperature_;
        cpu->tint_ = tint_;
        cpu->brightness_ = brightness_;
    }
    if (auto gpu = std::dynamic_pointer_cast<WhiteBalanceGPUImpl>(gpuImpl())) {
        gpu->temperature_ = temperature_;
        gpu->tint_ = tint_;
        gpu->brightness_ = brightness_;
    }
}

void WhiteBalanceEffect::setPreset(const QString& preset) {
    if (preset == QStringLiteral("Daylight")) setTemperature(5500.0f);
    else if (preset == QStringLiteral("Cloudy")) setTemperature(6500.0f);
    else if (preset == QStringLiteral("Shade")) setTemperature(7500.0f);
    else if (preset == QStringLiteral("Tungsten")) setTemperature(3200.0f);
    else if (preset == QStringLiteral("Fluorescent")) setTemperature(4000.0f);
    else if (preset == QStringLiteral("Flash")) setTemperature(6000.0f);
}

std::vector<AbstractProperty> WhiteBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty tempProp;
    tempProp.setName("Temperature (K)");
    tempProp.setType(PropertyType::Float);
    tempProp.setValue(temperature_);
    tempProp.setDisplayPriority(-10);
    props.push_back(tempProp);

    AbstractProperty tintProp;
    tintProp.setName("Tint");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tint_);
    tintProp.setDisplayPriority(0);
    props.push_back(tintProp);

    AbstractProperty brightnessProp;
    brightnessProp.setName("Brightness");
    brightnessProp.setType(PropertyType::Float);
    brightnessProp.setValue(brightness_);
    brightnessProp.setDisplayPriority(10);
    props.push_back(brightnessProp);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(0);
    presetProp.setDisplayPriority(-20);
    props.push_back(presetProp);

    return props;
}

void WhiteBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Temperature (K)")) setTemperature(value.toFloat());
    else if (key == QStringLiteral("Tint")) setTint(value.toFloat());
    else if (key == QStringLiteral("Brightness")) setBrightness(value.toFloat());
    else if (key == QStringLiteral("Preset")) {
        const QStringList presets = {
            QStringLiteral("Custom"),
            QStringLiteral("Daylight"),
            QStringLiteral("Cloudy"),
            QStringLiteral("Shade"),
            QStringLiteral("Tungsten"),
            QStringLiteral("Fluorescent"),
            QStringLiteral("Flash")
        };
        const int idx = value.toInt();
        if (idx > 0 && idx < presets.size()) {
            setPreset(presets[idx]);
        }
    }
}

} // namespace Artifact
