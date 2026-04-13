module;
#include <DeviceContext.h>
export module Artifact.Render.IRenderSubmitter;

import Artifact.Render.RenderCommandBuffer;

export namespace Artifact {

class IRenderSubmitter {
public:
    virtual ~IRenderSubmitter() = default;
    virtual void submit(RenderCommandBuffer& buf, Diligent::IDeviceContext* ctx) = 0;
};

} // namespace Artifact
