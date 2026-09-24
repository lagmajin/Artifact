module;

#include <QPointF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

export module Artifact.Widgets.CompositionEditUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.Shape;
import Event.Bus;
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

class ShapePathVertexEditCommand final : public UndoCommand {
public:
  ShapePathVertexEditCommand(
      ArtifactAbstractLayerPtr layer, std::vector<CustomPathVertex> before,
      std::vector<CustomPathVertex> after, bool beforeClosed, bool afterClosed,
      QJsonObject beforeGeometry = {}, QJsonObject afterGeometry = {})
      : layer_(std::move(layer)), before_(std::move(before)),
        after_(std::move(after)), beforeClosed_(beforeClosed),
        afterClosed_(afterClosed), beforeGeometry_(std::move(beforeGeometry)),
        afterGeometry_(std::move(afterGeometry)) {
    auto locked = layer_.lock();
    auto* shape = locked ? dynamic_cast<ArtifactShapeLayer*>(locked.get()) : nullptr;
    if (shape) {
      if (afterGeometry_.isEmpty()) afterGeometry_ = shape->customGeometrySnapshot();
      if (beforeGeometry_.isEmpty()) {
        beforeGeometry_ = afterGeometry_;
        beforeGeometry_[QStringLiteral("path")] =
            pathSnapshot(before_, beforeClosed_);
      }
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
    const bool reverse = action == QStringLiteral("undo");
    const QJsonObject& expected = reverse ? afterGeometry_ : beforeGeometry_;
    const QJsonObject& value = reverse ? beforeGeometry_ : afterGeometry_;
    if (QJsonDocument(expected).toJson(QJsonDocument::Compact).size() > 262144 ||
        QJsonDocument(value).toJson(QJsonDocument::Compact).size() > 262144)
      return false;
    operationType = QStringLiteral("layer.shapePath");
    operationLayerId = layer->id().toString();
    payload = QJsonObject{{QStringLiteral("expected"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override { return QStringLiteral("Edit Path Vertices"); }

private:
  static QJsonObject pathSnapshot(const std::vector<CustomPathVertex>& vertices,
                                  bool closed) {
    QJsonArray values;
    for (const auto& vertex : vertices) {
      values.append(QJsonArray{vertex.pos.x(), vertex.pos.y(),
                               vertex.inTangent.x(), vertex.inTangent.y(),
                               vertex.outTangent.x(), vertex.outTangent.y(),
                               vertex.smooth});
    }
    return QJsonObject{{QStringLiteral("vertices"), values},
                       {QStringLiteral("closed"), closed}};
  }
  bool apply(const QJsonObject& expected, const QJsonObject& value,
             bool allowPreApplied) {
    auto layer = layer_.lock();
    if (!layer) return false;
    auto *shape = dynamic_cast<ArtifactShapeLayer *>(layer.get());
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
  std::vector<CustomPathVertex> before_;
  std::vector<CustomPathVertex> after_;
  bool beforeClosed_ = false;
  bool afterClosed_ = false;
  QJsonObject beforeGeometry_;
  QJsonObject afterGeometry_;
  bool lastOperationSucceeded_ = true;
  bool firstRedo_ = true;
};

class LineEndpointUndoCommand final : public UndoCommand {
public:
  LineEndpointUndoCommand(ArtifactAbstractLayerPtr layer, int bw, int bh,
                          QPointF bp, float br, int aw, int ah, QPointF ap,
                          float ar)
      : layer_(std::move(layer)), bw_(bw), bh_(bh), bp_(bp), br_(br),
        aw_(aw), ah_(ah), ap_(ap), ar_(ar) {}

  void undo() override { lastOperationSucceeded_ = apply(bw_, bh_, bp_, br_); }
  void redo() override { lastOperationSucceeded_ = apply(aw_, ah_, ap_, ar_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Edit Line Endpoint"); }

private:
  bool apply(int w, int h, const QPointF &pos, float rot) {
    auto layer = layer_.lock();
    auto *shape = layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
    if (!shape || shape->shapeType() != ShapeType::Line) return false;
    shape->setSize(std::max(1, w), std::max(1, h));
    auto &transform = shape->transform3D();
    const auto time = transformTime(layer, layer->currentFrame());
    transform.setPosition(time, static_cast<float>(pos.x()),
                          static_cast<float>(pos.y()));
    transform.setRotation(time, rot);
    shape->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  int bw_ = 0;
  int bh_ = 0;
  int aw_ = 0;
  int ah_ = 0;
  QPointF bp_;
  QPointF ap_;
  float br_ = 0.0f;
  float ar_ = 0.0f;
  bool lastOperationSucceeded_ = true;
};

class CameraPoiUndoCommand final : public UndoCommand {
public:
  CameraPoiUndoCommand(ArtifactAbstractLayerPtr layer, QVector3D before,
                       QVector3D after)
      : layer_(layer), before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return QStringLiteral("Move Camera Point of Interest");
  }

private:
  bool apply(const QVector3D &poi) {
    auto layer = layer_.lock();
    if (!layer) return false;
    if (auto *camera = dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
      camera->setPointOfInterest(poi);
      publishLayerModified(layer);
    } else {
      return false;
    }
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    return true;
  }

  static void publishLayerModified(const ArtifactAbstractLayerPtr &layer) {
    layer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }

  ArtifactAbstractLayerWeak layer_;
  QVector3D before_;
  QVector3D after_;
  bool lastOperationSucceeded_ = true;
};

class TransformFieldUndoCommand final : public UndoCommand {
public:
  TransformFieldUndoCommand(ArtifactCompositionWeakPtr composition,
                            CompositionTransformField before,
                            CompositionTransformField after)
      : composition_(std::move(composition)), before_(std::move(before)),
        after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Edit Live Field"); }

private:
  bool apply(const CompositionTransformField &field) {
    const auto composition = composition_.lock();
    if (!composition) return false;
    composition->addTransformField(field);
    const auto fields = composition->transformFields();
    const bool present = std::any_of(
        fields.cbegin(), fields.cend(),
        [&field](const CompositionTransformField &candidate) {
          return candidate.fieldId == field.fieldId;
        });
    if (!present) return false;
    composition->changed();
    if (auto *mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    return true;
  }

  ArtifactCompositionWeakPtr composition_;
  CompositionTransformField before_;
  CompositionTransformField after_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
