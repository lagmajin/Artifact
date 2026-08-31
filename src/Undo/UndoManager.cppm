module;
#include <vector>
#include <cstddef>
#include <cstdint>
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
#include <QUuid>
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
import Artifact.Layer.Audio;
import Artifact.Layer.Text;
import Artifact.Layer.Clone;
import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Artifact.Composition.Abstract;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Project.PresetManager;
import Artifact.Undo.ProjectItemSupport;
import Artifact.Event.Types;
import Event.Bus;
import Animation.Transform3D;
import Time.Rational;
import Artifact.Layers.Selection.Manager;
import Audio.Modulation.Router;

namespace Artifact {

W_OBJECT_IMPL(UndoManager)

LayerMask decodeMask(const QJsonObject& object);
bool maskJsonStructureValid(const QJsonObject& object);

namespace {
constexpr qint64 kMaxUndoPayloadBytes = 64ll * 1024ll * 1024ll;

bool jsonInteger(const QJsonValue& value, qint64& result);

bool nonNegativeJsonInteger(const QJsonValue& value) {
    qint64 result = 0;
    return jsonInteger(value, result) && result >= 0;
}

bool jsonInteger(const QJsonValue& value, qint64& result) {
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    constexpr double kQInt64Min = -9223372036854775808.0;
    constexpr double kQInt64UpperExclusive = 9223372036854775808.0;
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < kQInt64Min || number >= kQInt64UpperExclusive) return false;
    result = value.toInteger();
    return true;
}

bool nonNegativeJsonInt(const QJsonValue& value, int& result) {
    qint64 number = 0;
    if (!jsonInteger(value, number) || number < 0 ||
        number > std::numeric_limits<int>::max()) return false;
    result = static_cast<int>(number);
    return true;
}

bool finiteJsonNumber(const QJsonObject& object, const QString& key,
                      float& value);

int64_t transformTimeScaleForLayer(const ArtifactAbstractLayerPtr& layer) {
    constexpr double kFallbackFps = 30.0;
    constexpr double kMinFps = 1.0;
    constexpr double kMaxFps = 10000.0;
    double fps = kFallbackFps;
    if (layer) {
        if (auto* composition = static_cast<ArtifactAbstractComposition*>(
                layer->composition())) {
            const double candidate = composition->frameRate().framerate();
            if (std::isfinite(candidate) && candidate > 0.0) {
                fps = std::clamp(candidate, kMinFps, kMaxFps);
            }
        }
    }
    return std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps)));
}

void notifyLayerTransformChanged(const ArtifactAbstractLayerPtr& layer) {
    if (!layer) {
        return;
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->addDirtyReason(LayerDirtyReason::TransformChanged);
    layer->changed();
    if (auto* composition = static_cast<ArtifactAbstractComposition*>(
            layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{composition->id().toString(),
                              layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }
}

void notifyLayerPropertyChanged(const ArtifactAbstractLayerPtr& layer,
                                const QString& propertyPath) {
    if (!layer) {
        return;
    }
    LayerDirtyFlag dirtyFlag = LayerDirtyFlag::Property;
    if (propertyPath.startsWith(QStringLiteral("transform."))) {
        dirtyFlag = LayerDirtyFlag::Transform;
    } else if (propertyPath.startsWith(QStringLiteral("mask."))) {
        dirtyFlag = LayerDirtyFlag::Mask;
    } else if (propertyPath.startsWith(QStringLiteral("source."))) {
        dirtyFlag = LayerDirtyFlag::Source;
    }
    layer->setDirty(dirtyFlag);
    layer->addDirtyReason(LayerDirtyReason::PropertyChanged);
    layer->changed();
    if (auto* composition = static_cast<ArtifactAbstractComposition*>(
            layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{composition->id().toString(),
                              layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }
}

bool applyEffectScalarSnapshot(const ArtifactAbstractEffectPtr& effect,
                               const UniString& propertyName,
                               const QVariant& value) {
    if (!effect) {
        return false;
    }
    const auto property = effect->editableProperty(propertyName.toQString());
    const auto previousValue = property ? property->getValue() : QVariant{};
    effect->setPropertyValue(propertyName, value);
    if (property && property->getValue() != value) {
        effect->setPropertyValue(propertyName, previousValue);
        return false;
    }
    return true;
}

bool applyAlignLayerSnapshots(const ArtifactCompositionPtr& composition,
                              const std::vector<AlignLayerSnapshot>& snapshots,
                              bool useAfter) {
    if (!composition || snapshots.empty()) {
        return false;
    }

    struct AppliedLayer {
        ArtifactAbstractLayerPtr layer;
        const AlignLayerSnapshot* snapshot = nullptr;
    };
    std::vector<AppliedLayer> applied;
    applied.reserve(snapshots.size());

    for (const auto& snapshot : snapshots) {
        if (snapshot.layerId.trimmed().isEmpty() ||
            !composition->layerById(LayerID(snapshot.layerId))) {
            return false;
        }
    }

    const auto setSnapshot = [](const ArtifactAbstractLayerPtr& layer,
                                const AlignLayerSnapshot& snapshot,
                                bool after) {
        const RationalTime time(0, transformTimeScaleForLayer(layer));
        layer->transform3D().setPosition(
            time, after ? snapshot.afterX : snapshot.beforeX,
            after ? snapshot.afterY : snapshot.beforeY);
        layer->transform3D().setScale(
            time, after ? snapshot.afterScaleX : snapshot.beforeScaleX,
            after ? snapshot.afterScaleY : snapshot.beforeScaleY);
    };
    const auto snapshotMatches = [](const ArtifactAbstractLayerPtr& layer,
                                    const AlignLayerSnapshot& snapshot,
                                    bool after) {
        constexpr float kTolerance = 0.0001f;
        const float expectedX = after ? snapshot.afterX : snapshot.beforeX;
        const float expectedY = after ? snapshot.afterY : snapshot.beforeY;
        const float expectedScaleX =
            after ? snapshot.afterScaleX : snapshot.beforeScaleX;
        const float expectedScaleY =
            after ? snapshot.afterScaleY : snapshot.beforeScaleY;
        const auto& transform = layer->transform3D();
        return std::abs(transform.positionX() - expectedX) <= kTolerance &&
               std::abs(transform.positionY() - expectedY) <= kTolerance &&
               std::abs(transform.scaleX() - expectedScaleX) <= kTolerance &&
               std::abs(transform.scaleY() - expectedScaleY) <= kTolerance;
    };

    for (const auto& snapshot : snapshots) {
        const auto layer = composition->layerById(LayerID(snapshot.layerId));
        applied.push_back({layer, &snapshot});
        setSnapshot(layer, snapshot, useAfter);
        if (!snapshotMatches(layer, snapshot, useAfter)) {
            for (const auto& appliedLayer : applied) {
                setSnapshot(appliedLayer.layer, *appliedLayer.snapshot, !useAfter);
                notifyLayerTransformChanged(appliedLayer.layer);
            }
            return false;
        }
    }

    for (const auto& appliedLayer : applied) {
        notifyLayerTransformChanged(appliedLayer.layer);
    }
    return true;
}

bool restoreLayerSelection(const ArtifactCompositionPtr& composition,
                           const QStringList& selectedLayerIds,
                           const QString& currentLayerId) {
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection || !composition ||
        selection->activeComposition() != composition) {
        return true;
    }

    QVector<ArtifactAbstractLayerPtr> layers;
    layers.reserve(selectedLayerIds.size());
    for (const auto& id : selectedLayerIds) {
        const auto layer = composition->layerById(LayerID(id));
        if (!layer) {
            return false;
        }
        layers.push_back(layer);
    }
    if (!currentLayerId.isEmpty() &&
        !composition->layerById(LayerID(currentLayerId))) {
        return false;
    }

    selection->clearSelection();
    bool selectedFirst = false;
    for (const auto& layer : layers) {
        if (!selectedFirst) {
            selection->selectLayer(layer);
            selectedFirst = true;
        } else {
            selection->addToSelection(layer);
        }
    }
    if (!currentLayerId.isEmpty()) {
        const auto current = composition->layerById(LayerID(currentLayerId));
        if (current && selection->isSelected(current)) {
            selection->addToSelection(current);
        }
    }

    const auto selected = selection->selectedLayersInOrder();
    if (selected.size() != selectedLayerIds.size()) {
        return false;
    }
    for (int index = 0; index < selected.size(); ++index) {
        if (!selected[index] ||
            selected[index]->id().toString() != selectedLayerIds[index]) {
            return false;
        }
    }
    const auto current = selection->currentLayer();
    return currentLayerId.isEmpty()
        ? !current
        : current && current->id().toString() == currentLayerId;
}

class OffloadedUndoCommand final : public UndoCommand {
public:
    using FactoryMap = QMap<QString, UndoManager::CommandFactory>;

    OffloadedUndoCommand(QString path, QString type, QString label,
                         FactoryMap* factories, size_t bytes)
        : path_(std::move(path)), type_(std::move(type)), label_(std::move(label)),
          factories_(factories), bytes_(bytes) {}

    ~OffloadedUndoCommand() override {
        if (!path_.isEmpty()) QFile::remove(path_);
    }

    void undo() override {
        lastOperationSucceeded_ = false;
        if (auto command = restore()) {
            command->undo();
            lastOperationSucceeded_ = command->lastOperationSucceeded();
        }
    }
    void redo() override {
        lastOperationSucceeded_ = false;
        if (auto command = restore()) {
            command->redo();
            lastOperationSucceeded_ = command->lastOperationSucceeded();
        }
    }
    bool lastOperationSucceeded() const override {
        return lastOperationSucceeded_;
    }
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
        if (!entry.value(QStringLiteral("type")).isString() ||
            !entry.value(QStringLiteral("label")).isString() ||
            !nonNegativeJsonInteger(entry.value(QStringLiteral("version"))) ||
            entry.value(QStringLiteral("version")).toInteger(-1) != 1 ||
            entry.value(QStringLiteral("type")).toString() != type_ ||
            entry.value(QStringLiteral("label")).toString() != label_ ||
            !nonNegativeJsonInteger(entry.value(QStringLiteral("estimatedBytes"))) ||
            entry.value(QStringLiteral("estimatedBytes")).toInteger(-1) !=
                static_cast<qint64>(bytes_)) return {};
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
        if (!entry.value(QStringLiteral("type")).isString() ||
            !entry.value(QStringLiteral("label")).isString() ||
            !nonNegativeJsonInteger(entry.value(QStringLiteral("version"))) ||
            entry.value(QStringLiteral("version")).toInteger(-1) != 1 ||
            entry.value(QStringLiteral("type")).toString() != type_ ||
            entry.value(QStringLiteral("label")).toString() != label_ ||
            !nonNegativeJsonInteger(entry.value(QStringLiteral("estimatedBytes"))) ||
            entry.value(QStringLiteral("estimatedBytes")).toInteger(-1) !=
                static_cast<qint64>(bytes_) ||
            !entry.value(QStringLiteral("data")).isObject()) return nullptr;
        const auto factory = factories_->find(type_);
        if (factory == factories_->end()) return nullptr;
        auto command = factory.value()(entry.value(QStringLiteral("data")).toObject());
        if (!command ||
            !command->deserialize(entry.value(QStringLiteral("data")).toObject()) ||
            command->commandType() != type_ || !command->canSerialize()) return nullptr;
        return command;
    }

    QString path_;
    QString type_;
    QString label_;
    FactoryMap* factories_ = nullptr;
    size_t bytes_ = 0;
    bool lastOperationSucceeded_ = true;
};
}

InOutPointsSnapshotCommand::InOutPointsSnapshotCommand(
    ArtifactInOutPoints* points, const QJsonObject& before,
    const QJsonObject& after)
    : points_(points), before_(before), after_(after) {}

void InOutPointsSnapshotCommand::undo() {
    lastOperationSucceeded_ = points_ && points_->fromJson(before_) &&
                              points_->toJson() == before_;
}

void InOutPointsSnapshotCommand::redo() {
    lastOperationSucceeded_ = points_ && points_->fromJson(after_) &&
                              points_->toJson() == after_;
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
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isObject() || !after.isObject()) return false;
    before_ = before.toObject();
    after_ = after.toObject();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    points_ = manager->resolveInOutPoints();
    return points_ != nullptr;
}

SetCompositionWorkAreaCommand::SetCompositionWorkAreaCommand(
    ArtifactCompositionPtr composition,
    qint64 beforeStart, qint64 beforeEnd,
    qint64 afterStart, qint64 afterEnd,
    std::function<void(const ArtifactCompositionPtr&, qint64, qint64)> sync)
    : composition_(composition), beforeStart_(beforeStart),
      beforeEnd_(beforeEnd), afterStart_(afterStart), afterEnd_(afterEnd),
      sync_(std::move(sync)) {}

bool SetCompositionWorkAreaCommand::apply(qint64 start, qint64 end) {
    auto composition = composition_.lock();
    if (!composition) {
        return false;
    }
    composition->setWorkAreaRange(FrameRange(start, end));
    const auto appliedRange = composition->workAreaRange();
    if (appliedRange.start() != start || appliedRange.end() != end) {
        return false;
    }
    if (sync_) {
        sync_(composition, start, end);
    }
    if (auto* manager = UndoManager::instance()) {
        manager->notifyAnythingChanged();
    }
    return true;
}

void SetCompositionWorkAreaCommand::undo() {
    lastOperationSucceeded_ = apply(beforeStart_, beforeEnd_);
}

void SetCompositionWorkAreaCommand::redo() {
    lastOperationSucceeded_ = apply(afterStart_, afterEnd_);
}

QString SetCompositionWorkAreaCommand::label() const {
    return QStringLiteral("Set Work Area");
}

size_t SetCompositionWorkAreaCommand::estimatedMemoryBytes() const {
    return sizeof(*this);
}

SetCompositionSettingsCommand::SetCompositionSettingsCommand(
    ArtifactCompositionPtr composition, QSize oldSize, float oldFrameRate,
    qint64 oldStart, qint64 oldEnd, FloatColor oldBackground,
    QSize newSize, float newFrameRate, qint64 newStart, qint64 newEnd,
    FloatColor newBackground)
    : composition_(composition),
      compositionId_(composition ? composition->id().toString() : QString()),
      oldSize_(oldSize), oldFrameRate_(oldFrameRate), oldStart_(oldStart),
      oldEnd_(oldEnd), oldBackground_(oldBackground), newSize_(newSize),
      newFrameRate_(newFrameRate), newStart_(newStart), newEnd_(newEnd),
      newBackground_(newBackground) {}

bool SetCompositionSettingsCommand::apply(
    QSize size, float frameRate, qint64 start, qint64 end,
    FloatColor background) {
    auto composition = composition_.lock();
    if (!composition || !size.isValid() || size.width() <= 0 ||
        size.height() <= 0 || !std::isfinite(frameRate) || frameRate <= 0.0f ||
        start > end) {
        return false;
    }
    composition->setCompositionSize(size);
    composition->setFrameRate(FrameRate(frameRate));
    composition->setFrameRange(FrameRange(FramePosition(start), FramePosition(end)));
    composition->setBackGroundColor(background);
    const auto appliedRange = composition->frameRange().normalized();
    const auto appliedColor = composition->backgroundColor();
    const bool ok = composition->settings().compositionSize() == size &&
                    std::abs(composition->frameRate().framerate() - frameRate) < 0.0001f &&
                    appliedRange.start() == start && appliedRange.end() == end &&
                    appliedColor.r() == background.r() &&
                    appliedColor.g() == background.g() &&
                    appliedColor.b() == background.b() &&
                    appliedColor.a() == background.a();
    if (!ok) return false;
    if (auto project = ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr()) {
        project->projectChanged();
    }
    if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    return true;
}

void SetCompositionSettingsCommand::undo() {
    lastOperationSucceeded_ = apply(oldSize_, oldFrameRate_, oldStart_, oldEnd_, oldBackground_);
}

void SetCompositionSettingsCommand::redo() {
    lastOperationSucceeded_ = apply(newSize_, newFrameRate_, newStart_, newEnd_, newBackground_);
}

QString SetCompositionSettingsCommand::label() const {
    return QStringLiteral("Set Composition Settings");
}

size_t SetCompositionSettingsCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(compositionId_.size()) * sizeof(QChar);
}

QJsonObject SetCompositionSettingsCommand::serialize() const {
    const auto color = [](const FloatColor& value) {
        return QJsonArray{value.r(), value.g(), value.b(), value.a()};
    };
    return QJsonObject{
        {QStringLiteral("compositionId"), compositionId_},
        {QStringLiteral("oldSize"), QJsonArray{oldSize_.width(), oldSize_.height()}},
        {QStringLiteral("oldFrameRate"), oldFrameRate_},
        {QStringLiteral("oldStart"), oldStart_}, {QStringLiteral("oldEnd"), oldEnd_},
        {QStringLiteral("oldBackground"), color(oldBackground_)},
        {QStringLiteral("newSize"), QJsonArray{newSize_.width(), newSize_.height()}},
        {QStringLiteral("newFrameRate"), newFrameRate_},
        {QStringLiteral("newStart"), newStart_}, {QStringLiteral("newEnd"), newEnd_},
        {QStringLiteral("newBackground"), color(newBackground_)}};
}

bool SetCompositionSettingsCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    const auto readSize = [](const QJsonValue& value) {
        const auto array = value.toArray();
        return QSize(array.value(0).toInt(), array.value(1).toInt());
    };
    const auto readColor = [](const QJsonValue& value) {
        const auto array = value.toArray();
        return FloatColor(static_cast<float>(array.value(0).toDouble()),
                          static_cast<float>(array.value(1).toDouble()),
                          static_cast<float>(array.value(2).toDouble()),
                          static_cast<float>(array.value(3).toDouble()));
    };
    oldSize_ = readSize(data.value(QStringLiteral("oldSize")));
    oldFrameRate_ = static_cast<float>(data.value(QStringLiteral("oldFrameRate")).toDouble());
    oldStart_ = data.value(QStringLiteral("oldStart")).toInteger();
    oldEnd_ = data.value(QStringLiteral("oldEnd")).toInteger();
    oldBackground_ = readColor(data.value(QStringLiteral("oldBackground")));
    newSize_ = readSize(data.value(QStringLiteral("newSize")));
    newFrameRate_ = static_cast<float>(data.value(QStringLiteral("newFrameRate")).toDouble());
    newStart_ = data.value(QStringLiteral("newStart")).toInteger();
    newEnd_ = data.value(QStringLiteral("newEnd")).toInteger();
    newBackground_ = readColor(data.value(QStringLiteral("newBackground")));
    auto* manager = UndoManager::instance();
    if (!manager || compositionId_.isEmpty()) return false;
    composition_ = manager->resolveComposition(CompositionID(compositionId_));
    return !composition_.expired();
}

SetCompositionResponsiveLayoutCommand::SetCompositionResponsiveLayoutCommand(
    ArtifactCompositionPtr composition, QJsonObject oldLayout,
    QJsonObject newLayout)
    : composition_(composition),
      compositionId_(composition ? composition->id().toString() : QString()),
      oldLayout_(std::move(oldLayout)), newLayout_(std::move(newLayout)) {}

bool SetCompositionResponsiveLayoutCommand::apply(const QJsonObject& layoutJson) {
    auto composition = composition_.lock();
    if (!composition || layoutJson.isEmpty()) return false;
    const auto layout = ResponsiveLayoutSet::fromJson(layoutJson);
    composition->setResponsiveLayout(layout);
    if (composition->responsiveLayout().toJson() != layout.toJson()) return false;
    if (auto project = ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr()) {
        project->projectChanged();
    }
    if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    return true;
}

void SetCompositionResponsiveLayoutCommand::undo() {
    lastOperationSucceeded_ = apply(oldLayout_);
}

void SetCompositionResponsiveLayoutCommand::redo() {
    lastOperationSucceeded_ = apply(newLayout_);
}

QString SetCompositionResponsiveLayoutCommand::label() const {
    return QStringLiteral("Edit Responsive Layout");
}

size_t SetCompositionResponsiveLayoutCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        compositionId_.size() * sizeof(QChar) +
        QJsonDocument(oldLayout_).toJson(QJsonDocument::Compact).size() +
        QJsonDocument(newLayout_).toJson(QJsonDocument::Compact).size());
}

QJsonObject SetCompositionResponsiveLayoutCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("oldLayout"), oldLayout_},
                       {QStringLiteral("newLayout"), newLayout_}};
}

bool SetCompositionResponsiveLayoutCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    oldLayout_ = data.value(QStringLiteral("oldLayout")).toObject();
    newLayout_ = data.value(QStringLiteral("newLayout")).toObject();
    auto* manager = UndoManager::instance();
    if (!manager || !canSerialize()) return false;
    composition_ = manager->resolveComposition(CompositionID(compositionId_));
    return !composition_.expired();
}

class UndoManager::Impl {
public:
    std::vector<std::unique_ptr<UndoCommand>> undoStack;
    std::vector<std::unique_ptr<UndoCommand>> redoStack;
    size_t maxHistorySize_ = 100;
    UndoManager::UndoBudget budget_;
    UndoManager::OffloadPolicy offloadPolicy_ = UndoManager::OffloadPolicy::Never;
    QString offloadDirectory_;
    const QString offloadSessionId_ =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QMap<QString, UndoManager::CommandFactory> commandFactories_;
    UndoManager::EffectResolver effectResolver_;
    UndoManager::LayerResolver layerResolver_;
    UndoManager::CompositionResolver compositionResolver_;
    UndoManager::InOutPointsResolver inOutPointsResolver_;
    std::vector<int64_t> undoStateIds_;
    std::vector<int64_t> redoStateIds_;
    int64_t nextStateId_ = 1;
    int64_t version_ = 0;
    int64_t savedVersion_ = 0;

    size_t stackBytes(const std::vector<std::unique_ptr<UndoCommand>>& stack) const {
        size_t total = 0;
        for (const auto& command : stack) {
            if (!command) continue;
            const size_t bytes = command->estimatedMemoryBytes();
            if (bytes > std::numeric_limits<size_t>::max() - total) {
                return std::numeric_limits<size_t>::max();
            }
            total += bytes;
        }
        return total;
    }

    size_t allStackBytes() const {
        const size_t undoBytes = stackBytes(undoStack);
        const size_t redoBytes = stackBytes(redoStack);
        if (redoBytes > std::numeric_limits<size_t>::max() - undoBytes) {
            return std::numeric_limits<size_t>::max();
        }
        return undoBytes + redoBytes;
    }

