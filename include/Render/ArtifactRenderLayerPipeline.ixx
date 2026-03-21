module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>

//#include <QVector>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module Artifact.Render.Pipeline;

import std;

import Layer.Blend;
import Artifact.Layer.Abstract;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;

export namespace Artifact
{
 using namespace Diligent;
 using ArtifactCore::LayerBlendPipeline;
 using ArtifactCore::GpuContext;
 using ArtifactCore::BlendMode;

 class LIBRARY_DLL_API RenderPipeline
 {
 public:
  RenderPipeline();
  ~RenderPipeline();

  bool initialize(IRenderDevice* device, Uint32 width, Uint32 height, TEXTURE_FORMAT format);
  void resize(Uint32 width, Uint32 height);
  void destroy();

  bool ready() const;

  bool renderComposition(
   IDeviceContext* ctx,
   const std::vector<ArtifactAbstractLayerPtr>& layers,
   int64_t currentFrame,
   ITextureView* outputRTV
  );

   ITextureView* accumSRV() const;
   ITextureView* accumUAV() const;
   ITextureView* tempSRV() const;
   ITextureView* tempUAV() const;
   ITextureView* tempRTV() const;
   Uint32 width() const;
   Uint32 height() const;

  private:
  bool createTextures(IRenderDevice* device, Uint32 width, Uint32 height, TEXTURE_FORMAT format);
  void swapAccumAndTemp();

  struct Impl;
  Impl* impl_;
 };

}
