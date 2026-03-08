module;
#include <wobjectimpl.h>
#include <vector>
#include <stack>
#include <memory>
#include <utility>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Undo.UndoManager;



import Undo.UndoManager;

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

QString SetPropertyCommand::label() const {
    return QStringLiteral("Set Property: %1").arg(name_.toQString());
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

QString MoveLayerCommand::label() const {
    return QStringLiteral("Move Layer");
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

QString AddLayerCommand::label() const {
    return QStringLiteral("Add Layer");
}

QString RemoveLayerCommand::label() const {
    return QStringLiteral("Remove Layer");
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
    Q_EMIT historyChanged();
}

void UndoManager::undo() {
    if (impl_->undoStack.empty()) return;
    auto cmd = std::move(impl_->undoStack.back());
    impl_->undoStack.pop_back();
    cmd->undo();
    impl_->redoStack.push_back(std::move(cmd));
    Q_EMIT historyChanged();
}

void UndoManager::redo() {
    if (impl_->redoStack.empty()) return;
    auto cmd = std::move(impl_->redoStack.back());
    impl_->redoStack.pop_back();
    cmd->redo();
    impl_->undoStack.push_back(std::move(cmd));
    Q_EMIT historyChanged();
}

bool UndoManager::canUndo() const { return !impl_->undoStack.empty(); }
bool UndoManager::canRedo() const { return !impl_->redoStack.empty(); }

void UndoManager::clearHistory() {
    impl_->undoStack.clear();
    impl_->redoStack.clear();
    impl_->version_ = 0;
    impl_->savedVersion_ = 0;
    Q_EMIT historyChanged();
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

QStringList UndoManager::undoHistoryLabels() const {
    QStringList labels;
    labels.reserve(static_cast<int>(impl_->undoStack.size()));
    for (auto it = impl_->undoStack.rbegin(); it != impl_->undoStack.rend(); ++it) {
        labels.append((*it) ? (*it)->label() : QStringLiteral("Command"));
    }
    return labels;
}

QStringList UndoManager::redoHistoryLabels() const {
    QStringList labels;
    labels.reserve(static_cast<int>(impl_->redoStack.size()));
    for (auto it = impl_->redoStack.rbegin(); it != impl_->redoStack.rend(); ++it) {
        labels.append((*it) ? (*it)->label() : QStringLiteral("Command"));
    }
    return labels;
}

void UndoManager::setMaxHistorySize(size_t maxSize) {
    impl_->maxHistorySize_ = maxSize;
    while (impl_->undoStack.size() > impl_->maxHistorySize_) {
        impl_->undoStack.erase(impl_->undoStack.begin());
    }
    Q_EMIT historyChanged();
}

size_t UndoManager::maxHistorySize() const { return impl_->maxHistorySize_; }
bool UndoManager::hasUnsavedChanges() const { return impl_->version_ != impl_->savedVersion_; }
void UndoManager::markAsSaved() { impl_->savedVersion_ = impl_->version_; }
int64_t UndoManager::currentVersion() const { return impl_->version_; }

}
