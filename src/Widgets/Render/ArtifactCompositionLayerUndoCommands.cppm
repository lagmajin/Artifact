module;

#include <QPointF>
#include <QString>
#include <QVector3D>

#include <cmath>
#include <cstdint>

export module Artifact.Widgets.CompositionLayerUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Event.Bus;
import Memory.SharedPtr;
import Undo.UndoManager;

export namespace Artifact {

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

} // namespace

class AnchorPointUndoCommand final : public UndoCommand {
public:
  AnchorPointUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame,
                         QVector3D beforeAnchor, QVector3D beforePosition,
                         QVector3D afterAnchor, QVector3D afterPosition)
      : layer_(layer), frame_(frame), beforeAnchor_(beforeAnchor),
        beforePosition_(beforePosition), afterAnchor_(afterAnchor),
        afterPosition_(afterPosition) {}

  void undo() override {
    lastOperationSucceeded_ = apply(beforeAnchor_, beforePosition_);
  }
  void redo() override {
    lastOperationSucceeded_ = apply(afterAnchor_, afterPosition_);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Reset Anchor Point"); }

private:
  bool apply(const QVector3D &anchor, const QVector3D &position) {
    auto layer = layer_.lock();
    if (!layer || !layer->is3D()) return false;
    const auto time = transformTime(layer, frame_);
    auto &transform = layer->transform3D();
    transform.setAnchor(time, anchor.x(), anchor.y(), anchor.z());
    transform.setPosition(time, position.x(), position.y());
    const auto actual = transform.snapshotAt(time);
    if (std::abs(actual.anchorX - anchor.x()) > 0.000001f ||
        std::abs(actual.anchorY - anchor.y()) > 0.000001f ||
        std::abs(actual.anchorZ - anchor.z()) > 0.000001f ||
        std::abs(actual.positionX - position.x()) > 0.000001f ||
        std::abs(actual.positionY - position.y()) > 0.000001f) {
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
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  int64_t frame_ = 0;
  QVector3D beforeAnchor_;
  QVector3D beforePosition_;
  QVector3D afterAnchor_;
  QVector3D afterPosition_;
  bool lastOperationSucceeded_ = true;
};

class AnchorPoint2DUndoCommand final : public UndoCommand {
public:
  AnchorPoint2DUndoCommand(ArtifactAbstractLayerPtr layer, QPointF beforeAnchor,
                           QPointF beforePosition, QPointF afterAnchor,
                           QPointF afterPosition)
      : layer_(layer), beforeAnchor_(beforeAnchor),
        beforePosition_(beforePosition), afterAnchor_(afterAnchor),
        afterPosition_(afterPosition) {}

  void undo() override {
    lastOperationSucceeded_ = apply(beforeAnchor_, beforePosition_);
  }
  void redo() override {
    lastOperationSucceeded_ = apply(afterAnchor_, afterPosition_);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Reset Anchor Point"); }

private:
  bool apply(const QPointF &anchor, const QPointF &position) {
    auto layer = layer_.lock();
    if (!layer || layer->is3D()) return false;
    auto &transform = layer->transform2D();
    (void)anchor;
    transform.setPosition(static_cast<float>(position.x()),
                          static_cast<float>(position.y()));
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  QPointF beforeAnchor_;
  QPointF beforePosition_;
  QPointF afterAnchor_;
  QPointF afterPosition_;
  bool lastOperationSucceeded_ = true;
};

class ShapeCornerRadiusUndoCommand final : public UndoCommand {
public:
  ShapeCornerRadiusUndoCommand(ArtifactAbstractLayerPtr layer, float before,
                               float after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Adjust Shape Corner Radius");
  }

private:
  bool apply(float radius) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    shape->setCornerRadius(radius);
    if (std::abs(shape->cornerRadius() - radius) > 0.000001f) return false;
    shape->setDirty(LayerDirtyFlag::Property);
    shape->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            shape->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), shape->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  float before_ = 0.0f;
  float after_ = 0.0f;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
