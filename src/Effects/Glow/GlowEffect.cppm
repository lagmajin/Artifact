module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <QVariant>
#include <QString>
#include <opencv2/opencv.hpp>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Glow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Artifact.Render.DiligentDeviceManager;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Memory.SharedPtr;
import ArtifactCore.ImageProcessing.VolumetricShine;

namespace Artifact {

class GlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float glowGain_ = 1.0f;
    int layerCount_ = 4;
    float baseSigma_ = 5.0f;
    float sigmaGrowth_ = 1.8f;
    float baseAlpha_ = 0.3f;
    float alphaFalloff_ = 0.6f;
    bool contributionPreview_ = false;

    void setGlowGain(float gain) { glowGain_ = std::isfinite(gain) ? std::clamp(gain, 0.0f, 10.0f) : 1.0f; }
    float glowGain() const { return glowGain_; }
    void setLayerCount(int count) { layerCount_ = std::clamp(count, 1, 16); }
    int layerCount() const { return layerCount_; }
    void setBaseSigma(float sigma) { baseSigma_ = std::isfinite(sigma) ? std::clamp(sigma, 0.1f, 128.0f) : 5.0f; }
    float baseSigma() const { return baseSigma_; }
    void setSigmaGrowth(float growth) { sigmaGrowth_ = std::isfinite(growth) ? std::clamp(growth, 0.1f, 4.0f) : 1.8f; }
    float sigmaGrowth() const { return sigmaGrowth_; }
    void setBaseAlpha(float alpha) { baseAlpha_ = std::isfinite(alpha) ? std::clamp(alpha, 0.0f, 1.0f) : 0.3f; }
    float baseAlpha() const { return baseAlpha_; }
    void setAlphaFalloff(float falloff) { alphaFalloff_ = std::isfinite(falloff) ? std::clamp(falloff, 0.0f, 1.0f) : 0.6f; }
    float alphaFalloff() const { return alphaFalloff_; }
    void setContributionPreview(bool enabled) { contributionPreview_ = enabled; }
    bool contributionPreview() const { return contributionPreview_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class GlowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float glowGain_ = 1.0f;
    int layerCount_ = 4;
    float baseSigma_ = 5.0f;
    float sigmaGrowth_ = 1.8f;
    float baseAlpha_ = 0.3f;
    float alphaFalloff_ = 0.6f;
    bool contributionPreview_ = false;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    bool pipelineReady_ = false;
    bool usingSharedDevice_ = false;

    ~GlowEffectGPUImpl() override;

    void setGlowGain(float gain) { glowGain_ = std::isfinite(gain) ? std::clamp(gain, 0.0f, 10.0f) : 1.0f; }
    float glowGain() const { return glowGain_; }
    void setLayerCount(int count) { layerCount_ = std::clamp(count, 1, 16); }
    int layerCount() const { return layerCount_; }
    void setBaseSigma(float sigma) { baseSigma_ = std::isfinite(sigma) ? std::clamp(sigma, 0.1f, 128.0f) : 5.0f; }
    float baseSigma() const { return baseSigma_; }
    void setSigmaGrowth(float growth) { sigmaGrowth_ = std::isfinite(growth) ? std::clamp(growth, 0.1f, 4.0f) : 1.8f; }
    float sigmaGrowth() const { return sigmaGrowth_; }
    void setBaseAlpha(float alpha) { baseAlpha_ = std::isfinite(alpha) ? std::clamp(alpha, 0.0f, 1.0f) : 0.3f; }
    float baseAlpha() const { return baseAlpha_; }
    void setAlphaFalloff(float falloff) { alphaFalloff_ = std::isfinite(falloff) ? std::clamp(falloff, 0.0f, 1.0f) : 0.6f; }
    float alphaFalloff() const { return alphaFalloff_; }
    void setContributionPreview(bool enabled) { contributionPreview_ = enabled; }
    bool contributionPreview() const { return contributionPreview_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

namespace {

bool imageBuffersDiffer(const ImageF32x4RGBAWithCache& a,
                        const ImageF32x4RGBAWithCache& b)
{
    const auto& ai = a.image();
    const auto& bi = b.image();
    if (ai.width() != bi.width() || ai.height() != bi.height()) {
        return true;
    }
    const float* ad = ai.rgba32fData();
    const float* bd = bi.rgba32fData();
    if (!ad || !bd) {
        return false;
    }
    const int width = ai.width();
    const int height = ai.height();
    const int stepX = std::max(1, width / 8);
    const int stepY = std::max(1, height / 8);
    for (int y = 0; y < height; y += stepY) {
        for (int x = 0; x < width; x += stepX) {
            const size_t idx = (static_cast<size_t>(y) * width + x) * 4ull;
            for (int c = 0; c < 4; ++c) {
                if (std::abs(ad[idx + c] - bd[idx + c]) > 0.0005f) {
                    return true;
                }
            }
        }
    }
    return false;
}

int kernelSizeForRadius(float radius) {
    const int estimated = static_cast<int>(std::ceil(std::max(0.5f, radius) * 2.5f));
    return std::max(3, (estimated * 2) + 1);
}

void writeGlowResultToDestination(const cv::Mat& rgba32f, ImageF32x4RGBAWithCache& dst) {
    if (rgba32f.empty()) {
        return;
    }
    dst.image().setFromRGBA32F(rgba32f.ptr<float>(), rgba32f.cols, rgba32f.rows);
}

static constexpr const char* kGlowHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GlowParams : register(b0) {
    float g_GlowGain;
    float g_LayerCount;
    float g_BaseSigma;
    float g_SigmaGrowth;
    float g_BaseAlpha;
    float g_AlphaFalloff;
    float g_ContributionOnly;
    float g_Pad;
};

float luminance(float3 c) {
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

float4 sampleTex(Texture2D<float4> tex, int2 p, uint w, uint h) {
    p.x = clamp(p.x, 0, (int)w - 1);
    p.y = clamp(p.y, 0, (int)h - 1);
    return tex[uint2(p)];
}

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) {
        return;
    }

    float4 center = g_InputTexture[dtid.xy];
    float3 accum = 0.0f;
    float alphaSum = 0.0f;
    int layers = max(1, (int)g_LayerCount);

    [loop]
    for (int i = 0; i < layers; ++i) {
        float sigma = max(0.1f, g_BaseSigma + g_SigmaGrowth * (float)i);
        int radius = max(1, (int)ceil(sigma * 2.5f));
        float weightSum = 0.0f;
        float3 layerAccum = 0.0f;

        [loop]
        for (int y = -radius; y <= radius; ++y) {
            [loop]
            for (int x = -radius; x <= radius; ++x) {
                float d = (float)(x * x + y * y);
                float wgt = exp(-0.5f * d / max(0.0001f, sigma * sigma));
                float4 samplePx = sampleTex(g_InputTexture, int2(dtid.xy) + int2(x, y), w, h);
                float lum = luminance(samplePx.rgb);
                float bright = lum > 0.6f ? saturate(lum * g_GlowGain) : 0.0f;
                layerAccum += samplePx.rgb * bright * wgt;
                weightSum += wgt;
            }
        }

        layerAccum /= max(weightSum, 0.0001f);
        float alphaWeight = g_BaseAlpha * pow(max(0.0f, g_AlphaFalloff), (float)i);
        accum += layerAccum * alphaWeight;
        alphaSum += alphaWeight;
    }

    float3 contribution = accum / max(alphaSum, 0.0001f);
    float3 result = g_ContributionOnly > 0.5f ? contribution : center.rgb + contribution;
    g_OutputTexture[dtid.xy] = float4(saturate(result), center.a);
}
)";

struct ParamsCB {
    float glowGain = 1.0f;
    float layerCount = 4.0f;
    float baseSigma = 5.0f;
    float sigmaGrowth = 1.8f;
    float baseAlpha = 0.3f;
    float alphaFalloff = 0.6f;
    float contributionOnly = 0.0f;
    float pad = 0.0f;
};

static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src,
                                   Diligent::IRenderDevice* device,
                                   Diligent::ITexture** outTex,
                                   const char* name) {
    if (!device || !outTex) {
        return false;
    }

    const auto& img = src.image();
    const float* data = img.rgba32fData();
    if (!data || img.width() <= 0 || img.height() <= 0) {
        return false;
    }

    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(img.width());
    desc.Height = static_cast<Diligent::Uint32>(img.height());
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Name = name;

    Diligent::TextureSubResData sub{};
    sub.pData = data;
    sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;

    Diligent::TextureData init{};
    init.pSubResources = &sub;
    init.NumSubresources = 1;
    device->CreateTexture(desc, &init, outTex);
    return *outTex != nullptr;
}

static bool readbackTexture(Diligent::IRenderDevice* device,
                            Diligent::IDeviceContext* ctx,
                            Diligent::ITexture* src,
                            ImageF32x4RGBAWithCache& dst,
                            const ArtifactCore::SurfaceColorDescriptor& colorDescriptor,
                            const char* name) {
    if (!device || !ctx || !src) {
        return false;
    }

    const auto desc = src->GetDesc();
    Diligent::TextureDesc stagingDesc;
    stagingDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = desc.Width;
    stagingDesc.Height = desc.Height;
    stagingDesc.Format = desc.Format;
    stagingDesc.ArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.SampleCount = 1;
    stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    stagingDesc.Name = name;

    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    device->CreateTexture(stagingDesc, nullptr, &staging);
    if (!staging) {
        return false;
    }

    Diligent::CopyTextureAttribs copy(
        src,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        staging,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->CopyTexture(copy);

    Diligent::MappedTextureSubresource mapped{};
    ctx->Flush();
    ctx->WaitForIdle();
    ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || mapped.Stride == 0) {
        return false;
    }

    cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
    dst.image().setFromCVMat(temp, colorDescriptor);
    ctx->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

} // namespace

void GlowEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }

    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
    std::vector<cv::Mat> channels;
    cv::split(srcMat, channels);

    cv::Mat color;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    cv::Mat alpha = channels[3];

    cv::Mat glowAccum = cv::Mat::zeros(color.size(), color.type());
    cv::Mat mask = channels[0] * 0.299f +
                   channels[1] * 0.587f +
                   channels[2] * 0.114f;
    cv::threshold(mask, mask, 0.6, 0.0, cv::THRESH_TOZERO);
    if (glowGain_ > 0.0f) {
        mask *= glowGain_;
    }
    cv::min(mask, cv::Scalar::all(1.0), mask);

    cv::Mat mask3;
    cv::merge(std::vector<cv::Mat>{mask, mask, mask}, mask3);

    const int layers = std::max(1, layerCount_);
    for (int i = 0; i < layers; ++i) {
        const float sigma = std::max(0.1f, baseSigma_ + (static_cast<float>(i) * sigmaGrowth_));
        const int ksize = kernelSizeForRadius(sigma);
        cv::Mat layer = color.mul(mask3);
        cv::GaussianBlur(layer, layer, cv::Size(ksize, ksize), sigma, sigma, cv::BORDER_REPLICATE);
        const float alphaWeight = baseAlpha_ * std::pow(std::max(0.0f, alphaFalloff_), static_cast<float>(i));
        glowAccum += layer * alphaWeight;
    }

    const float alphaNorm = std::max(
        0.0001f,
        baseAlpha_ * (1.0f - std::pow(alphaFalloff_, static_cast<float>(layers))) /
            std::max(0.0001f, 1.0f - alphaFalloff_));
    cv::Mat contribution = glowAccum / alphaNorm;
    cv::Mat result = contributionPreview_ ? contribution : color + contribution;
    cv::min(result, cv::Scalar::all(1.0), result);
    cv::max(result, cv::Scalar::all(0.0), result);
    std::vector<cv::Mat> outChannels;
    cv::split(result, outChannels);
    outChannels.push_back(alpha);
    cv::Mat out;
    cv::merge(outChannels, out);
    writeGlowResultToDestination(out, dst);
    dst.image().setColorDescriptor(srcImage.colorDescriptor());
}

void GlowEffectGPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    GlowEffectCPUImpl cpu;
    cpu.setGlowGain(glowGain_);
    cpu.setLayerCount(layerCount_);
    cpu.setBaseSigma(baseSigma_);
    cpu.setSigmaGrowth(sigmaGrowth_);
    cpu.setBaseAlpha(baseAlpha_);
    cpu.setAlphaFalloff(alphaFalloff_);
    cpu.setContributionPreview(contributionPreview_);
    cpu.applyCPU(src, dst);
}

GlowEffectGPUImpl::~GlowEffectGPUImpl() {
    if (context_) {
        context_->Flush();
        context_->WaitForIdle();
    }
    executor_.reset();
    gpuContext_.reset();
    paramsCB_.Release();
    context_.Release();
    device_.Release();
    if (usingSharedDevice_) {
        releaseSharedRenderDevice();
    }
}

void GlowEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    if (!device_ || !context_) {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        usingSharedDevice_ = true;
    }
    if (!executor_) {
        gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
    }
    if (!paramsCB_) {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "Glow/ParamsCB";
        cbDesc.Size = sizeof(ParamsCB);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
    }
    if (!paramsCB_) {
        applyCPU(src, dst);
        return;
    }

    static Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "GlowParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };

    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "Glow/PSO";
    desc.shaderSource = kGlowHlsl;
    desc.entryPoint = "main";
    desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 3;
    desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!pipelineReady_) {
        if (!executor_->build(desc) ||
            !executor_->createShaderResourceBinding(true) ||
            !executor_->setBuffer("GlowParams", paramsCB_)) {
            applyCPU(src, dst);
            return;
        }
        pipelineReady_ = true;
    }

    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createTextureFromImage(src, device_, &inputTex, "Glow/InputTexture")) { applyCPU(src, dst); return; }
    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "Glow/OutputTexture";
    if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
        outputTex_->GetDesc().Height != outDesc.Height ||
        outputTex_->GetDesc().Format != outDesc.Format ||
        outputTex_->GetDesc().BindFlags != outDesc.BindFlags) {
        outputTex_.Release();
        device_->CreateTexture(outDesc, nullptr, &outputTex_);
    }
    if (!outputTex_) { applyCPU(src, dst); return; }
    void* mapped = nullptr;
    context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) { applyCPU(src, dst); return; }
    ParamsCB params{};
    params.glowGain = glowGain_;
    params.layerCount = static_cast<float>(layerCount_);
    params.baseSigma = baseSigma_;
    params.sigmaGrowth = sigmaGrowth_;
    params.baseAlpha = baseAlpha_;
    params.alphaFalloff = alphaFalloff_;
    params.contributionOnly = contributionPreview_ ? 1.0f : 0.0f;
    std::memcpy(mapped, &params, sizeof(params));
    context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
    if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyCPU(src, dst);
        return;
    }
    auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
    executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!readbackTexture(device_, context_, outputTex_, dst, src.image().colorDescriptor(), "Glow/StagingTexture")) {
        applyCPU(src, dst);
        return;
    }
    if (!imageBuffersDiffer(src, dst)) {
        applyCPU(src, dst);
    }
    dst.image().setColorDescriptor(src.image().colorDescriptor());
}

