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

module Artifact.Effect.Glow.ChromaticGlow;

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

cv::Vec4f sampleRGBA(const cv::Mat &mat, float x, float y) {
    const float fx = std::clamp(x, 0.0f, static_cast<float>(mat.cols - 1));
    const float fy = std::clamp(y, 0.0f, static_cast<float>(mat.rows - 1));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, mat.cols - 1);
    const int y1 = std::min(y0 + 1, mat.rows - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const cv::Vec4f p00 = mat.at<cv::Vec4f>(y0, x0);
    const cv::Vec4f p10 = mat.at<cv::Vec4f>(y0, x1);
    const cv::Vec4f p01 = mat.at<cv::Vec4f>(y1, x0);
    const cv::Vec4f p11 = mat.at<cv::Vec4f>(y1, x1);

    const cv::Vec4f top = p00 * (1.0f - tx) + p10 * tx;
    const cv::Vec4f bottom = p01 * (1.0f - tx) + p11 * tx;
    return top * (1.0f - ty) + bottom * ty;
}

} // namespace

class ChromaticGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.62f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float dispersion_ = 0.35f;
    float angle_ = 35.0f;
    float tintMix_ = 0.2f;

    void applyCPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        const auto &srcImage = src.image();
        const float *srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4,
                       const_cast<float *>(srcData));
        std::vector<cv::Mat> channels;
        cv::split(srcMat, channels);

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        cv::Mat gray = channels[0] * 0.114f +
                       channels[1] * 0.587f +
                       channels[2] * 0.299f;

        cv::Mat brightMask;
        cv::subtract(gray, cv::Scalar::all(threshold_), brightMask);
        cv::threshold(brightMask, brightMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold_ < 0.999f) {
            brightMask *= 1.0f / std::max(0.001f, 1.0f - threshold_);
        }

        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{brightMask, brightMask, brightMask}, mask3);

        cv::Mat bloomSource = color.mul(mask3);
        const int ksize = kernelSizeForRadius(radius_);
        cv::GaussianBlur(bloomSource, bloomSource, cv::Size(ksize, ksize),
                         std::max(0.1f, radius_), std::max(0.1f, radius_),
                         cv::BORDER_REPLICATE);

        cv::Mat bloom4;
        {
            std::vector<cv::Mat> bloomChannels;
            cv::split(bloomSource, bloomChannels);
            bloomChannels.push_back(alpha);
            cv::merge(bloomChannels, bloom4);
        }

        const float angleRad = angle_ * 3.14159265f / 180.0f;
        const float dirX = std::cos(angleRad);
        const float dirY = std::sin(angleRad);
        const float shift = std::max(0.0f, dispersion_) * std::max(1.0f, radius_ * 0.25f);
        const float shiftX = dirX * shift;
        const float shiftY = dirY * shift;

        cv::Mat result = srcMat.clone();
        for (int y = 0; y < srcMat.rows; ++y) {
            for (int x = 0; x < srcMat.cols; ++x) {
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const cv::Vec4f rSample = sampleRGBA(bloom4, fx + shiftX, fy + shiftY);
                const cv::Vec4f gSample = sampleRGBA(bloom4, fx, fy);
                const cv::Vec4f bSample = sampleRGBA(bloom4, fx - shiftX, fy - shiftY);

                cv::Vec4f &dstPx = result.at<cv::Vec4f>(y, x);
                const cv::Vec3f spectral(
                    bSample[0] * (1.0f - tintMix_) + gSample[0] * tintMix_,
                    gSample[1],
                    rSample[2] * (1.0f - tintMix_) + gSample[2] * tintMix_);
                dstPx[0] = std::clamp(dstPx[0] + spectral[0] * intensity_, 0.0f, 1.0f);
                dstPx[1] = std::clamp(dstPx[1] + spectral[1] * intensity_, 0.0f, 1.0f);
                dstPx[2] = std::clamp(dstPx[2] + spectral[2] * intensity_, 0.0f, 1.0f);
            }
        }

        dst.image().setFromRGBA32F(result.ptr<float>(), result.cols, result.rows);
    }
};

class ChromaticGlowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.62f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float dispersion_ = 0.35f;
    float angle_ = 35.0f;
    float tintMix_ = 0.2f;

    void applyCPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) { Diligent::BufferDesc cbDesc; cbDesc.Name="ChromaticGlow/ParamsCB"; cbDesc.Size=sizeof(ParamsCB); cbDesc.Usage=Diligent::USAGE_DYNAMIC; cbDesc.BindFlags=Diligent::BIND_UNIFORM_BUFFER; cbDesc.CPUAccessFlags=Diligent::CPU_ACCESS_WRITE; device_->CreateBuffer(cbDesc,nullptr,&paramsCB_); }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "ChromaticGlowParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) { ArtifactCore::ComputePipelineDesc desc; desc.name="ChromaticGlow/PSO"; desc.shaderSource=kChromaticGlowHlsl; desc.entryPoint="main"; desc.sourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL; desc.variables=vars; desc.variableCount=3; desc.defaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC; if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("ChromaticGlowParams", paramsCB_)) { applyCPU(src, dst); return; } pipelineReady_=true; }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex; if (!createTextureFromImage(src, device_, &inputTex, "ChromaticGlow/InputTexture")) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc=inputTex->GetDesc(); outDesc.Usage=Diligent::USAGE_DEFAULT; outDesc.BindFlags=Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; outDesc.Name="ChromaticGlow/OutputTexture"; Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex; device_->CreateTexture(outDesc,nullptr,&outputTex); if (!outputTex) { applyCPU(src, dst); return; }
        void* mapped=nullptr; context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped); if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{}; params.threshold=threshold_; params.radius=radius_; params.intensity=intensity_; params.dispersion=dispersion_; params.angle=angle_; params.tintMix=tintMix_; std::memcpy(mapped,&params,sizeof(params)); context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) || !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs=ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width,outDesc.Height,1,8,8,1); executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst, "ChromaticGlow/StagingTexture")) { applyCPU(src, dst); return; }
    }

private:
    ChromaticGlowEffectCPUImpl cpuImpl_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;
    struct ParamsCB { float threshold=0.62f; float radius=12.0f; float intensity=1.0f; float dispersion=0.35f; float angle=35.0f; float tintMix=0.2f; float pad=0.0f; };
    static constexpr const char* kChromaticGlowHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0); RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer ChromaticGlowParams : register(b0){ float g_Threshold; float g_Radius; float g_Intensity; float g_Dispersion; float g_Angle; float g_TintMix; float g_Pad; float g_Pad2; };
float4 loadPx(Texture2D<float4> tex, int2 p, int2 size){ p = clamp(p, int2(0,0), size - 1); return tex.Load(int3(p,0)); }
[numthreads(8,8,1)] void main(uint3 dtid:SV_DispatchThreadID){ int2 size; g_OutputTexture.GetDimensions(size.x, size.y); if(dtid.x>=size.x||dtid.y>=size.y) return; float4 px=g_InputTexture[dtid.xy]; float lum=dot(px.rgb,float3(0.114f,0.587f,0.299f)); float mask=saturate((lum-g_Threshold)/max(0.001f,1.0f-g_Threshold)); float2 dir=float2(cos(radians(g_Angle)), sin(radians(g_Angle))); float2 off=dir * max(0.0f, g_Dispersion) * max(1.0f, g_Radius * 0.25f); int2 shift=int2(round(off)); float4 r = loadPx(g_InputTexture, int2(dtid.xy) + shift, size); float4 g = loadPx(g_InputTexture, int2(dtid.xy), size); float4 b = loadPx(g_InputTexture, int2(dtid.xy) - shift, size); float3 spectral = float3(b.r*(1.0f-g_TintMix)+g.r*g_TintMix, g.g, r.b*(1.0f-g_TintMix)+g.b*g_TintMix); px.rgb = saturate(px.rgb + spectral * (g_Intensity * mask)); g_OutputTexture[dtid.xy]=px; }
)";
    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex, const char* name){ const auto& img=src.image(); const float* data=img.rgba32fData(); if(!device||!outTex||!data||img.width()<=0||img.height()<=0) return false; Diligent::TextureDesc desc; desc.Type=Diligent::RESOURCE_DIM_TEX_2D; desc.Width=img.width(); desc.Height=img.height(); desc.Format=Diligent::TEX_FORMAT_RGBA32_FLOAT; desc.ArraySize=1; desc.MipLevels=1; desc.SampleCount=1; desc.Usage=Diligent::USAGE_IMMUTABLE; desc.BindFlags=Diligent::BIND_SHADER_RESOURCE; desc.Name=name; Diligent::TextureSubResData sub{}; sub.pData=data; sub.Stride=static_cast<Diligent::Uint64>(img.width())*sizeof(float)*4ull; Diligent::TextureData init{}; init.pSubResources=&sub; init.NumSubresources=1; device->CreateTexture(desc,&init,outTex); return *outTex!=nullptr; }
    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst, const char* name){ if(!device||!ctx||!src) return false; const auto desc=src->GetDesc(); Diligent::TextureDesc stagingDesc; stagingDesc.Type=Diligent::RESOURCE_DIM_TEX_2D; stagingDesc.Width=desc.Width; stagingDesc.Height=desc.Height; stagingDesc.Format=desc.Format; stagingDesc.ArraySize=1; stagingDesc.MipLevels=1; stagingDesc.SampleCount=1; stagingDesc.Usage=Diligent::USAGE_STAGING; stagingDesc.CPUAccessFlags=Diligent::CPU_ACCESS_READ; stagingDesc.Name=name; Diligent::RefCntAutoPtr<Diligent::ITexture> staging; device->CreateTexture(stagingDesc,nullptr,&staging); if(!staging) return false; Diligent::CopyTextureAttribs copy(src,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION); ctx->CopyTexture(copy); Diligent::MappedTextureSubresource mapped{}; ctx->Flush(); ctx->WaitForIdle(); ctx->MapTextureSubresource(staging,0,0,Diligent::MAP_READ,Diligent::MAP_FLAG_NONE,nullptr,mapped); if(!mapped.pData||mapped.Stride==0) return false; cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride); dst.image().setFromCVMat(temp); ctx->UnmapTextureSubresource(staging,0,0); return true; }
};

