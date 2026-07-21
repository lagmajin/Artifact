module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

#include <QDebug>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>

#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

module Artifact.Render.Pipeline;

import std;
import Layer.Blend;
import Artifact.Layer.Abstract;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

import Artifact.Render.Config;

namespace Artifact
{
 using namespace Diligent;
 using ArtifactCore::BlendMode;
 using ArtifactCore::GpuContext;
 using ArtifactCore::LayerBlendPipeline;

 namespace
 {
  inline constexpr const char* kScreenSpaceGlobalIlluminationShader = R"(
cbuffer SSGIParams : register(b0)
{
    uint g_InputWidth;
    uint g_InputHeight;
    uint g_OutputWidth;
    uint g_OutputHeight;
    uint g_RaySteps;
    float g_Intensity;
    float g_DepthThickness;
    float g_RadiusPixels;
};

Texture2D<float> g_Depth : register(t0);
Texture2D<float4> g_Normal : register(t1);
Texture2D<float4> g_Albedo : register(t2);
RWTexture2D<float4> g_Output : register(u0);

static const float2 kDirections[8] = {
    float2(1.0, 0.0), float2(0.7071, 0.7071),
    float2(0.0, 1.0), float2(-0.7071, 0.7071),
    float2(-1.0, 0.0), float2(-0.7071, -0.7071),
    float2(0.0, -1.0), float2(0.7071, -0.7071)
};

[numthreads(8, 8, 1)]
void ScreenSpaceGICS(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_OutputWidth || dispatchId.y >= g_OutputHeight) return;