    int64_t allocateStateId() {
        const size_t maxAttempts = undoStateIds_.size() +
                                    redoStateIds_.size() + 4;
        for (size_t attempt = 0; attempt < maxAttempts; ++attempt) {
            if (nextStateId_ == std::numeric_limits<int64_t>::max()) {
                nextStateId_ = 1;
            }
            const int64_t candidate = nextStateId_++;
            if (candidate == savedVersion_ || candidate == version_ ||
                std::find(undoStateIds_.begin(), undoStateIds_.end(), candidate) !=
                    undoStateIds_.end() ||
                std::find(redoStateIds_.begin(), redoStateIds_.end(), candidate) !=
                    redoStateIds_.end()) {
                continue;
            }
            return candidate;
        }
        // The stacks are bounded, so this is only reachable if the state ID
        // space has been exhausted by corrupted or adversarial state.
        return 0;
    }

    void alignUndoStateIds() {
        while (undoStateIds_.size() < undoStack.size()) {
            undoStateIds_.push_back(allocateStateId());
        }
        if (undoStateIds_.size() > undoStack.size()) {
            undoStateIds_.resize(undoStack.size());
        }
    }

    void eraseUndoEntry(size_t index) {
        if (index >= undoStack.size()) {
            return;
        }
        undoStack.erase(undoStack.begin() + static_cast<std::ptrdiff_t>(index));
        if (index < undoStateIds_.size()) {
            undoStateIds_.erase(undoStateIds_.begin() +
                                static_cast<std::ptrdiff_t>(index));
        }
    }

    void eraseRedoEntry(size_t index) {
        if (index >= redoStack.size()) {
            return;
        }
        redoStack.erase(redoStack.begin() + static_cast<std::ptrdiff_t>(index));
        if (index < redoStateIds_.size()) {
            redoStateIds_.erase(redoStateIds_.begin() +
                                static_cast<std::ptrdiff_t>(index));
        }
    }

    void enforceBudget() {
        maxHistorySize_ = budget_.maxEntryCount;
        alignUndoStateIds();
        for (size_t index = 0; index < undoStack.size();) {
            const auto& command = undoStack[index];
            if (!command || (!command->isOffloaded() &&
                             command->estimatedMemoryBytes() >
                                 budget_.maxSingleEntryBytes)) {
                eraseUndoEntry(index);
            } else {
                ++index;
            }
        }
        while (redoStack.size() > budget_.maxEntryCount ||
               undoStack.size() > budget_.maxEntryCount - redoStack.size()) {
            if (!redoStack.empty()) {
                eraseRedoEntry(0);
            } else if (!undoStack.empty()) {
                eraseUndoEntry(0);
            } else {
                break;
            }
        }
        while (allStackBytes() > budget_.maxMemoryBytes) {
            if (!redoStack.empty()) {
                eraseRedoEntry(0);
            } else if (!undoStack.empty()) {
                eraseUndoEntry(0);
            } else {
                break;
            }
        }
    }

    bool offloadEntry(size_t index) {
        if (index >= undoStack.size() || offloadDirectory_.isEmpty()) return false;
        auto& command = undoStack[index];
        if (!command || !command->canSerialize() || command->isOffloaded() || command->commandType().isEmpty()) return false;
        const QString type = command->commandType();
        const QString label = command->label();
        if (type.size() > 256 || label.size() > 4096) return false;
        const size_t estimatedBytes = command->estimatedMemoryBytes();
        if (estimatedBytes > static_cast<size_t>(std::numeric_limits<qint64>::max())) {
            return false;
        }
        if (!QDir().mkpath(offloadDirectory_)) return false;
        const QString path = QDir(offloadDirectory_).filePath(
            QStringLiteral("undo_%1_%2.json")
                .arg(offloadSessionId_)
                .arg(static_cast<qulonglong>(index)));
        QJsonObject entry;
        entry.insert(QStringLiteral("version"), 1);
        entry.insert(QStringLiteral("type"), type);
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("estimatedBytes"), static_cast<qint64>(estimatedBytes));
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
            QStringList{QStringLiteral("undo_%1_*.json").arg(offloadSessionId_)},
            QDir::Files);
        for (const auto& file : files) directory.remove(file);
    }

    float memoryPressure() const {
        if (budget_.maxMemoryBytes == 0) return 1.0f;
        return std::min(1.0f, static_cast<float>(allStackBytes()) /
                                  static_cast<float>(budget_.maxMemoryBytes));
    }
};

// --- SetPropertyCommand ---
SetPropertyCommand::SetPropertyCommand(ArtifactAbstractEffectPtr target, const UniString& propName, const QVariant& oldValue, const QVariant& newValue)
    : target_(target), effectId_(target ? target->effectID().toQString() : QString()),
      name_(propName), oldValue_(oldValue), newValue_(newValue) {}

void SetPropertyCommand::undo() {
    auto t = target_.lock();
    lastOperationSucceeded_ = applyEffectScalarSnapshot(t, name_, oldValue_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(t->effectID().toQString());
        }
    }
}

void SetPropertyCommand::redo() {
    auto t = target_.lock();
    lastOperationSucceeded_ = applyEffectScalarSnapshot(t, name_, newValue_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(t->effectID().toQString());
        }
    }
}

QString SetPropertyCommand::label() const {
    return QStringLiteral("Set Property: %1").arg(name_.toQString());
}

EffectPresetSnapshotCommand::EffectPresetSnapshotCommand(
    ArtifactAbstractEffectPtr effect, QJsonObject before, QJsonObject after,
    QString label)
    : effect_(effect), effectId_(effect ? effect->effectID().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}

namespace {
bool applyEffectPresetSnapshot(const ArtifactAbstractEffectPtr& effect,
                               const QJsonObject& snapshot,
                               const QJsonObject& compensation) {
    if (!effect || snapshot.isEmpty()) return false;
    auto target = effect;
    if (!ArtifactPresetManager::applyPresetJsonToEffect(target, snapshot)) {
        return false;
    }
    if (ArtifactPresetManager::effectToPresetJson(effect) == snapshot) {
        return true;
    }
    if (!compensation.isEmpty()) {
        auto restoreTarget = effect;
        ArtifactPresetManager::applyPresetJsonToEffect(restoreTarget, compensation);
    }
    return false;
}
}

void EffectPresetSnapshotCommand::undo() {
    lastOperationSucceeded_ = applyEffectPresetSnapshot(
        effect_.lock(), before_, after_);
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyPropertyChanged(effectId_);
    }
}

void EffectPresetSnapshotCommand::redo() {
    if (firstRedo_) {
        firstRedo_ = false;
        lastOperationSucceeded_ = true;
        return;
    }
    lastOperationSucceeded_ = applyEffectPresetSnapshot(
        effect_.lock(), after_, before_);
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyPropertyChanged(effectId_);
    }
}

QString EffectPresetSnapshotCommand::label() const { return label_; }

size_t EffectPresetSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        QJsonDocument(before_).toJson(QJsonDocument::Compact).size() +
        QJsonDocument(after_).toJson(QJsonDocument::Compact).size() +
        label_.size() * static_cast<int>(sizeof(QChar)));
}

bool EffectPresetSnapshotCommand::canSerialize() const {
    return !effectId_.isEmpty() && !effect_.expired() &&
           !before_.isEmpty() && !after_.isEmpty() && !label_.isEmpty();
}

QJsonObject EffectPresetSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("effectId"), effectId_},
                       {QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_},
                       {QStringLiteral("label"), label_}};
}

bool EffectPresetSnapshotCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    before_ = data.value(QStringLiteral("before")).toObject();
    after_ = data.value(QStringLiteral("after")).toObject();
    label_ = data.value(QStringLiteral("label")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    effect_ = manager->resolveEffect(effectId_);
    firstRedo_ = false;
    return canSerialize();
}

AnimationLayerStackSnapshotCommand::AnimationLayerStackSnapshotCommand(
    ArtifactAbstractLayerPtr layer, const QJsonObject& before,
    const QJsonObject& after)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()), before_(before), after_(after) {}

void AnimationLayerStackSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        layer->restoreAnimationLayersSnapshot(before_);
        layer->changed();
        lastOperationSucceeded_ = layer->animationLayersSnapshot() == before_;
        if (!lastOperationSucceeded_) {
            layer->restoreAnimationLayersSnapshot(after_);
            layer->changed();
        }
    }
}

void AnimationLayerStackSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        layer->restoreAnimationLayersSnapshot(after_);
        layer->changed();
        lastOperationSucceeded_ = layer->animationLayersSnapshot() == after_;
        if (!lastOperationSucceeded_) {
            layer->restoreAnimationLayersSnapshot(before_);
            layer->changed();
        }
    }
}

QString AnimationLayerStackSnapshotCommand::label() const {
    return QStringLiteral("Change Animation Layers");
}

ClonerTransformStackSnapshotCommand::ClonerTransformStackSnapshotCommand(
    ArtifactAbstractLayerPtr layer, QJsonArray before, QJsonArray after)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)) {}

void ClonerTransformStackSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->restoreClonerTransformsSnapshot(before_);
        if (!lastOperationSucceeded_) {
            layer->restoreClonerTransformsSnapshot(after_);
        }
    }
}

void ClonerTransformStackSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->restoreClonerTransformsSnapshot(after_);
        if (!lastOperationSucceeded_) {
            layer->restoreClonerTransformsSnapshot(before_);
        }
    }
}

size_t ClonerTransformStackSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        QJsonDocument(before_).toJson(QJsonDocument::Compact).size() +
        QJsonDocument(after_).toJson(QJsonDocument::Compact).size());
}

QJsonObject ClonerTransformStackSnapshotCommand::serialize() const {
    return QJsonObject{
        {QStringLiteral("layerId"), layerId_},
        {QStringLiteral("before"), before_},
        {QStringLiteral("after"), after_}};
}

bool ClonerTransformStackSnapshotCommand::deserialize(
    const QJsonObject &data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (layerId_.isEmpty() || !before.isArray() || !after.isArray()) {
        return false;
    }
    before_ = before.toArray();
    after_ = after.toArray();
    auto *manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layer_.expired();
}

LayerComponentDescriptorSnapshotCommand::LayerComponentDescriptorSnapshotCommand(
    ArtifactAbstractLayerPtr layer, QJsonObject before, QJsonObject after)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)) {}

void LayerComponentDescriptorSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->restoreComponentDescriptorSnapshot(before_);
        if (!lastOperationSucceeded_) {
            layer->restoreComponentDescriptorSnapshot(after_);
        }
    }
}

void LayerComponentDescriptorSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->restoreComponentDescriptorSnapshot(after_);
        if (!lastOperationSucceeded_) {
            layer->restoreComponentDescriptorSnapshot(before_);
        }
    }
}

size_t LayerComponentDescriptorSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        QJsonDocument(before_).toJson(QJsonDocument::Compact).size() +
        QJsonDocument(after_).toJson(QJsonDocument::Compact).size());
}

QJsonObject LayerComponentDescriptorSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_}};
}

bool LayerComponentDescriptorSnapshotCommand::deserialize(
    const QJsonObject &data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (layerId_.isEmpty() || !before.isObject() || !after.isObject()) {
        return false;
    }
    before_ = before.toObject();
    after_ = after.toObject();
    auto *manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layer_.expired();
}

CloneEffectorStackSnapshotCommand::CloneEffectorStackSnapshotCommand(
    ArtifactAbstractLayerPtr layer, QJsonArray before, QJsonArray after)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)) {}

void CloneEffectorStackSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        if (auto cloneLayer = dynamic_cast<ArtifactCloneLayer*>(layer.get())) {
            lastOperationSucceeded_ =
                cloneLayer->restoreEffectorStackSnapshot(before_);
            if (!lastOperationSucceeded_) {
                cloneLayer->restoreEffectorStackSnapshot(after_);
            }
        }
    }
}

void CloneEffectorStackSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        if (auto cloneLayer = dynamic_cast<ArtifactCloneLayer*>(layer.get())) {
            lastOperationSucceeded_ =
                cloneLayer->restoreEffectorStackSnapshot(after_);
            if (!lastOperationSucceeded_) {
                cloneLayer->restoreEffectorStackSnapshot(before_);
            }
        }
    }
}

size_t CloneEffectorStackSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        QJsonDocument(before_).toJson(QJsonDocument::Compact).size() +
        QJsonDocument(after_).toJson(QJsonDocument::Compact).size());
}

QJsonObject CloneEffectorStackSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_}};
}

bool CloneEffectorStackSnapshotCommand::deserialize(
    const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (layerId_.isEmpty() || !before.isArray() || !after.isArray()) {
        return false;
    }
    before_ = before.toArray();
    after_ = after.toArray();
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    if (layer_.expired()) {
        return false;
    }
    return static_cast<bool>(dynamic_cast<ArtifactCloneLayer*>(layer_.lock().get()));
}

namespace {
size_t modulationSnapshotBytes(
    const Audio::Modulation::ModulationRouterSnapshot& snapshot) {
    size_t bytes = sizeof(snapshot);
    for (const auto& source : snapshot.sources) bytes += sizeof(source);
    for (const auto& assignment : snapshot.assignments) {
        bytes += sizeof(assignment) + assignment.targetPath.size();
    }
    return bytes;
}

bool finiteJsonNumber(const QJsonObject& object, const QString& key, float& value) {
    const auto jsonValue = object.value(key);
    if (!jsonValue.isDouble()) return false;
    value = static_cast<float>(jsonValue.toDouble());
    return std::isfinite(value);
}

bool jsonUInt32(const QJsonValue& jsonValue, std::uint32_t& value, bool allowZero = false) {
    if (!jsonValue.isDouble()) return false;
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || number < (allowZero ? 0.0 : 1.0) ||
        number > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        std::floor(number) != number) {
        return false;
    }
    value = static_cast<std::uint32_t>(number);
    return true;
}

bool jsonEnumInt(const QJsonValue& jsonValue, int& value) {
    if (!jsonValue.isDouble()) return false;
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    value = static_cast<int>(number);
    return true;
}

QJsonObject encodeModulationRouterSnapshot(
    const Audio::Modulation::ModulationRouterSnapshot& snapshot) {
    QJsonArray sources;
    for (const auto& source : snapshot.sources) {
        sources.append(QJsonObject{
            {QStringLiteral("id"), static_cast<qint64>(source.id)},
            {QStringLiteral("type"), static_cast<int>(source.type)},
            {QStringLiteral("waveform"), static_cast<int>(source.waveform)},
            {QStringLiteral("frequency"), static_cast<double>(source.frequency)},
            {QStringLiteral("phaseOffset"), static_cast<double>(source.phaseOffset)},
            {QStringLiteral("pulseWidth"), static_cast<double>(source.pulseWidth)},
            {QStringLiteral("attack"), static_cast<double>(source.attack)},
            {QStringLiteral("decay"), static_cast<double>(source.decay)},
            {QStringLiteral("sustain"), static_cast<double>(source.sustain)},
            {QStringLiteral("release"), static_cast<double>(source.release)},
            {QStringLiteral("rate"), static_cast<double>(source.rate)},
            {QStringLiteral("smoothing"), static_cast<double>(source.smoothing)},
            {QStringLiteral("seed"), static_cast<qint64>(source.seed)},
            {QStringLiteral("macroValue"), static_cast<double>(source.macroValue)},
            {QStringLiteral("unipolar"), source.unipolar}});
    }

    QJsonArray assignments;
    for (const auto& assignment : snapshot.assignments) {
        assignments.append(QJsonObject{
            {QStringLiteral("sourceId"), static_cast<qint64>(assignment.sourceId)},
            {QStringLiteral("targetId"), QString::number(static_cast<qulonglong>(assignment.targetId))},
            {QStringLiteral("targetPath"), QString::fromStdString(assignment.targetPath)},
            {QStringLiteral("depth"), static_cast<double>(assignment.depth)},
            {QStringLiteral("enabled"), assignment.enabled},
            {QStringLiteral("mode"), static_cast<int>(assignment.mode)}});
    }

    return QJsonObject{{QStringLiteral("sources"), sources},
                       {QStringLiteral("assignments"), assignments},
                       {QStringLiteral("smoothingTime"),
                        static_cast<double>(snapshot.smoothingTime)}};
}

bool decodeModulationRouterSnapshot(
    const QJsonObject& object,
    Audio::Modulation::ModulationRouterSnapshot& snapshot) {
    const auto sourcesValue = object.value(QStringLiteral("sources"));
    const auto assignmentsValue = object.value(QStringLiteral("assignments"));
    if (!sourcesValue.isArray() || !assignmentsValue.isArray() ||
        !finiteJsonNumber(object, QStringLiteral("smoothingTime"), snapshot.smoothingTime) ||
        snapshot.smoothingTime < 0.0f) {
        return false;
    }

    using Source = Audio::Modulation::ModulationSourceDefinition;
    using SourceType = Audio::Modulation::ModulatorSourceType;
    using Waveform = Audio::Modulation::LfoWaveform;
    using Assignment = Audio::Modulation::ModulationAssignment;
    using MixMode = Audio::Modulation::ModulationMixMode;

    snapshot.sources.clear();
    snapshot.assignments.clear();
    std::set<std::uint32_t> sourceIds;
    for (const auto& value : sourcesValue.toArray()) {
        if (!value.isObject()) return false;
        const auto sourceObject = value.toObject();
        const auto idValue = sourceObject.value(QStringLiteral("id"));
        const auto typeValue = sourceObject.value(QStringLiteral("type"));
        const auto waveformValue = sourceObject.value(QStringLiteral("waveform"));
        const auto seedValue = sourceObject.value(QStringLiteral("seed"));
        std::uint32_t id = 0;
        std::uint32_t seed = 0;
        int type = -1;
        int waveform = -1;
        if (!jsonUInt32(idValue, id) || !jsonEnumInt(typeValue, type) ||
            !jsonEnumInt(waveformValue, waveform) || !jsonUInt32(seedValue, seed, true)) {
            return false;
        }
        if (!sourceIds.insert(id).second ||
            type < 0 || type > static_cast<int>(SourceType::Macro) ||
            waveform < 0 || waveform > static_cast<int>(Waveform::SampleAndHold)) {
            return false;
        }

        Source source;
        source.id = id;
        source.type = static_cast<SourceType>(type);
        source.waveform = static_cast<Waveform>(waveform);
        source.seed = seed;
        if (!finiteJsonNumber(sourceObject, QStringLiteral("frequency"), source.frequency) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("phaseOffset"), source.phaseOffset) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("pulseWidth"), source.pulseWidth) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("attack"), source.attack) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("decay"), source.decay) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("sustain"), source.sustain) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("release"), source.release) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("rate"), source.rate) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("smoothing"), source.smoothing) ||
            !finiteJsonNumber(sourceObject, QStringLiteral("macroValue"), source.macroValue) ||
            !sourceObject.value(QStringLiteral("unipolar")).isBool()) {
            return false;
        }
        source.unipolar = sourceObject.value(QStringLiteral("unipolar")).toBool();
        snapshot.sources.push_back(source);
    }

    for (const auto& value : assignmentsValue.toArray()) {
        if (!value.isObject()) return false;
        const auto assignmentObject = value.toObject();
        const auto sourceIdValue = assignmentObject.value(QStringLiteral("sourceId"));
        const auto targetIdValue = assignmentObject.value(QStringLiteral("targetId"));
        const auto targetPathValue = assignmentObject.value(QStringLiteral("targetPath"));
        const auto modeValue = assignmentObject.value(QStringLiteral("mode"));
        std::uint32_t sourceId = 0;
        int mode = -1;
        float depth = 0.0f;
        if (!jsonUInt32(sourceIdValue, sourceId) || !targetPathValue.isString() ||
            !jsonEnumInt(modeValue, mode) || !assignmentObject.value(QStringLiteral("enabled")).isBool() ||
            !finiteJsonNumber(assignmentObject, QStringLiteral("depth"), depth)) {
            return false;
        }
        bool targetIdOk = false;
        qulonglong targetId = 0;
        if (targetIdValue.isString()) {
            targetId = targetIdValue.toString().toULongLong(&targetIdOk);
        }
        const auto targetPath = targetPathValue.toString();
        if (sourceIds.find(sourceId) == sourceIds.end() ||
            !targetIdOk || targetId == 0 || targetPath.isEmpty() ||
            mode < 0 || mode > static_cast<int>(MixMode::Multiply)) {
            return false;
        }
        Assignment assignment;
        assignment.sourceId = sourceId;
        assignment.targetId = static_cast<std::uint64_t>(targetId);
        assignment.targetPath = targetPath.toStdString();
        assignment.depth = depth;
        assignment.enabled = assignmentObject.value(QStringLiteral("enabled")).toBool();
        assignment.mode = static_cast<MixMode>(mode);
        snapshot.assignments.push_back(std::move(assignment));
    }
    return true;
}

bool modulationSnapshotSerializable(
    const Audio::Modulation::ModulationRouterSnapshot& snapshot) {
    if (!std::isfinite(snapshot.smoothingTime) || snapshot.smoothingTime < 0.0f) {
        return false;
    }
    std::set<std::uint32_t> sourceIds;
    for (const auto& source : snapshot.sources) {
        if (source.id == 0 || !sourceIds.insert(source.id).second ||
            static_cast<int>(source.type) < 0 ||
            static_cast<int>(source.type) > static_cast<int>(Audio::Modulation::ModulatorSourceType::Macro) ||
            static_cast<int>(source.waveform) < 0 ||
            static_cast<int>(source.waveform) > static_cast<int>(Audio::Modulation::LfoWaveform::SampleAndHold) ||
            !std::isfinite(source.frequency) || !std::isfinite(source.phaseOffset) ||
            !std::isfinite(source.pulseWidth) || !std::isfinite(source.attack) ||
            !std::isfinite(source.decay) || !std::isfinite(source.sustain) ||
            !std::isfinite(source.release) || !std::isfinite(source.rate) ||
            !std::isfinite(source.smoothing) || !std::isfinite(source.macroValue)) {
            return false;
        }
    }
    for (const auto& assignment : snapshot.assignments) {
        if (assignment.sourceId == 0 ||
            sourceIds.find(assignment.sourceId) == sourceIds.end() ||
            assignment.targetId == 0 || assignment.targetPath.empty() ||
            static_cast<int>(assignment.mode) < 0 ||
            static_cast<int>(assignment.mode) > static_cast<int>(Audio::Modulation::ModulationMixMode::Multiply) ||
            !std::isfinite(assignment.depth)) {
            return false;
        }
    }
    return true;
}

bool applyModulationSnapshot(
    Audio::Modulation::ModulationRouter& router,
    const Audio::Modulation::ModulationRouterSnapshot& target,
    const Audio::Modulation::ModulationRouterSnapshot& compensation) {
    router.restoreSnapshot(target);
    if (encodeModulationRouterSnapshot(router.snapshot()) ==
        encodeModulationRouterSnapshot(target)) {
        return true;
    }
    router.restoreSnapshot(compensation);
    return false;
}
}

