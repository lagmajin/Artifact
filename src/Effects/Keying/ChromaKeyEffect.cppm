module;
#include <cmath>
#include <cstring>
#include <QVariant>
#include <QColor>

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
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
module Artifact.Effect.Keying.ChromaKey;




import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing;
import Artifact.Effect.Abstract;
import FloatRGBA;
import Utils.String.UniString;
import Property.Abstract;
import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentDeviceManager;
import Memory.SharedPtr;

// Global includes for Qt types used in this implementation

namespace Artifact {
 using namespace ArtifactCore;

void ChromaKeyEffectCPUImpl::applyCPU(const ArtifactCore::ImageF32x4RGBAWithCache& src, ArtifactCore::ImageF32x4RGBAWithCache& dst) {
    const ArtifactCore::ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData || srcImage.width() <= 0 || srcImage.height() <= 0) {
        dst = src;
        return;
    }
    ArtifactCore::ImageF32x4_RGBA result = srcImage;
    float* dstData = result.rgba32fData();
    if (!dstData) {
        dst = src;
        return;
    }
    ArtifactCore::Keying::ChromaKeyParams params;
    params.keyColor.x = keyColor_.r();
    params.keyColor.y = keyColor_.g();
    params.keyColor.z = keyColor_.b();
    params.keyColor.w = 1.0f;
    params.hueTolerance = hueTolerance_;
    params.edgeSoftness = edgeSoftness_;
    params.clipBlack = std::min(clipBlack_, clipWhite_ - 0.0001f);
    params.clipWhite = std::max(clipWhite_, clipBlack_ + 0.0001f);
    params.despillStrength = despillStrength_;
    params.despillMode = despillMode_;
    params.choke = choke_;
    params.matteBlur = matteBlur_;
    params.viewMode = viewMode_;
    const ArtifactCore::Keying::ChromaKeyBuffers buffers{
        srcData, dstData, srcImage.width(), srcImage.height()};
    if (!ArtifactCore::Keying::processChromaKey(buffers, params)) {
        dst = src;
        return;
    }
    dst = ImageF32x4RGBAWithCache(result);
}

class ChromaKeyEffectGPUImpl final : public ArtifactEffectImplBase {
public:
    ArtifactCore::Keying::ChromaKeyParams params_{};

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData || srcImage.width() <= 0 || srcImage.height() <= 0) {
            dst = src;
            return;
        }
        ArtifactCore::ImageF32x4_RGBA result = srcImage;
        float* dstData = result.rgba32fData();
        if (!dstData) {
            dst = src;
            return;
        }
        const ArtifactCore::Keying::ChromaKeyBuffers buffers{
            srcData, dstData, srcImage.width(), srcImage.height()};
        if (!ArtifactCore::Keying::processChromaKey(buffers, params_)) {
            dst = src;
            return;
        }
        dst = ImageF32x4RGBAWithCache(result);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
            applyCPU(src, dst);
            return;
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
            cbDesc.Name = "ChromaKey/ParamsCB";
            cbDesc.Size = sizeof(ArtifactCore::Keying::ChromaKeyGpuParams);
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
            {Diligent::SHADER_TYPE_COMPUTE, "ChromaParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "ForegroundTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {Diligent::SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        if (!pipelineReady_) {
            ArtifactCore::ComputePipelineDesc desc;
            desc.name = "ChromaKey/PSO";
            desc.shaderSource = kShader;
            desc.entryPoint = "main";
            desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            desc.variables = vars;
            desc.variableCount = 3;
            desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
                !executor_->setBuffer("ChromaParams", paramsCB_)) {
                applyCPU(src, dst);
                return;
            }
            pipelineReady_ = true;
        }
        Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
        if (!createTextureFromImage(src, device_, &inputTex, "ChromaKey/InputTexture")) {
            applyCPU(src, dst);
            return;
        }
        Diligent::TextureDesc outDesc = inputTex->GetDesc();
        outDesc.Usage = Diligent::USAGE_DEFAULT;
        outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
        outDesc.Name = "ChromaKey/OutputTexture";
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
        context_->MapBuffer(paramsCB_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            applyCPU(src, dst);
            return;
        }
        const auto gpuParams = ArtifactCore::Keying::ChromaKeyGpuParams::fromParams(params_);
        std::memcpy(mapped, &gpuParams, sizeof(gpuParams));
        context_->UnmapBuffer(paramsCB_, Diligent::MAP_WRITE);
        if (!executor_->setTextureView("ForegroundTexture",
                inputTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE)) ||
            !executor_->setTextureView("OutputTexture",
                outputTex_->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS))) {
            applyCPU(src, dst);
            return;
        }
        auto attribs = ArtifactCore::ComputeExecutor::makeDispatchAttribs(
            outDesc.Width, outDesc.Height, 1, 16, 16, 1);
        executor_->dispatch(context_, attribs, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (!readbackTexture(device_, context_, outputTex_, dst, "ChromaKey/StagingTexture",
                             src.image().colorDescriptor())) {
            applyCPU(src, dst);
        }
    }

