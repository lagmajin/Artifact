module;
#include <QMatrix4x4>
#include <QRectF>

module Artifact.Layer.Video;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Render.IRenderer;

namespace Artifact {
void drawVideoFrameWithClonerEffect(
    ArtifactVideoLayer* layer, ArtifactIRenderer* renderer,
    const QMatrix4x4& transform,
    const ArtifactCore::ImageF32x4_RGBA& frameBuffer,
    const QRectF& drawRect, const QRectF& uvRect) {
  drawWithClonerEffect(
      layer, transform,
      [renderer, frameBuffer, drawRect, uvRect, layer]
      (const QMatrix4x4& targetTransform, const float weight) {
        renderer->drawSpriteTransformed(
            static_cast<float>(drawRect.x()),
            static_cast<float>(drawRect.y()),
            static_cast<float>(drawRect.width()),
            static_cast<float>(drawRect.height()), targetTransform, frameBuffer,
            layer->opacity() * weight, uvRect);
      });
}
} // namespace Artifact
