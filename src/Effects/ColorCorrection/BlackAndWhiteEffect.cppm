module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QColor>
#include <QVariant>
#include <QElapsedTimer>
#include <QSettings>
#include <QDebug>
#include <QtGlobal>
#include <vector>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module BlackAndWhiteEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

namespace {
inline float calculateBWLuma(float r, float g, float b,
                             float reds, float yellows, float greens,
                             float cyans, float blues, float magentas) {
    const float maxVal = std::max({r, g, b});
    const float minVal = std::min({r, g, b});
    const float delta = maxVal - minVal;

    // 彩度が極めて低い場合は通常の明度
    if (delta <= 1.0e-5f) {
        return maxVal;
    }

    // 色相判定 (0.0 ~ 6.0)
    float h = 0.0f;
    if (maxVal == r) {
        h = (g - b) / delta;
        if (h < 0.0f) h += 6.0f;
    } else if (maxVal == g) {
        h = 2.0f + (b - r) / delta;
    } else {
        h = 4.0f + (r - g) / delta;
    }

    // 6色相の重み補間 (0=Red, 1=Yellow, 2=Green, 3=Cyan, 4=Blue, 5=Magenta)
    float weight = 0.0f;
    if (h < 1.0f) {
        weight = (1.0f - h) * reds + h * yellows;
    } else if (h < 2.0f) {
        const float t = h - 1.0f;
        weight = (1.0f - t) * yellows + t * greens;
    } else if (h < 3.0f) {
        const float t = h - 2.0f;
        weight = (1.0f - t) * greens + t * cyans;
    } else if (h < 4.0f) {
        const float t = h - 3.0f;
        weight = (1.0f - t) * cyans + t * blues;
    } else if (h < 5.0f) {
        const float t = h - 4.0f;
        weight = (1.0f - t) * blues + t * magentas;
    } else {
        const float t = h - 5.0f;
        weight = (1.0f - t) * magentas + t * reds;
    }

    const float luma = minVal + delta * weight;
    return std::clamp(luma, 0.0f, 1.0f);
}
} // namespace

class BlackAndWhiteEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float reds_ = 0.40f;
    float yellows_ = 0.60f;
    float greens_ = 0.40f;
    float cyans_ = 0.60f;
    float blues_ = 0.20f;
    float magentas_ = 0.80f;
    QColor tintColor_ = QColor(225, 199, 160);
    float tintAmount_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float rW = reds_;
        const float yW = yellows_;
        const float gW = greens_;
        const float cW = cyans_;
        const float bW = blues_;
        const float mW = magentas_;
        const float tAmt = tintAmount_;
        const float tR = static_cast<float>(tintColor_.redF());
        const float tG = static_cast<float>(tintColor_.greenF());
        const float tB = static_cast<float>(tintColor_.blueF());

        ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
            float* row = pixels + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
            for (int x = 0; x < width; ++x) {
                float* pixel = row + static_cast<size_t>(x) * 4u;
                const float r = pixel[0];
                const float g = pixel[1];
                const float b = pixel[2];

                const float luma = calculateBWLuma(r, g, b, rW, yW, gW, cW, bW, mW);

                if (tAmt > 0.0001f) {
                    const float tintedR = luma * tR;
                    const float tintedG = luma * tG;
                    const float tintedB = luma * tB;
                    pixel[0] = luma + (tintedR - luma) * tAmt;
                    pixel[1] = luma + (tintedG - luma) * tAmt;
                    pixel[2] = luma + (tintedB - luma) * tAmt;
                } else {
                    pixel[0] = luma;
                    pixel[1] = luma;
                    pixel[2] = luma;
                }
            }
        });
    }
};

class BlackAndWhiteEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float reds_ = 0.40f;
    float yellows_ = 0.60f;
    float greens_ = 0.40f;
    float cyans_ = 0.60f;
    float blues_ = 0.20f;
    float magentas_ = 0.80f;
    QColor tintColor_ = QColor(225, 199, 160);
    float tintAmount_ = 0.0f;

    mutable Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    mutable Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    mutable Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTex_;
    mutable bool pipelineReady_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto previousDevice = device_;
        const auto previousContext = context_;
        SharedRenderDeviceLease lease;
        if (!lease.acquire(device_, context_)) {
            applyCPU(src, dst);
            return;
        }
        if (previousDevice != device_ || previousContext != context_) {
            executor_.reset();
            gpuContext_.reset();
            paramsCB_.Release();
            outputTex_.Release();
            stagingTex_.Release();
            pipelineReady_ = false;
        }

        if (!gpuContext_) {
            gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device_, context_);
            executor_ = std::make_unique<ArtifactCore::ComputeExecutor>(*gpuContext_);
        }
        if (!executor_) {
            applyCPU(src, dst);
            return;
        }

        if (!paramsCB_) {
            Diligent::BufferDesc cbDesc;
            cbDesc.Name = "BlackAndWhite/ParamsCB";
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
            {Diligent::SHADER_TYPE_COMPUTE, "BWParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_InputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "g_OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "BlackAndWhite/PSO";
            desc.shaderSource = kBlackAndWhiteHlsl;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("BWParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }

        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "BlackAndWhite/InputTexture")) {
            applyCPU(src, dst);
            return;
        }

        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "BlackAndWhite/OutputTexture";
        if (!outputTex_ || outputTex_->GetDesc().Width != outDesc.Width ||
            outputTex_->GetDesc().Height != outDesc.Height ||
            outputTex_->GetDesc().Format != outDesc.Format ||
            outputTex_->GetDesc().BindFlags != outDesc.BindFlags) {
            outputTex_.Release();
            device_->CreateTexture(outDesc, nullptr, &outputTex_);
        }
        if (!outputTex_) {
            applyCPU(src, dst);
            return;
        }

        void* mapped = nullptr;
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        ParamsCB params{};
        params.weights0 = {reds_, yellows_, greens_, cyans_};
        params.weights1 = {blues_, magentas_, tintAmount_, 0.0f};
        params.tintColor = {
            static_cast<float>(tintColor_.redF()),
            static_cast<float>(tintColor_.greenF()),
            static_cast<float>(tintColor_.blueF()),
            0.0f
        };
        std::memcpy(mapped, &params, sizeof(params));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);

        if (!executor_->setTextureView("g_InputTexture", inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("g_OutputTexture", outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }

        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(outDesc.Width, outDesc.Height, 1, 8, 8, 1);
        executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (!readbackTexture(device_, context_, outputTex_, stagingTex_, dst, src.image().colorDescriptor(), "BlackAndWhite/StagingTexture")) {
            applyCPU(src, dst);
            return;
        }
        dst.image().setColorDescriptor(src.image().colorDescriptor());
    }

private:
    struct ParamsCB {
        float4 weights0; // r, y, g, c
        float4 weights1; // b, m, tintAmount, pad
        float4 tintColor;
    };

    static constexpr const char* kBlackAndWhiteHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer BWParams : register(b0) {
    float4 g_Weights0; // reds, yellows, greens, cyans
    float4 g_Weights1; // blues, magentas, tintAmount, pad
    float4 g_TintColor;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = g_InputTexture[dtid.xy];
    float maxVal = max(c.r, max(c.g, c.b));
    float minVal = min(c.r, min(c.g, c.b));
    float delta = maxVal - minVal;

    float luma = maxVal;
    if (delta > 0.00001f) {
        float h = 0.0f;
        if (maxVal == c.r) {
            h = (c.g - c.b) / delta;
            if (h < 0.0f) h += 6.0f;
        } else if (maxVal == c.g) {
            h = 2.0f + (c.b - c.r) / delta;
        } else {
            h = 4.0f + (c.r - c.g) / delta;
        }

        float rW = g_Weights0.x;
        float yW = g_Weights0.y;
        float gW = g_Weights0.z;
        float cW = g_Weights0.w;
        float bW = g_Weights1.x;
        float mW = g_Weights1.y;

        float weight = 0.0f;
        if (h < 1.0f) weight = lerp(rW, yW, h);
        else if (h < 2.0f) weight = lerp(yW, gW, h - 1.0f);
        else if (h < 3.0f) weight = lerp(gW, cW, h - 2.0f);
        else if (h < 4.0f) weight = lerp(cW, bW, h - 3.0f);
        else if (h < 5.0f) weight = lerp(bW, mW, h - 4.0f);
        else weight = lerp(mW, rW, h - 5.0f);

        luma = saturate(minVal + delta * weight);
    }

    float tAmt = g_Weights1.z;
    if (tAmt > 0.0001f) {
        c.rgb = lerp(float3(luma, luma, luma), luma * g_TintColor.rgb, tAmt);
    } else {
        c.rgb = float3(luma, luma, luma);
    }
    g_OutputTexture[dtid.xy] = c;
}
)";

    BlackAndWhiteEffectCPUImpl cpuImpl_;

    static bool createTextureFromImage(const ImageF32x4RGBAWithCache& img,
                                      Diligent::IRenderDevice* device,
                                      Diligent::ITexture** outTex,
                                      const char* name)
    {
        if (!device || !outTex) return false;
        const auto& image = img.image();
        const int width = image.width();
        const int height = image.height();
        if (width <= 0 || height <= 0) return false;
        const float* data = image.rgba32fData();
        if (!data) return false;

        Diligent::TextureDesc desc;
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(width);
        desc.Height = static_cast<Diligent::Uint32>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.SampleCount = 1;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Name = name;

        Diligent::TextureSubResData subres;
        subres.pData = data;
        subres.Stride = static_cast<Diligent::Uint32>(width * 4 * sizeof(float));
        Diligent::TextureData initData;
        initData.pSubResources = &subres;
        initData.NumSubresources = 1;

        device->CreateTexture(desc, &initData, outTex);
        return (*outTex != nullptr);
    }

    static bool readbackTexture(Diligent::IRenderDevice* device,
                               Diligent::IDeviceContext* ctx,
                               Diligent::ITexture* src,
                               Diligent::RefCntAutoPtr<Diligent::ITexture>& staging,
                               ImageF32x4RGBAWithCache& dst,
                               const ArtifactCore::ImageColorDescriptor& colorDescriptor,
                               const char* name)
    {
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
        if (!staging || staging->GetDesc().Width != stagingDesc.Width ||
            staging->GetDesc().Height != stagingDesc.Height ||
            staging->GetDesc().Format != stagingDesc.Format) {
            staging.Release();
            device->CreateTexture(stagingDesc, nullptr, &staging);
        }
        if (!staging) return false;

        Diligent::CopyTextureAttribs copy(src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                          staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->CopyTexture(copy);
        Diligent::MappedTextureSubresource mapped{};
        ctx->Flush();
        ctx->WaitForIdle();
        ctx->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mapped);
        if (!mapped.pData || mapped.Stride == 0) return false;

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
};

