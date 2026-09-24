module;

#include <QMatrix4x4>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector3D>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

export module Artifact.Widgets.CompositionGizmoUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.SourceCrop;
import Event.Bus;
import Memory.SharedPtr;
import Property.Abstract;
import Time.Rational;
import Undo.UndoManager;

export namespace Artifact {

struct GizmoPropertyKeySnapshot {
  QString path;
  ArtifactCore::KeyFrame key;
  bool hasKey = false;
  bool animated = false;
  bool animatable = false;
  QVariant baseValue;
};

struct GizmoTransformSnapshot {
  QVector3D position;
  QVector3D rotation;
  QVector3D scale{1.0f, 1.0f, 1.0f};
  bool is3D = false;
  bool hasPositionKey = false;
  bool hasRotationKey = false;
  bool hasScaleKey = false;
  bool positionAnimated = false;
  bool rotationAnimated = false;
  bool scaleAnimated = false;
  GizmoPropertyKeySnapshot properties[9] = {
      {QStringLiteral("transform.position.x"), {}},
      {QStringLiteral("transform.position.y"), {}},
      {QStringLiteral("transform.position.z"), {}},
      {QStringLiteral("transform.rotation"), {}},
      {QStringLiteral("transform.rotation.x"), {}},
      {QStringLiteral("transform.rotation.y"), {}},
      {QStringLiteral("transform.scale.x"), {}},
      {QStringLiteral("transform.scale.y"), {}},
      {QStringLiteral("transform.scale.z"), {}}};

  bool propertyAnimated(const QString &path) const {
    for (const auto &property : properties) {
      if (property.path == path) return property.animated;
    }
    return false;
  }
};

void captureGizmoPropertyKeys(const ArtifactAbstractLayerPtr &layer,
                              const ArtifactCore::RationalTime &time,
                              GizmoTransformSnapshot &snapshot) {
  if (!layer) return;
  for (auto &saved : snapshot.properties) {
    const auto property = layer->getProperty(saved.path);
    saved.hasKey = false;
    saved.animated = false;
    if (!property) continue;
    saved.baseValue = property->getValue();
    saved.animatable = property->isAnimatable();
    const auto keys = property->getKeyFrames();
    saved.animated = !keys.empty();
    for (const auto &key : keys) {
      if (key.time == time) {
        saved.key = key;
        saved.hasKey = true;
        break;
      }
    }
  }
}

void restoreGizmoPropertyKeys(const ArtifactAbstractLayerPtr &layer,
                              const ArtifactCore::RationalTime &time,
                              const GizmoTransformSnapshot &snapshot) {
  if (!layer) return;
  for (const auto &saved : snapshot.properties) {
    const auto property = layer->getProperty(saved.path);
    if (!property || !property->isAnimatable()) continue;
    if (saved.baseValue.isValid()) property->setValue(saved.baseValue);
    if (saved.hasKey) {
      const auto &key = saved.key;
      property->addKeyFrame(key.time, key.value, key.interpolation,
                            key.cp1_x, key.cp1_y, key.cp2_x, key.cp2_y,
                            key.roving);
      property->setKeyFrameAnchorAt(key.time, key.anchor);
      property->setKeyFrameColorLabelAt(key.time, key.colorLabel);
    } else {
      property->removeKeyFrame(time);
    }
  }
}

bool gizmoKeyMatches(const ArtifactCore::KeyFrame& lhs,
                    const ArtifactCore::KeyFrame& rhs) {
  return lhs.time == rhs.time && lhs.value == rhs.value &&
      lhs.interpolation == rhs.interpolation && lhs.cp1_x == rhs.cp1_x &&
      lhs.cp1_y == rhs.cp1_y && lhs.cp2_x == rhs.cp2_x &&
      lhs.cp2_y == rhs.cp2_y && lhs.roving == rhs.roving &&
      lhs.anchor == rhs.anchor && lhs.colorLabel == rhs.colorLabel;
}

bool gizmoKeyListsMatch(const std::vector<ArtifactCore::KeyFrame>& lhs,
                        const std::vector<ArtifactCore::KeyFrame>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t index = 0; index < lhs.size(); ++index)
    if (!gizmoKeyMatches(lhs[index], rhs[index])) return false;
  return true;
}