private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsCB_;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTex_;
    std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
    std::unique_ptr<ArtifactCore::ComputeExecutor> executor_;
    bool pipelineReady_ = false;

    static constexpr const char* kShader = R"(
Texture2D<float4> ForegroundTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer ChromaParams : register(b0) {
  float4 KeyColor;
  float HueTolerance; float EdgeSoftness; float ClipBlack; float ClipWhite;
  float DespillStrength; int DespillMode; int ViewMode; float Choke;
  float MatteBlur; float3 Pad;
};
float3 rgbToYCbCr(float3 c) {
  float y = dot(c, float3(0.299, 0.587, 0.114));
  return float3(y, (c.b - y) * 0.564, (c.r - y) * 0.713);
}
float baseAlpha(float3 rgb) {
  float3 pix = rgbToYCbCr(max(rgb, 0));
  float3 key = rgbToYCbCr(max(KeyColor.rgb, 0));
  float chroma = length(pix.yz - key.yz);
  float luma = abs(pix.x - key.x);
  float d = sqrt(chroma * chroma + (luma * 0.35) * (luma * 0.35));
  float a = smoothstep(HueTolerance, HueTolerance + max(EdgeSoftness, 1e-4), d);
  float range = max(ClipWhite - ClipBlack, 1e-4);
  return saturate((a - ClipBlack) / range);
}
float3 despillColor(float3 rgb, float alpha) {
  if (DespillMode <= 0 || DespillStrength <= 0 || alpha >= 0.999) return max(rgb, 0);
  float spill = (1.0 - alpha) * saturate(DespillStrength);
  if (DespillMode == 1) {
    if (KeyColor.g >= KeyColor.r && KeyColor.g >= KeyColor.b)
      rgb.g -= max(0, rgb.g - max(rgb.r, rgb.b)) * spill;
    else if (KeyColor.r >= KeyColor.g && KeyColor.r >= KeyColor.b)
      rgb.r -= max(0, rgb.r - max(rgb.g, rgb.b)) * spill;
    else
      rgb.b -= max(0, rgb.b - max(rgb.r, rgb.g)) * spill;
    return max(rgb, 0);
  }
  float y = dot(max(rgb, 0), float3(0.299, 0.587, 0.114));
  return max(lerp(rgb, float3(y, y, y), spill), 0);
}
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
  uint w, h; OutputTexture.GetDimensions(w, h);
  if (id.x >= w || id.y >= h) return;
  float4 fg = ForegroundTexture[id.xy];
  float alpha = baseAlpha(fg.rgb);
  int radius = (int)(abs(Choke) * 3.0 + 0.5);
  radius = clamp(radius, 0, 3);
  if (radius > 0) {
    float v = (Choke > 0) ? 1.0 : 0.0;
    for (int dy = -3; dy <= 3; ++dy)
      for (int dx = -3; dx <= 3; ++dx) {
        if (max(abs(dx), abs(dy)) > radius) continue;
        int2 q = clamp(int2(id.xy) + int2(dx, dy), int2(0, 0), int2(w, h) - 1);
        float s = baseAlpha(ForegroundTexture[uint2(q)].rgb);
        v = (Choke > 0) ? min(v, s) : max(v, s);
      }
    alpha = v;
  }
  if (MatteBlur > 1e-4) {
    float sum = 0;
    // 3x3 mean of the (possibly choked) matte re-evaluated per tap.
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int2 q = clamp(int2(id.xy) + int2(dx, dy), int2(0, 0), int2(w, h) - 1);
        float s = baseAlpha(ForegroundTexture[uint2(q)].rgb);
        if (radius > 0) {
          float v = (Choke > 0) ? 1.0 : 0.0;
          for (int ey = -3; ey <= 3; ++ey)
            for (int ex = -3; ex <= 3; ++ex) {
              if (max(abs(ex), abs(ey)) > radius) continue;
              int2 e = clamp(int2(q) + int2(ex, ey), int2(0, 0), int2(w, h) - 1);
              float t = baseAlpha(ForegroundTexture[uint2(e)].rgb);
              v = (Choke > 0) ? min(v, t) : max(v, t);
            }
          s = v;
        }
        sum += s;
      }
    alpha = lerp(alpha, sum / 9.0, saturate(MatteBlur * 0.5));
  }
  float3 color = despillColor(fg.rgb, alpha);
  float outA = saturate(alpha * fg.a);
  if (ViewMode == 1) { OutputTexture[id.xy] = float4(alpha, alpha, alpha, 1.0); return; }
  if (ViewMode == 2) { OutputTexture[id.xy] = float4(color, 1.0); return; }
  if (ViewMode == 3) { OutputTexture[id.xy] = float4(1.0 - alpha, alpha, 0.15 * (1.0 - abs(alpha * 2.0 - 1.0)), 1.0); return; }
  OutputTexture[id.xy] = float4(color, outA);
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

