module;

#include <QPointF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
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
  // The layer owns the frame domain its transform keys are stored in; asking
  // the layer keeps undo in sync with the live drag and the timeline.
  return layer ? layer->keyframeTimeAtFrame(frame)
               : ArtifactCore::RationalTime(frame, 24);
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
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const float expected = reverse ? after_ : before_;
    const float value = reverse ? before_ : after_;
    if (!std::isfinite(expected) || !std::isfinite(value)) return false;
    operationType = QStringLiteral("property.set");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{{QStringLiteral("propertyPath"),
                           QStringLiteral("shape.cornerRadius")},
                          {QStringLiteral("expectedValue"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override {
    return QStringLiteral("Adjust Shape Corner Radius");
  }

private:
  bool apply(float expected, float radius, bool allowPreApplied) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    const float current = shape->cornerRadius();
    if (current == radius && allowPreApplied) return true;
    if (current != expected) return false;
    shape->setCornerRadius(radius);
    if (shape->cornerRadius() != radius) {
      shape->setCornerRadius(expected);
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
  float before_ = 0.0f;
  float after_ = 0.0f;
  bool lastOperationSucceeded_ = true;
  bool firstRedo_ = true;
};

class ShapeStarInnerRadiusUndoCommand final : public UndoCommand {
 public:
  ShapeStarInnerRadiusUndoCommand(ArtifactAbstractLayerPtr layer, float before,
                                  float after)
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
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const float expected = reverse ? after_ : before_;
    const float value = reverse ? before_ : after_;
    if (!std::isfinite(expected) || !std::isfinite(value)) return false;
    operationType = QStringLiteral("property.set");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{{QStringLiteral("propertyPath"),
                           QStringLiteral("shape.starInnerRadius")},
                          {QStringLiteral("expectedValue"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override {
    return QStringLiteral("Adjust Shape Star Inner Radius");
  }

 private:
  bool apply(float expected, float radius, bool allowPreApplied) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    const float current = shape->starInnerRadius();
    if (current == radius && allowPreApplied) return true;
    if (current != expected) return false;
    shape->setStarInnerRadius(radius);
    if (shape->starInnerRadius() != radius) {
      shape->setStarInnerRadius(expected);
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
  float before_ = 0.0f;
  float after_ = 0.0f;
  bool lastOperationSucceeded_ = true;
  bool firstRedo_ = true;
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
        afterClosed_(afterClosed) {
    const auto locked = layer_.lock();
    const auto shape = locked
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(locked)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (shape) {
      afterGeometry_ = shape->customGeometrySnapshot();
      beforeGeometry_ = afterGeometry_;
      beforeGeometry_[QStringLiteral("polygon")] =
          polygonSnapshot(beforePoints_, beforeClosed_);
    }
  }

  void undo() override {
    lastOperationSucceeded_ = apply(afterGeometry_, beforeGeometry_, false);
  }
  void redo() override {
    const bool allowPreApplied = firstRedo_;
    firstRedo_ = false;
    lastOperationSucceeded_ = apply(beforeGeometry_, afterGeometry_,
                                    allowPreApplied);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toQString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const QJsonObject& expected = reverse ? afterGeometry_ : beforeGeometry_;
    const QJsonObject& value = reverse ? beforeGeometry_ : afterGeometry_;
    if (QJsonDocument(expected).toJson(QJsonDocument::Compact).size() > 262144 ||
        QJsonDocument(value).toJson(QJsonDocument::Compact).size() > 262144)
      return false;
    operationType = QStringLiteral("layer.shapePath");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{{QStringLiteral("expected"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override {
    return QStringLiteral("Edit Shape Polygon");
  }

 private:
  static QJsonObject polygonSnapshot(const std::vector<QPointF>& points,
                                    bool closed) {
    QJsonArray values;
    for (const QPointF& point : points)
      values.append(QJsonArray{point.x(), point.y()});
    return QJsonObject{{QStringLiteral("points"), values},
                       {QStringLiteral("closed"), closed}};
  }
  bool apply(const QJsonObject& expected, const QJsonObject& value,
             bool allowPreApplied) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape) return false;
    if (allowPreApplied && shape->customGeometrySnapshot() == value) return true;
    if (shape->customGeometrySnapshot() != expected) return false;
    if (!shape->restoreCustomGeometrySnapshot(value) ||
        shape->customGeometrySnapshot() != value) {
      if (shape->customGeometrySnapshot() != expected)
        shape->restoreCustomGeometrySnapshot(expected);
      return false;
    }
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
  bool firstRedo_ = true;
  QJsonObject beforeGeometry_;
  QJsonObject afterGeometry_;
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

  void undo() override {
    lastOperationSucceeded_ = apply(after_, before_, false);
  }
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
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || opIndex_ < 0 || field_.isEmpty() ||
        !std::isfinite(before_) || !std::isfinite(after_) ||
        (action != QStringLiteral("push") &&
         action != QStringLiteral("undo") &&
         action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    operationType = QStringLiteral("layer.shapeOperator");
    operationLayerId = layer->id().toQString();
    payload = QJsonObject{
        {QStringLiteral("operatorIndex"), opIndex_},
        {QStringLiteral("field"), field_},
        {QStringLiteral("expectedValue"), reverse ? after_ : before_},
        {QStringLiteral("value"), reverse ? before_ : after_}};
    return true;
  }
  QString label() const override {
    return QStringLiteral("Edit Shape Operator");
  }

 private:
  QString propertyPath() const {
    return QStringLiteral("shape.operator.%1.%2").arg(opIndex_).arg(field_);
  }
  bool apply(double expected, double value, bool allowPreApplied) {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    if (!shape || opIndex_ < 0 || opIndex_ >= shape->shapeOperatorCount()) {
      return false;
    }
    const double current = shape->shapeOperatorValue(opIndex_, field_).toDouble();
    if (allowPreApplied && current == value) return true;
    if (current != expected) return false;
    if (!shape->setLayerPropertyValue(propertyPath(), QVariant(value)) ||
        shape->shapeOperatorValue(opIndex_, field_).toDouble() != value) {
      shape->setLayerPropertyValue(propertyPath(), QVariant(expected));
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
  bool firstRedo_ = true;
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
        added_(std::move(added)) {
    const auto shape = lockedShape();
    if (shape && shape->shapeContentCount() == beforeCount_)
      before_ = shape->shapeContentsSnapshot();
  }

  void undo() override { lastOperationSucceeded_ = applyUndo(); }
  void redo() override { lastOperationSucceeded_ = applyRedo(); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Import SVG into Shape");
  }
  QStringList collaborationTargetLayerIds() const override {
    const auto layer = layer_.lock();
    return layer ? QStringList{layer->id().toQString()} : QStringList{};
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType,
                                   QString& operationLayerId,
                                   QJsonObject& payload) const override {
    const auto layer = layer_.lock();
    if (!layer || (action != QStringLiteral("push") &&
                   action != QStringLiteral("undo") &&
                   action != QStringLiteral("redo")) || before_.isEmpty() ||
        after_.isEmpty())
      return false;
    const bool reverse = action == QStringLiteral("undo");
    operationType = QStringLiteral("layer.shapeContents");
    operationLayerId = layer->id().toQString();
    payload = {{QStringLiteral("expected"), reverse ? after_ : before_},
               {QStringLiteral("value"), reverse ? before_ : after_}};
    return QJsonDocument(payload).toJson(QJsonDocument::Compact).size() <= 524288;
  }

 private:
  ArtifactCore::SharedPtr<ArtifactShapeLayer> lockedShape() const {
    auto layer = layer_.lock();
    auto shape = layer
        ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
        : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
    return shape;
  }

  bool applyUndo() {
    auto shape = lockedShape();
    if (!shape || after_.isEmpty() ||
        shape->shapeContentsSnapshot() != after_ ||
        !shape->restoreShapeContentsSnapshot(before_) ||
        shape->shapeContentsSnapshot() != before_) return false;
    shape->changed();
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  bool applyRedo() {
    auto shape = lockedShape();
    if (!shape || before_.isEmpty()) return false;
    if (!after_.isEmpty()) {
      if (shape->shapeContentsSnapshot() != before_ ||
          !shape->restoreShapeContentsSnapshot(after_) ||
          shape->shapeContentsSnapshot() != after_) return false;
      shape->changed();
      if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
      return true;
    }
    for (const auto &content : added_) {
      if (shape->addShapeContent(content) < 0) {
        shape->restoreShapeContentsSnapshot(before_);
        return false;
      }
    }
    after_ = shape->shapeContentsSnapshot();
    shape->changed();
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  int beforeCount_ = 0;
  std::vector<Artifact::ShapeContent> added_;
  QJsonObject before_;
  QJsonObject after_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