bool gizmoSnapshotMatches(const ArtifactAbstractLayerPtr& layer,
                          const ArtifactCore::RationalTime& time,
                          const GizmoTransformSnapshot& snapshot) {
  if (!layer) return false;
  for (const auto& saved : snapshot.properties) {
    if (!saved.baseValue.isValid()) continue;
    const auto property = layer->getProperty(saved.path);
    if (!property) return false;
    if (property->isAnimatable() != saved.animatable) return false;
    if (saved.animatable && property->getValue() != saved.baseValue)
      return false;
    const auto keys = property->getKeyFrames();
    if ((!keys.empty()) != saved.animated) return false;
    const auto found = std::find_if(keys.begin(), keys.end(),
        [&time](const ArtifactCore::KeyFrame& key) { return key.time == time; });
    if (saved.hasKey) {
      if (found == keys.end() || !gizmoKeyMatches(*found, saved.key)) return false;
    } else if (found != keys.end()) {
      return false;
    }
  }
  return true;
}

std::vector<ArtifactCore::KeyFrame> gizmoKeysWithSnapshot(
    std::vector<ArtifactCore::KeyFrame> keys,
    const ArtifactCore::RationalTime& time,
    const GizmoPropertyKeySnapshot& snapshot) {
  const auto found = std::find_if(keys.begin(), keys.end(),
      [&time](const ArtifactCore::KeyFrame& key) { return key.time == time; });
  if (!snapshot.hasKey) {
    if (found != keys.end()) keys.erase(found);
    return keys;
  }
  if (found != keys.end()) {
    *found = snapshot.key;
  } else {
    const auto insertion = std::lower_bound(keys.begin(), keys.end(), time,
        [](const ArtifactCore::KeyFrame& key,
           const ArtifactCore::RationalTime& target) { return key.time < target; });
    keys.insert(insertion, snapshot.key);
  }
  return keys;
}

bool appendGizmoTransformChanges(
    const ArtifactAbstractLayerPtr& layer, int64_t frame,
    const GizmoTransformSnapshot& before,
    const GizmoTransformSnapshot& after, const QString& action,
    QJsonArray& changes) {
  if (!layer) return false;
  const bool reverse = action == QStringLiteral("undo");
  const auto& currentSnapshot = reverse ? after : before;
  const auto& targetSnapshot = reverse ? before : after;
  const auto time = layer->keyframeTimeAtFrame(frame);
  const bool pushAlreadyApplied = action == QStringLiteral("push");
  if (!gizmoSnapshotMatches(layer, time,
                            pushAlreadyApplied ? after : currentSnapshot))
    return false;

  for (int index = 0; index < 9; ++index) {
    const auto& beforeProperty = before.properties[index];
    const auto& afterProperty = after.properties[index];
    if (!beforeProperty.baseValue.isValid() && !afterProperty.baseValue.isValid())
      continue;
    if (!beforeProperty.baseValue.isValid() || !afterProperty.baseValue.isValid())
      return false;
    const auto property = layer->getProperty(beforeProperty.path);
    if (!property) return false;
    const auto currentKeys = property->getKeyFrames();
    const auto expectedKeys = pushAlreadyApplied
        ? gizmoKeysWithSnapshot(currentKeys, time, beforeProperty)
        : currentKeys;
    const auto nextKeys = pushAlreadyApplied
        ? currentKeys
        : gizmoKeysWithSnapshot(currentKeys, time, targetSnapshot.properties[index]);
    const QVariant expectedBase = pushAlreadyApplied
        ? beforeProperty.baseValue : currentSnapshot.properties[index].baseValue;
    const QVariant nextBase = targetSnapshot.properties[index].baseValue;
    const bool keyframesDiffer = !gizmoKeyListsMatch(expectedKeys, nextKeys) ||
        (pushAlreadyApplied ? beforeProperty.animated : currentSnapshot.properties[index].animated) !=
            targetSnapshot.properties[index].animated;
    const bool baseDiffers = expectedBase != nextBase;
    if (keyframesDiffer && baseDiffers) return false;
    if (!keyframesDiffer && !baseDiffers) continue;

    if (keyframesDiffer) {
      SetLayerPropertyKeyframesCommand serializer(
          layer, beforeProperty.path, expectedKeys, nextKeys,
          QStringLiteral("Encode Gizmo Transform"),
          pushAlreadyApplied ? std::optional<bool>(beforeProperty.animatable)
                             : std::optional<bool>(currentSnapshot.properties[index].animatable),
          targetSnapshot.properties[index].animatable);
      const QJsonObject encoded = serializer.serialize();
      QJsonObject change{{QStringLiteral("layerId"), layer->id().toString()},
                         {QStringLiteral("propertyPath"), beforeProperty.path},
                         {QStringLiteral("kind"), QStringLiteral("keyframes")},
                         {QStringLiteral("expectedKeyframes"),
                          encoded.value(QStringLiteral("before"))},
                         {QStringLiteral("keyframes"),
                          encoded.value(QStringLiteral("after"))},
                         {QStringLiteral("expectedAnimatable"),
                          pushAlreadyApplied ? beforeProperty.animatable
                                             : currentSnapshot.properties[index].animatable},
                         {QStringLiteral("animatable"),
                          targetSnapshot.properties[index].animatable}};
      changes.append(change);
    } else {
      changes.append(QJsonObject{
          {QStringLiteral("layerId"), layer->id().toString()},
          {QStringLiteral("propertyPath"), beforeProperty.path},
          {QStringLiteral("kind"), QStringLiteral("value")},
          {QStringLiteral("expectedValue"), QJsonValue::fromVariant(expectedBase)},
          {QStringLiteral("value"), QJsonValue::fromVariant(nextBase)}});
    }
  }
  return true;
}

