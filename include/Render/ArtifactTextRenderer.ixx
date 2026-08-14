module;
#include <QImage>
#include <QPointF>
#include <span>
#include <RenderDevice.h>
#include <RefCntAutoPtr.hpp>
export module Artifact.Render.TextRenderer;

import Color.Float;
import Text.GlyphLayout;
import Text.Style;
import Utils.String.UniString;

export namespace Artifact {

/// Minimal GPU contract for text animation validation.
/// This deliberately excludes composition, 3D, effects, and layer rendering.
class ArtifactTextRenderer {
public:
    ArtifactTextRenderer();
    ~ArtifactTextRenderer();
    ArtifactTextRenderer(const ArtifactTextRenderer&) = delete;
    ArtifactTextRenderer& operator=(const ArtifactTextRenderer&) = delete;

    bool initializeHeadless(int width, int height);
    bool isInitialized() const;
    void clear(const ArtifactCore::FloatColor& color = {0.0f, 0.0f, 0.0f, 0.0f});
    void drawGlyphs(std::span<const ArtifactCore::GlyphItem> glyphs,
                    const ArtifactCore::TextStyle& style,
                    const ArtifactCore::FloatColor& color,
                    float opacity = 1.0f);
    void flushAndWait();
    QImage readbackToImage() const;
    void destroy();

private:
    class Impl;
    Impl* impl_ = nullptr;
};

}
