module;

#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVector3D>

#include <cmath>
#include <algorithm>
#include <cstdint>
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
  return ArtifactCore::RationalTime(
      frame, std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps))));
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

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("3D Gizmo Transform"); }

private:
  bool apply(const GizmoTransformSnapshot &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;
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

// VP crop drags drive the sourceCrop.* property paths (clamping and keyframe
// semantics stay identical to numeric edits). Snapshots are whole values.
class SourceCropRectUndoCommand final : public UndoCommand {
 public:
  SourceCropRectUndoCommand(ArtifactAbstractLayerPtr layer, SourceCrop before,
                            SourceCrop after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Crop Image Layer"); }

 private:
  bool apply(const SourceCrop &snapshot) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto imageLayer =
        ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
    if (!imageLayer) return false;
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                       snapshot.enabled());
    const QRectF rect = snapshot.cropRect();
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                       rect.x());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                       rect.y());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"),
                                       rect.width());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"),
                                       rect.height());
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
  bool lastOperationSucceeded_ = true;
};

// VP solid-size drags. Size is not keyframable: plain before/after values.
class SolidSizeUndoCommand final : public UndoCommand {
 public:
  SolidSizeUndoCommand(ArtifactAbstractLayerPtr layer, QSize before,
                       QSize after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Resize Solid Layer"); }

 private:
  bool apply(const QSize &size) {
    auto layer = layer_.lock();
    if (!layer) return false;
    const auto solidLayer =
        ArtifactCore::dynamicPointerCast<ArtifactSolid2DLayer>(layer);
    if (!solidLayer) return false;
    if (size.width() < 1 || size.height() < 1) return false;
    solidLayer->setSize(size.width(), size.height());
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

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Edit Solid Gradient"); }

 private:
  bool apply(const AxisState &snapshot) {
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
    const auto applyPath = [&](const QString &path, double value) {
      if (solid2D) {
        solid2D->setLayerPropertyValue(path, value);
      } else {
        solidImage->setLayerPropertyValue(path, value);
      }
    };
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
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
