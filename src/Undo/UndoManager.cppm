module;
#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <QMap>

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
#include <limits>
#include <regex>
#include <random>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
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

LayerMask decodeMask(const QJsonObject& object);

namespace {
constexpr qint64 kMaxUndoPayloadBytes = 64ll * 1024ll * 1024ll;

class OffloadedUndoCommand final : public UndoCommand {
public:
    using FactoryMap = QMap<QString, UndoManager::CommandFactory>;

    OffloadedUndoCommand(QString path, QString type, QString label,
                         FactoryMap* factories, size_t bytes)
        : path_(std::move(path)), type_(std::move(type)), label_(std::move(label)),
          factories_(factories), bytes_(bytes) {}

    void undo() override { if (auto command = restore()) command->undo(); }
    void redo() override { if (auto command = restore()) command->redo(); }
    QString label() const override { return label_; }
    size_t estimatedMemoryBytes() const override { return sizeof(*this); }
    QString commandType() const override { return type_; }
    bool canSerialize() const override { return true; }
    bool isOffloaded() const override { return true; }
    QJsonObject serialize() const override {
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly)) return {};
        if (file.size() < 0 || file.size() > kMaxUndoPayloadBytes) return {};
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
        const QJsonObject entry = document.object();
        if (entry.value(QStringLiteral("version")).toInt(-1) != 1) return {};
        return entry.value(QStringLiteral("data")).toObject();
    }

private:
    std::unique_ptr<UndoCommand> restore() const {
        if (!factories_) return nullptr;
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly)) return nullptr;
        if (file.size() < 0 || file.size() > kMaxUndoPayloadBytes) return nullptr;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return nullptr;
        const QJsonObject entry = document.object();
        if (entry.value(QStringLiteral("version")).toInt(-1) != 1 ||
            !entry.value(QStringLiteral("data")).isObject()) return nullptr;
        const auto factory = factories_->find(type_);
        if (factory == factories_->end()) return nullptr;
        auto command = factory.value()(entry.value(QStringLiteral("data")).toObject());
        if (!command ||
            !command->deserialize(entry.value(QStringLiteral("data")).toObject()) ||
            !command->canSerialize()) return nullptr;
        return command;
    }

    QString path_;
    QString type_;
    QString label_;
    FactoryMap* factories_ = nullptr;
    size_t bytes_ = 0;
};
}

InOutPointsSnapshotCommand::InOutPointsSnapshotCommand(
    ArtifactInOutPoints* points, const QJsonObject& before,
    const QJsonObject& after)
    : points_(points), before_(before), after_(after) {}

void InOutPointsSnapshotCommand::undo() {
    if (points_) points_->fromJson(before_);
}

void InOutPointsSnapshotCommand::redo() {
    if (points_) points_->fromJson(after_);
}

QString InOutPointsSnapshotCommand::label() const {
    return QStringLiteral("Change Timeline Markers");
}

size_t InOutPointsSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(QJsonDocument(before_).toJson(QJsonDocument::Compact).size())
        + static_cast<size_t>(QJsonDocument(after_).toJson(QJsonDocument::Compact).size());
}

QJsonObject InOutPointsSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_}};
}

bool InOutPointsSnapshotCommand::deserialize(const QJsonObject& data) {
    before_ = data.value(QStringLiteral("before")).toObject();
    after_ = data.value(QStringLiteral("after")).toObject();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    points_ = manager->resolveInOutPoints();
    return points_ != nullptr;
}

class UndoManager::Impl {
public:
    std::vector<std::unique_ptr<UndoCommand>> undoStack;
    std::vector<std::unique_ptr<UndoCommand>> redoStack;
    size_t maxHistorySize_ = 100;
    UndoManager::UndoBudget budget_;
    UndoManager::OffloadPolicy offloadPolicy_ = UndoManager::OffloadPolicy::Never;
    QString offloadDirectory_;
    QMap<QString, UndoManager::CommandFactory> commandFactories_;
    UndoManager::EffectResolver effectResolver_;
    UndoManager::LayerResolver layerResolver_;
    UndoManager::CompositionResolver compositionResolver_;
    UndoManager::InOutPointsResolver inOutPointsResolver_;
    int64_t version_ = 0;
    int64_t savedVersion_ = 0;

    size_t stackBytes(const std::vector<std::unique_ptr<UndoCommand>>& stack) const {
        size_t total = 0;
        for (const auto& command : stack) {
            if (command) total += command->estimatedMemoryBytes();
        }
        return total;
    }

    void enforceBudget() {
        maxHistorySize_ = budget_.maxEntryCount;
        undoStack.erase(std::remove_if(undoStack.begin(), undoStack.end(),
            [this](const auto& command) {
                return !command || (!command->isOffloaded() &&
                                     command->estimatedMemoryBytes() > budget_.maxSingleEntryBytes);
            }), undoStack.end());
        while (undoStack.size() > budget_.maxEntryCount && !undoStack.empty()) {
            undoStack.erase(undoStack.begin());
        }
        while (stackBytes(undoStack) > budget_.maxMemoryBytes && !undoStack.empty()) {
            undoStack.erase(undoStack.begin());
        }
    }

    bool offloadEntry(size_t index) {
        if (index >= undoStack.size() || offloadDirectory_.isEmpty()) return false;
        auto& command = undoStack[index];
        if (!command || !command->canSerialize() || command->isOffloaded() || command->commandType().isEmpty()) return false;
        if (!QDir().mkpath(offloadDirectory_)) return false;
        const QString path = QDir(offloadDirectory_).filePath(
            QStringLiteral("undo_%1.json").arg(static_cast<qulonglong>(index)));
        QJsonObject entry;
        entry.insert(QStringLiteral("version"), 1);
        entry.insert(QStringLiteral("type"), command->commandType());
        entry.insert(QStringLiteral("label"), command->label());
        entry.insert(QStringLiteral("estimatedBytes"), static_cast<qint64>(command->estimatedMemoryBytes()));
        const QJsonObject data = command->serialize();
        if (data.isEmpty()) return false;
        entry.insert(QStringLiteral("data"), data);
        const QByteArray payload = QJsonDocument(entry).toJson(QJsonDocument::Compact);
        if (payload.size() > kMaxUndoPayloadBytes) return false;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) return false;
        undoStack[index] = std::make_unique<OffloadedUndoCommand>(
            path, command->commandType(), command->label(), &commandFactories_,
            command->estimatedMemoryBytes());
        return true;
    }

    void applyOffloadPolicy() {
        if (offloadPolicy_ == UndoManager::OffloadPolicy::Never || undoStack.size() < 2) return;
        size_t limit = offloadPolicy_ == UndoManager::OffloadPolicy::Always
            ? undoStack.size() - 1
            : (memoryPressure() > 0.8f ? undoStack.size() / 2 : 0);
        for (size_t i = 0; i < limit; ++i) offloadEntry(i);
    }

    void cleanupOffloadFiles() {
        if (offloadDirectory_.isEmpty()) return;
        QDir directory(offloadDirectory_);
        const QStringList files = directory.entryList(
            QStringList{QStringLiteral("undo_*.json")}, QDir::Files);
        for (const auto& file : files) directory.remove(file);
    }

    float memoryPressure() const {
        if (budget_.maxMemoryBytes == 0) return 1.0f;
        return std::min(1.0f, static_cast<float>(stackBytes(undoStack)) /
                                  static_cast<float>(budget_.maxMemoryBytes));
    }
};

