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
import Artifact.Effect.Abstract;
import Artifact.Render.PointwiseEffectFusion;
import Artifact.Render.ROI;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;

export namespace Artifact
{
 using namespace Diligent;
 using ArtifactCore::LayerBlendPipeline;
 using ArtifactCore::GpuContext;
 using ArtifactCore::BlendMode;

 struct GlobalIlluminationInputs
 {
  ITextureView* depth = nullptr;
  ITextureView* normal = nullptr;
  ITextureView* albedo = nullptr;
  ITextureView* velocity = nullptr;
  ITextureView* emission = nullptr;

  bool validForScreenSpace() const
  {
   return depth != nullptr && normal != nullptr && albedo != nullptr;
  }

  bool validForTemporalReuse() const
  {
   return validForScreenSpace() && velocity != nullptr;
  }
 };

 class LIBRARY_DLL_API RenderPipeline
 {
 public:
  RenderPipeline();
  ~RenderPipeline();

  // Per-target auxiliary (AOV) request. Targets are allocated on demand;
  // each has*Target() is request && texture, and every draw gate already
  // null-checks its RTV, so unrequested targets are simply skipped.
  struct AuxiliaryTargetRequest {
    bool emission = false;
    bool normal = false;
    bool velocity = false;
    bool objectId = false;
    bool materialId = false;
    bool albedo = false;
    bool position = false;
    bool uv = false;
    bool operator==(const AuxiliaryTargetRequest&) const = default;
  };

  bool initialize(IRenderDevice* device, Uint32 width, Uint32 height,
                  TEXTURE_FORMAT format,
                  AuxiliaryTargetRequest auxiliaryTargets = {});
  void resize(Uint32 width, Uint32 height);
  void destroy();

  bool ready() const;

  bool renderComposition(
   IDeviceContext* ctx,
   const std::vector<ArtifactAbstractLayerPtr>& layers,
   int64_t currentFrame,
   ITextureView* outputRTV,
   const RenderROI& renderROI = RenderROI()
  );

  // Apply a pointwise effect stack to the current accumulation entirely on GPU.
  // The result remains available through accumSRV().
  bool applyPointwise(
   IDeviceContext* ctx,
   const ArtifactCore::PointwiseEffectStack& stack,
   ITextureView* backgroundSRV = nullptr,
   ITextureView* lutSRV = nullptr,
   ITextureView* historySRV = nullptr
  );

  // Execute one backend-neutral spatial node over existing GPU-resident
  // RGBA16F targets. Scratch and output must be distinct UAVs.
  bool applySpatialEffect(
   IDeviceContext* ctx, ITextureView* inputSRV,
   ITextureView* scratchUAV, ITextureView* outputUAV,
   const GpuSpatialEffectNode& node);

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
  ITextureView* matteSourceSRV(int index) const;
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
  ITextureView* positionSRV() const;
  ITextureView* positionRTV() const;
  bool hasPositionTarget() const;
  ITextureView* uvSRV() const;
  ITextureView* uvRTV() const;
  bool hasUvTarget() const;
  GlobalIlluminationInputs globalIlluminationInputs(
      ITextureView* depthSRV) const;
  bool dispatchScreenSpaceGlobalIllumination(
      IDeviceContext* ctx,
      const GlobalIlluminationInputs& inputs,
      float resolutionScale,
      Uint32 raySteps,
      float intensity = 1.0f,
      float depthThickness = 0.01f,
      bool temporalAccumulation = true,
      bool denoise = true);
  // Multiplies the resolved screen-space AO mask into a full-resolution
  // composition color target. Source and destination must not alias.
  bool applyScreenSpaceAmbientOcclusion(
      IDeviceContext* ctx, ITextureView* sourceColor,
      ITextureView* destinationColor, ITextureView* aoMask);
  // Lightweight post-resolve anti-aliasing for geometry rendered into the
  // single-sample composition targets.
  bool applyFastApproximateAntiAliasing(
      IDeviceContext* ctx, ITextureView* sourceColor,
      ITextureView* destinationColor);
  void resetScreenSpaceGlobalIlluminationHistory();
  ITextureView* screenSpaceGlobalIlluminationSRV() const;
  Uint32 screenSpaceGlobalIlluminationWidth() const;
  Uint32 screenSpaceGlobalIlluminationHeight() const;
  bool updateMatteSourceFromData(IDeviceContext* ctx,
                                  const void* data,
                                  Uint32 width,
                                  Uint32 height,
                                  Uint32 rowStride);
  bool updateMatteSourceFromData(IDeviceContext* ctx,
                                  int index,
                                  const void* data,
                                  Uint32 width,
                                  Uint32 height,
                                  Uint32 rowStride);
  Uint32 width() const;
  Uint32 height() const;

  void swapAccumAndTemp();

 private:
  bool createTextures(IRenderDevice* device, Uint32 width, Uint32 height,
                      TEXTURE_FORMAT format, AuxiliaryTargetRequest request);

  struct Impl;
  Impl* impl_ = nullptr;
 };

}
