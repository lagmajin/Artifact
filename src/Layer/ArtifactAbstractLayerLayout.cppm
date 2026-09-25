#include <algorithm>

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;

import Artifact.Composition.Abstract;
import Property.Abstract;

namespace Artifact {

LayerFieldChannelSample
ArtifactAbstractLayer::compositionFieldChannelsAtCanvasPoint(
    const QPointF& canvasPosition) const {
  const auto* composition =
      dynamic_cast<const ArtifactAbstractComposition*>(compositionObject());
  if (!composition) return {};
  const auto sample =
      composition->evaluateFieldChannelsAtCanvasPoint(id(), canvasPosition);
  return LayerFieldChannelSample{
      std::clamp(static_cast<float>(sample.weight), 0.0f, 1.0f),
      static_cast<float>(sample.scaleMultiplier),
      static_cast<float>(sample.timeOffsetSeconds), sample.affected};
}

QSizeF ArtifactAbstractLayer::compositionSizeHint() const {
  auto* composition =
      dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
  if (!composition) return {};
  const auto size = composition->settings().compositionSize();
  return QSizeF(size.width(), size.height());
}

bool applyResponsiveLayoutConstraints(
    const ArtifactAbstractLayer* layer, double& positionX, double& positionY,
    double& scaleX, double& scaleY, const double anchorX,
    const double anchorY, const bool componentEnabled,
    const bool responsiveEnabled, const int horizontalPinValue,
    const int verticalPinValue, const int scaleModeValue,
    const bool safeAreaEnabled, const double safeAreaPaddingX,
    const double safeAreaPaddingY, const double offsetX, const double offsetY) {
  if (!layer || layer->is3D() || !componentEnabled || !responsiveEnabled) {
    return false;
  }
  const QSizeF compositionSize = layer->compositionSizeHint();
  const QRectF localBounds = layer->localBounds();
  if (!compositionSize.isValid() || compositionSize.width() <= 0.0 ||
      compositionSize.height() <= 0.0 || !localBounds.isValid() ||
      localBounds.width() <= 0.0 || localBounds.height() <= 0.0) return false;
  const qreal paddingX = safeAreaEnabled
                             ? std::max<qreal>(0.0, safeAreaPaddingX)
                             : 0.0;
  const qreal paddingY = safeAreaEnabled
                             ? std::max<qreal>(0.0, safeAreaPaddingY)
                             : 0.0;
  const QRectF container(
      paddingX, paddingY,
      std::max<qreal>(0.0, compositionSize.width() - paddingX * 2.0),
      std::max<qreal>(0.0, compositionSize.height() - paddingY * 2.0));
  if (container.width() <= 0.0 || container.height() <= 0.0) return false;
  const int horizontalPin = std::clamp(horizontalPinValue, 0, 3);
  const int verticalPin = std::clamp(verticalPinValue, 0, 3);
  const int scaleMode = std::clamp(scaleModeValue, 0, 3);
  const qreal fitScaleX = container.width() / localBounds.width();
  const qreal fitScaleY = container.height() / localBounds.height();
  if (scaleMode == 1) {
    const qreal scale = std::min(fitScaleX, fitScaleY);
    scaleX = scale;
    scaleY = scale;
  } else if (scaleMode == 2) {
    const qreal scale = std::max(fitScaleX, fitScaleY);
    scaleX = scale;
    scaleY = scale;
  } else if (scaleMode == 3) {
    scaleX = fitScaleX;
    scaleY = fitScaleY;
  }
  if (horizontalPin == 3) scaleX = fitScaleX;
  if (verticalPin == 3) scaleY = fitScaleY;
  const qreal width = localBounds.width() * scaleX;
  const qreal height = localBounds.height() * scaleY;
  const auto positionForAxis = [](qreal start, qreal extent,
                                  qreal itemExtent, int pin) {
    if (pin == 1) return start + (extent - itemExtent) * 0.5;
    if (pin == 2) return start + extent - itemExtent;
    return start;
  };
  const qreal left = positionForAxis(container.left(), container.width(),
                                     width, horizontalPin);
  const qreal top = positionForAxis(container.top(), container.height(),
                                    height, verticalPin);
  positionX = left - scaleX * (localBounds.left() - anchorX) + offsetX;
  positionY = top - scaleY * (localBounds.top() - anchorY) + offsetY;
  return true;
}

} // namespace Artifact
