module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module Artifact.Render.MotionBlurPass;
import Graphics.Compute;

export namespace Artifact {

enum class MotionBlurShutterShape : unsigned {
  Rectangular = 0,
  Triangle = 1,
  Trapezoid = 2,
};

struct MotionBlurSettings {
  bool enabled = false;
  float shutterAngle = 180.0f;
  float shutterPhase = 0.0f; // Offset in frame fractions; 1.0 is one frame.
  float velocityScale = 1.0f;
  unsigned sampleCount = 8;
  MotionBlurShutterShape shutterShape = MotionBlurShutterShape::Rectangular;
  // Fraction of each half-aperture held at full weight for Trapezoid.
  float trapezoidPlateau = 0.5f;
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
  bool applyWithCandidates(
             Diligent::IDeviceContext* context,
             Diligent::ITextureView* color,
             Diligent::ITextureView* velocity,
             Diligent::ITextureView* depth,
             Diligent::ITextureView* output,
             Diligent::ITextureView* candidateIndex0SRV,
             Diligent::ITextureView* candidateIndex0UAV,
             Diligent::ITextureView* candidateIndex1SRV,
             Diligent::ITextureView* candidateIndex1UAV,
             unsigned width,
             unsigned height,
             const MotionBlurSettings& settings);
  bool ready() const;

 private:
  bool applyInternal(Diligent::IDeviceContext* context,
             Diligent::ITextureView* color,
             Diligent::ITextureView* velocity,
             Diligent::ITextureView* depth,
             Diligent::ITextureView* output,
             Diligent::ITextureView* candidateMap,
             unsigned width,
             unsigned height,
             const MotionBlurSettings& settings);
  Diligent::ITextureView* buildCandidateMap(
             Diligent::IDeviceContext* context,
             Diligent::ITextureView* velocity,
             Diligent::ITextureView* depth,
             Diligent::ITextureView* candidateIndex0SRV,
             Diligent::ITextureView* candidateIndex0UAV,
             Diligent::ITextureView* candidateIndex1SRV,
             Diligent::ITextureView* candidateIndex1UAV,
             unsigned width,
             unsigned height,
             const MotionBlurSettings& settings);
  struct Impl;
  Impl* impl_ = nullptr;
};

}
