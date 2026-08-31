module;

#include <QPointF>
#include <QString>

#include <utility>

export module Artifact.Widgets.CompositionTextPuppetUndoCommands;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
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
      : layer_(layer), before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
  QString label() const override { return QStringLiteral("Move Puppet Pin"); }

private:
  bool apply(const QPointF &position, float rotation) {
    if (!tool_) return false;
    tool_->movePin(pinId_, position);
    tool_->setPinRotation(pinId_, rotation);
    tool_->deformLayer(layerId_, renderer_);
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

class PuppetPinScalarUndoCommand final : public UndoCommand {
public:
  PuppetPinScalarUndoCommand(ArtifactPuppetTool *tool, QString pinId,
                             bool weight, float before, float after)
      : tool_(tool), pinId_(std::move(pinId)), weight_(weight),
        before_(before), after_(after) {}

  void undo() override { lastOperationSucceeded_ = apply(before_); }
  void redo() override { lastOperationSucceeded_ = apply(after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override {
    return weight_ ? QStringLiteral("Adjust Puppet Starch")
                   : QStringLiteral("Adjust Puppet Overlap");
  }

private:
  bool apply(float value) {
    if (!tool_) return false;
    if (weight_) tool_->setPinWeight(pinId_, value);
    else tool_->setPinDepth(pinId_, value);
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactPuppetTool *tool_ = nullptr;
  QString pinId_;
  bool weight_ = false;
  float before_ = 0.0f;
  float after_ = 0.0f;
  bool lastOperationSucceeded_ = true;
};

} // namespace Artifact
