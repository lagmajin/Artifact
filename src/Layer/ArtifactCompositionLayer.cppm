module;

#include <QSize>
#include <QVariant>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <QHash>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMetaType>

module Artifact.Layer.Composition;

import Property.Abstract;
import Property.Group;
import Artifact.Composition.Abstract;
import Artifact.Service.Project;
import Composition.Settings;
import Property.ExposedPropertyRegistry;

namespace Artifact {
using namespace ArtifactCore;

class ArtifactCompositionLayer::Impl {
public:
  CompositionID id_;
  bool restoreMoveAllAttributes_ = false;
  QHash<QString, QString> restoreExternalParents_;
  ArtifactCore::ExposedPropertyRegistry exposedProperties_;
  QMap<QString, QVariant> exposedPropertyOverrides_;
  bool exposedPropertyOverridesDirty_ = false;
};

ArtifactCompositionLayer::ArtifactCompositionLayer() : impl_(new Impl()) {}

ArtifactCompositionLayer::~ArtifactCompositionLayer() { delete impl_; }

CompositionID ArtifactCompositionLayer::sourceCompositionId() const {
  return impl_->id_;
}

void ArtifactCompositionLayer::setCompositionId(const CompositionID &id) {
  impl_->id_ = id;
  Q_EMIT changed();
}

void ArtifactCompositionLayer::setRestoreMoveAllAttributes(bool enabled) {
  impl_->restoreMoveAllAttributes_ = enabled;
}

bool ArtifactCompositionLayer::restoreMoveAllAttributes() const {
  return impl_->restoreMoveAllAttributes_;
}

void ArtifactCompositionLayer::clearRestoreExternalParents() {
  impl_->restoreExternalParents_.clear();
}

void ArtifactCompositionLayer::setRestoreExternalParent(
    const LayerID &childLayerId, const LayerID &parentLayerId) {
  if (childLayerId.isNil() || parentLayerId.isNil()) {
    return;
  }
  impl_->restoreExternalParents_.insert(childLayerId.toString(),
                                        parentLayerId.toString());
}

LayerID ArtifactCompositionLayer::restoreExternalParent(
    const LayerID &childLayerId) const {
  if (childLayerId.isNil()) {
    return LayerID();
  }
  const auto it = impl_->restoreExternalParents_.constFind(childLayerId.toString());
  if (it == impl_->restoreExternalParents_.constEnd()) {
    return LayerID();
  }
  return LayerID(*it);
}

const ArtifactCore::ExposedPropertyRegistry&
ArtifactCompositionLayer::exposedProperties() const {
  return impl_->exposedProperties_;
}

void ArtifactCompositionLayer::setExposedProperties(
    const ArtifactCore::ExposedPropertyRegistry& registry) {
  impl_->exposedProperties_ = registry;
  impl_->exposedPropertyOverrides_.clear();
  impl_->exposedPropertyOverridesDirty_ = false;
  Q_EMIT changed();
}

bool ArtifactCompositionLayer::setExposedPropertyOverride(
    const QString& id, const QVariant& value) {
  const QString normalizedId = id.trimmed();
  if (normalizedId.isEmpty() ||
      !impl_->exposedProperties_.contains(normalizedId)) {
    return false;
  }
  const auto binding = impl_->exposedProperties_.binding(normalizedId);
  QVariant normalizedValue = value;
  if (binding.defaultValue.isValid()) {
    const QMetaType targetType = binding.defaultValue.metaType();
    if (!normalizedValue.canConvert(targetType) ||
        !normalizedValue.convert(targetType)) {
      return false;
    }
  }
  const auto existing = impl_->exposedPropertyOverrides_.constFind(normalizedId);
  if (existing != impl_->exposedPropertyOverrides_.cend() &&
      existing.value() == normalizedValue) {
    return true;
  }
  impl_->exposedPropertyOverrides_[normalizedId] = normalizedValue;
  impl_->exposedPropertyOverridesDirty_ = true;
  Q_EMIT changed();
  return true;
}

bool ArtifactCompositionLayer::hasExposedPropertyOverride(
    const QString& id) const {
  return impl_->exposedPropertyOverrides_.contains(id.trimmed());
}

QVariant ArtifactCompositionLayer::exposedPropertyOverride(
    const QString& id) const {
  return impl_->exposedPropertyOverrides_.value(id.trimmed());
}

QVariant ArtifactCompositionLayer::effectiveExposedPropertyValue(
    const QString& id) const {
  const QString normalizedId = id.trimmed();
  const auto override = impl_->exposedPropertyOverrides_.constFind(normalizedId);
  if (override != impl_->exposedPropertyOverrides_.cend()) {
    return override.value();
  }
  return impl_->exposedProperties_.binding(normalizedId).defaultValue;
}

void ArtifactCompositionLayer::clearExposedPropertyOverride(const QString& id) {
  if (impl_->exposedPropertyOverrides_.remove(id.trimmed()) > 0) {
    impl_->exposedPropertyOverridesDirty_ = true;
    Q_EMIT changed();
  }
}

bool ArtifactCompositionLayer::applyExposedPropertyOverrides() {
  if (!impl_->exposedPropertyOverridesDirty_) return true;
  const auto source = sourceComposition();
  if (!source) return false;

  bool applied = false;
  bool succeeded = true;
  for (const auto& binding : impl_->exposedProperties_.bindings()) {
    if (binding.targetLayerId.trimmed().isEmpty() ||
        binding.internalPath.trimmed().isEmpty()) {
      succeeded = false;
      continue;
    }
    const auto target = source->layerById(LayerID(binding.targetLayerId));
    if (!target) {
      succeeded = false;
      continue;
    }
    const bool updated = target->setLayerPropertyValue(
        binding.internalPath, effectiveExposedPropertyValue(binding.id));
    applied = applied || updated;
    succeeded = succeeded && updated;
  }
  if (applied && succeeded) {
    impl_->exposedPropertyOverridesDirty_ = false;
  }
  return applied && succeeded;
}

QJsonObject ArtifactCompositionLayer::toJson() const {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Precomp);
  obj["composition.sourceId"] = impl_->id_.toString();
  QJsonObject restoreObj;
  restoreObj["moveAllAttributes"] = impl_->restoreMoveAllAttributes_;
  QJsonArray externalParents;
  for (auto it = impl_->restoreExternalParents_.constBegin();
       it != impl_->restoreExternalParents_.constEnd(); ++it) {
    QJsonObject entry;
    entry["childLayerId"] = it.key();
    entry["parentLayerId"] = it.value();
    externalParents.append(entry);
  }
  restoreObj["externalParents"] = externalParents;
  obj["precompose.restore"] = restoreObj;
  obj["masterProperties"] = impl_->exposedProperties_.toJson();
  QJsonObject overrides;
  for (auto it = impl_->exposedPropertyOverrides_.cbegin();
       it != impl_->exposedPropertyOverrides_.cend(); ++it) {
    overrides.insert(it.key(), QJsonValue::fromVariant(it.value()));
  }
  obj["masterPropertyOverrides"] = overrides;
  return obj;
}

