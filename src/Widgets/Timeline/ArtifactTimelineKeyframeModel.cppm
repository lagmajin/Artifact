module;
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <QStringList>
#include <QHash>
#include <QVariant>
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

bool ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
    const QString& groupName) {
  return isTimelineHiddenLayerPropertyGroup(groupName);
}

bool ArtifactTimelineKeyframeModel::isTimelinePropertyGroupExpandedByDefault(
    const QString& groupName) {
  return isTimelineExpandedByDefaultLayerPropertyGroup(groupName);
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

QString dopeSheetPropertyKey(const LayerID& layerId, const QString& propertyPath) {
    return QStringLiteral("%1|%2").arg(layerId.toString(), propertyPath);
}

std::shared_ptr<ArtifactCore::AbstractProperty> findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr& layer,
    const QString& propertyPath) {
    if (!layer) {
        return {};
    }

    for (const auto& group : layer->getLayerPropertyGroups()) {
        if (ArtifactTimelineKeyframeModel::shouldHideTimelinePropertyGroup(
                group.name())) {
            continue;
        }
        if (const auto property = group.findProperty(propertyPath)) {
            return property;
        }
    }
    return {};
}

void restorePropertyKeyframes(const std::shared_ptr<ArtifactCore::AbstractProperty>& property,
                              const std::vector<KeyFrame>& keyframes) {
    if (!property) {
        return;
    }

    property->clearKeyFrames();
    for (const auto& keyframe : keyframes) {
        property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation,
                              keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x,
                              keyframe.cp2_y, keyframe.roving);
        property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
        property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
    }
}

bool applyDopeSheetTransform(const CompositionID& compId,
                             const std::vector<DopeSheetKeyframeRef>& refs,
                             const std::function<RationalTime(const RationalTime&)>& remapTime) {
    if (refs.empty()) {
        return false;
    }

    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return false;
    }
    const auto findResult = svc->findComposition(compId);
    if (!findResult.success) {
        return false;
    }
    const auto composition = findResult.ptr.lock();
    if (!composition) {
        return false;
    }

    QHash<QString, QVector<RationalTime>> selectedTimesByProperty;
    QVector<QString> propertyOrder;
    for (const auto& ref : refs) {
        if (!ref.isValid()) {
            continue;
        }
        const QString key = dopeSheetPropertyKey(ref.layerId, ref.propertyPath);
        if (!selectedTimesByProperty.contains(key)) {
            propertyOrder.push_back(key);
        }
        selectedTimesByProperty[key].push_back(ref.time);
    }

    bool changed = false;
    QHash<QString, ArtifactAbstractLayerPtr> changedLayers;

    for (const auto& propertyKey : propertyOrder) {
        const int sep = propertyKey.indexOf(QLatin1Char('|'));
        if (sep <= 0) {
            continue;
        }

        const LayerID layerId(propertyKey.left(sep));
        const QString propertyPath = propertyKey.mid(sep + 1);
        const auto layer = composition->layerById(layerId);
        if (!layer) {
            continue;
        }
        const auto property = findLayerPropertyByPath(layer, propertyPath);
        if (!property || !property->isAnimatable()) {
            continue;
        }

        const auto originalKeyframes = property->getKeyFrames();
        if (originalKeyframes.empty()) {
            continue;
        }

        const auto& selectedTimes = selectedTimesByProperty[propertyKey];
        std::vector<KeyFrame> rebuilt;
        rebuilt.reserve(originalKeyframes.size());
        bool propertyChanged = false;

        for (const auto& keyframe : originalKeyframes) {
            const bool isSelected = std::any_of(
                selectedTimes.begin(), selectedTimes.end(),
                [&keyframe](const RationalTime& selectedTime) {
                    return keyframe.time == selectedTime;
                });
            if (!isSelected) {
                rebuilt.push_back(keyframe);
                continue;
            }

            KeyFrame moved = keyframe;
            moved.time = remapTime(keyframe.time);
            rebuilt.push_back(std::move(moved));
            propertyChanged = true;
        }

        if (!propertyChanged) {
            continue;
        }

        std::stable_sort(rebuilt.begin(), rebuilt.end(),
                         [](const KeyFrame& lhs, const KeyFrame& rhs) {
                             return lhs.time < rhs.time;
                         });

        std::vector<KeyFrame> deduped;
        deduped.reserve(rebuilt.size());
        for (const auto& keyframe : rebuilt) {
            if (!deduped.empty() && deduped.back().time == keyframe.time) {
                deduped.back() = keyframe;
                continue;
            }
            deduped.push_back(keyframe);
        }

        restorePropertyKeyframes(property, deduped);
        changedLayers.insert(layer->id().toString(), layer);
        changed = true;
    }

    for (auto it = changedLayers.begin(); it != changedLayers.end(); ++it) {
        const auto& layer = it.value();
        if (!layer) {
            continue;
        }
        notifyLayerChanged(layer, compId, layer->id());
    }

    return changed;
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

