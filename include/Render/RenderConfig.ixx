module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Primary render target format for the composition and UI swap chains
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    
    // Fallback or intermediate pipeline format (must be compatible with MainRTVFormat for final sprite draw)
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
};

} // namespace Artifact