BlackAndWhiteEffect::BlackAndWhiteEffect() {
    setEffectID(UniString("effect.colorcorrection.blackandwhite"));
    setDisplayName(UniString("Black & White"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<BlackAndWhiteEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<BlackAndWhiteEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

BlackAndWhiteEffect::~BlackAndWhiteEffect() = default;

void BlackAndWhiteEffect::syncImpls() {
    if (auto cpu = ArtifactCore::dynamicPointerCast<BlackAndWhiteEffectCPUImpl>(cpuImpl())) {
        cpu->reds_ = reds_;
        cpu->yellows_ = yellows_;
        cpu->greens_ = greens_;
        cpu->cyans_ = cyans_;
        cpu->blues_ = blues_;
        cpu->magentas_ = magentas_;
        cpu->tintColor_ = tintColor_;
        cpu->tintAmount_ = tintAmount_;
    }
    if (auto gpu = ArtifactCore::dynamicPointerCast<BlackAndWhiteEffectGPUImpl>(gpuImpl())) {
        gpu->reds_ = reds_;
        gpu->yellows_ = yellows_;
        gpu->greens_ = greens_;
        gpu->cyans_ = cyans_;
        gpu->blues_ = blues_;
        gpu->magentas_ = magentas_;
        gpu->tintColor_ = tintColor_;
        gpu->tintAmount_ = tintAmount_;
    }
}

std::vector<AbstractProperty> BlackAndWhiteEffect::getProperties() const {
    std::vector<AbstractProperty> props(8);

    props[0].setName("Reds");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(reds_)));

    props[1].setName("Yellows");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(yellows_)));

    props[2].setName("Greens");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(greens_)));

    props[3].setName("Cyans");
    props[3].setType(ArtifactCore::PropertyType::Float);
    props[3].setValue(QVariant(static_cast<double>(cyans_)));

    props[4].setName("Blues");
    props[4].setType(ArtifactCore::PropertyType::Float);
    props[4].setValue(QVariant(static_cast<double>(blues_)));

    props[5].setName("Magentas");
    props[5].setType(ArtifactCore::PropertyType::Float);
    props[5].setValue(QVariant(static_cast<double>(magentas_)));

    props[6].setName("Tint Color");
    props[6].setType(ArtifactCore::PropertyType::Color);
    props[6].setValue(QVariant(tintColor_));

    props[7].setName("Tint Amount");
    props[7].setType(ArtifactCore::PropertyType::Float);
    props[7].setValue(QVariant(static_cast<double>(tintAmount_)));

    return props;
}

void BlackAndWhiteEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Reds") setReds(value.toFloat());
    else if (name == "Yellows") setYellows(value.toFloat());
    else if (name == "Greens") setGreens(value.toFloat());
    else if (name == "Cyans") setCyans(value.toFloat());
    else if (name == "Blues") setBlues(value.toFloat());
    else if (name == "Magentas") setMagentas(value.toFloat());
    else if (name == "Tint Color") setTintColor(value.value<QColor>());
    else if (name == "Tint Amount") setTintAmount(value.toFloat());
}

} // namespace Artifact
