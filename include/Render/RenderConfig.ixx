module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Canonical linear HDR color format used by the main composition path.
    // Keep render targets, intermediate render textures, and PSO RTV formats aligned.
    static constexpr Diligent::TEXTURE_FORMAT LinearColorFormat = Diligent::TEX_FORMAT_RGBA16_FLOAT;
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = LinearColorFormat;
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = LinearColorFormat;
};

} // namespace Artifact
