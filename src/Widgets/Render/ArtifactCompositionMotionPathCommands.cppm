module;

#include <QPointF>
#include <QString>
#include <QVector>

#include <cstdint>
#include <utility>

export module Artifact.Widgets.CompositionMotionPathCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Event.Bus;
import Time.Rational;
import Undo.UndoManager;

export namespace Artifact {

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
                             QVector<MotionPathKeySnapshot> before,
                             QVector<MotionPathKeySnapshot> after)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)) {}

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
      const auto time = ArtifactCore::RationalTime(snapshot.frame, 24);
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
  MotionPathUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                        MotionPathPositionSnapshot before,
                        MotionPathPositionSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

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

    const ArtifactCore::RationalTime time(frame_, 24);
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
  int64_t frame_ = 0;
  MotionPathPositionSnapshot before_;
  MotionPathPositionSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

class MotionPathTangentUndoCommand final : public UndoCommand {
public:
  MotionPathTangentUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                               MotionPathTangentSnapshot before,
                               MotionPathTangentSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

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
    const ArtifactCore::RationalTime time(frame_, 24);
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
  int64_t frame_ = 0;
  MotionPathTangentSnapshot before_;
  MotionPathTangentSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

class MotionPathInterpolationUndoCommand final : public UndoCommand {
public:
  MotionPathInterpolationUndoCommand(ArtifactAbstractLayerPtr layer,
                                     int64_t frame,
                                     MotionPathInterpolationSnapshot before,
                                     MotionPathInterpolationSnapshot after)
      : layer_(layer), frame_(frame), before_(before), after_(after) {}

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

    const ArtifactCore::RationalTime time(frame_, 24);
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
  int64_t frame_ = 0;
  MotionPathInterpolationSnapshot before_;
  MotionPathInterpolationSnapshot after_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