struct GizmoGroupLayerState {
  ArtifactAbstractLayerPtr layer;
  int64_t frame = 0;
  GizmoTransformSnapshot before;
  QVector3D worldAnchor;
  QMatrix4x4 parentWorldInverse;
  bool parentWorldInvertible = true;
};

} // namespace Artifact

namespace Artifact {

namespace {

ArtifactCore::RationalTime transformTime(
    const ArtifactAbstractLayerPtr &layer, int64_t frame) {
  // Undo/redo must address the same frame domain the live gizmo drag wrote
  // into. The layer owns that domain, so ask it instead of re-deriving the
  // scale from the current composition frame rate.
  return layer ? layer->keyframeTimeAtFrame(frame)
               : ArtifactCore::RationalTime(frame, 24);
}

void restorePropertyKeyState(const ArtifactAbstractLayerPtr &layer,
                             int64_t frame,
                             const GizmoTransformSnapshot &snapshot) {
  restoreGizmoPropertyKeys(layer, transformTime(layer, frame), snapshot);
}

} // namespace

} // namespace Artifact

export namespace Artifact {

class GizmoTransformUndoCommand final : public UndoCommand {
public:
  GizmoTransformUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                            GizmoTransformSnapshot before,
                            GizmoTransformSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(after_, before_, false); }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(before_, after_, allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    QJsonArray changes;
    if (!appendGizmoTransformChanges(layer, frame_, before_, after_,
                                     action, changes) ||
        changes.isEmpty()) return false;
    payload = QJsonObject{{QStringLiteral("changes"), changes}};
    if (QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 1048576)
      return false;
    operationType = QStringLiteral("property.batch");
    operationLayerId.clear();
    return true;
  }
  QString label() const override { return QStringLiteral("3D Gizmo Transform"); }

private:
  bool apply(const GizmoTransformSnapshot& expected,
             const GizmoTransformSnapshot& snapshot, bool allowPreApplied) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto time = transformTime(layer, frame_);
    if (!gizmoSnapshotMatches(layer, time, expected)) {
      if (allowPreApplied && gizmoSnapshotMatches(layer, time, snapshot))
        return true;
      return false;
    }
    restorePropertyKeyState(layer, frame_, snapshot);
    if (!gizmoSnapshotMatches(layer, time, snapshot)) {
      restorePropertyKeyState(layer, frame_, expected);
      return false;
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  int64_t frame_ = 0;
  GizmoTransformSnapshot before_;
  GizmoTransformSnapshot after_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

struct GizmoGroupUndoEntry {
  ArtifactAbstractLayerWeak layer;
  int64_t frame = 0;
  GizmoTransformSnapshot before;
  GizmoTransformSnapshot after;
};

class GizmoGroupTransformUndoCommand final : public UndoCommand {
public:
  explicit GizmoGroupTransformUndoCommand(
      std::vector<GizmoGroupUndoEntry> entries)
      : entries_(std::move(entries)) {}

  void undo() override { lastOperationSucceeded_ = apply(false); }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(true, allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    QStringList ids;
    for (const auto& entry : entries_) {
      const auto layer = entry.layer.lock();
      if (layer && !ids.contains(layer->id().toString()))
        ids.append(layer->id().toString());
    }
    return ids;
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    if (entries_.empty() || (action != QStringLiteral("push") &&
        action != QStringLiteral("undo") && action != QStringLiteral("redo")))
      return false;
    QJsonArray changes;
    for (const auto& entry : entries_) {
      const auto layer = entry.layer.lock();
      if (!layer || !appendGizmoTransformChanges(
              layer, entry.frame, entry.before, entry.after, action, changes))
        return false;
    }
    if (changes.isEmpty() || changes.size() > 128) return false;
    payload = QJsonObject{{QStringLiteral("changes"), changes}};
    if (QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 1048576)
      return false;
    operationType = QStringLiteral("property.batch");
    operationLayerId.clear();
    return true;
  }
  QString label() const override {
    return QStringLiteral("Transform Selected Layers");
  }

private:
  bool apply(bool useAfter, bool allowPreApplied = false) {
    for (const auto& entry : entries_) {
      auto layer = entry.layer.lock();
      if (!layer) return false;
      const auto& expected = useAfter ? entry.before : entry.after;
      const auto& target = useAfter ? entry.after : entry.before;
      const auto time = transformTime(layer, entry.frame);
      if (!gizmoSnapshotMatches(layer, time, expected) &&
          !(allowPreApplied && useAfter &&
            gizmoSnapshotMatches(layer, time, target))) return false;
    }
    size_t appliedCount = 0;
    for (const auto &entry : entries_) {
      auto layer = entry.layer.lock();
      if (!layer) break;
      const auto& expected = useAfter ? entry.before : entry.after;
      const auto &snapshot = useAfter ? entry.after : entry.before;
      const auto time = transformTime(layer, entry.frame);
      if (allowPreApplied && useAfter &&
          gizmoSnapshotMatches(layer, time, snapshot)) {
        ++appliedCount;
        continue;
      }
      restorePropertyKeyState(layer, entry.frame, snapshot);
      if (!gizmoSnapshotMatches(layer, time, snapshot)) {
        restorePropertyKeyState(layer, entry.frame, expected);
        for (size_t rollback = appliedCount; rollback > 0; --rollback) {
          const auto& previous = entries_[rollback - 1];
          if (const auto previousLayer = previous.layer.lock())
            restorePropertyKeyState(previousLayer, previous.frame,
                                    useAfter ? previous.before : previous.after);
        }
        return false;
      }
      ++appliedCount;
      layer->setDirty(LayerDirtyFlag::Transform);
      layer->changed();
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(
              layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return appliedCount == entries_.size();
  }

  std::vector<GizmoGroupUndoEntry> entries_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

// VP crop drags drive the sourceCrop.* property paths (clamping and keyframe
// semantics stay identical to numeric edits). Snapshots are whole values.
class SourceCropRectUndoCommand final : public UndoCommand {
 public:
  SourceCropRectUndoCommand(ArtifactAbstractLayerPtr layer, SourceCrop before,
                            SourceCrop after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(after_, before_, false); }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(before_, after_, allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toQString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType, QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const QJsonObject expected = (reverse ? after_ : before_).toJson();
    const QJsonObject value = (reverse ? before_ : after_).toJson();
    if (QJsonDocument(expected).toJson(QJsonDocument::Compact).size() > 32768 ||
        QJsonDocument(value).toJson(QJsonDocument::Compact).size() > 32768) return false;
    operationType = QStringLiteral("layer.sourceCrop");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{{QStringLiteral("expected"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override { return QStringLiteral("Crop Image Layer"); }

 private:
  bool apply(const SourceCrop& expected, const SourceCrop& snapshot,
             bool allowPreApplied) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto imageLayer =
        ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
    if (!imageLayer) return false;
    const QJsonObject current = imageLayer->sourceCrop().toJson();
    const QJsonObject expectedState = expected.toJson();
    const QJsonObject nextState = snapshot.toJson();
    if (current == nextState) return allowPreApplied;
    if (current != expectedState) return false;
    if (!imageLayer->restoreSourceCropSnapshot(nextState) ||
        imageLayer->sourceCrop().toJson() != nextState) {
      if (imageLayer->sourceCrop().toJson() != expectedState)
        imageLayer->restoreSourceCropSnapshot(expectedState);
      return false;
    }
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  SourceCrop before_;
  SourceCrop after_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

// VP solid-size drags. Size is not keyframable: plain before/after values.
class SolidSizeUndoCommand final : public UndoCommand {
 public:
  SolidSizeUndoCommand(ArtifactAbstractLayerPtr layer, QSize before,
                       QSize after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(after_, before_, false); }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(before_, after_, allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toQString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType, QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const QSize expected = reverse ? after_ : before_;
    const QSize value = reverse ? before_ : after_;
    const auto valid = [](const QSize& size) {
      return size.width() >= 1 && size.width() <= 16384 &&
             size.height() >= 1 && size.height() <= 16384;
    };
    if (!valid(expected) || !valid(value)) return false;
    operationType = QStringLiteral("layer.solidSize");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{
        {QStringLiteral("expected"), QJsonObject{
             {QStringLiteral("width"), expected.width()},
             {QStringLiteral("height"), expected.height()}}},
        {QStringLiteral("value"), QJsonObject{
             {QStringLiteral("width"), value.width()},
             {QStringLiteral("height"), value.height()}}}};
    return true;
  }
  QString label() const override { return QStringLiteral("Resize Solid Layer"); }

 private:
  bool apply(const QSize& expected, const QSize& size, bool allowPreApplied) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto solidLayer =
        ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer);
    const auto solidImage = solidLayer
        ? ArtifactCore::SharedPtr<ArtifactSolidImageLayer>{}
        : ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer);
    if (!solidLayer && !solidImage) return false;
    const auto current = layer->sourceSize();
    if (current.width == size.width() && current.height == size.height())
      return allowPreApplied; // Viewport drag applies before the initial push.
    if (current.width != expected.width() || current.height != expected.height() ||
        size.width() < 1 || size.width() > 16384 ||
        size.height() < 1 || size.height() > 16384) return false;
    if (solidLayer) solidLayer->setSize(size.width(), size.height());
    else solidImage->setSize(size.width(), size.height());
    const auto applied = layer->sourceSize();
    if (applied.width != size.width() || applied.height != size.height()) {
      if (solidLayer) solidLayer->setSize(expected.width(), expected.height());
      else solidImage->setSize(expected.width(), expected.height());
      return false;
    }
    layer->setDirty(LayerDirtyFlag::Source);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  QSize before_;
  QSize after_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

// VP solid-gradient drags drive the solid.gradientCenterX/Y/AngleDegrees
// property paths (same semantics as numeric edits).
class SolidGradientUndoCommand final : public UndoCommand {
 public:
  struct AxisState {
    double centerX = 0.5;
    double centerY = 0.5;
    double angleDegrees = 90.0;
  };
  SolidGradientUndoCommand(ArtifactAbstractLayerPtr layer, AxisState before,
                           AxisState after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(after_, before_, false); }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(before_, after_, allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toQString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType, QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const AxisState& expected = reverse ? after_ : before_;
    const AxisState& value = reverse ? before_ : after_;
    const double expectedValues[] = {expected.centerX, expected.centerY,
                                     expected.angleDegrees};
    const double values[] = {value.centerX, value.centerY, value.angleDegrees};
    const QString paths[] = {QStringLiteral("solid.gradientCenterX"),
                             QStringLiteral("solid.gradientCenterY"),
                             QStringLiteral("solid.gradientAngleDegrees")};
    QJsonArray changes;
    for (int i = 0; i < 3; ++i) {
      if (!std::isfinite(expectedValues[i]) || !std::isfinite(values[i])) return false;
      changes.append(QJsonObject{
          {QStringLiteral("layerId"), layer->id().toQString()},
          {QStringLiteral("propertyPath"), paths[i]},
          {QStringLiteral("expectedValue"), expectedValues[i]},
          {QStringLiteral("value"), values[i]}});
    }
    payload = QJsonObject{{QStringLiteral("changes"), changes}};
    if (QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 1048576)
      return false;
    operationType = QStringLiteral("property.batch");
    operationLayerId = layer->id().toQString();
    return true;
  }
  QString label() const override { return QStringLiteral("Edit Solid Gradient"); }

 private:
  bool apply(const AxisState& expected, const AxisState& snapshot,
             bool allowPreApplied) {
    auto layer = layer_.lock();
    if (!layer) return false;
    // Solid2D and SolidImage share the solid.gradient* property paths.
    const auto solid2D =
        ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer);
    const auto solidImage =
        solid2D ? ArtifactAbstractLayerPtr{}
                : ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(
                      layer);
    if (!solid2D && !solidImage) return false;
    const double expectedValues[] = {expected.centerX, expected.centerY,
                                     expected.angleDegrees};
    const double values[] = {snapshot.centerX, snapshot.centerY,
                             snapshot.angleDegrees};
    const QString paths[] = {QStringLiteral("solid.gradientCenterX"),
                             QStringLiteral("solid.gradientCenterY"),
                             QStringLiteral("solid.gradientAngleDegrees")};
    bool alreadyApplied = true;
    bool expectedMatches = true;
    for (int i = 0; i < 3; ++i) {
      const auto property = layer->getProperty(paths[i]);
      if (!property || !std::isfinite(expectedValues[i]) ||
          !std::isfinite(values[i])) return false;
      const QJsonValue current = QJsonValue::fromVariant(property->getValue());
      alreadyApplied = alreadyApplied && current.isDouble() &&
                       current.toDouble() == values[i];
      expectedMatches = expectedMatches && current.isDouble() &&
                        current.toDouble() == expectedValues[i];
    }
    if (alreadyApplied) return allowPreApplied;
    if (!expectedMatches) return false;
    const auto applyPath = [&](const QString &path, double value) {
      if (solid2D) {
        solid2D->setLayerPropertyValue(path, value);
      } else {
        solidImage->setLayerPropertyValue(path, value);
      }
    };
    applyPath(QStringLiteral("solid.gradientCenterX"), snapshot.centerX);
    applyPath(QStringLiteral("solid.gradientCenterY"), snapshot.centerY);
    applyPath(QStringLiteral("solid.gradientAngleDegrees"),
              snapshot.angleDegrees);
    bool verified = true;
    for (int i = 0; i < 3; ++i) {
      const auto property = layer->getProperty(paths[i]);
      verified = verified && property &&
          QJsonValue::fromVariant(property->getValue()).isDouble() &&
          QJsonValue::fromVariant(property->getValue()).toDouble() == values[i];
    }
    if (!verified) {
      applyPath(paths[0], expectedValues[0]);
      applyPath(paths[1], expectedValues[1]);
      applyPath(paths[2], expectedValues[2]);
      return false;
    }
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  AxisState before_;
  AxisState after_;
  bool firstRedo_ = true;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
