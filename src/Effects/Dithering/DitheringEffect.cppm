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

module Artifact.Effect.Dithering;

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

static const int kNumAlgorithms = 8;

static const float kBayer2x2[4] = {0.0f, 2.0f, 3.0f, 1.0f};
static const float kBayer4x4[16] = {
     0.0f,  8.0f,  2.0f, 10.0f,
    12.0f,  4.0f, 14.0f,  6.0f,
     3.0f, 11.0f,  1.0f,  9.0f,
    15.0f,  7.0f, 13.0f,  5.0f
};

void quantizeChannel(float& ch, float levels) {
    ch = std::round(ch * levels) / levels;
}

class DitheringEffectCPUImpl : public ArtifactEffectImplBase {
public:
    DitherAlgorithm algorithm_ = DitherAlgorithm::Bayer4x4;
    int colorCount_ = 16;
    float amount_ = 1.0f;
    float patternScale_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) { dst = src; return; }
        const int w = srcImage.width();
        const int h = srcImage.height();
        dst = src;
        auto& dstImage = dst.image();
        float* dstPixels = dstImage.rgba32fData();
        if (!dstPixels) return;

        float levels = std::max(2.0f, std::sqrt(static_cast<float>(colorCount_)));
        float levelsPerChannel = std::max(2.0f, levels);

        if (algorithm_ == DitherAlgorithm::Bayer2x2 ||
            algorithm_ == DitherAlgorithm::Bayer4x4 ||
            algorithm_ == DitherAlgorithm::Bayer8x8 ||
            algorithm_ == DitherAlgorithm::Bayer16x16) {
            int bayerSize = 2;
            const float* bayerMat = kBayer2x2;
            int bayerN = 2;
            if (algorithm_ == DitherAlgorithm::Bayer4x4) { bayerSize = 4; bayerMat = kBayer4x4; bayerN = 4; }
            else if (algorithm_ == DitherAlgorithm::Bayer8x8) { bayerSize = 8; bayerN = 8; }
            else if (algorithm_ == DitherAlgorithm::Bayer16x16) { bayerSize = 16; bayerN = 16; }
            float bayerScale = 1.0f / static_cast<float>(bayerN * bayerN);
            float bayerOffset = 0.5f / static_cast<float>(bayerN * bayerN);

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int idx = (y * w + x) * 4;
                    int bx = int((x * patternScale_) / std::max(1.0f, w * 0.01f) * bayerSize) % bayerSize;
                    int by = int((y * patternScale_) / std::max(1.0f, h * 0.01f) * bayerSize) % bayerSize;
                    int bi = (bayerSize > 2) ? (by * bayerSize + bx) : ((by & 1) * 2 + (bx & 1));
                    if (bi >= bayerN * bayerN) bi %= (bayerN * bayerN);
                    float threshold = bayerMat[bi % (bayerN * bayerN)] * bayerScale + bayerOffset;
                    threshold = (threshold - 0.5f) * (1.0f - amount_) * 2.0f + 0.5f;
                    float quantized = std::floor(dstPixels[idx] * levelsPerChannel + threshold) / levelsPerChannel;
                    dstPixels[idx + 0] = std::clamp(quantized, 0.0f, 1.0f);
                    quantized = std::floor(dstPixels[idx + 1] * levelsPerChannel + threshold) / levelsPerChannel;
                    dstPixels[idx + 1] = std::clamp(quantized, 0.0f, 1.0f);
                    quantized = std::floor(dstPixels[idx + 2] * levelsPerChannel + threshold) / levelsPerChannel;
                    dstPixels[idx + 2] = std::clamp(quantized, 0.0f, 1.0f);
                }
            }
        } else {
            // Error diffusion (Floyd-Steinberg, Atkinson, Sierra, Stucki)
            std::vector<float> error(w * h * 3, 0.0f);
            int div = 16;
            int pattern = 0;
            switch (algorithm_) {
                case DitherAlgorithm::FloydSteinberg: div = 16; pattern = 0; break;
                case DitherAlgorithm::Atkinson: div = 8; pattern = 1; break;
                case DitherAlgorithm::Sierra: div = 32; pattern = 2; break;
                case DitherAlgorithm::Stucki: div = 42; pattern = 3; break;
                default: div = 16; pattern = 0; break;
            }

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int idx = (y * w + x) * 4;
                    float r = dstPixels[idx + 0] + error[(y * w + x) * 3 + 0];
                    float g = dstPixels[idx + 1] + error[(y * w + x) * 3 + 1];
                    float b = dstPixels[idx + 2] + error[(y * w + x) * 3 + 2];
                    float oldR = r, oldG = g, oldB = b;
                    r = std::round(r * levelsPerChannel) / levelsPerChannel;
                    g = std::round(g * levelsPerChannel) / levelsPerChannel;
                    b = std::round(b * levelsPerChannel) / levelsPerChannel;
                    dstPixels[idx + 0] = std::clamp(r, 0.0f, 1.0f);
                    dstPixels[idx + 1] = std::clamp(g, 0.0f, 1.0f);
                    dstPixels[idx + 2] = std::clamp(b, 0.0f, 1.0f);
                    float er = (oldR - r) * amount_;
                    float eg = (oldG - g) * amount_;
                    float eb = (oldB - b) * amount_;

                    auto addErr = [&](int px, int py, float wR, float wG, float wB, int d) {
                        if (px < 0 || px >= w || py < 0 || py >= h) return;
                        int xi = (py * w + px) * 3;
                        float f = 1.0f / static_cast<float>(d);
                        error[xi + 0] += er * wR * f;
                        error[xi + 1] += eg * wG * f;
                        error[xi + 2] += eb * wB * f;
                    };

                    if (pattern == 0) {
                        addErr(x + 1, y, 7, 7, 7, div);
                        addErr(x - 1, y + 1, 3, 3, 3, div);
                        addErr(x, y + 1, 5, 5, 5, div);
                        addErr(x + 1, y + 1, 1, 1, 1, div);
                    } else if (pattern == 1) {
                        addErr(x + 1, y, 1, 1, 1, div);
                        addErr(x + 2, y, 1, 1, 1, div);
                        addErr(x - 1, y + 1, 1, 1, 1, div);
                        addErr(x, y + 1, 1, 1, 1, div);
                        addErr(x + 1, y + 1, 1, 1, 1, div);
                        addErr(x, y + 2, 1, 1, 1, div);
                    } else if (pattern == 2) {
                        addErr(x + 1, y, 5, 5, 5, div);
                        addErr(x + 2, y, 3, 3, 3, div);
                        addErr(x - 2, y + 1, 2, 2, 2, div);
                        addErr(x - 1, y + 1, 4, 4, 4, div);
                        addErr(x, y + 1, 5, 5, 5, div);
                        addErr(x + 1, y + 1, 4, 4, 4, div);
                        addErr(x + 2, y + 1, 2, 2, 2, div);
                        addErr(x - 1, y + 2, 2, 2, 2, div);
                        addErr(x, y + 2, 3, 3, 3, div);
                        addErr(x + 1, y + 2, 2, 2, 2, div);
                    } else if (pattern == 3) {
                        addErr(x + 1, y, 8, 8, 8, div);
                        addErr(x + 2, y, 4, 4, 4, div);
                        addErr(x - 2, y + 1, 2, 2, 2, div);
                        addErr(x - 1, y + 1, 4, 4, 4, div);
                        addErr(x, y + 1, 8, 8, 8, div);
                        addErr(x + 1, y + 1, 4, 4, 4, div);
                        addErr(x + 2, y + 1, 2, 2, 2, div);
                        addErr(x - 2, y + 2, 1, 1, 1, div);
                        addErr(x - 1, y + 2, 2, 2, 2, div);
                        addErr(x, y + 2, 4, 4, 4, div);
                        addErr(x + 1, y + 2, 2, 2, 2, div);
                        addErr(x + 2, y + 2, 1, 1, 1, div);
                    }
                }
            }
        }
    }
};

