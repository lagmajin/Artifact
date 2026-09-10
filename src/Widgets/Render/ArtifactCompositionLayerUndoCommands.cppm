module;

#include <QPointF>
#include <QString>
#include <QVector3D>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

export module Artifact.Widgets.CompositionLayerUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Event.Bus;
import Memory.SharedPtr;
import Time.Rational;
import Undo.UndoManager;

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

} // namespace

} // namespace Artifact

export namespace Artifact {

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

class ShapeStarInnerRadiusUndoCommand final : public UndoCommand {
 public:
  ShapeStarInnerRadiusUndoCommand(ArtifactAbstractLayerPtr layer, float before,
                                  float after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Adjust Shape Star Inner Radius");
  }

 private:
  bool apply(float radius) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    shape->setStarInnerRadius(radius);
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

class ShapePolygonPointsUndoCommand final : public UndoCommand { public:
  ShapePolygonPointsUndoCommand(ArtifactAbstractLayerPtr layer,
                                 std::vector<QPointF> beforePoints,
                                 std::vector<QPointF> afterPoints,
                                 bool beforeClosed, bool afterClosed)
      : layer_(layer),
        beforePoints_(std::move(beforePoints)),
        afterPoints_(std::move(afterPoints)),
        beforeClosed_(beforeClosed),
        afterClosed_(afterClosed) {}

  void undo() override { lastOperationSucceeded_ = apply(beforePoints_, beforeClosed_); }
  void redo() override { lastOperationSucceeded_ = apply(afterPoints_, afterClosed_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Edit Shape Polygon");
  }

 private:
  bool apply(const std::vector<QPointF> &points, bool closed) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    if (points.size() >= 3) shape->setCustomPolygonPoints(points, closed);
    else shape->clearCustomPolygonPoints();
    shape->setDirty(LayerDirtyFlag::Source);
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
  std::vector<QPointF> beforePoints_;
  std::vector<QPointF> afterPoints_;
  bool beforeClosed_ = true;
  bool afterClosed_ = true;
  bool lastOperationSucceeded_ = true;
};

// F5: viewport drag of a single numeric shape-operator field. Values travel
// as doubles through shapeOperatorValue/setLayerPropertyValue so int fields
// (e.g. repeater copies) and float fields share one command type.
class ShapeOperatorValueUndoCommand final : public UndoCommand {
 public:
  ShapeOperatorValueUndoCommand(ArtifactAbstractLayerPtr layer, int opIndex,
                                 QString field, double before, double after)
      : layer_(layer),
        opIndex_(opIndex),
        field_(std::move(field)),
        before_(before),
        after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Edit Shape Operator");
  }

 private:
  bool apply(double value) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape || opIndex_ < 0 || opIndex_ >= shape->shapeOperatorCount()) {
      return false;
    }
    const QString path =
        QStringLiteral("shape.operator.%1.%2").arg(opIndex_).arg(field_);
    if (!shape->setLayerPropertyValue(path, QVariant(value))) {
      return false;
    }
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
  int opIndex_ = -1;
  QString field_;
  double before_ = 0.0;
  double after_ = 0.0;
  bool lastOperationSucceeded_ = true;
};

// F9: SVG/vector import appends parsed contents. Undo removes exactly the
// appended tail (restoring the pre-import count); redo re-appends the stored
// contents. The first append may snapshot the legacy shape as contents[0];
// trimming back to beforeCount() removes that snapshot too, which restores
// the empty-contents state with legacy parameters intact.
class ShapeSvgImportUndoCommand final : public UndoCommand {
 public:
  ShapeSvgImportUndoCommand(ArtifactAbstractLayerPtr layer, int beforeCount,
                             std::vector<Artifact::ShapeContent> added)
      : layer_(layer),
        beforeCount_(beforeCount),
        added_(std::move(added)) {}

  void undo() override { lastOperationSucceeded_ = applyUndo(); }
  void redo() override { lastOperationSucceeded_ = applyRedo(); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Import SVG into Shape");
  }

 private:
  Artifact::ArtifactAbstractLayerPtr lockedShape() const {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    return shape;
  }

  bool applyUndo() {
    auto shape = lockedShape();
    if (!shape) return false;
    while (shape->shapeContentCount() > beforeCount_) {
      if (!shape->removeShapeContentAt(shape->shapeContentCount() - 1)) {
        return false;
      }
    }
    shape->changed();
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  bool applyRedo() {
    auto shape = lockedShape();
    if (!shape) return false;
    for (const auto &content : added_) {
      if (shape->addShapeContent(content) < 0) {
        return false;
      }
    }
    shape->changed();
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  int beforeCount_ = 0;
  std::vector<Artifact::ShapeContent> added_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