    const float2 outputUV = (float2(dispatchId.xy) + 0.5) /
                            float2(g_OutputWidth, g_OutputHeight);
    const int2 centerPixel = clamp(
        int2(outputUV * float2(g_InputWidth, g_InputHeight)),
        int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
    const float centerDepth = g_Depth.Load(int3(centerPixel, 0));
    if (centerDepth >= 0.999999) {
        g_Output[dispatchId.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    const float3 centerNormal = normalize(
        g_Normal.Load(int3(centerPixel, 0)).xyz * 2.0 - 1.0);
    float3 indirect = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    const uint stepCount = clamp(g_RaySteps, 1u, 24u);

    [unroll]
    for (uint directionIndex = 0; directionIndex < 8; ++directionIndex) {
        const float2 direction = kDirections[directionIndex];
        [loop]
        for (uint stepIndex = 1; stepIndex <= 24; ++stepIndex) {
            if (stepIndex > stepCount) break;
            const float distanceWeight = 1.0 -
                (float(stepIndex - 1) / max(1.0, float(stepCount)));
            const float radius = g_RadiusPixels *
                (float(stepIndex) / float(stepCount));
            const int2 samplePixel = clamp(
                centerPixel + int2(round(direction * radius)),
                int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
            const float sampleDepth = g_Depth.Load(int3(samplePixel, 0));
            if (sampleDepth >= 0.999999) continue;

            const float depthDelta = abs(sampleDepth - centerDepth);
            const float depthWeight = saturate(
                1.0 - depthDelta / max(g_DepthThickness, 0.000001));
            if (depthWeight <= 0.0) continue;

            const float3 sampleNormal = normalize(
                g_Normal.Load(int3(samplePixel, 0)).xyz * 2.0 - 1.0);
            const float normalWeight = saturate(dot(centerNormal, sampleNormal));
            const float weight = depthWeight * normalWeight * distanceWeight;
            indirect += g_Albedo.Load(int3(samplePixel, 0)).rgb * weight;
            totalWeight += weight;
        }
    }

    indirect = totalWeight > 0.0
        ? indirect / totalWeight
        : float3(0.0, 0.0, 0.0);
    g_Output[dispatchId.xy] = float4(indirect * g_Intensity, 1.0);
}
)";

  inline constexpr const char* kScreenSpaceGlobalIlluminationResolveShader = R"(
cbuffer SSGIResolveParams : register(b0)
{
    uint g_InputWidth;
    uint g_InputHeight;
    uint g_OutputWidth;
    uint g_OutputHeight;
    uint g_HistoryValid;
    uint g_TemporalEnabled;
    uint g_DenoiseEnabled;
    float g_HistoryWeight;
    float g_DepthSigma;
    float g_NormalSigma;
    float g_VelocityScale;
    float _padding0;
};

Texture2D<float4> g_RawGI : register(t0);
Texture2D<float4> g_HistoryGI : register(t1);
Texture2D<float> g_ResolveDepth : register(t2);
Texture2D<float4> g_ResolveNormal : register(t3);
Texture2D<float4> g_ResolveVelocity : register(t4);
RWTexture2D<float4> g_ResolvedGI : register(u0);

[numthreads(8, 8, 1)]
void ScreenSpaceGIResolveCS(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_OutputWidth || dispatchId.y >= g_OutputHeight) return;

    const float2 outputSize = float2(g_OutputWidth, g_OutputHeight);
    const float2 inputSize = float2(g_InputWidth, g_InputHeight);
    const float2 uv = (float2(dispatchId.xy) + 0.5) / outputSize;
    const int2 fullPixel = clamp(int2(uv * inputSize), int2(0, 0),
                                 int2(g_InputWidth - 1, g_InputHeight - 1));
    const float centerDepth = g_ResolveDepth.Load(int3(fullPixel, 0));
    const float3 centerNormal = normalize(
        g_ResolveNormal.Load(int3(fullPixel, 0)).xyz * 2.0 - 1.0);

    float3 filtered = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    const int filterRadius = g_DenoiseEnabled != 0 ? 1 : 0;
    [loop]
    for (int y = -1; y <= 1; ++y) {
        [loop]
        for (int x = -1; x <= 1; ++x) {
            if (abs(x) > filterRadius || abs(y) > filterRadius) continue;
            const int2 sampleOutput = clamp(
                int2(dispatchId.xy) + int2(x, y), int2(0, 0),
                int2(g_OutputWidth - 1, g_OutputHeight - 1));
            const float2 sampleUV =
                (float2(sampleOutput) + 0.5) / outputSize;
            const int2 sampleFull = clamp(int2(sampleUV * inputSize),
                int2(0, 0), int2(g_InputWidth - 1, g_InputHeight - 1));
            const float sampleDepth =
                g_ResolveDepth.Load(int3(sampleFull, 0));
            const float3 sampleNormal = normalize(
                g_ResolveNormal.Load(int3(sampleFull, 0)).xyz * 2.0 - 1.0);
            const float depthWeight = exp(-abs(sampleDepth - centerDepth) /
                                          max(g_DepthSigma, 0.000001));
            const float normalWeight = pow(
                saturate(dot(centerNormal, sampleNormal)), g_NormalSigma);
            const float spatialWeight = (x == 0 && y == 0) ? 1.0 : 0.75;
            const float weight = depthWeight * normalWeight * spatialWeight;
            filtered += g_RawGI.Load(int3(sampleOutput, 0)).rgb * weight;
            totalWeight += weight;
        }
    }
    filtered = totalWeight > 0.0
        ? filtered / totalWeight
        : g_RawGI.Load(int3(dispatchId.xy, 0)).rgb;

    if (g_TemporalEnabled != 0 && g_HistoryValid != 0) {
        const float2 velocity =
            (g_ResolveVelocity.Load(int3(fullPixel, 0)).xy * 2.0 - 1.0) *
            g_VelocityScale;
        const int2 historyPixel = clamp(
            int2(round(float2(dispatchId.xy) - velocity * outputSize)),
            int2(0, 0), int2(g_OutputWidth - 1, g_OutputHeight - 1));
        const float3 history = g_HistoryGI.Load(int3(historyPixel, 0)).rgb;
        filtered = lerp(filtered, history, saturate(g_HistoryWeight));
    }

    g_ResolvedGI[dispatchId.xy] = float4(filtered, 1.0);
}
)";

  struct alignas(16) ScreenSpaceGlobalIlluminationParams
  {
   Uint32 inputWidth = 0;
   Uint32 inputHeight = 0;
   Uint32 outputWidth = 0;
   Uint32 outputHeight = 0;
   Uint32 raySteps = 8;
   float intensity = 1.0f;
   float depthThickness = 0.01f;
   float radiusPixels = 24.0f;
  };

  struct alignas(16) ScreenSpaceGlobalIlluminationResolveParams
  {
   Uint32 inputWidth = 0;
   Uint32 inputHeight = 0;
   Uint32 outputWidth = 0;
   Uint32 outputHeight = 0;
   Uint32 historyValid = 0;
   Uint32 temporalEnabled = 1;
   Uint32 denoiseEnabled = 1;
   float historyWeight = 0.9f;
   float depthSigma = 0.01f;
   float normalSigma = 16.0f;
   float velocityScale = 1.0f;
   float padding0 = 0.0f;
  };