class DitheringEffectGPUImpl : public ArtifactEffectImplBase {
public:
    DitheringEffectCPUImpl cpuImpl_;
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
            cbDesc.Name = "Dithering/ParamsCB";
            cbDesc.Size = sizeof(ParamsCB);
            cbDesc.Usage = Diligent::USAGE_DYNAMIC;
            cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
            cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
            device_->CreateBuffer(cbDesc, nullptr, &paramsCB_);
        }
        if (!paramsCB_) { applyCPU(src, dst); return; }
        static Diligent::ShaderResourceVariableDesc vars[] = {
            {Diligent::SHADER_TYPE_COMPUTE, "DitheringParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "Dithering/PSO";
            desc.shaderSource = kDitheringHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor->build(desc) || !executor->createShaderResourceBinding(true) || !executor->setBuffer("DitheringParams", paramsCB_)) { applyCPU(src, dst); return; }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex)) { applyCPU(src, dst); return; }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "Dithering/OutputTexture";
        Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex;
        device_->CreateTexture(outDesc, nullptr, &outputTex);
        if (!outputTex) { applyCPU(src, dst); return; }

        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) { applyCPU(src, dst); return; }
        ParamsCB params{};
        params.algorithm = static_cast<float>(static_cast<int>(cpuImpl_.algorithm_));
        params.colorCount = static_cast<float>(cpuImpl_.colorCount_);
        params.amount = cpuImpl_.amount_;
        params.patternScale = cpuImpl_.patternScale_;
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor->setTextureView("g_OutputTexture", outputTex->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) { applyCPU(src, dst); return; }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex, dst)) { applyCPU(src, dst); return; }
    }