// --- SetPropertyCommand ---
SetPropertyCommand::SetPropertyCommand(ArtifactAbstractEffectPtr target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue)
    : target_(target), effectId_(target ? target->effectID().toQString() : QString()),
      name_(propName), oldValue_(oldValue), newValue_(newValue) {}

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

AnimationLayerStackSnapshotCommand::AnimationLayerStackSnapshotCommand(
    ArtifactAbstractLayerPtr layer, const QJsonObject& before,
    const QJsonObject& after)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()), before_(before), after_(after) {}

void AnimationLayerStackSnapshotCommand::undo() {
    if (auto layer = layer_.lock()) {
        layer->restoreAnimationLayersSnapshot(before_);
        layer->changed();
    }
}

void AnimationLayerStackSnapshotCommand::redo() {
    if (auto layer = layer_.lock()) {
        layer->restoreAnimationLayersSnapshot(after_);
        layer->changed();
    }
}

QString AnimationLayerStackSnapshotCommand::label() const {
    return QStringLiteral("Change Animation Layers");
}

QJsonObject AnimationLayerStackSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_}};
}

bool AnimationLayerStackSnapshotCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    before_ = data.value(QStringLiteral("before")).toObject();
    after_ = data.value(QStringLiteral("after")).toObject();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

ReplaceLayerSourceCommand::ReplaceLayerSourceCommand(
    ArtifactAbstractLayerPtr layer, QString propertyPath,
    QString oldSourcePath, QString newSourcePath)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      propertyPath_(propertyPath),
      oldSourcePath_(oldSourcePath),
      newSourcePath_(newSourcePath) {}

void ReplaceLayerSourceCommand::undo() {
    if (auto layer = layer_.lock()) {
        layer->setLayerPropertyValue(propertyPath_, oldSourcePath_);
        if (auto* mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void ReplaceLayerSourceCommand::redo() {
    if (auto layer = layer_.lock()) {
        layer->setLayerPropertyValue(propertyPath_, newSourcePath_);
        if (auto* mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString ReplaceLayerSourceCommand::label() const {
    return QStringLiteral("Replace Layer Source");
}

size_t ReplaceLayerSourceCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size() + propertyPath_.size() +
                                                oldSourcePath_.size() + newSourcePath_.size()) * sizeof(QChar);
}

QJsonObject ReplaceLayerSourceCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("propertyPath"), propertyPath_},
                       {QStringLiteral("oldSourcePath"), oldSourcePath_},
                       {QStringLiteral("newSourcePath"), newSourcePath_}};
}

bool ReplaceLayerSourceCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    propertyPath_ = data.value(QStringLiteral("propertyPath")).toString();
    oldSourcePath_ = data.value(QStringLiteral("oldSourcePath")).toString();
    newSourcePath_ = data.value(QStringLiteral("newSourcePath")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !propertyPath_.isEmpty() && !layer_.expired();
}

ToggleLocalizedSourceCommand::ToggleLocalizedSourceCommand(
    std::function<void()> localize, std::function<void()> relinkShared,
    QString label)
    : localize_(localize), relinkShared_(relinkShared), label_(label) {}

void ToggleLocalizedSourceCommand::undo() {
    if (relinkShared_) {
        relinkShared_();
    }
    if (auto* mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void ToggleLocalizedSourceCommand::redo() {
    if (localize_) {
        localize_();
    }
    if (auto* mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString ToggleLocalizedSourceCommand::label() const {
    return label_;
}

// --- MoveLayerCommand ---
MoveLayerCommand::MoveLayerCommand(ArtifactAbstractLayerPtr layer, float deltaX, float deltaY, int64_t frame)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      dx_(deltaX), dy_(deltaY), frame_(frame) {}

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

size_t MoveLayerCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject MoveLayerCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("dx"), dx_}, {QStringLiteral("dy"), dy_},
                       {QStringLiteral("frame"), static_cast<qint64>(frame_)}};
}

bool MoveLayerCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    dx_ = static_cast<float>(data.value(QStringLiteral("dx")).toDouble());
    dy_ = static_cast<float>(data.value(QStringLiteral("dy")).toDouble());
    frame_ = data.value(QStringLiteral("frame")).toInteger();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- AddLayerCommand ---
AddLayerCommand::AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop)
    : comp_(comp), layer_(layer),
      compositionId_(comp ? comp->id().toString() : QString()),
      layerId_(layer ? layer->id().toQString() : QString()),
      atTop_(atTop), savedIndex_(-1) {}

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

size_t AddLayerCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(compositionId_.size() + layerId_.size()) * sizeof(QChar);
}

QJsonObject AddLayerCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("atTop"), atTop_},
                       {QStringLiteral("savedIndex"), savedIndex_}};
}

bool AddLayerCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    atTop_ = data.value(QStringLiteral("atTop")).toBool(true);
    savedIndex_ = data.value(QStringLiteral("savedIndex")).toInt(-1);
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    comp_ = manager->resolveComposition(compositionId_);
    layer_ = manager->resolveLayer(layerId_);
    return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && static_cast<bool>(layer_);
}

// --- RemoveLayerCommand ---
RemoveLayerCommand::RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer)
    : comp_(comp), layer_(layer),
      compositionId_(comp ? comp->id().toString() : QString()),
      layerId_(layer ? layer->id().toQString() : QString()),
      originalIndex_(-1) {}

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

size_t RemoveLayerCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(compositionId_.size() + layerId_.size()) * sizeof(QChar);
}

QJsonObject RemoveLayerCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("originalIndex"), originalIndex_}};
}

bool RemoveLayerCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    originalIndex_ = data.value(QStringLiteral("originalIndex")).toInt(-1);
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    comp_ = manager->resolveComposition(compositionId_);
    layer_ = manager->resolveLayer(layerId_);
    return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && static_cast<bool>(layer_);
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

void applyLayerPropertyKeyframeSnapshot(
    const ArtifactAbstractLayerPtr& layer,
    const QString& propertyPath,
    const std::vector<ArtifactCore::KeyFrame>& keyframes) {
    if (!layer || propertyPath.trimmed().isEmpty()) {
        return;
    }

    auto property = layer->getProperty(propertyPath);
    if (!property) {
        return;
    }

    if (keyframes.empty()) {
        property->clearKeyFrames();
    } else {
        property->clearKeyFrames();
        for (const auto& keyframe : keyframes) {
            property->addKeyFrame(keyframe.time, keyframe.value,
                                  static_cast<int>(keyframe.interpolation),
                                  keyframe.cp1_x, keyframe.cp1_y,
                                  keyframe.cp2_x, keyframe.cp2_y,
                                  keyframe.roving);
        }
    }

    layer->changed();
}

void applyTextLayerTextSnapshot(const ArtifactAbstractLayerPtr& layer,
                                const QString& text) {
    if (!layer) {
        return;
    }

    layer->setLayerPropertyValue(QStringLiteral("text.value"), text);
}

void applyEffectMaskImageSnapshot(
    const ArtifactAbstractEffectPtr& effect,
    const std::vector<SharedPtr<ImageF32x4_RGBA>>& masks) {
    if (!effect) {
        return;
    }

    effect->clearEffectMaskImages();
    for (const auto& mask : masks) {
        if (mask) {
            effect->addEffectMaskImage(mask);
        }
    }
}
} // namespace

// --- MaskEditCommand ---
MaskEditCommand::MaskEditCommand(ArtifactAbstractLayerPtr layer,
                                 std::vector<LayerMask> beforeMasks,
                                 std::vector<LayerMask> afterMasks)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      beforeMasks_(std::move(beforeMasks)), afterMasks_(std::move(afterMasks)) {}

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

