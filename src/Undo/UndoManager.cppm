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
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Composition.Result;
import Animation.Transform3D;
import Time.Rational;

namespace Artifact {

W_OBJECT_IMPL(UndoManager)

class UndoManager::Impl {
public:
    std::vector<std::unique_ptr<UndoCommand>> undoStack;
    std::vector<std::unique_ptr<UndoCommand>> redoStack;
    size_t maxHistorySize_ = 100;
    int64_t version_ = 0;
    int64_t savedVersion_ = 0;
};

// --- SetPropertyCommand ---
SetPropertyCommand::SetPropertyCommand(std::shared_ptr<ArtifactAbstractEffect> target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue)
    : target_(target), name_(propName), oldValue_(oldValue), newValue_(newValue) {}

void SetPropertyCommand::undo() {
    auto t = target_.lock();
    if (t) t->setPropertyValue(name_, oldValue_);
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

// --- MoveLayerCommand ---
MoveLayerCommand::MoveLayerCommand(ArtifactAbstractLayerPtr layer, float deltaX, float deltaY, int64_t frame)
    : layer_(layer), dx_(deltaX), dy_(deltaY), frame_(frame) {}

void MoveLayerCommand::undo() {
    auto l = layer_.lock();
    if (l) {
        auto& t3 = l->transform3D();
        ArtifactCore::RationalTime t0(frame_, 30000); // simplified rate
        t3.setPosition(t0, t3.positionX() - dx_, t3.positionY() - dy_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void MoveLayerCommand::redo() {
    auto l = layer_.lock();
    if (l) {
        auto& t3 = l->transform3D();
        ArtifactCore::RationalTime t0(frame_, 30000);
        t3.setPosition(t0, t3.positionX() + dx_, t3.positionY() + dy_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

// --- AddLayerCommand ---
AddLayerCommand::AddLayerCommand(std::shared_ptr<ArtifactAbstractComposition> comp, std::shared_ptr<ArtifactAbstractLayer> layer, bool atTop)
    : comp_(comp), layer_(layer), atTop_(atTop) {}

void AddLayerCommand::undo() {
    if (comp_ && layer_) comp_->removeLayerById(layer_->id());
}

void AddLayerCommand::redo() {
    if (comp_ && layer_) {
        if (atTop_) comp_->appendLayerTop(layer_);
        else comp_->appendLayerBottom(layer_);
    }
}


// --- UndoManager ---
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
    
    while (impl_->undoStack.size() >= impl_->maxHistorySize_) {
        impl_->undoStack.erase(impl_->undoStack.begin());
    }
    
    impl_->undoStack.push_back(std::move(cmd));
    impl_->redoStack.clear();
    impl_->version_++;
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

void UndoManager::clearHistory() {
    impl_->undoStack.clear();
    impl_->redoStack.clear();
    impl_->version_ = 0;
    impl_->savedVersion_ = 0;
}

size_t UndoManager::undoCount() const { return impl_->undoStack.size(); }
size_t UndoManager::redoCount() const { return impl_->redoStack.size(); }

QString UndoManager::undoDescription() const {
    if (impl_->undoStack.empty()) return QString();
    return QString("Undo (%1 actions)").arg(impl_->undoStack.size());
}

QString UndoManager::redoDescription() const {
    if (impl_->redoStack.empty()) return QString();
    return QString("Redo (%1 actions)").arg(impl_->redoStack.size());
}

void UndoManager::setMaxHistorySize(size_t maxSize) {
    impl_->maxHistorySize_ = maxSize;
    while (impl_->undoStack.size() > impl_->maxHistorySize_) {
        impl_->undoStack.erase(impl_->undoStack.begin());
    }
}

size_t UndoManager::maxHistorySize() const { return impl_->maxHistorySize_; }
bool UndoManager::hasUnsavedChanges() const { return impl_->version_ != impl_->savedVersion_; }
void UndoManager::markAsSaved() { impl_->savedVersion_ = impl_->version_; }
int64_t UndoManager::currentVersion() const { return impl_->version_; }

}
