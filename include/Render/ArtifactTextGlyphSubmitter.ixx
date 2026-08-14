module;
#include <QImage>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <span>
export module Artifact.Render.TextGlyphSubmitter.Contract;

import Color.Float;
import Text.GlyphLayout;
import Text.Style;

export namespace Artifact {

/// Provider boundary for the two glyph pipelines. Implementations may be
/// backed by the existing ShaderManager during migration, but the submitter
/// does not depend on that concrete type.
struct ArtifactTextGlyphPipelineProvider {
    Diligent::IPipelineState* glyphPipeline = nullptr;
    Diligent::IShaderResourceBinding* glyphBinding = nullptr;
    Diligent::IPipelineState* transformedGlyphPipeline = nullptr;
    Diligent::IShaderResourceBinding* transformedGlyphBinding = nullptr;
    Diligent::ISampler* atlasSampler = nullptr;

    bool isValid() const noexcept {
        return glyphPipeline && glyphBinding && transformedGlyphPipeline &&
               transformedGlyphBinding && atlasSampler;
    }
};

/// Glyph-only GPU submission contract.
/// This boundary intentionally knows nothing about layers, swap chains,
/// composition, 3D, particles, or post-processing.
class ArtifactTextGlyphSubmitter final {
public:
    ArtifactTextGlyphSubmitter();
    ~ArtifactTextGlyphSubmitter();
    ArtifactTextGlyphSubmitter(const ArtifactTextGlyphSubmitter&) = delete;
    ArtifactTextGlyphSubmitter& operator=(const ArtifactTextGlyphSubmitter&) = delete;

    bool initialize(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                    Diligent::TEXTURE_FORMAT targetFormat,
                    ArtifactTextGlyphPipelineProvider pipelines);
    bool isInitialized() const;
    void clear(Diligent::IDeviceContext* context,
               Diligent::ITextureView* target,
               const ArtifactCore::FloatColor& color = {0.0f, 0.0f, 0.0f, 0.0f});
    bool submit(Diligent::IDeviceContext* context,
                Diligent::ITextureView* target,
                std::span<const ArtifactCore::GlyphItem> glyphs,
                const ArtifactCore::TextStyle& style,
                const ArtifactCore::FloatColor& color,
                float opacity = 1.0f);
    void flush(Diligent::IDeviceContext* context);
    void destroy();

private:
    class Impl;
    Impl* impl_ = nullptr;
};

}
