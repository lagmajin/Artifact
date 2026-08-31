module;

#include <QSet>
#include <QString>
#include <QVector>

#include <utility>

export module Artifact.Timeline.KeyframeApplyCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Event.Bus;
import Property.Abstract;
import Time.Rational;
import Undo.UndoManager;

export namespace Artifact {

namespace {

ArtifactCore::AbstractPropertyPtr findLayerPropertyByPath(
    const ArtifactAbstractLayerPtr &layer, const QString &propertyPath) {
  if (!layer || propertyPath.trimmed().isEmpty()) return {};
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto &group : groups) {
    for (const auto &property : group.sortedProperties()) {
      if (property && property->getName() == propertyPath) return property;
    }
  }
  return {};
}

} // namespace

struct InterpolationChangeRecord {
  ArtifactAbstractLayerWeak layer;
  QString propertyPath;
  ArtifactCore::RationalTime time;
  ArtifactCore::KeyFrame before;
  ArtifactCore::KeyFrame after;
};

class ApplyInterpolationCommand final : public UndoCommand {
public:
  explicit ApplyInterpolationCommand(QVector<InterpolationChangeRecord> records)
      : records_(std::move(records)) {}

  void undo() override { lastOperationSucceeded_ = apply(false); }
  void redo() override { lastOperationSucceeded_ = apply(true); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Apply Interpolation"); }

private:
  bool apply(bool useAfter) {
    bool succeeded = true;
    QSet<QString> changedLayerIds;
    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        succeeded = false;
        continue;
      }
      const auto property = findLayerPropertyByPath(layer, record.propertyPath);
      if (!property) {
        succeeded = false;
        continue;
      }
      const auto &keyframe = useAfter ? record.after : record.before;
      property->addKeyFrame(
          keyframe.time,
          keyframe.value.isValid() ? keyframe.value : property->getValue(),
          keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
          keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      layer->changed();
      changedLayerIds.insert(layer->id().toString());
    }

    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer || !changedLayerIds.contains(layer->id().toString())) continue;
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(
              layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }

    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    return succeeded && !changedLayerIds.isEmpty();
  }

  QVector<InterpolationChangeRecord> records_;
  bool lastOperationSucceeded_ = true;
};

struct RovingChangeRecord {
  ArtifactAbstractLayerWeak layer;
  QString propertyPath;
  ArtifactCore::RationalTime time;
  ArtifactCore::KeyFrame before;
  ArtifactCore::KeyFrame after;
};

class ApplyRovingCommand final : public UndoCommand {
public:
  explicit ApplyRovingCommand(QVector<RovingChangeRecord> records)
      : records_(std::move(records)) {}

  void undo() override { lastOperationSucceeded_ = apply(false); }
  void redo() override { lastOperationSucceeded_ = apply(true); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Apply Roving"); }

private:
  bool apply(bool useAfter) {
    bool succeeded = true;
    QSet<QString> changedLayerIds;
    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer) {
        succeeded = false;
        continue;
      }
      const auto property = findLayerPropertyByPath(layer, record.propertyPath);
      if (!property) {
        succeeded = false;
        continue;
      }
      const auto &keyframe = useAfter ? record.after : record.before;
      property->addKeyFrame(
          keyframe.time,
          keyframe.value.isValid() ? keyframe.value : property->getValue(),
          keyframe.interpolation, keyframe.cp1_x, keyframe.cp1_y,
          keyframe.cp2_x, keyframe.cp2_y, keyframe.roving);
      property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
      property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
      layer->changed();
      changedLayerIds.insert(layer->id().toString());
    }

    for (const auto &record : records_) {
      auto layer = record.layer.lock();
      if (!layer || !changedLayerIds.contains(layer->id().toString())) continue;
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(
              layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
    }

    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    return succeeded && !changedLayerIds.isEmpty();
  }

  QVector<RovingChangeRecord> records_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
