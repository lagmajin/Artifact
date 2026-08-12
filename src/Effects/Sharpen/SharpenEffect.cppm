module;
#include <algorithm>
#include <cmath>
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
import Memory.SharedPtr;

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
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable bool pipelineReady_ = false;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override { cpuImpl_.applyCPU(src, dst); }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src,dst); return; }
        if (!gpuContext_) { gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_); executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_); }
        if (!executor_) { applyCPU(src, dst); return; }
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="Sharpen/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src,dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "SharpenParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="Sharpen/PSO"; desc.shaderSource=kSharpenHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) || !executor_->setBuffer("SharpenParams", paramsCB_)) { applyCPU(src,dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "Sharpen/InputTexture")) { applyCPU(src,dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="Sharpen/OutputTexture"; if(!outputTex_||outputTex_->GetDesc().Width!=outDesc.Width||outputTex_->GetDesc().Height!=outDesc.Height||outputTex_->GetDesc().Format!=outDesc.Format||outputTex_->GetDesc().BindFlags!=outDesc.BindFlags){outputTex_.Release();device_->CreateTexture(outDesc,nullptr,&outputTex_);} if (!outputTex_) { applyCPU(src,dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src,dst); return; }
        ParamsCB params{}; params.amount=cpuImpl_.amount_; params.sigma=cpuImpl_.sigma_; params.threshold=cpuImpl_.threshold_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src,dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex_, dst, "Sharpen/StagingTexture", src.image().colorDescriptor())) { applyCPU(src,dst); return; }
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
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name, const auto& colorDescriptor){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp,colorDescriptor); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

SharpenEffect::SharpenEffect() {
    setDisplayName(UniString("Sharpen"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<SharpenEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<SharpenEffectGPUImpl>());
}

SharpenEffect::~SharpenEffect() = default;

float SharpenEffect::amount() const { return amount_; }
void SharpenEffect::setAmount(float v) { amount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 10.0f) : 1.0f; syncImpls(); }
float SharpenEffect::sigma() const { return sigma_; }
void SharpenEffect::setSigma(float v) { sigma_ = std::isfinite(v) ? std::clamp(v, 0.0f, 10.0f) : 1.0f; syncImpls(); }
float SharpenEffect::threshold() const { return threshold_; }
void SharpenEffect::setThreshold(float v) { threshold_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f; syncImpls(); }

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

class MagicSharpEffect::Impl {
public:
    float amount = 1.0f;
    float fine = 0.75f;
    float small = 0.62f;
    float medium = 0.42f;
    float coarse = 0.24f;
    float threshold = 0.018f;
    float edgeProtection = 0.62f;
    float shadowProtection = 0.45f;
    float mix = 1.0f;
};