QJsonObject encodeEffectMaskImage(const SharedPtr<ImageF32x4_RGBA>& image) {
    if (!image || image->isEmpty()) return QJsonObject{};
    const int width = image->width();
    const int height = image->height();
    QByteArray raw;
    raw.resize(static_cast<int>(static_cast<std::size_t>(width) * height * 4u * sizeof(float)));
    auto* pixels = reinterpret_cast<float*>(raw.data());
    std::size_t index = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto color = image->getPixel(x, y);
            pixels[index++] = color.r(); pixels[index++] = color.g();
            pixels[index++] = color.b(); pixels[index++] = color.a();
        }
    }
    return QJsonObject{{QStringLiteral("width"), width}, {QStringLiteral("height"), height},
                       {QStringLiteral("rgba32f"), QString::fromLatin1(raw.toBase64())}};
}

SharedPtr<ImageF32x4_RGBA> decodeEffectMaskImage(const QJsonObject& object) {
    const int width = object.value(QStringLiteral("width")).toInt();
    const int height = object.value(QStringLiteral("height")).toInt();
    const QByteArray raw = QByteArray::fromBase64(object.value(QStringLiteral("rgba32f")).toString().toLatin1());
    const std::size_t expected = static_cast<std::size_t>(std::max(0, width)) *
                                 static_cast<std::size_t>(std::max(0, height)) * 4u * sizeof(float);
    constexpr std::size_t kMaxSerializedMaskBytes = 64u * 1024u * 1024u;
    if (width <= 0 || height <= 0 || expected > kMaxSerializedMaskBytes ||
        expected > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max()) ||
        raw.size() != static_cast<qsizetype>(expected)) return {};
    auto image = SharedPtr<ImageF32x4_RGBA>(std::make_shared<ImageF32x4_RGBA>());
    image->setFromRGBA32F(reinterpret_cast<const float*>(raw.constData()), width, height);
    return image;
}

QJsonObject encodeMask(const LayerMask& mask);

QJsonObject MaskEditCommand::serialize() const {
    const auto encode = [](const auto& masks) {
        QJsonArray values;
        for (const auto& mask : masks) values.append(encodeMask(mask));
        return values;
    };
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), encode(beforeMasks_)},
                       {QStringLiteral("after"), encode(afterMasks_)}};
}

bool MaskEditCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto decode = [](const QJsonArray& values, auto& masks) {
        masks.clear();
        for (const auto& value : values) masks.push_back(decodeMask(value.toObject()));
    };
    decode(data.value(QStringLiteral("before")).toArray(), beforeMasks_);
    decode(data.value(QStringLiteral("after")).toArray(), afterMasks_);
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

QJsonObject encodeMask(const LayerMask& mask) {
    QJsonArray paths;
    for (int i = 0; i < mask.maskPathCount(); ++i) {
        const auto path = mask.maskPath(i);
        QJsonArray vertices;
        for (int v = 0; v < path.vertexCount(); ++v) {
            const auto vertex = path.vertex(v);
            vertices.append(QJsonObject{
                {QStringLiteral("x"), vertex.position.x()}, {QStringLiteral("y"), vertex.position.y()},
                {QStringLiteral("inX"), vertex.inTangent.x()}, {QStringLiteral("inY"), vertex.inTangent.y()},
                {QStringLiteral("outX"), vertex.outTangent.x()}, {QStringLiteral("outY"), vertex.outTangent.y()}});
        }
        paths.append(QJsonObject{
            {QStringLiteral("vertices"), vertices}, {QStringLiteral("closed"), path.isClosed()},
            {QStringLiteral("opacity"), path.opacity()}, {QStringLiteral("feather"), path.feather()},
            {QStringLiteral("featherHorizontal"), path.featherHorizontal()},
            {QStringLiteral("featherVertical"), path.featherVertical()},
            {QStringLiteral("featherInner"), path.featherInner()}, {QStringLiteral("featherOuter"), path.featherOuter()},
            {QStringLiteral("expansion"), path.expansion()}, {QStringLiteral("inverted"), path.isInverted()},
            {QStringLiteral("mode"), static_cast<int>(path.mode())}, {QStringLiteral("name"), path.name().toQString()}});
    }
    return QJsonObject{{QStringLiteral("enabled"), mask.isEnabled()},
                       {QStringLiteral("locked"), mask.isLocked()}, {QStringLiteral("paths"), paths}};
}

LayerMask decodeMask(const QJsonObject& object) {
    LayerMask mask;
    mask.setEnabled(object.value(QStringLiteral("enabled")).toBool(true));
    mask.setLocked(object.value(QStringLiteral("locked")).toBool(false));
    for (const auto& value : object.value(QStringLiteral("paths")).toArray()) {
        const auto pathObject = value.toObject();
        MaskPath path;
        path.setClosed(pathObject.value(QStringLiteral("closed")).toBool(true));
        path.setOpacity(static_cast<float>(pathObject.value(QStringLiteral("opacity")).toDouble(1.0)));
        path.setFeather(static_cast<float>(pathObject.value(QStringLiteral("feather")).toDouble()));
        path.setFeatherHorizontal(static_cast<float>(pathObject.value(QStringLiteral("featherHorizontal")).toDouble()));
        path.setFeatherVertical(static_cast<float>(pathObject.value(QStringLiteral("featherVertical")).toDouble()));
        path.setFeatherInner(static_cast<float>(pathObject.value(QStringLiteral("featherInner")).toDouble()));
        path.setFeatherOuter(static_cast<float>(pathObject.value(QStringLiteral("featherOuter")).toDouble()));
        path.setExpansion(static_cast<float>(pathObject.value(QStringLiteral("expansion")).toDouble()));
        path.setInverted(pathObject.value(QStringLiteral("inverted")).toBool(false));
        path.setMode(static_cast<MaskMode>(pathObject.value(QStringLiteral("mode")).toInt()));
        path.setName(UniString::fromQString(pathObject.value(QStringLiteral("name")).toString()));
        for (const auto& vertexValue : pathObject.value(QStringLiteral("vertices")).toArray()) {
            const auto vertexObject = vertexValue.toObject();
            MaskVertex vertex;
            vertex.position = QPointF(vertexObject.value(QStringLiteral("x")).toDouble(), vertexObject.value(QStringLiteral("y")).toDouble());
            vertex.inTangent = QPointF(vertexObject.value(QStringLiteral("inX")).toDouble(), vertexObject.value(QStringLiteral("inY")).toDouble());
            vertex.outTangent = QPointF(vertexObject.value(QStringLiteral("outX")).toDouble(), vertexObject.value(QStringLiteral("outY")).toDouble());
            path.addVertex(vertex);
        }
        mask.addMaskPath(path);
    }
    return mask;
}

// --- ChangeLayerMatteReferencesCommand ---
ChangeLayerMatteReferencesCommand::ChangeLayerMatteReferencesCommand(
    ArtifactAbstractLayerPtr layer,
    std::vector<LayerMatteReference> beforeRefs,
    std::vector<LayerMatteReference> afterRefs)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
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

size_t ChangeLayerMatteReferencesCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + (beforeRefs_.size() + afterRefs_.size()) * sizeof(LayerMatteReference);
}

QJsonObject ChangeLayerMatteReferencesCommand::serialize() const {
    const auto encode = [](const auto& refs) {
        QJsonArray values;
        for (const auto& ref : refs) values.append(ref.toJson());
        return values;
    };
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), encode(beforeRefs_)},
                       {QStringLiteral("after"), encode(afterRefs_)}};
}

bool ChangeLayerMatteReferencesCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto decode = [](const QJsonArray& values, auto& refs) {
        refs.clear();
        for (const auto& value : values) {
            LayerMatteReference ref;
            ref.fromJson(value.toObject());
            refs.push_back(std::move(ref));
        }
    };
    decode(data.value(QStringLiteral("before")).toArray(), beforeRefs_);
    decode(data.value(QStringLiteral("after")).toArray(), afterRefs_);
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

namespace {
QJsonValue encodeKeyframeValue(const std::any& value, bool& supported) {
    supported = true;
    if (!value.has_value()) return QJsonValue::Null;
    if (const auto* v = std::any_cast<QVariant>(&value)) return QJsonValue::fromVariant(*v);
    if (const auto* v = std::any_cast<float>(&value)) return *v;
    if (const auto* v = std::any_cast<double>(&value)) return *v;
    if (const auto* v = std::any_cast<int>(&value)) return *v;
    if (const auto* v = std::any_cast<bool>(&value)) return *v;
    if (const auto* v = std::any_cast<QString>(&value)) return *v;
    supported = false;
    return {};
}

bool decodeKeyframes(const QJsonArray& encoded, std::vector<ArtifactCore::KeyFrame>& target) {
    target.clear();
    target.reserve(encoded.size());
    for (const auto& item : encoded) {
        const auto object = item.toObject();
        if (!object.contains(QStringLiteral("frame")) || !object.contains(QStringLiteral("value"))) return false;
        ArtifactCore::KeyFrame keyframe;
        keyframe.time = ArtifactCore::RationalTime(
            static_cast<int64_t>(object.value(QStringLiteral("frame")).toDouble()), 30);
        keyframe.value = QVariant(object.value(QStringLiteral("value")).toVariant());
        keyframe.interpolation = static_cast<ArtifactCore::InterpolationType>(object.value(QStringLiteral("interpolation")).toInt());
        keyframe.cp1_x = static_cast<float>(object.value(QStringLiteral("cp1_x")).toDouble());
        keyframe.cp1_y = static_cast<float>(object.value(QStringLiteral("cp1_y")).toDouble());
        keyframe.cp2_x = static_cast<float>(object.value(QStringLiteral("cp2_x")).toDouble());
        keyframe.cp2_y = static_cast<float>(object.value(QStringLiteral("cp2_y")).toDouble());
        target.push_back(std::move(keyframe));
    }
    return true;
}
}

// --- SetLayerPropertyKeyframesCommand ---
SetLayerPropertyKeyframesCommand::SetLayerPropertyKeyframesCommand(
    ArtifactAbstractLayerPtr layer,
    QString propertyPath,
    std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
    std::vector<ArtifactCore::KeyFrame> afterKeyframes,
    QString label)
    : layer_(layer),
      propertyPath_(std::move(propertyPath)),
      beforeKeyframes_(std::move(beforeKeyframes)),
      afterKeyframes_(std::move(afterKeyframes)),
      label_(std::move(label)) {}

void SetLayerPropertyKeyframesCommand::undo() {
    applyLayerPropertyKeyframeSnapshot(layer_.lock(), propertyPath_, beforeKeyframes_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void SetLayerPropertyKeyframesCommand::redo() {
    applyLayerPropertyKeyframeSnapshot(layer_.lock(), propertyPath_, afterKeyframes_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString SetLayerPropertyKeyframesCommand::label() const {
    return label_;
}

bool SetLayerPropertyKeyframesCommand::canSerialize() const {
    if (layerId_.isEmpty() || propertyPath_.isEmpty()) return false;
    for (const auto& keyframe : beforeKeyframes_) { bool supported = false; encodeKeyframeValue(keyframe.value, supported); if (!supported) return false; }
    for (const auto& keyframe : afterKeyframes_) { bool supported = false; encodeKeyframeValue(keyframe.value, supported); if (!supported) return false; }
    return true;
}

size_t SetPropertyCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(effectId_.size() + name_.toQString().size()) * sizeof(QChar) +
           static_cast<size_t>(oldValue_.toString().size() + newValue_.toString().size()) * sizeof(QChar);
}

QJsonObject SetLayerPropertyKeyframesCommand::serialize() const {
    QJsonObject data{{QStringLiteral("layerId"), layerId_}, {QStringLiteral("propertyPath"), propertyPath_}, {QStringLiteral("label"), label_}};
    const auto encode = [](const auto& keyframes) {
        QJsonArray result;
        for (const auto& keyframe : keyframes) {
            bool supported = false;
            const auto value = encodeKeyframeValue(keyframe.value, supported);
            if (!supported) continue;
            result.append(QJsonObject{{QStringLiteral("frame"), static_cast<qint64>(keyframe.time.rescaledTo(30))}, {QStringLiteral("value"), value}, {QStringLiteral("interpolation"), static_cast<int>(keyframe.interpolation)}, {QStringLiteral("cp1_x"), keyframe.cp1_x}, {QStringLiteral("cp1_y"), keyframe.cp1_y}, {QStringLiteral("cp2_x"), keyframe.cp2_x}, {QStringLiteral("cp2_y"), keyframe.cp2_y}});
        }
        return result;
    };
    data.insert(QStringLiteral("before"), encode(beforeKeyframes_));
    data.insert(QStringLiteral("after"), encode(afterKeyframes_));
    return data;
}

bool SetLayerPropertyKeyframesCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    propertyPath_ = data.value(QStringLiteral("propertyPath")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    if (!decodeKeyframes(data.value(QStringLiteral("before")).toArray(), beforeKeyframes_) || !decodeKeyframes(data.value(QStringLiteral("after")).toArray(), afterKeyframes_)) return false;
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !propertyPath_.isEmpty() && !layer_.expired();
}

QJsonObject SetPropertyCommand::serialize() const {
    QJsonObject data;
    data.insert(QStringLiteral("effectId"), effectId_);
    data.insert(QStringLiteral("property"), name_.toQString());
    data.insert(QStringLiteral("oldValue"), QJsonValue::fromVariant(oldValue_));
    data.insert(QStringLiteral("newValue"), QJsonValue::fromVariant(newValue_));
    return data;
}

bool SetPropertyCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    name_ = UniString::fromQString(data.value(QStringLiteral("property")).toString());
    oldValue_ = data.value(QStringLiteral("oldValue")).toVariant();
    newValue_ = data.value(QStringLiteral("newValue")).toVariant();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    target_ = manager->resolveEffect(effectId_);
    return !effectId_.isEmpty() && !name_.toQString().isEmpty() && !target_.expired();
}

size_t SetLayerPropertyKeyframesCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size() + propertyPath_.size() + label_.size()) * sizeof(QChar) +
           (beforeKeyframes_.size() + afterKeyframes_.size()) * sizeof(ArtifactCore::KeyFrame);
}

// --- SetTextLayerTextCommand ---
SetTextLayerTextCommand::SetTextLayerTextCommand(
    ArtifactAbstractLayerPtr layer,
    QString beforeText,
    QString afterText,
    QString label)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      beforeText_(std::move(beforeText)),
      afterText_(std::move(afterText)),
      label_(std::move(label)) {}

void SetTextLayerTextCommand::undo() {
    applyTextLayerTextSnapshot(layer_.lock(), beforeText_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void SetTextLayerTextCommand::redo() {
    applyTextLayerTextSnapshot(layer_.lock(), afterText_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString SetTextLayerTextCommand::label() const {
    return label_;
}

size_t SetTextLayerTextCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(beforeText_.size() + afterText_.size() + label_.size()) * sizeof(QChar);
}

QJsonObject SetTextLayerTextCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("beforeText"), beforeText_},
                       {QStringLiteral("afterText"), afterText_},
                       {QStringLiteral("label"), label_}};
}

