module;
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
#include <functional>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <cstddef>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QByteArray>
#include <wobjectdefs.h>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include <QVariant>
#include <QObject>
#include <QFile>
#include <QJsonObject>
export module Undo.UndoManager;





export import Artifact.Composition.Abstract;
import Artifact.Composition.InOutPoints;
import Utils.Id;
import Utils.String.UniString;
import Core.ArtifactString;
import Artifact.Effect.Abstract;
import Layer.Blend;
import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Geometry.ResolutionRemap;
import Image.ImageF32x4_RGBA;

export namespace Artifact {
 using namespace ArtifactCore;

class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual QString label() const { return QStringLiteral("Command"); }
    virtual size_t estimatedMemoryBytes() const { return 1024; }
    // Optional persistence contract. Existing commands remain in-memory until
    // they explicitly opt in by overriding these methods.
    virtual QString commandType() const { return {}; }
    virtual bool canSerialize() const { return false; }
    virtual bool isOffloaded() const { return false; }
    virtual QJsonObject serialize() const { return {}; }
    virtual bool deserialize(const QJsonObject&) { return false; }
};

class SetPropertyCommand : public UndoCommand {
public:
    SetPropertyCommand(ArtifactAbstractEffectPtr target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetPropertyCommand"); }
    bool canSerialize() const override { return !effectId_.isEmpty() && !name_.toQString().isEmpty() && !target_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr target_;
    QString effectId_;
    UniString name_;
    QVariant oldValue_;
    QVariant newValue_;
};

class AnimationLayerStackSnapshotCommand : public UndoCommand {
public:
    AnimationLayerStackSnapshotCommand(ArtifactAbstractLayerPtr layer,
                                       const QJsonObject& before,
                                       const QJsonObject& after);
    void undo() override;
    void redo() override;
    QString label() const override;
    QString commandType() const override { return QStringLiteral("AnimationLayerStackSnapshotCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QJsonObject before_;
    QJsonObject after_;
};

class MoveLayerCommand : public UndoCommand {
public:
    MoveLayerCommand(ArtifactAbstractLayerPtr layer, float deltaX, float deltaY, int64_t frame);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveLayerCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    float dx_, dy_;
    int64_t frame_;
};

class AddLayerCommand : public UndoCommand {
public:
    AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop = true);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("AddLayerCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && static_cast<bool>(layer_) && removedMatteReferences_.empty() && removedParentReferences_.empty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerPtr layer_;
    QString compositionId_;
    QString layerId_;
    bool atTop_;
    int savedIndex_ = -1;
    std::vector<std::pair<ArtifactAbstractLayerPtr,
                          std::vector<LayerMatteReference>>>
        removedMatteReferences_;
    std::vector<std::pair<ArtifactAbstractLayerPtr, ArtifactCore::LayerID>>
        removedParentReferences_;
};

class RemoveLayerCommand : public UndoCommand {
public:
    RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("RemoveLayerCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && static_cast<bool>(layer_) && removedMatteReferences_.empty() && removedParentReferences_.empty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerPtr layer_;
    QString compositionId_;
    QString layerId_;
    int originalIndex_ = -1;
    std::vector<std::pair<ArtifactAbstractLayerPtr,
                          std::vector<LayerMatteReference>>>
        removedMatteReferences_;
    std::vector<std::pair<ArtifactAbstractLayerPtr, ArtifactCore::LayerID>>
        removedParentReferences_;
};

class MaskEditCommand : public UndoCommand {
public:
    MaskEditCommand(ArtifactAbstractLayerPtr layer,
                    std::vector<LayerMask> beforeMasks,
                    std::vector<LayerMask> afterMasks);
    void undo() override;
    void redo() override;
    QString label() const override;
    QString commandType() const override { return QStringLiteral("MaskEditCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    std::vector<LayerMask> beforeMasks_;
    std::vector<LayerMask> afterMasks_;
};

class ChangeLayerMatteReferencesCommand : public UndoCommand {
public:
    ChangeLayerMatteReferencesCommand(ArtifactAbstractLayerPtr layer,
                                       std::vector<LayerMatteReference> beforeRefs,
                                       std::vector<LayerMatteReference> afterRefs);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ChangeLayerMatteReferencesCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    std::vector<LayerMatteReference> beforeRefs_;
    std::vector<LayerMatteReference> afterRefs_;
};

class SetLayerPropertyKeyframesCommand : public UndoCommand {
public:
    SetLayerPropertyKeyframesCommand(ArtifactAbstractLayerPtr layer,
                                     QString propertyPath,
                                     std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
                                     std::vector<ArtifactCore::KeyFrame> afterKeyframes,
                                     QString label = QStringLiteral("Edit Layer Property Keyframes"));
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerPropertyKeyframesCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString propertyPath_;
    std::vector<ArtifactCore::KeyFrame> beforeKeyframes_;
    std::vector<ArtifactCore::KeyFrame> afterKeyframes_;
    QString label_;
};

class SetTextLayerTextCommand : public UndoCommand {
public:
    SetTextLayerTextCommand(ArtifactAbstractLayerPtr layer,
                            QString beforeText,
                            QString afterText,
                            QString label = QStringLiteral("Edit Text"));
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetTextLayerTextCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString beforeText_;
    QString afterText_;
    QString label_;
};

class ReplaceLayerSourceCommand : public UndoCommand {
public:
    ReplaceLayerSourceCommand(ArtifactAbstractLayerPtr layer,
                              QString propertyPath,
                              QString oldSourcePath,
                              QString newSourcePath);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ReplaceLayerSourceCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired() && !propertyPath_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString propertyPath_;
    QString oldSourcePath_;
    QString newSourcePath_;
};

class ToggleLocalizedSourceCommand : public UndoCommand {
public:
    ToggleLocalizedSourceCommand(std::function<void()> localize,
                                 std::function<void()> relinkShared,
                                 QString label = QStringLiteral("Localize Layer Source"));
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    std::function<void()> localize_;
    std::function<void()> relinkShared_;
    QString label_;
};

class SetEffectMaskImagesCommand : public UndoCommand {
public:
    SetEffectMaskImagesCommand(ArtifactAbstractEffectPtr effect,
                               std::vector<SharedPtr<ImageF32x4_RGBA>> beforeMasks,
                               std::vector<SharedPtr<ImageF32x4_RGBA>> afterMasks,
                               QString label = QStringLiteral("Edit Effect Mask Images"));
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetEffectMaskImagesCommand"); }
    bool canSerialize() const override { return !effectId_.isEmpty() && !effect_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr effect_;
    QString effectId_;
    std::vector<SharedPtr<ImageF32x4_RGBA>> beforeMasks_;
    std::vector<SharedPtr<ImageF32x4_RGBA>> afterMasks_;
    QString label_;
};

// 新規コマンド：レイヤー移動（インデックス変更）
class MoveLayerIndexCommand : public UndoCommand {
public:
    MoveLayerIndexCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveLayerIndexCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerWeak layer_;
    QString compositionId_;
    QString layerId_;
    int oldIndex_;
    int newIndex_;
};

// 新規コマンド：レイヤー名変更
class RenameLayerCommand : public UndoCommand {
public:
    RenameLayerCommand(ArtifactAbstractLayerPtr layer, const QString& oldName, const QString& newName);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("RenameLayerCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString oldName_;
    QString newName_;
};

// 新規コマンド：レイヤー整列・分布
struct AlignLayerSnapshot {
    QString layerId;
    float beforeX, beforeY;
    float afterX, afterY;
    float beforeScaleX = 1.0f;
    float beforeScaleY = 1.0f;
    float afterScaleX = 1.0f;
    float afterScaleY = 1.0f;
};

class InOutPointsSnapshotCommand : public UndoCommand {
public:
    InOutPointsSnapshotCommand(ArtifactInOutPoints* points,
                               const QJsonObject& before,
                               const QJsonObject& after);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("InOutPointsSnapshotCommand"); }
    bool canSerialize() const override { return points_ != nullptr; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactInOutPoints* points_ = nullptr;
    QJsonObject before_;
    QJsonObject after_;
};
class AlignLayersUndoCommand : public UndoCommand {
public:
    AlignLayersUndoCommand(const std::vector<AlignLayerSnapshot>& snapshots, const QString& label);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("AlignLayersUndoCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !snapshots_.empty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    std::vector<AlignLayerSnapshot> snapshots_;
    QString compositionId_;
    QString label_;
};

// 新規コマンド：レイヤー不透明度変更
class ChangeLayerOpacityCommand : public UndoCommand {
public:
    ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, float oldOpacity, float newOpacity);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ChangeLayerOpacityCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    float oldOpacity_;
    float newOpacity_;
};

// 新規コマンド：Variant 切り替え
class ChangeActiveVariantCommand : public UndoCommand {
public:
    ChangeActiveVariantCommand(ArtifactAbstractLayerPtr layer, size_t oldIndex, size_t newIndex);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ChangeActiveVariantCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    size_t oldIndex_;
    size_t newIndex_;
};

// 新規コマンド：Variant 作成
class CreateVariantCommand : public UndoCommand {
public:
    CreateVariantCommand(ArtifactAbstractLayerPtr layer, const ArtifactCore::String& name);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("CreateVariantCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    ArtifactCore::String name_;
    size_t index_;
    std::unique_ptr<LayerVariant> extracted_;
};

// 新規コマンド：コンポジション解像度変更 / Remap
// before snapshot として各レイヤーの mask と transform プロパティ keyframe 列を保持し、
// redo で applyResolutionRemap、undo で size 復元 + snapshot 復元を行う。
class ChangeCompositionResolutionCommand : public UndoCommand {
public:
    ChangeCompositionResolutionCommand(
        ArtifactCompositionPtr comp,
        const QSize& oldSize,
        const QSize& newSize,
        ArtifactCore::RemapPolicy policy);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;

    // レイヤー単位の transform プロパティ snapshot。
    // propertyPath → keyframe 列。非アニメーション値は空 keyframe 列 + currentValue で表現。
    struct PropertySnapshot {
        QString propertyPath;
        QVariant currentValue;
        std::vector<ArtifactCore::KeyFrame> keyframes;
    };
    struct LayerSnapshot {
        ArtifactCore::LayerID layerId;
        std::vector<LayerMask> masks;
        std::vector<PropertySnapshot> properties;
    };

private:
    ArtifactCompositionWeakPtr comp_;
    QSize oldSize_;
    QSize newSize_;
    ArtifactCore::RemapPolicy policy_;
    std::vector<LayerSnapshot> beforeSnapshots_;
};

// === Undo commands for layer state toggles ===

class SetLayerVisibilityCommand : public UndoCommand {
public:
    SetLayerVisibilityCommand(ArtifactAbstractLayerPtr layer, bool visible);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerVisibilityCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    bool oldVisible_;
    bool newVisible_;
};

class SetLayerLockCommand : public UndoCommand {
public:
    SetLayerLockCommand(ArtifactAbstractLayerPtr layer, bool locked);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerLockCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    bool oldLocked_;
    bool newLocked_;
};

class SetLayerSoloCommand : public UndoCommand {
public:
    SetLayerSoloCommand(ArtifactAbstractLayerPtr layer, bool solo);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerSoloCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    bool oldSolo_;
    bool newSolo_;
};

class SetLayerShyCommand : public UndoCommand {
public:
    SetLayerShyCommand(ArtifactAbstractLayerPtr layer, bool shy);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerShyCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    bool oldShy_;
    bool newShy_;
};

class ChangeLayerBlendModeCommand : public UndoCommand {
public:
    ChangeLayerBlendModeCommand(ArtifactAbstractLayerPtr layer, LAYER_BLEND_TYPE newMode);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ChangeLayerBlendModeCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    LAYER_BLEND_TYPE oldMode_;
    LAYER_BLEND_TYPE newMode_;
};

// === Macro command for batching multiple undo commands ===

class MacroUndoCommand : public UndoCommand {
public:
    explicit MacroUndoCommand(const QString& label);
    void addChild(std::unique_ptr<UndoCommand> child);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MacroUndoCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    std::vector<std::unique_ptr<UndoCommand>> children_;
    QString label_;
};

class MoveAssetFileCommand : public UndoCommand {
public:
    MoveAssetFileCommand(const QString& oldPath, const QString& newPath);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveAssetFileCommand"); }
    bool canSerialize() const override { return !oldPath_.isEmpty() && !newPath_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    QString oldPath_;
    QString newPath_;
};

class UndoManager : public QObject {
    W_OBJECT(UndoManager)
public:
    using CommandFactory = std::function<std::unique_ptr<UndoCommand>(const QJsonObject&)>;
    using EffectResolver = std::function<ArtifactAbstractEffectPtr(const QString&)>;
    using LayerResolver = std::function<ArtifactAbstractLayerPtr(const QString&)>;
    using CompositionResolver = std::function<ArtifactCompositionPtr(const QString&)>;
    using InOutPointsResolver = std::function<ArtifactInOutPoints*()>;
    enum class OffloadPolicy { Never, OnPressure, Always };
    struct UndoBudget {
        size_t maxEntryCount = 100;
        size_t maxMemoryBytes = 256u * 1024u * 1024u;
        size_t maxSingleEntryBytes = 64u * 1024u * 1024u;
    };
    UndoManager();
    ~UndoManager();
    void push(std::unique_ptr<UndoCommand> cmd);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    static UndoManager* instance();

    // === History Management ===
    
    void clearHistory();
    size_t undoCount() const;
    size_t redoCount() const;
    QString undoDescription() const;
    QString redoDescription() const;
    QStringList undoHistoryLabels() const;
    QStringList redoHistoryLabels() const;
    void setMaxHistorySize(size_t maxSize);
    size_t maxHistorySize() const;
    void setBudget(const UndoBudget& budget);
    const UndoBudget& budget() const;
    size_t currentMemoryBytes() const;
    float memoryPressure() const;
    void setOffloadPolicy(OffloadPolicy policy);
    OffloadPolicy offloadPolicy() const;
    void setOffloadDirectory(const QString& path);
    QString offloadDirectory() const;
    bool saveSessionHistory(const QString& path) const;
    bool loadSessionHistory(const QString& path);
    void registerCommandFactory(const QString& type, CommandFactory factory);
    std::unique_ptr<UndoCommand> createCommand(const QString& type,
                                               const QJsonObject& data) const;
    void setEffectResolver(EffectResolver resolver);
    ArtifactAbstractEffectPtr resolveEffect(const QString& effectId) const;
    void setLayerResolver(LayerResolver resolver);
    ArtifactAbstractLayerPtr resolveLayer(const QString& layerId) const;
    void setCompositionResolver(CompositionResolver resolver);
    ArtifactCompositionPtr resolveComposition(const QString& compositionId) const;
    void setInOutPointsResolver(InOutPointsResolver resolver);
    ArtifactInOutPoints* resolveInOutPoints() const;

    // === Serialization for Project Save ===
    
    bool hasUnsavedChanges() const;
    void markAsSaved();
    int64_t currentVersion() const;

    void notifyPropertyChanged(const QString& effectId);
    void notifyAnythingChanged();

    // Verdigris signal declaration
    void propertyChanged(const QString& effectId) W_SIGNAL(propertyChanged, effectId);
    void anythingChanged() W_SIGNAL(anythingChanged);
    void historyChanged() W_SIGNAL(historyChanged);

private:
    class Impl;
    Impl* impl_;
};

class LayoutSnapshotCommand : public UndoCommand {
public:
    using RestoreFn = std::function<bool(const QByteArray&)>;

    LayoutSnapshotCommand(QString label,
                          QByteArray beforeState,
                          QByteArray afterState,
                          RestoreFn restoreFn);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
private:
    QString label_;
    QByteArray beforeState_;
    QByteArray afterState_;
    RestoreFn restoreFn_;
};

}
