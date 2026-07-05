module;
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
#include <wobjectimpl.h>

module Undo.UndoManager;



import Utils.String.UniString;
import Artifact.Effect.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Event.Bus;
import Animation.Transform3D;
import Time.Rational;
import Artifact.Layers.Selection.Manager;

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
AddLayerCommand::AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop)
    : comp_(comp), layer_(layer), atTop_(atTop), savedIndex_(-1) {}

void AddLayerCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    if (comp && layer) {
        comp->removeLayer(layer->id());
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void AddLayerCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    if (comp && layer) {
        if (atTop_) {
            comp->appendLayerTop(layer);
        } else {
            comp->appendLayerBottom(layer);
        }
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString AddLayerCommand::label() const {
    if (layer_) {
        return QStringLiteral("Add Layer: %1").arg(layer_->id().toString());
    }
    return QStringLiteral("Add Layer");
}

// --- RemoveLayerCommand ---
RemoveLayerCommand::RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer)
    : comp_(comp), layer_(layer), originalIndex_(-1) {}

void RemoveLayerCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    if (comp && layer) {
        if (originalIndex_ >= 0) {
            comp->insertLayerAt(layer, originalIndex_);
        } else {
            comp->appendLayerTop(layer);
        }
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void RemoveLayerCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    if (comp && layer) {
        // Save original index before removing
        auto layers = comp->allLayer();
        for (int i = 0; i < layers.size(); ++i) {
            if (layers[i] && layers[i]->id() == layer->id()) {
                originalIndex_ = i;
                break;
            }
        }
        comp->removeLayer(layer->id());
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString RemoveLayerCommand::label() const {
    if (layer_) {
        return QStringLiteral("Remove Layer: %1").arg(layer_->id().toString());
    }
    return QStringLiteral("Remove Layer");
}

namespace {
void applyMaskSnapshot(const ArtifactAbstractLayerPtr& layer, const std::vector<LayerMask>& masks) {
    if (!layer) {
        return;
    }

    layer->clearMasks();
    for (const auto& mask : masks) {
        layer->addMask(mask);
    }
    layer->changed();
}

void applyMatteSnapshot(const ArtifactAbstractLayerPtr& layer,
                        const std::vector<LayerMatteReference>& mattes) {
    if (!layer) {
        return;
    }

    layer->setMatteReferences(mattes);
    layer->changed();
}
} // namespace

// --- MaskEditCommand ---
MaskEditCommand::MaskEditCommand(ArtifactAbstractLayerPtr layer,
                                 std::vector<LayerMask> beforeMasks,
                                 std::vector<LayerMask> afterMasks)
    : layer_(layer), beforeMasks_(std::move(beforeMasks)), afterMasks_(std::move(afterMasks)) {}

void MaskEditCommand::undo() {
    applyMaskSnapshot(layer_.lock(), beforeMasks_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void MaskEditCommand::redo() {
    applyMaskSnapshot(layer_.lock(), afterMasks_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString MaskEditCommand::label() const {
    return QStringLiteral("Edit Mask");
}

// --- ChangeLayerMatteReferencesCommand ---
ChangeLayerMatteReferencesCommand::ChangeLayerMatteReferencesCommand(
    ArtifactAbstractLayerPtr layer,
    std::vector<LayerMatteReference> beforeRefs,
    std::vector<LayerMatteReference> afterRefs)
    : layer_(layer),
      beforeRefs_(std::move(beforeRefs)),
      afterRefs_(std::move(afterRefs)) {}

void ChangeLayerMatteReferencesCommand::undo() {
    applyMatteSnapshot(layer_.lock(), beforeRefs_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void ChangeLayerMatteReferencesCommand::redo() {
    applyMatteSnapshot(layer_.lock(), afterRefs_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString ChangeLayerMatteReferencesCommand::label() const {
    return QStringLiteral("Edit Track Mattes");
}

// --- AlignLayersUndoCommand ---
AlignLayersUndoCommand::AlignLayersUndoCommand(const std::vector<AlignLayerSnapshot>& snapshots, const QString& label)
    : snapshots_(snapshots), label_(label) {}

void AlignLayersUndoCommand::undo() {
    for (const auto& s : snapshots_) {
        auto* sel = ArtifactLayerSelectionManager::instance();
        if (!sel) continue;
        auto comp = sel->activeComposition();
        if (!comp) continue;
        auto layer = comp->layerById(LayerID(s.layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(RationalTime(0, 30000), s.beforeX, s.beforeY);
        layer->changed();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

void AlignLayersUndoCommand::redo() {
    for (const auto& s : snapshots_) {
        auto* sel = ArtifactLayerSelectionManager::instance();
        if (!sel) continue;
        auto comp = sel->activeComposition();
        if (!comp) continue;
        auto layer = comp->layerById(LayerID(s.layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(RationalTime(0, 30000), s.afterX, s.afterY);
        layer->changed();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

QString AlignLayersUndoCommand::label() const { return label_; }

// --- SetLayerVisibilityCommand ---
SetLayerVisibilityCommand::SetLayerVisibilityCommand(ArtifactAbstractLayerPtr layer, bool visible)
    : layer_(layer), oldVisible_(layer ? layer->isVisible() : true), newVisible_(visible) {}

void SetLayerVisibilityCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setVisible(oldVisible_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void SetLayerVisibilityCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setVisible(newVisible_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString SetLayerVisibilityCommand::label() const {
    return newVisible_ ? QStringLiteral("Show Layer") : QStringLiteral("Hide Layer");
}

// --- SetLayerLockCommand ---
SetLayerLockCommand::SetLayerLockCommand(ArtifactAbstractLayerPtr layer, bool locked)
    : layer_(layer), oldLocked_(layer ? layer->isLocked() : false), newLocked_(locked) {}

void SetLayerLockCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLocked(oldLocked_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void SetLayerLockCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLocked(newLocked_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString SetLayerLockCommand::label() const {
    return newLocked_ ? QStringLiteral("Lock Layer") : QStringLiteral("Unlock Layer");
}

// --- SetLayerSoloCommand ---
SetLayerSoloCommand::SetLayerSoloCommand(ArtifactAbstractLayerPtr layer, bool solo)
    : layer_(layer), oldSolo_(layer ? layer->isSolo() : false), newSolo_(solo) {}

void SetLayerSoloCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setSolo(oldSolo_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void SetLayerSoloCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setSolo(newSolo_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString SetLayerSoloCommand::label() const {
    return newSolo_ ? QStringLiteral("Solo Layer") : QStringLiteral("Unsolo Layer");
}

// --- SetLayerShyCommand ---
SetLayerShyCommand::SetLayerShyCommand(ArtifactAbstractLayerPtr layer, bool shy)
    : layer_(layer), oldShy_(layer ? layer->isShy() : false), newShy_(shy) {}

void SetLayerShyCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setShy(oldShy_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void SetLayerShyCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setShy(newShy_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString SetLayerShyCommand::label() const {
    return newShy_ ? QStringLiteral("Shy Layer") : QStringLiteral("Unshy Layer");
}

// --- ChangeLayerBlendModeCommand ---
ChangeLayerBlendModeCommand::ChangeLayerBlendModeCommand(ArtifactAbstractLayerPtr layer, LAYER_BLEND_TYPE newMode)
    : layer_(layer), oldMode_(layer ? layer->layerBlendType() : LAYER_BLEND_TYPE::BLEND_NORMAL), newMode_(newMode) {}

namespace {

void notifyLayerBlendModeChanged(const ArtifactAbstractLayerPtr& layer) {
    if (!layer) {
        return;
    }
    if (auto* composition =
            static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{composition->id().toString(),
                              layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }
}

}

void ChangeLayerBlendModeCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setBlendMode(oldMode_);
        layer->changed();
        notifyLayerBlendModeChanged(layer);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void ChangeLayerBlendModeCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setBlendMode(newMode_);
        layer->changed();
        notifyLayerBlendModeChanged(layer);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString ChangeLayerBlendModeCommand::label() const {
    return QStringLiteral("Change Blend Mode");
}

// --- MacroUndoCommand ---
MacroUndoCommand::MacroUndoCommand(const QString& label)
    : label_(label) {}

void MacroUndoCommand::addChild(std::unique_ptr<UndoCommand> child) {
    if (child) {
        children_.push_back(std::move(child));
    }
}

void MacroUndoCommand::undo() {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (*it) (*it)->undo();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

void MacroUndoCommand::redo() {
    for (auto& child : children_) {
        if (child) child->redo();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

QString MacroUndoCommand::label() const {
    return label_;
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

// --- MoveLayerIndexCommand ---
MoveLayerIndexCommand::MoveLayerIndexCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex)
    : comp_(comp), layer_(layer), oldIndex_(oldIndex), newIndex_(newIndex) {}

void MoveLayerIndexCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (comp && layer) {
        comp->moveLayerToIndex(layer->id(), oldIndex_);
    }
}

void MoveLayerIndexCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (comp && layer) {
        comp->moveLayerToIndex(layer->id(), newIndex_);
    }
}

QString MoveLayerIndexCommand::label() const {
    return QStringLiteral("Move Layer: %1 → %2").arg(oldIndex_).arg(newIndex_);
}

// --- RenameLayerCommand ---
RenameLayerCommand::RenameLayerCommand(ArtifactAbstractLayerPtr layer, const QString& oldName, const QString& newName)
    : layer_(layer), oldName_(oldName), newName_(newName) {}

void RenameLayerCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLayerName(oldName_);
    }
}

void RenameLayerCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setLayerName(newName_);
    }
}

QString RenameLayerCommand::label() const {
    return QStringLiteral("Rename Layer: %1 → %2").arg(oldName_).arg(newName_);
}

// --- ChangeLayerOpacityCommand ---
ChangeLayerOpacityCommand::ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, float oldOpacity, float newOpacity)
    : layer_(layer), oldOpacity_(oldOpacity), newOpacity_(newOpacity) {}

void ChangeLayerOpacityCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setOpacity(oldOpacity_);
    }
}

void ChangeLayerOpacityCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setOpacity(newOpacity_);
    }
}

QString ChangeLayerOpacityCommand::label() const {
    return QStringLiteral("Change Opacity: %1% → %2%").arg(oldOpacity_ * 100).arg(newOpacity_ * 100);
}

// --- ChangeActiveVariantCommand ---
ChangeActiveVariantCommand::ChangeActiveVariantCommand(ArtifactAbstractLayerPtr layer, size_t oldIndex, size_t newIndex)
    : layer_(layer), oldIndex_(oldIndex), newIndex_(newIndex) {}

void ChangeActiveVariantCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setActiveVariant(oldIndex_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void ChangeActiveVariantCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        layer->setActiveVariant(newIndex_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString ChangeActiveVariantCommand::label() const {
    return QStringLiteral("Change Layer Variant");
}

// --- CreateVariantCommand ---
CreateVariantCommand::CreateVariantCommand(ArtifactAbstractLayerPtr layer, const std::string& name)
    : layer_(layer), name_(name), index_(0) {}

void CreateVariantCommand::undo() {
    auto layer = layer_.lock();
    if (layer) {
        extracted_ = layer->extractVariant(index_);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void CreateVariantCommand::redo() {
    auto layer = layer_.lock();
    if (layer) {
        if (extracted_) {
            layer->insertVariant(index_, std::move(extracted_));
        } else {
            layer->createVariantFromCurrent(name_);
            index_ = layer->getVariants().size() - 1;
        }
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString CreateVariantCommand::label() const {
    return QStringLiteral("Create Layer Variant");
}

// --- ChangeCompositionResolutionCommand ---

namespace {

// レイヤーの mask と transform 系プロパティ（position/anchor/scale の X/Y）の
// 現在値 + keyframe 列を snapshot する。rotation/opacity は aspect 非依存のため除外。
// これらは applyResolutionRemap が書き換える対象のみを保持する。
ChangeCompositionResolutionCommand::LayerSnapshot
captureLayerSnapshotForResolutionRemap(const ArtifactAbstractLayerPtr& layer) {
    ChangeCompositionResolutionCommand::LayerSnapshot snap;
    if (!layer) return snap;

    snap.layerId = layer->id();

    // masks
    if (layer->hasMasks()) {
        snap.masks.reserve(static_cast<std::size_t>(layer->maskCount()));
        for (int mi = 0; mi < layer->maskCount(); ++mi) {
            snap.masks.push_back(layer->mask(mi));
        }
    }

    // transform 系プロパティのみ収集
    static const std::unordered_set<QString> kTransformPaths = {
        QStringLiteral("transform.position.x"),
        QStringLiteral("transform.position.y"),
        QStringLiteral("transform.anchor.x"),
        QStringLiteral("transform.anchor.y"),
        QStringLiteral("transform.scale.x"),
        QStringLiteral("transform.scale.y"),
    };

    for (const auto& group : layer->getLayerPropertyGroups()) {
        for (const auto& prop : group.allProperties()) {
            if (!prop || !prop->isAnimatable()) continue;
            if (kTransformPaths.find(prop->getName()) == kTransformPaths.end()) continue;

            ChangeCompositionResolutionCommand::PropertySnapshot psnap;
            psnap.propertyPath = prop->getName();
            psnap.currentValue = prop->getValue();
            psnap.keyframes = prop->getKeyFrames();
            snap.properties.push_back(std::move(psnap));
        }
    }
    return snap;
}

// snapshot から mask / transform プロパティを復元する。
// keyframe があるプロパティは clearKeyFrames + addKeyFrame で再構築、
// 無いプロパティは setValue で現在値を戻す。
void restoreLayerSnapshotForResolutionRemap(
    const ArtifactAbstractLayerPtr& layer,
    const ChangeCompositionResolutionCommand::LayerSnapshot& snap) {
    if (!layer) return;

    // masks
    layer->clearMasks();
    for (const auto& mask : snap.masks) {
        layer->addMask(mask);
    }

    // transform プロパティを path 名で引いて復元
    for (const auto& psnap : snap.properties) {
        ArtifactCore::AbstractPropertyPtr prop;
        for (const auto& group : layer->getLayerPropertyGroups()) {
            prop = group.findProperty(psnap.propertyPath);
            if (prop) break;
        }
        if (!prop) continue;

        if (psnap.keyframes.empty()) {
            prop->setValue(psnap.currentValue);
        } else {
            prop->clearKeyFrames();
            for (const auto& k : psnap.keyframes) {
                prop->addKeyFrame(k.time, k.value, static_cast<int>(k.interpolation),
                                  k.cp1_x, k.cp1_y, k.cp2_x, k.cp2_y, k.roving);
            }
        }
    }

    layer->changed();
}

} // namespace

ChangeCompositionResolutionCommand::ChangeCompositionResolutionCommand(
    ArtifactCompositionPtr comp,
    const QSize& oldSize,
    const QSize& newSize,
    ArtifactCore::RemapPolicy policy)
    : comp_(comp), oldSize_(oldSize), newSize_(newSize), policy_(policy) {
    // コンストラクト時点（applyResolutionRemap 実行前）の before snapshot を採取する。
    // 呼び出し元は remap を直接呼ばず、このコマンドを push すること。
    if (comp) {
        const auto layers = comp->allLayer();
        beforeSnapshots_.reserve(layers.size());
        for (const auto& layer : layers) {
            if (!layer) continue;
            beforeSnapshots_.push_back(captureLayerSnapshotForResolutionRemap(layer));
        }
    }
}

void ChangeCompositionResolutionCommand::undo() {
    const auto comp = comp_.lock();
    if (!comp) return;

    // size を元に戻す。applyResolutionRemap を逆呼びすると mask/transform まで
    // 再計算されて snapshot 復元と衝突するため、size のみ直接戻す。
    comp->setCompositionSize(oldSize_);

    // snapshot から mask / transform を復元
    for (const auto& snap : beforeSnapshots_) {
        const auto layer = comp->layerById(snap.layerId);
        restoreLayerSnapshotForResolutionRemap(layer, snap);
    }

    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void ChangeCompositionResolutionCommand::redo() {
    const auto comp = comp_.lock();
    if (!comp) return;

    // applyResolutionRemap が size 設定 + mask/transform remap をまとめて行う。
    // before snapshot はコンストラクタで採取済み。
    comp->applyResolutionRemap(newSize_, policy_);

    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString ChangeCompositionResolutionCommand::label() const {
    return QStringLiteral("Change Composition Resolution");
}

}
