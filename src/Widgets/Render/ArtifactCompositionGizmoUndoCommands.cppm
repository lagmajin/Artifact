module;

#include <QMatrix4x4>
#include <QString>
#include <QVariant>
#include <QVector3D>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

export module Artifact.Widgets.CompositionGizmoUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Event.Bus;
import Time.Rational;
import Undo.UndoManager;

export namespace Artifact {

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
};

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
  double fps = 24.0;
  if (layer) {
    if (auto *composition = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      const double candidate = composition->frameRate().framerate();
      if (candidate > 0.0) fps = candidate;
    }
  }
  return ArtifactCore::RationalTime(frame, fps);
}

void applyPlanarTransform(const ArtifactAbstractLayerPtr &layer, int64_t frame,
                          const GizmoTransformSnapshot &snapshot) {
  if (!layer) return;
  auto &transform = layer->transform3D();
  const auto time = transformTime(layer, frame);
  if (snapshot.hasPositionKey) {
    const float initialX = transform.positionX() - transform.positionXAt(time);
    const float initialY = transform.positionY() - transform.positionYAt(time);
    transform.setPosition(time, snapshot.position.x() - initialX,
                          snapshot.position.y() - initialY);
  } else {
    transform.removePositionKeyFrameAt(time);
    if (!snapshot.positionAnimated) {
      transform.setInitialPosition(time, snapshot.position.x(),
                                   snapshot.position.y());
    }
  }
  if (snapshot.hasRotationKey) {
    transform.setRotation(time, snapshot.rotation.z());
  } else {
    transform.removeRotationKeyFrameAt(time);
    if (!snapshot.rotationAnimated) {
      transform.setInitialRotation(time, snapshot.rotation.z());
    }
  }
  if (snapshot.hasScaleKey) {
    transform.setScale(time, snapshot.scale.x(), snapshot.scale.y());
  } else {
    transform.removeScaleKeyFrameAt(time);
    if (!snapshot.scaleAnimated) {
      transform.setInitialScale(time, snapshot.scale.x(), snapshot.scale.y());
    }
  }
}

void restorePropertyKeyState(const ArtifactAbstractLayerPtr &layer,
                             int64_t frame,
                             const GizmoTransformSnapshot &snapshot) {
  if (!layer) return;
  const auto time = transformTime(layer, frame);
  const auto restore = [&](const QString &path, bool hasKey,
                           const QVariant &value) {
    const auto property = layer->getProperty(path);
    if (!property || !property->isAnimatable()) return;
    if (hasKey) property->addKeyFrame(time, value);
    else property->removeKeyFrame(time);
  };
  restore(QStringLiteral("transform.position.x"), snapshot.hasPositionKey,
          snapshot.position.x());
  restore(QStringLiteral("transform.position.y"), snapshot.hasPositionKey,
          snapshot.position.y());
  if (snapshot.is3D) {
    restore(QStringLiteral("transform.position.z"), snapshot.hasPositionKey,
            snapshot.position.z());
  }
  restore(QStringLiteral("transform.rotation"), snapshot.hasRotationKey,
          snapshot.is3D ? snapshot.rotation.x() : snapshot.rotation.z());
  restore(QStringLiteral("transform.scale.x"), snapshot.hasScaleKey,
          snapshot.scale.x());
  restore(QStringLiteral("transform.scale.y"), snapshot.hasScaleKey,
          snapshot.scale.y());
  if (snapshot.is3D) {
    restore(QStringLiteral("transform.scale.z"), snapshot.hasScaleKey,
            snapshot.scale.z());
  }
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

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("3D Gizmo Transform"); }

