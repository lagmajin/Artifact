module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Rasterizer.DeBlink;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import ImageProcessing;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

namespace Artifact {
using namespace ArtifactCore;

class DeBlinkCPUImpl : public ArtifactEffectImplBase {
public:
    double threshold_=0.06, strength_=1.0, sceneCut_=0.25, maxGain_=4.0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        const int W = si.width(), H = si.height();
        if (!sd || W <= 0 || H <= 0) {
            dst = src;
            return;
        }
        const double curLuma = Repair::frameLuma(sd, W, H);
        double prevLuma = std::numeric_limits<double>::quiet_NaN();
        double nextLuma = std::numeric_limits<double>::quiet_NaN();
        if (context_.sampler) {
            ImageF32x4RGBAWithCache neighbor;
            if (context_.sampler->sampleCurrentLayerFrameRelative(-1, neighbor) &&
                neighbor.image().rgba32fData())
                prevLuma = Repair::frameLuma(neighbor.image().rgba32fData(),
                                             neighbor.width(), neighbor.height());
            if (context_.sampler->sampleCurrentLayerFrameRelative(1, neighbor) &&
                neighbor.image().rgba32fData())
                nextLuma = Repair::frameLuma(neighbor.image().rgba32fData(),
                                             neighbor.width(), neighbor.height());
        }
        Repair::DeBlinkParams params;
        params.threshold = threshold_;
        params.strength = strength_;
        params.sceneCut = sceneCut_;
        params.maxGain = maxGain_;
        bool applied = false;
        const double gain = Repair::deblinkGain(curLuma, prevLuma, nextLuma,
                                               params, &applied);
        if (!applied || gain == 1.0) {
            dst = src;
            return;
        }
        const float fac = static_cast<float>(gain);
        dst = src.DeepCopy();
        float* d = dst.image().rgba32fData();
        ArtifactCore::Parallel::For(0, H, W * H, [&](int y) {
            float* o = d + static_cast<size_t>(y) * W * 4;
            for (int x = 0; x < W; ++x) {
                float* p = o + static_cast<size_t>(x) * 4;
                p[0] = std::clamp(p[0] * fac, 0.0f, 1.0f);
                p[1] = std::clamp(p[1] * fac, 0.0f, 1.0f);
                p[2] = std::clamp(p[2] * fac, 0.0f, 1.0f);
            }
        });
    }
};

class DeBlinkGPUImpl : public ArtifactEffectImplBase {
public:
    double threshold_ = 0.06, strength_ = 1.0, sceneCut_ = 0.25, maxGain_ = 4.0;

    void applyCPU(const ImageF32x4RGBAWithCache& s, ImageF32x4RGBAWithCache& d) override {
        // Gain is resolved on the host; reuse the CPU path for it, then the
        // GPU path only runs when a correction was actually detected. The
        // effect re-resolves per frame, so delegate fully here.
        DeBlinkCPUImpl cpu;
        cpu.threshold_ = threshold_;
        cpu.strength_ = strength_;
        cpu.sceneCut_ = sceneCut_;
        cpu.maxGain_ = maxGain_;
        cpu.setContext(ArtifactEffectImplBase::context_);
        cpu.applyCPU(s, d);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto& si = src.image();
        const float* sd = si.rgba32fData();
        const int W = si.width(), H = si.height();
        if (!sd || W <= 0 || H <= 0) {
            dst = src;
            return;
        }
        const double curLuma = Repair::frameLuma(sd, W, H);
        double prevLuma = std::numeric_limits<double>::quiet_NaN();
        double nextLuma = std::numeric_limits<double>::quiet_NaN();
        if (ArtifactEffectImplBase::context_.sampler) {
            ImageF32x4RGBAWithCache neighbor;
            if (ArtifactEffectImplBase::context_.sampler->sampleCurrentLayerFrameRelative(-1, neighbor) &&
                neighbor.image().rgba32fData())
                prevLuma = Repair::frameLuma(neighbor.image().rgba32fData(),
                                             neighbor.width(), neighbor.height());
            if (ArtifactEffectImplBase::context_.sampler->sampleCurrentLayerFrameRelative(1, neighbor) &&
                neighbor.image().rgba32fData())
                nextLuma = Repair::frameLuma(neighbor.image().rgba32fData(),
                                             neighbor.width(), neighbor.height());
        }
        Repair::DeBlinkParams params;
        params.threshold = threshold_;
        params.strength = strength_;
        params.sceneCut = sceneCut_;
        params.maxGain = maxGain_;
        bool applied = false;
        const double gain = Repair::deblinkGain(curLuma, prevLuma, nextLuma,
                                               params, &applied);
        if (!applied || gain == 1.0) {
            dst = src;
            return;
        }
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, deviceContext_)) {
            applyCPU(src, dst);
            return;
        }
        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, deviceContext_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) {
            applyCPU(src, dst);
            return;
        }
        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "DeBlink/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "DeBlinkParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "DeBlink/PSO";
            desc.shaderSource = kShader;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("DeBlinkParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "DeBlink/InputTexture")) {
            applyCPU(src, dst);
            return;
        }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "DeBlink/OutputTexture";
        if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
            outputTex_->GetDesc().Height != outDesc.Height) {
            outputTex_.Release();
            device_->CreateTexture(outDesc, nullptr, &outputTex_);
        }
        if (!outputTex_) {
            applyCPU(src, dst);
            return;
        }
        void* mapped = nullptr;
        deviceContext_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        ParamsCB cb{};
        cb.gain = static_cast<float>(gain);
        std::memcpy(mapped, &cb, sizeof(cb));
        deviceContext_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("g_InputTexture",
                inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture",
                outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(
            outDesc.Width, outDesc.Height, 1, 16, 16, 1);
        executor_->dispatch(deviceContext_, attribs,
                            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, deviceContext_, outputTex_, dst,
                             "DeBlink/StagingTexture",
                             si.colorDescriptor())) {
            applyCPU(src, dst);
        }
    }