bool SetTextLayerTextCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    beforeText_ = data.value(QStringLiteral("beforeText")).toString();
    afterText_ = data.value(QStringLiteral("afterText")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- SetEffectMaskImagesCommand ---
SetEffectMaskImagesCommand::SetEffectMaskImagesCommand(
    ArtifactAbstractEffectPtr effect,
    std::vector<SharedPtr<ImageF32x4_RGBA>> beforeMasks,
    std::vector<SharedPtr<ImageF32x4_RGBA>> afterMasks,
    QString label)
    : effect_(effect),
      effectId_(effect ? effect->effectID().toQString() : QString()),
      beforeMasks_(std::move(beforeMasks)),
      afterMasks_(std::move(afterMasks)),
      label_(std::move(label)) {}

void SetEffectMaskImagesCommand::undo() {
    applyEffectMaskImageSnapshot(effect_.lock(), beforeMasks_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void SetEffectMaskImagesCommand::redo() {
    applyEffectMaskImageSnapshot(effect_.lock(), afterMasks_);
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString SetEffectMaskImagesCommand::label() const {
    return label_;
}

QJsonObject SetEffectMaskImagesCommand::serialize() const {
    const auto encode = [](const auto& masks) {
        QJsonArray values;
        for (const auto& mask : masks) values.append(encodeEffectMaskImage(mask));
        return values;
    };
    return QJsonObject{{QStringLiteral("effectId"), effectId_},
                       {QStringLiteral("label"), label_},
                       {QStringLiteral("before"), encode(beforeMasks_)},
                       {QStringLiteral("after"), encode(afterMasks_)}};
}

bool SetEffectMaskImagesCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    const auto decode = [](const QJsonArray& values, auto& masks) {
        masks.clear();
        for (const auto& value : values) {
            auto image = decodeEffectMaskImage(value.toObject());
            if (!image) return false;
            masks.push_back(std::move(image));
        }
        return true;
    };
    if (!decode(data.value(QStringLiteral("before")).toArray(), beforeMasks_) ||
        !decode(data.value(QStringLiteral("after")).toArray(), afterMasks_)) return false;
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    effect_ = manager->resolveEffect(effectId_);
    return !effectId_.isEmpty() && !effect_.expired();
}

size_t SetEffectMaskImagesCommand::estimatedMemoryBytes() const {
    size_t total = sizeof(*this);
    const auto addImageBytes = [&total](const auto& images) {
        for (const auto& image : images) {
            if (image) {
                total += static_cast<size_t>(std::max(0, image->width())) *
                         static_cast<size_t>(std::max(0, image->height())) *
                         4u * sizeof(float);
            }
        }
    };
    addImageBytes(beforeMasks_);
    addImageBytes(afterMasks_);
    return total;
}

// --- AlignLayersUndoCommand ---
AlignLayersUndoCommand::AlignLayersUndoCommand(const std::vector<AlignLayerSnapshot>& snapshots, const QString& label)
    : snapshots_(snapshots), label_(label) {
    if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        if (const auto composition = selection->activeComposition()) {
            compositionId_ = composition->id().toString();
        }
    }
}

void AlignLayersUndoCommand::undo() {
    for (const auto& s : snapshots_) {
        auto* manager = UndoManager::instance();
        auto comp = manager ? manager->resolveComposition(compositionId_) : ArtifactCompositionPtr{};
        if (!comp) {
            auto* sel = ArtifactLayerSelectionManager::instance();
            comp = sel ? sel->activeComposition() : ArtifactCompositionPtr{};
        }
        if (!comp) continue;
        auto layer = comp->layerById(LayerID(s.layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(RationalTime(0, 30000), s.beforeX, s.beforeY);
        layer->transform3D().setScale(RationalTime(0, 30000), s.beforeScaleX, s.beforeScaleY);
        layer->changed();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

void AlignLayersUndoCommand::redo() {
    for (const auto& s : snapshots_) {
        auto* manager = UndoManager::instance();
        auto comp = manager ? manager->resolveComposition(compositionId_) : ArtifactCompositionPtr{};
        if (!comp) {
            auto* sel = ArtifactLayerSelectionManager::instance();
            comp = sel ? sel->activeComposition() : ArtifactCompositionPtr{};
        }
        if (!comp) continue;
        auto layer = comp->layerById(LayerID(s.layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(RationalTime(0, 30000), s.afterX, s.afterY);
        layer->transform3D().setScale(RationalTime(0, 30000), s.afterScaleX, s.afterScaleY);
        layer->changed();
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

QString AlignLayersUndoCommand::label() const { return label_; }

size_t AlignLayersUndoCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this) + static_cast<size_t>(compositionId_.size() + label_.size()) * sizeof(QChar);
    for (const auto& snapshot : snapshots_) {
        bytes += sizeof(snapshot) + static_cast<size_t>(snapshot.layerId.size()) * sizeof(QChar);
    }
    return bytes;
}

QJsonObject AlignLayersUndoCommand::serialize() const {
    QJsonArray values;
    for (const auto& snapshot : snapshots_) {
        values.append(QJsonObject{{QStringLiteral("layerId"), snapshot.layerId},
                                  {QStringLiteral("beforeX"), snapshot.beforeX},
                                  {QStringLiteral("beforeY"), snapshot.beforeY},
                                  {QStringLiteral("afterX"), snapshot.afterX},
                                  {QStringLiteral("afterY"), snapshot.afterY},
                                  {QStringLiteral("beforeScaleX"), snapshot.beforeScaleX},
                                  {QStringLiteral("beforeScaleY"), snapshot.beforeScaleY},
                                  {QStringLiteral("afterScaleX"), snapshot.afterScaleX},
                                  {QStringLiteral("afterScaleY"), snapshot.afterScaleY}});
    }
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("label"), label_},
                       {QStringLiteral("snapshots"), values}};
}

bool AlignLayersUndoCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    snapshots_.clear();
    for (const auto& value : data.value(QStringLiteral("snapshots")).toArray()) {
        const auto object = value.toObject();
        AlignLayerSnapshot snapshot{};
        snapshot.layerId = object.value(QStringLiteral("layerId")).toString();
        snapshot.beforeX = static_cast<float>(object.value(QStringLiteral("beforeX")).toDouble());
        snapshot.beforeY = static_cast<float>(object.value(QStringLiteral("beforeY")).toDouble());
        snapshot.afterX = static_cast<float>(object.value(QStringLiteral("afterX")).toDouble());
        snapshot.afterY = static_cast<float>(object.value(QStringLiteral("afterY")).toDouble());
        snapshot.beforeScaleX = static_cast<float>(object.value(QStringLiteral("beforeScaleX")).toDouble(1.0));
        snapshot.beforeScaleY = static_cast<float>(object.value(QStringLiteral("beforeScaleY")).toDouble(1.0));
        snapshot.afterScaleX = static_cast<float>(object.value(QStringLiteral("afterScaleX")).toDouble(1.0));
        snapshot.afterScaleY = static_cast<float>(object.value(QStringLiteral("afterScaleY")).toDouble(1.0));
        snapshots_.push_back(std::move(snapshot));
    }
    auto* manager = UndoManager::instance();
    return manager && !compositionId_.isEmpty() && !snapshots_.empty() &&
           static_cast<bool>(manager->resolveComposition(compositionId_));
}

// --- SetLayerVisibilityCommand ---
SetLayerVisibilityCommand::SetLayerVisibilityCommand(ArtifactAbstractLayerPtr layer, bool visible)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldVisible_(layer ? layer->isVisible() : true), newVisible_(visible) {}

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

size_t SetLayerVisibilityCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject SetLayerVisibilityCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldVisible"), oldVisible_},
                       {QStringLiteral("newVisible"), newVisible_}};
}

bool SetLayerVisibilityCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldVisible_ = data.value(QStringLiteral("oldVisible")).toBool();
    newVisible_ = data.value(QStringLiteral("newVisible")).toBool();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- SetLayerLockCommand ---
SetLayerLockCommand::SetLayerLockCommand(ArtifactAbstractLayerPtr layer, bool locked)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldLocked_(layer ? layer->isLocked() : false), newLocked_(locked) {}

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

size_t SetLayerLockCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject SetLayerLockCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldLocked"), oldLocked_},
                       {QStringLiteral("newLocked"), newLocked_}};
}

bool SetLayerLockCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldLocked_ = data.value(QStringLiteral("oldLocked")).toBool();
    newLocked_ = data.value(QStringLiteral("newLocked")).toBool();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- SetLayerSoloCommand ---
SetLayerSoloCommand::SetLayerSoloCommand(ArtifactAbstractLayerPtr layer, bool solo)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldSolo_(layer ? layer->isSolo() : false), newSolo_(solo) {}

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

size_t SetLayerSoloCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject SetLayerSoloCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldSolo"), oldSolo_},
                       {QStringLiteral("newSolo"), newSolo_}};
}

