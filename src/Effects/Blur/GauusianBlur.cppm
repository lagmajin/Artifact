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
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Translation.Manager;

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
        sum += g_InputTexture[uint2(samplePos)] * w;
        weightSum += w;
    }

    g_OutputTexture[dtid.xy] = sum / max(weightSum, 0.0001f);
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
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    device->CreateTexture(stagingDesc, nullptr, &staging);
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
    dst.image().setFromCVMat(temp);
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
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
    cv::Mat dstMat;

    // OpenCVのガウシアンブラーを適用
    cv::GaussianBlur(srcMat, dstMat, cv::Size(kernelSize_, kernelSize_), sigma_);

    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(dstMat.ptr<float>(), dstMat.cols, dstMat.rows);
    dst = ImageF32x4RGBAWithCache(dstImage);
}

class GaussianBlurGPUImpl::Resources {
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB;
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
    if (sigma_ <= 0.0f) {
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
            applyGaussianBlurCPUFallback(sigma_, src, dst);
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
        applyGaussianBlurCPUFallback(sigma_, src, dst);
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
            applyGaussianBlurCPUFallback(sigma_, src, dst);
            return;
        }
        resources.pipelineReady = true;
    }

    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createTextureFromImage(src, resources.device, &inputTex,
                                "GaussianBlur/InputTexture")) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }

    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "GaussianBlur/OutputTexture";
    Diligent::RefCntAutoPtr<Diligent::ITexture> tempTex;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
    resources.device->CreateTexture(outDesc, nullptr, &tempTex);
    resources.device->CreateTexture(outDesc, nullptr, &outputTex);
    if (!tempTex || !outputTex) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    void* mapped = nullptr;
    resources.context->MapBuffer(resources.paramsCB, Diligent::MAP_WRITE,
                                 Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    GaussianBlurParamsCB params{};
    params.sigma = sigma_;
    params.horizontal = 1.0f;
    std::memcpy(mapped, &params, sizeof(params));
    resources.context->UnmapBuffer(resources.paramsCB, Diligent::MAP_WRITE);
    if (!resources.executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !resources.executor->setTextureView("g_OutputTexture", tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
    resources.executor->dispatch(resources.context, attribs,
                                 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    resources.context->MapBuffer(resources.paramsCB, Diligent::MAP_WRITE,
                                 Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    params = {};
    params.sigma = sigma_;
    params.horizontal = 0.0f;
    std::memcpy(mapped, &params, sizeof(params));
    resources.context->UnmapBuffer(resources.paramsCB, Diligent::MAP_WRITE);
    if (!resources.executor->setTextureView("g_InputTexture", tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
        !resources.executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    resources.executor->dispatch(resources.context, attribs,
                                 Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (!readbackTexture(resources.device, resources.context, outputTex, dst,
                         "GaussianBlur/StagingTexture")) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
        return;
    }
    if (sigma_ >= 0.5f && !imageBuffersDiffer(src, dst)) {
        applyGaussianBlurCPUFallback(sigma_, src, dst);
    }
}

class GaussianBlur::Impl {
public:
    std::shared_ptr<GaussianBlurCPUImpl> cpuImpl_;
    std::shared_ptr<GaussianBlurGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<GaussianBlurCPUImpl>();
        gpuImpl_ = std::make_shared<GaussianBlurGPUImpl>();
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
