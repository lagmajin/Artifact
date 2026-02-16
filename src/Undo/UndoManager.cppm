module;
#include <wobjectimpl.h>
#include <vector>
#include <stack>
#include <memory>
#include <utility>


module Undo.UndoManager;

import Undo.UndoManager;
import std;
import Utils.String.UniString;
import Artifact.Effect.Abstract;

namespace Artifact {

W_OBJECT_IMPL(UndoManager)

class UndoManager::Impl {
public:
    std::vector<std::unique_ptr<UndoCommand>> undoStack;
    std::vector<std::unique_ptr<UndoCommand>> redoStack;
};

SetPropertyCommand::SetPropertyCommand(std::shared_ptr<ArtifactAbstractEffect> target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue)
    : target_(target), name_(propName), oldValue_(oldValue), newValue_(newValue) {}

void SetPropertyCommand::undo() {
    auto t = target_.lock();
    if (t) t->setPropertyValue(name_, oldValue_);
    // notify listeners that a property changed
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyPropertyChanged(t ? t->effectID().toQString() : QString());
    }
}

void SetPropertyCommand::redo() {
    auto t = target_.lock();
    if (t) t->setPropertyValue(name_, newValue_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyPropertyChanged(t ? t->effectID().toQString() : QString());
    }
}

UndoManager::UndoManager(): impl_(new Impl()) {}

UndoManager::~UndoManager() { delete impl_; }

void UndoManager::notifyPropertyChanged(const QString& effectId) {
    Q_EMIT propertyChanged(effectId);
}

void UndoManager::notifyAnythingChanged() {
    Q_EMIT anythingChanged();
}

UndoManager* UndoManager::instance() {
    static UndoManager inst;
    return &inst;
}

void UndoManager::push(std::unique_ptr<UndoCommand> cmd) {
    if (!cmd) return;
    // Execute immediately and record for undo
    cmd->redo();
    impl_->undoStack.push_back(std::move(cmd));
    impl_->redoStack.clear();
}

void UndoManager::undo() {
    if (impl_->undoStack.empty()) return;
    auto cmd = std::move(impl_->undoStack.back());
    impl_->undoStack.pop_back();
    cmd->undo();
    impl_->redoStack.push_back(std::move(cmd));
}

void UndoManager::redo() {
    if (impl_->redoStack.empty()) return;
    auto cmd = std::move(impl_->redoStack.back());
    impl_->redoStack.pop_back();
    cmd->redo();
    impl_->undoStack.push_back(std::move(cmd));
}

bool UndoManager::canUndo() const { return !impl_->undoStack.empty(); }
bool UndoManager::canRedo() const { return !impl_->redoStack.empty(); }

}
