module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Artifact.Effect.Keying.LumaKey;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
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

void LumaKeyEffectCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src,
                                    ImageF32x4RGBAWithCache& dst) {
  const auto& image = src.image();
  const float* data = image.rgba32fData();
  if (!data || image.width() <= 0 || image.height() <= 0) { dst = src; return; }
  ImageF32x4_RGBA result = image;
  float* out = result.rgba32fData();
  if (!out) { dst = src; return; }
  Keying::LumaKeyParams params;
  params.low = lowThreshold_;
  params.high = highThreshold_;
  params.softness = softness_;
  params.choke = choke_;
  params.matteBlur = matteBlur_;
  params.viewMode = viewMode_;
  const Keying::LumaKeyBuffers buffers{data, out, image.width(), image.height()};
  if (!Keying::processLumaKey(buffers, params)) { dst = src; return; }
  dst = ImageF32x4RGBAWithCache(result);
}

class LumaKeyEffectGPUImpl final : public ArtifactEffectImplBase {
public:
  Keying::LumaKeyParams params_{};

  void applyCPU(const ImageF32x4RGBAWithCache& src,
                ImageF32x4RGBAWithCache& dst) override {
    const auto& image = src.image();
    const float* data = image.rgba32fData();
    if (!data || image.width() <= 0 || image.height() <= 0) { dst = src; return; }
    ImageF32x4_RGBA result = image;
    float* out = result.rgba32fData();
    if (!out) { dst = src; return; }
    const Keying::LumaKeyBuffers buffers{data, out, image.width(), image.height()};
    if (!Keying::processLumaKey(buffers, params_)) { dst = src; return; }
    dst = ImageF32x4RGBAWithCache(result);
  }

  void applyGPU(const ImageF32x4RGBAWithCache& src,
                ImageF32x4RGBAWithCache& dst) override {
    // Edge finishing (choke/blur) is a CPU morphology pass; the GPU path
    // covers the range matte and falls back otherwise.
    if (std::abs(params_.choke) > 1.0e-4f || params_.matteBlur > 1.0e-4f) {
      applyCPU(src, dst);
      return;
    }
    if (!acquireSharedRenderDeviceForCurrentBackend(device_, context_)) {
      applyCPU(src, dst);
      return;
    }
    const auto& srcImage = src.image();
    if (!srcImage.rgba32fData() || srcImage.width() <= 0 || srcImage.height() <= 0) {
      dst = src;
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
      cbDesc.Name = "LumaKey/ParamsCB";
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
      {Diligent::SHADER_TYPE_COMPUTE, "LumaParams", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_COMPUTE, "ForegroundTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
    };
    if (!pipelineReady_) {
      ArtifactCore::ComputePipelineDesc desc;
      desc.name = "LumaKey/PSO";
      desc.shaderSource = kShader;
      desc.entryPoint = "main";
      desc.sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
      desc.variables = vars;
      desc.variableCount = 3;
      desc.defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
      if (!executor_->build(desc) || !executor_->createShaderResourceBinding(true) ||
          !executor_->setBuffer("LumaParams", paramsCB_)) {
        applyCPU(src, dst);
        return;
      }
      pipelineReady_ = true;
    }
    Diligent::RefCntAutoPtr<Diligent::ITexture> inputTex;
    if (!createFloatTexture(srcImage.rgba32fData(), srcImage.width(), srcImage.height(),
                            device_, &inputTex, "LumaKey/Input")) {
      applyCPU(src, dst);
      return;
    }
    Diligent::TextureDesc outDesc = inputTex->GetDesc();
    outDesc.Usage = Diligent::USAGE_DEFAULT;
    outDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
    outDesc.Name = "LumaKey/Output";
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
    ParamsCB cb{};
    cb.low = std::min(params_.low, params_.high);
    cb.high = std::max(params_.low, params_.high);
    cb.softness = std::max(params_.softness, 1.0e-4f);
    cb.viewMode = params_.viewMode;
    std::memcpy(mapped, &cb, sizeof(cb));
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
    executor_->dispatch(context_, attribs,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!readbackTexture(device_, context_, outputTex_, dst,
                         "LumaKey/Staging", srcImage.colorDescriptor())) {
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

  struct ParamsCB {
    float low = 0.15f;
    float high = 0.85f;
    float softness = 0.08f;
    int viewMode = 0;
  };

  static constexpr const char* kShader = R"(
Texture2D<float4> ForegroundTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);
cbuffer LumaParams : register(b0) { float Low; float High; float Softness; int ViewMode; };
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
  uint w, h; OutputTexture.GetDimensions(w, h);
  if (id.x >= w || id.y >= h) return;
  float4 fg = ForegroundTexture[id.xy];
  float luma = dot(fg.rgb, float3(0.2126, 0.7152, 0.0722));
  float alpha = saturate(min((luma - Low) / max(Softness, 1e-4),
                             (High - luma) / max(Softness, 1e-4)));
  if (ViewMode == 1) { OutputTexture[id.xy] = float4(alpha, alpha, alpha, 1.0); return; }
  OutputTexture[id.xy] = float4(fg.rgb, saturate(fg.a * alpha));
})";

