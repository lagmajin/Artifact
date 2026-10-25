module;
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QVector4D>
module Artifact.Preview.Pipeline;

import Artifact.Layer.Svg;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Clone;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Color.Float;

namespace Artifact
{
void drawCloneSelectionOverlay(ArtifactIRenderer* renderer,
                               const ArtifactAbstractLayerPtr& layer) {
  if (!renderer || !layer) return;
  const auto cloneLayer = ArtifactCore::dynamicPointerCast<ArtifactCloneLayer>(layer);
  if (!cloneLayer) return;

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) return;

  const auto clones = cloneLayer->generateCloneData();
  if (clones.empty()) return;

  const QTransform globalTransform = layer->getGlobalTransform();
  const FloatColor outerColor(0.96f, 0.56f, 0.18f, 0.90f);
  const FloatColor innerColor(0.18f, 0.10f, 0.04f, 0.64f);
  const auto mapClonePoint = [&](const QMatrix4x4& cloneTransform,
                                 const QPointF& point) -> QPointF {
    const QVector4D mapped = cloneTransform *
        QVector4D(static_cast<float>(point.x()), static_cast<float>(point.y()), 0.0f, 1.0f);
    return globalTransform.map(QPointF(static_cast<qreal>(mapped.x()),
                                       static_cast<qreal>(mapped.y())));
  };

  for (const auto& clone : clones) {
    if (!clone.visible) continue;
    const QPointF tl = mapClonePoint(clone.transform, localBounds.topLeft());
    const QPointF tr = mapClonePoint(clone.transform, localBounds.topRight());
    const QPointF br = mapClonePoint(clone.transform, localBounds.bottomRight());
    const QPointF bl = mapClonePoint(clone.transform, localBounds.bottomLeft());
    renderer->drawThickLineLocal({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 {static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 1.7f, outerColor);
    renderer->drawThickLineLocal({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                                 {static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(br.x()), static_cast<float>(br.y())},
                                 {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 0.8f, innerColor);
    renderer->drawThickLineLocal({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                                 {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                                 0.8f, innerColor);
  }
}

bool tryDrawSvgPreview(ArtifactIRenderer* renderer,
                      ArtifactAbstractLayer* layerPtr)
{
  if (const auto svgLayer = dynamic_cast<ArtifactSvgLayer*>(layerPtr)) {
    if (svgLayer->isLoaded()) {
      svgLayer->draw(renderer);
      return true;
    }
  }
  return false;
}

bool tryDrawOtherLayerPreview(ArtifactIRenderer* renderer,
                              ArtifactAbstractLayer* layerPtr)
{
  if (layerPtr->isParticleLayer()) {
    layerPtr->draw(renderer);
    return true;
   }
  if (const auto formParticleLayer = dynamic_cast<ArtifactFormParticleLayer*>(layerPtr)) {
    formParticleLayer->draw(renderer);
    return true;
   }
  return false;
}
}
