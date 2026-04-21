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
    const QString& propertyPath) {
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
  prop->addKeyFrame(time, value, InterpolationType::Bezier, cp1_x, cp1_y, cp2_x, cp2_y);

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
  prop->removeKeyFrame(fromTime);
  prop->addKeyFrame(toTime, value, interpolation, cp1_x, cp1_y, cp2_x, cp2_y);

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