void ArtifactCompositionLayer::fromJsonProperties(const QJsonObject &obj) {
  ArtifactAbstractLayer::fromJsonProperties(obj);
  if (obj.contains("composition.sourceId")) {
    setCompositionId(CompositionID(obj["composition.sourceId"].toString()));
  }

  impl_->restoreMoveAllAttributes_ = false;
  impl_->restoreExternalParents_.clear();
  const QJsonObject restoreObj = obj.value("precompose.restore").toObject();
  if (!restoreObj.isEmpty()) {
    impl_->restoreMoveAllAttributes_ =
        restoreObj.value("moveAllAttributes").toBool(false);
    const QJsonArray externalParents =
        restoreObj.value("externalParents").toArray();
    for (const auto &value : externalParents) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject entry = value.toObject();
      const QString childId = entry.value("childLayerId").toString();
      const QString parentId = entry.value("parentLayerId").toString();
      if (childId.isEmpty() || parentId.isEmpty()) {
        continue;
      }
      impl_->restoreExternalParents_.insert(childId, parentId);
    }
  }
  impl_->exposedProperties_.fromJson(
      obj.value("masterProperties").toArray());
  impl_->exposedPropertyOverrides_.clear();
  const QJsonObject overrides =
      obj.value("masterPropertyOverrides").toObject();
  for (auto it = overrides.begin(); it != overrides.end(); ++it) {
    if (impl_->exposedProperties_.contains(it.key())) {
      impl_->exposedPropertyOverrides_.insert(it.key(), it.value().toVariant());
    }
  }
  impl_->exposedPropertyOverridesDirty_ =
      !impl_->exposedPropertyOverrides_.isEmpty();
}

