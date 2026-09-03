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
#include <QSize>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
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
import Artifact.Project.Items;
import Layer.Blend;
import Artifact.Layer.Abstract;
import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Geometry.ResolutionRemap;
import Image.ImageF32x4_RGBA;
import Color.Float;
import Audio.Modulation.Router;

export namespace Artifact {
 using namespace ArtifactCore;

class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    // Commands that perform fallible external work may report whether the
    // last undo/redo actually changed state. The default keeps existing
    // in-memory commands source-compatible and assumes success.
    virtual bool lastOperationSucceeded() const { return true; }
    // Composite commands may compensate a failed operation internally. The
    // manager must not invoke a second inverse operation in that case.
    virtual bool handlesFailedOperationCompensation() const { return false; }
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
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetPropertyCommand"); }
    bool canSerialize() const override {
        return !effectId_.isEmpty() && !name_.toQString().isEmpty() && !target_.expired() &&
               !QJsonValue::fromVariant(oldValue_).isUndefined() &&
               !QJsonValue::fromVariant(newValue_).isUndefined();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr target_;
    QString effectId_;
    UniString name_;
    QVariant oldValue_;
    QVariant newValue_;
    bool lastOperationSucceeded_ = true;
};

class EffectPresetSnapshotCommand : public UndoCommand {
public:
    EffectPresetSnapshotCommand(ArtifactAbstractEffectPtr effect,
                                QJsonObject before, QJsonObject after,
                                QString label = QStringLiteral("Apply Effect Preset"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("EffectPresetSnapshotCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr effect_;
    QString effectId_;
    QJsonObject before_;
    QJsonObject after_;
    QString label_;
    bool firstRedo_ = true;
    bool lastOperationSucceeded_ = true;
};

class SetEffectPropertyKeyframesCommand : public UndoCommand {
public:
    SetEffectPropertyKeyframesCommand(
        ArtifactAbstractEffectPtr effect, QString propertyName,
        std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
        std::vector<ArtifactCore::KeyFrame> afterKeyframes,
        QString label = QStringLiteral("Edit Effect Property Keyframes"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetEffectPropertyKeyframesCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr effect_;
    QString effectId_;
    QString propertyName_;
    std::vector<ArtifactCore::KeyFrame> beforeKeyframes_;
    std::vector<ArtifactCore::KeyFrame> afterKeyframes_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetEffectPropertyExpressionCommand : public UndoCommand {
public:
    SetEffectPropertyExpressionCommand(ArtifactAbstractEffectPtr effect,
                                        QString propertyName,
                                        QString beforeExpression,
                                        QString afterExpression,
                                        QString label = QStringLiteral("Edit Effect Expression"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetEffectPropertyExpressionCommand"); }
    bool canSerialize() const override { return !effectId_.isEmpty() && !effect_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr effect_;
    QString effectId_;
    QString propertyName_;
    QString beforeExpression_;
    QString afterExpression_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class AnimationLayerStackSnapshotCommand : public UndoCommand {
public:
    AnimationLayerStackSnapshotCommand(ArtifactAbstractLayerPtr layer,
                                       const QJsonObject& before,
                                       const QJsonObject& after);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class ClonerTransformStackSnapshotCommand : public UndoCommand {
public:
    ClonerTransformStackSnapshotCommand(ArtifactAbstractLayerPtr layer,
                                         QJsonArray before,
                                         QJsonArray after);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override { return QStringLiteral("Edit Cloner Transforms"); }
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override {
        return QStringLiteral("ClonerTransformStackSnapshotCommand");
    }
    bool canSerialize() const override {
        return !layerId_.isEmpty() && !layer_.expired();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QJsonArray before_;
    QJsonArray after_;
    bool lastOperationSucceeded_ = true;
};

class LayerComponentDescriptorSnapshotCommand : public UndoCommand {
public:
    LayerComponentDescriptorSnapshotCommand(ArtifactAbstractLayerPtr layer,
                                             QJsonObject before,
                                             QJsonObject after);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override {
        return QStringLiteral("Edit Layer Components");
    }
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override {
        return QStringLiteral("LayerComponentDescriptorSnapshotCommand");
    }
    bool canSerialize() const override {
        return !layerId_.isEmpty() && !layer_.expired();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QJsonObject before_;
    QJsonObject after_;
    bool lastOperationSucceeded_ = true;
};

class CloneEffectorStackSnapshotCommand : public UndoCommand {
public:
    CloneEffectorStackSnapshotCommand(ArtifactAbstractLayerPtr layer,
                                      QJsonArray before, QJsonArray after);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override {
        return QStringLiteral("Edit Clone Effectors");
    }
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override {
        return QStringLiteral("CloneEffectorStackSnapshotCommand");
    }
    bool canSerialize() const override {
        return !layerId_.isEmpty() && !layer_.expired();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QJsonArray before_;
    QJsonArray after_;
    bool lastOperationSucceeded_ = true;
};

class EffectModulationSnapshotCommand : public UndoCommand {
public:
    EffectModulationSnapshotCommand(
        ArtifactAbstractEffectPtr effect,
        ArtifactCore::Audio::Modulation::ModulationRouterSnapshot before,
        ArtifactCore::Audio::Modulation::ModulationRouterSnapshot after,
        QString label = QStringLiteral("Edit Effect Modulation"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("EffectModulationSnapshotCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractEffectWeakPtr effect_;
    QString effectId_;
    ArtifactCore::Audio::Modulation::ModulationRouterSnapshot before_;
    ArtifactCore::Audio::Modulation::ModulationRouterSnapshot after_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class LayerModulationSnapshotCommand : public UndoCommand {
public:
    LayerModulationSnapshotCommand(
        ArtifactAbstractLayerPtr layer,
        ArtifactCore::Audio::Modulation::ModulationRouterSnapshot before,
        ArtifactCore::Audio::Modulation::ModulationRouterSnapshot after,
        QString label = QStringLiteral("Edit Layer Modulation"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("LayerModulationSnapshotCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    ArtifactCore::Audio::Modulation::ModulationRouterSnapshot before_;
    ArtifactCore::Audio::Modulation::ModulationRouterSnapshot after_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class MoveLayerCommand : public UndoCommand {
public:
    MoveLayerCommand(ArtifactAbstractLayerPtr layer, float deltaX, float deltaY, int64_t frame);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class MoveMaskCommand : public UndoCommand {
public:
    MoveMaskCommand(ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveMaskCommand"); }
    bool canSerialize() const override {
        return !layerId_.isEmpty() && !layer_.expired() &&
               oldIndex_ >= 0 && newIndex_ >= 0;
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    int oldIndex_ = -1;
    int newIndex_ = -1;
    bool lastOperationSucceeded_ = true;
};

class AddLayerCommand : public UndoCommand {
public:
    AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop = true);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class AddLayerEffectCommand : public UndoCommand {
public:
    AddLayerEffectCommand(ArtifactAbstractLayerPtr layer,
                          ArtifactAbstractEffectPtr effect);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override { return QStringLiteral("Add Layer Effect"); }
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override {
        return QStringLiteral("AddLayerEffectCommand");
    }
    bool canSerialize() const override {
        return !layerId_.isEmpty() && !effectId_.isEmpty() &&
               !layer_.expired() && static_cast<bool>(effect_);
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    ArtifactAbstractEffectPtr effect_;
    QString layerId_;
    QString effectId_;
    bool lastOperationSucceeded_ = true;
};

class RemoveLayerCommand : public UndoCommand {
public:
    RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    QStringList selectedLayerIds_;
    QString currentSelectedLayerId_;
    bool hasSelectionSnapshot_ = false;
    bool lastOperationSucceeded_ = true;
};

class LayerSelectionSnapshotCommand : public UndoCommand {
public:
    LayerSelectionSnapshotCommand(ArtifactCompositionPtr composition,
                                  QStringList beforeLayerIds,
                                  QString beforeCurrentLayerId,
                                  QStringList afterLayerIds,
                                  QString afterCurrentLayerId);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
private:
    void apply(const QStringList& layerIds, const QString& currentLayerId);
    ArtifactCompositionWeakPtr composition_;
    QStringList beforeLayerIds_;
    QString beforeCurrentLayerId_;
    QStringList afterLayerIds_;
    QString afterCurrentLayerId_;
    bool lastOperationSucceeded_ = true;
};

class MaskEditCommand : public UndoCommand {
public:
    MaskEditCommand(ArtifactAbstractLayerPtr layer,
                    std::vector<LayerMask> beforeMasks,
                    std::vector<LayerMask> afterMasks);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class ChangeLayerMatteReferencesCommand : public UndoCommand {
public:
    ChangeLayerMatteReferencesCommand(ArtifactAbstractLayerPtr layer,
                                       std::vector<LayerMatteReference> beforeRefs,
                                       std::vector<LayerMatteReference> afterRefs);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetLayerPropertyKeyframesCommand : public UndoCommand {
public:
    SetLayerPropertyKeyframesCommand(ArtifactAbstractLayerPtr layer,
                                     QString propertyPath,
                                     std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
                                     std::vector<ArtifactCore::KeyFrame> afterKeyframes,
                                     QString label = QStringLiteral("Edit Layer Property Keyframes"),
                                     std::optional<bool> beforeAnimatable = std::nullopt,
                                     std::optional<bool> afterAnimatable = std::nullopt);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    std::optional<bool> beforeAnimatable_;
    std::optional<bool> afterAnimatable_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetLayerPropertyValueCommand : public UndoCommand {
public:
    SetLayerPropertyValueCommand(ArtifactAbstractLayerPtr layer,
                                 QString propertyPath,
                                 QVariant beforeValue,
                                 QVariant afterValue,
                                 QString label = QStringLiteral("Edit Layer Property Value"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerPropertyValueCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString propertyPath_;
    QVariant beforeValue_;
    QVariant afterValue_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetAudioDeClickRangesCommand : public UndoCommand {
public:
    SetAudioDeClickRangesCommand(
        ArtifactAbstractLayerPtr layer,
        std::vector<std::pair<qint64, qint64>> beforeRanges,
        std::vector<std::pair<qint64, qint64>> afterRanges,
        QString label = QStringLiteral("Edit Audio De-click Ranges"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetAudioDeClickRangesCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    std::vector<std::pair<qint64, qint64>> beforeRanges_;
    std::vector<std::pair<qint64, qint64>> afterRanges_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetLayerPropertyExpressionCommand : public UndoCommand {
public:
    SetLayerPropertyExpressionCommand(ArtifactAbstractLayerPtr layer,
                                      QString propertyPath,
                                      QString beforeExpression,
                                      QString afterExpression,
                                      QString label = QStringLiteral("Edit Layer Property Expression"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetLayerPropertyExpressionCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired() && !propertyPath_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QString propertyPath_;
    QString beforeExpression_;
    QString afterExpression_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetTextLayerTextCommand : public UndoCommand {
public:
    SetTextLayerTextCommand(ArtifactAbstractLayerPtr layer,
                            QString beforeText,
                            QString afterText,
                            QString label = QStringLiteral("Edit Text"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetTextAnimatorStackCommand : public UndoCommand {
public:
    SetTextAnimatorStackCommand(ArtifactAbstractLayerPtr layer,
                                QJsonArray beforeStack,
                                QJsonArray afterStack,
                                QString label = QStringLiteral("Edit Text Animators"));
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetTextAnimatorStackCommand"); }
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    bool apply(const QJsonArray& stack, const QJsonArray& compensationStack);
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    QJsonArray beforeStack_;
    QJsonArray afterStack_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class ReplaceLayerSourceCommand : public UndoCommand {
public:
    ReplaceLayerSourceCommand(ArtifactAbstractLayerPtr layer,
                              QString propertyPath,
                              QString oldSourcePath,
                              QString newSourcePath);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class ToggleLocalizedSourceCommand : public UndoCommand {
public:
    ToggleLocalizedSourceCommand(std::function<bool()> localize,
                                 std::function<bool()> relinkShared,
                                 QString label = QStringLiteral("Localize Layer Source"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
private:
    std::function<bool()> localize_;
    std::function<bool()> relinkShared_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class SetEffectMaskImagesCommand : public UndoCommand {
public:
    SetEffectMaskImagesCommand(ArtifactAbstractEffectPtr effect,
                               std::vector<SharedPtr<ImageF32x4_RGBA>> beforeMasks,
                               std::vector<SharedPtr<ImageF32x4_RGBA>> afterMasks,
                               QString label = QStringLiteral("Edit Effect Mask Images"));
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

// 新規コマンド：レイヤー移動（インデックス変更）
class MoveLayerIndexCommand : public UndoCommand {
public:
    MoveLayerIndexCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

// 新規コマンド：レイヤー名変更
class RenameLayerCommand : public UndoCommand {
public:
    RenameLayerCommand(ArtifactAbstractLayerPtr layer, const QString& oldName, const QString& newName);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetCompositionNoteCommand : public UndoCommand {
public:
    SetCompositionNoteCommand(ArtifactCompositionPtr composition,
                              QString oldNote, QString newNote);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetCompositionNoteCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !composition_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactCompositionWeakPtr composition_;
    QString compositionId_;
    QString oldNote_;
    QString newNote_;
    bool lastOperationSucceeded_ = true;
};

class RenameCompositionCommand : public UndoCommand {
public:
    RenameCompositionCommand(ArtifactCompositionPtr composition,
                              QString oldName, QString newName);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("RenameCompositionCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !composition_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactCompositionWeakPtr composition_;
    QString compositionId_;
    QString oldName_;
    QString newName_;
    bool lastOperationSucceeded_ = true;
};

class ChangeLayerParentCommand : public UndoCommand {
public:
    ChangeLayerParentCommand(ArtifactAbstractLayerPtr layer,
                             LayerID oldParentId, LayerID newParentId);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("ChangeLayerParentCommand"); }
    bool canSerialize() const override { return !layerId_.isEmpty() && !layer_.expired(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    LayerID oldParentId_;
    LayerID newParentId_;
    bool lastOperationSucceeded_ = true;
};

class RenameProjectItemCommand : public UndoCommand {
public:
    RenameProjectItemCommand(ProjectItem* item,
                             QString oldName, QString newName);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("RenameProjectItemCommand"); }
    bool canSerialize() const override { return !itemId_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString itemId_;
    QString oldName_;
    QString newName_;
    bool lastOperationSucceeded_ = true;
};

class SetProjectItemTagsCommand : public UndoCommand {
public:
    SetProjectItemTagsCommand(ProjectItem* item,
                              QStringList oldTags, QStringList newTags);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetProjectItemTagsCommand"); }
    bool canSerialize() const override { return !itemId_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString itemId_;
    QStringList oldTags_;
    QStringList newTags_;
    bool lastOperationSucceeded_ = true;
};

class SetFootageAssetRoleCommand : public UndoCommand {
public:
    SetFootageAssetRoleCommand(FootageItem* item,
                               ProjectAssetUsage oldUsage,
                               ProjectRenderInputRole oldRole,
                               ProjectAssetUsage newUsage,
                               ProjectRenderInputRole newRole);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetFootageAssetRoleCommand"); }
    bool canSerialize() const override { return !itemId_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString itemId_;
    ProjectAssetUsage oldUsage_ = ProjectAssetUsage::Production;
    ProjectRenderInputRole oldRole_ = ProjectRenderInputRole::Generic;
    ProjectAssetUsage newUsage_ = ProjectAssetUsage::Production;
    ProjectRenderInputRole newRole_ = ProjectRenderInputRole::Generic;
    bool lastOperationSucceeded_ = true;
};

class MoveProjectItemCommand : public UndoCommand {
public:
    MoveProjectItemCommand(ProjectItem* item, ProjectItem* newParent);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveProjectItemCommand"); }
    bool canSerialize() const override {
        return !itemId_.isEmpty() && !oldParentId_.isEmpty() &&
               !newParentId_.isEmpty();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString itemId_;
    QString oldParentId_;
    QString newParentId_;
    bool lastOperationSucceeded_ = true;
};

class CreateProjectFolderCommand : public UndoCommand {
public:
    CreateProjectFolderCommand(QString folderId, QString parentId,
                               QString name, QStringList tags = {});
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("CreateProjectFolderCommand"); }
    bool canSerialize() const override {
        return !folderId_.isEmpty() && !name_.isEmpty();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString folderId_;
    QString parentId_;
    QString name_;
    QStringList tags_;
    bool lastOperationSucceeded_ = true;
};

class AddProjectItemsCommand : public UndoCommand {
public:
    AddProjectItemsCommand(QJsonArray items, ProjectItem* parent = nullptr,
                           QString afterCurrentCompositionId = {});
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("AddProjectItemsCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QJsonArray items_;
    QString parentId_;
    QStringList itemIds_;
    QString beforeCurrentCompositionId_;
    QString afterCurrentCompositionId_;
    bool lastOperationSucceeded_ = true;
};

class RemoveProjectItemCommand : public UndoCommand {
public:
    explicit RemoveProjectItemCommand(ProjectItem* item);
    RemoveProjectItemCommand(ProjectItem* item, QJsonObject snapshot);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("RemoveProjectItemCommand"); }
    bool canSerialize() const override {
        return !itemId_.isEmpty() && !parentId_.isEmpty() &&
               !snapshot_.isEmpty();
    }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    QString itemId_;
    QString parentId_;
    int parentIndex_ = -1;
    QJsonObject snapshot_;
    bool lastOperationSucceeded_ = true;
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
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    ArtifactInOutPoints* points_ = nullptr;
    QJsonObject before_;
    QJsonObject after_;
    bool lastOperationSucceeded_ = true;
};

class SetCompositionWorkAreaCommand : public UndoCommand {
public:
    SetCompositionWorkAreaCommand(
        ArtifactCompositionPtr composition,
        qint64 beforeStart, qint64 beforeEnd,
        qint64 afterStart, qint64 afterEnd,
        std::function<void(const ArtifactCompositionPtr&, qint64, qint64)> sync = {});
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
private:
    bool apply(qint64 start, qint64 end);
    ArtifactCompositionWeakPtr composition_;
    qint64 beforeStart_ = 0;
    qint64 beforeEnd_ = 0;
    qint64 afterStart_ = 0;
    qint64 afterEnd_ = 0;
    std::function<void(const ArtifactCompositionPtr&, qint64, qint64)> sync_;
    bool lastOperationSucceeded_ = true;
};

class SetCompositionSettingsCommand : public UndoCommand {
public:
    SetCompositionSettingsCommand(ArtifactCompositionPtr composition,
                                  QSize oldSize, float oldFrameRate,
                                  qint64 oldStart, qint64 oldEnd,
                                  FloatColor oldBackground,
                                  QSize newSize, float newFrameRate,
                                  qint64 newStart, qint64 newEnd,
                                  FloatColor newBackground);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetCompositionSettingsCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    bool apply(QSize size, float frameRate, qint64 start, qint64 end,
               FloatColor background);
    ArtifactCompositionWeakPtr composition_;
    QString compositionId_;
    QSize oldSize_;
    float oldFrameRate_ = 0.0f;
    qint64 oldStart_ = 0;
    qint64 oldEnd_ = 0;
    FloatColor oldBackground_;
    QSize newSize_;
    float newFrameRate_ = 0.0f;
    qint64 newStart_ = 0;
    qint64 newEnd_ = 0;
    FloatColor newBackground_;
    bool lastOperationSucceeded_ = true;
};

class SetCompositionResponsiveLayoutCommand : public UndoCommand {
public:
    SetCompositionResponsiveLayoutCommand(ArtifactCompositionPtr composition,
                                           QJsonObject oldLayout,
                                           QJsonObject newLayout);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("SetCompositionResponsiveLayoutCommand"); }
    bool canSerialize() const override { return !compositionId_.isEmpty() && !oldLayout_.isEmpty() && !newLayout_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    bool apply(const QJsonObject& layoutJson);
    ArtifactCompositionWeakPtr composition_;
    QString compositionId_;
    QJsonObject oldLayout_;
    QJsonObject newLayout_;
    bool lastOperationSucceeded_ = true;
};

class AlignLayersUndoCommand : public UndoCommand {
public:
    AlignLayersUndoCommand(const std::vector<AlignLayerSnapshot>& snapshots, const QString& label);
    void undo() override;
    void redo() override;
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("AlignLayersUndoCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    std::vector<AlignLayerSnapshot> snapshots_;
    QString compositionId_;
    QString label_;
    bool lastOperationSucceeded_ = true;
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
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    float oldOpacity_;
    float newOpacity_;
    bool lastOperationSucceeded_ = true;
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
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    size_t oldIndex_;
    size_t newIndex_;
    bool lastOperationSucceeded_ = true;
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
    // extracted_ owns the exact variant needed for redo after undo.  The
    // current JSON payload does not contain that variant state, so allowing
    // offload/session serialization would make redo create a different copy.
    bool canSerialize() const override { return false; }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
private:
    ArtifactAbstractLayerWeak layer_;
    QString layerId_;
    ArtifactCore::String name_;
    size_t index_;
    std::unique_ptr<LayerVariant> extracted_;
    bool lastOperationSucceeded_ = true;
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
    QString commandType() const override { return QStringLiteral("ChangeCompositionResolutionCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

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
    QString compositionId_;
    QSize oldSize_;
    QSize newSize_;
    ArtifactCore::RemapPolicy policy_;
    std::vector<LayerSnapshot> beforeSnapshots_;
    bool lastOperationSucceeded_ = true;
};

// === Undo commands for layer state toggles ===

class SetLayerVisibilityCommand : public UndoCommand {
public:
    SetLayerVisibilityCommand(ArtifactAbstractLayerPtr layer, bool visible);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetLayerLockCommand : public UndoCommand {
public:
    SetLayerLockCommand(ArtifactAbstractLayerPtr layer, bool locked);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetLayerSoloCommand : public UndoCommand {
public:
    SetLayerSoloCommand(ArtifactAbstractLayerPtr layer, bool solo);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class SetLayerShyCommand : public UndoCommand {
public:
    SetLayerShyCommand(ArtifactAbstractLayerPtr layer, bool shy);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

class ChangeLayerBlendModeCommand : public UndoCommand {
public:
    ChangeLayerBlendModeCommand(ArtifactAbstractLayerPtr layer, LAYER_BLEND_TYPE newMode);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
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
    bool lastOperationSucceeded_ = true;
};

// === Macro command for batching multiple undo commands ===

class MacroUndoCommand : public UndoCommand {
public:
    explicit MacroUndoCommand(const QString& label);
    void addChild(std::unique_ptr<UndoCommand> child);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    bool handlesFailedOperationCompensation() const override { return true; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MacroUndoCommand"); }
    bool canSerialize() const override;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    std::vector<std::unique_ptr<UndoCommand>> children_;
    QString label_;
    bool lastOperationSucceeded_ = true;
};

class MoveAssetFileCommand : public UndoCommand {
public:
    MoveAssetFileCommand(const QString& oldPath, const QString& newPath);
    void undo() override;
    void redo() override;
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
    QString commandType() const override { return QStringLiteral("MoveAssetFileCommand"); }
    bool canSerialize() const override { return !oldPath_.isEmpty() && !newPath_.isEmpty(); }
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject& data) override;
private:
    QString oldPath_;
    QString newPath_;
    bool lastOperationSucceeded_ = true;
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
    // Executes and retains cmd. Returns false when the budget rejects it or
    // the command reports that its initial redo did not succeed.
    bool push(std::unique_ptr<UndoCommand> cmd);
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
    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
    QString label() const override;
    size_t estimatedMemoryBytes() const override;
private:
    QString label_;
    QByteArray beforeState_;
    QByteArray afterState_;
    RestoreFn restoreFn_;
    bool lastOperationSucceeded_ = true;
};

}
