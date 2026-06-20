module;
#include <algorithm>
#include <memory>
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
    if (field.compare(QStringLiteral("featherHorizontal"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Feather H");
    }
    if (field.compare(QStringLiteral("featherVertical"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Feather V");
    }
    if (field.compare(QStringLiteral("featherInner"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Feather Inner");
    }
    if (field.compare(QStringLiteral("featherOuter"), Qt::CaseInsensitive) == 0) {
      return pathLabel + QStringLiteral(" / Feather Outer");
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

  if (parts.size() >= 3 &&
      parts[0].compare(QStringLiteral("text"), Qt::CaseInsensitive) == 0 &&
      parts[1].compare(QStringLiteral("animators"), Qt::CaseInsensitive) == 0) {
    const int animatorIndex = parts[2].toInt();
    if (parts.size() == 3) {
      return QStringLiteral("Text Animator %1")
          .arg(animatorIndex + 1);
    }
    if (parts.size() < 4) {
      return QStringLiteral("Text Animator %1")
          .arg(animatorIndex + 1);
    }
    const QString field = parts[3];
    QString fieldLabel = field;
    if (!fieldLabel.isEmpty()) {
      fieldLabel[0] = fieldLabel[0].toUpper();
    }
    return QStringLiteral("Text Animator %1 / %2")
        .arg(animatorIndex + 1)
        .arg(fieldLabel);
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
  if (propertyPath == QStringLiteral("text.value")) {
    return QStringLiteral("Source Text");
  }
  if (propertyPath == QStringLiteral("text.animatorCount")) {
    return QStringLiteral("Text Animators / Count");
  }
  if (propertyPath == QStringLiteral("text.animatorPreset")) {
    return QStringLiteral("Text Animators / Preset");
  }

  QString fallback = propertyPath;
  fallback.replace(QLatin1Char('.'), QStringLiteral(" / "));
  return fallback;
}

ArtifactTimelineKeyframeModel::ArtifactTimelineKeyframeModel(QObject* parent)
    : QObject(parent) {}

ArtifactTimelineKeyframeModel::~ArtifactTimelineKeyframeModel() {}

namespace {
struct LayerPropertyLookup {
    std::shared_ptr<ArtifactCore::AbstractProperty> prop;
    ArtifactAbstractLayerPtr layer;
    bool success = false;
};

LayerPropertyLookup resolveLayerProperty(
    const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath) {
    LayerPropertyLookup result;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) return result;
    auto findResult = svc->findComposition(compId);
    if (!findResult.success) return result;
    auto comp = findResult.ptr.lock();
    if (!comp) return result;
    result.layer = comp->layerById(layerId);
    if (!result.layer) return result;
    result.prop = result.layer->getProperty(propertyPath);
    if (!result.prop) return result;
    result.success = true;
    return result;
}

void notifyLayerChanged(const ArtifactAbstractLayerPtr& layer, const CompositionID& compId, const LayerID& layerId) {
    if (layer) {
        layer->setDirty();
        layer->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{compId.toString(), layerId.toString(),
                             LayerChangedEvent::ChangeType::Modified});
    }
}
} // namespace

std::vector<KeyFrame> ArtifactTimelineKeyframeModel::getKeyframesFor(
    const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath) const {
    if (auto lookup = resolveLayerProperty(compId, layerId, propertyPath); lookup.success) {
        return lookup.prop->getKeyFrames();
    }
    return {};
}

bool ArtifactTimelineKeyframeModel::addKeyframe(const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath,
    const RationalTime& time,
    const QVariant& value,
    InterpolationType interpolation) {
    if (auto lookup = resolveLayerProperty(compId, layerId, propertyPath); lookup.success) {
        lookup.prop->setAnimatable(true);
        lookup.prop->addKeyFrame(time, value, interpolation);
        return true;
    }
    return false;
}

bool ArtifactTimelineKeyframeModel::addKeyframeWithBezier(const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath,
    const RationalTime& time,
    const QVariant& value,
    float cp1_x, float cp1_y, float cp2_x, float cp2_y) {
    if (auto lookup = resolveLayerProperty(compId, layerId, propertyPath); lookup.success) {
        lookup.prop->setAnimatable(true);
        lookup.prop->addKeyFrame(time, value, InterpolationType::Bezier, cp1_x, cp1_y, cp2_x, cp2_y, false);
        notifyLayerChanged(lookup.layer, compId, layerId);
        return true;
    }
    return false;
}

bool ArtifactTimelineKeyframeModel::moveKeyframe(const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath,
    const RationalTime& fromTime,
    const RationalTime& toTime) {
    if (auto lookup = resolveLayerProperty(compId, layerId, propertyPath); lookup.success) {
        if (!lookup.prop->isAnimatable() || fromTime == toTime) return false;
        const auto keyframes = lookup.prop->getKeyFrames();
        const auto it = std::find_if(keyframes.begin(), keyframes.end(),
            [&fromTime](const KeyFrame& kf) { return kf.time == fromTime; });
        if (it == keyframes.end()) return false;
        lookup.prop->removeKeyFrame(fromTime);
        lookup.prop->addKeyFrame(toTime, it->value, it->interpolation, it->cp1_x, it->cp1_y, it->cp2_x, it->cp2_y, it->roving);
        notifyLayerChanged(lookup.layer, compId, layerId);
        return true;
    }
    return false;
}

bool ArtifactTimelineKeyframeModel::removeKeyframe(const CompositionID& compId,
    const LayerID& layerId,
    const QString& propertyPath,
    const RationalTime& time) {
    if (auto lookup = resolveLayerProperty(compId, layerId, propertyPath); lookup.success) {
        lookup.prop->removeKeyFrame(time);
        notifyLayerChanged(lookup.layer, compId, layerId);
        return true;
    }
return false;
}

} // namespace Artifact