class GlowEffect::Impl {
public:
    ArtifactCore::SharedPtr<GlowEffectCPUImpl> cpuImpl_;
    ArtifactCore::SharedPtr<GlowEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = ArtifactCore::makeShared<GlowEffectCPUImpl>();
        gpuImpl_ = ArtifactCore::makeShared<GlowEffectGPUImpl>();
    }
};

GlowEffect::GlowEffect() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Glow (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

GlowEffect::~GlowEffect() {
    delete impl_;
}

void GlowEffect::setGlowGain(float gain) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setGlowGain(gain);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setGlowGain(gain);
    }
}

float GlowEffect::glowGain() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->glowGain();
    }
    return 0.0f;
}

void GlowEffect::setLayerCount(int count) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setLayerCount(count);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setLayerCount(count);
    }
}

int GlowEffect::layerCount() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->layerCount();
    }
    return 0;
}

void GlowEffect::setBaseSigma(float sigma) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setBaseSigma(sigma);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setBaseSigma(sigma);
    }
}

float GlowEffect::baseSigma() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->baseSigma();
    }
    return 0.0f;
}

void GlowEffect::setSigmaGrowth(float growth) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setSigmaGrowth(growth);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setSigmaGrowth(growth);
    }
}

float GlowEffect::sigmaGrowth() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->sigmaGrowth();
    }
    return 0.0f;
}

