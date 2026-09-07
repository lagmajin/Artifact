module;
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"

export module Artifact.Render.AccumulatorPass;

export namespace Artifact {
struct AccumulatorPassParams {
  float inputGain = 1.0f;
  float feedback = 0.95f;
  float decay = 1.0f;
  std::uint32_t blendMode = 1; // ArtifactCore::BlendMode::Add
};

class LIBRARY_DLL_API AccumulatorPass {
  class Impl;
  Impl* impl_ = nullptr;
public:
  AccumulatorPass();
  ~AccumulatorPass();
  bool initialize(Diligent::IRenderDevice* device, Diligent::IDeviceContext* context);
  bool ready() const;
  bool apply(Diligent::IDeviceContext* context,
             Diligent::ITextureView* currentSRV,
             Diligent::ITextureView* previousSRV,
             Diligent::ITextureView* outputUAV,
             Diligent::Uint32 width, Diligent::Uint32 height,
             const AccumulatorPassParams& params);
};
}
