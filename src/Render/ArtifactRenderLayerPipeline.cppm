module;
#include <algorithm>
#include <utility>

#include <QDebug>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
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

import Artifact.Render.Config;

namespace Artifact
{
 using namespace Diligent;
 using ArtifactCore::BlendMode;
 using ArtifactCore::GpuContext;
 using ArtifactCore::LayerBlendPipeline;

 namespace
 {
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
