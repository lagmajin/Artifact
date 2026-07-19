module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cstring>
#include <cmath>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Kuwahara;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

class KuwaharaEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float radius_ = 5.0f;
    float sharpness_ = 0.5f;
    bool anisotropic_ = false;

    static cv::Vec4f quadrantMean(const cv::Mat& mat, int cx, int cy, int dx, int dy, int r) {
        int count = 0;
        cv::Vec4f sum(0, 0, 0, 0);
        for (int y = cy + dy; y != cy + dy + r; dy > 0 ? ++y : --y) {
            if (y < 0 || y >= mat.rows) continue;
            for (int x = cx + dx; x != cx + dx + r; dx > 0 ? ++x : --x) {
                if (x < 0 || x >= mat.cols) continue;
                sum += mat.at<cv::Vec4f>(y, x);
                ++count;
            }
        }
        if (count > 0) sum /= static_cast<float>(count);
        return sum;
    }

    static float quadrantVariance(const cv::Mat& mat, int cx, int cy, int dx, int dy, int r, const cv::Vec4f& mean) {
        float var = 0.0f;
        int count = 0;
        for (int y = cy + dy; y != cy + dy + r; dy > 0 ? ++y : --y) {
            if (y < 0 || y >= mat.rows) continue;
            for (int x = cx + dx; x != cx + dx + r; dx > 0 ? ++x : --x) {
                if (x < 0 || x >= mat.cols) continue;
                cv::Vec4f diff = mat.at<cv::Vec4f>(y, x) - mean;
                var += diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];
                ++count;
            }
        }
        return count > 0 ? var / static_cast<float>(count) : 0.0f;
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) { dst = src; return; }
        const int w = srcImage.width();
        const int h = srcImage.height();
        dst = src.DeepCopy();
        auto& dstImage = dst.image();
        float* dstPixels = dstImage.rgba32fData();
        if (!dstPixels) return;
        cv::Mat mat(h, w, CV_32FC4, const_cast<float*>(pixels));
        int r = std::max(1, static_cast<int>(radius_));
        ArtifactCore::Parallel::For(0, h, [&](int y) {
            for (int x = 0; x < w; ++x) {
                cv::Vec4f m0 = quadrantMean(mat, x, y, 0, -r, r);
                cv::Vec4f m1 = quadrantMean(mat, x, y, 0, 0, r);
                cv::Vec4f m2 = quadrantMean(mat, x, y, -r, -r, r);
                cv::Vec4f m3 = quadrantMean(mat, x, y, -r, 0, r);
                float v0 = quadrantVariance(mat, x, y, 0, -r, r, m0);
                float v1 = quadrantVariance(mat, x, y, 0, 0, r, m1);
                float v2 = quadrantVariance(mat, x, y, -r, -r, r, m2);
                float v3 = quadrantVariance(mat, x, y, -r, 0, r, m3);
                float sharp = sharpness_ * sharpness_;
                float w0 = 1.0f / std::max(v0 + sharp, 0.0001f);
                float w1 = 1.0f / std::max(v1 + sharp, 0.0001f);
                float w2 = 1.0f / std::max(v2 + sharp, 0.0001f);
                float w3 = 1.0f / std::max(v3 + sharp, 0.0001f);
                float wsum = w0 + w1 + w2 + w3;
                w0 /= wsum; w1 /= wsum; w2 /= wsum; w3 /= wsum;
                int idx = (y * w + x) * 4;
                dstPixels[idx + 0] = m0[0] * w0 + m1[0] * w1 + m2[0] * w2 + m3[0] * w3;
                dstPixels[idx + 1] = m0[1] * w0 + m1[1] * w1 + m2[1] * w2 + m3[1] * w3;
                dstPixels[idx + 2] = m0[2] * w0 + m1[2] * w1 + m2[2] * w2 + m3[2] * w3;
                dstPixels[idx + 3] = m0[3] * w0 + m1[3] * w1 + m2[3] * w2 + m3[3] * w3;
            }
        });
    }
};