// Properties - single definitions placed after implementation
std::vector<ArtifactCore::AbstractProperty> ChromaKeyEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(10);

    auto& keyColorProp = props.emplace_back();
    keyColorProp.setName("keyColor");
    keyColorProp.setDisplayLabel(QStringLiteral("Key Color"));
    keyColorProp.setType(ArtifactCore::PropertyType::Color);
    keyColorProp.setValue(QColor::fromRgbF(keyColor().r(), keyColor().g(), keyColor().b(), keyColor().a()));
    keyColorProp.setDefaultValue(QColor::fromRgbF(0.0, 1.0, 0.0, 1.0));
    keyColorProp.setTooltip(QStringLiteral("Screen color to remove."));

    const auto addFloat = [&](const char* name, const char* label, double value,
                              double softMin, double softMax, double hardMin, double hardMax,
                              double defaultValue, const char* tip) {
        auto& p = props.emplace_back();
        p.setName(name);
        p.setDisplayLabel(QString::fromUtf8(label));
        p.setType(ArtifactCore::PropertyType::Float);
        p.setSoftRange(softMin, softMax);
        p.setHardRange(hardMin, hardMax);
        p.setMinValue(QVariant(hardMin));
        p.setMaxValue(QVariant(hardMax));
        p.setDefaultValue(QVariant(defaultValue));
        p.setValue(QVariant(value));
        p.setStep(0.01);
        p.setTooltip(QString::fromUtf8(tip));
    };
    addFloat("hueTolerance", "Screen Tolerance", hueTolerance(), 0.0, 1.0, 0.0, 1.0, 0.28,
             "YCbCr chroma distance tolerance for the screen.");
    addFloat("edgeSoftness", "Edge Softness", edgeSoftness(), 0.0001, 1.0, 0.0001, 1.0, 0.12,
             "Softens the transition at the keyed edge.");
    addFloat("clipBlack", "Matte Clip Black", clipBlack(), 0.0, 1.0, 0.0, 1.0, 0.0,
             "Raises the matte floor to remove weak residual screen coverage.");
    addFloat("clipWhite", "Matte Clip White", clipWhite(), 0.0, 1.0, 0.0, 1.0, 1.0,
             "Lowers the matte ceiling to force clean opaque foreground.");
    addFloat("despillStrength", "Despill Strength", despillStrength(), 0.0, 1.0, 0.0, 1.0, 0.7,
             "Suppresses the screen color in retained pixels.");
    addFloat("choke", "Matte Choke", choke(), -1.0, 1.0, -1.0, 1.0, 0.0,
             "Erodes (+) or dilates (-) the matte, about 3px at full range.");
    addFloat("matteBlur", "Matte Blur", matteBlur(), 0.0, 2.0, 0.0, 2.0, 0.0,
             "Softens the finished matte with a 3x3 blend.");

    auto& despillModeProp = props.emplace_back();
    despillModeProp.setName("despillMode");
    despillModeProp.setDisplayLabel(QStringLiteral("Despill Mode"));
    despillModeProp.setType(ArtifactCore::PropertyType::Integer);
    despillModeProp.setSoftRange(0, 2);
    despillModeProp.setHardRange(0, 2);
    despillModeProp.setMinValue(QVariant(0));
    despillModeProp.setMaxValue(QVariant(2));
    despillModeProp.setDefaultValue(QVariant(1));
    despillModeProp.setValue(QVariant(despillMode()));
    despillModeProp.setTooltip(QStringLiteral("0=Off, 1=Channel Suppress, 2=Luminance Preserve."));

    auto& viewProp = props.emplace_back();
    viewProp.setName("viewMode");
    viewProp.setDisplayLabel(QStringLiteral("Matte View"));
    viewProp.setType(ArtifactCore::PropertyType::Integer);
    viewProp.setSoftRange(0, 3);
    viewProp.setHardRange(0, 3);
    viewProp.setMinValue(QVariant(0));
    viewProp.setMaxValue(QVariant(3));
    viewProp.setDefaultValue(QVariant(0));
    viewProp.setValue(QVariant(viewMode()));
    viewProp.setTooltip(QStringLiteral("0=Final, 1=Screen Matte, 2=Despill Map, 3=Status."));

    return props;
}

void ChromaKeyEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    QString n = name.toQString();
    const auto safeFloat = [&value](float fallback, float minimum, float maximum) {
        const float raw = static_cast<float>(value.toDouble());
        return std::isfinite(raw) ? std::clamp(raw, minimum, maximum) : fallback;
    };
    if (n == "hueTolerance") {
        setHueTolerance(safeFloat(0.28f, 0.0f, 1.0f));
    } else if (n == "edgeSoftness") {
        setEdgeSoftness(safeFloat(0.12f, 0.0001f, 1.0f));
    } else if (n == "clipBlack") {
        setClipBlack(safeFloat(0.0f, 0.0f, 1.0f));
    } else if (n == "clipWhite") {
        setClipWhite(safeFloat(1.0f, 0.0f, 1.0f));
    } else if (n == "despillStrength") {
        setDespillStrength(safeFloat(0.7f, 0.0f, 1.0f));
    } else if (n == "despillMode") {
        setDespillMode(value.isValid() ? std::clamp(value.toInt(), 0, 2) : 1);
    } else if (n == "choke") {
        setChoke(safeFloat(0.0f, -1.0f, 1.0f));
    } else if (n == "matteBlur") {
        setMatteBlur(safeFloat(0.0f, 0.0f, 2.0f));
    } else if (n == "viewMode") {
        setViewMode(value.isValid() ? std::clamp(value.toInt(), 0, 3) : 0);
    } else if (n == "keyColor") {
        if (value.canConvert<QColor>()) {
            QColor c = value.value<QColor>();
            if (!c.isValid()) return;
            setKeyColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        }
    // Legacy migration: approximate mapping so old projects still load.
    } else if (n == "similarity") {
        setHueTolerance(std::clamp(safeFloat(0.4f, 0.0f, 1.7320508f) * 0.35f, 0.0f, 1.0f));
    } else if (n == "smoothness") {
        setEdgeSoftness(safeFloat(0.1f, 0.0001f, 1.7320508f));
    } else if (n == "spillReduction") {
        setDespillStrength(safeFloat(0.5f, 0.0f, 1.0f));
        if (despillMode() == 0 && despillStrength() > 0.0f) setDespillMode(1);
    } else if (n == "spillDesaturation") {
        const float v = safeFloat(1.0f, 0.0f, 1.0f);
        setDespillMode(v >= 0.5f ? 2 : 1);
    } else if (n == "blackClip") {
        setClipBlack(safeFloat(0.0f, 0.0f, 1.0f));
    } else if (n == "whiteClip") {
        setClipWhite(safeFloat(1.0f, 0.0f, 1.0f));
    } else if (n == "previewMatte") {
        setViewMode(value.toBool() ? 1 : 0);
    } else if (n == "lumaOnly") {
        // Dropped: the YCbCr metric already weights luma. Kept as no-op.
        return;
    } else {
        setCommonPropertyValue(n, value);
    }
}

