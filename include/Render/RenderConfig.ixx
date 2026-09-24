module;
#include <utility>
#include <atomic>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypes.h>

export module Artifact.Render.Config;

import Graphics.SurfaceColorContract;

export namespace Artifact {

// Global render configuration constants
struct RenderConfig {
    static bool hdrDisplayEnabled() noexcept { return HDRDisplayEnabled.load(std::memory_order_relaxed); }
    static void setHDRDisplayEnabled(bool enabled) noexcept { HDRDisplayEnabled.store(enabled, std::memory_order_relaxed); }
    // Read once at startup from the file beside the running executable.
    // Validation is opt-in because Diligent's validation layer can be costly
    // during normal interactive rendering.
    static bool diligentValidationEnabled()
    {
        static const bool enabled = [] {
            const QString optionsPath = QDir(QCoreApplication::applicationDirPath())
                .filePath(QStringLiteral("ArtifactStudio.options.ini"));
            QSettings options(optionsPath, QSettings::IniFormat);
            const QString key = QStringLiteral("Diligent/EnableValidation");
            if (!options.contains(key)) {
                options.setValue(key, false);
                options.sync();
            }
            return options.value(key, false).toBool();
        }();
        return enabled;
    }
    // Swapchain-facing and immediate graphics RTV format.
    static constexpr Diligent::TEXTURE_FORMAT MainRTVFormat = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    // Composite intermediates stay in float so GPU blending is not forced down to
    // 8-bit UNORM. Use a storage-image compatible linear format for Vulkan.
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormatF32 = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormatF16 = Diligent::TEX_FORMAT_RGBA16_FLOAT;
    // Preserve the existing F32 composition path as the default.
    static constexpr Diligent::TEXTURE_FORMAT PipelineFormat = PipelineFormatF32;
    static constexpr Diligent::TEXTURE_FORMAT LinearColorFormat = PipelineFormat;
    static constexpr ArtifactCore::SurfaceColorDescriptor MainRTVColor =
        ArtifactCore::SurfaceColorDescriptor::encodedSrgbRgba8Premultiplied();
    static constexpr ArtifactCore::SurfaceColorDescriptor PipelineColor =
        ArtifactCore::SurfaceColorDescriptor::canonicalLinearPremultiplied();

private:
    static inline std::atomic_bool HDRDisplayEnabled{false};
};

} // namespace Artifact