EffectModulationSnapshotCommand::EffectModulationSnapshotCommand(
    ArtifactAbstractEffectPtr effect,
    Audio::Modulation::ModulationRouterSnapshot before,
    Audio::Modulation::ModulationRouterSnapshot after, QString label)
    : effect_(effect), effectId_(effect ? effect->effectID().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)),
      label_(std::move(label)) {}

void EffectModulationSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto effect = effect_.lock()) {
        lastOperationSucceeded_ = applyModulationSnapshot(
            effect->modulationRouter(), before_, after_);
    }
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyAnythingChanged();
    }
}

void EffectModulationSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto effect = effect_.lock()) {
        lastOperationSucceeded_ = applyModulationSnapshot(
            effect->modulationRouter(), after_, before_);
    }
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyAnythingChanged();
    }
}

QString EffectModulationSnapshotCommand::label() const { return label_; }

size_t EffectModulationSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + modulationSnapshotBytes(before_) +
           modulationSnapshotBytes(after_) + static_cast<size_t>(label_.size()) * sizeof(QChar);
}

bool EffectModulationSnapshotCommand::canSerialize() const {
    return !effectId_.isEmpty() && !effect_.expired() &&
           modulationSnapshotSerializable(before_) &&
           modulationSnapshotSerializable(after_);
}

QJsonObject EffectModulationSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("effectId"), effectId_},
                       {QStringLiteral("before"), encodeModulationRouterSnapshot(before_)},
                       {QStringLiteral("after"), encodeModulationRouterSnapshot(after_)},
                       {QStringLiteral("label"), label_}};
}

bool EffectModulationSnapshotCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    if (!decodeModulationRouterSnapshot(data.value(QStringLiteral("before")).toObject(), before_) ||
        !decodeModulationRouterSnapshot(data.value(QStringLiteral("after")).toObject(), after_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    effect_ = manager->resolveEffect(effectId_);
    return canSerialize();
}

LayerModulationSnapshotCommand::LayerModulationSnapshotCommand(
    ArtifactAbstractLayerPtr layer,
    Audio::Modulation::ModulationRouterSnapshot before,
    Audio::Modulation::ModulationRouterSnapshot after, QString label)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      before_(std::move(before)), after_(std::move(after)),
      label_(std::move(label)) {}

void LayerModulationSnapshotCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ = applyModulationSnapshot(
            layer->modulationRouter(), before_, after_);
    }
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyAnythingChanged();
    }
}

void LayerModulationSnapshotCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ = applyModulationSnapshot(
            layer->modulationRouter(), after_, before_);
    }
    if (lastOperationSucceeded_) {
        if (auto manager = UndoManager::instance()) manager->notifyAnythingChanged();
    }
}

QString LayerModulationSnapshotCommand::label() const { return label_; }

size_t LayerModulationSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + modulationSnapshotBytes(before_) +
           modulationSnapshotBytes(after_) + static_cast<size_t>(label_.size()) * sizeof(QChar);
}

bool LayerModulationSnapshotCommand::canSerialize() const {
    return !layerId_.isEmpty() && !layer_.expired() &&
           modulationSnapshotSerializable(before_) &&
           modulationSnapshotSerializable(after_);
}

QJsonObject LayerModulationSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), encodeModulationRouterSnapshot(before_)},
                       {QStringLiteral("after"), encodeModulationRouterSnapshot(after_)},
                       {QStringLiteral("label"), label_}};
}

bool LayerModulationSnapshotCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    if (!decodeModulationRouterSnapshot(data.value(QStringLiteral("before")).toObject(), before_) ||
        !decodeModulationRouterSnapshot(data.value(QStringLiteral("after")).toObject(), after_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return canSerialize();
}

QJsonObject AnimationLayerStackSnapshotCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), before_},
                       {QStringLiteral("after"), after_}};
}

bool AnimationLayerStackSnapshotCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isObject() || !after.isObject()) return false;
    before_ = before.toObject();
    after_ = after.toObject();
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
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->setLayerPropertyValue(propertyPath_, oldSourcePath_);
        if (lastOperationSucceeded_) {
            if (auto* mgr = UndoManager::instance()) {
                mgr->notifyAnythingChanged();
            }
        }
    }
}

void ReplaceLayerSourceCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto layer = layer_.lock()) {
        lastOperationSucceeded_ =
            layer->setLayerPropertyValue(propertyPath_, newSourcePath_);
        if (lastOperationSucceeded_) {
            if (auto* mgr = UndoManager::instance()) {
                mgr->notifyAnythingChanged();
            }
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
    std::function<bool()> localize, std::function<bool()> relinkShared,
    QString label)
    : localize_(localize), relinkShared_(relinkShared), label_(label) {}

void ToggleLocalizedSourceCommand::undo() {
    lastOperationSucceeded_ = relinkShared_ && relinkShared_();
    if (lastOperationSucceeded_) {
        if (auto* mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void ToggleLocalizedSourceCommand::redo() {
    lastOperationSucceeded_ = localize_ && localize_();
    if (lastOperationSucceeded_) {
        if (auto* mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
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
    lastOperationSucceeded_ = static_cast<bool>(l);
    if (l) {
        auto& t3 = l->transform3D();
        const ArtifactCore::RationalTime t0(
            frame_, transformTimeScaleForLayer(l));
        t3.setPosition(t0, t3.positionX() - dx_, t3.positionY() - dy_);
        notifyLayerTransformChanged(l);
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void MoveLayerCommand::redo() {
    auto l = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(l);
    if (l) {
        auto& t3 = l->transform3D();
        const ArtifactCore::RationalTime t0(
            frame_, transformTimeScaleForLayer(l));
        t3.setPosition(t0, t3.positionX() + dx_, t3.positionY() + dy_);
        notifyLayerTransformChanged(l);
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    qint64 frame = 0;
    if (!finiteJsonNumber(data, QStringLiteral("dx"), dx_) ||
        !finiteJsonNumber(data, QStringLiteral("dy"), dy_) ||
        !jsonInteger(data.value(QStringLiteral("frame")), frame)) return false;
    frame_ = frame;
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- AddLayerCommand ---
namespace {
bool matteReferencesEqual(const std::vector<LayerMatteReference>& actual,
                          const std::vector<LayerMatteReference>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (size_t index = 0; index < actual.size(); ++index) {
        if (actual[index].toJson() != expected[index].toJson()) {
            return false;
        }
    }
    return true;
}

struct LayerRelationshipSnapshot {
    ArtifactAbstractLayerPtr layer;
    std::vector<LayerMatteReference> matteReferences;
    LayerID parentId;
};

using MatteRelationshipList = std::vector<
    std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMatteReference>>>;
using ParentRelationshipList = std::vector<
    std::pair<ArtifactAbstractLayerPtr, LayerID>>;

std::vector<LayerRelationshipSnapshot> captureLayerRelationships(
    const MatteRelationshipList& matteRelationships,
    const ParentRelationshipList& parentRelationships) {
    std::vector<LayerRelationshipSnapshot> snapshots;
    snapshots.reserve(matteRelationships.size() + parentRelationships.size());
    for (const auto& [layer, refs] : matteRelationships) {
        if (layer) {
            snapshots.push_back(
                {layer, layer->matteReferences(), layer->parentLayerId()});
        }
    }
    for (const auto& [layer, parentId] : parentRelationships) {
        if (layer) {
            snapshots.push_back(
                {layer, layer->matteReferences(), layer->parentLayerId()});
        }
    }
    return snapshots;
}

bool restoreLayerRelationships(
    const std::vector<LayerRelationshipSnapshot>& snapshots) {
    bool restored = true;
    for (const auto& snapshot : snapshots) {
        if (!snapshot.layer) {
            restored = false;
            continue;
        }
        snapshot.layer->setMatteReferences(snapshot.matteReferences);
        snapshot.layer->changed();
        restored = matteReferencesEqual(
            snapshot.layer->matteReferences(), snapshot.matteReferences) && restored;
        snapshot.layer->setParentById(snapshot.parentId);
        restored = snapshot.layer->parentLayerId() == snapshot.parentId && restored;
    }
    return restored;
}
}

AddLayerCommand::AddLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer, bool atTop)
    : comp_(comp), layer_(layer),
      compositionId_(comp ? comp->id().toString() : QString()),
      layerId_(layer ? layer->id().toQString() : QString()),
      atTop_(atTop), savedIndex_(-1) {
    if (!comp || !layer) {
        return;
    }
    for (const auto& candidate : comp->allLayer()) {
        if (!candidate || candidate->id() == layer->id()) {
            continue;
        }
        if (candidate->parentLayerId() == layer->id()) {
            removedParentReferences_.emplace_back(candidate,
                                                  candidate->parentLayerId());
        }
        const auto refs = candidate->matteReferences();
        const auto hasReference = std::any_of(
            refs.cbegin(), refs.cend(), [&layer](const LayerMatteReference& ref) {
                return ref.sourceLayerId == layer->id();
            });
        if (hasReference) {
            removedMatteReferences_.emplace_back(candidate, refs);
        }
    }
}

void AddLayerCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    lastOperationSucceeded_ = false;
    if (comp && layer && comp->containsLayerById(layer->id())) {
        const auto relationshipBefore = captureLayerRelationships(
            removedMatteReferences_, removedParentReferences_);
        int currentIndex = -1;
        const auto currentLayers = comp->allLayer();
        for (int index = 0; index < currentLayers.size(); ++index) {
            if (currentLayers[index] && currentLayers[index]->id() == layer->id()) {
                currentIndex = index;
                break;
            }
        }
        if (currentIndex < 0) return;
        comp->removeLayer(layer->id());
        if (comp->containsLayerById(layer->id())) return;
        bool referencesRestored = true;
        for (const auto& [dependentLayer, refs] : removedMatteReferences_) {
            if (!dependentLayer || !comp->containsLayerById(dependentLayer->id())) {
                referencesRestored = false;
                continue;
            }
            dependentLayer->setMatteReferences(refs);
            dependentLayer->changed();
            referencesRestored = matteReferencesEqual(
                dependentLayer->matteReferences(), refs) && referencesRestored;
        }
        lastOperationSucceeded_ = referencesRestored;
        if (!lastOperationSucceeded_) {
            comp->insertLayerAt(layer, currentIndex);
            restoreLayerRelationships(relationshipBefore);
            return;
        }
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void AddLayerCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    lastOperationSucceeded_ = false;
    if (comp && layer && !comp->containsLayerById(layer->id())) {
        const auto relationshipBefore = captureLayerRelationships(
            removedMatteReferences_, removedParentReferences_);
        const auto result = atTop_ ? comp->appendLayerTop(layer)
                                   : comp->appendLayerBottom(layer);
        if (!result.success || !comp->containsLayerById(layer->id())) {
            if (comp->containsLayerById(layer->id())) {
                comp->removeLayer(layer->id());
            }
            restoreLayerRelationships(relationshipBefore);
            return;
        }
        bool referencesRestored = true;
        for (const auto& [dependentLayer, refs] : removedMatteReferences_) {
            if (!dependentLayer ||
                (!comp->containsLayerById(dependentLayer->id()) &&
                 dependentLayer->compositionObject() != nullptr)) {
                referencesRestored = false;
                continue;
            }
            dependentLayer->setMatteReferences(refs);
            if (comp->containsLayerById(dependentLayer->id())) {
                dependentLayer->changed();
            }
            referencesRestored = matteReferencesEqual(
                dependentLayer->matteReferences(), refs) && referencesRestored;
        }
        bool parentsRestored = true;
        for (const auto& [dependentLayer, parentId] : removedParentReferences_) {
            if (!dependentLayer ||
                (!comp->containsLayerById(dependentLayer->id()) &&
                 dependentLayer->compositionObject() != nullptr)) {
                parentsRestored = false;
                continue;
            }
            dependentLayer->setParentById(parentId);
            parentsRestored = dependentLayer->parentLayerId() == parentId &&
                              parentsRestored;
        }
        lastOperationSucceeded_ = referencesRestored && parentsRestored;
        if (!lastOperationSucceeded_) {
            comp->removeLayer(layer->id());
            restoreLayerRelationships(relationshipBefore);
            return;
        }
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

QString AddLayerCommand::label() const {
    if (layer_) {
        return QStringLiteral("Add Layer: %1").arg(layer_->id().toString());
    }
    return QStringLiteral("Add Layer");
}

size_t AddLayerCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this) +
        static_cast<size_t>(compositionId_.size() + layerId_.size()) * sizeof(QChar);
    for (const auto& [layer, refs] : removedMatteReferences_) {
        Q_UNUSED(layer);
        bytes += sizeof(LayerMatteReference) * refs.size();
    }
    bytes += sizeof(ArtifactCore::LayerID) * removedParentReferences_.size();
    return bytes;
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

// --- AddLayerEffectCommand ---
namespace {
bool layerHasEffect(const ArtifactAbstractLayerPtr& layer,
                    const QString& effectId) {
    if (!layer || effectId.isEmpty()) return false;
    const auto effects = layer->getEffects();
    return std::any_of(effects.cbegin(), effects.cend(),
                       [&effectId](const auto& effect) {
                           return effect &&
                                  effect->effectID().toQString() == effectId;
                       });
}
}

AddLayerEffectCommand::AddLayerEffectCommand(
    ArtifactAbstractLayerPtr layer, ArtifactAbstractEffectPtr effect)
    : layer_(layer), effect_(std::move(effect)),
      layerId_(layer ? layer->id().toString() : QString()),
      effectId_(effect_ ? effect_->effectID().toQString() : QString()) {}

void AddLayerEffectCommand::undo() {
    lastOperationSucceeded_ = false;
    auto layer = layer_.lock();
    if (!layer || !effect_ || effectId_.isEmpty()) return;
    if (!layerHasEffect(layer, effectId_)) {
        lastOperationSucceeded_ = true;
        return;
    }
    layer->removeEffect(UniString::fromQString(effectId_));
    lastOperationSucceeded_ = !layerHasEffect(layer, effectId_);
    if (lastOperationSucceeded_) {
        layer->changed();
        if (auto* manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
    }
}

void AddLayerEffectCommand::redo() {
    lastOperationSucceeded_ = false;
    auto layer = layer_.lock();
    if (!layer || !effect_ || effectId_.isEmpty()) return;
    if (!layerHasEffect(layer, effectId_)) {
        layer->addEffect(effect_);
    }
    lastOperationSucceeded_ = layerHasEffect(layer, effectId_);
    if (lastOperationSucceeded_) {
        layer->changed();
        if (auto* manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
    }
}

size_t AddLayerEffectCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        (layerId_.size() + effectId_.size()) * sizeof(QChar));
}

QJsonObject AddLayerEffectCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("effectId"), effectId_}};
}

bool AddLayerEffectCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    auto* manager = UndoManager::instance();
    if (!manager || layerId_.isEmpty() || effectId_.isEmpty()) return false;
    layer_ = manager->resolveLayer(layerId_);
    effect_ = manager->resolveEffect(effectId_);
    return !layer_.expired() && static_cast<bool>(effect_);
}

// --- RemoveLayerCommand ---
MoveMaskCommand::MoveMaskCommand(ArtifactAbstractLayerPtr layer, int oldIndex, int newIndex)
    : layer_(layer), layerId_(layer ? layer->id().toString() : QString()),
      oldIndex_(oldIndex), newIndex_(newIndex) {}

void MoveMaskCommand::undo() {
  lastOperationSucceeded_ = false;
  if (auto layer = layer_.lock()) {
    lastOperationSucceeded_ = layer->moveMask(newIndex_, oldIndex_);
    if (!lastOperationSucceeded_) return;
    layer->changed();
  }
}

void MoveMaskCommand::redo() {
  lastOperationSucceeded_ = false;
  if (auto layer = layer_.lock()) {
    lastOperationSucceeded_ = layer->moveMask(oldIndex_, newIndex_);
    if (!lastOperationSucceeded_) return;
    layer->changed();
  }
}

QString MoveMaskCommand::label() const {
  return QStringLiteral("Move Mask");
}

size_t MoveMaskCommand::estimatedMemoryBytes() const {
  return sizeof(*this);
}

QJsonObject MoveMaskCommand::serialize() const {
  return QJsonObject{{QStringLiteral("layerId"), layerId_},
                     {QStringLiteral("oldIndex"), oldIndex_},
                     {QStringLiteral("newIndex"), newIndex_}};
}

bool MoveMaskCommand::deserialize(const QJsonObject& data) {
  layerId_ = data.value(QStringLiteral("layerId")).toString();
  if (!nonNegativeJsonInt(data.value(QStringLiteral("oldIndex")), oldIndex_) ||
      !nonNegativeJsonInt(data.value(QStringLiteral("newIndex")), newIndex_)) return false;
  auto* manager = UndoManager::instance();
  if (!manager) return false;
  layer_ = manager->resolveLayer(layerId_);
  return !layerId_.isEmpty() && oldIndex_ >= 0 && newIndex_ >= 0 &&
         !layer_.expired();
}

// --- RemoveLayerCommand ---
RemoveLayerCommand::RemoveLayerCommand(ArtifactCompositionPtr comp, ArtifactAbstractLayerPtr layer)
    : comp_(comp), layer_(layer),
      compositionId_(comp ? comp->id().toString() : QString()),
      layerId_(layer ? layer->id().toQString() : QString()),
      originalIndex_(-1) {
    if (!comp || !layer) {
        return;
    }
    if (auto* selection = ArtifactLayerSelectionManager::instance();
        selection && selection->activeComposition() == comp) {
        hasSelectionSnapshot_ = true;
        for (const auto& selected : selection->selectedLayersInOrder()) {
            if (selected) selectedLayerIds_.append(selected->id().toString());
        }
        if (const auto current = selection->currentLayer()) {
            currentSelectedLayerId_ = current->id().toString();
        }
    }
    for (const auto& candidate : comp->allLayer()) {
        if (!candidate || candidate->id() == layer->id()) {
            continue;
        }
        if (candidate->parentLayerId() == layer->id()) {
            removedParentReferences_.emplace_back(candidate,
                                                  candidate->parentLayerId());
        }
        const auto refs = candidate->matteReferences();
        const auto hasReference = std::any_of(
            refs.cbegin(), refs.cend(), [&layer](const LayerMatteReference& ref) {
                return ref.sourceLayerId == layer->id();
            });
        if (hasReference) {
            removedMatteReferences_.emplace_back(candidate, refs);
        }
    }
}

void RemoveLayerCommand::undo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    lastOperationSucceeded_ = false;
    if (comp && layer && !comp->containsLayerById(layer->id())) {
        const auto relationshipBefore = captureLayerRelationships(
            removedMatteReferences_, removedParentReferences_);
        QStringList selectedAfter = selectedLayerIds_;
        selectedAfter.removeAll(layerId_);
        QString currentAfter = currentSelectedLayerId_;
        if (currentAfter == layerId_) {
            currentAfter = selectedAfter.isEmpty() ? QString()
                                                    : selectedAfter.back();
        }
        if (originalIndex_ >= 0) {
            comp->insertLayerAt(layer, originalIndex_);
        } else {
            comp->appendLayerTop(layer);
        }
        if (!comp->containsLayerById(layer->id())) return;
        bool referencesRestored = true;
        for (const auto& [dependentLayer, refs] : removedMatteReferences_) {
            if (!dependentLayer || !comp->containsLayerById(dependentLayer->id())) {
                referencesRestored = false;
                continue;
            }
            dependentLayer->setMatteReferences(refs);
            dependentLayer->changed();
            referencesRestored = matteReferencesEqual(
                dependentLayer->matteReferences(), refs) && referencesRestored;
        }
        bool parentsRestored = true;
        for (const auto& [dependentLayer, parentId] : removedParentReferences_) {
            if (!dependentLayer || !comp->containsLayerById(dependentLayer->id())) {
                parentsRestored = false;
                continue;
            }
            dependentLayer->setParentById(parentId);
            parentsRestored = dependentLayer->parentLayerId() == parentId &&
                              parentsRestored;
        }
        const bool selectionRestored =
            !hasSelectionSnapshot_ ||
            restoreLayerSelection(comp, selectedLayerIds_, currentSelectedLayerId_);
        lastOperationSucceeded_ = referencesRestored && parentsRestored &&
                                  selectionRestored;
        if (!lastOperationSucceeded_) {
            comp->removeLayer(layer->id());
            restoreLayerRelationships(relationshipBefore);
            if (hasSelectionSnapshot_) {
                restoreLayerSelection(comp, selectedAfter, currentAfter);
            }
            return;
        }
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void RemoveLayerCommand::redo() {
    auto comp = comp_.lock();
    auto layer = layer_;
    lastOperationSucceeded_ = false;
    if (comp && layer && comp->containsLayerById(layer->id())) {
        const auto relationshipBefore = captureLayerRelationships(
            removedMatteReferences_, removedParentReferences_);
        QStringList selectedAfter = selectedLayerIds_;
        selectedAfter.removeAll(layerId_);
        QString currentAfter = currentSelectedLayerId_;
        if (currentAfter == layerId_) {
            currentAfter = selectedAfter.isEmpty() ? QString()
                                                    : selectedAfter.back();
        }
        // Save original index before removing
        auto layers = comp->allLayer();
        for (int i = 0; i < layers.size(); ++i) {
            if (layers[i] && layers[i]->id() == layer->id()) {
                originalIndex_ = i;
                break;
            }
        }
        if (originalIndex_ < 0) return;
        comp->removeLayer(layer->id());
        if (comp->containsLayerById(layer->id())) return;
        bool selectionRestored = true;
        bool referencesRemoved = true;
        for (const auto& [dependentLayer, refs] : removedMatteReferences_) {
            if (!dependentLayer || !comp->containsLayerById(dependentLayer->id())) {
                referencesRemoved = false;
                continue;
            }
            const auto currentRefs = dependentLayer->matteReferences();
            const bool stillReferencesRemoved = std::none_of(
                currentRefs.cbegin(), currentRefs.cend(),
                [&layer](const LayerMatteReference& ref) {
                    return ref.sourceLayerId == layer->id();
                });
            referencesRemoved = stillReferencesRemoved && referencesRemoved;
        }
        bool parentsRemoved = true;
        for (const auto& [dependentLayer, parentId] : removedParentReferences_) {
            if (!dependentLayer || !comp->containsLayerById(dependentLayer->id())) {
                parentsRemoved = false;
                continue;
            }
            parentsRemoved = dependentLayer->parentLayerId().isNil() &&
                             parentsRemoved;
        }
        if (hasSelectionSnapshot_) {
            selectionRestored = restoreLayerSelection(comp, selectedAfter, currentAfter);
        }
        lastOperationSucceeded_ = referencesRemoved && parentsRemoved &&
                                  selectionRestored;
        if (!lastOperationSucceeded_) {
            comp->insertLayerAt(layer, originalIndex_);
            restoreLayerRelationships(relationshipBefore);
            if (hasSelectionSnapshot_) {
                restoreLayerSelection(comp, selectedLayerIds_, currentSelectedLayerId_);
            }
            return;
        }
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

QString RemoveLayerCommand::label() const {
    if (layer_) {
        return QStringLiteral("Remove Layer: %1").arg(layer_->id().toString());
    }
    return QStringLiteral("Remove Layer");
}

size_t RemoveLayerCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this) +
        static_cast<size_t>(compositionId_.size() + layerId_.size()) * sizeof(QChar);
    for (const auto& [layer, refs] : removedMatteReferences_) {
        Q_UNUSED(layer);
        bytes += sizeof(LayerMatteReference) * refs.size();
    }
    bytes += sizeof(ArtifactCore::LayerID) * removedParentReferences_.size();
    bytes += static_cast<size_t>(currentSelectedLayerId_.size()) * sizeof(QChar);
    for (const auto& id : selectedLayerIds_) {
        bytes += static_cast<size_t>(id.size()) * sizeof(QChar);
    }
    return bytes;
}

QJsonObject RemoveLayerCommand::serialize() const {
    QJsonArray selected;
    for (const auto& id : selectedLayerIds_) selected.append(id);
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("originalIndex"), originalIndex_},
                       {QStringLiteral("selectedLayerIds"), selected},
                       {QStringLiteral("currentSelectedLayerId"),
                        currentSelectedLayerId_}};
}

bool RemoveLayerCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    if (!nonNegativeJsonInt(data.value(QStringLiteral("originalIndex")), originalIndex_)) return false;
    hasSelectionSnapshot_ = data.contains(QStringLiteral("selectedLayerIds")) ||
                            data.contains(QStringLiteral("currentSelectedLayerId"));
    selectedLayerIds_.clear();
    for (const auto& value : data.value(QStringLiteral("selectedLayerIds")).toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty()) selectedLayerIds_.append(id);
    }
    currentSelectedLayerId_ = data.value(
        QStringLiteral("currentSelectedLayerId")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    comp_ = manager->resolveComposition(compositionId_);
    layer_ = manager->resolveLayer(layerId_);
    return !compositionId_.isEmpty() && !layerId_.isEmpty() && !comp_.expired() && static_cast<bool>(layer_);
}

LayerSelectionSnapshotCommand::LayerSelectionSnapshotCommand(
    ArtifactCompositionPtr composition,
    QStringList beforeLayerIds, QString beforeCurrentLayerId,
    QStringList afterLayerIds, QString afterCurrentLayerId)
    : composition_(composition), beforeLayerIds_(std::move(beforeLayerIds)),
      beforeCurrentLayerId_(std::move(beforeCurrentLayerId)),
      afterLayerIds_(std::move(afterLayerIds)),
      afterCurrentLayerId_(std::move(afterCurrentLayerId)) {}

void LayerSelectionSnapshotCommand::apply(
    const QStringList& layerIds, const QString& currentLayerId) {
    if (auto composition = composition_.lock()) {
        lastOperationSucceeded_ = restoreLayerSelection(
            composition, layerIds, currentLayerId);
    } else {
        lastOperationSucceeded_ = false;
    }
}

void LayerSelectionSnapshotCommand::undo() {
    apply(beforeLayerIds_, beforeCurrentLayerId_);
}

void LayerSelectionSnapshotCommand::redo() {
    apply(afterLayerIds_, afterCurrentLayerId_);
}

QString LayerSelectionSnapshotCommand::label() const {
    return QStringLiteral("Restore Layer Selection");
}

size_t LayerSelectionSnapshotCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this);
    bytes += static_cast<size_t>(beforeCurrentLayerId_.size() +
                                 afterCurrentLayerId_.size()) * sizeof(QChar);
    for (const auto& id : beforeLayerIds_) {
        bytes += static_cast<size_t>(id.size()) * sizeof(QChar);
    }
    for (const auto& id : afterLayerIds_) {
        bytes += static_cast<size_t>(id.size()) * sizeof(QChar);
    }
    return bytes;
}

namespace {
bool applyMaskSnapshot(const ArtifactAbstractLayerPtr& layer,
                       const std::vector<LayerMask>& masks,
                       const std::vector<LayerMask>& compensationMasks) {
    if (!layer) {
        return false;
    }

    const auto apply = [&layer](const std::vector<LayerMask>& snapshot) {
        layer->clearMasks();
        for (const auto& mask : snapshot) {
            layer->addMask(mask);
        }
        layer->changed();
        return layer->maskCount() == static_cast<int>(snapshot.size());
    };
    if (apply(masks)) {
        return true;
    }
    apply(compensationMasks);
    return false;
}

bool applyMatteSnapshot(const ArtifactAbstractLayerPtr& layer,
                        const std::vector<LayerMatteReference>& mattes,
                        const std::vector<LayerMatteReference>& compensationMattes) {
    if (!layer) {
        return false;
    }

    const auto apply = [&layer](const std::vector<LayerMatteReference>& snapshot) {
        layer->setMatteReferences(snapshot);
        layer->changed();
        const auto applied = layer->matteReferences();
        if (applied.size() != snapshot.size()) {
            return false;
        }
        for (size_t index = 0; index < applied.size(); ++index) {
            if (applied[index].toJson() != snapshot[index].toJson()) {
                return false;
            }
        }
        return true;
    };
    if (apply(mattes)) {
        return true;
    }
    apply(compensationMattes);
    return false;
}

bool applyLayerPropertyKeyframeSnapshot(
    const ArtifactAbstractLayerPtr& layer,
    const QString& propertyPath,
    const std::vector<ArtifactCore::KeyFrame>& keyframes,
    const std::optional<bool>& animatable = std::nullopt) {
    if (!layer || propertyPath.trimmed().isEmpty()) {
        return false;
    }

    auto property = layer->getProperty(propertyPath);
    if (!property) {
        return false;
    }

    const auto previousKeyframes = property->getKeyFrames();
    const bool previousAnimatable = property->isAnimatable();

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
            property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
            property->setKeyFrameColorLabelAt(keyframe.time,
                                              keyframe.colorLabel);
        }
    }
    if (animatable.has_value()) {
        property->setAnimatable(*animatable);
    }

    const auto appliedKeyframes = property->getKeyFrames();
    bool applied = appliedKeyframes.size() == keyframes.size();
    if (applied) {
        for (int index = 0; index < appliedKeyframes.size(); ++index) {
            const auto& actual = appliedKeyframes[static_cast<size_t>(index)];
            const auto& expected = keyframes[static_cast<size_t>(index)];
            if (actual.time.value() != expected.time.value() ||
                actual.time.scale() != expected.time.scale() ||
                actual.value != expected.value ||
                actual.interpolation != expected.interpolation ||
                actual.cp1_x != expected.cp1_x || actual.cp1_y != expected.cp1_y ||
                actual.cp2_x != expected.cp2_x || actual.cp2_y != expected.cp2_y ||
                actual.roving != expected.roving ||
                actual.anchor != expected.anchor ||
                actual.colorLabel != expected.colorLabel) {
                applied = false;
                break;
            }
        }
    }
    if (animatable.has_value() && property->isAnimatable() != *animatable) {
        applied = false;
    }
    if (!applied) {
        property->clearKeyFrames();
        for (const auto& keyframe : previousKeyframes) {
            property->addKeyFrame(keyframe.time, keyframe.value,
                                  static_cast<int>(keyframe.interpolation),
                                  keyframe.cp1_x, keyframe.cp1_y,
                                  keyframe.cp2_x, keyframe.cp2_y,
                                  keyframe.roving);
            property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
            property->setKeyFrameColorLabelAt(keyframe.time,
                                              keyframe.colorLabel);
        }
        property->setAnimatable(previousAnimatable);
        return false;
    }

    notifyLayerPropertyChanged(layer, propertyPath);
    return true;
}

bool applyLayerPropertyValue(const ArtifactAbstractLayerPtr& layer,
                             const QString& propertyPath,
                             const QVariant& value) {
    if (!layer || propertyPath.trimmed().isEmpty()) {
        return false;
    }
    if (layer->setLayerPropertyValue(propertyPath, value)) {
        return true;
    }
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
        return false;
    }
    const auto previousValue = property->getValue();
    property->setValue(value);
    if (property->getValue() != value) {
        property->setValue(previousValue);
        return false;
    }
    notifyLayerPropertyChanged(layer, propertyPath);
    return true;
}