class KuwaharaEffectGPUImpl : public ArtifactEffectImplBase {
public:
    KuwaharaEffectCPUImpl cpuImpl_;
    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) { applyCPU(src, dst); return; }
        auto gpuContext = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
        auto executor = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext);
        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "Kuwahara/ParamsCB";
            cbDesc.Size = sizeof(ParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "KuwaharaParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Kuwahara/PSO";
            desc.shaderSource = kKuwaharaHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("KuwaharaParams", paramsCB_)) { applyCPU(src, dst); return; }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex)) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Kuwahara/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) { applyCPU(src, dst); return; }

        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{};
        params.radius = cpuImpl_.radius_;
        params.sharpness = cpuImpl_.sharpness_;
        params.anisotropic = cpuImpl_.anisotropic_ ? 1.0f : 0.0f;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst)) { applyCPU(src, dst); return; }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float radius = 5.0f;
        float sharpness = 0.5f;
        float anisotropic = 0.0f;
        float pad = 0.0f;
    };

    static constexpr const char* kKuwaharaHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer KuwaharaParams : register(b0) {
    float g_Radius, g_Sharpness, g_Anisotropic, g_Pad;
};
float4 kuwaharaQuadrant(uint2 pos, int2 off, int r) {
    uint w, h;
    g_InputTexture.GetDimensions(w, h);
    float4 sum = 0;
    int count = 0;
    int step = (off.y >= 0) ? 1 : -1;
    for (int y = pos.y + off.y; y != pos.y + off.y + r * step; y += step) {
        if (y < 0 || y >= int(h)) continue;
        for (int x = pos.x + off.x; x != pos.x + off.x + r; ++x) {
            if (x < 0 || x >= int(w)) continue;
            sum += g_InputTexture[uint2(x, y)];
            ++count;
        }
    }
    return count > 0 ? sum / float(count) : float4(0, 0, 0, 0);
}
float kuwaharaVar(uint2 pos, int2 off, int r, float4 mean) {
    uint w, h;
    g_InputTexture.GetDimensions(w, h);
    float var = 0;
    int count = 0;
    int step = (off.y >= 0) ? 1 : -1;
    for (int y = pos.y + off.y; y != pos.y + off.y + r * step; y += step) {
        if (y < 0 || y >= int(h)) continue;
        for (int x = pos.x + off.x; x != pos.x + off.x + r; ++x) {
            if (x < 0 || x >= int(w)) continue;
            float4 d = g_InputTexture[uint2(x, y)] - mean;
            var += d.x * d.x + d.y * d.y + d.z * d.z;
            ++count;
        }
    }
    return count > 0 ? var / float(count) : 0;
}
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;
    int r = max(1, int(g_Radius));
    uint2 p = dtid.xy;
    float4 m0 = kuwaharaQuadrant(p, int2(0, -r), r);
    float4 m1 = kuwaharaQuadrant(p, int2(0, 0), r);
    float4 m2 = kuwaharaQuadrant(p, int2(-r, -r), r);
    float4 m3 = kuwaharaQuadrant(p, int2(-r, 0), r);
    float v0 = kuwaharaVar(p, int2(0, -r), r, m0);
    float v1 = kuwaharaVar(p, int2(0, 0), r, m1);
    float v2 = kuwaharaVar(p, int2(-r, -r), r, m2);
    float v3 = kuwaharaVar(p, int2(-r, 0), r, m3);
    float s = g_Sharpness * g_Sharpness;
    float w0 = 1.0 / max(v0 + s, 0.0001);
    float w1 = 1.0 / max(v1 + s, 0.0001);
    float w2 = 1.0 / max(v2 + s, 0.0001);
    float w3 = 1.0 / max(v3 + s, 0.0001);
    float ws = w0 + w1 + w2 + w3;
    w0 /= ws; w1 /= ws; w2 /= ws; w3 /= ws;
    g_OutputTexture[dtid.xy] = m0 * w0 + m1 * w1 + m2 * w2 + m3 * w3;
}
)";

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src, Diligent::IRenderDevice* device, Diligent::ITexture** outTex) {
        const auto& img = src.image();
        const float* data = img.rgba32fData();
        if (!device || !outTex || !data || img.width() <= 0 || img.height() <= 0) return false;
        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = img.width();
        desc.Height = img.height();
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = "Kuwahara/InputTexture";
        Diligent::TextureSubResData sub{};
        sub.pData = data;
        sub.Stride = static_cast<Diligent::Uint64>(img.width()) * sizeof(float) * 4ull;
        Diligent::TextureData init{};
        init.pSubResources = &sub;
        init.NumSubresources = 1;
        device->CreateTexture(desc, &init, outTex);
        return *outTex != nullptr;
    }

    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx, Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst) {
        if (!device || !ctx || !src) return false;
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
        stagingDesc.Name = "Kuwahara/StagingTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) return false;
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width), CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

KuwaharaEffect::KuwaharaEffect() {
    setDisplayName(UniString("Kuwahara"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<KuwaharaEffectCPUImpl>());
    setGPUImpl(std::make_shared<KuwaharaEffectGPUImpl>());
}

KuwaharaEffect::~KuwaharaEffect() = default;

float KuwaharaEffect::radius() const { return radius_; }
void KuwaharaEffect::setRadius(float v) { radius_ = std::clamp(v, 1.0f, 50.0f); syncImpls(); }
float KuwaharaEffect::sharpness() const { return sharpness_; }
void KuwaharaEffect::setSharpness(float v) { sharpness_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
bool KuwaharaEffect::anisotropic() const { return anisotropic_; }
void KuwaharaEffect::setAnisotropic(bool v) { anisotropic_ = v; syncImpls(); }

void KuwaharaEffect::syncImpls() {
    if (auto* c = dynamic_cast<KuwaharaEffectCPUImpl*>(cpuImpl().get())) {
        c->radius_ = radius_;
        c->sharpness_ = sharpness_;
        c->anisotropic_ = anisotropic_;
    }
    if (auto* g = dynamic_cast<KuwaharaEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.radius_ = radius_;
        g->cpuImpl_.sharpness_ = sharpness_;
        g->cpuImpl_.anisotropic_ = anisotropic_;
    }
}

std::vector<AbstractProperty> KuwaharaEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& r = props.emplace_back(); r.setName("Radius"); r.setType(PropertyType::Float); r.setValue(radius_);
    auto& s = props.emplace_back(); s.setName("Sharpness"); s.setType(PropertyType::Float); s.setValue(sharpness_);
    auto& a = props.emplace_back(); a.setName("Anisotropic"); a.setType(PropertyType::Boolean); a.setValue(anisotropic_);
    return props;
}

void KuwaharaEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Radius") setRadius(v.toFloat());
    else if (k == "Sharpness") setSharpness(v.toFloat());
    else if (k == "Anisotropic") setAnisotropic(v.toBool());
}

} // namespace Artifact