void GlowEffect::setBaseAlpha(float alpha) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setBaseAlpha(alpha);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setBaseAlpha(alpha);
    }
}

float GlowEffect::baseAlpha() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->baseAlpha();
    }
    return 0.0f;
}

void GlowEffect::setAlphaFalloff(float falloff) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setAlphaFalloff(falloff);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setAlphaFalloff(falloff);
    }
}

float GlowEffect::alphaFalloff() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->alphaFalloff();
    }
    return 0.0f;
}

void GlowEffect::setContributionPreview(bool enabled) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setContributionPreview(enabled);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setContributionPreview(enabled);
}

bool GlowEffect::contributionPreview() const {
    return impl_->cpuImpl_ && impl_->cpuImpl_->contributionPreview();
}

std::vector<ArtifactCore::AbstractProperty> GlowEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(7);

    auto& gainProp = props.emplace_back();
    gainProp.setName("glowGain");
    gainProp.setType(ArtifactCore::PropertyType::Float);
    gainProp.setDefaultValue(QVariant(1.0));
    gainProp.setValue(QVariant(static_cast<double>(glowGain())));
    gainProp.setDisplayLabel(QStringLiteral("Intensity"));
    gainProp.setHardRange(0.0, 10.0);
    gainProp.setSoftRange(0.0, 3.0);
    gainProp.setStep(0.01);
    gainProp.setTooltip(QStringLiteral("Gain applied to highlights before bloom extraction."));

    auto& layerCountProp = props.emplace_back();
    layerCountProp.setName("layerCount");
    layerCountProp.setType(ArtifactCore::PropertyType::Integer);
    layerCountProp.setDefaultValue(QVariant(4));
    layerCountProp.setValue(QVariant(layerCount()));
    layerCountProp.setDisplayLabel(QStringLiteral("Quality Layers"));
    layerCountProp.setHardRange(1, 16);
    layerCountProp.setSoftRange(1, 8);
    layerCountProp.setStep(1);
    layerCountProp.setUnit(QStringLiteral("passes"));
    layerCountProp.setTooltip(QStringLiteral("More layers produce a richer bloom at a higher processing cost."));

    auto& sigmaProp = props.emplace_back();
    sigmaProp.setName("baseSigma");
    sigmaProp.setType(ArtifactCore::PropertyType::Float);
    sigmaProp.setDefaultValue(QVariant(5.0));
    sigmaProp.setValue(QVariant(static_cast<double>(baseSigma())));
    sigmaProp.setDisplayLabel(QStringLiteral("Radius"));
    sigmaProp.setHardRange(0.1, 128.0);
    sigmaProp.setSoftRange(0.1, 32.0);
    sigmaProp.setStep(0.1);
    sigmaProp.setUnit(QStringLiteral("px"));

    auto& growthProp = props.emplace_back();
    growthProp.setName("sigmaGrowth");
    growthProp.setType(ArtifactCore::PropertyType::Float);
    growthProp.setDefaultValue(QVariant(1.8));
    growthProp.setValue(QVariant(static_cast<double>(sigmaGrowth())));
    growthProp.setDisplayLabel(QStringLiteral("Radius Growth"));
    growthProp.setHardRange(0.1, 4.0);
    growthProp.setSoftRange(0.5, 3.0);
    growthProp.setStep(0.01);

    auto& alphaProp = props.emplace_back();
    alphaProp.setName("baseAlpha");
    alphaProp.setType(ArtifactCore::PropertyType::Float);
    alphaProp.setDefaultValue(QVariant(0.3));
    alphaProp.setValue(QVariant(static_cast<double>(baseAlpha())));
    alphaProp.setDisplayLabel(QStringLiteral("Core Opacity"));
    alphaProp.setHardRange(0.0, 1.0);
    alphaProp.setSoftRange(0.0, 1.0);
    alphaProp.setStep(0.01);

    auto& falloffProp = props.emplace_back();
    falloffProp.setName("alphaFalloff");
    falloffProp.setType(ArtifactCore::PropertyType::Float);
    falloffProp.setDefaultValue(QVariant(0.6));
    falloffProp.setValue(QVariant(static_cast<double>(alphaFalloff())));
    falloffProp.setDisplayLabel(QStringLiteral("Layer Falloff"));
    falloffProp.setHardRange(0.0, 1.0);
    falloffProp.setSoftRange(0.0, 1.0);
    falloffProp.setStep(0.01);

    auto& contributionProp = props.emplace_back();
    contributionProp.setName(QStringLiteral("preview.contributionOnly"));
    contributionProp.setDisplayLabel(QStringLiteral("Glow Contribution Only"));
    contributionProp.setType(ArtifactCore::PropertyType::Boolean);
    contributionProp.setDefaultValue(false);
    contributionProp.setValue(contributionPreview());
    contributionProp.setAnimatable(false);
    contributionProp.setTooltip(QStringLiteral(
        "Show only the generated glow contribution for threshold and radius tuning."));

    return props;
}

void GlowEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    const QString n = name.toQString();
    if (n == "glowGain") {
        setGlowGain(static_cast<float>(value.toDouble()));
    } else if (n == "layerCount") {
        setLayerCount(value.toInt());
    } else if (n == "baseSigma") {
        setBaseSigma(static_cast<float>(value.toDouble()));
    } else if (n == "sigmaGrowth") {
        setSigmaGrowth(static_cast<float>(value.toDouble()));
    } else if (n == "baseAlpha") {
        setBaseAlpha(static_cast<float>(value.toDouble()));
    } else if (n == "alphaFalloff") {
        setAlphaFalloff(static_cast<float>(value.toDouble()));
    } else if (n == QStringLiteral("preview.contributionOnly")) {
        setContributionPreview(value.toBool());
    }
}

EffectROIHint GlowEffect::roiHint() const {
    const int layers = std::max(1, layerCount());
    const float sigmaMax = std::max(0.1f, baseSigma() + sigmaGrowth() * static_cast<float>(layers - 1));
    return EffectROIHint{
        .kind = EffectROIHintKind::Blur,
        .expansionPixels = sigmaMax * 3.0f,
        .requiresFullFrame = false
    };
}

class OpticalGlowEffect::Impl {
public:
    float threshold = 0.72f;
    float softKnee = 0.35f;
    float radius = 10.0f;
    float intensity = 1.15f;
    float afterglowRadius = 42.0f;
    float afterglowIntensity = 0.38f;
    float streakStrength = 0.18f;
    float atmosphere = 0.16f;
    float warmth = 0.08f;
};

