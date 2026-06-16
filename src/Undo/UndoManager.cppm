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

std::unique_ptr<UndoCommand> SetPropertyCommand::cloneForRepeat() const {
    auto target = target_.lock();
    if (!target) {
        return nullptr;
    }
    // For a repeat we want the next "before" to be the current value of
    // the property (i.e. the value left behind by the previous redo), so
    // that subsequent undo/redo steps land on the same final state.
    const QString propertyName = name_.toQString();
    QVariant currentValue = newValue_;
    for (const auto& prop : target->getProperties()) {
        if (prop.getName() == propertyName) {
            currentValue = prop.getValue();
            break;
        }
    }
    return std::make_unique<SetPropertyCommand>(target, name_, currentValue, newValue_);
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

std::unique_ptr<UndoCommand> MoveLayerCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    // The same delta is reapplied at the same frame. This produces
    // additive moves (e.g. "nudge right" repeats keep marching right) which
    // is the intuitive behavior for repeat-last-action.
    return std::make_unique<MoveLayerCommand>(layer, dx_, dy_, frame_);
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

std::unique_ptr<UndoCommand> AddLayerCommand::cloneForRepeat() const {
    auto comp = comp_.lock();
    if (!comp || !layer_) {
        return nullptr;
    }
    // Repeat adds a fresh instance of the same layer with the same stack
    // position, mirroring the original "add" behavior. The new layer gets
    // its own id from the composition's appendLayer path.
    return std::make_unique<AddLayerCommand>(comp, layer_, atTop_);
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

std::unique_ptr<UndoCommand> RemoveLayerCommand::cloneForRepeat() const {
    auto comp = comp_.lock();
    if (!comp || !layer_) {
        return nullptr;
    }
    // Reuses the same layer pointer; originalIndex_ is recomputed on redo.
    return std::make_unique<RemoveLayerCommand>(comp, layer_);
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

std::unique_ptr<UndoCommand> MaskEditCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    // Capture the layer's current mask set as the new "before" snapshot
    // so the repeat undo lands back on the latest state, not the original.
    std::vector<LayerMask> currentMasks;
    if (layer->hasMasks()) {
        currentMasks.reserve(static_cast<std::size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
            currentMasks.push_back(layer->mask(i));
        }
    }
    return std::make_unique<MaskEditCommand>(layer, std::move(currentMasks), afterMasks_);
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

std::unique_ptr<UndoCommand> ChangeLayerMatteReferencesCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    return std::make_unique<ChangeLayerMatteReferencesCommand>(
        layer, layer->matteReferences(), afterRefs_);
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

bool UndoManager::canRepeat() const {
    if (impl_->undoStack.empty()) {
        return false;
    }
    const auto& top = impl_->undoStack.back();
    if (!top) {
        return false;
    }
    // Probe cloneForRepeat() without committing to a clone.
    return top->cloneForRepeat() != nullptr;
}

QString UndoManager::repeatDescription() const {
    if (impl_->undoStack.empty()) {
        return QString();
    }
    const auto& top = impl_->undoStack.back();
    return top ? top->label() : QString();
}

bool UndoManager::repeatLast() {
    if (impl_->undoStack.empty()) {
        return false;
    }
    const auto& top = impl_->undoStack.back();
    if (!top) {
        return false;
    }
    auto cloned = top->cloneForRepeat();
    if (!cloned) {
        return false;
    }
    // push() runs redo() on the clone, which is what we want for "repeat".
    push(std::move(cloned));
    return true;
}

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

std::unique_ptr<UndoCommand> MoveLayerIndexCommand::cloneForRepeat() const {
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (!comp || !layer) {
        return nullptr;
    }
    // The current index becomes the "old" index; the same destination
    // is reused, so each repeat kicks the layer toward the same slot.
    return std::make_unique<MoveLayerIndexCommand>(comp, layer, newIndex_, newIndex_);
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

std::unique_ptr<UndoCommand> RenameLayerCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    return std::make_unique<RenameLayerCommand>(layer, layer->layerName(), newName_);
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

std::unique_ptr<UndoCommand> ChangeLayerOpacityCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    // Re-apply the same opacity delta. "current → current + delta" mirrors
    // MoveLayerCommand's behavior, which is the typical repeat semantics.
    const float currentOpacity = layer->opacity();
    const float delta = newOpacity_ - oldOpacity_;
    return std::make_unique<ChangeLayerOpacityCommand>(
        layer, currentOpacity, currentOpacity + delta);
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

std::unique_ptr<UndoCommand> ChangeActiveVariantCommand::cloneForRepeat() const {
    auto layer = layer_.lock();
    if (!layer) {
        return nullptr;
    }
    return std::make_unique<ChangeActiveVariantCommand>(
        layer, layer->getActiveVariantIndex(), newIndex_);
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
                prop->addKeyFrame(k.time, k.value, k.interpolation,
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
