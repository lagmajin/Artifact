module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Primary render target format for the composition and UI swap chains
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    
    // Compute compositing buffers run in linear float space.
    // Keep this aligned with RWTexture2D<float4> in the current blend shaders.
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = Diligent::TEX_FORMAT_RGBA32_FLOAT;
};

} // namespace Artifact
