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

namespace Artifact {

class GlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float glowGain_ = 1.0f;
    int layerCount_ = 4;
    float baseSigma_ = 5.0f;
    float sigmaGrowth_ = 1.8f;
    float baseAlpha_ = 0.3f;
    float alphaFalloff_ = 0.6f;

    void setGlowGain(float gain) { glowGain_ = gain; }
    float glowGain() const { return glowGain_; }
    void setLayerCount(int count) { layerCount_ = count; }
    int layerCount() const { return layerCount_; }
    void setBaseSigma(float sigma) { baseSigma_ = sigma; }
    float baseSigma() const { return baseSigma_; }
    void setSigmaGrowth(float growth) { sigmaGrowth_ = growth; }
    float sigmaGrowth() const { return sigmaGrowth_; }
    void setBaseAlpha(float alpha) { baseAlpha_ = alpha; }
    float baseAlpha() const { return baseAlpha_; }
    void setAlphaFalloff(float falloff) { alphaFalloff_ = falloff; }
    float alphaFalloff() const { return alphaFalloff_; }

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

    void setGlowGain(float gain) { glowGain_ = gain; }
    float glowGain() const { return glowGain_; }
    void setLayerCount(int count) { layerCount_ = count; }
    int layerCount() const { return layerCount_; }
    void setBaseSigma(float sigma) { baseSigma_ = sigma; }
    float baseSigma() const { return baseSigma_; }
    void setSigmaGrowth(float growth) { sigmaGrowth_ = growth; }
    float sigmaGrowth() const { return sigmaGrowth_; }
    void setBaseAlpha(float alpha) { baseAlpha_ = alpha; }
    float baseAlpha() const { return baseAlpha_; }
    void setAlphaFalloff(float falloff) { alphaFalloff_ = falloff; }
    float alphaFalloff() const { return alphaFalloff_; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

namespace {

struct SharedRenderDeviceLease {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>& device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>& context;

    ~SharedRenderDeviceLease()
    {
        context.Release();
        device.Release();
        releaseSharedRenderDevice();
    }
};

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
    float2 g_Pad;
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

    float3 result = center.rgb + accum / max(alphaSum, 0.0001f);
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
    float pad[2]{};
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
    dst.image().setFromCVMat(temp);
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
    cv::Mat mask;
    cv::cvtColor(color, mask, cv::COLOR_BGR2GRAY);
    cv::threshold(mask, mask, 0.6, 1.0, cv::THRESH_TOZERO);
    if (glowGain_ > 0.0f) {
        mask *= glowGain_;
    }

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
    cv::Mat result = color + glowAccum / alphaNorm;
    cv::min(result, cv::Scalar::all(1.0), result);
    cv::max(result, cv::Scalar::all(0.0), result);
    std::vector<cv::Mat> outChannels;
    cv::split(result, outChannels);
    outChannels.push_back(alpha);
    cv::Mat out;
    cv::merge(outChannels, out);
    writeGlowResultToDestination(out, dst);
}

void GlowEffectGPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    GlowEffectCPUImpl cpu;
    cpu.setGlowGain(glowGain_);
    cpu.setLayerCount(layerCount_);
    cpu.setBaseSigma(baseSigma_);
    cpu.setSigmaGrowth(sigmaGrowth_);
    cpu.setBaseAlpha(baseAlpha_);
    cpu.setAlphaFalloff(alphaFalloff_);
    cpu.applyCPU(src, dst);
}

void GlowEffectGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    if (!acquireSharedRenderDeviceForCurrentBackend(device, context)) {
        applyCPU(src, dst);
        return;
    }
    const SharedRenderDeviceLease sharedDeviceLease{device, context};

    auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
    auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);

    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB;
    Diligent::BufferDesc cbDesc;
    cbDesc.Name = "Glow/ParamsCB";
    cbDesc.Size = sizeof(ParamsCB);
    cbDesc.Usage = Diligent::USAGE_DYNAMIC;
    cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device->CreateBuffer(cbDesc, nullptr, &paramsCB);
    if (!paramsCB) {
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
    if (!executor->build(desc) || !executor->createShaderResourceBinding(true)) {
        applyCPU(src, dst);
        return;
    }
    if (!executor->setBuffer("GlowParams", paramsCB)) {
        applyCPU(src, dst);
        return;
    }

    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createTextureFromImage(src, device, &inputTex, "Glow/InputTexture")) { applyCPU(src, dst); return; }
    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "Glow/OutputTexture";
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
    device->CreateTexture(outDesc, nullptr, &outputTex);
    if (!outputTex) { applyCPU(src, dst); return; }
    void* mapped = nullptr;
    context->MapBuffer(paramsCB, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) { applyCPU(src, dst); return; }
    ParamsCB params{};
    params.glowGain = glowGain_;
    params.layerCount = static_cast<float>(layerCount_);
    params.baseSigma = baseSigma_;
    params.sigmaGrowth = sigmaGrowth_;
    params.baseAlpha = baseAlpha_;
    params.alphaFalloff = alphaFalloff_;
    std::memcpy(mapped, &params, sizeof(params));
    context->UnmapBuffer(paramsCB, Diligent::MAP_WRITE);
    if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyCPU(src, dst);
        return;
    }
    auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
    executor->dispatch(context, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!readbackTexture(device, context, outputTex, dst, "Glow/StagingTexture")) {
        applyCPU(src, dst);
        return;
    }
    if (!imageBuffersDiffer(src, dst)) {
        applyCPU(src, dst);
    }
}

class GlowEffect::Impl {
public:
    std::shared_ptr<GlowEffectCPUImpl> cpuImpl_;
    std::shared_ptr<GlowEffectGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<GlowEffectCPUImpl>();
        gpuImpl_ = std::make_shared<GlowEffectGPUImpl>();
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

std::vector<ArtifactCore::AbstractProperty> GlowEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(6);

    auto& gainProp = props.emplace_back();
    gainProp.setName("glowGain");
    gainProp.setType(ArtifactCore::PropertyType::Float);
    gainProp.setDefaultValue(QVariant(static_cast<double>(glowGain())));
    gainProp.setValue(QVariant(static_cast<double>(glowGain())));

    auto& layerCountProp = props.emplace_back();
    layerCountProp.setName("layerCount");
    layerCountProp.setType(ArtifactCore::PropertyType::Integer);
    layerCountProp.setDefaultValue(QVariant(layerCount()));
    layerCountProp.setValue(QVariant(layerCount()));

    auto& sigmaProp = props.emplace_back();
    sigmaProp.setName("baseSigma");
    sigmaProp.setType(ArtifactCore::PropertyType::Float);
    sigmaProp.setDefaultValue(QVariant(static_cast<double>(baseSigma())));
    sigmaProp.setValue(QVariant(static_cast<double>(baseSigma())));

    auto& growthProp = props.emplace_back();
    growthProp.setName("sigmaGrowth");
    growthProp.setType(ArtifactCore::PropertyType::Float);
    growthProp.setDefaultValue(QVariant(static_cast<double>(sigmaGrowth())));
    growthProp.setValue(QVariant(static_cast<double>(sigmaGrowth())));

    auto& alphaProp = props.emplace_back();
    alphaProp.setName("baseAlpha");
    alphaProp.setType(ArtifactCore::PropertyType::Float);
    alphaProp.setDefaultValue(QVariant(static_cast<double>(baseAlpha())));
    alphaProp.setValue(QVariant(static_cast<double>(baseAlpha())));

    auto& falloffProp = props.emplace_back();
    falloffProp.setName("alphaFalloff");
    falloffProp.setType(ArtifactCore::PropertyType::Float);
    falloffProp.setDefaultValue(QVariant(static_cast<double>(alphaFalloff())));
    falloffProp.setValue(QVariant(static_cast<double>(alphaFalloff())));

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

} // namespace Artifact