  struct TextureBundle
  {
   RefCntAutoPtr<ITexture> texture;
   RefCntAutoPtr<ITextureView> srv;
   RefCntAutoPtr<ITextureView> uav;
   RefCntAutoPtr<ITextureView> rtv;
  };

  bool createTextureBundle(IRenderDevice* device,
                           Uint32 width,
                           Uint32 height,
                           TEXTURE_FORMAT format,
                           BIND_FLAGS bindFlags,
                           const char* name,
                           TextureBundle& bundle)
  {
   if (!device || width == 0 || height == 0)
   {
    return false;
   }

   TextureDesc desc;
   desc.Name = name;
   desc.Type = RESOURCE_DIM_TEX_2D;
   desc.Width = width;
   desc.Height = height;
   desc.Format = format;
   desc.MipLevels = 1;
   desc.ArraySize = 1;
   desc.SampleCount = 1;
   desc.Usage = USAGE_DEFAULT;
   desc.BindFlags = bindFlags;

   bundle = {};
   device->CreateTexture(desc, nullptr, &bundle.texture);
   if (!bundle.texture)
   {
    qWarning() << "[RenderPipeline] CreateTexture failed for" << name
               << "size=" << width << "x" << height << "format=" << int(format);
    return false;
   }

   const bool needsSrv = (bindFlags & BIND_SHADER_RESOURCE) != 0;
   const bool needsUav = (bindFlags & BIND_UNORDERED_ACCESS) != 0;
   const bool needsRtv = (bindFlags & BIND_RENDER_TARGET) != 0;

   bundle.srv = needsSrv
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
                    : nullptr;
   bundle.uav = needsUav
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS)
                    : nullptr;
   bundle.rtv = needsRtv
                    ? bundle.texture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)
                    : nullptr;

   if ((needsSrv && !bundle.srv) || (needsUav && !bundle.uav) ||
       (needsRtv && !bundle.rtv))
   {
    qWarning() << "[RenderPipeline] Missing default views for" << name;
    bundle = {};
    return false;
   }

   return true;
  }
 } // namespace

 class RenderPipeline::Impl
 {
 public:
  RefCntAutoPtr<IRenderDevice> device_;
  TextureBundle accum_;
  TextureBundle temp_;
 TextureBundle layer_;
 TextureBundle layerFloat_;
  TextureBundle matteSource_;
  TextureBundle emission_;
  TextureBundle normal_;
  TextureBundle velocity_;
  TextureBundle objectId_;
  TextureBundle materialId_;
  TextureBundle albedo_;
  TextureBundle screenSpaceGI_;
  std::shared_ptr<GpuContext> screenSpaceGIContext_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> screenSpaceGIExecutor_;
  RefCntAutoPtr<IBuffer> screenSpaceGIParams_;
  std::unique_ptr<ArtifactCore::ComputeExecutor> screenSpaceGIResolveExecutor_;
  RefCntAutoPtr<IBuffer> screenSpaceGIResolveParams_;
  TextureBundle screenSpaceGIHistory_[2];
  Uint32 screenSpaceGIHistoryWriteIndex_ = 0;
  bool screenSpaceGIHistoryValid_ = false;
  Uint32 width_ = 0;
  Uint32 height_ = 0;
  TEXTURE_FORMAT format_ = TEX_FORMAT_UNKNOWN;
  bool emissionEnabled_ = false;
 };

 RenderPipeline::RenderPipeline()
     : impl_(new Impl())
 {
 }

 RenderPipeline::~RenderPipeline()
 {
  destroy();
  delete impl_;
  impl_ = nullptr;
 }