bool SetLayerSoloCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldSolo_ = data.value(QStringLiteral("oldSolo")).toBool();
    newSolo_ = data.value(QStringLiteral("newSolo")).toBool();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- SetLayerShyCommand ---
SetLayerShyCommand::SetLayerShyCommand(ArtifactAbstractLayerPtr layer, bool shy)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldShy_(layer ? layer->isShy() : false), newShy_(shy) {}

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

size_t SetLayerShyCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject SetLayerShyCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldShy"), oldShy_},
                       {QStringLiteral("newShy"), newShy_}};
}

bool SetLayerShyCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldShy_ = data.value(QStringLiteral("oldShy")).toBool();
    newShy_ = data.value(QStringLiteral("newShy")).toBool();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- ChangeLayerBlendModeCommand ---
ChangeLayerBlendModeCommand::ChangeLayerBlendModeCommand(ArtifactAbstractLayerPtr layer, LAYER_BLEND_TYPE newMode)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldMode_(layer ? layer->layerBlendType() : LAYER_BLEND_TYPE::BLEND_NORMAL), newMode_(newMode) {}

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

size_t ChangeLayerBlendModeCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject ChangeLayerBlendModeCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldMode"), static_cast<int>(oldMode_)},
                       {QStringLiteral("newMode"), static_cast<int>(newMode_)}};
}

bool ChangeLayerBlendModeCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldMode_ = static_cast<LAYER_BLEND_TYPE>(data.value(QStringLiteral("oldMode")).toInt());
    newMode_ = static_cast<LAYER_BLEND_TYPE>(data.value(QStringLiteral("newMode")).toInt());
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
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

bool MacroUndoCommand::canSerialize() const {
    return std::all_of(children_.begin(), children_.end(),
        [](const auto& child) { return child && child->canSerialize() && !child->isOffloaded(); });
}

QJsonObject MacroUndoCommand::serialize() const {
    QJsonObject data;
    data.insert(QStringLiteral("label"), label_);
    QJsonArray children;
    for (const auto& child : children_) {
        if (!child || !child->canSerialize()) continue;
        QJsonObject entry;
        entry.insert(QStringLiteral("type"), child->commandType());
        entry.insert(QStringLiteral("data"), child->serialize());
        children.append(entry);
    }
    data.insert(QStringLiteral("children"), children);
    return data;
}

bool MacroUndoCommand::deserialize(const QJsonObject& data) {
    label_ = data.value(QStringLiteral("label")).toString();
    children_.clear();
    for (const auto& value : data.value(QStringLiteral("children")).toArray()) {
        const auto entry = value.toObject();
        auto child = UndoManager::instance()->createCommand(
            entry.value(QStringLiteral("type")).toString(),
            entry.value(QStringLiteral("data")).toObject());
        if (!child) return false;
        children_.push_back(std::move(child));
    }
    return true;
}

size_t MacroUndoCommand::estimatedMemoryBytes() const {
    size_t total = sizeof(*this);
    for (const auto& child : children_) {
        if (child) total += child->estimatedMemoryBytes();
    }
    return total;
}

// --- MoveAssetFileCommand ---
MoveAssetFileCommand::MoveAssetFileCommand(const QString& oldPath,
                                           const QString& newPath)
    : oldPath_(oldPath), newPath_(newPath) {}

