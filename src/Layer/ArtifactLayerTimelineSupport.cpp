#include <cstdint>

#include <QPointF>

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Frame.Rate;
import Frame.Position;
import Time.Rational;

namespace Artifact {

void applyCompositionTransformFields(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY) {
  if (!layer) return;
  const auto* composition =
      dynamic_cast<const ArtifactAbstractComposition*>(layer->compositionObject());
  if (!composition) return;
  const auto adjustment = composition->evaluateTransformFields(
      layer->id(), QPointF(positionX, positionY));
  if (!adjustment.affected) return;
  positionX += adjustment.positionOffset.x();
  positionY += adjustment.positionOffset.y();
  scaleX *= adjustment.scaleMultiplier;
  scaleY *= adjustment.scaleMultiplier;
}

double effectiveLayerFrameRate(const ArtifactAbstractLayer* layer) {
  if (!layer) return 30.0;
  auto* composition =
      dynamic_cast<ArtifactAbstractComposition*>(layer->compositionObject());
  if (!composition) return 30.0;
  const double fps = composition->frameRate().framerate();
  return fps > 0.0 ? fps : 30.0;
}

int64_t currentTimelineFrame(const ArtifactAbstractLayer* layer) {
  if (!layer) return 0;
  auto* composition =
      dynamic_cast<ArtifactAbstractComposition*>(layer->compositionObject());
  return composition ? composition->framePosition().framePosition()
                     : layer->currentFrame();
}

RationalTime currentTimelineTime(const ArtifactAbstractLayer* layer) {
  return RationalTime(
      currentTimelineFrame(layer),
      ArtifactCore::FrameRate::storageScaleForFps(effectiveLayerFrameRate(layer)));
}

RationalTime timelineTimeForFramePosition(const ArtifactAbstractLayer* layer,
                                          const FramePosition& position) {
  return RationalTime(
      position.framePosition(),
      ArtifactCore::FrameRate::storageScaleForFps(effectiveLayerFrameRate(layer)));
}

} // namespace Artifact