std::shared_ptr<ArtifactAbstractComposition>
ArtifactCompositionLayer::sourceComposition() const {
  auto *service = ArtifactProjectService::instance();
  if (!service) {
    return nullptr;
  }
  const auto result = service->findComposition(impl_->id_);
  return result.ptr.lock();
}

void ArtifactCompositionLayer::draw(ArtifactIRenderer *) {
  // Precomp layers are rendered through the composition view drawing path.
  // The layer itself acts as a container/reference marker in the timeline and
  // inspector, so the direct layer draw is intentionally a no-op.
}

QRectF ArtifactCompositionLayer::localBounds() const {
  if (auto comp = sourceComposition()) {
    const QSize size = comp->settings().compositionSize();
    return QRectF(0, 0, size.width(), size.height());
  }
  return QRectF(0, 0, 100, 100);
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactCompositionLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup compGroup(QStringLiteral("Composition"));

  auto idProp = persistentLayerProperty(QStringLiteral("composition.sourceId"),
                                        ArtifactCore::PropertyType::String,
                                        sourceCompositionId().toString(), -120);
  compGroup.addProperty(idProp);

  groups.push_back(compGroup);

  const auto bindings = impl_->exposedProperties_.bindings();
  if (!bindings.isEmpty()) {
    ArtifactCore::PropertyGroup exposedGroup(QStringLiteral("Master Properties"));
    for (const auto& binding : bindings) {
      const QVariant value = effectiveExposedPropertyValue(binding.id);
      ArtifactCore::PropertyType type = ArtifactCore::PropertyType::String;
      if (value.metaType().id() == QMetaType::Bool) {
        type = ArtifactCore::PropertyType::Boolean;
      } else if (value.canConvert<QColor>()) {
        type = ArtifactCore::PropertyType::Color;
      } else if (value.metaType().id() == QMetaType::Double ||
                 value.metaType().id() == QMetaType::Float ||
                 value.metaType().id() == QMetaType::Int ||
                 value.metaType().id() == QMetaType::LongLong ||
                 value.metaType().id() == QMetaType::UInt ||
                 value.metaType().id() == QMetaType::ULongLong) {
        type = ArtifactCore::PropertyType::Float;
      }
      auto property = persistentLayerProperty(
          QStringLiteral("Master Properties/") + binding.id, type, value, -100);
      if (!binding.label.isEmpty()) property->setDisplayLabel(binding.label);
      exposedGroup.addProperty(property);
    }
    groups.push_back(exposedGroup);
  }
  return groups;
}

bool ArtifactCompositionLayer::setLayerPropertyValue(
    const QString &propertyPath, const QVariant &value) {
  if (propertyPath == QStringLiteral("composition.sourceId")) {
    setCompositionId(CompositionID(value.toString()));
    return true;
  }
  const QString prefix = QStringLiteral("Master Properties/");
  if (propertyPath.startsWith(prefix)) {
    return setExposedPropertyOverride(propertyPath.mid(prefix.size()), value);
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

}; // namespace Artifact