void MoveAssetFileCommand::undo() {
    if (QFile::rename(newPath_, oldPath_)) {
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void MoveAssetFileCommand::redo() {
    if (QFile::rename(oldPath_, newPath_)) {
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString MoveAssetFileCommand::label() const {
    return QStringLiteral("Move Asset File");
}

size_t MoveAssetFileCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(oldPath_.size() + newPath_.size()) * sizeof(QChar);
}

QJsonObject MoveAssetFileCommand::serialize() const {
    QJsonObject data;
    data.insert(QStringLiteral("oldPath"), oldPath_);
    data.insert(QStringLiteral("newPath"), newPath_);
    return data;
}

bool MoveAssetFileCommand::deserialize(const QJsonObject& data) {
    oldPath_ = data.value(QStringLiteral("oldPath")).toString();
    newPath_ = data.value(QStringLiteral("newPath")).toString();
    return !oldPath_.isEmpty() && !newPath_.isEmpty();
}

// --- UndoManager ---
UndoManager::UndoManager(): impl_(new Impl()) {
    impl_->commandFactories_.insert(
        QStringLiteral("SetPropertyCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetPropertyCommand>(
                ArtifactAbstractEffectPtr{}, UniString(), QVariant(), QVariant());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MoveAssetFileCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MoveAssetFileCommand>(QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MacroUndoCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<MacroUndoCommand>(data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeLayerOpacityCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeLayerOpacityCommand>(
                ArtifactAbstractLayerPtr{}, 0.0f, 0.0f);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("RenameLayerCommand"),
        [](const QJsonObject&) {
            return std::make_unique<RenameLayerCommand>(ArtifactAbstractLayerPtr{}, QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerVisibilityCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetLayerVisibilityCommand>(ArtifactAbstractLayerPtr{}, false);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerLockCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetLayerLockCommand>(ArtifactAbstractLayerPtr{}, false);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerSoloCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetLayerSoloCommand>(ArtifactAbstractLayerPtr{}, false);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerShyCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetLayerShyCommand>(ArtifactAbstractLayerPtr{}, false);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeActiveVariantCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeActiveVariantCommand>(ArtifactAbstractLayerPtr{}, 0, 0);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("CreateVariantCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<CreateVariantCommand>(
                ArtifactAbstractLayerPtr{},
                data.value(QStringLiteral("name")).toString().toStdString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeLayerBlendModeCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeLayerBlendModeCommand>(
                ArtifactAbstractLayerPtr{}, LAYER_BLEND_TYPE::BLEND_NORMAL);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MoveLayerIndexCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MoveLayerIndexCommand>(
                ArtifactCompositionPtr{}, ArtifactAbstractLayerPtr{}, 0, 0);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetTextLayerTextCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetTextLayerTextCommand>(
                ArtifactAbstractLayerPtr{}, QString(), QString(),
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ReplaceLayerSourceCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ReplaceLayerSourceCommand>(
                ArtifactAbstractLayerPtr{}, QString(), QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerPropertyKeyframesCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetLayerPropertyKeyframesCommand>(
                ArtifactAbstractLayerPtr{}, QString(),
                std::vector<ArtifactCore::KeyFrame>{},
                std::vector<ArtifactCore::KeyFrame>{},
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeLayerMatteReferencesCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeLayerMatteReferencesCommand>(
                ArtifactAbstractLayerPtr{},
                std::vector<LayerMatteReference>{},
                std::vector<LayerMatteReference>{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MoveLayerCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MoveLayerCommand>(ArtifactAbstractLayerPtr{}, 0.0f, 0.0f, 0);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("AddLayerCommand"),
        [](const QJsonObject&) {
            return std::make_unique<AddLayerCommand>(ArtifactCompositionPtr{}, ArtifactAbstractLayerPtr{}, true);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("RemoveLayerCommand"),
        [](const QJsonObject&) {
            return std::make_unique<RemoveLayerCommand>(ArtifactCompositionPtr{}, ArtifactAbstractLayerPtr{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("AnimationLayerStackSnapshotCommand"),
        [](const QJsonObject&) {
            return std::make_unique<AnimationLayerStackSnapshotCommand>(
                ArtifactAbstractLayerPtr{}, QJsonObject{}, QJsonObject{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("AlignLayersUndoCommand"),
        [](const QJsonObject&) {
            return std::make_unique<AlignLayersUndoCommand>(
                std::vector<AlignLayerSnapshot>{}, QStringLiteral("Align Layers"));
        });
    impl_->commandFactories_.insert(
        QStringLiteral("InOutPointsSnapshotCommand"),
        [](const QJsonObject&) {
            return std::make_unique<InOutPointsSnapshotCommand>(
                nullptr, QJsonObject{}, QJsonObject{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MaskEditCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MaskEditCommand>(
                ArtifactAbstractLayerPtr{}, std::vector<LayerMask>{}, std::vector<LayerMask>{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetEffectMaskImagesCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetEffectMaskImagesCommand>(
                ArtifactAbstractEffectPtr{}, std::vector<SharedPtr<ImageF32x4_RGBA>>{},
                std::vector<SharedPtr<ImageF32x4_RGBA>>{},
                data.value(QStringLiteral("label")).toString());
        });
}

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
    if (cmd->estimatedMemoryBytes() > impl_->budget_.maxSingleEntryBytes) {
        // Do not execute a command that cannot be retained under the active
        // budget; callers can inspect the unchanged history and retry after
        // changing the policy.
        return;
    }
    // Execute immediately and record for undo
    cmd->redo();
    
    impl_->undoStack.push_back(std::move(cmd));
    impl_->redoStack.clear();
    impl_->version_++;
    impl_->enforceBudget();
    impl_->applyOffloadPolicy();
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
    impl_->cleanupOffloadFiles();
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
    impl_->budget_.maxEntryCount = std::max<size_t>(1, maxSize);
    impl_->enforceBudget();
    Q_EMIT historyChanged();
}

size_t UndoManager::maxHistorySize() const { return impl_->maxHistorySize_; }
void UndoManager::setBudget(const UndoBudget& budget) {
    impl_->budget_ = budget;
    impl_->budget_.maxEntryCount = std::max<size_t>(1, impl_->budget_.maxEntryCount);
    impl_->enforceBudget();
    Q_EMIT historyChanged();
}
const UndoManager::UndoBudget& UndoManager::budget() const { return impl_->budget_; }
size_t UndoManager::currentMemoryBytes() const { return impl_->stackBytes(impl_->undoStack); }
float UndoManager::memoryPressure() const {
    if (impl_->budget_.maxMemoryBytes == 0) return 1.0f;
    return std::min(1.0f, static_cast<float>(currentMemoryBytes()) /
                              static_cast<float>(impl_->budget_.maxMemoryBytes));
}

void UndoManager::setOffloadPolicy(const OffloadPolicy policy) {
    impl_->offloadPolicy_ = policy;
    impl_->applyOffloadPolicy();
}

UndoManager::OffloadPolicy UndoManager::offloadPolicy() const {
    return impl_->offloadPolicy_;
}

void UndoManager::setOffloadDirectory(const QString& path) {
    impl_->offloadDirectory_ = path;
    if (!path.isEmpty()) QDir().mkpath(path);
}

QString UndoManager::offloadDirectory() const {
    return impl_->offloadDirectory_;
}

bool UndoManager::saveSessionHistory(const QString& path) const {
    constexpr qsizetype kMaxHistoryFileBytes = 64ll * 1024ll * 1024ll;
    if (path.trimmed().isEmpty()) return false;
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("savedVersion"), static_cast<qint64>(impl_->savedVersion_));
    QJsonArray entries;
    for (const auto& command : impl_->undoStack) {
        if (!command || !command->canSerialize()) continue;
        QJsonObject entry;
        entry.insert(QStringLiteral("type"), command->commandType());
        entry.insert(QStringLiteral("label"), command->label());
        entry.insert(QStringLiteral("estimatedBytes"), static_cast<qint64>(command->estimatedMemoryBytes()));
        const QJsonObject data = command->serialize();
        if (data.isEmpty()) continue;
        entry.insert(QStringLiteral("data"), data);
        entries.append(entry);
    }
    root.insert(QStringLiteral("entries"), entries);
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) return false;
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (payload.size() > kMaxHistoryFileBytes) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(payload) == payload.size() && file.commit();
}

void UndoManager::registerCommandFactory(const QString& type, CommandFactory factory) {
    if (type.isEmpty() || !factory) return;
    impl_->commandFactories_[type] = std::move(factory);
}

std::unique_ptr<UndoCommand> UndoManager::createCommand(
    const QString& type, const QJsonObject& data) const {
    const auto factory = impl_->commandFactories_.find(type);
    if (factory == impl_->commandFactories_.end()) return nullptr;
    auto command = factory.value()(data);
    if (!command || !command->deserialize(data) || !command->canSerialize() ||
        command->estimatedMemoryBytes() > impl_->budget_.maxSingleEntryBytes) return nullptr;
    return command;
}

void UndoManager::setEffectResolver(EffectResolver resolver) {
    impl_->effectResolver_ = std::move(resolver);
}

ArtifactAbstractEffectPtr UndoManager::resolveEffect(const QString& effectId) const {
    return impl_->effectResolver_ ? impl_->effectResolver_(effectId) : ArtifactAbstractEffectPtr{};
}

void UndoManager::setLayerResolver(LayerResolver resolver) {
    impl_->layerResolver_ = std::move(resolver);
}

ArtifactAbstractLayerPtr UndoManager::resolveLayer(const QString& layerId) const {
    return impl_->layerResolver_ ? impl_->layerResolver_(layerId) : ArtifactAbstractLayerPtr{};
}

void UndoManager::setCompositionResolver(CompositionResolver resolver) {
    impl_->compositionResolver_ = std::move(resolver);
}

ArtifactCompositionPtr UndoManager::resolveComposition(const QString& compositionId) const {
    return impl_->compositionResolver_ ? impl_->compositionResolver_(compositionId) : ArtifactCompositionPtr{};
}

void UndoManager::setInOutPointsResolver(InOutPointsResolver resolver) {
    impl_->inOutPointsResolver_ = std::move(resolver);
}

ArtifactInOutPoints* UndoManager::resolveInOutPoints() const {
    return impl_->inOutPointsResolver_ ? impl_->inOutPointsResolver_() : nullptr;
}

bool UndoManager::loadSessionHistory(const QString& path) {
    constexpr qint64 kMaxHistoryFileBytes = 64ll * 1024ll * 1024ll;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    if (file.size() < 0 || file.size() > kMaxHistoryFileBytes) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1 ||
        !root.value(QStringLiteral("entries")).isArray()) return false;
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    if (entries.size() > static_cast<qsizetype>(impl_->budget_.maxEntryCount)) return false;
    std::vector<std::unique_ptr<UndoCommand>> restored;
    restored.reserve(entries.size());
    for (const auto& value : entries) {
        if (!value.isObject()) continue;
        const QJsonObject entry = value.toObject();
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString label = entry.value(QStringLiteral("label")).toString();
        if (type.isEmpty() || type.size() > 256 || label.size() > 4096 ||
            !entry.value(QStringLiteral("data")).isObject()) continue;
        auto command = createCommand(type, entry.value(QStringLiteral("data")).toObject());
        if (!command) continue;
        restored.push_back(std::move(command));
    }
    impl_->undoStack = std::move(restored);
    impl_->redoStack.clear();
    impl_->savedVersion_ = root.value(QStringLiteral("savedVersion")).toInteger();
    impl_->version_ = impl_->savedVersion_;
    impl_->enforceBudget();
    Q_EMIT historyChanged();
    return true;
}
bool UndoManager::hasUnsavedChanges() const { return impl_->version_ != impl_->savedVersion_; }
void UndoManager::markAsSaved() { impl_->savedVersion_ = impl_->version_; }
int64_t UndoManager::currentVersion() const { return impl_->version_; }

// --- MoveLayerIndexCommand ---
MoveLayerIndexCommand::MoveLayerIndexCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex)
    : comp_(comp), layer_(layer),
      compositionId_(comp ? comp->id().toString() : QString()),
      layerId_(layer ? layer->id().toQString() : QString()),
      oldIndex_(oldIndex), newIndex_(newIndex) {}

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

size_t MoveLayerIndexCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(compositionId_.size() + layerId_.size()) * sizeof(QChar);
}

QJsonObject MoveLayerIndexCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldIndex"), oldIndex_},
                       {QStringLiteral("newIndex"), newIndex_}};
}

bool MoveLayerIndexCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldIndex_ = data.value(QStringLiteral("oldIndex")).toInt();
    newIndex_ = data.value(QStringLiteral("newIndex")).toInt();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    comp_ = manager->resolveComposition(compositionId_);
    layer_ = manager->resolveLayer(layerId_);
    return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && !layer_.expired();
}

// --- RenameLayerCommand ---
RenameLayerCommand::RenameLayerCommand(ArtifactAbstractLayerPtr layer, const QString& oldName, const QString& newName)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldName_(oldName), newName_(newName) {}

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

size_t RenameLayerCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size() + oldName_.size() + newName_.size()) * sizeof(QChar);
}

QJsonObject RenameLayerCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldName"), oldName_},
                       {QStringLiteral("newName"), newName_}};
}

bool RenameLayerCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldName_ = data.value(QStringLiteral("oldName")).toString();
    newName_ = data.value(QStringLiteral("newName")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- ChangeLayerOpacityCommand ---
ChangeLayerOpacityCommand::ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, float oldOpacity, float newOpacity)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldOpacity_(oldOpacity), newOpacity_(newOpacity) {}

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

size_t ChangeLayerOpacityCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject ChangeLayerOpacityCommand::serialize() const {
    QJsonObject data;
    data.insert(QStringLiteral("layerId"), layerId_);
    data.insert(QStringLiteral("oldOpacity"), oldOpacity_);
    data.insert(QStringLiteral("newOpacity"), newOpacity_);
    return data;
}

bool ChangeLayerOpacityCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldOpacity_ = static_cast<float>(data.value(QStringLiteral("oldOpacity")).toDouble());
    newOpacity_ = static_cast<float>(data.value(QStringLiteral("newOpacity")).toDouble());
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- ChangeActiveVariantCommand ---
ChangeActiveVariantCommand::ChangeActiveVariantCommand(ArtifactAbstractLayerPtr layer, size_t oldIndex, size_t newIndex)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldIndex_(oldIndex), newIndex_(newIndex) {}

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

size_t ChangeActiveVariantCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject ChangeActiveVariantCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldIndex"), static_cast<qint64>(oldIndex_)},
                       {QStringLiteral("newIndex"), static_cast<qint64>(newIndex_)}};
}

bool ChangeActiveVariantCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldIndex_ = static_cast<size_t>(data.value(QStringLiteral("oldIndex")).toInteger());
    newIndex_ = static_cast<size_t>(data.value(QStringLiteral("newIndex")).toInteger());
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- CreateVariantCommand ---
CreateVariantCommand::CreateVariantCommand(ArtifactAbstractLayerPtr layer, const ArtifactCore::String& name)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      name_(name), index_(0) {}

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

size_t CreateVariantCommand::estimatedMemoryBytes() const {
        return sizeof(*this) + static_cast<size_t>(name_.length()) * sizeof(QChar);
}

QJsonObject CreateVariantCommand::serialize() const {
    return {
        {QStringLiteral("layerId"), layerId_},
        {QStringLiteral("name"), QString::fromUtf8(name_.data(),
                                                     static_cast<int>(name_.length()))},
        {QStringLiteral("index"), static_cast<qint64>(index_)},
        {QStringLiteral("label"), label()}
    };
}

bool CreateVariantCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    name_ = data.value(QStringLiteral("name")).toString().toStdString();
    const qint64 serializedIndex = data.value(QStringLiteral("index")).toInteger(-1);
    if (serializedIndex < 0) return false;
    index_ = static_cast<size_t>(serializedIndex);
    if (auto* manager = UndoManager::instance()) {
        layer_ = manager->resolveLayer(layerId_);
    }
    return !layerId_.isEmpty() && !layer_.expired();
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

size_t ChangeCompositionResolutionCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this);
    for (const auto& layer : beforeSnapshots_) {
        bytes += sizeof(layer) + static_cast<size_t>(layer.layerId.toQString().size()) * sizeof(QChar);
        bytes += layer.masks.size() * sizeof(LayerMask);
        for (const auto& property : layer.properties) {
            bytes += sizeof(property) + static_cast<size_t>(property.propertyPath.size()) * sizeof(QChar);
            bytes += property.keyframes.size() * sizeof(ArtifactCore::KeyFrame);
        }
    }
    return bytes;
}

// --- LayoutSnapshotCommand ---
LayoutSnapshotCommand::LayoutSnapshotCommand(QString label,
                                             QByteArray beforeState,
                                             QByteArray afterState,
                                             RestoreFn restoreFn)
    : label_(std::move(label)),
      beforeState_(std::move(beforeState)),
      afterState_(std::move(afterState)),
      restoreFn_(std::move(restoreFn)) {}

void LayoutSnapshotCommand::undo() {
    if (restoreFn_) {
        restoreFn_(beforeState_);
    }
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void LayoutSnapshotCommand::redo() {
    if (restoreFn_) {
        restoreFn_(afterState_);
    }
    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

QString LayoutSnapshotCommand::label() const {
    return label_.isEmpty() ? QStringLiteral("Layout Change") : label_;
}

size_t LayoutSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(beforeState_.size() + afterState_.size());
}

}
