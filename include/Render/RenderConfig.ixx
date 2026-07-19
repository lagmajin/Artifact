module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

import Graphics.SurfaceColorContract;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    // Swapchain-facing and immediate graphics RTV format.
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    // Composite intermediates stay in float so GPU blending is not forced down to
    // 8-bit UNORM. Use a storage-image compatible linear format for Vulkan.
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    static constexpr Diligent::TEXTURE_FORMAT LinearColorFormat = PipelineFormat;
    static constexpr ArtifactCore::SurfaceColorDescriptor MainRTVColor =
        ArtifactCore::SurfaceColorDescriptor::encodedSrgbRgba8Premultiplied();
    static constexpr ArtifactCore::SurfaceColorDescriptor PipelineColor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();
};

} // namespace Artifact
