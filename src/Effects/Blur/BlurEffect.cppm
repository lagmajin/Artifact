module;
#include <utility>
#include <memory>
#include <opencv2/opencv.hpp>
#include <QList>
#include <QByteArray>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <cstring>
#include <algorithm>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.Blur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;

namespace Artifact {

using namespace ArtifactCore;

namespace {

struct BlurParamsCB {
    float radius = 10.0f;
    float horizontal = 1.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
};

const char* kBlurRadiusHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer BlurParams : register(b0) { float g_Radius; float g_Horizontal; float2 g_Pad; };
float gaussianWeight(float x, float sigma) { return exp(-0.5f * (x * x) / max(0.0001f, sigma * sigma)); }
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;
    const float radius = max(1.0f, g_Radius);
    const float sigma = max(0.5f, radius * 0.5f);
    const int r = min(64, (int)ceil(radius));
    float4 sum = 0.0f;
    float weightSum = 0.0f;
    [loop] for (int i = -r; i <= r; ++i) {
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

static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex)
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
    desc.Name = "Blur/InputTexture";
    Diligent::TextureSubResData sub{};
    sub.pData = data;
    sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;
    Diligent::TextureData init{};
    init.pSubResources = &sub;
    init.NumSubresources = 1;
    device->CreateTexture(desc, &init, outTex);
    return *outTex != nullptr;
}

static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst)
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
    stagingDesc.Name = "Blur/StagingTexture";
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

}

class BlurEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float radius_ = 10.0f;
    float strength_ = 1.0f;
    int iterations_ = 1;
    BlurMode mode_ = BlurMode::Gaussian;
    bool premultiplied_ = true;
    float edgeThreshold_ = 0.1f;

    void setRadius(float r) { radius_ = std::max(0.1f, r); }
    void setStrength(float s) { strength_ = std::clamp(s, 0.0f, 1.0f); }
    void setIterations(int n) { iterations_ = std::max(1, n); }
    void setMode(BlurMode m) { mode_ = m; }
    void setPremultiplied(bool p) { premultiplied_ = p; }
    void setEdgeThreshold(float t) { edgeThreshold_ = std::clamp(t, 0.0f, 1.0f); }

    void mixWithSource(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) const {
        const float mix = std::clamp(strength_, 0.0f, 1.0f);
        if (mix >= 0.999f) {
            return;
        }
        const float keep = 1.0f - mix;
        const auto& srcImage = src.image();
        const auto& dstImage = dst.image();
        const float* srcData = srcImage.rgba32fData();
        const float* dstData = dstImage.rgba32fData();
        if (!srcData || !dstData || srcImage.width() != dstImage.width() || srcImage.height() != dstImage.height()) {
            return;
        }
        cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));
        cv::Mat dstMat(dstImage.height(), dstImage.width(), CV_32FC4, const_cast<float*>(dstData));
        cv::Mat blended;
        cv::addWeighted(srcMat, keep, dstMat, mix, 0.0, blended);
        dst.image().setFromRGBA32F(blended.ptr<float>(), blended.cols, blended.rows);
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }
        if (strength_ <= 0.001f) {
            dst = src;
            return;
        }

        cv::Mat floatMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

        std::vector<cv::Mat> channels;
        cv::split(floatMat, channels);
        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        if (premultiplied_) {
            for (int y = 0; y < color.rows; ++y) {
                for (int x = 0; x < color.cols; ++x) {
                    const float a = alpha.at<float>(y, x);
                    if (a > 0.001f) {
                        cv::Vec3f& pixel = color.at<cv::Vec3f>(y, x);
                        pixel[0] /= a;
                        pixel[1] /= a;
                        pixel[2] /= a;
                    }
                }
            }
        }

        const float sigma = std::max(0.1f, radius_ * 0.5f);
        const int ksize = std::max(3, static_cast<int>(sigma * 6.0f) | 1);
        for (int i = 0; i < iterations_; ++i) {
            cv::GaussianBlur(color, color, cv::Size(ksize, ksize),
                             sigma,
                             sigma,
                             cv::BORDER_REPLICATE);
            if (mode_ == BlurMode::EdgePreserving) {
                cv::GaussianBlur(color, color, cv::Size(ksize, ksize),
                                 std::max(0.1f, sigma * 0.6f),
                                 std::max(0.1f, sigma * 0.6f),
                                 cv::BORDER_REPLICATE);
            }
        }

        if (premultiplied_) {
            for (int y = 0; y < color.rows; ++y) {
                for (int x = 0; x < color.cols; ++x) {
                    const float a = alpha.at<float>(y, x);
                    cv::Vec3f& pixel = color.at<cv::Vec3f>(y, x);
                    pixel[0] *= a;
                    pixel[1] *= a;
                    pixel[2] *= a;
                }
            }
        }

        std::vector<cv::Mat> outChannels;
        cv::split(color, outChannels);
        outChannels.push_back(alpha);
        cv::Mat dstMat;
        cv::merge(outChannels, dstMat);
        dst.image().setFromRGBA32F(dstMat.ptr<float>(), dstMat.cols, dstMat.rows);
        mixWithSource(src, dst);
    }
};