private:
    struct ParamsCB {
        float algorithm = 1.0f;
        float colorCount = 16.0f;
        float amount = 1.0f;
        float patternScale = 1.0f;
    };

    static constexpr const char* kDitheringHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer DitheringParams : register(b0) {
    float g_Algorithm, g_ColorCount, g_Amount, g_PatternScale;
};
static const float bayer4[16] = {
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5
};
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;
    float4 color = g_InputTexture[dtid.xy];
    float levels = max(2.0, sqrt(g_ColorCount));
    int algo = int(g_Algorithm);
    if (algo == 0 || algo == 1) {
        int bayerN = (algo == 0) ? 2 : 4;
        float scale = 1.0 / float(bayerN * bayerN);
        float offset = 0.5 / float(bayerN * bayerN);
        int bx = int(dtid.x) % bayerN;
        int by = int(dtid.y) % bayerN;
        int bi = (algo == 0) ? ((by & 1) * 2 + (bx & 1)) : (by * 4 + bx);
        float threshold = bayer4[bi % 16] * scale + offset;
        threshold = (threshold - 0.5) * (1.0 - g_Amount) * 2.0 + 0.5;
        color.r = floor(color.r * levels + threshold) / levels;
        color.g = floor(color.g * levels + threshold) / levels;
        color.b = floor(color.b * levels + threshold) / levels;
    }
    g_OutputTexture[dtid.xy] = float4(saturate(color.rgb), color.a);
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
        desc.Name = "Dithering/InputTexture";
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
        stagingDesc.Name = "Dithering/StagingTexture";
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

DitheringEffect::DitheringEffect() {
    setDisplayName(UniString("Dithering"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<DitheringEffectCPUImpl>());
    setGPUImpl(std::make_shared<DitheringEffectGPUImpl>());
}

DitheringEffect::~DitheringEffect() = default;

DitherAlgorithm DitheringEffect::algorithm() const { return algorithm_; }
void DitheringEffect::setAlgorithm(DitherAlgorithm v) { algorithm_ = v; syncImpls(); }
int DitheringEffect::colorCount() const { return colorCount_; }
void DitheringEffect::setColorCount(int v) { colorCount_ = std::clamp(v, 2, 256); syncImpls(); }
float DitheringEffect::amount() const { return amount_; }
void DitheringEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float DitheringEffect::patternScale() const { return patternScale_; }
void DitheringEffect::setPatternScale(float v) { patternScale_ = std::max(0.1f, v); syncImpls(); }

void DitheringEffect::syncImpls() {
    if (auto* c = dynamic_cast<DitheringEffectCPUImpl*>(cpuImpl().get())) {
        c->algorithm_ = algorithm_;
        c->colorCount_ = colorCount_;
        c->amount_ = amount_;
        c->patternScale_ = patternScale_;
    }
    if (auto* g = dynamic_cast<DitheringEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.algorithm_ = algorithm_;
        g->cpuImpl_.colorCount_ = colorCount_;
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.patternScale_ = patternScale_;
    }
}

std::vector<AbstractProperty> DitheringEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Algorithm"); a.setType(PropertyType::Integer); a.setValue(static_cast<int>(algorithm_));
    auto& c = props.emplace_back(); c.setName("Color Count"); c.setType(PropertyType::Integer); c.setValue(colorCount_);
    auto& m = props.emplace_back(); m.setName("Amount"); m.setType(PropertyType::Float); m.setValue(amount_);
    auto& p = props.emplace_back(); p.setName("Pattern Scale"); p.setType(PropertyType::Float); p.setValue(patternScale_);
    return props;
}

void DitheringEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Algorithm") setAlgorithm(static_cast<DitherAlgorithm>(v.toInt()));
    else if (k == "Color Count") setColorCount(v.toInt());
    else if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Pattern Scale") setPatternScale(v.toFloat());
}

} // namespace Artifact