bool applyAudioDeClickRanges(
    const ArtifactAbstractLayerPtr& layer,
    const std::vector<std::pair<qint64, qint64>>& ranges) {
    const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer);
    if (!audioLayer) {
        return false;
    }
    audioLayer->setDeClickRanges(ranges);
    return audioLayer->deClickRanges() == ranges;
}

CompositionItem* findCompositionItemInTreeForUndo(
    const QVector<ProjectItem*>& items, const CompositionID& compositionId) {
    for (auto* item : items) {
        if (!item) {
            continue;
        }
        if (item->type() == eProjectItemType::Composition) {
            auto* compositionItem = static_cast<CompositionItem*>(item);
            if (compositionItem->compositionId == compositionId) {
                return compositionItem;
            }
        }
        if (auto* nested = findCompositionItemInTreeForUndo(item->children,
                                                              compositionId)) {
            return nested;
        }
    }
    return nullptr;
}

bool applyCompositionDisplayNameForUndo(
    const ArtifactCompositionPtr& composition, const QString& name) {
    if (!composition) {
        return false;
    }
    composition->setCompositionName(UniString::fromQString(name));
    if (composition->settings().compositionName().toQString() != name) {
        return false;
    }
    auto& manager = ArtifactProjectManager::getInstance();
    if (auto project = manager.getCurrentProjectSharedPtr()) {
        if (auto* item = findCompositionItemInTreeForUndo(
                project->projectItems(), composition->id())) {
            item->name = UniString::fromQString(name);
        }
        project->projectChanged();
    }
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
        {QString(), QString()});
    if (auto* undo = UndoManager::instance()) {
        undo->notifyAnythingChanged();
    }
    return true;
}

bool applyLayerPropertyExpression(const ArtifactAbstractLayerPtr& layer,
                                  const QString& propertyPath,
                                  const QString& expression) {
    if (!layer || propertyPath.trimmed().isEmpty()) {
        return false;
    }
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
        return false;
    }
    const auto previousExpression = property->getExpression();
    property->setExpression(expression);
    if (property->getExpression() != expression) {
        property->setExpression(previousExpression);
        return false;
    }
    notifyLayerPropertyChanged(layer, propertyPath);
    return true;
}

bool applyEffectPropertyKeyframeSnapshot(
    const ArtifactAbstractEffectPtr& effect, const QString& propertyName,
    const std::vector<ArtifactCore::KeyFrame>& keyframes) {
    if (!effect || propertyName.trimmed().isEmpty()) {
        return false;
    }
    const auto property = effect->editableProperty(propertyName);
    if (!property) {
        return false;
    }
    const auto previousKeyframes = property->getKeyFrames();
    property->clearKeyFrames();
    for (const auto& keyframe : keyframes) {
        property->addKeyFrame(keyframe.time, keyframe.value,
                              static_cast<int>(keyframe.interpolation),
                              keyframe.cp1_x, keyframe.cp1_y,
                              keyframe.cp2_x, keyframe.cp2_y,
                              keyframe.roving);
        property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
        property->setKeyFrameColorLabelAt(keyframe.time, keyframe.colorLabel);
    }
    const auto appliedKeyframes = property->getKeyFrames();
    bool applied = appliedKeyframes.size() == keyframes.size();
    if (applied) {
        for (int index = 0; index < appliedKeyframes.size(); ++index) {
            const auto& actual = appliedKeyframes[static_cast<size_t>(index)];
            const auto& expected = keyframes[static_cast<size_t>(index)];
            if (actual.time.value() != expected.time.value() ||
                actual.time.scale() != expected.time.scale() ||
                actual.value != expected.value ||
                actual.interpolation != expected.interpolation ||
                actual.cp1_x != expected.cp1_x || actual.cp1_y != expected.cp1_y ||
                actual.cp2_x != expected.cp2_x || actual.cp2_y != expected.cp2_y ||
                actual.roving != expected.roving ||
                actual.anchor != expected.anchor ||
                actual.colorLabel != expected.colorLabel) {
                applied = false;
                break;
            }
        }
    }
    if (!applied) {
        property->clearKeyFrames();
        for (const auto& keyframe : previousKeyframes) {
            property->addKeyFrame(keyframe.time, keyframe.value,
                                  static_cast<int>(keyframe.interpolation),
                                  keyframe.cp1_x, keyframe.cp1_y,
                                  keyframe.cp2_x, keyframe.cp2_y,
                                  keyframe.roving);
            property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
            property->setKeyFrameColorLabelAt(keyframe.time,
                                              keyframe.colorLabel);
        }
        return false;
    }
    return true;
}

bool applyEffectPropertyExpression(const ArtifactAbstractEffectPtr& effect,
                                   const QString& propertyName,
                                   const QString& expression) {
    if (!effect || propertyName.trimmed().isEmpty()) {
        return false;
    }
    if (const auto property = effect->editableProperty(propertyName)) {
    const auto previousExpression = property->getExpression();
    property->setExpression(expression);
    if (property->getExpression() != expression) {
        property->setExpression(previousExpression);
        return false;
    }
    return true;
}
    return false;
}

bool applyTextLayerTextSnapshot(const ArtifactAbstractLayerPtr& layer,
                                const QString& text) {
    if (!layer) {
        return false;
    }

    return layer->setLayerPropertyValue(QStringLiteral("text.value"), text);
}

bool applyEffectMaskImageSnapshot(
    const ArtifactAbstractEffectPtr& effect,
    const std::vector<SharedPtr<ImageF32x4_RGBA>>& masks) {
    if (!effect) {
        return false;
    }

    effect->clearEffectMaskImages();
    int expectedCount = 0;
    for (const auto& mask : masks) {
        if (mask) {
            effect->addEffectMaskImage(mask);
            ++expectedCount;
        }
    }
    if (effect->effectMaskImageCount() != expectedCount) {
        return false;
    }
    int imageIndex = 0;
    for (const auto& mask : masks) {
        if (mask && effect->effectMaskImage(imageIndex++) != mask) {
            return false;
        }
    }
    return true;
}
} // namespace

// --- MaskEditCommand ---
MaskEditCommand::MaskEditCommand(ArtifactAbstractLayerPtr layer,
                                 std::vector<LayerMask> beforeMasks,
                                 std::vector<LayerMask> afterMasks)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      beforeMasks_(std::move(beforeMasks)), afterMasks_(std::move(afterMasks)) {}