bool RenderPipeline::initialize(IRenderDevice* device,
                                Uint32 width,
                                Uint32 height,
                                TEXTURE_FORMAT format,
                                bool enableEmission)
 {
  if (!device || width == 0 || height == 0)
  {
   destroy();
   return false;
  }

  const TEXTURE_FORMAT resolvedFormat = format != TEX_FORMAT_UNKNOWN
                                             ? format
                                             : RenderConfig::LinearColorFormat;

  const bool sameSize = impl_->device_ == device &&
                        impl_->width_ == width &&
                        impl_->height_ == height &&
                        impl_->format_ == resolvedFormat &&
                        impl_->emissionEnabled_ == enableEmission &&
                        ready();
  if (sameSize)
  {
   return true;
  }

  destroy();
  impl_->device_ = device;
  impl_->width_ = width;
  impl_->height_ = height;
  impl_->format_ = resolvedFormat;
  impl_->emissionEnabled_ = enableEmission;

  if (!createTextures(device, width, height, resolvedFormat, enableEmission))
  {
   destroy();
   return false;
  }

  return true;
 }

 void RenderPipeline::resize(Uint32 width, Uint32 height)
 {
  if (!impl_->device_ || width == 0 || height == 0)
  {
   destroy();
   return;
  }

  if (impl_->width_ == width && impl_->height_ == height && ready())
  {
   return;
  }

  initialize(impl_->device_, width, height, impl_->format_,
             impl_->emissionEnabled_);
 }

 void RenderPipeline::destroy()
 {
  impl_->accum_ = {};
  impl_->temp_ = {};
  impl_->layer_ = {};
  impl_->layerFloat_ = {};
  impl_->matteSource_ = {};
  impl_->emission_ = {};
  impl_->normal_ = {};
  impl_->velocity_ = {};
  impl_->objectId_ = {};
  impl_->materialId_ = {};
  impl_->albedo_ = {};
  impl_->screenSpaceGI_ = {};
  impl_->screenSpaceGIExecutor_.reset();
  impl_->screenSpaceGIContext_.reset();
  impl_->screenSpaceGIParams_.Release();
  impl_->screenSpaceGIResolveExecutor_.reset();
  impl_->screenSpaceGIResolveParams_.Release();
  impl_->screenSpaceGIHistory_[0] = {};
  impl_->screenSpaceGIHistory_[1] = {};
  impl_->screenSpaceGIHistoryWriteIndex_ = 0;
  impl_->screenSpaceGIHistoryValid_ = false;
  impl_->width_ = 0;
  impl_->height_ = 0;
  impl_->format_ = TEX_FORMAT_UNKNOWN;
  impl_->emissionEnabled_ = false;
  impl_->device_ = nullptr;
 }

 bool RenderPipeline::ready() const
 {
 return impl_->device_ != nullptr && impl_->width_ > 0 && impl_->height_ > 0 &&
         impl_->accum_.texture && impl_->temp_.texture && impl_->layer_.texture &&
         impl_->layerFloat_.texture && impl_->matteSource_.texture &&
         impl_->accum_.srv && impl_->accum_.uav && impl_->accum_.rtv &&
         impl_->temp_.srv && impl_->temp_.uav && impl_->temp_.rtv &&
         impl_->layer_.srv && impl_->layer_.rtv &&
         impl_->layerFloat_.srv && impl_->layerFloat_.uav &&
         impl_->matteSource_.srv &&
         (!impl_->emissionEnabled_ ||
          (impl_->emission_.texture && impl_->emission_.srv &&
           impl_->emission_.rtv &&
           impl_->normal_.texture && impl_->normal_.srv &&
           impl_->normal_.rtv &&
           impl_->velocity_.texture && impl_->velocity_.srv &&
           impl_->velocity_.rtv)) &&
         (!impl_->emissionEnabled_ ||
          (impl_->objectId_.texture && impl_->objectId_.srv &&
           impl_->objectId_.rtv &&
           impl_->materialId_.texture && impl_->materialId_.srv &&
           impl_->materialId_.rtv &&
           impl_->albedo_.texture && impl_->albedo_.srv &&
           impl_->albedo_.rtv));
 }

 bool RenderPipeline::renderComposition(
  IDeviceContext* ctx,
  const std::vector<ArtifactAbstractLayerPtr>& layers,
  int64_t currentFrame,
  ITextureView* outputRTV)
 {
  if (!ctx || !outputRTV || !ready())
  {
   return false;
  }

  // The controller currently owns the actual layer draw/blend loop.
  // Keep this entry point available for future consolidation.
  (void)layers;
  (void)currentFrame;
  const float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
  ctx->SetRenderTargets(1, &outputRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  ctx->ClearRenderTarget(outputRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return true;
 }

 ITextureView* RenderPipeline::accumSRV() const { return impl_->accum_.srv; }
 ITextureView* RenderPipeline::accumUAV() const { return impl_->accum_.uav; }
 ITextureView* RenderPipeline::accumRTV() const { return impl_->accum_.rtv; }
 ITextureView* RenderPipeline::tempSRV() const { return impl_->temp_.srv; }
 ITextureView* RenderPipeline::tempUAV() const { return impl_->temp_.uav; }
 ITextureView* RenderPipeline::tempRTV() const { return impl_->temp_.rtv; }
 ITextureView* RenderPipeline::layerSRV() const { return impl_->layer_.srv; }
 ITextureView* RenderPipeline::layerUAV() const { return impl_->layer_.uav; }
 ITextureView* RenderPipeline::layerRTV() const { return impl_->layer_.rtv; }
ITextureView* RenderPipeline::layerFloatSRV() const { return impl_->layerFloat_.srv; }
ITextureView* RenderPipeline::layerFloatUAV() const { return impl_->layerFloat_.uav; }
ITextureView* RenderPipeline::matteSourceSRV() const { return impl_->matteSource_.srv; }
ITextureView* RenderPipeline::emissionSRV() const { return impl_->emission_.srv; }
ITextureView* RenderPipeline::emissionRTV() const { return impl_->emission_.rtv; }
bool RenderPipeline::hasEmissionTarget() const { return impl_->emissionEnabled_ && impl_->emission_.texture; }
ITextureView* RenderPipeline::normalSRV() const { return impl_->normal_.srv; }
ITextureView* RenderPipeline::normalRTV() const { return impl_->normal_.rtv; }
bool RenderPipeline::hasNormalTarget() const { return impl_->emissionEnabled_ && impl_->normal_.texture; }
ITextureView* RenderPipeline::velocitySRV() const { return impl_->velocity_.srv; }
ITextureView* RenderPipeline::velocityRTV() const { return impl_->velocity_.rtv; }
bool RenderPipeline::hasVelocityTarget() const { return impl_->emissionEnabled_ && impl_->velocity_.texture; }
ITextureView* RenderPipeline::objectIdSRV() const { return impl_->objectId_.srv; }
ITextureView* RenderPipeline::objectIdRTV() const { return impl_->objectId_.rtv; }
bool RenderPipeline::hasObjectIdTarget() const { return impl_->emissionEnabled_ && impl_->objectId_.texture; }
ITextureView* RenderPipeline::materialIdSRV() const { return impl_->materialId_.srv; }
ITextureView* RenderPipeline::materialIdRTV() const { return impl_->materialId_.rtv; }
bool RenderPipeline::hasMaterialIdTarget() const { return impl_->emissionEnabled_ && impl_->materialId_.texture; }
ITextureView* RenderPipeline::albedoSRV() const { return impl_->albedo_.srv; }
ITextureView* RenderPipeline::albedoRTV() const { return impl_->albedo_.rtv; }
bool RenderPipeline::hasAlbedoTarget() const { return impl_->emissionEnabled_ && impl_->albedo_.texture; }
GlobalIlluminationInputs RenderPipeline::globalIlluminationInputs(
    ITextureView* depthSRV) const
{
 GlobalIlluminationInputs inputs;
 inputs.depth = depthSRV;
 inputs.normal = normalSRV();
 inputs.albedo = albedoSRV();
 inputs.velocity = velocitySRV();
 inputs.emission = emissionSRV();
 return inputs;
}
bool RenderPipeline::dispatchScreenSpaceGlobalIllumination(
    IDeviceContext* ctx,
    const GlobalIlluminationInputs& inputs,
    float resolutionScale,
    Uint32 raySteps,
    float intensity,
    float depthThickness,
    bool temporalAccumulation,
    bool denoise)
{
 if (!ctx || !impl_->device_ || !inputs.validForScreenSpace() ||
     impl_->width_ == 0 || impl_->height_ == 0) {
  return false;
 }

 const float safeScale = std::clamp(resolutionScale, 0.25f, 1.0f);
 const Uint32 outputWidth = std::max(
     1u, static_cast<Uint32>(std::ceil(impl_->width_ * safeScale)));
 const Uint32 outputHeight = std::max(
     1u, static_cast<Uint32>(std::ceil(impl_->height_ * safeScale)));

 if (!impl_->screenSpaceGI_.texture ||
     impl_->screenSpaceGI_.texture->GetDesc().Width != outputWidth ||
     impl_->screenSpaceGI_.texture->GetDesc().Height != outputHeight) {
  if (!createTextureBundle(impl_->device_, outputWidth, outputHeight,
                           TEX_FORMAT_RGBA16_FLOAT,
                           BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.ScreenSpaceGI",
                           impl_->screenSpaceGI_)) {
   return false;
  }
  impl_->screenSpaceGIHistoryValid_ = false;
 }

 for (auto& history : impl_->screenSpaceGIHistory_) {
  if (!history.texture || history.texture->GetDesc().Width != outputWidth ||
      history.texture->GetDesc().Height != outputHeight) {
   if (!createTextureBundle(impl_->device_, outputWidth, outputHeight,
                            TEX_FORMAT_RGBA16_FLOAT,
                            BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                            "RenderPipeline.ScreenSpaceGIHistory",
                            history)) {
    return false;
   }
   impl_->screenSpaceGIHistoryValid_ = false;
  }
 }

 if (!impl_->screenSpaceGIExecutor_) {
  impl_->screenSpaceGIContext_ =
      std::make_shared<GpuContext>(impl_->device_, ctx);
  impl_->screenSpaceGIExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(
          *impl_->screenSpaceGIContext_);

  BufferDesc paramsDesc;
  paramsDesc.Name = "ScreenSpaceGI Params";
  paramsDesc.Usage = USAGE_DYNAMIC;
  paramsDesc.Size = sizeof(ScreenSpaceGlobalIlluminationParams);
  paramsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  paramsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  impl_->device_->CreateBuffer(paramsDesc, nullptr,
                               &impl_->screenSpaceGIParams_);

  static const ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "SSGIParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Depth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Normal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Albedo", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_Output", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc desc;
  desc.name = "ScreenSpaceGI PSO";
  desc.shaderSource = kScreenSpaceGlobalIlluminationShader;
  desc.entryPoint = "ScreenSpaceGICS";
  desc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  desc.variables = variables;
  desc.variableCount = static_cast<Uint32>(
      sizeof(variables) / sizeof(variables[0]));
  desc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->screenSpaceGIParams_ ||
      !impl_->screenSpaceGIExecutor_->build(desc) ||
      !impl_->screenSpaceGIExecutor_->createShaderResourceBinding(true)) {
   impl_->screenSpaceGIExecutor_.reset();
   impl_->screenSpaceGIContext_.reset();
   impl_->screenSpaceGIParams_.Release();
   return false;
  }

  impl_->screenSpaceGIResolveExecutor_ =
      std::make_unique<ArtifactCore::ComputeExecutor>(
          *impl_->screenSpaceGIContext_);
  BufferDesc resolveParamsDesc;
  resolveParamsDesc.Name = "ScreenSpaceGI Resolve Params";
  resolveParamsDesc.Usage = USAGE_DYNAMIC;
  resolveParamsDesc.Size =
      sizeof(ScreenSpaceGlobalIlluminationResolveParams);
  resolveParamsDesc.BindFlags = BIND_UNIFORM_BUFFER;
  resolveParamsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  impl_->device_->CreateBuffer(resolveParamsDesc, nullptr,
                               &impl_->screenSpaceGIResolveParams_);
  static const ShaderResourceVariableDesc resolveVariables[] = {
      {SHADER_TYPE_COMPUTE, "SSGIResolveParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_RawGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_HistoryGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveNormal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolveVelocity", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_COMPUTE, "g_ResolvedGI", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };
  ArtifactCore::ComputePipelineDesc resolveDesc;
  resolveDesc.name = "ScreenSpaceGI Resolve PSO";
  resolveDesc.shaderSource = kScreenSpaceGlobalIlluminationResolveShader;
  resolveDesc.entryPoint = "ScreenSpaceGIResolveCS";
  resolveDesc.sourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  resolveDesc.variables = resolveVariables;
  resolveDesc.variableCount = static_cast<Uint32>(
      sizeof(resolveVariables) / sizeof(resolveVariables[0]));
  resolveDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  if (!impl_->screenSpaceGIResolveParams_ ||
      !impl_->screenSpaceGIResolveExecutor_->build(resolveDesc) ||
      !impl_->screenSpaceGIResolveExecutor_->createShaderResourceBinding(true)) {
   impl_->screenSpaceGIResolveExecutor_.reset();
   impl_->screenSpaceGIResolveParams_.Release();
   impl_->screenSpaceGIExecutor_.reset();
   impl_->screenSpaceGIParams_.Release();
   impl_->screenSpaceGIContext_.reset();
   return false;
  }
 }

 ScreenSpaceGlobalIlluminationParams params;
 params.inputWidth = impl_->width_;
 params.inputHeight = impl_->height_;
 params.outputWidth = outputWidth;
 params.outputHeight = outputHeight;
 params.raySteps = std::clamp(raySteps, 1u, 24u);
 params.intensity = std::clamp(intensity, 0.0f, 8.0f);
 params.depthThickness = std::clamp(depthThickness, 0.0001f, 1.0f);
 params.radiusPixels = 32.0f * safeScale;

 void* mappedParams = nullptr;
 ctx->MapBuffer(impl_->screenSpaceGIParams_, MAP_WRITE,
                MAP_FLAG_DISCARD, mappedParams);
 if (!mappedParams) {
  return false;
 }
 std::memcpy(mappedParams, &params, sizeof(params));
 ctx->UnmapBuffer(impl_->screenSpaceGIParams_, MAP_WRITE);

 auto& executor = *impl_->screenSpaceGIExecutor_;
 if (!executor.setBuffer("SSGIParams", impl_->screenSpaceGIParams_) ||
     !executor.setTextureView("g_Depth", inputs.depth) ||
     !executor.setTextureView("g_Normal", inputs.normal) ||
     !executor.setTextureView("g_Albedo", inputs.albedo) ||
     !executor.setTextureView("g_Output", impl_->screenSpaceGI_.uav)) {
  return false;
 }

 const auto dispatch = ArtifactCore::ComputeExecutor::makeDispatchAttribs(
     outputWidth, outputHeight, 1, 8, 8, 1);
 executor.dispatch(ctx, dispatch,
                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 const Uint32 writeIndex = impl_->screenSpaceGIHistoryWriteIndex_;
 const Uint32 readIndex = 1u - writeIndex;
 ScreenSpaceGlobalIlluminationResolveParams resolveParams;
 resolveParams.inputWidth = impl_->width_;
 resolveParams.inputHeight = impl_->height_;
 resolveParams.outputWidth = outputWidth;
 resolveParams.outputHeight = outputHeight;
 resolveParams.historyValid = impl_->screenSpaceGIHistoryValid_ ? 1u : 0u;
 resolveParams.temporalEnabled =
     temporalAccumulation && inputs.validForTemporalReuse() ? 1u : 0u;
 resolveParams.denoiseEnabled = denoise ? 1u : 0u;
 resolveParams.historyWeight = 0.9f;
 resolveParams.depthSigma = params.depthThickness;
 resolveParams.normalSigma = 16.0f;
 resolveParams.velocityScale = 1.0f;

 void* mappedResolveParams = nullptr;
 ctx->MapBuffer(impl_->screenSpaceGIResolveParams_, MAP_WRITE,
                MAP_FLAG_DISCARD, mappedResolveParams);
 if (!mappedResolveParams) {
  return false;
 }
 std::memcpy(mappedResolveParams, &resolveParams, sizeof(resolveParams));
 ctx->UnmapBuffer(impl_->screenSpaceGIResolveParams_, MAP_WRITE);

 auto& resolveExecutor = *impl_->screenSpaceGIResolveExecutor_;
 if (!resolveExecutor.setBuffer("SSGIResolveParams",
                                impl_->screenSpaceGIResolveParams_) ||
     !resolveExecutor.setTextureView("g_RawGI", impl_->screenSpaceGI_.srv) ||
     !resolveExecutor.setTextureView(
         "g_HistoryGI", impl_->screenSpaceGIHistory_[readIndex].srv) ||
     !resolveExecutor.setTextureView("g_ResolveDepth", inputs.depth) ||
     !resolveExecutor.setTextureView("g_ResolveNormal", inputs.normal) ||
     !resolveExecutor.setTextureView(
         "g_ResolveVelocity",
         inputs.velocity ? inputs.velocity : inputs.normal) ||
     !resolveExecutor.setTextureView(
         "g_ResolvedGI", impl_->screenSpaceGIHistory_[writeIndex].uav)) {
  return false;
 }
 resolveExecutor.dispatch(ctx, dispatch,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 impl_->screenSpaceGIHistoryWriteIndex_ = readIndex;
 impl_->screenSpaceGIHistoryValid_ = true;
 return true;
}
ITextureView* RenderPipeline::screenSpaceGlobalIlluminationSRV() const
{
 if (impl_->screenSpaceGIHistoryValid_) {
  const Uint32 latestIndex = 1u - impl_->screenSpaceGIHistoryWriteIndex_;
  return impl_->screenSpaceGIHistory_[latestIndex].srv;
 }
 return impl_->screenSpaceGI_.srv;
}
void RenderPipeline::resetScreenSpaceGlobalIlluminationHistory()
{
 impl_->screenSpaceGIHistoryValid_ = false;
 impl_->screenSpaceGIHistoryWriteIndex_ = 0;
}
Uint32 RenderPipeline::screenSpaceGlobalIlluminationWidth() const
{
 return impl_->screenSpaceGI_.texture
            ? impl_->screenSpaceGI_.texture->GetDesc().Width
            : 0;
}
Uint32 RenderPipeline::screenSpaceGlobalIlluminationHeight() const
{
 return impl_->screenSpaceGI_.texture
            ? impl_->screenSpaceGI_.texture->GetDesc().Height
            : 0;
}
 bool RenderPipeline::updateMatteSourceFromData(IDeviceContext* ctx,
                                                 const void* data,
                                                 Uint32 width,
                                                 Uint32 height,
                                                 Uint32 rowStride)
 {
  if (!ctx || !data || !impl_->matteSource_.texture) return false;
  if (width == 0 || height == 0) return false;

  const auto& texDesc = impl_->matteSource_.texture->GetDesc();
  if (texDesc.Width != width || texDesc.Height != height) {
   if (!createTextureBundle(impl_->device_, width, height,
                            RenderConfig::MainRTVFormat,
                            BIND_SHADER_RESOURCE,
                            "RenderPipeline.MatteSource", impl_->matteSource_))
   {
    qWarning() << "[RenderPipeline] Matte source reallocation failed";
    return false;
   }
  }

  Box dstBox = {};
  dstBox.MinX = 0; dstBox.MaxX = width;
  dstBox.MinY = 0; dstBox.MaxY = height;
  dstBox.MinZ = 0; dstBox.MaxZ = 1;

  TextureSubResData subRes = {};
  subRes.pData = data;
  subRes.Stride = rowStride;

  ctx->UpdateTexture(impl_->matteSource_.texture, 0, 0, dstBox, subRes,
                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return true;
 }
 Uint32 RenderPipeline::width() const { return impl_->width_; }
 Uint32 RenderPipeline::height() const { return impl_->height_; }

 void RenderPipeline::swapAccumAndTemp()
 {
  std::swap(impl_->accum_, impl_->temp_);
 }

bool RenderPipeline::createTextures(IRenderDevice* device,
                                    Uint32 width,
                                    Uint32 height,
                                    TEXTURE_FORMAT format,
                                    bool enableEmission)
 {
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.Accum", impl_->accum_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.Temp", impl_->temp_))
  {
   return false;
  }
   if (!createTextureBundle(device, width, height, RenderConfig::MainRTVFormat,
                            BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                            "RenderPipeline.Layer", impl_->layer_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS,
                           "RenderPipeline.LayerFloat", impl_->layerFloat_))
  {
   return false;
  }

  // Matte source: 8-bit RGBA (non-sRGB) for CPU-uploaded matte source layer content.
  // Non-sRGB format preserves QImage byte values as-is for correct luma calculation.
  if (!createTextureBundle(device, width, height, TEX_FORMAT_RGBA8_UNORM,
                           BIND_SHADER_RESOURCE,
                           "RenderPipeline.MatteSource", impl_->matteSource_))
  {
   return false;
  }

  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Emission", impl_->emission_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Normal", impl_->normal_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Velocity", impl_->velocity_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, TEX_FORMAT_RGBA16_FLOAT,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.ObjectId", impl_->objectId_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, TEX_FORMAT_RGBA16_FLOAT,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.MaterialId", impl_->materialId_))
  {
   return false;
  }
  if (enableEmission &&
      !createTextureBundle(device, width, height, format,
                           BIND_RENDER_TARGET | BIND_SHADER_RESOURCE,
                           "RenderPipeline.Albedo", impl_->albedo_))
  {
   return false;
  }

  return true;
}
}