private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    bool pipelineReady_ = false;

    struct ParamsCB {
        float gain = 1.0f;
        float pad0 = 0.0f;
        float pad1 = 0.0f;
        float pad2 = 0.0f;
    };

    static constexpr const char* kShader = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer DeBlinkParams : register(b0) { float g_Gain; float3 Pad; };
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
  uint w, h; g_OutputTexture.GetDimensions(w, h);
  if (id.x >= w || id.y >= h) return;
  float4 px = g_InputTexture[id.xy];
  g_OutputTexture[id.xy] = float4(saturate(px.rgb * g_Gain), px.a);
})";

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& src,
                                       Diligent::IRenderDevice* device,
                                       Diligent::ITexture** outTex, const char* name) {
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

    static bool readbackTexture(Diligent::IRenderDevice* device, Diligent::IDeviceContext* ctx,
                                Diligent::ITexture* src, ImageF32x4RGBAWithCache& dst,
                                const char* name, const auto& colorDescriptor) {
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
        stagingDesc.Name = name;
        Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
        device->CreateTexture(stagingDesc, nullptr, &staging);
        if (!staging) return false;
        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        ctx->Flush();
        ctx->WaitForIdle();
        Diligent::MappedTextureSubresource mapped{};
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;
        cv::Mat temp(static_cast<int>(desc.Height), static_cast<int>(desc.Width),
                     CV_32FC4, mapped.pData, mapped.Stride);
        dst.image().setFromCVMat(temp, colorDescriptor);
        ctx->UnmapTextureSubresource(staging, 0, 0);
        return true;
    }
};

DeBlinkEffect::DeBlinkEffect() : ArtifactAbstractEffect() {
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
    setComputeMode(ComputeMode::AUTO);
}
DeBlinkEffect::~DeBlinkEffect() = default;

double DeBlinkEffect::threshold() const { return threshold_; }
void DeBlinkEffect::setThreshold(double v) {
    threshold_ = std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.06;
    syncImpls();
}
double DeBlinkEffect::strength() const { return strength_; }
void DeBlinkEffect::setStrength(double v) {
    strength_ = std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 1.0;
    syncImpls();
}
double DeBlinkEffect::sceneCut() const { return sceneCut_; }
void DeBlinkEffect::setSceneCut(double v) {
    sceneCut_ = std::isfinite(v) ? std::clamp(v, 0.0, 2.0) : 0.25;
    syncImpls();
}
double DeBlinkEffect::maxGain() const { return maxGain_; }
void DeBlinkEffect::setMaxGain(double v) {
    maxGain_ = std::isfinite(v) ? std::clamp(v, 1.0, 16.0) : 4.0;
    syncImpls();
}

std::vector<AbstractProperty> DeBlinkEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(4);
    auto addFloat = [&props](const char* name, double value, double minValue, double maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(value));
        prop.setDefaultValue(QVariant(value));
        prop.setMinValue(QVariant(minValue));
        prop.setMaxValue(QVariant(maxValue));
        props.push_back(std::move(prop));
    };
    addFloat("threshold", threshold_, 0.0, 1.0);
    addFloat("strength", strength_, 0.0, 1.0);
    addFloat("sceneCut", sceneCut_, 0.0, 2.0);
    addFloat("maxGain", maxGain_, 1.0, 16.0);
    return props;
}

void DeBlinkEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "threshold") setThreshold(v.toDouble());
    else if (k == "strength") setStrength(v.toDouble());
    else if (k == "sceneCut") setSceneCut(v.toDouble());
    else if (k == "maxGain") setMaxGain(v.toDouble());
}

void DeBlinkEffect::syncImpls() {
    auto c = ArtifactCore::makeShared<DeBlinkCPUImpl>();
    c->threshold_ = threshold_;
    c->strength_ = strength_;
    c->sceneCut_ = sceneCut_;
    c->maxGain_ = maxGain_;
    setCPUImpl(c);
    auto g = ArtifactCore::makeShared<DeBlinkGPUImpl>();
    g->threshold_ = threshold_;
    g->strength_ = strength_;
    g->sceneCut_ = sceneCut_;
    g->maxGain_ = maxGain_;
    setGPUImpl(g);
}

} // namespace Artifact
