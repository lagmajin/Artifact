module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Primary render target format for the composition and UI swap chains
    // RGBA16_FLOAT: linear HDR, Vulkan/D3D12/WebGL 全バックエンド対応
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA16_FLOAT;
    
    // Compute compositing buffers run in linear float space.
    // Keep this aligned with RWTexture2D<float4> in the current blend shaders.
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = Diligent::TEX_FORMAT_RGBA32_FLOAT;
};

} // namespace Artifact
