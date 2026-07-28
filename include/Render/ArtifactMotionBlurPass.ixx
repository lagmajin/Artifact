module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module Artifact.Render.MotionBlurPass;
import Graphics.Compute;

export namespace Artifact {

struct MotionBlurSettings {
  bool enabled = false;
  float shutterAngle = 180.0f;
  float shutterPhase = 0.0f;
  float velocityScale = 1.0f;
  unsigned sampleCount = 8;
};

class LIBRARY_DLL_API MotionBlurPass {
 public:
  MotionBlurPass();
  ~MotionBlurPass();

  bool initialize(Diligent::IRenderDevice* device,
                  Diligent::IDeviceContext* context);
  bool apply(Diligent::IDeviceContext* context,
             Diligent::ITextureView* color,
             Diligent::ITextureView* velocity,
             Diligent::ITextureView* depth,
             Diligent::ITextureView* output,
             unsigned width,
             unsigned height,
             const MotionBlurSettings& settings);
  bool ready() const;

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}