OpticalGlowEffect::OpticalGlowEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.optical_glow"));
    setDisplayName(ArtifactCore::UniString("Optical Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

OpticalGlowEffect::~OpticalGlowEffect() {
    delete impl_;
    impl_ = nullptr;
}

void OpticalGlowEffect::apply(const ImageF32x4RGBAWithCache& src,
                              ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* sourcePixels = image.rgba32fData();
    if (!sourcePixels || width <= 0 || height <= 0) {
        dst = src;
        return;
    }

    cv::Mat source(height, width, CV_32FC4,
                   const_cast<float*>(sourcePixels));
    cv::Mat highlights = cv::Mat::zeros(height, width, CV_32FC4);
    const float threshold = std::clamp(impl_->threshold, 0.0f, 4.0f);
    const float knee = std::max(0.0001f, impl_->softKnee *
                                           std::max(threshold, 0.01f));

    for (int y = 0; y < height; ++y) {
        const cv::Vec4f* sourceRow = source.ptr<cv::Vec4f>(y);
        cv::Vec4f* highlightRow = highlights.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            const cv::Vec4f pixel = sourceRow[x];
            const float luminance = pixel[0] * 0.2126f +
                                    pixel[1] * 0.7152f +
                                    pixel[2] * 0.0722f;
            const float soft = std::clamp((luminance - threshold + knee) /
                                              (2.0f * knee),
                                          0.0f, 1.0f);
            const float hard = std::max(luminance - threshold, 0.0f);
            const float contribution = hard + soft * soft * knee * 0.5f;
            const float scale = contribution / std::max(luminance, 0.0001f);
            highlightRow[x] = cv::Vec4f(pixel[0] * scale,
                                        pixel[1] * scale,
                                        pixel[2] * scale, 0.0f);
        }
    }

    cv::Mat primary;
    cv::Mat afterglow;
    cv::Mat streak;
    cv::Mat haze;
    cv::GaussianBlur(highlights, primary, cv::Size(),
                     std::max(0.1f, impl_->radius));
    cv::GaussianBlur(highlights, afterglow, cv::Size(),
                     std::max(0.1f, impl_->afterglowRadius));
    cv::GaussianBlur(highlights, streak, cv::Size(),
                     std::max(0.1f, impl_->radius * 4.0f),
                     std::max(0.1f, impl_->radius * 0.22f));
    cv::GaussianBlur(highlights, haze, cv::Size(),
                     std::max(0.1f, impl_->afterglowRadius * 1.75f));

    auto result = image.DeepCopy();
    float* destinationPixels = result.rgba32fData();
    const float warmR = 1.0f + impl_->warmth * 0.30f;
    const float warmG = 1.0f;
    const float warmB = 1.0f - impl_->warmth * 0.42f;
    for (int y = 0; y < height; ++y) {
        const cv::Vec4f* primaryRow = primary.ptr<cv::Vec4f>(y);
        const cv::Vec4f* afterRow = afterglow.ptr<cv::Vec4f>(y);
        const cv::Vec4f* streakRow = streak.ptr<cv::Vec4f>(y);
        const cv::Vec4f* hazeRow = haze.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            const float channelTint[3] = {warmR, warmG, warmB};
            for (int channel = 0; channel < 3; ++channel) {
                const float glow = primaryRow[x][channel] * impl_->intensity +
                    afterRow[x][channel] * impl_->afterglowIntensity +
                    streakRow[x][channel] * impl_->streakStrength +
                    hazeRow[x][channel] * impl_->atmosphere;
                destinationPixels[offset + channel] =
                    sourcePixels[offset + channel] + glow * channelTint[channel];
            }
            destinationPixels[offset + 3] = sourcePixels[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> OpticalGlowEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    properties.reserve(9);
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setDefaultValue(QVariant(static_cast<double>(value)));
        property.setValue(QVariant(static_cast<double>(value)));
        property.setHardRange(QVariant(static_cast<double>(minimum)),
                              QVariant(static_cast<double>(maximum)));
        property.setAnimatable(true);
    };
    addFloat("threshold", "Threshold", impl_->threshold, 0.0f, 4.0f);
    addFloat("softKnee", "Soft Knee", impl_->softKnee, 0.0f, 1.0f);
    addFloat("radius", "Primary Radius", impl_->radius, 0.1f, 128.0f);
    addFloat("intensity", "Primary Intensity", impl_->intensity, 0.0f, 8.0f);
    addFloat("afterglowRadius", "Afterglow Radius", impl_->afterglowRadius, 0.1f, 256.0f);
    addFloat("afterglowIntensity", "Afterglow Intensity", impl_->afterglowIntensity, 0.0f, 4.0f);
    addFloat("streakStrength", "Anamorphic Streak", impl_->streakStrength, 0.0f, 4.0f);
    addFloat("atmosphere", "Atmosphere", impl_->atmosphere, 0.0f, 2.0f);
    addFloat("warmth", "Optical Warmth", impl_->warmth, -1.0f, 1.0f);
    return properties;
}

void OpticalGlowEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                         const QVariant& value) {
    const QString key = name.toQString();
    const float raw = static_cast<float>(value.toDouble());
    const float number = std::isfinite(raw) ? raw : 0.0f;
    if (key == QStringLiteral("threshold")) impl_->threshold = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("softKnee")) impl_->softKnee = std::clamp(number, 0.0f, 1.0f);
    else if (key == QStringLiteral("radius")) impl_->radius = std::clamp(number, 0.1f, 128.0f);
    else if (key == QStringLiteral("intensity")) impl_->intensity = std::clamp(number, 0.0f, 8.0f);
    else if (key == QStringLiteral("afterglowRadius")) impl_->afterglowRadius = std::clamp(number, 0.1f, 256.0f);
    else if (key == QStringLiteral("afterglowIntensity")) impl_->afterglowIntensity = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("streakStrength")) impl_->streakStrength = std::clamp(number, 0.0f, 4.0f);
    else if (key == QStringLiteral("atmosphere")) impl_->atmosphere = std::clamp(number, 0.0f, 2.0f);
    else if (key == QStringLiteral("warmth")) impl_->warmth = std::clamp(number, -1.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint OpticalGlowEffect::roiHint() const {
    const float widestSigma = std::max(impl_->radius * 4.0f,
                                       impl_->afterglowRadius * 1.75f);
    return EffectROIHint{
        .kind = EffectROIHintKind::Blur,
        .expansionPixels = widestSigma * 3.0f,
        .requiresFullFrame = false
    };
}

class VolumetricShineEffect::Impl {
public:
    float sourceX = 0.5f;
    float sourceY = 0.38f;
    float rayLength = 0.55f;
    float intensity = 1.0f;
    float decay = 0.94f;
    int samples = 40;
    float threshold = 0.62f;
    float warmth = 0.14f;
};

VolumetricShineEffect::VolumetricShineEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.volumetric_shine"));
    setDisplayName(ArtifactCore::UniString("Volumetric Shine"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

VolumetricShineEffect::~VolumetricShineEffect() {
    delete impl_;
    impl_ = nullptr;
}

void VolumetricShineEffect::apply(const ImageF32x4RGBAWithCache& src,
                                  ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width();
    const int height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) {
        dst = src;
        return;
    }
    using ShinePixel = decltype(ArtifactCore::VolumetricShine::Settings{}.tint);
    std::vector<ShinePixel> rays(image.totalPixels());
    for (std::size_t pixel = 0; pixel < rays.size(); ++pixel) {
        const std::size_t offset = pixel * 4u;
        const float luminance = source[offset] * 0.2126f +
            source[offset + 1] * 0.7152f + source[offset + 2] * 0.0722f;
        const float selection = std::max(0.0f, luminance - impl_->threshold) /
                                std::max(luminance, 0.0001f);
        rays[pixel] = ShinePixel{
            source[offset] * selection,
            source[offset + 1] * selection,
            source[offset + 2] * selection,
            source[offset + 3]};
    }
    ArtifactCore::VolumetricShine::Settings settings;
    settings.sourcePos = {impl_->sourceX, impl_->sourceY};
    settings.rayLength = impl_->rayLength;
    settings.intensity = impl_->intensity;
    settings.decay = impl_->decay;
    settings.samples = impl_->samples;
    settings.tint = {1.0f + impl_->warmth * 0.2f, 1.0f,
                     1.0f - impl_->warmth * 0.35f, 1.0f};
    const auto selectedHighlights = rays;
    ArtifactCore::VolumetricShine shine;
    shine.process(rays.data(), width, height, settings);

    auto result = image.DeepCopy();
    float* output = result.rgba32fData();
    for (std::size_t pixel = 0; pixel < rays.size(); ++pixel) {
        const std::size_t offset = pixel * 4u;
        output[offset] = source[offset] + rays[pixel].x - selectedHighlights[pixel].x;
        output[offset + 1] = source[offset + 1] + rays[pixel].y - selectedHighlights[pixel].y;
        output[offset + 2] = source[offset + 2] + rays[pixel].z - selectedHighlights[pixel].z;
        output[offset + 3] = source[offset + 3];
    }
    result.setColorDescriptor(image.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> VolumetricShineEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    auto addFloat = [&](const char* name, const char* label, float value,
                        float minimum, float maximum) {
        auto& property = properties.emplace_back();
        property.setName(QString::fromUtf8(name));
        property.setDisplayLabel(QString::fromUtf8(label));
        property.setType(ArtifactCore::PropertyType::Float);
        property.setValue(value); property.setDefaultValue(value);
        property.setHardRange(minimum, maximum); property.setAnimatable(true);
    };
    addFloat("sourceX", "Source X", impl_->sourceX, 0.0f, 1.0f);
    addFloat("sourceY", "Source Y", impl_->sourceY, 0.0f, 1.0f);
    addFloat("rayLength", "Ray Length", impl_->rayLength, 0.0f, 2.0f);
    addFloat("intensity", "Intensity", impl_->intensity, 0.0f, 8.0f);
    addFloat("decay", "Decay", impl_->decay, 0.0f, 1.0f);
    auto& samples = properties.emplace_back();
    samples.setName(QStringLiteral("samples")); samples.setDisplayLabel(QStringLiteral("Samples"));
    samples.setType(ArtifactCore::PropertyType::Integer); samples.setValue(impl_->samples);
    samples.setDefaultValue(40); samples.setHardRange(4, 128); samples.setAnimatable(false);
    addFloat("threshold", "Threshold", impl_->threshold, 0.0f, 4.0f);
    addFloat("warmth", "Warmth", impl_->warmth, -1.0f, 1.0f);
    return properties;
}

void VolumetricShineEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                             const QVariant& value) {
    const QString key = name.toQString(); const float raw = value.toFloat();
    const float n = std::isfinite(raw) ? raw : 0.0f;
    if (key == "sourceX") impl_->sourceX = std::clamp(n, 0.0f, 1.0f);
    else if (key == "sourceY") impl_->sourceY = std::clamp(n, 0.0f, 1.0f);
    else if (key == "rayLength") impl_->rayLength = std::clamp(n, 0.0f, 2.0f);
    else if (key == "intensity") impl_->intensity = std::clamp(n, 0.0f, 8.0f);
    else if (key == "decay") impl_->decay = std::clamp(n, 0.0f, 1.0f);
    else if (key == "samples") impl_->samples = std::clamp(value.toInt(), 4, 128);
    else if (key == "threshold") impl_->threshold = std::clamp(n, 0.0f, 4.0f);
    else if (key == "warmth") impl_->warmth = std::clamp(n, -1.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint VolumetricShineEffect::roiHint() const {
    return EffectROIHint{.kind = EffectROIHintKind::Glow,
                         .requiresFullFrame = true};
}

class GlintStarFilterEffect::Impl {
public:
    float threshold = 0.78f;
    float intensity = 1.1f;
    float length = 36.0f;
    float width = 1.5f;
    int rays = 6;
    float rotation = 0.0f;
    float chromatic = 0.18f;
    float softness = 0.65f;
};

GlintStarFilterEffect::GlintStarFilterEffect() : impl_(new Impl()) {
    setEffectID(ArtifactCore::UniString("builtin.glint_star_filter"));
    setDisplayName(ArtifactCore::UniString("Glint / Star Filter"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setAllowOverscan(true);
}

GlintStarFilterEffect::~GlintStarFilterEffect() {
    delete impl_; impl_ = nullptr;
}

void GlintStarFilterEffect::apply(const ImageF32x4RGBAWithCache& src,
                                  ImageF32x4RGBAWithCache& dst) {
    const auto& image = src.image();
    const int width = image.width(), height = image.height();
    const float* source = image.rgba32fData();
    if (!source || width <= 0 || height <= 0) { dst = src; return; }
    cv::Mat highlights = cv::Mat::zeros(height, width, CV_32FC3);
    for (int y = 0; y < height; ++y) {
        cv::Vec3f* row = highlights.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
            const float lum = source[offset] * 0.2126f + source[offset + 1] * 0.7152f +
                              source[offset + 2] * 0.0722f;
            const float select = std::max(0.0f, lum - impl_->threshold) /
                                 std::max(lum, 0.0001f);
            row[x] = {source[offset] * select, source[offset + 1] * select,
                      source[offset + 2] * select};
        }
    }
    cv::Mat accumulation = cv::Mat::zeros(height, width, CV_32FC3);
    const int rayCount = std::clamp(impl_->rays, 2, 12);
    const cv::Point2f center(width * 0.5f, height * 0.5f);
    for (int ray = 0; ray < rayCount; ++ray) {
        const float angle = impl_->rotation + 180.0f * ray / rayCount;
        const cv::Mat rotate = cv::getRotationMatrix2D(center, -angle, 1.0);
        const cv::Mat restore = cv::getRotationMatrix2D(center, angle, 1.0);
        cv::Mat aligned;
        cv::Mat stretched;
        cv::Mat restored;
        cv::warpAffine(highlights, aligned, rotate, highlights.size(), cv::INTER_LINEAR,
                       cv::BORDER_REFLECT_101);
        const float sigmaX = std::max(0.5f, impl_->length *
            (0.22f + (1.0f - impl_->softness) * 0.18f));
        cv::GaussianBlur(aligned, stretched, cv::Size(), sigmaX,
                         std::max(0.1f, impl_->width));
        cv::warpAffine(stretched, restored, restore, highlights.size(), cv::INTER_LINEAR,
                       cv::BORDER_REFLECT_101);
        accumulation += restored * (impl_->intensity / rayCount);
    }
    auto result = image.DeepCopy(); float* output = result.rgba32fData();
    for (int y = 0; y < height; ++y) {
        const cv::Vec3f* row = accumulation.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4u;
            output[offset] = source[offset] + row[x][0] * (1.0f + impl_->chromatic * 0.2f);
            output[offset + 1] = source[offset + 1] + row[x][1];
            output[offset + 2] = source[offset + 2] + row[x][2] * (1.0f - impl_->chromatic * 0.15f);
            output[offset + 3] = source[offset + 3];
        }
    }
    result.setColorDescriptor(image.colorDescriptor()); dst = ImageF32x4RGBAWithCache(result);
}

std::vector<ArtifactCore::AbstractProperty> GlintStarFilterEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> properties;
    auto addFloat = [&](const char* name, const char* label, float value, float lo, float hi) {
        auto& p = properties.emplace_back(); p.setName(QString::fromUtf8(name));
        p.setDisplayLabel(QString::fromUtf8(label)); p.setType(ArtifactCore::PropertyType::Float);
        p.setValue(value); p.setDefaultValue(value); p.setHardRange(lo, hi); p.setAnimatable(true);
    };
    addFloat("threshold", "Threshold", impl_->threshold, 0.0f, 4.0f);
    addFloat("intensity", "Intensity", impl_->intensity, 0.0f, 8.0f);
    addFloat("length", "Ray Length", impl_->length, 1.0f, 256.0f);
    addFloat("width", "Ray Width", impl_->width, 0.1f, 16.0f);
    auto& rays = properties.emplace_back(); rays.setName("rays"); rays.setDisplayLabel("Ray Count");
    rays.setType(ArtifactCore::PropertyType::Integer); rays.setValue(impl_->rays);
    rays.setDefaultValue(6); rays.setHardRange(2, 12);
    addFloat("rotation", "Rotation", impl_->rotation, -180.0f, 180.0f);
    addFloat("chromatic", "Chromatic Dispersion", impl_->chromatic, 0.0f, 1.0f);
    addFloat("softness", "Falloff Softness", impl_->softness, 0.0f, 1.0f);
    return properties;
}

void GlintStarFilterEffect::setPropertyValue(const ArtifactCore::UniString& name,
                                             const QVariant& value) {
    const QString key = name.toQString(); const float raw = value.toFloat();
    const float n = std::isfinite(raw) ? raw : 0.0f;
    if (key == "threshold") impl_->threshold = std::clamp(n, 0.0f, 4.0f);
    else if (key == "intensity") impl_->intensity = std::clamp(n, 0.0f, 8.0f);
    else if (key == "length") impl_->length = std::clamp(n, 1.0f, 256.0f);
    else if (key == "width") impl_->width = std::clamp(n, 0.1f, 16.0f);
    else if (key == "rays") impl_->rays = std::clamp(value.toInt(), 2, 12);
    else if (key == "rotation") impl_->rotation = std::clamp(n, -180.0f, 180.0f);
    else if (key == "chromatic") impl_->chromatic = std::clamp(n, 0.0f, 1.0f);
    else if (key == "softness") impl_->softness = std::clamp(n, 0.0f, 1.0f);
    else ArtifactAbstractEffect::setPropertyValue(name, value);
}

EffectROIHint GlintStarFilterEffect::roiHint() const {
    return EffectROIHint{.kind = EffectROIHintKind::Glow,
                         .expansionPixels = impl_->length + impl_->width * 3.0f};
}

} // namespace Artifact