std::vector<DopeSheetKeyframeEntry>
ArtifactTimelineKeyframeModel::collectDopeSheetKeyframesForLayer(
    const CompositionID& compId,
    const LayerID& layerId) const {
    std::vector<DopeSheetKeyframeEntry> result;
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return result;
    }

    const auto findResult = svc->findComposition(compId);
    if (!findResult.success) {
        return result;
    }

    const auto composition = findResult.ptr.lock();
    if (!composition) {
        return result;
    }

    const auto layer = composition->layerById(layerId);
    if (!layer) {
        return result;
    }

    for (const auto& group : layer->getLayerPropertyGroups()) {
        if (shouldHideTimelinePropertyGroup(group.name())) {
            continue;
        }
        for (const auto& property : group.sortedProperties()) {
            if (!property || !property->isAnimatable()) {
                continue;
            }
            for (const auto& keyframe : property->getKeyFrames()) {
                result.push_back(
                    DopeSheetKeyframeEntry{layerId, property->getName(), keyframe});
            }
        }
    }

    std::stable_sort(result.begin(), result.end(),
                     [](const DopeSheetKeyframeEntry& lhs,
                        const DopeSheetKeyframeEntry& rhs) {
                         if (lhs.keyframe.time == rhs.keyframe.time) {
                             return lhs.propertyPath < rhs.propertyPath;
                         }
                         return lhs.keyframe.time < rhs.keyframe.time;
                     });
    return result;
}

bool ArtifactTimelineKeyframeModel::offsetKeyframes(
    const CompositionID& compId,
    const std::vector<DopeSheetKeyframeRef>& refs,
    const RationalTime& delta) {
    if (delta.value() == 0) {
        return false;
    }

    return applyDopeSheetTransform(
        compId, refs,
        [&delta](const RationalTime& time) {
            const RationalTime moved = time + delta;
            return moved < RationalTime(0, moved.scale())
                       ? RationalTime(0, moved.scale())
                       : moved;
        });
}

bool ArtifactTimelineKeyframeModel::scaleKeyframes(
    const CompositionID& compId,
    const std::vector<DopeSheetKeyframeRef>& refs,
    const RationalTime& pivot,
    double factor) {
    if (refs.empty() || !std::isfinite(factor) || factor <= 0.0) {
        return false;
    }

    return applyDopeSheetTransform(
        compId, refs,
        [&pivot, factor](const RationalTime& time) {
            const int64_t scale = std::max<int64_t>(1, time.scale());
            const int64_t pivotValue = pivot.rescaledTo(scale);
            const int64_t sourceValue = time.rescaledTo(scale);
            const double scaledValue =
                static_cast<double>(pivotValue) +
                (static_cast<double>(sourceValue - pivotValue) * factor);
            const int64_t roundedValue =
                static_cast<int64_t>(std::llround(scaledValue));
            return RationalTime(std::max<int64_t>(0, roundedValue), scale);
        });
}

} // namespace Artifact
