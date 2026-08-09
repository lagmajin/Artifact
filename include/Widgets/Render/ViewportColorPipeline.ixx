module;

#include <wobjectdefs.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Artifact.Widgets.ViewportColorPipeline;

export namespace Artifact {

class ViewportColorPipeline {
public:
  ViewportColorPipeline(Diligent::IRenderDevice* device,
                        Diligent::IDeviceContext* context);
  ~ViewportColorPipeline();

  Diligent::ITextureView* apply(Diligent::IDeviceContext* context,
                                Diligent::ITextureView* source,
                                Diligent::ITextureView* destination,
                                int width, int height);
  void clear();

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace Artifact
