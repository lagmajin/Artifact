module;
#include <algorithm>
#include <cstring>
#include <memory>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module Artifact.Render.DepthOfFieldPass;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

export namespace Artifact {

struct DepthOfFieldSettings {
  bool enabled = false;
  float focusDistance = 1000.0f; // view-space units
  float nearClip = 1.0f;         // camera near plane (projection contract)
  float farClip = 100000.0f;     // camera far plane
  float maxCocRadius = 16.0f;    // pixels at full defocus
  float cocScale = 1.0f;         // authored blur amount normalization

  // Thin-lens physical model (used when fStop > 0):
  //   CoC(d) = |aperture * focalLength * (focusDistance - d)|
  //            / (focusDistance * (d - focalLength)) * maxCocRadius
  // aperture is the authored aperture value interpreted as an f-stop scale.
  float focalLength = 50.0f;     // mm, 35mm-equivalent lens
  float fStop = 0.0f;            // <= 0 falls back to the linear ramp

  unsigned sampleCount = 12;
};

class LIBRARY_DLL_API DepthOfFieldPass {
 public:
  DepthOfFieldPass();
  ~DepthOfFieldPass();

  bool initialize(Diligent::IRenderDevice* device,
                  Diligent::IDeviceContext* context);
  // color: HDR scene color, depth: the same non-linear [0,1] window depth
  // the SSGI pass consumes (TEX_FORMAT_D32_FLOAT SRV),
  // output: blurred result UAV.
  bool apply(Diligent::IDeviceContext* context,
             Diligent::ITextureView* color,
             Diligent::ITextureView* depth,
             Diligent::ITextureView* output,
             unsigned width,
             unsigned height,
             const DepthOfFieldSettings& settings);
  bool ready() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}