  static bool createFloatTexture(const float* data, int width, int height,
                                 Diligent::IRenderDevice* device,
                                 Diligent::ITexture** outTex, const char* name) {
    if (!data || width <= 0 || height <= 0 || !device || !outTex) return false;
    Diligent::TextureDesc desc;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = width;
    desc.Height = height;
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleCount = 1;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Name = name;
    Diligent::TextureSubResData sub{};
    sub.pData = data;
    sub.Stride = static_cast<Diligent::Uint64>(width) * sizeof(float) * 4ull;
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

void LumaKeyEffect::syncGpuImpl() {
  auto* gpu = dynamic_cast<LumaKeyEffectGPUImpl*>(gpuImpl().get());
  if (!gpu) return;
  gpu->params_.low = typedCpuImpl_->lowThreshold();
  gpu->params_.high = typedCpuImpl_->highThreshold();
  gpu->params_.softness = typedCpuImpl_->softness();
  gpu->params_.choke = typedCpuImpl_->choke();
  gpu->params_.matteBlur = typedCpuImpl_->matteBlur();
  gpu->params_.viewMode = typedCpuImpl_->viewMode();
}

LumaKeyEffect::LumaKeyEffect() : ArtifactAbstractEffect() {
  typedCpuImpl_ = makeShared<LumaKeyEffectCPUImpl>();
  setCPUImpl(typedCpuImpl_);
  setGPUImpl(makeShared<LumaKeyEffectGPUImpl>());
  syncGpuImpl();
  setDisplayName("Luma Key");
  setEffectID("Effect.Keying.LumaKey");
  setPipelineStage(EffectPipelineStage::Rasterizer);
  setComputeMode(ComputeMode::AUTO);
}

std::vector<AbstractProperty> LumaKeyEffect::getProperties() const {
  std::vector<AbstractProperty> properties;
  for (const auto& item : {std::pair{"lowThreshold", typedCpuImpl_->lowThreshold()},
                           std::pair{"highThreshold", typedCpuImpl_->highThreshold()},
                           std::pair{"softness", typedCpuImpl_->softness()},
                           std::pair{"choke", typedCpuImpl_->choke()},
                           std::pair{"matteBlur", typedCpuImpl_->matteBlur()}}) {
    auto& property = properties.emplace_back();
    const QString itemName = QString::fromLatin1(item.first);
    property.setName(item.first);
    property.setType(PropertyType::Float);
    const bool isChoke = itemName == QStringLiteral("choke");
    const bool isBlur = itemName == QStringLiteral("matteBlur");
    const bool isSoftness = itemName == QStringLiteral("softness");
    const double lo = isChoke ? -1.0 : 0.0;
    const double hi = isChoke ? 1.0 : (isBlur ? 2.0 : 1.0);
    property.setSoftRange(lo, hi);
    property.setHardRange(isSoftness ? 0.001 : lo, hi);
    property.setDefaultValue(QVariant(static_cast<double>(item.second)));
    property.setValue(QVariant(static_cast<double>(item.second)));
  }
  auto& view = properties.emplace_back();
  view.setName("viewMode");
  view.setType(PropertyType::Integer);
  view.setSoftRange(0, 1);
  view.setHardRange(0, 1);
  view.setDefaultValue(QVariant(0));
  view.setValue(QVariant(typedCpuImpl_->viewMode()));
  return properties;
}

void LumaKeyEffect::setPropertyValue(const UniString& name, const QVariant& value) {
  const QString property = name.toQString();
  const float raw = static_cast<float>(value.toDouble());
  const float v = std::isfinite(raw) ? raw : 0.0f;
  if (property == "lowThreshold") typedCpuImpl_->setLowThreshold(std::clamp(v, 0.0f, 1.0f));
  else if (property == "highThreshold") typedCpuImpl_->setHighThreshold(std::clamp(v, 0.0f, 1.0f));
  else if (property == "softness") typedCpuImpl_->setSoftness(std::clamp(v, 0.001f, 1.0f));
  else if (property == "choke") typedCpuImpl_->setChoke(std::clamp(v, -1.0f, 1.0f));
  else if (property == "matteBlur") typedCpuImpl_->setMatteBlur(std::clamp(v, 0.0f, 2.0f));
  else if (property == "viewMode") typedCpuImpl_->setViewMode(value.toInt());
  else {
    setCommonPropertyValue(property, value);
    return;
  }
  syncGpuImpl();
}

void LumaKeyEffect::setChoke(float value) {
  typedCpuImpl_->setChoke(value);
  syncGpuImpl();
}
float LumaKeyEffect::choke() const { return typedCpuImpl_->choke(); }

void LumaKeyEffect::setMatteBlur(float value) {
  typedCpuImpl_->setMatteBlur(value);
  syncGpuImpl();
}
float LumaKeyEffect::matteBlur() const { return typedCpuImpl_->matteBlur(); }

void LumaKeyEffect::setViewMode(int mode) {
  typedCpuImpl_->setViewMode(mode);
  syncGpuImpl();
}
int LumaKeyEffect::viewMode() const { return typedCpuImpl_->viewMode(); }

} // namespace Artifact
