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
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
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
#include <wobjectdefs.h>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QObject>
export module Undo.UndoManager;





export import Artifact.Composition.Abstract;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Effect.Abstract;
import Layer.Blend;
import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Geometry.ResolutionRemap;

export namespace Artifact {
 using namespace ArtifactCore;

class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual QString label() const { return QStringLiteral("Command"); }
};

class SetPropertyCommand : public UndoCommand {
public:
    SetPropertyCommand(std::shared_ptr<ArtifactAbstractEffect> target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    std::weak_ptr<ArtifactAbstractEffect> target_;
    UniString name_;
    QVariant oldValue_;
    QVariant newValue_;
};

class MoveLayerCommand : public UndoCommand {
public:
    MoveLayerCommand(ArtifactAbstractLayerPtr layer, float deltaX, float deltaY, int64_t frame);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
    float dx_, dy_;
    int64_t frame_;
};

class AddLayerCommand : public UndoCommand {
public:
    AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop = true);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerPtr layer_;
    bool atTop_;
    int savedIndex_ = -1;
};

class RemoveLayerCommand : public UndoCommand {
public:
    RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerPtr layer_;
    int originalIndex_ = -1;
};

class MaskEditCommand : public UndoCommand {
public:
    MaskEditCommand(ArtifactAbstractLayerPtr layer,
                    std::vector<LayerMask> beforeMasks,
                    std::vector<LayerMask> afterMasks);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
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
private:
    ArtifactAbstractLayerWeak layer_;
    std::vector<LayerMatteReference> beforeRefs_;
    std::vector<LayerMatteReference> afterRefs_;
};

// 新規コマンド：レイヤー移動（インデックス変更）
class MoveLayerIndexCommand : public UndoCommand {
public:
    MoveLayerIndexCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactCompositionWeakPtr comp_;
    ArtifactAbstractLayerWeak layer_;
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
private:
    ArtifactAbstractLayerWeak layer_;
    QString oldName_;
    QString newName_;
};

// 新規コマンド：レイヤー整列・分布
struct AlignLayerSnapshot {
    QString layerId;
    float beforeX, beforeY;
    float afterX, afterY;
};
class AlignLayersUndoCommand : public UndoCommand {
public:
    AlignLayersUndoCommand(const std::vector<AlignLayerSnapshot>& snapshots, const QString& label);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    std::vector<AlignLayerSnapshot> snapshots_;
    QString label_;
};

// 新規コマンド：レイヤー不透明度変更
class ChangeLayerOpacityCommand : public UndoCommand {
public:
    ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, float oldOpacity, float newOpacity);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
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
private:
    ArtifactAbstractLayerWeak layer_;
    size_t oldIndex_;
    size_t newIndex_;
};

// 新規コマンド：Variant 作成
class CreateVariantCommand : public UndoCommand {
public:
    CreateVariantCommand(ArtifactAbstractLayerPtr layer, const std::string& name);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
    std::string name_;
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
private:
    ArtifactAbstractLayerWeak layer_;
    bool oldVisible_;
    bool newVisible_;
};

class SetLayerLockCommand : public UndoCommand {
public:
    SetLayerLockCommand(ArtifactAbstractLayerPtr layer, bool locked);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
    bool oldLocked_;
    bool newLocked_;
};

class SetLayerSoloCommand : public UndoCommand {
public:
    SetLayerSoloCommand(ArtifactAbstractLayerPtr layer, bool solo);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
    bool oldSolo_;
    bool newSolo_;
};

class SetLayerShyCommand : public UndoCommand {
public:
    SetLayerShyCommand(ArtifactAbstractLayerPtr layer, bool shy);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
    bool oldShy_;
    bool newShy_;
};

class ChangeLayerBlendModeCommand : public UndoCommand {
public:
    ChangeLayerBlendModeCommand(ArtifactAbstractLayerPtr layer, LAYER_BLEND_TYPE newMode);
    void undo() override;
    void redo() override;
    QString label() const override;
private:
    ArtifactAbstractLayerWeak layer_;
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
private:
    std::vector<std::unique_ptr<UndoCommand>> children_;
    QString label_;
};

class UndoManager : public QObject {
    W_OBJECT(UndoManager)
public:
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

}
