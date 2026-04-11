module;
#include <QVariant>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Timeline.KeyframeModel;

import Artifact.Service.Project;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Time.Rational;

namespace Artifact {

W_OBJECT_IMPL(ArtifactTimelineKeyframeModel)

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
                                                EasingType easing) {
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

  // Ensure property is animatable
  prop->setAnimatable(true);
  prop->addKeyFrame(time, value, easing);

  // Notify layer/timeline
  layer->setDirty();
  layer->changed();
  Q_EMIT keyframesChanged(layerId, propertyPath);
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
  Q_EMIT keyframesChanged(layerId, propertyPath);
  return true;
}

} // namespace Artifact
