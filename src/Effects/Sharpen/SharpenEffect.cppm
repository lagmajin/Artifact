module;
#include <algorithm>
#include <memory>
#include <utility>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <QString>
#include <QVariant>
#include <QVector>

module Artifact.Effect.Rasterizer.Sharpen;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

class SharpenEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 1.0f;
    float sigma_ = 1.0f;
    float threshold_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;

        cv::Mat floatMat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::vector<cv::Mat> channels;
        cv::split(floatMat, channels);
        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        const int ksize = std::max(3, static_cast<int>(sigma_ * 6.0f) | 1);
        cv::Mat blurred;
        cv::GaussianBlur(color, blurred, cv::Size(ksize, ksize), std::max(0.1f, sigma_), std::max(0.1f, sigma_), cv::BORDER_REPLICATE);

        cv::Mat result = color + (color - blurred) * amount_;
        if (threshold_ > 0.0f) {
            cv::Mat diff = cv::abs(color - blurred) * amount_;
            cv::Mat mask;
            cv::compare(diff, threshold_, diff, cv::CMP_GT);
            color.copyTo(result, ~diff);
        }
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> outChannels;
        cv::split(result, outChannels);
        outChannels.push_back(alpha);
        cv::merge(outChannels, floatMat);
    }
};

class SharpenEffectGPUImpl : public ArtifactEffectImplBase {
public:
    SharpenEffectCPUImpl cpuImpl_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override { cpuImpl_.applyCPU(src, dst); }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Sharpen/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "SharpenParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Sharpen/PSO"; desc.shaderSource=kSharpenHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("SharpenParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Sharpen/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="Sharpen/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.amount=cpuImpl_.amount_; params.sigma=cpuImpl_.sigma_; params.threshold=cpuImpl_.threshold_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "Sharpen/StagingTexture")) { applyCPU(src,dst); return; }
    }
private:
    struct ParamsCB { float amount=1.0f; float sigma=1.0f; float threshold=0.0f; float pad=0.0f; };
    static constexpr const char* kSharpenHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer SharpenParams : register(b0){ float g_Amount; float g_Sigma; float g_Threshold; float g_Pad; };
float4 sampleTex(Texture2D<float4> tex, int2 p, uint w, uint h){ p.x=clamp(p.x,0,(int)w-1); p.y=clamp(p.y,0,(int)h-1); return tex[uint2(p)]; }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ uint w,h; g_OutputTexture.GetDimensions(w,h); if(dtid.x>=w||dtid.y>=h) return; int r=max(1,(int)ceil(max(0.1f,g_Sigma)*3.0f)); float sigma=max(0.1f,g_Sigma); float4 center=g_InputTexture[dtid.xy]; float4 blur=0; float weightSum=0; [loop] for(int y=-r;y<=r;++y){ [loop] for(int x=-r;x<=r;++x){ float d=(float)(x*x+y*y); float wgt=exp(-0.5f*d/max(0.0001f,sigma*sigma)); blur += sampleTex(g_InputTexture, int2(dtid.xy)+int2(x,y), w, h) * wgt; weightSum += wgt; }} blur /= max(weightSum,0.0001f); float4 result = center + (center - blur) * g_Amount; if(g_Threshold > 0.0f){ float4 diff = abs(center - blur) * g_Amount; float mask = step(g_Threshold, max(diff.r,max(diff.g,diff.b))); result = lerp(center, result, mask); } g_OutputTexture[dtid.xy] = float4(saturate(result.rgb), center.a); }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

SharpenEffect::SharpenEffect() {
    setDisplayName(UniString("Sharpen"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SharpenEffectCPUImpl>());
    setGPUImpl(std::make_shared<SharpenEffectGPUImpl>());
}

SharpenEffect::~SharpenEffect() = default;

float SharpenEffect::amount() const { return amount_; }
void SharpenEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 10.0f); syncImpls(); }
float SharpenEffect::sigma() const { return sigma_; }
void SharpenEffect::setSigma(float v) { sigma_ = std::clamp(v, 0.0f, 10.0f); syncImpls(); }
float SharpenEffect::threshold() const { return threshold_; }
void SharpenEffect::setThreshold(float v) { threshold_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void SharpenEffect::syncImpls() {
    if (auto* c = dynamic_cast<SharpenEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->sigma_ = sigma_;
        c->threshold_ = threshold_;
    }
    if (auto* g = dynamic_cast<SharpenEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.sigma_ = sigma_;
        g->cpuImpl_.threshold_ = threshold_;
    }
}

std::vector<AbstractProperty> SharpenEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& s = props.emplace_back(); s.setName("Sigma"); s.setType(PropertyType::Float); s.setValue(sigma_);
    auto& t = props.emplace_back(); t.setName("Threshold"); t.setType(PropertyType::Float); t.setValue(threshold_);
    return props;
}

void SharpenEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Sigma") setSigma(v.toFloat());
    else if (k == "Threshold") setThreshold(v.toFloat());
}

} // namespace Artifact
