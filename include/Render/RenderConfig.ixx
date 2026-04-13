module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Single color format used across the current render path.
    // Keep render targets, intermediate textures, and PSO RTV formats aligned.
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = MainRTVFormat;
    static constexpr Diligent::TEXTURE_FORMAT LinearColorFormat = MainRTVFormat;
};

} // namespace Artifact