class BlurEffectGPUImpl : public ArtifactEffectImplBase {
public:
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    mutable std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;
    mutable bool usingSharedDevice_ = false;

    ~BlurEffectGPUImpl()
    {
        releaseGpuResources();
    }

    void releaseGpuResources() const
    {
        if (context_) {
            context_->Flush();
            context_->WaitForIdle();
        }
        executor_.reset();
        gpuContext_.reset();
        paramsCB_.Release();
        context_.Release();
        device_.Release();
        pipelineReady_ = false;
        if (usingSharedDevice_) {
            releaseSharedRenderDevice();
            usingSharedDevice_ = false;
        }
    }

    bool ensureDevice() const
    {
        if (device_ && context_) {
            return true;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            device_.Release();
            context_.Release();
            return false;
        }
        usingSharedDevice_ = true;
        return device_ && context_;
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (cpuImpl_.strength_ <= 0.001f) {
            dst = src;
            return;
        }
        if (!ensureDevice()) {
            applyCPU(src, dst);
            return;
        }
        auto device = device_;
        auto ctx = context_;

        if (!executor_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device, ctx);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "Blur/ParamsCB";
            cbDesc.Size = sizeof(BlurParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) {
            applyCPU(src, dst);
            return;
        }

        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "BlurParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Blur/RadiusPSO";
            desc.shaderSource = kBlurRadiusHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true)) {
                applyCPU(src, dst);
                return;
            }
            if (!executor_->setBuffer("BlurParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device, &inputTex)) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Blur/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> scratchTex;
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device->CreateTexture(outDesc, nullptr, &scratchTex);
        device->CreateTexture(outDesc, nullptr, &outputTex);
        if (!scratchTex || !outputTex) {
            applyCPU(src, dst);
            return;
        }
        void* mapped = nullptr;
        ctx->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        BlurParamsCB params{};
        params.radius = cpuImpl_.radius_;
        params.horizontal = 1.0f;
        std::memcpy(mapped, &params, sizeof(params));
        ctx->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", scratchTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor_->dispatch(ctx, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        ctx->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        params = {};
        params.radius = cpuImpl_.radius_;
        params.horizontal = 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        ctx->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("g_InputTexture", scratchTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        executor_->dispatch(ctx, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device, ctx, outputTex, dst)) {
            applyCPU(src, dst);
            return;
        }
        cpuImpl_.mixWithSource(src, dst);
    }

    void setRadius(float r) { cpuImpl_.setRadius(r); }
    void setStrength(float s) { cpuImpl_.setStrength(s); }
    void setIterations(int n) { cpuImpl_.setIterations(n); }
    void setMode(BlurMode m) { cpuImpl_.setMode(m); }
    void setPremultiplied(bool p) { cpuImpl_.setPremultiplied(p); }
    void setEdgeThreshold(float t) { cpuImpl_.setEdgeThreshold(t); }

private:
    BlurEffectCPUImpl cpuImpl_;
};

BlurEffect::BlurEffect() {
    setDisplayName(UniString("Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BlurEffectCPUImpl>());
    setGPUImpl(std::make_shared<BlurEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

void BlurEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<BlurEffectCPUImpl*>(cpuImpl().get())) {
        cpu->setRadius(radius_);
        cpu->setStrength(strength_);
        cpu->setIterations(iterations_);
        cpu->setMode(mode_);
        cpu->setPremultiplied(premultiplied_);
        cpu->setEdgeThreshold(edgeThreshold_);
    }
    if (auto* gpu = dynamic_cast<BlurEffectGPUImpl*>(gpuImpl().get())) {
        gpu->setRadius(radius_);
        gpu->setStrength(strength_);
        gpu->setIterations(iterations_);
        gpu->setMode(mode_);
        gpu->setPremultiplied(premultiplied_);
        gpu->setEdgeThreshold(edgeThreshold_);
    }
}

} // namespace Artifact
