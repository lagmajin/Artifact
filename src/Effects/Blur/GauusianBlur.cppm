module;
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Effect.GauusianBlur;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Image.GpuImageUpload;
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Translation.Manager;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

namespace {

struct GaussianBlurParamsCB {
    float sigma = 5.0f;
    float horizontal = 1.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
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

void applyGaussianBlurCPUFallback(float sigma,
                                  const ImageF32x4RGBAWithCache& src,
                                  ImageF32x4RGBAWithCache& dst)
{
    GaussianBlurCPUImpl cpuImpl(sigma);
    cpuImpl.applyCPU(src, dst);
}

const char* kGaussianBlurHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GaussianBlurParams : register(b0) { float g_Sigma; float g_Horizontal; float2 g_Pad; };

float gaussianWeight(float x, float sigma)
{
    return exp(-0.5f * (x * x) / max(0.0001f, sigma * sigma));
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    const float sigma = max(0.1f, g_Sigma);
    const int radius = min(64, (int)ceil(sigma * 3.0f));
    float4 sum = 0.0f;
    float weightSum = 0.0f;

    [loop] for (int i = -radius; i <= radius; ++i) {
        int2 samplePos = int2(dtid.xy);
        if (g_Horizontal > 0.5f) samplePos.x = clamp(samplePos.x + i, 0, int(width) - 1);
        else samplePos.y = clamp(samplePos.y + i, 0, int(height) - 1);
        const float w = gaussianWeight((float)i, sigma);
        const float4 sample = g_InputTexture[uint2(samplePos)];
        sum.rgb += sample.rgb * sample.a * w;
        sum.a += sample.a * w;
        weightSum += w;
    }

    sum /= max(weightSum, 0.0001f);
    if (sum.a > 1e-5f) sum.rgb /= sum.a;
    g_OutputTexture[dtid.xy] = sum;
}
)";

static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src,
                                   Diligent::IRenderDevice* device,
                                   Diligent::ITexture** outTex,
                                   const char* name)
{
    if (!device || !outTex) {
        return false;
    }
    const auto& img = src.image();
    const auto upload = ArtifactCore::makeGpuImageUploadBuffer(img.surfaceView());
    if (!upload.isValid() || img.width() <= 0 || img.height() <= 0) {
        return false;
    }
    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(img.width());
    desc.Height = static_cast<Diligent::Uint32>(img.height());
    desc.Format = upload.format == ArtifactCore::GpuImageFormat::Rgba16Float
        ? Diligent::TEX_FORMAT_RGBA16_FLOAT
        : Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Name = name;
    Diligent::TextureSubResData sub{};
    sub.pData = upload.bytes.data();
    sub.Stride = upload.rowStride;
    Diligent::TextureData init{};
    init.pSubResources = &sub;
    init.NumSubresources = 1;
    device->CreateTexture(desc, &init, outTex);
    return *outTex != nullptr;
}

static bool readbackTexture(Diligent::IRenderDevice* device,
                            Diligent::IDeviceContext* ctx,
                            Diligent::ITexture* src,
                            Diligent::RefCntAutoPtr<Diligent::ITexture>& staging,
                            ImageF32x4RGBAWithCache& dst,
                            const ArtifactCore::SurfaceColorDescriptor& colorDescriptor,
                            const char* name)
{
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
    if (!staging || staging->GetDesc().Width != stagingDesc.Width ||
        staging->GetDesc().Height != stagingDesc.Height ||
        staging->GetDesc().Format != stagingDesc.Format) {
        staging.Release();
        device->CreateTexture(stagingDesc, nullptr, &staging);
    }
    if (!staging) {
        return false;
    }
    Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                      staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->CopyTexture(copy);
    Diligent::MappedTextureSubresource mapped{};
    ctx->Flush();
    ctx->WaitForIdle();
    ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
    if (!mapped.pData || mapped.Stride == 0) {
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(desc.Width) * sizeof(float) * 4ull;
    if (mapped.Stride < rowBytes) {
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return false;
    }
    cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
    dst.image().setFromCVMat(temp, colorDescriptor);
    ctx->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

} // namespace

void GaussianBlurCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat source(srcImage.height(), srcImage.width(), CV_32FC4,
                   const_cast<float*>(srcData));
    cv::Mat srcMat;
    if (srcImage.colorDescriptor().channelOrder ==
        ArtifactCore::SurfaceChannelOrder::BGRA) {
        cv::cvtColor(source, srcMat, cv::COLOR_BGRA2RGBA);
    } else {
        srcMat = source;
    }
    cv::Mat dstMat;

    // Blur RGB in premultiplied-alpha space to avoid transparent-edge color bleed.
    cv::Mat premultiplied = srcMat.clone();
    for (int y = 0; y < premultiplied.rows; ++y) {
        auto* row = premultiplied.ptr<cv::Vec4f>(y);
        for (int x = 0; x < premultiplied.cols; ++x) {
            row[x][0] *= row[x][3];
            row[x][1] *= row[x][3];
            row[x][2] *= row[x][3];
        }
    }

    const float appliedSigma = context_.isInteractive
        ? std::max(0.0f, sigma_ *
              std::clamp(context_.resolutionScale, 0.125f, 1.0f))
        : sigma_;
    int appliedKernelSize = std::max(1, static_cast<int>(6.0f * appliedSigma + 1.0f));
    if (appliedKernelSize % 2 == 0) {
        ++appliedKernelSize;
    }
    cv::GaussianBlur(premultiplied, dstMat,
                     cv::Size(appliedKernelSize, appliedKernelSize),
                     appliedSigma);
    for (int y = 0; y < dstMat.rows; ++y) {
        auto* row = dstMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < dstMat.cols; ++x) {
            if (row[x][3] > 1e-5f) {
                row[x][0] /= row[x][3];
                row[x][1] /= row[x][3];
                row[x][2] /= row[x][3];
            } else {
                row[x][0] = row[x][1] = row[x][2] = 0.0f;
            }
        }
    }

    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    auto outputDescriptor = srcImage.colorDescriptor();
    outputDescriptor.channelOrder = ArtifactCore::SurfaceChannelOrder::RGBA;
    dstImage.setFromRGBA32F(dstMat.ptr<float>(), dstMat.cols, dstMat.rows,
                            outputDescriptor);
    dst = ImageF32x4RGBAWithCache(dstImage);
}