ChromaticGlowEffect::ChromaticGlowEffect() {
    setEffectID(UniString("chromatic_glow"));
    setDisplayName(UniString("Chromatic Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ChromaticGlowEffectCPUImpl>());
    setGPUImpl(std::make_shared<ChromaticGlowEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ChromaticGlowEffect::~ChromaticGlowEffect() = default;

void ChromaticGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<ChromaticGlowEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->radius_ = radius_;
        cpu->intensity_ = intensity_;
        cpu->dispersion_ = dispersion_;
        cpu->angle_ = angle_;
        cpu->tintMix_ = tintMix_;
    }
    if (auto gpu = std::dynamic_pointer_cast<ChromaticGlowEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->radius_ = radius_;
        gpu->intensity_ = intensity_;
        gpu->dispersion_ = dispersion_;
        gpu->angle_ = angle_;
        gpu->tintMix_ = tintMix_;
    }
}

std::vector<AbstractProperty> ChromaticGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

    auto &thresholdProp = props.emplace_back();
    thresholdProp.setName("Threshold");
    thresholdProp.setType(PropertyType::Float);
    thresholdProp.setValue(threshold_);

    auto &radiusProp = props.emplace_back();
    radiusProp.setName("Radius");
    radiusProp.setType(PropertyType::Float);
    radiusProp.setValue(radius_);

    auto &intensityProp = props.emplace_back();
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);

    auto &dispersionProp = props.emplace_back();
    dispersionProp.setName("Dispersion");
    dispersionProp.setType(PropertyType::Float);
    dispersionProp.setValue(dispersion_);

    auto &angleProp = props.emplace_back();
    angleProp.setName("Angle");
    angleProp.setType(PropertyType::Float);
    angleProp.setValue(angle_);

    auto &tintProp = props.emplace_back();
    tintProp.setName("Tint Mix");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tintMix_);

    return props;
}

void ChromaticGlowEffect::setPropertyValue(const UniString &name, const QVariant &value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) {
        setThreshold(value.toFloat());
    } else if (key == QStringLiteral("Radius")) {
        setRadius(value.toFloat());
    } else if (key == QStringLiteral("Intensity")) {
        setIntensity(value.toFloat());
    } else if (key == QStringLiteral("Dispersion")) {
        setDispersion(value.toFloat());
    } else if (key == QStringLiteral("Angle")) {
        setAngle(value.toFloat());
    } else if (key == QStringLiteral("Tint Mix")) {
        setTintMix(value.toFloat());
    }
}

} // namespace Artifact