void ChromaKeyEffect::syncGpuImpl() {
    auto* gpu = dynamic_cast<ChromaKeyEffectGPUImpl*>(gpuImpl().get());
    if (!gpu) return;
    gpu->params_.keyColor.x = typedCpuImpl_->keyColor().r();
    gpu->params_.keyColor.y = typedCpuImpl_->keyColor().g();
    gpu->params_.keyColor.z = typedCpuImpl_->keyColor().b();
    gpu->params_.keyColor.w = 1.0f;
    gpu->params_.hueTolerance = typedCpuImpl_->hueTolerance();
    gpu->params_.edgeSoftness = typedCpuImpl_->edgeSoftness();
    gpu->params_.clipBlack = typedCpuImpl_->clipBlack();
    gpu->params_.clipWhite = typedCpuImpl_->clipWhite();
    gpu->params_.despillStrength = typedCpuImpl_->despillStrength();
    gpu->params_.despillMode = typedCpuImpl_->despillMode();
    gpu->params_.choke = typedCpuImpl_->choke();
    gpu->params_.matteBlur = typedCpuImpl_->matteBlur();
    gpu->params_.viewMode = typedCpuImpl_->viewMode();
}

ChromaKeyEffect::ChromaKeyEffect() : ArtifactAbstractEffect() {
    typedCpuImpl_ = ArtifactCore::makeShared<ChromaKeyEffectCPUImpl>();
    setCPUImpl(typedCpuImpl_);
    auto gpu = ArtifactCore::makeShared<ChromaKeyEffectGPUImpl>();
    setGPUImpl(gpu);
    syncGpuImpl();
    setDisplayName("Chroma Key");
    setEffectID("Effect.Keying.ChromaKey");
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setComputeMode(ComputeMode::AUTO);
}

void ChromaKeyEffect::setKeyColor(const FloatRGBA& color) {
    typedCpuImpl_->setKeyColor(color);
    syncGpuImpl();
}
const FloatRGBA& ChromaKeyEffect::keyColor() const {
    return typedCpuImpl_->keyColor();
}

void ChromaKeyEffect::setHueTolerance(float val) {
    typedCpuImpl_->setHueTolerance(val);
    syncGpuImpl();
}
float ChromaKeyEffect::hueTolerance() const {
    return typedCpuImpl_->hueTolerance();
}

void ChromaKeyEffect::setEdgeSoftness(float val) {
    typedCpuImpl_->setEdgeSoftness(val);
    syncGpuImpl();
}
float ChromaKeyEffect::edgeSoftness() const {
    return typedCpuImpl_->edgeSoftness();
}

void ChromaKeyEffect::setClipBlack(float val) {
    typedCpuImpl_->setClipBlack(std::min(val, typedCpuImpl_->clipWhite() - 0.0001f));
    syncGpuImpl();
}
float ChromaKeyEffect::clipBlack() const { return typedCpuImpl_->clipBlack(); }

void ChromaKeyEffect::setClipWhite(float val) {
    typedCpuImpl_->setClipWhite(std::max(val, typedCpuImpl_->clipBlack() + 0.0001f));
    syncGpuImpl();
}
float ChromaKeyEffect::clipWhite() const { return typedCpuImpl_->clipWhite(); }

void ChromaKeyEffect::setDespillStrength(float val) {
    typedCpuImpl_->setDespillStrength(val);
    syncGpuImpl();
}
float ChromaKeyEffect::despillStrength() const { return typedCpuImpl_->despillStrength(); }

void ChromaKeyEffect::setDespillMode(int mode) {
    typedCpuImpl_->setDespillMode(mode);
    syncGpuImpl();
}
int ChromaKeyEffect::despillMode() const { return typedCpuImpl_->despillMode(); }

void ChromaKeyEffect::setChoke(float val) {
    typedCpuImpl_->setChoke(val);
    syncGpuImpl();
}
float ChromaKeyEffect::choke() const { return typedCpuImpl_->choke(); }

void ChromaKeyEffect::setMatteBlur(float val) {
    typedCpuImpl_->setMatteBlur(val);
    syncGpuImpl();
}
float ChromaKeyEffect::matteBlur() const { return typedCpuImpl_->matteBlur(); }

void ChromaKeyEffect::setViewMode(int mode) {
    typedCpuImpl_->setViewMode(mode);
    syncGpuImpl();
}
int ChromaKeyEffect::viewMode() const { return typedCpuImpl_->viewMode(); }

}