class GaussianBlurGPUImpl::Resources {
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB;
    Diligent::RefCntAutoPtr<Diligent::ITexture> tempTex;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex;
    bool pipelineReady = false;
    bool usingSharedDevice = false;

    ~Resources() {
        if (context) {
            context->Flush();
            context->WaitForIdle();
        }
        executor.reset();
        gpuContext.reset();
        paramsCB.Release();
        context.Release();
        device.Release();
        if (usingSharedDevice) {
            releaseSharedRenderDevice();
        }
    }
};

GaussianBlurGPUImpl::GaussianBlurGPUImpl()
    : resources_(std::make_unique<Resources>()) {}

GaussianBlurGPUImpl::GaussianBlurGPUImpl(const float sigma)
    : sigma_(sigma), resources_(std::make_unique<Resources>()) {}

GaussianBlurGPUImpl::~GaussianBlurGPUImpl() = default;

void GaussianBlurGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const float appliedSigma = context_.isInteractive
        ? std::max(0.0f, sigma_ *
              std::clamp(context_.resolutionScale, 0.125f, 1.0f))
        : sigma_;
    if (appliedSigma <= 0.0f) {
        dst = src;
        return;
    }

    if (!resources_) {
        resources_ = std::make_unique<Resources>();
    }
    auto& resources = *resources_;
    if (!resources.device || !resources.context) {
        if (!acquireSharedRenderDeviceForCurrentBackend(resources.device,
                                                        resources.context)) {
            applyGaussianBlurCPUFallback(appliedSigma, src, dst);
            return;
        }
        resources.usingSharedDevice = true;
    }
    if (!resources.executor) {
        resources.gpuContext = std::make_unique<ArtifactCore::GpuContext>(
            resources.device, resources.context);
        resources.executor = std::make_unique<ArtifactCore::ComputeExecutor>(
            *resources.gpuContext);
    }
    if (!resources.paramsCB) {
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "GaussianBlur/ParamsCB";
        cbDesc.Size = sizeof(GaussianBlurParamsCB);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        resources.device->CreateBuffer(cbDesc, nullptr, &resources.paramsCB);
    }
    if (!resources.paramsCB) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }

    static Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_COMPUTE, "GaussianBlurParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };

    ArtifactCore::ComputePipelineDesc desc;
    desc.name = "GaussianBlur/RadiusPSO";
    desc.shaderSource = kGaussianBlurHlsl;
    desc.entryPoint = "main";
    desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    desc.variables = vars;
    desc.variableCount = 3;
    desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    if (!resources.pipelineReady) {
        if (!resources.executor->build(desc) ||
            !resources.executor->createShaderResourceBinding(true) ||
            !resources.executor->setBuffer("GaussianBlurParams",
                                           resources.paramsCB)) {
            applyGaussianBlurCPUFallback(appliedSigma, src, dst);
            return;
        }
        resources.pipelineReady = true;
    }

    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createTextureFromImage(src, resources.device, &inputTex,
                                "GaussianBlur/InputTexture")) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }

    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "GaussianBlur/OutputTexture";
    const auto textureMatches = [&outDesc](
        const Diligent::RefCntAutoPtr<Diligent::ITexture>& texture) {
        if (!texture) return false;
        const auto current = texture->GetDesc();
        return current.Width == outDesc.Width &&
               current.Height == outDesc.Height &&
               current.Format == outDesc.Format;
    };
    if (!textureMatches(resources.tempTex) ||
        !textureMatches(resources.outputTex)) {
        resources.tempTex.Release();
        resources.outputTex.Release();
        resources.device->CreateTexture(outDesc, nullptr, &resources.tempTex);
        resources.device->CreateTexture(outDesc, nullptr, &resources.outputTex);
    }
    if (!resources.tempTex || !resources.outputTex) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    void* mapped = nullptr;
    resources.context->MapBuffer(resources.paramsCB, Diligent::MAP_WRITE,
                                 Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    GaussianBlurParamsCB params{};
    params.sigma = appliedSigma;
    params.horizontal = 1.0f;
    std::memcpy(mapped, &params, sizeof(params));
    resources.context->UnmapBuffer(resources.paramsCB, Diligent::MAP_WRITE);
    if (!resources.executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !resources.executor->setTextureView("g_OutputTexture", resources.tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
    resources.executor->dispatch(resources.context, attribs,
                                 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    resources.context->MapBuffer(resources.paramsCB, Diligent::MAP_WRITE,
                                 Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    params = {};
    params.sigma = appliedSigma;
    params.horizontal = 0.0f;
    std::memcpy(mapped, &params, sizeof(params));
    resources.context->UnmapBuffer(resources.paramsCB, Diligent::MAP_WRITE);
    if (!resources.executor->setTextureView("g_InputTexture", resources.tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !resources.executor->setTextureView("g_OutputTexture", resources.outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    resources.executor->dispatch(resources.context, attribs,
                                 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    auto outputDescriptor = src.image().colorDescriptor();
    outputDescriptor.channelOrder = ArtifactCore::SurfaceChannelOrder::RGBA;
    if (!readbackTexture(resources.device, resources.context, resources.outputTex,
                         resources.stagingTex, dst,
                         outputDescriptor,
                         "GaussianBlur/StagingTexture")) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
        return;
    }
    if (appliedSigma >= 0.5f && !imageBuffersDiffer(src, dst)) {
        applyGaussianBlurCPUFallback(appliedSigma, src, dst);
    }
}

class GaussianBlur::Impl {
public:
    ArtifactCore::SharedPtr<GaussianBlurCPUImpl> cpuImpl_;
    ArtifactCore::SharedPtr<GaussianBlurGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = ArtifactCore::makeShared<GaussianBlurCPUImpl>();
        gpuImpl_ = ArtifactCore::makeShared<GaussianBlurGPUImpl>();
    }
};

GaussianBlur::GaussianBlur() : impl_(new Impl()) {
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
    setComputeMode(ComputeMode::GPU);
    setEffectID(UniString("effect.blur.gaussian"));
    setDisplayName(UniString("Gaussian Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
}

GaussianBlur::~GaussianBlur() {
    delete impl_;
}

void GaussianBlur::setSigma(float sigma) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setSigma(sigma);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setSigma(sigma);
    }
}

float GaussianBlur::sigma() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->sigma();
    }
    return 0.0f;
}

std::vector<AbstractProperty> GaussianBlur::getProperties() const {
    std::vector<AbstractProperty> props;
    AbstractProperty sigmaProp;
    sigmaProp.setName(TranslationManager::instance().tr("effect.blur.gaussian.strength", "Strength"));
    sigmaProp.setType(PropertyType::Float);
    sigmaProp.setValue(sigma());
    sigmaProp.setHardRange(0.0, 64.0);
    sigmaProp.setSoftRange(0.0, 16.0);
    sigmaProp.setStep(0.1);
    props.push_back(sigmaProp);
    return props;
}

void GaussianBlur::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QString::fromLatin1("Strength") ||
        key == TranslationManager::instance().tr("effect.blur.gaussian.strength", "Strength")) {
        setSigma(value.toFloat());
    }
}

EffectROIHint GaussianBlur::roiHint() const {
    // ガウスカーネルの有効範囲は 3σ。
    // sigma が大きいほど広い入力領域が必要になる。
    const float s = sigma();
    return EffectROIHint{
        .kind = EffectROIHintKind::Blur,
        .expansionPixels = s * 3.0f,
        .requiresFullFrame = false
    };
}

}
