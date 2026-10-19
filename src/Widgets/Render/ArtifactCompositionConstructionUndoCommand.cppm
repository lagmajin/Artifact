module;

#include <QString>

#include <cstddef>

export module Artifact.Widgets.CompositionConstructionUndoCommand;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Construction;
import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;

namespace Artifact {
export class ConstructionItemUndoCommand final : public UndoCommand {
  ArtifactAbstractLayerWeak layer_;
  size_t index_;
  ConstructionItem before_, after_;
  bool succeeded_ = true;
  void apply(const ConstructionItem &item) {
    const auto layer = ArtifactCore::dynamicPointerCast<ArtifactConstructionLayer>(layer_.lock());
    succeeded_ = layer && index_ < layer->constructionItems().size() &&
        layer->constructionItems()[index_].id == item.id && layer->setConstructionItem(index_, item);
    if (!succeeded_) return;
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(LayerChangedEvent{
          comp->id().toString(), layer->id().toString(), LayerChangedEvent::ChangeType::Modified});
    }
    if (auto *manager = UndoManager::instance()) manager->notifyAnythingChanged();
  }
public:
  ConstructionItemUndoCommand(const ArtifactAbstractLayerPtr &layer, size_t index,
      const ConstructionItem &before, const ConstructionItem &after)
      : layer_(layer), index_(index), before_(before), after_(after) {}
  void undo() override { apply(before_); }
  void redo() override { apply(after_); }
  QString label() const override { return QStringLiteral("Edit construction item"); }
  bool lastOperationSucceeded() const override { return succeeded_; }
};


} // namespace Artifact
