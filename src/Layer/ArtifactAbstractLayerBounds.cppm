#include <QRectF>
#include <QString>

import Artifact.Layer.Abstract;

import Artifact.Effect.Abstract;
import Artifact.Render.ROI;

namespace Artifact {

QRectF LayerBounds::boundsFor(LayerBoundsKind kind) const {
  switch (kind) {
    case LayerBoundsKind::Source:
      return sourceBounds;
    case LayerBoundsKind::Visible:
      return visibleBounds;
    case LayerBoundsKind::Effect:
      return effectBounds;
    case LayerBoundsKind::Mask:
      return maskBounds;
    case LayerBoundsKind::Layout:
      return layoutBounds;
  }
  return layoutBounds;
}

LayerBounds ArtifactAbstractLayer::contentBounds() const {
  const QRectF source = localBounds();
  const QRectF visible = transformedBoundingBox();
  LayerBounds bounds;
  bounds.sourceBounds = source;
  bounds.visibleBounds = visible;

  // A layer can resolve the local effect stack but not a full composition ROI.
  RenderROI effectROI(visible);
  for (const auto& effect : getEffects()) {
    if (!effect || !effect->isEnabled()) continue;
    if (effect->roiHint().requiresFullFrame) continue;
    effectROI = effect->expandedROI(effectROI);
  }
  bounds.effectBounds = effectROI.rect;
  bounds.maskBounds = visible;
  bounds.layoutBounds = source.isValid() ? source : visible;
  return bounds;
}

QRectF ArtifactAbstractLayer::contentBounds(LayerBoundsKind kind) const {
  return contentBounds().boundsFor(kind);
}

QRectF ArtifactAbstractLayer::sourceBounds() const {
  return contentBounds(LayerBoundsKind::Source);
}

QRectF ArtifactAbstractLayer::visibleBounds() const {
  return contentBounds(LayerBoundsKind::Visible);
}

QString ArtifactAbstractLayer::contentBoundsSummary() const {
  const LayerBounds bounds = contentBounds();
  const auto rectString = [](const QRectF& rect) {
    return rect.isValid()
               ? QStringLiteral("%1,%2 %3x%4")
                     .arg(rect.x(), 0, 'f', 1)
                     .arg(rect.y(), 0, 'f', 1)
                     .arg(rect.width(), 0, 'f', 1)
                     .arg(rect.height(), 0, 'f', 1)
               : QStringLiteral("invalid");
  };
  return QStringLiteral("source=%1 visible=%2 effect=%3 mask=%4 layout=%5")
      .arg(rectString(bounds.sourceBounds), rectString(bounds.visibleBounds),
           rectString(bounds.effectBounds), rectString(bounds.maskBounds),
           rectString(bounds.layoutBounds));
}

QRectF ArtifactAbstractLayer::effectBounds() const {
  return contentBounds(LayerBoundsKind::Effect);
}

QRectF ArtifactAbstractLayer::maskBounds() const {
  return contentBounds(LayerBoundsKind::Mask);
}

QRectF ArtifactAbstractLayer::layoutBounds() const {
  return contentBounds(LayerBoundsKind::Layout);
}

} // namespace Artifact
