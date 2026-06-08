module;
#include <algorithm>
#include <QVariant>
#include <QStringList>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Timeline.KeyframeModel;

import Event.Bus;
import Artifact.Service.Project;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Time.Rational;

namespace Artifact {

W_OBJECT_IMPL(ArtifactTimelineKeyframeModel)

namespace {

QString editablePathDisplayLabel(const QString &propertyPath) {
  if (propertyPath.isEmpty()) {
    return {};
  }

  const auto parts = propertyPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return {};
  }

  const bool isMaskPath =
      parts.front().compare(QStringLiteral("mask"), Qt::CaseInsensitive) == 0;
  const bool isRotoPath =
      parts.front().compare(QStringLiteral("roto"), Qt::CaseInsensitive) == 0;
  if (!isMaskPath && !isRotoPath) {
    return {};
  }

  const QString rootLabel =
      isRotoPath ? QStringLiteral("Roto") : QStringLiteral("Mask");
  if (parts.size() == 1) {
    return rootLabel;
  }

  if (parts.size() == 3 &&
      parts[2].compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0) {
    const int maskIndex = parts[1].toInt();
    return QStringLiteral("%1 %2 / Enabled")
        .arg(rootLabel)
        .arg(maskIndex + 1);
  }

  if (parts.size() == 5 &&
      parts[2].compare(QStringLiteral("path"), Qt::CaseInsensitive) == 0) {
    const int maskIndex = parts[1].toInt();
    const int pathIndex = parts[3].toInt();
    const QString pathLabel = QStringLiteral("%1 %2 / Path %3")
                                  .arg(rootLabel)
                                  .arg(maskIndex + 1)
                                  .arg(pathIndex + 1);
    const QString field = parts[4];
    if (field.compare(QStringLiteral("closed"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Closed");
    }
    if (field.compare(QStringLiteral("opacity"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Opacity");
    }
    if (field.compare(QStringLiteral("feather"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Feather");
    }
    if (field.compare(QStringLiteral("expansion"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Expansion");
    }
    if (field.compare(QStringLiteral("inverted"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Inverted");
    }
    if (field.compare(QStringLiteral("mode"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Mode");
    }
    if (field.compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Name");
    }
  }

  return QStringLiteral("%1 / %2")
      .arg(rootLabel, propertyPath.mid(propertyPath.indexOf(QLatin1Char('.')) + 1));
}

} // namespace

QStringList ArtifactTimelineKeyframeModel::transformPropertyPaths() {
  return {
      QStringLiteral("transform.position.x"),
      QStringLiteral("transform.position.y"),
      QStringLiteral("transform.rotation"),
      QStringLiteral("transform.scale.x"),
      QStringLiteral("transform.scale.y"),
      QStringLiteral("transform.anchor.x"),
      QStringLiteral("transform.anchor.y"),
  };
}

bool ArtifactTimelineKeyframeModel::isTransformPropertyPath(
    const QString& propertyPath) {
  return transformPropertyPaths().contains(propertyPath);
}

QString ArtifactTimelineKeyframeModel::displayLabelForPropertyPath(
    QString propertyPath) {
  if (propertyPath.isNull() || propertyPath.isEmpty()) {
    return QStringLiteral("Property");
  }
  const QString editableLabel = editablePathDisplayLabel(propertyPath);
  if (!editableLabel.isEmpty()) {
    return editableLabel;
  }
  if (propertyPath == QStringLiteral("transform.position.x")) {
    return QStringLiteral("Transform / Position X");
  }
  if (propertyPath == QStringLiteral("transform.position.y")) {
    return QStringLiteral("Transform / Position Y");
  }
  if (propertyPath == QStringLiteral("transform.rotation")) {
    return QStringLiteral("Transform / Rotation");
  }
  if (propertyPath == QStringLiteral("transform.scale.x")) {
    return QStringLiteral("Transform / Scale X");
  }
  if (propertyPath == QStringLiteral("transform.scale.y")) {
    return QStringLiteral("Transform / Scale Y");
  }
  if (propertyPath == QStringLiteral("transform.anchor.x")) {
    return QStringLiteral("Transform / Anchor X");
  }
  if (propertyPath == QStringLiteral("transform.anchor.y")) {
    return QStringLiteral("Transform / Anchor Y");
  }

  QString fallback = propertyPath;
  fallback.replace(QLatin1Char('.'), QStringLiteral(" / "));
  return fallback;
}

ArtifactTimelineKeyframeModel::ArtifactTimelineKeyframeModel(QObject* parent)
    : QObject(parent) {}

ArtifactTimelineKeyframeModel::~ArtifactTimelineKeyframeModel() {}

std::vector<KeyFrame> ArtifactTimelineKeyframeModel::getKeyframesFor(
    const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath) const {
  auto* svc = ArtifactProjectService::instance();
  if (!svc) return {};
  auto result = svc->findComposition(compId);
  if (!result.success) return {};
  auto comp = result.ptr.lock();
  if (!comp) return {};
  auto layer = comp->layerById(layerId);
  if (!layer) return {};
  auto prop = layer->getProperty(propertyPath);
  if (!prop) return {};
  return prop->getKeyFrames();
}

bool ArtifactTimelineKeyframeModel::addKeyframe(const CompositionID& compId,
                                                const LayerID& layerId,
                                                const QString& propertyPath,
                                                const RationalTime& time,
                                                const QVariant& value,
                                                InterpolationType interpolation) {
  auto* svc = ArtifactProjectService::instance();
  if (!svc) return false;
  auto result = svc->findComposition(compId);
  if (!result.success) return false;
  auto comp = result.ptr.lock();
  if (!comp) return false;
  auto layer = comp->layerById(layerId);
  if (!layer) return false;
  auto prop = layer->getProperty(propertyPath);
  if (!prop) return false;

  prop->setAnimatable(true);
  prop->addKeyFrame(time, value, interpolation);

  layer->setDirty();
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compId.toString(), layerId.toString(),
                        LayerChangedEvent::ChangeType::Modified});
  return true;
}

bool ArtifactTimelineKeyframeModel::addKeyframeWithBezier(const CompositionID& compId,
                                                          const LayerID& layerId,
                                                          const QString& propertyPath,
                                                          const RationalTime& time,
                                                          const QVariant& value,
                                                          float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
  auto* svc = ArtifactProjectService::instance();
  if (!svc) return false;
  auto result = svc->findComposition(compId);
  if (!result.success) return false;
  auto comp = result.ptr.lock();
  if (!comp) return false;
  auto layer = comp->layerById(layerId);
  if (!layer) return false;
  auto prop = layer->getProperty(propertyPath);
  if (!prop) return false;

  prop->setAnimatable(true);
  prop->addKeyFrame(time, value, InterpolationType::Bezier, cp1_x, cp1_y, cp2_x, cp2_y, false);

  layer->setDirty();
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compId.toString(), layerId.toString(),
                        LayerChangedEvent::ChangeType::Modified});
  return true;
}

bool ArtifactTimelineKeyframeModel::moveKeyframe(const CompositionID& compId,
                                                 const LayerID& layerId,
                                                 const QString& propertyPath,
                                                 const RationalTime& fromTime,
                                                 const RationalTime& toTime) {
  auto* svc = ArtifactProjectService::instance();
  if (!svc) return false;
  auto result = svc->findComposition(compId);
  if (!result.success) return false;
  auto comp = result.ptr.lock();
  if (!comp) return false;
  auto layer = comp->layerById(layerId);
  if (!layer) return false;
  auto prop = layer->getProperty(propertyPath);
  if (!prop || !prop->isAnimatable()) return false;
  if (fromTime == toTime) return false;

  const auto keyframes = prop->getKeyFrames();
  const auto it = std::find_if(keyframes.begin(), keyframes.end(),
                               [&fromTime](const KeyFrame& kf) {
                                 return kf.time == fromTime;
                               });
  if (it == keyframes.end()) {
    return false;
  }

  const QVariant value = it->value;
  const InterpolationType interpolation = it->interpolation;
  const float cp1_x = it->cp1_x;
  const float cp1_y = it->cp1_y;
  const float cp2_x = it->cp2_x;
  const float cp2_y = it->cp2_y;
  const bool roving = it->roving;
  prop->removeKeyFrame(fromTime);
  prop->addKeyFrame(toTime, value, interpolation, cp1_x, cp1_y, cp2_x, cp2_y,
                    roving);

  layer->setDirty();
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compId.toString(), layerId.toString(),
                        LayerChangedEvent::ChangeType::Modified});
  return true;
}

bool ArtifactTimelineKeyframeModel::removeKeyframe(const CompositionID& compId,
                                                   const LayerID& layerId,
                                                   const QString& propertyPath,
                                                   const RationalTime& time) {
  auto* svc = ArtifactProjectService::instance();
  if (!svc) return false;
  auto result = svc->findComposition(compId);
  if (!result.success) return false;
  auto comp = result.ptr.lock();
  if (!comp) return false;
  auto layer = comp->layerById(layerId);
  if (!layer) return false;
  auto prop = layer->getProperty(propertyPath);
  if (!prop) return false;

  prop->removeKeyFrame(time);
  layer->setDirty();
  layer->changed();
  ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
      LayerChangedEvent{compId.toString(), layerId.toString(),
                        LayerChangedEvent::ChangeType::Modified});
  return true;
}

} // namespace Artifact