MagicSharpEffect::MagicSharpEffect() : impl_(new Impl()) {
    setEffectID(UniString("builtin.magic_sharp"));
    setDisplayName(UniString("Magic Sharp"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

MagicSharpEffect::~MagicSharpEffect() {
    delete impl_; impl_ = nullptr;
}

void MagicSharpEffect::apply(const ImageF32x4RGBAWithCache& src,
                             ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width(), height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) { dst = src; return; }
    cv::Mat rgba(height, width, CV_32FC4, const_cast<float*>(source));
    std::vector<cv::Mat> channels; cv::split(rgba, channels);
    cv::Mat luma = channels[0] * 0.2126f + channels[1] * 0.7152f + channels[2] * 0.0722f;
    cv::Mat blurFine, blurSmall, blurMedium, blurCoarse;
    cv::GaussianBlur(luma, blurFine, cv::Size(), 0.65, 0.65, cv::BORDER_REFLECT_101);
    cv::GaussianBlur(luma, blurSmall, cv::Size(), 1.35, 1.35, cv::BORDER_REFLECT_101);
    cv::GaussianBlur(luma, blurMedium, cv::Size(), 2.8, 2.8, cv::BORDER_REFLECT_101);
    cv::GaussianBlur(luma, blurCoarse, cv::Size(), 5.8, 5.8, cv::BORDER_REFLECT_101);
    auto result = image.DeepCopy(); float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const float* lumaRow = luma.ptr<float>(y);
        const float* fineRow = blurFine.ptr<float>(y);
        const float* smallRow = blurSmall.ptr<float>(y);
        const float* mediumRow = blurMedium.ptr<float>(y);
        const float* coarseRow = blurCoarse.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const float base = lumaRow[x];
            const float fineDetail = base - fineRow[x];
            const float smallDetail = fineRow[x] - smallRow[x];
            const float mediumDetail = smallRow[x] - mediumRow[x];
            const float coarseDetail = mediumRow[x] - coarseRow[x];
            float detail = fineDetail * impl_->fine + smallDetail * impl_->small +
                           mediumDetail * impl_->medium + coarseDetail * impl_->coarse;
            const float edgeMagnitude = std::abs(base - coarseRow[x]);
            const float highContrastProtection = 1.0f - impl_->edgeProtection *
                std::clamp(edgeMagnitude * 2.5f, 0.0f, 1.0f);
            const float shadowWeight = std::lerp(
                std::clamp(base / 0.18f, 0.0f, 1.0f), 1.0f,
                1.0f - impl_->shadowProtection);
            if (std::abs(detail) < impl_->threshold) detail = 0.0f;
            const float sharpenedLuma = base + detail * impl_->amount *
                highContrastProtection * shadowWeight;
            const float delta = (sharpenedLuma - base) * impl_->mix;
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
            output[offset] = std::max(0.0f, source[offset] + delta);
            output[offset + 1] = std::max(0.0f, source[offset + 1] + delta);
            output[offset + 2] = std::max(0.0f, source[offset + 2] + delta);
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor()); dst = ImageF32x4RGBAWithCache(result);
}

std::vector<AbstractProperty> MagicSharpEffect::getProperties() const {
    std::vector<AbstractProperty> properties;
    auto add = [&](const char* name, const char* label, float value, float lo, float hi) {
        auto& p = properties.emplace_back(); p.setName(name); p.setDisplayLabel(label);
        p.setType(PropertyType::Float); p.setValue(value); p.setDefaultValue(value);
        p.setHardRange(lo, hi); p.setAnimatable(true);
    };
    add("amount", "Master Amount", impl_->amount, 0.0f, 5.0f);
    add("fine", "Fine Detail", impl_->fine, 0.0f, 2.0f);
    add("small", "Small Detail", impl_->small, 0.0f, 2.0f);
    add("medium", "Medium Detail", impl_->medium, 0.0f, 2.0f);
    add("coarse", "Coarse Detail", impl_->coarse, 0.0f, 2.0f);
    add("threshold", "Noise Threshold", impl_->threshold, 0.0f, 0.25f);
    add("edgeProtection", "Edge Protection", impl_->edgeProtection, 0.0f, 1.0f);
    add("shadowProtection", "Shadow Protection", impl_->shadowProtection, 0.0f, 1.0f);
    add("mix", "Mix", impl_->mix, 0.0f, 1.0f);
    return properties;
}

void MagicSharpEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString(); const float raw = value.toFloat();
    const float n = std::isfinite(raw) ? raw : 0.0f;
    if (key == "amount") impl_->amount = std::clamp(n, 0.0f, 5.0f);
    else if (key == "fine") impl_->fine = std::clamp(n, 0.0f, 2.0f);
    else if (key == "small") impl_->small = std::clamp(n, 0.0f, 2.0f);
    else if (key == "medium") impl_->medium = std::clamp(n, 0.0f, 2.0f);
    else if (key == "coarse") impl_->coarse = std::clamp(n, 0.0f, 2.0f);
    else if (key == "threshold") impl_->threshold = std::clamp(n, 0.0f, 0.25f);
    else if (key == "edgeProtection") impl_->edgeProtection = std::clamp(n, 0.0f, 1.0f);
    else if (key == "shadowProtection") impl_->shadowProtection = std::clamp(n, 0.0f, 1.0f);
    else if (key == "mix") impl_->mix = std::clamp(n, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint MagicSharpEffect::roiHint() const {
    return EffectROIHint{.kind = EffectROIHintKind::Blur,
                         .expansionPixels = 18.0f};
}

} // namespace Artifact
