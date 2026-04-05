module;
#include <wobjectdefs.h>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QObject>
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
export module Undo.UndoManager;




import Utils.String.UniString;
import Artifact.Effect.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Composition.Abstract;

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
