module;
#include <wobjectdefs.h>
#include <QString>
#include <QVariant>
#include <QObject>
export module Undo.UndoManager;

import std;
import Utils.String.UniString;
import Artifact.Effect.Abstract;

export namespace Artifact {
 using namespace ArtifactCore;

class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

class SetPropertyCommand : public UndoCommand {
public:
    SetPropertyCommand(std::shared_ptr<ArtifactAbstractEffect> target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue);
    void undo() override;
    void redo() override;
private:
    std::weak_ptr<ArtifactAbstractEffect> target_;
    UniString name_;
    QVariant oldValue_;
    QVariant newValue_;
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
    
    /// Clear all undo/redo history
    void clearHistory();
    
    /// Get undo history size
    size_t undoCount() const;
    
    /// Get redo history size
    size_t redoCount() const;
    
    /// Get description of next undo action
    QString undoDescription() const;
    
    /// Get description of next redo action
    QString redoDescription() const;
    
    /// Set maximum history size
    void setMaxHistorySize(size_t maxSize);
    
    /// Get maximum history size
    size_t maxHistorySize() const;

    // === Serialization for Project Save ===
    
    /// Check if there are unsaved changes
    bool hasUnsavedChanges() const;
    
    /// Mark current state as saved (clear dirty flag)
    void markAsSaved();
    
    /// Get current dirty state version
    int64_t currentVersion() const;

    // Emit when a property's value changed (effect id as QString)
    void notifyPropertyChanged(const QString& effectId);
    void notifyAnythingChanged();

    // Verdigris signal declaration
    void propertyChanged(const QString& effectId) W_SIGNAL(propertyChanged, effectId);
    void anythingChanged() W_SIGNAL(anythingChanged);

private:
    // opaque storage
    class Impl;
    Impl* impl_;
};

}