void MaskEditCommand::undo() {
    lastOperationSucceeded_ = applyMaskSnapshot(
        layer_.lock(), beforeMasks_, afterMasks_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void MaskEditCommand::redo() {
    lastOperationSucceeded_ = applyMaskSnapshot(
        layer_.lock(), afterMasks_, beforeMasks_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
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
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isArray() || !after.isArray()) return false;
    const auto decode = [](const QJsonArray& values, auto& masks) {
        masks.clear();
        for (const auto& value : values) {
            if (!value.isObject() || !maskJsonStructureValid(value.toObject())) return false;
            masks.push_back(decodeMask(value.toObject()));
        }
        return true;
    };
    if (!decode(before.toArray(), beforeMasks_) ||
        !decode(after.toArray(), afterMasks_)) return false;
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

bool maskJsonStructureValid(const QJsonObject& object) {
    const auto paths = object.value(QStringLiteral("paths"));
    if (!object.value(QStringLiteral("enabled")).isBool() ||
        !object.value(QStringLiteral("locked")).isBool() || !paths.isArray()) {
        return false;
    }
    const auto pathValues = paths.toArray();
    if (pathValues.size() > 100000) return false;
    const auto finiteIfPresent = [](const QJsonObject& value,
                                    const QString& key) {
        if (!value.contains(key)) return true;
        float converted = 0.0f;
        return finiteJsonNumber(value, key, converted);
    };
    for (const auto& pathValue : pathValues) {
        if (!pathValue.isObject()) return false;
        const auto path = pathValue.toObject();
        const auto vertices = path.value(QStringLiteral("vertices"));
        if (!path.value(QStringLiteral("closed")).isBool() ||
            !path.value(QStringLiteral("inverted")).isBool() ||
            !path.value(QStringLiteral("name")).isString() ||
            !vertices.isArray() ||
            !finiteIfPresent(path, QStringLiteral("opacity")) ||
            !finiteIfPresent(path, QStringLiteral("feather")) ||
            !finiteIfPresent(path, QStringLiteral("featherHorizontal")) ||
            !finiteIfPresent(path, QStringLiteral("featherVertical")) ||
            !finiteIfPresent(path, QStringLiteral("featherInner")) ||
            !finiteIfPresent(path, QStringLiteral("featherOuter")) ||
            !finiteIfPresent(path, QStringLiteral("expansion"))) return false;
        if (path.contains(QStringLiteral("mode"))) {
            int mode = 0;
            if (!jsonEnumInt(path.value(QStringLiteral("mode")), mode) ||
                mode < static_cast<int>(MaskMode::Add) ||
                mode > static_cast<int>(MaskMode::Difference)) {
                return false;
            }
        }
        const auto vertexValues = vertices.toArray();
        if (vertexValues.size() > 100000) return false;
        for (const auto& vertexValue : vertexValues) {
            if (!vertexValue.isObject()) return false;
            const auto vertex = vertexValue.toObject();
            float coordinate = 0.0f;
            if (!finiteJsonNumber(vertex, QStringLiteral("x"), coordinate) ||
                !finiteJsonNumber(vertex, QStringLiteral("y"), coordinate) ||
                !finiteJsonNumber(vertex, QStringLiteral("inX"), coordinate) ||
                !finiteJsonNumber(vertex, QStringLiteral("inY"), coordinate) ||
                !finiteJsonNumber(vertex, QStringLiteral("outX"), coordinate) ||
                !finiteJsonNumber(vertex, QStringLiteral("outY"), coordinate)) {
                return false;
            }
        }
    }
    return true;
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
    lastOperationSucceeded_ = applyMatteSnapshot(
        layer_.lock(), beforeRefs_, afterRefs_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void ChangeLayerMatteReferencesCommand::redo() {
    lastOperationSucceeded_ = applyMatteSnapshot(
        layer_.lock(), afterRefs_, beforeRefs_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
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
    const auto beforeValue = data.value(QStringLiteral("before"));
    const auto afterValue = data.value(QStringLiteral("after"));
    if (!beforeValue.isArray() || !afterValue.isArray()) {
        return false;
    }
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto decode = [](const QJsonArray& values, auto& refs) {
        refs.clear();
        for (const auto& value : values) {
            if (!value.isObject()) {
                return false;
            }
            LayerMatteReference ref;
            ref.fromJson(value.toObject());
            refs.push_back(std::move(ref));
        }
        return true;
    };
    if (!decode(beforeValue.toArray(), beforeRefs_) ||
        !decode(afterValue.toArray(), afterRefs_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

namespace {
QJsonValue encodeKeyframeValue(const std::any& value, bool& supported) {
    supported = true;
    if (!value.has_value()) return QJsonValue::Null;
    if (const auto* v = std::any_cast<QVariant>(&value)) {
        if (!v->isValid()) return QJsonValue::Null;
        const auto encoded = QJsonValue::fromVariant(*v);
        if (encoded.isUndefined()) supported = false;
        return encoded;
    }
    if (const auto* v = std::any_cast<float>(&value)) return *v;
    if (const auto* v = std::any_cast<double>(&value)) return *v;
    if (const auto* v = std::any_cast<int>(&value)) return *v;
    if (const auto* v = std::any_cast<bool>(&value)) return *v;
    if (const auto* v = std::any_cast<QString>(&value)) return *v;
    supported = false;
    return {};
}

bool decodeKeyframes(const QJsonArray& encoded, std::vector<ArtifactCore::KeyFrame>& target) {
    if (encoded.size() > 100000) return false;
    target.clear();
    target.reserve(encoded.size());
    for (const auto& item : encoded) {
        if (!item.isObject()) return false;
        const auto object = item.toObject();
        const auto frameValue = object.value(QStringLiteral("frame"));
        const auto valueValue = object.value(QStringLiteral("value"));
        qint64 frame = 0;
        if (!jsonInteger(frameValue, frame) || valueValue.isUndefined()) return false;
        ArtifactCore::KeyFrame keyframe;
        const bool hasTimeValue = object.contains(QStringLiteral("timeValue"));
        const bool hasTimeScale = object.contains(QStringLiteral("timeScale"));
        if (hasTimeValue != hasTimeScale) return false;
        if (hasTimeValue) {
            qint64 timeValue = 0;
            qint64 timeScale = 0;
            if (!jsonInteger(object.value(QStringLiteral("timeValue")), timeValue) ||
                !jsonInteger(object.value(QStringLiteral("timeScale")), timeScale) ||
                timeScale <= 0) return false;
            keyframe.time = ArtifactCore::RationalTime(
                timeValue, timeScale);
        } else {
            // Legacy payloads used a 30fps frame-only representation.
            keyframe.time = ArtifactCore::RationalTime(frame, 30);
        }
        keyframe.value = QVariant(valueValue.toVariant());
        int interpolation = 0;
        if (!jsonEnumInt(object.value(QStringLiteral("interpolation")), interpolation) ||
            interpolation < 0 ||
            interpolation > static_cast<int>(ArtifactCore::InterpolationType::Perceptual) ||
            !finiteJsonNumber(object, QStringLiteral("cp1_x"), keyframe.cp1_x) ||
            !finiteJsonNumber(object, QStringLiteral("cp1_y"), keyframe.cp1_y) ||
            !finiteJsonNumber(object, QStringLiteral("cp2_x"), keyframe.cp2_x) ||
            !finiteJsonNumber(object, QStringLiteral("cp2_y"), keyframe.cp2_y)) return false;
        keyframe.interpolation = static_cast<ArtifactCore::InterpolationType>(interpolation);
        if (object.contains(QStringLiteral("roving"))) {
            if (!object.value(QStringLiteral("roving")).isBool()) return false;
            keyframe.roving = object.value(QStringLiteral("roving")).toBool();
        }
        if (object.contains(QStringLiteral("anchor"))) {
            int anchor = 0;
            if (!jsonEnumInt(object.value(QStringLiteral("anchor")), anchor) ||
                anchor < static_cast<int>(ArtifactCore::KeyFrame::Anchor::Absolute) ||
                anchor > static_cast<int>(ArtifactCore::KeyFrame::Anchor::StretchWithLayer)) return false;
            keyframe.anchor = static_cast<ArtifactCore::KeyFrame::Anchor>(anchor);
        }
        if (object.contains(QStringLiteral("colorLabel"))) {
            int colorLabel = 0;
            if (!jsonEnumInt(object.value(QStringLiteral("colorLabel")), colorLabel) ||
                colorLabel < static_cast<int>(ArtifactCore::KeyFrame::ColorLabel::None) ||
                colorLabel > static_cast<int>(ArtifactCore::KeyFrame::ColorLabel::Gray)) return false;
            keyframe.colorLabel = static_cast<ArtifactCore::KeyFrame::ColorLabel>(colorLabel);
        }
        target.push_back(std::move(keyframe));
    }
    return true;
}
}

// --- SetEffectPropertyKeyframesCommand ---
SetEffectPropertyKeyframesCommand::SetEffectPropertyKeyframesCommand(
    ArtifactAbstractEffectPtr effect, QString propertyName,
    std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
    std::vector<ArtifactCore::KeyFrame> afterKeyframes, QString label)
    : effect_(effect),
      effectId_(effect ? effect->effectID().toQString() : QString()),
      propertyName_(std::move(propertyName)),
      beforeKeyframes_(std::move(beforeKeyframes)),
      afterKeyframes_(std::move(afterKeyframes)), label_(std::move(label)) {}

void SetEffectPropertyKeyframesCommand::undo() {
    auto effect = effect_.lock();
    lastOperationSucceeded_ = applyEffectPropertyKeyframeSnapshot(
        effect, propertyName_, beforeKeyframes_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(effectId_);
        }
    }
}

void SetEffectPropertyKeyframesCommand::redo() {
    auto effect = effect_.lock();
    lastOperationSucceeded_ = applyEffectPropertyKeyframeSnapshot(
        effect, propertyName_, afterKeyframes_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(effectId_);
        }
    }
}

QString SetEffectPropertyKeyframesCommand::label() const {
    return label_;
}

size_t SetEffectPropertyKeyframesCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        (beforeKeyframes_.size() + afterKeyframes_.size()) *
            sizeof(ArtifactCore::KeyFrame) +
        (effectId_.size() + propertyName_.size() + label_.size()) * sizeof(QChar));
}

bool SetEffectPropertyKeyframesCommand::canSerialize() const {
    if (effectId_.isEmpty() || effect_.expired() || propertyName_.isEmpty()) {
        return false;
    }
    const auto valuesSerializable = [](const auto& keyframes) {
        for (const auto& keyframe : keyframes) {
            bool supported = false;
            if (encodeKeyframeValue(keyframe.value, supported).isUndefined() || !supported) {
                return false;
            }
        }
        return true;
    };
    return valuesSerializable(beforeKeyframes_) && valuesSerializable(afterKeyframes_);
}

QJsonObject SetEffectPropertyKeyframesCommand::serialize() const {
    const auto encode = [](const std::vector<ArtifactCore::KeyFrame>& keyframes) {
        QJsonArray values;
        for (const auto& keyframe : keyframes) {
            values.append(QJsonObject{
                {QStringLiteral("frame"),
                 static_cast<qint64>(keyframe.time.rescaledTo(30))},
                {QStringLiteral("timeValue"),
                 static_cast<qint64>(keyframe.time.value())},
                {QStringLiteral("timeScale"),
                 static_cast<qint64>(keyframe.time.scale())},
                {QStringLiteral("value"), QJsonValue::fromVariant(keyframe.value)},
                {QStringLiteral("interpolation"),
                 static_cast<int>(keyframe.interpolation)},
                {QStringLiteral("cp1_x"), keyframe.cp1_x},
                {QStringLiteral("cp1_y"), keyframe.cp1_y},
                {QStringLiteral("cp2_x"), keyframe.cp2_x},
                {QStringLiteral("cp2_y"), keyframe.cp2_y},
                {QStringLiteral("roving"), keyframe.roving},
                {QStringLiteral("anchor"), static_cast<int>(keyframe.anchor)},
                {QStringLiteral("colorLabel"),
                 static_cast<int>(keyframe.colorLabel)}});
        }
        return values;
    };
    return QJsonObject{{QStringLiteral("effectId"), effectId_},
                       {QStringLiteral("propertyName"), propertyName_},
                       {QStringLiteral("before"), encode(beforeKeyframes_)},
                       {QStringLiteral("after"), encode(afterKeyframes_)},
                       {QStringLiteral("label"), label_}};
}

bool SetEffectPropertyKeyframesCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    propertyName_ = data.value(QStringLiteral("propertyName")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isArray() || !after.isArray()) return false;
    if (!decodeKeyframes(before.toArray(),
                         beforeKeyframes_) ||
        !decodeKeyframes(after.toArray(),
                         afterKeyframes_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    effect_ = manager->resolveEffect(effectId_);
    return !effectId_.isEmpty() && !propertyName_.isEmpty() &&
           !effect_.expired();
}

// --- SetEffectPropertyExpressionCommand ---
SetEffectPropertyExpressionCommand::SetEffectPropertyExpressionCommand(
    ArtifactAbstractEffectPtr effect, QString propertyName,
    QString beforeExpression, QString afterExpression, QString label)
    : effect_(effect),
      effectId_(effect ? effect->effectID().toQString() : QString()),
      propertyName_(std::move(propertyName)),
      beforeExpression_(std::move(beforeExpression)),
      afterExpression_(std::move(afterExpression)), label_(std::move(label)) {}

void SetEffectPropertyExpressionCommand::undo() {
    lastOperationSucceeded_ = applyEffectPropertyExpression(
        effect_.lock(), propertyName_, beforeExpression_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(effectId_);
        }
    }
}

void SetEffectPropertyExpressionCommand::redo() {
    lastOperationSucceeded_ = applyEffectPropertyExpression(
        effect_.lock(), propertyName_, afterExpression_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyPropertyChanged(effectId_);
        }
    }
}

QString SetEffectPropertyExpressionCommand::label() const {
    return label_;
}

size_t SetEffectPropertyExpressionCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        effectId_.size() + propertyName_.size() + beforeExpression_.size() +
        afterExpression_.size() + label_.size()) * sizeof(QChar);
}

QJsonObject SetEffectPropertyExpressionCommand::serialize() const {
    return QJsonObject{{QStringLiteral("effectId"), effectId_},
                       {QStringLiteral("propertyName"), propertyName_},
                       {QStringLiteral("beforeExpression"), beforeExpression_},
                       {QStringLiteral("afterExpression"), afterExpression_},
                       {QStringLiteral("label"), label_}};
}

bool SetEffectPropertyExpressionCommand::deserialize(const QJsonObject& data) {
    effectId_ = data.value(QStringLiteral("effectId")).toString();
    propertyName_ = data.value(QStringLiteral("propertyName")).toString();
    beforeExpression_ = data.value(QStringLiteral("beforeExpression")).toString();
    afterExpression_ = data.value(QStringLiteral("afterExpression")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    effect_ = manager->resolveEffect(effectId_);
    return !effectId_.isEmpty() && !propertyName_.isEmpty() &&
           !effect_.expired();
}

// --- SetLayerPropertyKeyframesCommand ---
SetLayerPropertyKeyframesCommand::SetLayerPropertyKeyframesCommand(
    ArtifactAbstractLayerPtr layer,
    QString propertyPath,
    std::vector<ArtifactCore::KeyFrame> beforeKeyframes,
    std::vector<ArtifactCore::KeyFrame> afterKeyframes,
    QString label,
    std::optional<bool> beforeAnimatable,
    std::optional<bool> afterAnimatable)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      propertyPath_(std::move(propertyPath)),
      beforeKeyframes_(std::move(beforeKeyframes)),
      afterKeyframes_(std::move(afterKeyframes)),
      beforeAnimatable_(beforeAnimatable),
      afterAnimatable_(afterAnimatable),
      label_(std::move(label)) {}

void SetLayerPropertyKeyframesCommand::undo() {
    lastOperationSucceeded_ = applyLayerPropertyKeyframeSnapshot(
        layer_.lock(), propertyPath_, beforeKeyframes_, beforeAnimatable_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerPropertyKeyframesCommand::redo() {
    lastOperationSucceeded_ = applyLayerPropertyKeyframeSnapshot(
        layer_.lock(), propertyPath_, afterKeyframes_, afterAnimatable_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString SetLayerPropertyKeyframesCommand::label() const {
    return label_;
}

SetLayerPropertyValueCommand::SetLayerPropertyValueCommand(
    ArtifactAbstractLayerPtr layer,
    QString propertyPath,
    QVariant beforeValue,
    QVariant afterValue,
    QString label)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      propertyPath_(std::move(propertyPath)),
      beforeValue_(std::move(beforeValue)),
      afterValue_(std::move(afterValue)),
      label_(std::move(label)) {}

void SetLayerPropertyValueCommand::undo() {
    lastOperationSucceeded_ = applyLayerPropertyValue(
        layer_.lock(), propertyPath_, beforeValue_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerPropertyValueCommand::redo() {
    lastOperationSucceeded_ = applyLayerPropertyValue(
        layer_.lock(), propertyPath_, afterValue_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString SetLayerPropertyValueCommand::label() const {
    return label_;
}

size_t SetLayerPropertyValueCommand::estimatedMemoryBytes() const {
    const auto beforeJson = QJsonValue::fromVariant(beforeValue_);
    const auto afterJson = QJsonValue::fromVariant(afterValue_);
    const size_t valueBytes = static_cast<size_t>(
        QJsonDocument(QJsonObject{{QStringLiteral("value"), beforeJson},
                                   {QStringLiteral("other"), afterJson}})
            .toJson(QJsonDocument::Compact)
            .size());
    return sizeof(*this) +
           static_cast<size_t>(layerId_.size() + propertyPath_.size() +
                               label_.size()) * sizeof(QChar) +
           valueBytes;
}

bool SetLayerPropertyValueCommand::canSerialize() const {
    if (layerId_.isEmpty() || propertyPath_.isEmpty() || layer_.expired()) {
        return false;
    }
    return !QJsonValue::fromVariant(beforeValue_).isUndefined() &&
           !QJsonValue::fromVariant(afterValue_).isUndefined();
}

QJsonObject SetLayerPropertyValueCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("propertyPath"), propertyPath_},
                       {QStringLiteral("beforeValue"),
                        QJsonValue::fromVariant(beforeValue_)},
                       {QStringLiteral("afterValue"),
                        QJsonValue::fromVariant(afterValue_)},
                       {QStringLiteral("label"), label_}};
}

bool SetLayerPropertyValueCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    propertyPath_ = data.value(QStringLiteral("propertyPath")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    const auto before = data.value(QStringLiteral("beforeValue"));
    const auto after = data.value(QStringLiteral("afterValue"));
    if (before.isUndefined() || after.isUndefined()) {
        return false;
    }
    beforeValue_ = before.toVariant();
    afterValue_ = after.toVariant();
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !propertyPath_.isEmpty() &&
           !layer_.expired();
}

SetAudioDeClickRangesCommand::SetAudioDeClickRangesCommand(
    ArtifactAbstractLayerPtr layer,
    std::vector<std::pair<qint64, qint64>> beforeRanges,
    std::vector<std::pair<qint64, qint64>> afterRanges,
    QString label)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      beforeRanges_(std::move(beforeRanges)),
      afterRanges_(std::move(afterRanges)),
      label_(std::move(label)) {}

void SetAudioDeClickRangesCommand::undo() {
    lastOperationSucceeded_ = applyAudioDeClickRanges(layer_.lock(), beforeRanges_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetAudioDeClickRangesCommand::redo() {
    lastOperationSucceeded_ = applyAudioDeClickRanges(layer_.lock(), afterRanges_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString SetAudioDeClickRangesCommand::label() const {
    return label_;
}

size_t SetAudioDeClickRangesCommand::estimatedMemoryBytes() const {
    return sizeof(*this) +
           static_cast<size_t>(layerId_.size() + label_.size()) * sizeof(QChar) +
           (beforeRanges_.size() + afterRanges_.size()) * sizeof(std::pair<qint64, qint64>);
}

bool SetAudioDeClickRangesCommand::canSerialize() const {
    return !layerId_.isEmpty() && !layer_.expired();
}

QJsonObject SetAudioDeClickRangesCommand::serialize() const {
    const auto encode = [](const auto& ranges) {
        QJsonArray result;
        for (const auto& range : ranges) {
            result.append(QJsonArray{
                static_cast<qint64>(range.first),
                static_cast<qint64>(range.second)});
        }
        return result;
    };
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("before"), encode(beforeRanges_)},
                       {QStringLiteral("after"), encode(afterRanges_)},
                       {QStringLiteral("label"), label_}};
}

bool SetAudioDeClickRangesCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    const auto decode = [](const QJsonValue& value,
                           std::vector<std::pair<qint64, qint64>>& target) {
        if (!value.isArray()) {
            return false;
        }
        const auto encoded = value.toArray();
        if (encoded.size() > 100000) {
            return false;
        }
        target.clear();
        target.reserve(encoded.size());
        for (const auto& item : encoded) {
            if (!item.isArray()) {
                return false;
            }
            const auto range = item.toArray();
            if (range.size() != 2) {
                return false;
            }
            qint64 start = 0;
            qint64 end = 0;
            if (!jsonInteger(range.at(0), start) || !jsonInteger(range.at(1), end)) {
                return false;
            }
            if (start < 0 || end <= start) {
                return false;
            }
            target.emplace_back(start, end);
        }
        return true;
    };
    if (!decode(data.value(QStringLiteral("before")), beforeRanges_) ||
        !decode(data.value(QStringLiteral("after")), afterRanges_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

SetLayerPropertyExpressionCommand::SetLayerPropertyExpressionCommand(
    ArtifactAbstractLayerPtr layer,
    QString propertyPath,
    QString beforeExpression,
    QString afterExpression,
    QString label)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      propertyPath_(std::move(propertyPath)),
      beforeExpression_(std::move(beforeExpression)),
      afterExpression_(std::move(afterExpression)),
      label_(std::move(label)) {}

void SetLayerPropertyExpressionCommand::undo() {
    lastOperationSucceeded_ = applyLayerPropertyExpression(
        layer_.lock(), propertyPath_, beforeExpression_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerPropertyExpressionCommand::redo() {
    lastOperationSucceeded_ = applyLayerPropertyExpression(
        layer_.lock(), propertyPath_, afterExpression_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString SetLayerPropertyExpressionCommand::label() const {
    return label_;
}

size_t SetLayerPropertyExpressionCommand::estimatedMemoryBytes() const {
    return sizeof(*this) +
           static_cast<size_t>(layerId_.size() + propertyPath_.size() +
                               beforeExpression_.size() + afterExpression_.size() +
                               label_.size()) * sizeof(QChar);
}

QJsonObject SetLayerPropertyExpressionCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("propertyPath"), propertyPath_},
                       {QStringLiteral("beforeExpression"), beforeExpression_},
                       {QStringLiteral("afterExpression"), afterExpression_},
                       {QStringLiteral("label"), label_}};
}

bool SetLayerPropertyExpressionCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    propertyPath_ = data.value(QStringLiteral("propertyPath")).toString();
    beforeExpression_ = data.value(QStringLiteral("beforeExpression")).toString();
    afterExpression_ = data.value(QStringLiteral("afterExpression")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !propertyPath_.isEmpty() &&
           !layer_.expired();
}

bool SetLayerPropertyKeyframesCommand::canSerialize() const {
    if (layerId_.isEmpty() || propertyPath_.isEmpty() || layer_.expired()) return false;
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
            result.append(QJsonObject{
                {QStringLiteral("frame"),
                 static_cast<qint64>(keyframe.time.rescaledTo(30))},
                {QStringLiteral("timeValue"),
                 static_cast<qint64>(keyframe.time.value())},
                {QStringLiteral("timeScale"),
                 static_cast<qint64>(std::max<int64_t>(1, keyframe.time.scale()))},
                {QStringLiteral("value"), value},
                {QStringLiteral("interpolation"),
                 static_cast<int>(keyframe.interpolation)},
                {QStringLiteral("cp1_x"), keyframe.cp1_x},
                {QStringLiteral("cp1_y"), keyframe.cp1_y},
                {QStringLiteral("cp2_x"), keyframe.cp2_x},
                {QStringLiteral("cp2_y"), keyframe.cp2_y},
                {QStringLiteral("roving"), keyframe.roving},
                {QStringLiteral("anchor"), static_cast<int>(keyframe.anchor)},
                {QStringLiteral("colorLabel"),
                 static_cast<int>(keyframe.colorLabel)}});
        }
        return result;
    };
    data.insert(QStringLiteral("before"), encode(beforeKeyframes_));
    data.insert(QStringLiteral("after"), encode(afterKeyframes_));
    if (beforeAnimatable_.has_value()) {
        data.insert(QStringLiteral("beforeAnimatable"), *beforeAnimatable_);
    }
    if (afterAnimatable_.has_value()) {
        data.insert(QStringLiteral("afterAnimatable"), *afterAnimatable_);
    }
    return data;
}

bool SetLayerPropertyKeyframesCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    propertyPath_ = data.value(QStringLiteral("propertyPath")).toString();
    label_ = data.value(QStringLiteral("label")).toString();
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isArray() || !after.isArray() ||
        !decodeKeyframes(before.toArray(), beforeKeyframes_) ||
        !decodeKeyframes(after.toArray(), afterKeyframes_)) return false;
    if (data.contains(QStringLiteral("beforeAnimatable"))) {
        beforeAnimatable_ = data.value(QStringLiteral("beforeAnimatable")).toBool();
    } else {
        beforeAnimatable_.reset();
    }
    if (data.contains(QStringLiteral("afterAnimatable"))) {
        afterAnimatable_ = data.value(QStringLiteral("afterAnimatable")).toBool();
    } else {
        afterAnimatable_.reset();
    }
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
    lastOperationSucceeded_ = applyTextLayerTextSnapshot(layer_.lock(), beforeText_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetTextLayerTextCommand::redo() {
    lastOperationSucceeded_ = applyTextLayerTextSnapshot(layer_.lock(), afterText_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
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

// --- SetTextAnimatorStackCommand ---
SetTextAnimatorStackCommand::SetTextAnimatorStackCommand(
    ArtifactAbstractLayerPtr layer,
    QJsonArray beforeStack,
    QJsonArray afterStack,
    QString label)
    : layer_(layer),
      layerId_(layer ? layer->id().toQString() : QString()),
      beforeStack_(std::move(beforeStack)),
      afterStack_(std::move(afterStack)),
      label_(std::move(label)) {}

bool SetTextAnimatorStackCommand::apply(
    const QJsonArray& stack, const QJsonArray& compensationStack) {
    auto layer = layer_.lock();
    if (!layer) return false;
    if (auto textLayer =
            ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
        textLayer->restoreTextAnimatorStack(stack);
        if (textLayer->textAnimatorStackSnapshot() != stack) {
            textLayer->restoreTextAnimatorStack(compensationStack);
            return false;
        }
        if (auto* manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
        return true;
    }
    return false;
}

void SetTextAnimatorStackCommand::undo() {
    lastOperationSucceeded_ = apply(beforeStack_, afterStack_);
}

void SetTextAnimatorStackCommand::redo() {
    lastOperationSucceeded_ = apply(afterStack_, beforeStack_);
}

QString SetTextAnimatorStackCommand::label() const {
    return label_;
}

size_t SetTextAnimatorStackCommand::estimatedMemoryBytes() const {
    return sizeof(*this) +
           static_cast<size_t>(
               QJsonDocument(beforeStack_).toJson(QJsonDocument::Compact).size() +
               QJsonDocument(afterStack_).toJson(QJsonDocument::Compact).size());
}

QJsonObject SetTextAnimatorStackCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("beforeStack"), beforeStack_},
                       {QStringLiteral("afterStack"), afterStack_},
                       {QStringLiteral("label"), label_}};
}

bool SetTextAnimatorStackCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    const auto before = data.value(QStringLiteral("beforeStack"));
    const auto after = data.value(QStringLiteral("afterStack"));
    if (!before.isArray() || !after.isArray()) return false;
    beforeStack_ = before.toArray();
    afterStack_ = after.toArray();
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
    lastOperationSucceeded_ = applyEffectMaskImageSnapshot(effect_.lock(), beforeMasks_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void SetEffectMaskImagesCommand::redo() {
    lastOperationSucceeded_ = applyEffectMaskImageSnapshot(effect_.lock(), afterMasks_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
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
    const auto before = data.value(QStringLiteral("before"));
    const auto after = data.value(QStringLiteral("after"));
    if (!before.isArray() || !after.isArray()) return false;
    const auto decode = [](const QJsonArray& values, auto& masks) {
        masks.clear();
        for (const auto& value : values) {
            if (!value.isObject()) return false;
            auto image = decodeEffectMaskImage(value.toObject());
            if (!image) return false;
            masks.push_back(std::move(image));
        }
        return true;
    };
    if (!decode(before.toArray(), beforeMasks_) ||
        !decode(after.toArray(), afterMasks_)) return false;
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
    auto* manager = UndoManager::instance();
    auto composition = manager ? manager->resolveComposition(compositionId_)
                               : ArtifactCompositionPtr{};
    if (!composition) {
        auto* selection = ArtifactLayerSelectionManager::instance();
        composition = selection ? selection->activeComposition()
                                : ArtifactCompositionPtr{};
    }
    lastOperationSucceeded_ =
        applyAlignLayerSnapshots(composition, snapshots_, false);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void AlignLayersUndoCommand::redo() {
    auto* manager = UndoManager::instance();
    auto composition = manager ? manager->resolveComposition(compositionId_)
                               : ArtifactCompositionPtr{};
    if (!composition) {
        auto* selection = ArtifactLayerSelectionManager::instance();
        composition = selection ? selection->activeComposition()
                                : ArtifactCompositionPtr{};
    }
    lastOperationSucceeded_ =
        applyAlignLayerSnapshots(composition, snapshots_, true);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString AlignLayersUndoCommand::label() const { return label_; }

size_t AlignLayersUndoCommand::estimatedMemoryBytes() const {
    size_t bytes = sizeof(*this) + static_cast<size_t>(compositionId_.size() + label_.size()) * sizeof(QChar);
    for (const auto& snapshot : snapshots_) {
        bytes += sizeof(snapshot) + static_cast<size_t>(snapshot.layerId.size()) * sizeof(QChar);
    }
    return bytes;
}

bool AlignLayersUndoCommand::canSerialize() const {
    if (compositionId_.isEmpty() || snapshots_.empty()) {
        return false;
    }
    const auto finite = [](const AlignLayerSnapshot& snapshot) {
        return !snapshot.layerId.trimmed().isEmpty() &&
               std::isfinite(snapshot.beforeX) &&
               std::isfinite(snapshot.beforeY) &&
               std::isfinite(snapshot.afterX) &&
               std::isfinite(snapshot.afterY) &&
               std::isfinite(snapshot.beforeScaleX) &&
               std::isfinite(snapshot.beforeScaleY) &&
               std::isfinite(snapshot.afterScaleX) &&
               std::isfinite(snapshot.afterScaleY);
    };
    return std::all_of(snapshots_.begin(), snapshots_.end(), finite);
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
    const auto encoded = data.value(QStringLiteral("snapshots"));
    if (!encoded.isArray() || encoded.toArray().isEmpty() ||
        encoded.toArray().size() > 100000) {
        return false;
    }
    snapshots_.clear();
    snapshots_.reserve(encoded.toArray().size());
    for (const auto& value : encoded.toArray()) {
        if (!value.isObject()) {
            return false;
        }
        const auto object = value.toObject();
        AlignLayerSnapshot snapshot{};
        if (!object.value(QStringLiteral("layerId")).isString() ||
            !finiteJsonNumber(object, QStringLiteral("beforeX"), snapshot.beforeX) ||
            !finiteJsonNumber(object, QStringLiteral("beforeY"), snapshot.beforeY) ||
            !finiteJsonNumber(object, QStringLiteral("afterX"), snapshot.afterX) ||
            !finiteJsonNumber(object, QStringLiteral("afterY"), snapshot.afterY)) {
            return false;
        }
        const auto decodeScale = [&object](const QString& key, float& target) {
            if (!object.contains(key)) {
                target = 1.0f;
                return true;
            }
            return finiteJsonNumber(object, key, target);
        };
        if (!decodeScale(QStringLiteral("beforeScaleX"), snapshot.beforeScaleX) ||
            !decodeScale(QStringLiteral("beforeScaleY"), snapshot.beforeScaleY) ||
            !decodeScale(QStringLiteral("afterScaleX"), snapshot.afterScaleX) ||
            !decodeScale(QStringLiteral("afterScaleY"), snapshot.afterScaleY)) {
            return false;
        }
        snapshot.layerId = object.value(QStringLiteral("layerId")).toString();
        if (snapshot.layerId.trimmed().isEmpty()) {
            return false;
        }
        snapshots_.push_back(std::move(snapshot));
    }
    auto* manager = UndoManager::instance();
    return manager && canSerialize() &&
           static_cast<bool>(manager->resolveComposition(compositionId_));
}

// --- SetLayerVisibilityCommand ---
SetLayerVisibilityCommand::SetLayerVisibilityCommand(ArtifactAbstractLayerPtr layer, bool visible)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldVisible_(layer ? layer->isVisible() : true), newVisible_(visible) {}

void SetLayerVisibilityCommand::undo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setVisible(oldVisible_);
        lastOperationSucceeded_ = layer->isVisible() == oldVisible_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerVisibilityCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setVisible(newVisible_);
        lastOperationSucceeded_ = layer->isVisible() == newVisible_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    const auto oldVisible = data.value(QStringLiteral("oldVisible"));
    const auto newVisible = data.value(QStringLiteral("newVisible"));
    if (!oldVisible.isBool() || !newVisible.isBool()) return false;
    oldVisible_ = oldVisible.toBool();
    newVisible_ = newVisible.toBool();
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setLocked(oldLocked_);
        lastOperationSucceeded_ = layer->isLocked() == oldLocked_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerLockCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setLocked(newLocked_);
        lastOperationSucceeded_ = layer->isLocked() == newLocked_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    const auto oldLocked = data.value(QStringLiteral("oldLocked"));
    const auto newLocked = data.value(QStringLiteral("newLocked"));
    if (!oldLocked.isBool() || !newLocked.isBool()) return false;
    oldLocked_ = oldLocked.toBool();
    newLocked_ = newLocked.toBool();
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setSolo(oldSolo_);
        lastOperationSucceeded_ = layer->isSolo() == oldSolo_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerSoloCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setSolo(newSolo_);
        lastOperationSucceeded_ = layer->isSolo() == newSolo_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    const auto oldSolo = data.value(QStringLiteral("oldSolo"));
    const auto newSolo = data.value(QStringLiteral("newSolo"));
    if (!oldSolo.isBool() || !newSolo.isBool()) return false;
    oldSolo_ = oldSolo.toBool();
    newSolo_ = newSolo.toBool();
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setShy(oldShy_);
        lastOperationSucceeded_ = layer->isShy() == oldShy_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void SetLayerShyCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setShy(newShy_);
        lastOperationSucceeded_ = layer->isShy() == newShy_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    const auto oldShy = data.value(QStringLiteral("oldShy"));
    const auto newShy = data.value(QStringLiteral("newShy"));
    if (!oldShy.isBool() || !newShy.isBool()) return false;
    oldShy_ = oldShy.toBool();
    newShy_ = newShy.toBool();
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setBlendMode(oldMode_);
        lastOperationSucceeded_ = layer->layerBlendType() == oldMode_;
        if (!lastOperationSucceeded_) return;
        layer->changed();
        notifyLayerBlendModeChanged(layer);
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void ChangeLayerBlendModeCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setBlendMode(newMode_);
        lastOperationSucceeded_ = layer->layerBlendType() == newMode_;
        if (!lastOperationSucceeded_) return;
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
    if (children_.empty()) {
        lastOperationSucceeded_ = false;
        return;
    }
    lastOperationSucceeded_ = true;
    size_t undone = 0;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (!*it) {
            continue;
        }
        (*it)->undo();
        if (!(*it)->lastOperationSucceeded()) {
            // Restore the failed child's post-undo state before restoring the
            // children that were already undone below it.
            if (!(*it)->handlesFailedOperationCompensation()) {
                (*it)->redo();
            }
            for (size_t i = children_.size() - undone; i < children_.size(); ++i) {
                if (children_[i]) {
                    children_[i]->redo();
                }
            }
            lastOperationSucceeded_ = false;
            return;
        }
        ++undone;
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

void MacroUndoCommand::redo() {
    if (children_.empty()) {
        lastOperationSucceeded_ = false;
        return;
    }
    lastOperationSucceeded_ = true;
    size_t applied = 0;
    for (auto& child : children_) {
        if (!child) {
            continue;
        }
        child->redo();
        if (!child->lastOperationSucceeded()) {
            // The failed child may have mutated state before reporting its
            // postcondition failure.  Let the child compensate that partial
            // redo before rolling back the children that completed earlier.
            if (!child->handlesFailedOperationCompensation()) {
                child->undo();
            }
            while (applied > 0) {
                --applied;
                if (children_[applied]) {
                    children_[applied]->undo();
                }
            }
            lastOperationSucceeded_ = false;
            return;
        }
        ++applied;
    }
    if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
}

QString MacroUndoCommand::label() const {
    return label_;
}

bool MacroUndoCommand::canSerialize() const {
    return !children_.empty() && std::all_of(children_.begin(), children_.end(),
        [](const auto& child) { return child && child->canSerialize() && !child->isOffloaded(); });
}

QJsonObject MacroUndoCommand::serialize() const {
    if (!canSerialize()) return {};
    QJsonObject data;
    data.insert(QStringLiteral("label"), label_);
    QJsonArray children;
    for (const auto& child : children_) {
        QJsonObject entry;
        entry.insert(QStringLiteral("type"), child->commandType());
        entry.insert(QStringLiteral("data"), child->serialize());
        if (entry.value(QStringLiteral("type")).toString().isEmpty() ||
            !entry.value(QStringLiteral("data")).isObject() ||
            entry.value(QStringLiteral("data")).toObject().isEmpty()) {
            return {};
        }
        children.append(entry);
    }
    data.insert(QStringLiteral("children"), children);
    return data;
}

bool MacroUndoCommand::deserialize(const QJsonObject& data) {
    label_ = data.value(QStringLiteral("label")).toString();
    const auto childrenValue = data.value(QStringLiteral("children"));
    if (!childrenValue.isArray()) return false;
    if (childrenValue.toArray().isEmpty()) return false;
    children_.clear();
    for (const auto& value : childrenValue.toArray()) {
        if (!value.isObject()) return false;
        const auto entry = value.toObject();
        if (!entry.value(QStringLiteral("type")).isString() ||
            !entry.value(QStringLiteral("data")).isObject()) return false;
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
        if (!child) continue;
        const size_t bytes = child->estimatedMemoryBytes();
        if (bytes > std::numeric_limits<size_t>::max() - total) {
            return std::numeric_limits<size_t>::max();
        }
        total += bytes;
    }
    return total;
}

// --- MoveAssetFileCommand ---
MoveAssetFileCommand::MoveAssetFileCommand(const QString& oldPath,
                                           const QString& newPath)
    : oldPath_(oldPath), newPath_(newPath) {}

void MoveAssetFileCommand::undo() {
    lastOperationSucceeded_ = QFile::rename(newPath_, oldPath_);
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void MoveAssetFileCommand::redo() {
    lastOperationSucceeded_ = QFile::rename(oldPath_, newPath_);
    if (lastOperationSucceeded_) {
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
        QStringLiteral("EffectPresetSnapshotCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<EffectPresetSnapshotCommand>(
                ArtifactAbstractEffectPtr{}, QJsonObject{}, QJsonObject{},
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetEffectPropertyKeyframesCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetEffectPropertyKeyframesCommand>(
                ArtifactAbstractEffectPtr{}, QString(),
                std::vector<ArtifactCore::KeyFrame>{},
                std::vector<ArtifactCore::KeyFrame>{},
                 data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("EffectModulationSnapshotCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<EffectModulationSnapshotCommand>(
                ArtifactAbstractEffectPtr{},
                Audio::Modulation::ModulationRouterSnapshot{},
                Audio::Modulation::ModulationRouterSnapshot{},
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("LayerModulationSnapshotCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<LayerModulationSnapshotCommand>(
                ArtifactAbstractLayerPtr{},
                Audio::Modulation::ModulationRouterSnapshot{},
                Audio::Modulation::ModulationRouterSnapshot{},
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetEffectPropertyExpressionCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetEffectPropertyExpressionCommand>(
                ArtifactAbstractEffectPtr{}, QString(), QString(), QString(),
                data.value(QStringLiteral("label")).toString());
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
        QStringLiteral("SetLayerPropertyValueCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetLayerPropertyValueCommand>(
                ArtifactAbstractLayerPtr{}, QString(), QVariant(), QVariant());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetAudioDeClickRangesCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetAudioDeClickRangesCommand>(
                ArtifactAbstractLayerPtr{},
                std::vector<std::pair<qint64, qint64>>{},
                std::vector<std::pair<qint64, qint64>>{},
                data.value(QStringLiteral("label")).toString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetLayerPropertyExpressionCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetLayerPropertyExpressionCommand>(
                ArtifactAbstractLayerPtr{}, QString(), QString(), QString(),
                data.value(QStringLiteral("label")).toString());
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
        QStringLiteral("SetCompositionNoteCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetCompositionNoteCommand>(
                ArtifactCompositionPtr{}, QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetCompositionSettingsCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetCompositionSettingsCommand>(
                ArtifactCompositionPtr{}, QSize{}, 0.0f, 0, 0, FloatColor{},
                QSize{}, 0.0f, 0, 0, FloatColor{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetCompositionResponsiveLayoutCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetCompositionResponsiveLayoutCommand>(
                ArtifactCompositionPtr{}, QJsonObject{}, QJsonObject{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("RenameCompositionCommand"),
        [](const QJsonObject&) {
            return std::make_unique<RenameCompositionCommand>(
                ArtifactCompositionPtr{}, QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeLayerParentCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeLayerParentCommand>(
                ArtifactAbstractLayerPtr{}, LayerID{}, LayerID{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("RenameProjectItemCommand"),
        [](const QJsonObject&) {
            return std::make_unique<RenameProjectItemCommand>(
                nullptr, QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetProjectItemTagsCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetProjectItemTagsCommand>(
                nullptr, QStringList{}, QStringList{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("SetFootageAssetRoleCommand"),
        [](const QJsonObject&) {
            return std::make_unique<SetFootageAssetRoleCommand>(
                nullptr, ProjectAssetUsage::Production,
                ProjectRenderInputRole::Generic, ProjectAssetUsage::Production,
                ProjectRenderInputRole::Generic);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MoveProjectItemCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MoveProjectItemCommand>(nullptr, nullptr);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("CreateProjectFolderCommand"),
        [](const QJsonObject&) {
            return std::make_unique<CreateProjectFolderCommand>(
                QString(), QString(), QString());
        });
    impl_->commandFactories_.insert(
        QStringLiteral("AddProjectItemsCommand"),
        [](const QJsonObject&) {
            return std::make_unique<AddProjectItemsCommand>(QJsonArray{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("RemoveProjectItemCommand"),
        [](const QJsonObject&) {
            return std::make_unique<RemoveProjectItemCommand>(
                nullptr, QJsonObject{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("MoveMaskCommand"),
        [](const QJsonObject&) {
            return std::make_unique<MoveMaskCommand>(
                ArtifactAbstractLayerPtr{}, -1, -1);
        });
    impl_->commandFactories_.insert(
        QStringLiteral("ChangeCompositionResolutionCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ChangeCompositionResolutionCommand>(
                ArtifactCompositionPtr{}, QSize{}, QSize{},
                ArtifactCore::RemapPolicy::CenterLocked);
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
        QStringLiteral("SetTextAnimatorStackCommand"),
        [](const QJsonObject& data) {
            return std::make_unique<SetTextAnimatorStackCommand>(
                ArtifactAbstractLayerPtr{}, QJsonArray{}, QJsonArray{},
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
        QStringLiteral("AddLayerEffectCommand"),
        [](const QJsonObject&) {
            return std::make_unique<AddLayerEffectCommand>(
                ArtifactAbstractLayerPtr{}, ArtifactAbstractEffectPtr{});
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
        QStringLiteral("ClonerTransformStackSnapshotCommand"),
        [](const QJsonObject&) {
            return std::make_unique<ClonerTransformStackSnapshotCommand>(
                ArtifactAbstractLayerPtr{}, QJsonArray{}, QJsonArray{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("LayerComponentDescriptorSnapshotCommand"),
        [](const QJsonObject&) {
            return std::make_unique<LayerComponentDescriptorSnapshotCommand>(
                ArtifactAbstractLayerPtr{}, QJsonObject{}, QJsonObject{});
        });
    impl_->commandFactories_.insert(
        QStringLiteral("CloneEffectorStackSnapshotCommand"),
        [](const QJsonObject&) {
            return std::make_unique<CloneEffectorStackSnapshotCommand>(
                ArtifactAbstractLayerPtr{}, QJsonArray{}, QJsonArray{});
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
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::PropertyChanged, effectId});
}

void UndoManager::notifyAnythingChanged() {
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::AnythingChanged, {}});
}

UndoManager* UndoManager::instance() {
    static UndoManager inst;
    return &inst;
}

bool UndoManager::push(std::unique_ptr<UndoCommand> cmd) {
    if (!cmd) return false;
    const size_t estimatedBytes = cmd->estimatedMemoryBytes();
    if (impl_->budget_.maxEntryCount == 0 ||
        estimatedBytes > impl_->budget_.maxSingleEntryBytes ||
        impl_->budget_.maxMemoryBytes == 0 ||
        estimatedBytes > impl_->budget_.maxMemoryBytes) {
        // Do not execute a command that cannot be retained under the active
        // budget; callers can inspect the unchanged history and retry after
        // changing the policy.
        return false;
    }
    // Execute immediately and record for undo
    cmd->redo();
    if (!cmd->lastOperationSucceeded()) {
        // A command may have applied part of its change before reporting a
        // postcondition failure.  Compensate the initial redo before
        // discarding the command so a rejected push cannot leave project
        // state outside the history.
        // MacroUndoCommand already compensates its failed child and all
        // previously applied children inside redo().  Calling undo() again
        // would also touch children that were never applied.
        if (!cmd->handlesFailedOperationCompensation()) {
            cmd->undo();
        }
        return false;
    }

    impl_->undoStack.push_back(std::move(cmd));
    impl_->undoStateIds_.push_back(impl_->allocateStateId());
    impl_->redoStack.clear();
    impl_->redoStateIds_.clear();
    impl_->version_ = impl_->undoStateIds_.back();
    impl_->enforceBudget();
    impl_->applyOffloadPolicy();
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
    return true;
}

void UndoManager::undo() {
    if (impl_->undoStack.empty()) return;
    impl_->alignUndoStateIds();
    auto cmd = std::move(impl_->undoStack.back());
    impl_->undoStack.pop_back();
    const int64_t stateId = impl_->undoStateIds_.back();
    impl_->undoStateIds_.pop_back();
    cmd->undo();
    if (!cmd->lastOperationSucceeded()) {
        // A failed undo can also be a partial mutation.  Restore the state
        // represented by the command before returning it to the undo stack.
        if (!cmd->handlesFailedOperationCompensation()) {
            cmd->redo();
        }
        impl_->undoStack.push_back(std::move(cmd));
        impl_->undoStateIds_.push_back(stateId);
        return;
    }
    impl_->redoStack.push_back(std::move(cmd));
    impl_->redoStateIds_.push_back(stateId);
    impl_->version_ = impl_->undoStateIds_.empty()
                          ? 0
                          : impl_->undoStateIds_.back();
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
}

void UndoManager::redo() {
    if (impl_->redoStack.empty()) return;
    const int64_t stateId = impl_->redoStateIds_.empty()
                                ? impl_->allocateStateId()
                                : impl_->redoStateIds_.back();
    if (!impl_->redoStateIds_.empty()) {
        impl_->redoStateIds_.pop_back();
    }
    auto cmd = std::move(impl_->redoStack.back());
    impl_->redoStack.pop_back();
    cmd->redo();
    if (!cmd->lastOperationSucceeded()) {
        if (!cmd->handlesFailedOperationCompensation()) {
            cmd->undo();
        }
        impl_->redoStack.push_back(std::move(cmd));
        impl_->redoStateIds_.push_back(stateId);
        return;
    }
    impl_->undoStack.push_back(std::move(cmd));
    impl_->undoStateIds_.push_back(stateId);
    impl_->version_ = stateId;
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
}

bool UndoManager::canUndo() const { return !impl_->undoStack.empty(); }
bool UndoManager::canRedo() const { return !impl_->redoStack.empty(); }

void UndoManager::clearHistory() {
    impl_->cleanupOffloadFiles();
    impl_->undoStack.clear();
    impl_->redoStack.clear();
    impl_->undoStateIds_.clear();
    impl_->redoStateIds_.clear();
    impl_->nextStateId_ = 1;
    impl_->version_ = 0;
    impl_->savedVersion_ = 0;
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
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
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
}

size_t UndoManager::maxHistorySize() const { return impl_->maxHistorySize_; }
void UndoManager::setBudget(const UndoBudget& budget) {
    impl_->budget_ = budget;
    impl_->budget_.maxEntryCount = std::max<size_t>(1, impl_->budget_.maxEntryCount);
    impl_->enforceBudget();
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
}
const UndoManager::UndoBudget& UndoManager::budget() const { return impl_->budget_; }
size_t UndoManager::currentMemoryBytes() const {
    return impl_->allStackBytes();
}
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
    root.insert(QStringLiteral("currentVersion"), static_cast<qint64>(impl_->version_));
    QJsonArray entries;
    for (const auto& command : impl_->undoStack) {
        if (!command || !command->canSerialize()) return false;
        const QString type = command->commandType();
        const QString label = command->label();
        if (type.isEmpty() || type.size() > 256 || label.size() > 4096) return false;
        const size_t estimatedBytes = command->estimatedMemoryBytes();
        if (estimatedBytes > static_cast<size_t>(std::numeric_limits<qint64>::max())) {
            return false;
        }
        QJsonObject entry;
        entry.insert(QStringLiteral("type"), type);
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("estimatedBytes"), static_cast<qint64>(estimatedBytes));
        const QJsonObject data = command->serialize();
        if (data.isEmpty()) return false;
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
    if (type.isEmpty()) return nullptr;
    const auto factory = impl_->commandFactories_.find(type);
    if (factory == impl_->commandFactories_.end()) return nullptr;
    auto command = factory.value()(data);
    if (!command || !command->deserialize(data) || command->commandType() != type ||
        !command->canSerialize() ||
        command->estimatedMemoryBytes() > impl_->budget_.maxSingleEntryBytes ||
        impl_->budget_.maxMemoryBytes == 0 ||
        command->estimatedMemoryBytes() > impl_->budget_.maxMemoryBytes) return nullptr;
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
    if (!nonNegativeJsonInteger(root.value(QStringLiteral("version"))) ||
        root.value(QStringLiteral("version")).toInteger(-1) != 1 ||
        !root.value(QStringLiteral("entries")).isArray()) return false;
    const QJsonValue savedVersionValue = root.value(QStringLiteral("savedVersion"));
    const QJsonValue currentVersionValue = root.value(QStringLiteral("currentVersion"));
    if ((!savedVersionValue.isUndefined() && !nonNegativeJsonInteger(savedVersionValue)) ||
        (!currentVersionValue.isUndefined() && !nonNegativeJsonInteger(currentVersionValue))) return false;
    const int64_t savedVersion = savedVersionValue.isUndefined()
        ? 0 : savedVersionValue.toInteger(-1);
    const int64_t restoredVersion = currentVersionValue.isUndefined()
        ? savedVersion : currentVersionValue.toInteger(-1);
    if (savedVersion < 0 || restoredVersion < 0 ||
        savedVersion > restoredVersion ||
        savedVersion == std::numeric_limits<int64_t>::max() ||
        restoredVersion == std::numeric_limits<int64_t>::max()) return false;
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    if (entries.size() > static_cast<qsizetype>(impl_->budget_.maxEntryCount)) return false;
    std::vector<std::unique_ptr<UndoCommand>> restored;
    restored.reserve(entries.size());
    for (const auto& value : entries) {
        if (!value.isObject()) return false;
        const QJsonObject entry = value.toObject();
        if (!entry.value(QStringLiteral("type")).isString() ||
            !entry.value(QStringLiteral("label")).isString()) return false;
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString label = entry.value(QStringLiteral("label")).toString();
        if (type.isEmpty() || type.size() > 256 || label.size() > 4096 ||
            !entry.value(QStringLiteral("data")).isObject()) return false;
        const QJsonValue estimatedBytes = entry.value(QStringLiteral("estimatedBytes"));
        if (!estimatedBytes.isUndefined() &&
            !nonNegativeJsonInteger(estimatedBytes)) return false;
        auto command = createCommand(type, entry.value(QStringLiteral("data")).toObject());
        if (!command) return false;
        if (command->label() != label) return false;
        if (command->estimatedMemoryBytes() >
            static_cast<size_t>(std::numeric_limits<qint64>::max())) return false;
        if (!estimatedBytes.isUndefined() &&
            estimatedBytes.toInteger(-1) !=
                static_cast<qint64>(command->estimatedMemoryBytes())) {
            return false;
        }
        restored.push_back(std::move(command));
    }
    impl_->undoStack = std::move(restored);
    impl_->redoStack.clear();
    impl_->undoStateIds_.clear();
    impl_->redoStateIds_.clear();
    impl_->savedVersion_ = savedVersion;
    impl_->nextStateId_ = std::max<int64_t>(
        1, std::max(impl_->savedVersion_, restoredVersion) + 1);
    impl_->alignUndoStateIds();
    if (!impl_->undoStateIds_.empty()) {
        impl_->undoStateIds_.back() = restoredVersion;
    }
    impl_->version_ = restoredVersion;
    impl_->enforceBudget();
    ArtifactCore::globalEventBus().publish<UndoManagerChangedEvent>(
        {UndoManagerChangeKind::HistoryChanged, {}});
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
    lastOperationSucceeded_ = false;
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (!comp || !layer || oldIndex_ < 0 || oldIndex_ >= comp->layerCount()) return;
    comp->moveLayerToIndex(layer->id(), oldIndex_);
    lastOperationSucceeded_ = comp->allLayerRef().indexOf(layer) == oldIndex_;
}

void MoveLayerIndexCommand::redo() {
    lastOperationSucceeded_ = false;
    auto comp = comp_.lock();
    auto layer = layer_.lock();
    if (!comp || !layer || newIndex_ < 0 || newIndex_ >= comp->layerCount()) return;
    comp->moveLayerToIndex(layer->id(), newIndex_);
    lastOperationSucceeded_ = comp->allLayerRef().indexOf(layer) == newIndex_;
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
    if (!nonNegativeJsonInt(data.value(QStringLiteral("oldIndex")), oldIndex_) ||
        !nonNegativeJsonInt(data.value(QStringLiteral("newIndex")), newIndex_)) return false;
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setLayerName(oldName_);
        lastOperationSucceeded_ = layer->layerName() == oldName_;
    }
}

void RenameLayerCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (layer) {
        layer->setLayerName(newName_);
        lastOperationSucceeded_ = layer->layerName() == newName_;
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

// --- SetCompositionNoteCommand ---
SetCompositionNoteCommand::SetCompositionNoteCommand(
    ArtifactCompositionPtr composition, QString oldNote, QString newNote)
    : composition_(composition),
      compositionId_(composition ? composition->id().toString() : QString()),
      oldNote_(std::move(oldNote)), newNote_(std::move(newNote)) {}

void SetCompositionNoteCommand::undo() {
    lastOperationSucceeded_ = false;
    if (auto composition = composition_.lock()) {
        composition->setCompositionNote(oldNote_);
        lastOperationSucceeded_ = composition->compositionNote() == oldNote_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

void SetCompositionNoteCommand::redo() {
    lastOperationSucceeded_ = false;
    if (auto composition = composition_.lock()) {
        composition->setCompositionNote(newNote_);
        lastOperationSucceeded_ = composition->compositionNote() == newNote_;
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
    }
}

QString SetCompositionNoteCommand::label() const {
    return QStringLiteral("Set Composition Note");
}

size_t SetCompositionNoteCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        compositionId_.size() + oldNote_.size() + newNote_.size()) * sizeof(QChar);
}

QJsonObject SetCompositionNoteCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("oldNote"), oldNote_},
                       {QStringLiteral("newNote"), newNote_}};
}

bool SetCompositionNoteCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    oldNote_ = data.value(QStringLiteral("oldNote")).toString();
    newNote_ = data.value(QStringLiteral("newNote")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    composition_ = manager->resolveComposition(compositionId_);
    return !compositionId_.isEmpty() && !composition_.expired();
}

// --- RenameCompositionCommand ---
RenameCompositionCommand::RenameCompositionCommand(
    ArtifactCompositionPtr composition, QString oldName, QString newName)
    : composition_(composition),
      compositionId_(composition ? composition->id().toString() : QString()),
      oldName_(std::move(oldName)), newName_(std::move(newName)) {}

void RenameCompositionCommand::undo() {
    lastOperationSucceeded_ =
        applyCompositionDisplayNameForUndo(composition_.lock(), oldName_);
}

void RenameCompositionCommand::redo() {
    lastOperationSucceeded_ =
        applyCompositionDisplayNameForUndo(composition_.lock(), newName_);
}

QString RenameCompositionCommand::label() const {
    return QStringLiteral("Rename Composition: %1 → %2")
        .arg(oldName_, newName_);
}

size_t RenameCompositionCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        compositionId_.size() + oldName_.size() + newName_.size()) * sizeof(QChar);
}

QJsonObject RenameCompositionCommand::serialize() const {
    return QJsonObject{{QStringLiteral("compositionId"), compositionId_},
                       {QStringLiteral("oldName"), oldName_},
                       {QStringLiteral("newName"), newName_}};
}

bool RenameCompositionCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    oldName_ = data.value(QStringLiteral("oldName")).toString();
    newName_ = data.value(QStringLiteral("newName")).toString();
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    composition_ = manager->resolveComposition(compositionId_);
    return !compositionId_.isEmpty() && !composition_.expired();
}

// --- ChangeLayerParentCommand ---
ChangeLayerParentCommand::ChangeLayerParentCommand(
    ArtifactAbstractLayerPtr layer, LayerID oldParentId, LayerID newParentId)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldParentId_(oldParentId), newParentId_(newParentId) {}

namespace {
bool applyLayerParent(const ArtifactAbstractLayerPtr& layer,
                      const LayerID& parentId) {
    if (!layer) {
        return false;
    }
    if (parentId.isNil()) {
        layer->clearParent();
    } else {
        layer->setParentById(parentId);
    }
    return layer->parentLayerId() == parentId;
}
}

void ChangeLayerParentCommand::undo() {
    const auto layer = layer_.lock();
    lastOperationSucceeded_ = applyLayerParent(layer, oldParentId_);
    if (lastOperationSucceeded_) {
        notifyLayerTransformChanged(layer);
    }
    if (lastOperationSucceeded_) {
      if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
}

void ChangeLayerParentCommand::redo() {
    const auto layer = layer_.lock();
    lastOperationSucceeded_ = applyLayerParent(layer, newParentId_);
    if (lastOperationSucceeded_) {
        notifyLayerTransformChanged(layer);
    }
    if (lastOperationSucceeded_) {
      if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
      }
    }
}

QString ChangeLayerParentCommand::label() const {
    return newParentId_.isNil() ? QStringLiteral("Clear Layer Parent")
                                : QStringLiteral("Set Layer Parent");
}

size_t ChangeLayerParentCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

QJsonObject ChangeLayerParentCommand::serialize() const {
    return QJsonObject{{QStringLiteral("layerId"), layerId_},
                       {QStringLiteral("oldParentId"), oldParentId_.toString()},
                       {QStringLiteral("newParentId"), newParentId_.toString()}};
}

bool ChangeLayerParentCommand::deserialize(const QJsonObject& data) {
    layerId_ = data.value(QStringLiteral("layerId")).toString();
    oldParentId_ = LayerID(data.value(QStringLiteral("oldParentId")).toString());
    newParentId_ = LayerID(data.value(QStringLiteral("newParentId")).toString());
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    layer_ = manager->resolveLayer(layerId_);
    return !layerId_.isEmpty() && !layer_.expired();
}

// --- Project item commands ---
namespace {
bool applyProjectItemNameForUndo(const QString& itemId, const QString& name) {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId);
    if (!item) {
        return false;
    }
    item->name = UniString::fromQString(name);
    if (item->name.toQString() != name) {
        return false;
    }
    project->projectChanged();
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
        {QString(), QString()});
    if (auto* undo = UndoManager::instance()) {
        undo->notifyAnythingChanged();
    }
    return true;
}

bool applyProjectItemParentForUndo(const QString& itemId,
                                   const QString& parentId) {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId);
    auto* parent = findProjectItemInTreeForUndo(project->projectItems(), parentId);
    if (!item || !parent || parent->type() != eProjectItemType::Folder) {
        return false;
    }
    if (!project->moveItem(item, parent)) {
        return false;
    }
    if (item->parent != parent) {
        return false;
    }
    if (auto* undo = UndoManager::instance()) {
        undo->notifyAnythingChanged();
    }
    return true;
}

bool applyProjectItemTagsForUndo(const QString& itemId,
                                 const QStringList& tags) {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId);
    if (!item) {
        return false;
    }
    item->tags = tags;
    if (item->tags != tags) {
        return false;
    }
    project->projectChanged();
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
        {QString(), QString()});
    if (auto* undo = UndoManager::instance()) {
        undo->notifyAnythingChanged();
    }
    return true;
}

bool applyFootageAssetRoleForUndo(const QString& itemId,
                                  ProjectAssetUsage usage,
                                  ProjectRenderInputRole role) {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    if (!project) return false;
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId);
    if (!item || item->type() != eProjectItemType::Footage) return false;
    auto* footage = static_cast<FootageItem*>(item);
    footage->assetUsage = usage;
    footage->renderInputRole = role;
    if (footage->assetUsage != usage || footage->renderInputRole != role) {
        return false;
    }
    project->projectChanged();
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>(
        {QString(), QString()});
    if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    return true;
}
}

RenameProjectItemCommand::RenameProjectItemCommand(
    ProjectItem* item, QString oldName, QString newName)
    : itemId_(item ? item->id.toString() : QString()),
      oldName_(std::move(oldName)), newName_(std::move(newName)) {}

void RenameProjectItemCommand::undo() {
    lastOperationSucceeded_ = applyProjectItemNameForUndo(itemId_, oldName_);
}

void RenameProjectItemCommand::redo() {
    lastOperationSucceeded_ = applyProjectItemNameForUndo(itemId_, newName_);
}

QString RenameProjectItemCommand::label() const {
    return QStringLiteral("Rename Project Item: %1 → %2")
        .arg(oldName_, newName_);
}

size_t RenameProjectItemCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        itemId_.size() + oldName_.size() + newName_.size()) * sizeof(QChar);
}

QJsonObject RenameProjectItemCommand::serialize() const {
    return QJsonObject{{QStringLiteral("itemId"), itemId_},
                       {QStringLiteral("oldName"), oldName_},
                       {QStringLiteral("newName"), newName_}};
}

bool RenameProjectItemCommand::deserialize(const QJsonObject& data) {
    itemId_ = data.value(QStringLiteral("itemId")).toString();
    oldName_ = data.value(QStringLiteral("oldName")).toString();
    newName_ = data.value(QStringLiteral("newName")).toString();
    return !itemId_.isEmpty();
}

SetProjectItemTagsCommand::SetProjectItemTagsCommand(
    ProjectItem* item, QStringList oldTags, QStringList newTags)
    : itemId_(item ? item->id.toString() : QString()),
      oldTags_(std::move(oldTags)), newTags_(std::move(newTags)) {}

void SetProjectItemTagsCommand::undo() {
    lastOperationSucceeded_ = applyProjectItemTagsForUndo(itemId_, oldTags_);
}

void SetProjectItemTagsCommand::redo() {
    lastOperationSucceeded_ = applyProjectItemTagsForUndo(itemId_, newTags_);
}

QString SetProjectItemTagsCommand::label() const {
    return QStringLiteral("Edit Project Item Tags");
}

size_t SetProjectItemTagsCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        itemId_.size() + oldTags_.join(QStringLiteral("\n")).size() +
        newTags_.join(QStringLiteral("\n")).size()) * sizeof(QChar);
}

QJsonObject SetProjectItemTagsCommand::serialize() const {
    return QJsonObject{{QStringLiteral("itemId"), itemId_},
                       {QStringLiteral("oldTags"), QJsonArray::fromStringList(oldTags_)},
                       {QStringLiteral("newTags"), QJsonArray::fromStringList(newTags_)}};
}

bool SetProjectItemTagsCommand::deserialize(const QJsonObject& data) {
    itemId_ = data.value(QStringLiteral("itemId")).toString();
    oldTags_.clear();
    newTags_.clear();
    for (const auto& value : data.value(QStringLiteral("oldTags")).toArray()) {
        if (value.isString()) oldTags_.append(value.toString());
    }
    for (const auto& value : data.value(QStringLiteral("newTags")).toArray()) {
        if (value.isString()) newTags_.append(value.toString());
    }
    return !itemId_.isEmpty();
}

SetFootageAssetRoleCommand::SetFootageAssetRoleCommand(
    FootageItem* item, ProjectAssetUsage oldUsage,
    ProjectRenderInputRole oldRole, ProjectAssetUsage newUsage,
    ProjectRenderInputRole newRole)
    : itemId_(item ? item->id.toString() : QString()), oldUsage_(oldUsage),
      oldRole_(oldRole), newUsage_(newUsage), newRole_(newRole) {}

void SetFootageAssetRoleCommand::undo() {
    lastOperationSucceeded_ = applyFootageAssetRoleForUndo(itemId_, oldUsage_, oldRole_);
}

void SetFootageAssetRoleCommand::redo() {
    lastOperationSucceeded_ = applyFootageAssetRoleForUndo(itemId_, newUsage_, newRole_);
}

QString SetFootageAssetRoleCommand::label() const {
    return QStringLiteral("Set Footage Input Source Role");
}

size_t SetFootageAssetRoleCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(itemId_.size()) * sizeof(QChar);
}

QJsonObject SetFootageAssetRoleCommand::serialize() const {
    return QJsonObject{{QStringLiteral("itemId"), itemId_},
                       {QStringLiteral("oldUsage"), static_cast<int>(oldUsage_)},
                       {QStringLiteral("oldRole"), static_cast<int>(oldRole_)},
                       {QStringLiteral("newUsage"), static_cast<int>(newUsage_)},
                       {QStringLiteral("newRole"), static_cast<int>(newRole_)}};
}

bool SetFootageAssetRoleCommand::deserialize(const QJsonObject& data) {
    itemId_ = data.value(QStringLiteral("itemId")).toString();
    const int oldUsage = data.value(QStringLiteral("oldUsage")).toInt(-1);
    const int oldRole = data.value(QStringLiteral("oldRole")).toInt(-1);
    const int newUsage = data.value(QStringLiteral("newUsage")).toInt(-1);
    const int newRole = data.value(QStringLiteral("newRole")).toInt(-1);
    if (oldUsage < 0 || oldUsage > static_cast<int>(ProjectAssetUsage::RenderInput) ||
        newUsage < 0 || newUsage > static_cast<int>(ProjectAssetUsage::RenderInput) ||
        oldRole < 0 || oldRole > static_cast<int>(ProjectRenderInputRole::Texture) ||
        newRole < 0 || newRole > static_cast<int>(ProjectRenderInputRole::Texture)) {
        return false;
    }
    oldUsage_ = static_cast<ProjectAssetUsage>(oldUsage);
    oldRole_ = static_cast<ProjectRenderInputRole>(oldRole);
    newUsage_ = static_cast<ProjectAssetUsage>(newUsage);
    newRole_ = static_cast<ProjectRenderInputRole>(newRole);
    return !itemId_.isEmpty();
}

MoveProjectItemCommand::MoveProjectItemCommand(ProjectItem* item,
                                               ProjectItem* newParent)
    : itemId_(item ? item->id.toString() : QString()),
      oldParentId_(item && item->parent ? item->parent->id.toString() : QString()),
      newParentId_(newParent ? newParent->id.toString() : QString()) {}

void MoveProjectItemCommand::undo() {
    lastOperationSucceeded_ = applyProjectItemParentForUndo(itemId_, oldParentId_);
}

void MoveProjectItemCommand::redo() {
    lastOperationSucceeded_ = applyProjectItemParentForUndo(itemId_, newParentId_);
}

QString MoveProjectItemCommand::label() const {
    return QStringLiteral("Move Project Item");
}

size_t MoveProjectItemCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        itemId_.size() + oldParentId_.size() + newParentId_.size()) * sizeof(QChar);
}

QJsonObject MoveProjectItemCommand::serialize() const {
    return QJsonObject{{QStringLiteral("itemId"), itemId_},
                       {QStringLiteral("oldParentId"), oldParentId_},
                       {QStringLiteral("newParentId"), newParentId_}};
}

bool MoveProjectItemCommand::deserialize(const QJsonObject& data) {
    itemId_ = data.value(QStringLiteral("itemId")).toString();
    oldParentId_ = data.value(QStringLiteral("oldParentId")).toString();
    newParentId_ = data.value(QStringLiteral("newParentId")).toString();
    return !itemId_.isEmpty() && !oldParentId_.isEmpty() &&
           !newParentId_.isEmpty();
}

CreateProjectFolderCommand::CreateProjectFolderCommand(
    QString folderId, QString parentId, QString name, QStringList tags)
    : folderId_(std::move(folderId)), parentId_(std::move(parentId)),
      name_(std::move(name)), tags_(std::move(tags)) {}

void CreateProjectFolderCommand::undo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project) {
        return;
    }
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), folderId_);
    if (!item || item->type() != eProjectItemType::Folder) {
        return;
    }
    // Creation undo must not recursively remove content added afterwards.
    if (!item->children.isEmpty()) {
        return;
    }
    lastOperationSucceeded_ = project->removeItem(item) &&
                              !findProjectItemInTreeForUndo(
                                  project->projectItems(), folderId_);
    if (lastOperationSucceeded_) {
        if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    }
}

void CreateProjectFolderCommand::redo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project) {
        return;
    }
    if (findProjectItemInTreeForUndo(project->projectItems(), folderId_)) {
        return;
    }
    ProjectItem* parent = nullptr;
    if (!parentId_.isEmpty()) {
        parent = findProjectItemInTreeForUndo(project->projectItems(), parentId_);
        if (!parent || parent->type() != eProjectItemType::Folder) {
            return;
        }
    }
    QJsonObject folder{{QStringLiteral("type"), QStringLiteral("folder")},
                       {QStringLiteral("id"), folderId_},
                       {QStringLiteral("name"), name_}};
    QJsonArray tags;
    for (const auto& tag : tags_) {
        tags.append(tag);
    }
    folder.insert(QStringLiteral("tags"), tags);
    if (!project->addProjectItemsFromJson(QJsonArray{folder}, parent)) {
        return;
    }
    auto* created = findProjectItemInTreeForUndo(project->projectItems(), folderId_);
    auto* expectedParent = parent ? parent : project->projectItems().value(0);
    lastOperationSucceeded_ = created && created->type() == eProjectItemType::Folder &&
                              created->parent == expectedParent;
    if (lastOperationSucceeded_) {
        if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    }
}

QString CreateProjectFolderCommand::label() const {
    return QStringLiteral("Create Project Folder");
}

size_t CreateProjectFolderCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        folderId_.size() + parentId_.size() + name_.size() +
        tags_.join(QStringLiteral("\n")).size()) * sizeof(QChar);
}

QJsonObject CreateProjectFolderCommand::serialize() const {
    QJsonArray tags;
    for (const auto& tag : tags_) {
        tags.append(tag);
    }
    return QJsonObject{{QStringLiteral("folderId"), folderId_},
                       {QStringLiteral("parentId"), parentId_},
                       {QStringLiteral("name"), name_},
                       {QStringLiteral("tags"), tags}};
}

bool CreateProjectFolderCommand::deserialize(const QJsonObject& data) {
    folderId_ = data.value(QStringLiteral("folderId")).toString();
    parentId_ = data.value(QStringLiteral("parentId")).toString();
    name_ = data.value(QStringLiteral("name")).toString();
    tags_.clear();
    for (const auto& value : data.value(QStringLiteral("tags")).toArray()) {
        if (value.isString()) {
            tags_.append(value.toString());
        }
    }
    return !folderId_.isEmpty() && !name_.isEmpty();
}

AddProjectItemsCommand::AddProjectItemsCommand(
    QJsonArray items, ProjectItem* parent, QString afterCurrentCompositionId)
    : items_(std::move(items)),
      parentId_(parent ? parent->id.toString() : QString()),
      afterCurrentCompositionId_(std::move(afterCurrentCompositionId)) {
    if (auto project = ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr()) {
        beforeCurrentCompositionId_ = project->currentCompositionId().toString();
    }
    for (const auto& value : items_) {
        if (value.isObject()) {
            itemIds_.append(value.toObject().value(QStringLiteral("id")).toString());
        }
    }
}

void AddProjectItemsCommand::undo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project || itemIds_.isEmpty()) return;
    ProjectItem* parent = nullptr;
    if (parentId_.isEmpty()) {
        const auto roots = project->projectItems();
        parent = roots.isEmpty() ? nullptr : roots.front();
    } else {
        parent = findProjectItemInTreeForUndo(project->projectItems(), parentId_);
    }
    if (!parent || parent->type() != eProjectItemType::Folder) return;
    QStringList removedIds;
    for (const auto& itemId : itemIds_) {
        auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId);
        if (!item || !project->removeItem(item)) {
            for (const auto& removedId : removedIds) {
                for (const auto& value : items_) {
                    if (value.isObject() &&
                        value.toObject().value(QStringLiteral("id")).toString() == removedId) {
                        project->addProjectItemsFromJson(
                            QJsonArray{value.toObject()}, parent);
                        break;
                    }
                }
            }
            project->projectChanged();
            return;
        }
        removedIds.append(itemId);
    }
    for (const auto& itemId : itemIds_) {
        if (findProjectItemInTreeForUndo(project->projectItems(), itemId)) return;
    }
    if (!afterCurrentCompositionId_.isEmpty() &&
        project->currentCompositionId().toString() == afterCurrentCompositionId_ &&
        !beforeCurrentCompositionId_.isEmpty()) {
        project->setCurrentCompositionId(
            CompositionID(beforeCurrentCompositionId_), false);
    }
    project->projectChanged();
    lastOperationSucceeded_ = true;
    if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
}

void AddProjectItemsCommand::redo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project || !canSerialize()) return;
    ProjectItem* parent = nullptr;
    if (parentId_.isEmpty()) {
        const auto roots = project->projectItems();
        parent = roots.isEmpty() ? nullptr : roots.front();
    } else {
        parent = findProjectItemInTreeForUndo(project->projectItems(), parentId_);
    }
    const bool added = parent && parent->type() == eProjectItemType::Folder &&
                       project->addProjectItemsFromJson(items_, parent);
    if (!added) {
        for (const auto& itemId : itemIds_) {
            if (auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId)) {
                project->removeItem(item);
            }
        }
        project->projectChanged();
        return;
    }
    for (const auto& itemId : itemIds_) {
        if (!findProjectItemInTreeForUndo(project->projectItems(), itemId)) {
            for (const auto& addedId : itemIds_) {
                if (auto* item = findProjectItemInTreeForUndo(project->projectItems(), addedId)) {
                    project->removeItem(item);
                }
            }
            project->projectChanged();
            return;
        }
    }
    if (!afterCurrentCompositionId_.isEmpty()) {
        project->setCurrentCompositionId(
            CompositionID(afterCurrentCompositionId_), false);
        if (project->currentCompositionId().toString() != afterCurrentCompositionId_) {
            for (const auto& addedId : itemIds_) {
                if (auto* item = findProjectItemInTreeForUndo(project->projectItems(), addedId)) {
                    project->removeItem(item);
                }
            }
            project->projectChanged();
            return;
        }
    }
    project->projectChanged();
    lastOperationSucceeded_ = true;
    if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
}

QString AddProjectItemsCommand::label() const {
    return QStringLiteral("Add Project Items");
}

size_t AddProjectItemsCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        QJsonDocument(items_).toJson(QJsonDocument::Compact).size() +
        parentId_.size() * sizeof(QChar));
}

bool AddProjectItemsCommand::canSerialize() const {
    if (items_.isEmpty() || itemIds_.size() != items_.size()) return false;
    for (int i = 0; i < itemIds_.size(); ++i) {
        const auto& itemId = itemIds_.at(i);
        if (itemId.trimmed().isEmpty()) return false;
        for (int j = 0; j < i; ++j) {
            if (itemIds_.at(j) == itemId) return false;
        }
    }
    return true;
}

QJsonObject AddProjectItemsCommand::serialize() const {
    return QJsonObject{{QStringLiteral("items"), items_},
                       {QStringLiteral("parentId"), parentId_},
                       {QStringLiteral("itemIds"), QJsonArray::fromStringList(itemIds_)},
                       {QStringLiteral("beforeCurrentCompositionId"), beforeCurrentCompositionId_},
                       {QStringLiteral("afterCurrentCompositionId"), afterCurrentCompositionId_}};
}

bool AddProjectItemsCommand::deserialize(const QJsonObject& data) {
    items_ = data.value(QStringLiteral("items")).toArray();
    parentId_ = data.value(QStringLiteral("parentId")).toString();
    beforeCurrentCompositionId_ = data.value(QStringLiteral("beforeCurrentCompositionId")).toString();
    afterCurrentCompositionId_ = data.value(QStringLiteral("afterCurrentCompositionId")).toString();
    itemIds_.clear();
    for (const auto& value : data.value(QStringLiteral("itemIds")).toArray()) {
        if (value.isString()) itemIds_.append(value.toString());
    }
    return canSerialize();
}

RemoveProjectItemCommand::RemoveProjectItemCommand(ProjectItem* item)
    : RemoveProjectItemCommand(item, projectItemSnapshotForUndo(item)) {}

RemoveProjectItemCommand::RemoveProjectItemCommand(
    ProjectItem* item, QJsonObject snapshot)
    : itemId_(item ? item->id.toString() : QString()),
      parentId_(item && item->parent ? item->parent->id.toString() : QString()),
      parentIndex_(item && item->parent
                       ? static_cast<int>(item->parent->children.indexOf(item))
                       : -1),
      snapshot_(std::move(snapshot)) {}

void RemoveProjectItemCommand::undo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project || snapshot_.isEmpty()) {
        return;
    }
    if (findProjectItemInTreeForUndo(project->projectItems(), itemId_)) {
        return;
    }
    auto* parent = findProjectItemInTreeForUndo(project->projectItems(), parentId_);
    if (!parent || parent->type() != eProjectItemType::Folder) {
        return;
    }
    if (!project->addProjectItemsFromJson(QJsonArray{snapshot_}, parent)) {
        return;
    }
    auto* restored = findProjectItemInTreeForUndo(project->projectItems(), itemId_);
    if (!restored || restored->parent != parent) {
        return;
    }
    parent->children.removeOne(restored);
    const int childCount = static_cast<int>(parent->children.size());
    const int insertIndex = std::clamp(parentIndex_, 0, childCount);
    parent->children.insert(insertIndex, restored);
    project->projectChanged();
    lastOperationSucceeded_ = restored->parent == parent &&
                              parent->children.value(insertIndex) == restored;
    if (lastOperationSucceeded_) {
        if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
    }
}

void RemoveProjectItemCommand::redo() {
    auto& manager = ArtifactProjectManager::getInstance();
    auto project = manager.getCurrentProjectSharedPtr();
    lastOperationSucceeded_ = false;
    if (!project) {
        return;
    }
    auto* item = findProjectItemInTreeForUndo(project->projectItems(), itemId_);
    if (item) {
        lastOperationSucceeded_ = project->removeItem(item) &&
                                  !findProjectItemInTreeForUndo(
                                      project->projectItems(), itemId_);
        if (lastOperationSucceeded_) {
            if (auto* undo = UndoManager::instance()) undo->notifyAnythingChanged();
        }
    }
}

QString RemoveProjectItemCommand::label() const {
    return QStringLiteral("Remove Project Item");
}

size_t RemoveProjectItemCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(
        itemId_.size() + parentId_.size()) * sizeof(QChar) +
        static_cast<size_t>(QJsonDocument(snapshot_)
                                .toJson(QJsonDocument::Compact)
                                .size());
}

QJsonObject RemoveProjectItemCommand::serialize() const {
    return QJsonObject{{QStringLiteral("itemId"), itemId_},
                       {QStringLiteral("parentId"), parentId_},
                       {QStringLiteral("parentIndex"), parentIndex_},
                       {QStringLiteral("snapshot"), snapshot_}};
}

bool RemoveProjectItemCommand::deserialize(const QJsonObject& data) {
    itemId_ = data.value(QStringLiteral("itemId")).toString();
    parentId_ = data.value(QStringLiteral("parentId")).toString();
    parentIndex_ = data.value(QStringLiteral("parentIndex")).toInt(-1);
    snapshot_ = data.value(QStringLiteral("snapshot")).toObject();
    const QString type = snapshot_.value(QStringLiteral("type")).toString();
    return !itemId_.isEmpty() && !parentId_.isEmpty() && parentIndex_ >= 0 &&
           !snapshot_.isEmpty() &&
           (type == QStringLiteral("folder") ||
            type == QStringLiteral("footage") ||
            type == QStringLiteral("solid"));
}

// --- ChangeLayerOpacityCommand ---
ChangeLayerOpacityCommand::ChangeLayerOpacityCommand(ArtifactAbstractLayerPtr layer, float oldOpacity, float newOpacity)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldOpacity_(oldOpacity), newOpacity_(newOpacity) {}

void ChangeLayerOpacityCommand::undo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        layer->setOpacity(oldOpacity_);
        notifyLayerPropertyChanged(layer, QStringLiteral("layer.opacity"));
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void ChangeLayerOpacityCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        layer->setOpacity(newOpacity_);
        notifyLayerPropertyChanged(layer, QStringLiteral("layer.opacity"));
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

QString ChangeLayerOpacityCommand::label() const {
    return QStringLiteral("Change Opacity: %1% → %2%").arg(oldOpacity_ * 100).arg(newOpacity_ * 100);
}

size_t ChangeLayerOpacityCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(layerId_.size()) * sizeof(QChar);
}

bool ChangeLayerOpacityCommand::canSerialize() const {
    return !layerId_.isEmpty() && !layer_.expired() &&
           std::isfinite(oldOpacity_) && std::isfinite(newOpacity_);
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
    if (!finiteJsonNumber(data, QStringLiteral("oldOpacity"), oldOpacity_) ||
        !finiteJsonNumber(data, QStringLiteral("newOpacity"), newOpacity_)) {
        return false;
    }
    auto* manager = UndoManager::instance();
    if (!manager) return false;
    layer_ = manager->resolveLayer(layerId_);
    return canSerialize();
}

// --- ChangeActiveVariantCommand ---
ChangeActiveVariantCommand::ChangeActiveVariantCommand(ArtifactAbstractLayerPtr layer, size_t oldIndex, size_t newIndex)
    : layer_(layer), layerId_(layer ? layer->id().toQString() : QString()),
      oldIndex_(oldIndex), newIndex_(newIndex) {}

void ChangeActiveVariantCommand::undo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        layer->setActiveVariant(oldIndex_);
        lastOperationSucceeded_ = layer->getActiveVariantIndex() == oldIndex_;
        if (!lastOperationSucceeded_) return;
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void ChangeActiveVariantCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        layer->setActiveVariant(newIndex_);
        lastOperationSucceeded_ = layer->getActiveVariantIndex() == newIndex_;
        if (!lastOperationSucceeded_) return;
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
    qint64 oldIndex = 0;
    qint64 newIndex = 0;
    if (!jsonInteger(data.value(QStringLiteral("oldIndex")), oldIndex) ||
        !jsonInteger(data.value(QStringLiteral("newIndex")), newIndex) ||
        oldIndex < 0 || newIndex < 0) return false;
    oldIndex_ = static_cast<size_t>(oldIndex);
    newIndex_ = static_cast<size_t>(newIndex);
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
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        extracted_ = layer->extractVariant(index_);
        lastOperationSucceeded_ = static_cast<bool>(extracted_);
        if (!lastOperationSucceeded_) return;
        if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
    }
}

void CreateVariantCommand::redo() {
    auto layer = layer_.lock();
    lastOperationSucceeded_ = static_cast<bool>(layer);
    if (lastOperationSucceeded_) {
        if (extracted_) {
            const auto variantCountBefore = layer->getVariants().size();
            if (index_ > variantCountBefore) {
                lastOperationSucceeded_ = false;
            } else {
                layer->insertVariant(index_, std::move(extracted_));
                lastOperationSucceeded_ =
                    index_ < layer->getVariants().size();
            }
        } else {
            const auto variantCountBefore = layer->getVariants().size();
            lastOperationSucceeded_ =
                layer->createVariantFromCurrent(name_) != nullptr;
            if (lastOperationSucceeded_) {
                const auto variants = layer->getVariants();
                lastOperationSucceeded_ = variants.size() == variantCountBefore + 1;
                if (lastOperationSucceeded_) index_ = variants.size() - 1;
            }
        }
        if (lastOperationSucceeded_) {
            if (auto mgr = UndoManager::instance()) mgr->notifyAnythingChanged();
        }
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
    qint64 serializedIndex = 0;
    if (!jsonInteger(data.value(QStringLiteral("index")), serializedIndex) ||
        serializedIndex < 0) return false;
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
bool restoreLayerSnapshotForResolutionRemap(
    const ArtifactAbstractLayerPtr& layer,
    const ChangeCompositionResolutionCommand::LayerSnapshot& snap) {
    if (!layer) return false;
    bool success = true;

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
        if (!prop) {
            success = false;
            continue;
        }

        if (psnap.keyframes.empty()) {
            prop->setValue(psnap.currentValue);
        } else {
            prop->clearKeyFrames();
            for (const auto& k : psnap.keyframes) {
                prop->addKeyFrame(k.time, k.value, static_cast<int>(k.interpolation),
                                  k.cp1_x, k.cp1_y, k.cp2_x, k.cp2_y, k.roving);
                prop->setKeyFrameAnchorAt(k.time, k.anchor);
                prop->setKeyFrameColorLabelAt(k.time, k.colorLabel);
            }
        }
    }

    layer->changed();
    if (layer->maskCount() != static_cast<int>(snap.masks.size())) {
        success = false;
    }
    for (const auto& psnap : snap.properties) {
        ArtifactCore::AbstractPropertyPtr prop;
        for (const auto& group : layer->getLayerPropertyGroups()) {
            prop = group.findProperty(psnap.propertyPath);
            if (prop) break;
        }
        if (!prop || prop->getKeyFrames().size() != psnap.keyframes.size()) {
            success = false;
        }
    }
    return success;
}

} // namespace

ChangeCompositionResolutionCommand::ChangeCompositionResolutionCommand(
    ArtifactCompositionPtr comp,
    const QSize& oldSize,
    const QSize& newSize,
    ArtifactCore::RemapPolicy policy)
    : comp_(comp), compositionId_(comp ? comp->id().toString() : QString()),
      oldSize_(oldSize), newSize_(newSize), policy_(policy) {
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
    lastOperationSucceeded_ = false;
    if (!comp) return;
    for (const auto& snap : beforeSnapshots_) {
        if (!comp->layerById(snap.layerId)) return;
    }

    // size を元に戻す。applyResolutionRemap を逆呼びすると mask/transform まで
    // 再計算されて snapshot 復元と衝突するため、size のみ直接戻す。
    comp->setCompositionSize(oldSize_);
    if (comp->settings().compositionSize() != oldSize_) return;

    // snapshot から mask / transform を復元
    bool restored = true;
    for (const auto& snap : beforeSnapshots_) {
        const auto layer = comp->layerById(snap.layerId);
        restored = restoreLayerSnapshotForResolutionRemap(layer, snap) && restored;
    }
    lastOperationSucceeded_ = restored &&
                              comp->settings().compositionSize() == oldSize_;
    if (!lastOperationSucceeded_) return;

    if (auto mgr = UndoManager::instance()) {
        mgr->notifyAnythingChanged();
    }
}

void ChangeCompositionResolutionCommand::redo() {
    const auto comp = comp_.lock();
    lastOperationSucceeded_ = false;
    if (!comp) return;
    for (const auto& snap : beforeSnapshots_) {
        if (!comp->layerById(snap.layerId)) return;
    }

    // applyResolutionRemap が size 設定 + mask/transform remap をまとめて行う。
    // before snapshot はコンストラクタで採取済み。
    comp->applyResolutionRemap(newSize_, policy_);
    lastOperationSucceeded_ = comp->settings().compositionSize() == newSize_;
    if (!lastOperationSucceeded_) {
        comp->setCompositionSize(oldSize_);
        for (const auto& snap : beforeSnapshots_) {
            restoreLayerSnapshotForResolutionRemap(comp->layerById(snap.layerId), snap);
        }
        return;
    }

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

bool ChangeCompositionResolutionCommand::canSerialize() const {
    if (compositionId_.isEmpty() || comp_.expired() ||
        oldSize_.width() <= 0 || oldSize_.height() <= 0 ||
        newSize_.width() <= 0 || newSize_.height() <= 0) {
        return false;
    }
    const int policy = static_cast<int>(policy_);
    if (policy < static_cast<int>(ArtifactCore::RemapPolicy::CenterLocked) ||
        policy > static_cast<int>(ArtifactCore::RemapPolicy::FitWithCrop)) {
        return false;
    }
    for (const auto& layer : beforeSnapshots_) {
        if (layer.layerId.isNil()) {
            return false;
        }
        for (const auto& property : layer.properties) {
            if (property.propertyPath.isEmpty() ||
                QJsonValue::fromVariant(property.currentValue).isUndefined()) {
                return false;
            }
            for (const auto& keyframe : property.keyframes) {
                bool supported = false;
                encodeKeyframeValue(keyframe.value, supported);
                if (!supported) {
                    return false;
                }
            }
        }
    }
    return true;
}

QJsonObject ChangeCompositionResolutionCommand::serialize() const {
    const auto encodeKeyframes = [](const std::vector<ArtifactCore::KeyFrame>& keyframes) {
        QJsonArray values;
        for (const auto& keyframe : keyframes) {
            bool supported = false;
            const QJsonValue value = encodeKeyframeValue(keyframe.value, supported);
            if (!supported) {
                continue;
            }
            values.append(QJsonObject{
                {QStringLiteral("frame"),
                 static_cast<qint64>(keyframe.time.rescaledTo(30))},
                {QStringLiteral("timeValue"),
                 static_cast<qint64>(keyframe.time.value())},
                {QStringLiteral("timeScale"),
                 static_cast<qint64>(keyframe.time.scale())},
                {QStringLiteral("value"), value},
                {QStringLiteral("interpolation"),
                 static_cast<int>(keyframe.interpolation)},
                {QStringLiteral("cp1_x"), keyframe.cp1_x},
                {QStringLiteral("cp1_y"), keyframe.cp1_y},
                {QStringLiteral("cp2_x"), keyframe.cp2_x},
                {QStringLiteral("cp2_y"), keyframe.cp2_y},
                {QStringLiteral("roving"), keyframe.roving},
                {QStringLiteral("anchor"), static_cast<int>(keyframe.anchor)},
                {QStringLiteral("colorLabel"),
                 static_cast<int>(keyframe.colorLabel)}});
        }
        return values;
    };

    QJsonArray layers;
    for (const auto& layer : beforeSnapshots_) {
        QJsonArray masks;
        for (const auto& mask : layer.masks) {
            masks.append(encodeMask(mask));
        }
        QJsonArray properties;
        for (const auto& property : layer.properties) {
            properties.append(QJsonObject{
                {QStringLiteral("propertyPath"), property.propertyPath},
                {QStringLiteral("currentValue"),
                 QJsonValue::fromVariant(property.currentValue)},
                {QStringLiteral("keyframes"),
                 encodeKeyframes(property.keyframes)}});
        }
        layers.append(QJsonObject{
            {QStringLiteral("layerId"), layer.layerId.toString()},
            {QStringLiteral("masks"), masks},
            {QStringLiteral("properties"), properties}});
    }
    return QJsonObject{
        {QStringLiteral("compositionId"), compositionId_},
        {QStringLiteral("oldWidth"), oldSize_.width()},
        {QStringLiteral("oldHeight"), oldSize_.height()},
        {QStringLiteral("newWidth"), newSize_.width()},
        {QStringLiteral("newHeight"), newSize_.height()},
        {QStringLiteral("policy"), static_cast<int>(policy_)},
        {QStringLiteral("beforeSnapshots"), layers}};
}

bool ChangeCompositionResolutionCommand::deserialize(const QJsonObject& data) {
    compositionId_ = data.value(QStringLiteral("compositionId")).toString();
    int oldWidth = 0;
    int oldHeight = 0;
    int newWidth = 0;
    int newHeight = 0;
    qint64 policyValue = 0;
    if (!nonNegativeJsonInt(data.value(QStringLiteral("oldWidth")), oldWidth) ||
        !nonNegativeJsonInt(data.value(QStringLiteral("oldHeight")), oldHeight) ||
        !nonNegativeJsonInt(data.value(QStringLiteral("newWidth")), newWidth) ||
        !nonNegativeJsonInt(data.value(QStringLiteral("newHeight")), newHeight) ||
        oldWidth <= 0 || oldHeight <= 0 || newWidth <= 0 || newHeight <= 0 ||
        !jsonInteger(data.value(QStringLiteral("policy")), policyValue)) {
        return false;
    }
    oldSize_ = QSize(oldWidth, oldHeight);
    newSize_ = QSize(newWidth, newHeight);
    if (policyValue < static_cast<qint64>(ArtifactCore::RemapPolicy::CenterLocked) ||
        policyValue > static_cast<qint64>(ArtifactCore::RemapPolicy::FitWithCrop) ||
        !data.value(QStringLiteral("beforeSnapshots")).isArray()) {
        return false;
    }
    const int policy = static_cast<int>(policyValue);
    policy_ = static_cast<ArtifactCore::RemapPolicy>(policy);
    const auto encodedLayers = data.value(QStringLiteral("beforeSnapshots")).toArray();
    if (encodedLayers.size() > 100000) {
        return false;
    }
    beforeSnapshots_.clear();
    beforeSnapshots_.reserve(encodedLayers.size());
    for (const auto& layerValue : encodedLayers) {
        if (!layerValue.isObject()) {
            return false;
        }
        const auto layerObject = layerValue.toObject();
        LayerSnapshot layer;
        layer.layerId = ArtifactCore::LayerID(
            layerObject.value(QStringLiteral("layerId")).toString());
        if (layer.layerId.isNil() ||
            !layerObject.value(QStringLiteral("masks")).isArray() ||
            !layerObject.value(QStringLiteral("properties")).isArray()) {
            return false;
        }
        for (const auto& maskValue :
             layerObject.value(QStringLiteral("masks")).toArray()) {
            if (!maskValue.isObject()) {
                return false;
            }
            if (!maskJsonStructureValid(maskValue.toObject())) {
                return false;
            }
            layer.masks.push_back(decodeMask(maskValue.toObject()));
        }
        for (const auto& propertyValue :
             layerObject.value(QStringLiteral("properties")).toArray()) {
            if (!propertyValue.isObject()) {
                return false;
            }
            const auto propertyObject = propertyValue.toObject();
            PropertySnapshot property;
            property.propertyPath =
                propertyObject.value(QStringLiteral("propertyPath")).toString();
            const auto currentValue =
                propertyObject.value(QStringLiteral("currentValue"));
            const auto keyframes = propertyObject.value(QStringLiteral("keyframes"));
            if (property.propertyPath.isEmpty() || currentValue.isUndefined() ||
                (!currentValue.isArray() && !currentValue.isBool() &&
                 !currentValue.isDouble() && !currentValue.isNull() &&
                 !currentValue.isObject() && !currentValue.isString()) ||
                !keyframes.isArray()) {
                return false;
            }
            property.currentValue = currentValue.toVariant();
            if (!decodeKeyframes(keyframes.toArray(), property.keyframes)) {
                return false;
            }
            layer.properties.push_back(std::move(property));
        }
        beforeSnapshots_.push_back(std::move(layer));
    }
    auto* manager = UndoManager::instance();
    if (!manager) {
        return false;
    }
    comp_ = manager->resolveComposition(compositionId_);
    return canSerialize();
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
    lastOperationSucceeded_ = restoreFn_ && restoreFn_(beforeState_);
    if (!lastOperationSucceeded_ && restoreFn_) {
        restoreFn_(afterState_);
    }
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

void LayoutSnapshotCommand::redo() {
    lastOperationSucceeded_ = restoreFn_ && restoreFn_(afterState_);
    if (!lastOperationSucceeded_ && restoreFn_) {
        restoreFn_(beforeState_);
    }
    if (lastOperationSucceeded_) {
        if (auto mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
    }
}

QString LayoutSnapshotCommand::label() const {
    return label_.isEmpty() ? QStringLiteral("Layout Change") : label_;
}

size_t LayoutSnapshotCommand::estimatedMemoryBytes() const {
    return sizeof(*this) + static_cast<size_t>(beforeState_.size() + afterState_.size());
}

}
