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
   desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

   bundle = {};
   device->CreateTexture(desc, nullptr, &bundle.texture);
   if (!bundle.texture)
   {
    qWarning() << "[RenderPipeline] CreateTexture failed for" << name
               << "size=" << width << "x" << height << "format=" << int(format);
    return false;
   }

   bundle.srv = bundle.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
   bundle.uav = bundle.texture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
   bundle.rtv = bundle.texture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

   if (!bundle.srv || !bundle.uav || !bundle.rtv)
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
  Uint32 width_ = 0;
  Uint32 height_ = 0;
  TEXTURE_FORMAT format_ = TEX_FORMAT_UNKNOWN;
 };

 RenderPipeline::RenderPipeline()
     : impl_(new Impl())
 {
 }

 RenderPipeline::~RenderPipeline()
 {
  destroy();
  delete impl_;
 }

 bool RenderPipeline::initialize(IRenderDevice* device,
                                 Uint32 width,
                                 Uint32 height,
                                 TEXTURE_FORMAT format)
 {
  if (!device || width == 0 || height == 0)
  {
   destroy();
   return false;
  }

  const TEXTURE_FORMAT resolvedFormat = format != TEX_FORMAT_UNKNOWN
                                            ? format
                                            : RenderConfig::PipelineFormat;

  const bool sameSize = impl_->device_ == device &&
                        impl_->width_ == width &&
                        impl_->height_ == height &&
                        impl_->format_ == resolvedFormat &&
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

  if (!createTextures(device, width, height, resolvedFormat))
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

  initialize(impl_->device_, width, height, impl_->format_);
 }

 void RenderPipeline::destroy()
 {
  impl_->accum_ = {};
  impl_->temp_ = {};
  impl_->layer_ = {};
  impl_->width_ = 0;
  impl_->height_ = 0;
  impl_->format_ = TEX_FORMAT_UNKNOWN;
  impl_->device_ = nullptr;
 }

 bool RenderPipeline::ready() const
 {
  return impl_->device_ != nullptr && impl_->width_ > 0 && impl_->height_ > 0 &&
         impl_->accum_.texture && impl_->temp_.texture && impl_->layer_.texture &&
         impl_->accum_.srv && impl_->accum_.uav && impl_->accum_.rtv &&
         impl_->temp_.srv && impl_->temp_.uav && impl_->temp_.rtv &&
         impl_->layer_.srv && impl_->layer_.uav && impl_->layer_.rtv;
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
 Uint32 RenderPipeline::width() const { return impl_->width_; }
 Uint32 RenderPipeline::height() const { return impl_->height_; }

 void RenderPipeline::swapAccumAndTemp()
 {
  std::swap(impl_->accum_, impl_->temp_);
 }

 bool RenderPipeline::createTextures(IRenderDevice* device,
                                     Uint32 width,
                                     Uint32 height,
                                     TEXTURE_FORMAT format)
 {
  if (!createTextureBundle(device, width, height, format, "RenderPipeline.Accum", impl_->accum_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format, "RenderPipeline.Temp", impl_->temp_))
  {
   return false;
  }
  if (!createTextureBundle(device, width, height, format, "RenderPipeline.Layer", impl_->layer_))
  {
   return false;
  }

  return true;
 }
}
