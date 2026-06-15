module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Glow.ReactiveGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius) {
    const int estimated = static_cast<int>(std::ceil(std::max(0.5f, radius) * 2.5f));
    return std::max(3, (estimated * 2) + 1);
}

} // namespace

class ReactiveGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;
    float radius_ = 14.0f;
    float intensity_ = 1.0f;
    float reaction_ = 0.8f;
    float saturationWeight_ = 0.65f;
    float tintMix_ = 0.25f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4,
                       const_cast<float*>(srcData));
        std::vector<cv::Mat> channels;
        cv::split(srcMat, channels);

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        cv::Mat gray = channels[0] * 0.114f +
                       channels[1] * 0.587f +
                       channels[2] * 0.299f;

        cv::Mat maxCh;
        cv::Mat minCh;
        cv::max(channels[0], channels[1], maxCh);
        cv::max(maxCh, channels[2], maxCh);
        cv::min(channels[0], channels[1], minCh);
        cv::min(minCh, channels[2], minCh);

        cv::Mat saturation = maxCh - minCh;
        cv::Mat reactionMask;
        cv::subtract(gray, cv::Scalar::all(threshold_), reactionMask);
        cv::threshold(reactionMask, reactionMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold_ < 0.999f) {
            reactionMask *= 1.0f / std::max(0.001f, 1.0f - threshold_);
        }
        reactionMask = reactionMask * reaction_ + saturation * saturationWeight_;
        cv::threshold(reactionMask, reactionMask, 1.0, 1.0, cv::THRESH_TRUNC);

        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{reactionMask, reactionMask, reactionMask}, mask3);
        cv::Mat bloom = color.mul(mask3);

        const int ksize = kernelSizeForRadius(radius_);
        cv::GaussianBlur(bloom, bloom, cv::Size(ksize, ksize),
                         std::max(0.1f, radius_), std::max(0.1f, radius_),
                         cv::BORDER_REPLICATE);

        cv::Mat result = color + bloom * intensity_ * (0.5f + tintMix_);
        cv::Mat desat = color * (1.0f - tintMix_) + bloom * tintMix_;
        result = result * (1.0f - 0.35f * tintMix_) + desat * (0.35f * tintMix_);
        cv::min(result, cv::Scalar::all(1.0), result);
        cv::max(result, cv::Scalar::all(0.0), result);

        std::vector<cv::Mat> outChannels;
        cv::split(result, outChannels);
        outChannels.push_back(alpha);
        cv::Mat outMat;
        cv::merge(outChannels, outMat);
        dst.image().setFromRGBA32F(outMat.ptr<float>(), outMat.cols, outMat.rows);
    }
};

class ReactiveGlowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;
    float radius_ = 14.0f;
    float intensity_ = 1.0f;
    float reaction_ = 0.8f;
    float saturationWeight_ = 0.65f;
    float tintMix_ = 0.25f;
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
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="ReactiveGlow/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "ReactiveGlowParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="ReactiveGlow/PSO"; desc.shaderSource=kReactiveGlowHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("ReactiveGlowParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "ReactiveGlow/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc(); outDesc.Usage = Diligent::USAGE_DEFAULT; outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name = "ReactiveGlow/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.threshold=threshold_; params.radius=radius_; params.intensity=intensity_; params.reaction=reaction_; params.saturationWeight=saturationWeight_; params.tintMix=tintMix_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "ReactiveGlow/StagingTexture")) { applyCPU(src,dst); return; }
    }

private:
    struct ParamsCB { float threshold=0.5f; float radius=14.0f; float intensity=1.0f; float reaction=0.8f; float saturationWeight=0.65f; float tintMix=0.25f; float pad[2]{}; };
    ReactiveGlowEffectCPUImpl cpuImpl_;
    static constexpr const char* kReactiveGlowHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ReactiveGlowParams : register(b0){ float g_Threshold; float g_Radius; float g_Intensity; float g_Reaction; float g_SaturationWeight; float g_TintMix; float2 g_Pad; };
float luma(float3 c){ return dot(c,float3(0.299f,0.587f,0.114f)); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; float4 px=g_InputTexture[dtid.xy]; float3 c=px.rgb; float lum=luma(c); float sat=max(c.r,max(c.g,c.b))-min(c.r,min(c.g,c.b)); float mask=saturate((lum-g_Threshold)/max(0.0001f,1.0f-g_Threshold)); mask = saturate(mask*g_Reaction + sat*g_SaturationWeight); float3 glow = lerp(float3(mask,mask,mask), c*mask, g_TintMix); px.rgb = saturate(c + glow * g_Intensity); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

ReactiveGlowEffect::ReactiveGlowEffect() {
    setEffectID(UniString("reactive_glow"));
    setDisplayName(UniString("Reactive Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ReactiveGlowEffectCPUImpl>());
    setGPUImpl(std::make_shared<ReactiveGlowEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ReactiveGlowEffect::~ReactiveGlowEffect() = default;

void ReactiveGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<ReactiveGlowEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->radius_ = radius_;
        cpu->intensity_ = intensity_;
        cpu->reaction_ = reaction_;
        cpu->saturationWeight_ = saturationWeight_;
        cpu->tintMix_ = tintMix_;
    }
    if (auto gpu = std::dynamic_pointer_cast<ReactiveGlowEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->radius_ = radius_;
        gpu->intensity_ = intensity_;
        gpu->reaction_ = reaction_;
        gpu->saturationWeight_ = saturationWeight_;
        gpu->tintMix_ = tintMix_;
    }
}

std::vector<AbstractProperty> ReactiveGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

    auto& thresholdProp = props.emplace_back();
    thresholdProp.setName("Threshold");
    thresholdProp.setType(PropertyType::Float);
    thresholdProp.setValue(threshold_);

    auto& radiusProp = props.emplace_back();
    radiusProp.setName("Radius");
    radiusProp.setType(PropertyType::Float);
    radiusProp.setValue(radius_);

    auto& intensityProp = props.emplace_back();
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);

    auto& reactionProp = props.emplace_back();
    reactionProp.setName("Reaction");
    reactionProp.setType(PropertyType::Float);
    reactionProp.setValue(reaction_);

    auto& saturationProp = props.emplace_back();
    saturationProp.setName("Saturation Weight");
    saturationProp.setType(PropertyType::Float);
    saturationProp.setValue(saturationWeight_);

    auto& tintProp = props.emplace_back();
    tintProp.setName("Tint Mix");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tintMix_);

    return props;
}

void ReactiveGlowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) {
        setThreshold(value.toFloat());
    } else if (key == QStringLiteral("Radius")) {
        setRadius(value.toFloat());
    } else if (key == QStringLiteral("Intensity")) {
        setIntensity(value.toFloat());
    } else if (key == QStringLiteral("Reaction")) {
        setReaction(value.toFloat());
    } else if (key == QStringLiteral("Saturation Weight")) {
        setSaturationWeight(value.toFloat());
    } else if (key == QStringLiteral("Tint Mix")) {
        setTintMix(value.toFloat());
    }
}

} // namespace Artifact