private:
  bool apply(const GizmoTransformSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;
    if (snapshot.is3D) {
      auto &transform = layer->transform3D();
      const auto time = transformTime(layer, frame_);
      if (snapshot.hasPositionKey) {
        const float initialX = transform.positionX() - transform.positionXAt(time);
        const float initialY = transform.positionY() - transform.positionYAt(time);
        transform.setPosition(time, snapshot.position.x() - initialX,
                              snapshot.position.y() - initialY);
      } else {
        transform.removePositionKeyFrameAt(time);
        if (!snapshot.positionAnimated) {
          transform.setInitialPosition(time, snapshot.position.x(),
                                       snapshot.position.y());
        }
      }
      transform.setCurrentPositionZ(snapshot.position.z());
      if (snapshot.hasRotationKey) {
        transform.setRotationX(time, snapshot.rotation.x());
        transform.setRotationY(time, snapshot.rotation.y());
        transform.setRotationZ(time, snapshot.rotation.z());
      } else {
        transform.removeRotationKeyFrameAt(time);
        if (!snapshot.rotationAnimated) {
          transform.setCurrentRotationX(snapshot.rotation.x());
          transform.setCurrentRotationY(snapshot.rotation.y());
          transform.setInitialRotation(time, snapshot.rotation.z());
        }
      }
      if (snapshot.hasScaleKey) {
        transform.setScale(time, snapshot.scale.x(), snapshot.scale.y());
      } else {
        transform.removeScaleKeyFrameAt(time);
        if (!snapshot.scaleAnimated) {
          transform.setInitialScale(time, snapshot.scale.x(),
                                    snapshot.scale.y());
        }
      }
      if (std::abs(transform.snapshotAt(time).scaleZ - snapshot.scale.z()) >
          0.000001f) {
        transform.setScale(time, snapshot.scale.x(), snapshot.scale.y(),
                           snapshot.scale.z());
      }
    } else {
      applyPlanarTransform(layer, frame_, snapshot);
    }
    restorePropertyKeyState(layer, frame_, snapshot);
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
  void redo() override { lastOperationSucceeded_ = apply(true); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Transform Selected Layers");
  }

private:
  bool apply(bool useAfter) {
    bool succeeded = true;
    for (const auto &entry : entries_) {
      auto layer = entry.layer.lock();
      if (!layer) {
        succeeded = false;
        continue;
      }
      const auto &snapshot = useAfter ? entry.after : entry.before;
      if (snapshot.is3D) {
        auto &transform = layer->transform3D();
        const auto time = transformTime(layer, entry.frame);
        if (snapshot.hasPositionKey) {
          const float initialX = transform.positionX() - transform.positionXAt(time);
          const float initialY = transform.positionY() - transform.positionYAt(time);
          transform.setPosition(time, snapshot.position.x() - initialX,
                                snapshot.position.y() - initialY);
        } else {
          transform.removePositionKeyFrameAt(time);
          if (!snapshot.positionAnimated) {
            transform.setInitialPosition(time, snapshot.position.x(),
                                         snapshot.position.y());
          }
        }
        transform.setCurrentPositionZ(snapshot.position.z());
        if (snapshot.hasRotationKey) {
          transform.setRotationX(time, snapshot.rotation.x());
          transform.setRotationY(time, snapshot.rotation.y());
          transform.setRotationZ(time, snapshot.rotation.z());
        } else {
          transform.removeRotationKeyFrameAt(time);
          if (!snapshot.rotationAnimated) {
            transform.setCurrentRotationX(snapshot.rotation.x());
            transform.setCurrentRotationY(snapshot.rotation.y());
            transform.setInitialRotation(time, snapshot.rotation.z());
          }
        }
        if (snapshot.hasScaleKey) {
          transform.setScale(time, snapshot.scale.x(), snapshot.scale.y());
        } else {
          transform.removeScaleKeyFrameAt(time);
          if (!snapshot.scaleAnimated) {
            transform.setInitialScale(time, snapshot.scale.x(),
                                      snapshot.scale.y());
          }
        }
        if (std::abs(transform.snapshotAt(time).scaleZ - snapshot.scale.z()) >
            0.000001f) {
          transform.setScale(time, snapshot.scale.x(), snapshot.scale.y(),
                             snapshot.scale.z());
        }
      } else {
        applyPlanarTransform(layer, entry.frame, snapshot);
      }
      restorePropertyKeyState(layer, entry.frame, snapshot);
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
    return succeeded;
  }

  std::vector<GizmoGroupUndoEntry> entries_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
