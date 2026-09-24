module;

#include <QPointF>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QTransform>
#include <algorithm>

#include <utility>

export module Artifact.Widgets.CompositionTextPuppetUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Selection.Manager;
import Artifact.Layer.Text;
import Artifact.Render.IRenderer;
import Artifact.Tool.PuppetTool;
import Event.Bus;
import Undo.UndoManager;

export namespace Artifact {

class TextContentUndoCommand final : public UndoCommand {
public:
  TextContentUndoCommand(ArtifactAbstractLayerPtr layer, QString before,
                         QString after)
      : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
        before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    return layerId_.isEmpty() ? QStringList{} : QStringList{layerId_};
  }
  bool collaborationTargetScopeResolved() const override {
    return !layerId_.isEmpty() && !layer_.expired();
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType, QString& layerId,
                                   QJsonObject& payload) const override {
    if (layerId_.isEmpty() || layer_.expired() ||
        (action != QStringLiteral("push") && action != QStringLiteral("undo") &&
         action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const QString& expected = reverse ? after_ : before_;
    const QString& value = reverse ? before_ : after_;
    if (expected.toUtf8().size() > 131072 || value.toUtf8().size() > 131072) return false;
    operationType = QStringLiteral("layer.text");
    layerId = layerId_;
    payload = QJsonObject{{QStringLiteral("expected"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override { return QStringLiteral("Edit Text"); }

private:
  bool apply(const QString &value) {
    auto layer = layer_.lock();
    auto *textLayer = layer
        ? dynamic_cast<ArtifactTextLayer *>(layer.get())
        : nullptr;
    if (!textLayer) return false;
    textLayer->setText(UniString(value));
    if (textLayer->text().toQString() != value) return false;
    textLayer->changed();
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            textLayer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), textLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractLayerWeak layer_;
  QString layerId_;
  QString before_;
  QString after_;
  bool lastOperationSucceeded_ = true;
};

class PuppetPinUndoCommand final : public UndoCommand {
public:
  PuppetPinUndoCommand(ArtifactPuppetTool *tool, ArtifactIRenderer *renderer,
                       LayerID layerId, QString pinId, QPointF beforePosition,
                       QPointF afterPosition, float beforeRotation,
                       float afterRotation)
      : tool_(tool), renderer_(renderer), layerId_(layerId),
        pinId_(std::move(pinId)), beforePosition_(beforePosition),
        afterPosition_(afterPosition), beforeRotation_(beforeRotation),
        afterRotation_(afterRotation) {}

  void undo() override {
    lastOperationSucceeded_ = apply(beforePosition_, beforeRotation_);
  }
  void redo() override {
    lastOperationSucceeded_ = apply(afterPosition_, afterRotation_);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    return layerId_.isNil() ? QStringList{} : QStringList{layerId_.toString()};
  }
  bool collaborationTargetScopeResolved() const override {
    return !layerId_.isNil();
  }
  QString label() const override { return QStringLiteral("Move Puppet Pin"); }

private:
  bool apply(const QPointF &position, float rotation) {
    if (!tool_) return false;
    const LayerID layerId = tool_->pinLayerId(pinId_);
    if (layerId.isNil() || layerId != layerId_) return false;
    if (!tool_->movePin(pinId_, position)) return false;
    tool_->setPinRotation(pinId_, rotation);
    tool_->deformLayer(layerId, renderer_);
    tool_->persistLayerData(layerId_);
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactPuppetTool *tool_ = nullptr;
  ArtifactIRenderer *renderer_ = nullptr;
  LayerID layerId_;
  QString pinId_;
  QPointF beforePosition_;
  QPointF afterPosition_;
  float beforeRotation_ = 0.0f;
  float afterRotation_ = 0.0f;
  bool lastOperationSucceeded_ = true;
};

class Deformation2DStateUndoCommand final : public UndoCommand {
public:
  Deformation2DStateUndoCommand(ArtifactAbstractLayerPtr layer,
                                QJsonObject before, QJsonObject after,
                                QString label, ArtifactPuppetTool* tool)
      : layer_(layer), layerId_(layer ? layer->id().toString() : QString()),
        before_(std::move(before)), after_(std::move(after)),
        label_(std::move(label)), tool_(tool) {}
  void undo() override { lastOperationSucceeded_ = apply(after_, before_); }
  void redo() override { lastOperationSucceeded_ = apply(before_, after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    return layerId_.isEmpty() ? QStringList{} : QStringList{layerId_};
  }
  bool collaborationTargetScopeResolved() const override {
    return !layerId_.isEmpty();
  }
  bool buildCollaborationOperation(const QString& action,
                                   QString& operationType, QString& operationLayerId,
                                   QJsonObject& payload) const override {
    if (layerId_.isEmpty() || layer_.expired() ||
        (action != QStringLiteral("push") && action != QStringLiteral("undo") &&
         action != QStringLiteral("redo"))) return false;
    const bool reverse = action == QStringLiteral("undo");
    const QJsonObject& expected = reverse ? after_ : before_;
    const QJsonObject& value = reverse ? before_ : after_;
    if (QJsonDocument(expected).toJson(QJsonDocument::Compact).size() > 262144 ||
        QJsonDocument(value).toJson(QJsonDocument::Compact).size() > 262144) return false;
    operationType = QStringLiteral("layer.deformation2D");
    operationLayerId = layerId_;
    payload = QJsonObject{{QStringLiteral("expected"), expected},
                          {QStringLiteral("value"), value}};
    return true;
  }
  QString label() const override { return label_; }
  size_t estimatedMemoryBytes() const override {
    const auto beforeBytes = QJsonDocument(before_).toJson(QJsonDocument::Compact).size();
    const auto afterBytes = QJsonDocument(after_).toJson(QJsonDocument::Compact).size();
    return static_cast<size_t>(std::max<qsizetype>(0, beforeBytes) +
                               std::max<qsizetype>(0, afterBytes));
  }

private:
  bool apply(const QJsonObject& expected, const QJsonObject& state) {
    ArtifactAbstractLayerPtr layer = layer_.lock();
    if (!layer) {
      if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        auto selected = selection->currentLayer();
        if (selected && selected->id().toString() == layerId_) layer = selected;
      }
    }
    auto* imageLayer = layer ? dynamic_cast<ArtifactImageLayer*>(layer.get()) : nullptr;
    if (!imageLayer || imageLayer->id().toString() != layerId_) return false;
    const QJsonObject current = imageLayer->deformation2DData();
    if (current == state) return true; // The editing path applies before pushing its command.
    if (current != expected) return false;
    if (tool_) {
      const auto id = imageLayer->id();
      if (!tool_->restoreLayerData(id, state, imageLayer) ||
          imageLayer->deformation2DData() != state) {
        tool_->restoreLayerData(id, expected, imageLayer);
        return false;
      }
    } else {
      imageLayer->setDeformation2DData(state);
      if (imageLayer->deformation2DData() != state) {
        imageLayer->setDeformation2DData(expected);
        return false;
      }
    }
    if (auto* manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }
  ArtifactAbstractLayerWeak layer_;
  ArtifactPuppetTool* tool_ = nullptr;
  QString layerId_;
  QJsonObject before_;
  QJsonObject after_;
  QString label_;
  bool lastOperationSucceeded_ = true;
};

class DeformerControlKeyframeUndoCommand final : public UndoCommand {
public:
  DeformerControlKeyframeUndoCommand(ArtifactPuppetTool* tool,
                                     LayerID layerId, QString pinId,
                                     QJsonObject before, QJsonObject after,
                                     QPointF beforePosition,
                                     QPointF afterPosition)
      : tool_(tool), layerId_(std::move(layerId)), pinId_(std::move(pinId)),
        before_(std::move(before)), after_(std::move(after)),
        beforePosition_(beforePosition), afterPosition_(afterPosition) {}
  void undo() override {
    lastOperationSucceeded_ = apply(before_, beforePosition_);
  }
  void redo() override {
    lastOperationSucceeded_ = apply(after_, afterPosition_);
  }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QStringList collaborationTargetLayerIds() const override {
    return layerId_.isNil() ? QStringList{} : QStringList{layerId_.toString()};
  }
  bool collaborationTargetScopeResolved() const override {
    return !layerId_.isNil();
  }
  QString label() const override { return QStringLiteral("Set Deformer Keyframe"); }
private:
  bool apply(const QJsonObject& snapshot, const QPointF& position) {
    if (!tool_ || !tool_->restorePinPositionAnimation(
                      layerId_, pinId_, snapshot, position)) return false;
    const QJsonObject animation =
        tool_->pinPositionAnimationSnapshot(layerId_, pinId_);
    if (!animation.isEmpty() && snapshot.contains(QStringLiteral("x")) &&
        snapshot.contains(QStringLiteral("y"))) {
      const double x = snapshot.value(QStringLiteral("xBase")).toDouble();
      const double y = snapshot.value(QStringLiteral("yBase")).toDouble();
      const QTransform transform = [&]() {
        auto* selection = ArtifactLayerSelectionManager::instance();
        const auto layer = selection ? selection->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
        return layer ? layer->getGlobalTransform() : QTransform{};
      }();
      tool_->movePin(pinId_, transform.map(QPointF(x, y)));
    }
    tool_->evaluatePinPositionsAtCurrentFrame(layerId_);
    if (auto* manager = UndoManager::instance()) manager->notifyAnythingChanged();
    return true;
  }
  ArtifactPuppetTool* tool_ = nullptr;
  LayerID layerId_;
  QString pinId_;
  QJsonObject before_;
  QJsonObject after_;
  QPointF beforePosition_;
  QPointF afterPosition_;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
