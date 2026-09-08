module;

#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVector>

#include <cstdint>
#include <cmath>
#include <utility>

export module Artifact.Widgets.CompositionMotionPathCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Animation.Transform3D;
import Event.Bus;
import Property.Abstract;
import Time.Rational;
import Undo.UndoManager;

export namespace Artifact {

// Only the two scale properties are owned by this transaction. Keep the exact
// rational time and full key metadata so undo never edits the playhead frame.
struct MotionPathScaleState {
  ArtifactCore::KeyFrame keys[2];
  bool present[2] = {false, false};
  QVariant values[2];
};

bool captureMotionPathScale(const ArtifactAbstractLayerPtr &layer,
                            const ArtifactCore::RationalTime &time,
                            MotionPathScaleState &state) {
  if (!layer) return false;
  const QString paths[2] = {QStringLiteral("transform.scale.x"),
                            QStringLiteral("transform.scale.y")};
  for (int i = 0; i < 2; ++i) {
    const auto property = layer->getProperty(paths[i]);
    if (!property || !property->isAnimatable() || property->hasExpression() ||
        property->hasEnvelopes()) return false;
    state.present[i] = false;
    const auto keys = property->getKeyFrames();
    state.values[i] = keys.empty()
        ? QVariant(i == 0 ? layer->transform3D().scaleXAt(time)
                           : layer->transform3D().scaleYAt(time))
        : property->interpolateValue(time);
    for (const auto &key : keys) {
      if (key.time == time) {
        state.keys[i] = key;
        state.present[i] = true;
        break;
      }
    }
  }
  return true;
}

bool applyMotionPathScale(const ArtifactAbstractLayerPtr &layer,
                          const ArtifactCore::RationalTime &time,
                          const MotionPathScaleState &state) {
  if (!layer) return false;
  const QString paths[2] = {QStringLiteral("transform.scale.x"),
                            QStringLiteral("transform.scale.y")};
  for (const auto &path : paths) {
    const auto property = layer->getProperty(path);
    if (!property || !property->isAnimatable()) return false;
  }
  for (int i = 0; i < 2; ++i) {
    if (state.present[i] && !std::isfinite(state.values[i].toDouble())) return false;
  }
  for (int i = 0; i < 2; ++i) {
    const auto property = layer->getProperty(paths[i]);
    if (state.present[i]) {
      const auto &key = state.keys[i];
      property->addKeyFrame(time, state.values[i], key.interpolation,
                            key.cp1_x, key.cp1_y, key.cp2_x, key.cp2_y,
                            key.roving);
      property->setKeyFrameAnchorAt(time, key.anchor);
      property->setKeyFrameColorLabelAt(time, key.colorLabel);
    } else {
      property->removeKeyFrame(time);
    }
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  return true;
}

class MotionPathScaleUndoCommand final : public UndoCommand {
public:
  MotionPathScaleUndoCommand(ArtifactAbstractLayerPtr layer,
                             ArtifactCore::RationalTime time,
                             MotionPathScaleState before,
                             MotionPathScaleState after)
      : layer_(layer), time_(time), before_(before), after_(after) {}
  void undo() override { succeeded_ = applyMotionPathScale(layer_.lock(), time_, before_); }
  void redo() override { succeeded_ = applyMotionPathScale(layer_.lock(), time_, after_); }
  bool lastOperationSucceeded() const override { return succeeded_; }
  QString label() const override { return QStringLiteral("Scale Motion Path Frame"); }
private:
  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::RationalTime time_;
  MotionPathScaleState before_;
  MotionPathScaleState after_;
  bool succeeded_ = true;
};

enum class MotionPathSampleKind { Keyframe, Current };

struct MotionPathSample {
  QPointF position;
  MotionPathSampleKind kind = MotionPathSampleKind::Keyframe;
  int64_t framePosition = -1;
};

struct MotionPathPositionSnapshot {
  bool hasPositionKey = false;
  float x = 0.0f;
  float y = 0.0f;
};

struct MotionPathKeySnapshot {
  int64_t frame = 0;
  MotionPathPositionSnapshot value;
  bool hasTangents = false;
  ArtifactCore::PositionSpatialTangents tangents;
};

class MotionPathGroupUndoCommand final : public UndoCommand {
public:
  MotionPathGroupUndoCommand(ArtifactAbstractLayerPtr layer,
                             int64_t timeScale,
                             QVector<MotionPathKeySnapshot> before,
                             QVector<MotionPathKeySnapshot> after)
      : layer_(layer), before_(std::move(before)), timeScale_(timeScale), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Transform Motion Path Keys");
  }

private:
  bool apply(const QVector<MotionPathKeySnapshot> &snapshots) {
    auto layer = layer_.lock();
    if (!layer) return false;
    auto &transform = layer->transform3D();
    for (const auto &snapshot : snapshots) {
      const auto time = ArtifactCore::RationalTime(snapshot.frame, timeScale_);
      if (snapshot.value.hasPositionKey) {
        transform.setPositionKeyFrameValueAt(time, snapshot.value.x,
                                              snapshot.value.y);
        if (snapshot.hasTangents) {
          transform.setPositionKeyFrameSpatialTangentsAt(time,
                                                         snapshot.tangents);
        }
      } else {
        transform.removePositionKeyFrameAt(time);
      }
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  QVector<MotionPathKeySnapshot> before_;
  int64_t timeScale_ = 1;
  QVector<MotionPathKeySnapshot> after_;
  bool lastOperationSucceeded_ = true;
};

struct MotionPathInterpolationSnapshot {
  bool hasPositionKey = false;
  int xInterpolation = 0;
  int yInterpolation = 0;
};

enum class MotionPathTangentHandle { None, In, Out };
enum class MotionPathGroupTransform { Translate, Rotate, Scale };

struct MotionPathTangentSnapshot {
  bool present = false;
  ArtifactCore::PositionSpatialTangents tangents;
};

class MotionPathUndoCommand final : public UndoCommand {
public:
  MotionPathUndoCommand(ArtifactAbstractLayerPtr layer, ArtifactCore::RationalTime time,
                        MotionPathPositionSnapshot before,
                        MotionPathPositionSnapshot after)
      : layer_(layer), time_(time), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Move Motion Path Keyframe");
  }

private:
  bool apply(const MotionPathPositionSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;

    const auto &time = time_;
    auto &t3d = layer->transform3D();
    if (snapshot.hasPositionKey) {
      t3d.setPositionKeyFrameValueAt(time, snapshot.x, snapshot.y);
    } else {
      t3d.removePositionKeyFrameAt(time);
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::RationalTime time_;
  MotionPathPositionSnapshot before_;
  MotionPathPositionSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

class MotionPathTangentUndoCommand final : public UndoCommand {
public:
  MotionPathTangentUndoCommand(ArtifactAbstractLayerPtr layer, ArtifactCore::RationalTime time,
                               MotionPathTangentSnapshot before,
                               MotionPathTangentSnapshot after)
      : layer_(layer), time_(time), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Edit Motion Path Tangent");
  }

private:
  bool apply(const MotionPathTangentSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto &time = time_;
    auto &t3d = layer->transform3D();
    if (snapshot.present) {
      t3d.setPositionKeyFrameSpatialTangentsAt(time, snapshot.tangents);
    } else {
      t3d.removePositionKeyFrameSpatialTangentsAt(time);
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
  ArtifactCore::RationalTime time_;
  MotionPathTangentSnapshot before_;
  MotionPathTangentSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

class MotionPathInterpolationUndoCommand final : public UndoCommand {
public:
  MotionPathInterpolationUndoCommand(ArtifactAbstractLayerPtr layer,
                                     ArtifactCore::RationalTime time,
                                     MotionPathInterpolationSnapshot before,
                                     MotionPathInterpolationSnapshot after)
      : layer_(layer), time_(time), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Change Motion Path Interpolation");
  }

private:
  bool apply(const MotionPathInterpolationSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;

    const auto &time = time_;
    auto &t3d = layer->transform3D();
    if (!snapshot.hasPositionKey || !t3d.hasPositionKeyFrameAt(time)) {
      return false;
    }

    const auto xInterp = static_cast<ArtifactCore::InterpolationType>(
        snapshot.xInterpolation);
    const auto yInterp = static_cast<ArtifactCore::InterpolationType>(
        snapshot.yInterpolation);
    t3d.setPositionKeyFrameInterpolationAt(time, xInterp, yInterp);
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  ArtifactCore::RationalTime time_;
  MotionPathInterpolationSnapshot before_;
  MotionPathInterpolationSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
