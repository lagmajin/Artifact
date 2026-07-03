module;
#include <utility>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"
#include <vector>

export module Artifact.Render.Pipeline;
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

  bool initialize(IRenderDevice* device, Uint32 width, Uint32 height,
                  TEXTURE_FORMAT format,
                  bool enableEmission = false);
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
  ITextureView* accumRTV() const;
  ITextureView* tempSRV() const;
  ITextureView* tempUAV() const;
  ITextureView* tempRTV() const;
  ITextureView* layerSRV() const;
  ITextureView* layerUAV() const;
  ITextureView* layerRTV() const;
  ITextureView* layerFloatSRV() const;
  ITextureView* layerFloatUAV() const;
  ITextureView* matteSourceSRV() const;
  ITextureView* emissionSRV() const;
  ITextureView* emissionRTV() const;
  bool hasEmissionTarget() const;
  ITextureView* normalSRV() const;
  ITextureView* normalRTV() const;
  bool hasNormalTarget() const;
  ITextureView* velocitySRV() const;
  ITextureView* velocityRTV() const;
  bool hasVelocityTarget() const;
  ITextureView* objectIdSRV() const;
  ITextureView* objectIdRTV() const;
  bool hasObjectIdTarget() const;
  ITextureView* materialIdSRV() const;
  ITextureView* materialIdRTV() const;
  bool hasMaterialIdTarget() const;
  ITextureView* albedoSRV() const;
  ITextureView* albedoRTV() const;
  bool hasAlbedoTarget() const;
  bool updateMatteSourceFromData(IDeviceContext* ctx,
                                  const void* data,
                                  Uint32 width,
                                  Uint32 height,
                                  Uint32 rowStride);
  Uint32 width() const;
  Uint32 height() const;

  void swapAccumAndTemp();

 private:
  bool createTextures(IRenderDevice* device, Uint32 width, Uint32 height,
                      TEXTURE_FORMAT format, bool enableEmission);

  struct Impl;
  Impl* impl_ = nullptr;
 };

}
