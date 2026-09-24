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

QPointF parentAutoLayoutOffset(const ArtifactAbstractLayer* layer,
                               const ArtifactAbstractLayerPtr& parent) {
  if (!layer || !parent) return {};
  const auto enabled = parent->getProperty(
      QStringLiteral("component.layout.enabled"));
  if (!enabled || !enabled->getValue().toBool()) return {};
  const auto participation = layer->getProperty(
      QStringLiteral("component.layout.mode"));
  if (participation && participation->getValue().toInt() == 2) return {};
  auto* composition = static_cast<ArtifactAbstractComposition*>(
      parent->composition());
  if (!composition) return {};
  const auto direction = parent->getProperty(
      QStringLiteral("component.layout.stackDirection"));
  const auto alignmentProperty = parent->getProperty(
      QStringLiteral("component.layout.anchorMode"));
  const auto gapProperty = parent->getProperty(QStringLiteral("component.layout.gap"));
  const auto paddingXProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaPaddingX"));
  const auto paddingYProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaPaddingY"));
  const auto safeAreaEnabledProperty = parent->getProperty(
      QStringLiteral("component.layout.safeAreaEnabled"));
  const bool vertical = direction && direction->getValue().toInt() != 0;
  const int alignment = alignmentProperty
                            ? std::clamp(alignmentProperty->getValue().toInt(), 0, 2)
                            : 0;
  const qreal gap = gapProperty ? gapProperty->getValue().toDouble() : 0.0;
  const bool usePadding = safeAreaEnabledProperty &&
                          safeAreaEnabledProperty->getValue().toBool();
  const QRectF parentBounds = parent->localBounds();
  const qreal paddingX = usePadding && paddingXProperty
                             ? paddingXProperty->getValue().toDouble() : 0.0;
  const qreal paddingY = usePadding && paddingYProperty
                             ? paddingYProperty->getValue().toDouble() : 0.0;
  qreal cursorX = parentBounds.left() + paddingX;
  qreal cursorY = parentBounds.top() + paddingY;
  const auto siblings = composition->childLayersOf(parent->id());
  for (const auto& sibling : siblings) {
    if (!sibling) continue;
    const auto siblingParticipation = sibling->getProperty(
        QStringLiteral("component.layout.mode"));
    if (siblingParticipation && siblingParticipation->getValue().toInt() == 2) {
      continue;
    }
    const QRectF bounds = sibling->visualLocalBounds();
    if (sibling.get() == layer) {
      qreal targetX = cursorX - bounds.left();
      qreal targetY = cursorY - bounds.top();
      if (vertical) {
        const qreal availableWidth = std::max<qreal>(
            0.0, parentBounds.width() - paddingX * 2.0);
        if (alignment == 1) {
          targetX = parentBounds.left() + paddingX +
                    (availableWidth - bounds.width()) * 0.5 - bounds.left();
        } else if (alignment == 2) {
          targetX = parentBounds.right() - paddingX - bounds.width() - bounds.left();
        }
      } else {
        const qreal availableHeight = std::max<qreal>(
            0.0, parentBounds.height() - paddingY * 2.0);
        if (alignment == 1) {
          targetY = parentBounds.top() + paddingY +
                    (availableHeight - bounds.height()) * 0.5 - bounds.top();
        } else if (alignment == 2) {
          targetY = parentBounds.bottom() - paddingY - bounds.height() - bounds.top();
        }
      }
      return QPointF(targetX, targetY);
    }
    if (vertical) {
      cursorY += std::max<qreal>(0.0, bounds.height()) + gap;
    } else {
      cursorX += std::max<qreal>(0.0, bounds.width()) + gap;
    }
  }
  return {};
}

} // namespace Artifact
