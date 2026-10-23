module;
#include <utility>

module Artifact.Render.CompositionRenderer;

namespace Artifact {

CompositionRenderer::CompositionRenderer(ArtifactIRenderer& renderer)
 : renderer_(&renderer)
{
}

void CompositionRenderer::SetCompositionSize(float width, float height)
{
 if (width > 0.0f) {
  compositionWidth_ = width;
 }
 if (height > 0.0f) {
  compositionHeight_ = height;
 }
}

void CompositionRenderer::ApplyCompositionSpace()
{
 if (!renderer_) {
  return;
 }
 renderer_->setCanvasSize(compositionWidth_, compositionHeight_);
}

void CompositionRenderer::DrawCompositionRect(float x, float y, float w, float h, const ArtifactCore::FloatColor& color, float opacity)
{
 if (!renderer_) {
  return;
 }
 renderer_->drawRectLocal(x, y, w, h, color, opacity);
}

void CompositionRenderer::DrawCompositionBackground(const ArtifactCore::FloatColor& color, float opacity)
{
 DrawCompositionRect(0.0f, 0.0f, compositionWidth_, compositionHeight_, color, opacity);
}

}
