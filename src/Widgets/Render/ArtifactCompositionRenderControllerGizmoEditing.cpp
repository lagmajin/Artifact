module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <QApplication>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVariant>
#include <QVector>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController:GizmoEditing;

import Animation.Transform3D;
import Application.AppSettings;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionGizmoUndoCommands;
import Color.Float;
import Frame.Rate;
import Math.Vec;
import Property.Abstract;
import Settings.Accessibility;
import Time.Rational;
import Utils.Id;

namespace Artifact {

ArtifactCore::RationalTime gizmoTransformTime(
    const ArtifactAbstractLayerPtr &layer, int64_t frame) {
  double fps = 24.0;
  if (layer) {
    if (auto *composition = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      const double candidate = composition->frameRate().framerate();
      if (candidate > 0.0) fps = candidate;
    }
  }
  // Transform keys live in the layer's own frame domain (pinned to the
  // composition frame rate when the layer joins the composition). Deriving
  // the scale from the composition fps again would let a key that exists on
  // the current frame fall outside the lookup whenever the two roundings
  // differ, so a drag on a keyed frame would silently rewrite the initial
  // value instead of the key.
  return layer ? layer->keyframeTimeAtFrame(frame)
               : ArtifactCore::RationalTime(
                     frame, ArtifactCore::FrameRate::storageScaleForFps(fps, 24));
}

float layerLocalPositionDeltaLengthSquared(
    ArtifactCore::Coordinates::LayerLocalPoint3 lhs,
    ArtifactCore::Coordinates::LayerLocalPoint3 rhs) {
  const auto delta = lhs - rhs;
  return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
}

bool layerLocalPositionsEqual(
    ArtifactCore::Coordinates::LayerLocalPoint3 lhs,
    ArtifactCore::Coordinates::LayerLocalPoint3 rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

float eulerDegreesDeltaLengthSquared(
    ArtifactCore::Units::EulerDegrees3 lhs,
    ArtifactCore::Units::EulerDegrees3 rhs) {
  const float x = lhs.x.value - rhs.x.value;
  const float y = lhs.y.value - rhs.y.value;
  const float z = lhs.z.value - rhs.z.value;
  return x * x + y * y + z * z;
}

float scale3DeltaLengthSquared(ArtifactCore::Units::Scale3 lhs,
                               ArtifactCore::Units::Scale3 rhs) {
  const float x = lhs.x - rhs.x;
  const float y = lhs.y - rhs.y;
  const float z = lhs.z - rhs.z;
  return x * x + y * y + z * z;
}

bool eulerDegreesEqual(ArtifactCore::Units::EulerDegrees3 lhs,
                       ArtifactCore::Units::EulerDegrees3 rhs) {
  return lhs.x.value == rhs.x.value && lhs.y.value == rhs.y.value &&
         lhs.z.value == rhs.z.value;
}

bool scale3Equal(ArtifactCore::Units::Scale3 lhs,
                 ArtifactCore::Units::Scale3 rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

void captureGizmoKeyState(const ArtifactAbstractLayerPtr &layer, int64_t frame,
                          GizmoTransformSnapshot &snapshot) {
  if (!layer) return;
  const auto &transform = layer->transform3D();
  const auto time = gizmoTransformTime(layer, frame);
  snapshot.hasPositionKey = transform.hasPositionKeyFrameAt(time);
  snapshot.hasRotationKey = transform.hasRotationKeyFrameAt(time);
  snapshot.hasScaleKey = transform.hasScaleKeyFrameAt(time);
  snapshot.positionAnimated = transform.getPositionKeyFrameCount() > 0;
  snapshot.rotationAnimated = transform.getRotationKeyFrameCount() > 0;
  snapshot.scaleAnimated = transform.getScaleKeyFrameCount() > 0;
  captureGizmoPropertyKeys(layer, time, snapshot);
  // The drag must start from the same evaluated values as the inspector and
  // renderer, including keys authored only through the timeline.
  for (const auto &saved : snapshot.properties) {
    const auto property = layer->getProperty(saved.path);
    if (!property) continue;
    bool ok = false;
    const float value = property->interpolateValue(time).toFloat(&ok);
    if (!ok || !std::isfinite(value)) continue;
    if (saved.path == QStringLiteral("transform.position.x")) snapshot.position.x = value;
    else if (saved.path == QStringLiteral("transform.position.y")) snapshot.position.y = value;
    else if (saved.path == QStringLiteral("transform.position.z")) snapshot.position.z = value;
    else if (saved.path == QStringLiteral("transform.scale.x")) snapshot.scale.x = value;
    else if (saved.path == QStringLiteral("transform.scale.y")) snapshot.scale.y = value;
    else if (saved.path == QStringLiteral("transform.scale.z")) snapshot.scale.z = value;
    else if (saved.path == QStringLiteral("transform.rotation")) snapshot.rotation.z = {value};
    else if (saved.path == QStringLiteral("transform.rotation.x")) snapshot.rotation.x = {value};
    else if (saved.path == QStringLiteral("transform.rotation.y")) snapshot.rotation.y = {value};
  }
}

void applyPlanarGizmoTransform(const ArtifactAbstractLayerPtr &layer,
                               int64_t frame,
                               const GizmoTransformSnapshot &snapshot) {
  if (!layer) {
    return;
  }

  restoreGizmoPropertyKeys(layer, gizmoTransformTime(layer, frame), snapshot);
}

bool gizmoAutoKeyApplies(const ArtifactAbstractLayerPtr &layer,
                         const QString &propertyPrefix) {
  const auto *settings = ArtifactCore::ArtifactAppSettings::instance();
  if (!settings || !settings->timelineAutoKeyEnabled() || !layer) {
    return false;
  }
  const QString scope = settings->timelineAutoKeyScopeText();
  if (scope.compare(QStringLiteral("Current Layer"), Qt::CaseInsensitive) == 0) {
    const auto *selection = ArtifactLayerSelectionManager::instance();
    if (!selection || selection->currentLayer() != layer) return false;
  } else if (scope.compare(QStringLiteral("Selected Layers"),
                           Qt::CaseInsensitive) == 0) {
    const auto *selection = ArtifactLayerSelectionManager::instance();
    if (!selection || !selection->isSelected(layer)) return false;
  }
  if (settings->timelineKeyingSetModeText().compare(
          QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
    const QStringList paths =
        settings->timelineCustomKeyingSetPropertyPaths();
    if (!paths.isEmpty()) {
      const bool transformAllowed = std::any_of(
          paths.cbegin(), paths.cend(), [&propertyPrefix](const QString &path) {
            return path.compare(propertyPrefix, Qt::CaseInsensitive) == 0 ||
                   path.startsWith(propertyPrefix + QLatin1Char('.'),
                                   Qt::CaseInsensitive);
          });
      if (!transformAllowed) return false;
    }
  }
  return true;
}

void applyLiveGizmoTransform(const ArtifactAbstractLayerPtr &layer,
                             int64_t frame,
                             const GizmoTransformSnapshot &before,
                             const GizmoTransformSnapshot &current) {
  if (!layer) return;
  const auto time = gizmoTransformTime(layer, frame);
  // A drag is evaluated from its start snapshot on every update, so moving
  // back to the start removes keys introduced by an earlier mouse move.
  restoreGizmoPropertyKeys(layer, time, before);
  const bool autoKeyPosition = gizmoAutoKeyApplies(
      layer, QStringLiteral("transform.position"));
  const bool autoKeyRotation = gizmoAutoKeyApplies(
      layer, QStringLiteral("transform.rotation"));
  const bool autoKeyScale = gizmoAutoKeyApplies(
      layer, QStringLiteral("transform.scale"));

  const bool positionChanged =
      layerLocalPositionDeltaLengthSquared(current.position, before.position) >
      0.000001f;
  const bool rotationChanged =
      eulerDegreesDeltaLengthSquared(current.rotation, before.rotation) >
      0.000001f;
  const bool scaleChanged =
      scale3DeltaLengthSquared(current.scale, before.scale) > 0.000001f;
  const float currentRotation = current.rotation.z.value;
  const auto syncProperty = [&](const QString &path, const QVariant &value,
                                bool keyed) {
    const auto property = layer->getProperty(path);
    if (!property || !property->isAnimatable()) return;
    const auto old = property->interpolateValue(time);
    if (old.isValid() && std::abs(old.toDouble() - value.toDouble()) <= 0.000001) return;
    if (keyed) {
      // Preserve interpolation/labels when editing an existing property key.
      for (const auto &saved : before.properties) {
        if (saved.path == path && saved.hasKey) {
          const auto &key = saved.key;
          property->addKeyFrame(time, value, key.interpolation,
                                key.cp1_x, key.cp1_y, key.cp2_x, key.cp2_y,
                                key.roving);
          property->setKeyFrameAnchorAt(time, key.anchor);
          property->setKeyFrameColorLabelAt(time, key.colorLabel);
          property->setKeyFrameSoftAt(time, key.soft);
          return;
        }
      }
      property->addKeyFrame(time, value);
    } else {
      property->setValue(value);
    }
  };
  // Each axis owns its keying state. Shared native storage is not a reason to
  // key an unanimated sibling when the timeline has keyed only X or Y.
  const auto positionChannelKeyed = [&](const QString &path) {
    return before.propertyAnimated(path) || autoKeyPosition;
  };
  const auto scaleChannelKeyed = [&](const QString &path) {
    return before.propertyAnimated(path) || autoKeyScale;
  };
  if (positionChanged) {
    syncProperty(QStringLiteral("transform.position.x"),
                 current.position.x, positionChannelKeyed(QStringLiteral("transform.position.x")));
    syncProperty(QStringLiteral("transform.position.y"),
                 current.position.y, positionChannelKeyed(QStringLiteral("transform.position.y")));
    if (current.is3D) {
      syncProperty(QStringLiteral("transform.position.z"),
                   current.position.z, positionChannelKeyed(QStringLiteral("transform.position.z")));
    }
  }
  if (rotationChanged) {
    syncProperty(QStringLiteral("transform.rotation"), currentRotation,
                 before.propertyAnimated(QStringLiteral("transform.rotation")) || autoKeyRotation);
    if (current.is3D) {
      syncProperty(QStringLiteral("transform.rotation.x"), current.rotation.x.value,
          before.propertyAnimated(QStringLiteral("transform.rotation.x")) || autoKeyRotation);
      syncProperty(QStringLiteral("transform.rotation.y"), current.rotation.y.value,
          before.propertyAnimated(QStringLiteral("transform.rotation.y")) || autoKeyRotation);
    }
  }
  if (scaleChanged) {
    syncProperty(QStringLiteral("transform.scale.x"), current.scale.x,
                 scaleChannelKeyed(QStringLiteral("transform.scale.x")));
    syncProperty(QStringLiteral("transform.scale.y"), current.scale.y,
                 scaleChannelKeyed(QStringLiteral("transform.scale.y")));
    if (current.is3D) {
      syncProperty(QStringLiteral("transform.scale.z"), current.scale.z,
                   scaleChannelKeyed(QStringLiteral("transform.scale.z")));
    }
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
}

void restoreGizmoPropertyKeyState(const ArtifactAbstractLayerPtr &layer,
                                  int64_t frame,
                                  const GizmoTransformSnapshot &snapshot) {
  restoreGizmoPropertyKeys(layer, gizmoTransformTime(layer, frame), snapshot);
}

enum class TransformFieldDragMode { None, Center, Radius, SecondaryRadius };

bool fieldTargetsSelectedLayers(const CompositionTransformField &field,
                               const QSet<LayerID> &selectedLayerIds)
{
  if (selectedLayerIds.isEmpty()) {
    return false;
  }
  for (const auto &targetLayerId : field.targetLayerIds) {
    if (selectedLayerIds.contains(targetLayerId)) {
      return true;
    }
  }
  return false;
}



QPointF transformFieldDisplayCenter(const ArtifactCompositionPtr &comp,
                                   const CompositionTransformField &field)
{
  QPointF displayCenter = field.center;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return displayCenter;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    displayCenter = parentLayer->getGlobalTransform().map(displayCenter);
  }
  return displayCenter;
}



QPointF transformFieldDisplayRadiusPoint(const ArtifactCompositionPtr &comp,
                                         const CompositionTransformField &field)
{
  const qreal angleRadians =
      field.rotationDegrees * (std::acos(-1.0) / 180.0);
  const QPointF radiusDirection = field.shape == QStringLiteral("linear")
                                      ? QPointF(std::cos(angleRadians),
                                                std::sin(angleRadians))
                                      : QPointF(1.0, 0.0);
  const QPointF radiusPoint = field.center + radiusDirection * field.radius;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return radiusPoint;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    return parentLayer->getGlobalTransform().map(radiusPoint);
  }
  return radiusPoint;
}

QPointF transformFieldDisplaySecondaryRadiusPoint(
    const ArtifactCompositionPtr &comp, const CompositionTransformField &field)
{
  const QPointF radiusPoint =
      field.center + QPointF(0.0, field.secondaryRadius);
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return radiusPoint;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    return parentLayer->getGlobalTransform().map(radiusPoint);
  }
  return radiusPoint;
}

std::array<QPointF, 4> transformFieldDisplayBoxCorners(
    const ArtifactCompositionPtr &comp, const CompositionTransformField &field)
{
  std::array<QPointF, 4> corners = {
      field.center + QPointF(-field.radius, -field.secondaryRadius),
      field.center + QPointF(field.radius, -field.secondaryRadius),
      field.center + QPointF(field.radius, field.secondaryRadius),
      field.center + QPointF(-field.radius, field.secondaryRadius),
  };
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return corners;
  }
  if (const auto parentLayer = comp->layerById(field.coordinateParentLayerId)) {
    const QTransform transform = parentLayer->getGlobalTransform();
    for (auto &corner : corners) {
      corner = transform.map(corner);
    }
  }
  return corners;
}



bool transformFieldLocalPointFromCanvas(const ArtifactCompositionPtr &comp,
                                        const CompositionTransformField &field,
                                        const QPointF &canvasPoint,
                                        QPointF &outLocalPoint)
{
  outLocalPoint = canvasPoint;
  if (!comp || field.coordinateParentLayerId.isNil()) {
    return true;
  }
  const auto parentLayer = comp->layerById(field.coordinateParentLayerId);
  if (!parentLayer) {
    return true;
  }
  bool invertible = false;
  const QTransform invTransform = parentLayer->getGlobalTransform().inverted(&invertible);
  if (!invertible) {
    return false;
  }
  outLocalPoint = invTransform.map(canvasPoint);
  return true;
}



bool hitTestTransformFieldHandle(
    const ArtifactCompositionPtr &comp, const QPointF &canvasPos,
    const QSet<LayerID> &selectedLayerIds, qreal hitThreshold,
    QString &outFieldId, TransformFieldDragMode &outMode)
{
  outFieldId.clear();
  outMode = TransformFieldDragMode::None;
  if (!comp) {
    return false;
  }

  qreal bestDistance = std::numeric_limits<qreal>::max();

  for (const auto &field : comp->transformFields()) {
    if (!field.enabled || !fieldTargetsSelectedLayers(field, selectedLayerIds)) {
      continue;
    }

    const QPointF displayCenter = transformFieldDisplayCenter(comp, field);
    const QPointF displayRadiusPoint = transformFieldDisplayRadiusPoint(comp, field);
    const QPointF displaySecondaryRadiusPoint =
        transformFieldDisplaySecondaryRadiusPoint(comp, field);

    const qreal centerDistance = std::hypot(canvasPos.x() - displayCenter.x(),
                                            canvasPos.y() - displayCenter.y());
    if (centerDistance <= hitThreshold && centerDistance < bestDistance) {
      bestDistance = centerDistance;
      outFieldId = field.fieldId;
      outMode = TransformFieldDragMode::Center;
    }

    const qreal radiusDistance = std::hypot(
        canvasPos.x() - displayRadiusPoint.x(),
        canvasPos.y() - displayRadiusPoint.y());
    if (radiusDistance <= hitThreshold && radiusDistance < bestDistance) {
      bestDistance = radiusDistance;
      outFieldId = field.fieldId;
      outMode = TransformFieldDragMode::Radius;
    }

    if (field.shape == QStringLiteral("box")) {
      const qreal secondaryRadiusDistance = std::hypot(
          canvasPos.x() - displaySecondaryRadiusPoint.x(),
          canvasPos.y() - displaySecondaryRadiusPoint.y());
      if (secondaryRadiusDistance <= hitThreshold &&
          secondaryRadiusDistance < bestDistance) {
        bestDistance = secondaryRadiusDistance;
        outFieldId = field.fieldId;
        outMode = TransformFieldDragMode::SecondaryRadius;
      }
    }
  }

  return !outFieldId.isEmpty() && outMode != TransformFieldDragMode::None;
}



void drawTransformFieldBadge(ArtifactIRenderer *renderer,
                             const CompositionTransformField &field,
                             const QPointF &displayCenter,
                             const QPointF &displayRadiusPoint,
                             bool isHoveredField, bool isDraggingField,
                             bool isActiveField, float inverseZoom)
{
  if (!renderer || !isActiveField) {
    return;
  }

  const QString badgeTitle = field.displayName.trimmed().isEmpty()
                                 ? QStringLiteral("Live Field")
                                 : field.displayName.trimmed();
  const QString blendMode = field.blendMode.trimmed().toLower();
  const QString blendLabel =
      blendMode == QStringLiteral("additive")
          ? QStringLiteral("Additive")
          : blendMode == QStringLiteral("multiply")
                ? QStringLiteral("Multiply")
                : blendMode == QStringLiteral("screen")
                      ? QStringLiteral("Screen")
                      : QStringLiteral("Normal");
  const QString shapeLabel = field.shape == QStringLiteral("box")
                                 ? QStringLiteral("BOX")
                                 : field.shape == QStringLiteral("linear")
                                       ? QStringLiteral("LINEAR")
                                       : QStringLiteral("RADIAL");
  const QString badgeSubtitle = QStringLiteral("%1 / %2 / %3 / %4")
                                   .arg(QStringLiteral("ACTIVE"), shapeLabel, blendLabel,
                                        field.enabled ? QStringLiteral("enabled")
                                                      : QStringLiteral("disabled"));
  const QString badgeDetails = QStringLiteral("%1 / %2 / scale %3 / time %4s")
                                   .arg(QStringLiteral("strength %1")
                                            .arg(QString::number(field.strength, 'f', 1)),
                                         field.invert ? QStringLiteral("invert on")
                                                     : QStringLiteral("invert off"),
                                         QString::number(field.edgeScale, 'f', 2),
                                         QString::number(field.timeOffsetSeconds, 'f', 2));

  const QPointF anchor =
      QPointF((displayCenter.x() + displayRadiusPoint.x()) * 0.5,
              (displayCenter.y() + displayRadiusPoint.y()) * 0.5);
  const double accessibilityFontScale =
      std::max(1.0f, Accessibility::fontScale());
  const float panelW =
      std::clamp(160.0f + static_cast<float>(badgeTitle.size()) * 4.5f, 160.0f,
                 320.0f);
  const float panelH = 54.0f * static_cast<float>(accessibilityFontScale);
  const float panelX = static_cast<float>(anchor.x()) + 12.0f;
  const float panelY = static_cast<float>(anchor.y()) - 10.0f - panelH;

  const FloatColor panelFill =
      isDraggingField
          ? FloatColor{0.16f, 0.10f, 0.05f, 0.88f}
          : isHoveredField
                ? FloatColor{0.10f, 0.12f, 0.08f, 0.86f}
                : FloatColor{0.05f, 0.12f, 0.08f, 0.82f};
  const FloatColor panelBorder =
      isDraggingField
          ? FloatColor{1.0f, 0.55f, 0.18f, 0.96f}
          : isHoveredField ? FloatColor{1.0f, 0.83f, 0.32f, 0.94f}
                           : FloatColor{0.38f, 0.95f, 0.63f, 0.94f};
  const float contrastScale = Accessibility::contrastScale();

  renderer->drawOverlayPanel(panelX, panelY, panelW, panelH, panelFill,
                             panelBorder);

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(
      std::max(9.0, static_cast<double>(titleFont.pointSizeF()) *
                          accessibilityFontScale));
  titleFont.setWeight(QFont::DemiBold);
  QFont detailFont = QApplication::font();
  detailFont.setPointSizeF(
      std::max(7.5, (static_cast<double>(detailFont.pointSizeF()) - 0.5) *
                        accessibilityFontScale));

  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 6.0f, panelW - 20.0f, 16.0f),
      badgeTitle, titleFont, FloatColor{0.98f, 0.99f, 1.0f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 22.0f, panelW - 20.0f, 14.0f),
      badgeSubtitle, detailFont, FloatColor{0.88f, 0.94f, 0.91f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);
  renderer->drawText(
      QRectF(panelX + 10.0f, panelY + 36.0f, panelW - 20.0f, 12.0f),
      badgeDetails, detailFont, FloatColor{0.80f, 0.88f, 0.84f, 1.0f},
      Qt::AlignLeft | Qt::AlignVCenter);

  const float tagSize = std::max(8.0f, 10.0f * inverseZoom);
  renderer->drawCircle(static_cast<float>(displayCenter.x()),
                       static_cast<float>(displayCenter.y()),
                       tagSize * 1.4f, panelBorder, 1.4f * contrastScale, true);
}

} // namespace Artifact
