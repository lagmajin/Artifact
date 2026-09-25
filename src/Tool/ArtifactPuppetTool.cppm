module;
#include <utility>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <QApplication>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QVariant>
#include <QTransform>
#include <QMatrix4x4>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>

module Artifact.Tool.PuppetTool;

import Utils.Id;
import Color.Float;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Composition.Abstract;
import Artifact.Composition.Manager;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderOverlay;
import Event.Bus;
import ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;
import Memory.SharedPtr;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPuppetTool)

qint64 layerTimelineFrame(const ArtifactAbstractLayer* layer)
{
    if (!layer) return 0;
    return layer->currentFrame() - layer->startTime().framePosition() +
           layer->inPoint().framePosition();
}

struct PinRecord {
    QString id;
    std::string engineId;
    LayerID layerId;
    QPointF canvasPos;  // current position
    QPointF originalPos;
    int type = 0; // 0=Position, 1=Starch, 2=Bend, 3=Overlap
    float rotation = 0.0f;
    float weight = 1.0f;
    float depth = 0.0f;
};

struct LayerPins {
    std::vector<PinRecord> pins;
    std::vector<ArtifactCore::PuppetPin> enginePinScratch;
    std::unique_ptr<ArtifactCore::OpenCVPuppetEngine> engine;
    bool needsRebind = false;
    bool loadedFromLayer = false;
    bool needsDeform = true;
    bool enabled = true;
    qint64 lastEvaluationFrame = std::numeric_limits<qint64>::min();
    QJsonObject lastEvaluationState;
    std::uint64_t sourceVersion = 0;
    qint64 sourceFrameIndex = -1;
    qint64 sourceFrameContentKey = -1;
    int sourceWidth = 0;
    int sourceHeight = 0;
    QRect sourceCropPixels;
    QRectF meshDisplayRect;
    QMatrix4x4 meshLocalTransform;
    std::vector<size_t> triangleOrder;
    QTransform pinSpaceToCanvas;
    bool hasPinSpaceTransform = false;
    QTransform canvasToLocal;
    QRectF layerBounds;
    bool hasCanvasToLocal = false;
    Deformation2DMode mode = Deformation2DMode::Pins;
    int gridColumns = 5;
    int gridRows = 5;
    std::vector<cv::Point2f> gridPositionScratch;
};

QPointF pinAuthoredToDisplayCanvas(const LayerPins& lp,
                                   ArtifactAbstractLayer* layer,
                                   const QPointF& authoredCanvas)
{
    if (!layer) return authoredCanvas;
    const QTransform localToCanvas = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform canvasToLocal = localToCanvas.inverted(&invertible);
    if (!invertible) return authoredCanvas;
    return localToCanvas.map(lp.meshLocalTransform.map(
        canvasToLocal.map(authoredCanvas)));
}

QPointF pinDisplayToAuthoredCanvas(const LayerPins& lp,
                                   ArtifactAbstractLayer* layer,
                                   const QPointF& displayCanvas)
{
    if (!layer) return displayCanvas;
    const QTransform localToCanvas = layer->getGlobalTransform();
    bool transformInvertible = false;
    const QTransform canvasToLocal = localToCanvas.inverted(&transformInvertible);
    if (!transformInvertible) return displayCanvas;
    bool cropInvertible = false;
    const QMatrix4x4 displayToLocal = lp.meshLocalTransform.inverted(&cropInvertible);
    const QPointF local = canvasToLocal.map(displayCanvas);
    return localToCanvas.map(cropInvertible ? displayToLocal.map(local) : local);
}

class ArtifactPuppetTool::Impl {
public:
    bool active = false;
    bool proportionalEditingEnabled = false;
    float proportionalEditRadius = 120.0f;
    QString selectedPinId;
    std::map<QString, LayerPins> layerPins; // layerId.toString() -> LayerPins

    LayerPins* getOrCreateLayerPins(const LayerID& layerId) {
        const QString key = layerId.toString();
        auto it = layerPins.find(key);
        if (it == layerPins.end()) {
            LayerPins lp;
            lp.engine = std::make_unique<ArtifactCore::OpenCVPuppetEngine>();
            lp.enginePinScratch.reserve(1024);
            lp.needsRebind = true;
            auto result = layerPins.emplace(key, std::move(lp));
            return &result.first->second;
        }
        return &it->second;
    }

    LayerPins* getLayerPins(const LayerID& layerId) {
        const QString key = layerId.toString();
        auto it = layerPins.find(key);
        return it != layerPins.end() ? &it->second : nullptr;
    }

    PinRecord* findPin(const QString& pinId) {
        for (auto& [key, lp] : layerPins) {
            for (auto& pin : lp.pins) {
                if (pin.id == pinId) return &pin;
            }
        }
        return nullptr;
    }
};

void restoreDeformerControlProperties(ArtifactAbstractLayer* layer,
                                      const QString& controlId,
                                      const QJsonObject& control,
                                      bool forceReplace = false)
{
    if (!layer || controlId.isEmpty()) return;
    const qint64 frameScale = layer->keyframeTimeScale();
    QStringList fields{QStringLiteral("x"), QStringLiteral("y")};
    if (control.contains(QStringLiteral("rotation")) ||
        control.contains(QStringLiteral("weight"))) {
        fields.append(QStringLiteral("rotation"));
        fields.append(QStringLiteral("weight"));
    }
    for (const QString& axis : fields) {
        const QJsonArray keys = control.value(axis + QStringLiteral("Keys"))
                                    .toArray();
        const QString path = QStringLiteral("deformation2D.%1.%2")
                                 .arg(controlId, axis);
        auto property = layer->persistentLayerProperty(
            path, ArtifactCore::PropertyType::Float,
            control.value(axis).toDouble(), 100);
        if (forceReplace) property->clearKeyFrames();
        if (keys.isEmpty()) {
            if (forceReplace || property->getKeyFrames().empty()) {
                property->setAnimatable(false);
                property->setValue(control.value(axis).toDouble());
            }
            continue;
        }
        if (!forceReplace && !property->getKeyFrames().empty()) continue;
        property->clearKeyFrames();
        property->setAnimatable(true);
        for (const QJsonValue& keyValue : keys) {
            if (!keyValue.isObject()) continue;
            const QJsonObject key = keyValue.toObject();
            const double value = key.value(QStringLiteral("value")).toDouble();
            if (!std::isfinite(value)) continue;
            const qint64 frame = key.value(QStringLiteral("frame"))
                                     .toVariant().toLongLong();
            const ArtifactCore::RationalTime time(frame, frameScale);
            property->addKeyFrame(
                time, value,
                static_cast<ArtifactCore::InterpolationType>(std::clamp(
                    key.value(QStringLiteral("interpolation")).toInt(), 0, 32)),
                static_cast<float>(key.value(QStringLiteral("cp1_x"))
                                       .toDouble(0.42)),
                static_cast<float>(key.value(QStringLiteral("cp1_y"))
                                       .toDouble()),
                static_cast<float>(key.value(QStringLiteral("cp2_x"))
                                       .toDouble(0.58)),
                static_cast<float>(key.value(QStringLiteral("cp2_y"))
                                       .toDouble(1.0)),
                key.value(QStringLiteral("roving")).toBool());
            property->setKeyFrameAnchorAt(
                time, static_cast<ArtifactCore::KeyFrame::Anchor>(
                    std::clamp(key.value(QStringLiteral("anchor")).toInt(),
                               0, 3)));
            property->setKeyFrameColorLabelAt(
                time, static_cast<ArtifactCore::KeyFrame::ColorLabel>(
                    std::clamp(key.value(QStringLiteral("colorLabel")).toInt(),
                               0, 6)));
        }
    }
}

void ArtifactPuppetTool::ensureLayerLoaded(const LayerID& layerId)
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) return;
    auto layer = selection->currentLayer();
    if (!layer || layer->id() != layerId) return;
    ensureLayerLoaded(layerId, layer.get());
}

void ArtifactPuppetTool::ensureLayerLoaded(const LayerID& layerId,
                                            ArtifactAbstractLayer* layer)
{
    if (!layer || layer->id() != layerId) return;
    auto* lp = impl_->getOrCreateLayerPins(layerId);
    if (!lp || lp->loadedFromLayer) return;
    lp->pinSpaceToCanvas = layer->getGlobalTransform();
    lp->hasPinSpaceTransform = true;
    lp->loadedFromLayer = true;

    QJsonObject state = layer->deformation2DData();
    lp->enabled = state.value(QStringLiteral("enabled")).toBool(true);
    lp->mode = state.value(QStringLiteral("mode")).toString() ==
                       QStringLiteral("grid")
        ? Deformation2DMode::Grid : Deformation2DMode::Pins;
    lp->gridColumns = std::clamp(
        state.value(QStringLiteral("columns")).toInt(5), 2, 64);
    lp->gridRows = std::clamp(
        state.value(QStringLiteral("rows")).toInt(5), 2, 64);
    const QJsonArray pins = state.value(QStringLiteral("pins")).toArray();
    const QJsonArray gridControls = state.value(QStringLiteral("gridControls")).toArray();
    const QJsonArray& controls = lp->mode == Deformation2DMode::Grid
        ? gridControls : pins;
    const QTransform localToCanvas = layer->getGlobalTransform();
    const size_t pinLimit = lp->mode == Deformation2DMode::Grid
        ? static_cast<size_t>(lp->gridColumns * lp->gridRows) : 1024u;
    for (const QJsonValue& value : controls) {
        if (!value.isObject() || lp->pins.size() >= pinLimit) continue;
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        const QPointF originalLocal(object.value(QStringLiteral("originalX")).toDouble(),
                                    object.value(QStringLiteral("originalY")).toDouble());
        const QPointF currentLocal(object.value(QStringLiteral("x")).toDouble(),
                                   object.value(QStringLiteral("y")).toDouble());
        if (id.isEmpty() || !std::isfinite(originalLocal.x()) ||
            !std::isfinite(originalLocal.y()) || !std::isfinite(currentLocal.x()) ||
            !std::isfinite(currentLocal.y())) continue;
        const double rotation =
            object.value(QStringLiteral("rotation")).toDouble();
        const double weight =
            object.value(QStringLiteral("weight")).toDouble(1.0);
        const double depth =
            object.value(QStringLiteral("depth")).toDouble();
        if (!std::isfinite(rotation) || !std::isfinite(weight) ||
            !std::isfinite(depth)) continue;
        PinRecord pin;
        pin.id = id;
        pin.engineId = id.toStdString();
        pin.layerId = layerId;
        pin.originalPos = localToCanvas.map(originalLocal);
        pin.canvasPos = localToCanvas.map(currentLocal);
        pin.type = std::clamp(object.value(QStringLiteral("type")).toInt(), 0, 4);
        pin.rotation = static_cast<float>(
            std::clamp(rotation, -180.0, 180.0));
        pin.weight = static_cast<float>(std::clamp(weight, 0.0, 1.0));
        pin.depth = static_cast<float>(std::clamp(depth, -1.0, 1.0));
        restoreDeformerControlProperties(layer, id, object);
        lp->pins.push_back(std::move(pin));
    }
    lp->needsRebind = true;
}

void ArtifactPuppetTool::rebaseLayerPins(const LayerID& layerId,
                                          ArtifactAbstractLayer* layer)
{
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp || !layer || layer->id() != layerId) return;
    const QTransform currentTransform = layer->getGlobalTransform();
    if (!lp->hasPinSpaceTransform) {
        lp->pinSpaceToCanvas = currentTransform;
        lp->hasPinSpaceTransform = true;
        return;
    }
    if (lp->pinSpaceToCanvas == currentTransform) return;
    bool invertible = false;
    const QTransform previousCanvasToPinSpace =
        lp->pinSpaceToCanvas.inverted(&invertible);
    if (!invertible) return;
    for (PinRecord& pin : lp->pins) {
        pin.originalPos = currentTransform.map(
            previousCanvasToPinSpace.map(pin.originalPos));
        pin.canvasPos = currentTransform.map(
            previousCanvasToPinSpace.map(pin.canvasPos));
    }
    lp->pinSpaceToCanvas = currentTransform;
}

void ArtifactPuppetTool::persistLayerData(const LayerID& layerId)
{
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return;
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) return;
    auto layer = selection->currentLayer();
    if (!layer || layer->id() != layerId) return;
    rebaseLayerPins(layerId, layer.get());

    bool invertible = false;
    const QTransform canvasToLocal = layer->getGlobalTransform().inverted(&invertible);
    if (!invertible) return;
    QJsonArray pins;
    for (const PinRecord& pin : lp->pins) {
        const QPointF originalLocal = canvasToLocal.map(pin.originalPos);
        const QPointF currentLocal = canvasToLocal.map(pin.canvasPos);
        QJsonObject object;
        object[QStringLiteral("id")] = pin.id;
        object[QStringLiteral("originalX")] = originalLocal.x();
        object[QStringLiteral("originalY")] = originalLocal.y();
        object[QStringLiteral("x")] = currentLocal.x();
        object[QStringLiteral("y")] = currentLocal.y();
        object[QStringLiteral("type")] = pin.type;
        object[QStringLiteral("rotation")] = pin.rotation;
        object[QStringLiteral("weight")] = pin.weight;
        object[QStringLiteral("depth")] = pin.depth;
        const QJsonObject previousState = layer->deformation2DData();
        const QString previousControlsKey =
            previousState.value(QStringLiteral("mode")).toString() ==
                    QStringLiteral("grid")
                ? QStringLiteral("gridControls") : QStringLiteral("pins");
        const QJsonArray previousControls =
            previousState.value(previousControlsKey).toArray();
        for (const QJsonValue& previousValue : previousControls) {
            if (!previousValue.isObject()) continue;
            const QJsonObject previous = previousValue.toObject();
            if (previous.value(QStringLiteral("id")).toString() != pin.id)
                continue;
            const QString xPath = QStringLiteral("deformation2D.%1.x").arg(pin.id);
            const QString yPath = QStringLiteral("deformation2D.%1.y").arg(pin.id);
            const auto xProperty = layer->getProperty(xPath);
            const auto yProperty = layer->getProperty(yPath);
            const QString rotationPath =
                QStringLiteral("deformation2D.%1.rotation").arg(pin.id);
            const QString weightPath =
                QStringLiteral("deformation2D.%1.weight").arg(pin.id);
            const auto rotationProperty = layer->getProperty(rotationPath);
            const auto weightProperty = layer->getProperty(weightPath);
            const bool hasXAnimation = xProperty &&
                                       !xProperty->getKeyFrames().empty();
            const bool hasYAnimation = yProperty &&
                                       !yProperty->getKeyFrames().empty();
            const bool hasRotationAnimation = rotationProperty &&
                !rotationProperty->getKeyFrames().empty();
            const bool hasWeightAnimation = weightProperty &&
                !weightProperty->getKeyFrames().empty();
            if (hasXAnimation || hasYAnimation) {
                object[QStringLiteral("originalX")] =
                    previous.value(QStringLiteral("originalX"));
                object[QStringLiteral("originalY")] =
                    previous.value(QStringLiteral("originalY"));
                if (hasXAnimation) {
                    object[QStringLiteral("x")] = previous.value(QStringLiteral("x"));
                    object[QStringLiteral("xKeys")] =
                        previous.value(QStringLiteral("xKeys"));
                } else {
                    object[QStringLiteral("x")] = currentLocal.x();
                }
                if (hasYAnimation) {
                    object[QStringLiteral("y")] = previous.value(QStringLiteral("y"));
                    object[QStringLiteral("yKeys")] =
                        previous.value(QStringLiteral("yKeys"));
                } else {
                    object[QStringLiteral("y")] = currentLocal.y();
                }
            }
            if (hasRotationAnimation) {
                object[QStringLiteral("rotation")] =
                    previous.value(QStringLiteral("rotation"));
                object[QStringLiteral("rotationKeys")] =
                    previous.value(QStringLiteral("rotationKeys"));
            }
            if (hasWeightAnimation) {
                object[QStringLiteral("weight")] =
                    previous.value(QStringLiteral("weight"));
                object[QStringLiteral("weightKeys")] =
                    previous.value(QStringLiteral("weightKeys"));
            }
            break;
        }
        QStringList animatedFields{QStringLiteral("x"), QStringLiteral("y")};
        if (lp->mode == Deformation2DMode::Pins) {
            animatedFields.append(QStringLiteral("rotation"));
            animatedFields.append(QStringLiteral("weight"));
        }
        for (const QString& axis : animatedFields) {
            const QString propertyPath =
                QStringLiteral("deformation2D.%1.%2").arg(pin.id, axis);
            const auto property = layer->getProperty(propertyPath);
            if (!property) continue;
            QJsonArray keys;
            for (const auto& key : property->getKeyFrames()) {
                keys.append(QJsonObject{
                    {QStringLiteral("frame"), QString::number(
                         key.time.rescaledTo(layer->keyframeTimeScale()))},
                    {QStringLiteral("value"),
                     QJsonValue::fromVariant(key.value)},
                    {QStringLiteral("interpolation"),
                     static_cast<int>(key.interpolation)},
                    {QStringLiteral("cp1_x"), key.cp1_x},
                    {QStringLiteral("cp1_y"), key.cp1_y},
                    {QStringLiteral("cp2_x"), key.cp2_x},
                    {QStringLiteral("cp2_y"), key.cp2_y},
                    {QStringLiteral("roving"), key.roving},
                    {QStringLiteral("anchor"), static_cast<int>(key.anchor)},
                    {QStringLiteral("colorLabel"),
                     static_cast<int>(key.colorLabel)}});
            }
            object[axis + QStringLiteral("Keys")] = keys;
            if (!property->getKeyFrames().empty()) {
                object[axis] = QJsonValue::fromVariant(
                    property->getKeyFrames().front().value);
            }
        }
        pins.append(object);
    }
    QJsonObject state = layer->deformation2DData();
    state[QStringLiteral("version")] = 1;
    state[QStringLiteral("enabled")] =
        state.value(QStringLiteral("enabled")).toBool(true);
    if (lp->mode == Deformation2DMode::Grid) {
        state[QStringLiteral("mode")] = QStringLiteral("grid");
        state[QStringLiteral("columns")] = lp->gridColumns;
        state[QStringLiteral("rows")] = lp->gridRows;
        state[QStringLiteral("gridControls")] = pins;
    } else {
        state[QStringLiteral("mode")] = QStringLiteral("pins");
        state[QStringLiteral("pins")] = pins;
    }
    const QString inactiveArray = lp->mode == Deformation2DMode::Grid
        ? QStringLiteral("pins") : QStringLiteral("gridControls");
    if (!state.contains(inactiveArray)) state[inactiveArray] = QJsonArray{};
    layer->setDeformation2DData(state);
    lp->pinSpaceToCanvas = layer->getGlobalTransform();
    lp->hasPinSpaceTransform = true;
}

bool ArtifactPuppetTool::restoreLayerData(const LayerID& layerId,
                                           const QJsonObject& state,
                                           ArtifactAbstractLayer* layerOverride)
{
    ArtifactAbstractLayerPtr layerOwner;
    ArtifactAbstractLayer* layer = layerOverride;
    if (layerOverride && layerOverride->id() == layerId) {
        layer = layerOverride;
    } else {
        auto* application = ArtifactApplicationManager::instance();
        auto* compositions = application ? application->compositionManager() : nullptr;
        if (!compositions) return false;
        for (const auto& composition : compositions->allCompositions()) {
            if (!composition) continue;
            layerOwner = composition->layerById(layerId);
            if (layerOwner) {
                layer = layerOwner.get();
                break;
            }
        }
    }
    if (!layer) return false;
    layer->setDeformation2DData(state);
    auto* lp = impl_->getLayerPins(layerId);
    if (lp) {
        lp->loadedFromLayer = false;
        lp->pins.clear();
        lp->mode = Deformation2DMode::Pins;
        lp->needsRebind = true;
        lp->needsDeform = true;
        if (lp->engine) lp->engine->reset();
    }
    const QString selectedId = impl_->selectedPinId;
    impl_->selectedPinId.clear();
    ensureLayerLoaded(layerId, layer);
    const QJsonObject restoredState = layer->deformation2DData();
    const QString mode = restoredState.value(QStringLiteral("mode")).toString();
    const QJsonArray controls = restoredState.value(
        mode == QStringLiteral("grid")
            ? QStringLiteral("gridControls") : QStringLiteral("pins"))
        .toArray();
    QStringList controlIds;
    for (const QJsonValue& value : controls) {
        if (value.isObject()) {
            const QJsonObject control = value.toObject();
            const QString controlId =
                control.value(QStringLiteral("id")).toString();
            controlIds.append(controlId);
        }
    }
    for (const QString& controlId : controlIds) {
        layer->removePersistentLayerPropertiesWithPrefix(
            QStringLiteral("deformation2D.%1.").arg(controlId));
    }
    for (const QJsonValue& value : controls) {
        if (!value.isObject()) continue;
        const QJsonObject control = value.toObject();
        restoreDeformerControlProperties(
            layer, control.value(QStringLiteral("id")).toString(), control,
            true);
    }
    if (!selectedId.isEmpty() && impl_->findPin(selectedId)) {
        impl_->selectedPinId = selectedId;
    }
    return layer->deformation2DData() == restoredState;
}

bool ArtifactPuppetTool::setDeformation2DMode(
    const LayerID& layerId, Deformation2DMode mode, int columns, int rows)
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) return false;
    const auto layer = selection->currentLayer();
    if (!layer || layer->id() != layerId) return false;
    const QRectF requestedBounds = layer->localBounds();
    if (mode == Deformation2DMode::Grid &&
        (!requestedBounds.isValid() || requestedBounds.width() <= 0.0 ||
         requestedBounds.height() <= 0.0)) return false;

    ensureLayerLoaded(layerId, layer.get());
    rebaseLayerPins(layerId, layer.get());
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return false;
    const Deformation2DMode previousMode = lp->mode;
    const int previousColumns = lp->gridColumns;
    const int previousRows = lp->gridRows;
    std::vector<QPointF> previousGridPositions;
    if (previousMode == Deformation2DMode::Grid &&
        lp->pins.size() == static_cast<size_t>(previousColumns * previousRows)) {
        previousGridPositions.reserve(lp->pins.size());
        for (const auto& control : lp->pins) {
            previousGridPositions.push_back(control.canvasPos);
        }
    }
    lp->pins.clear();
    lp->mode = mode;
    lp->needsRebind = true;
    lp->needsDeform = true;
    if (lp->engine) lp->engine->reset();
    if (mode == Deformation2DMode::Grid) {
        lp->gridColumns = std::clamp(columns, 2, 64);
        lp->gridRows = std::clamp(rows, 2, 64);
        lp->pins.reserve(static_cast<size_t>(lp->gridColumns * lp->gridRows));
        const QTransform localToCanvas = layer->getGlobalTransform();
        for (int row = 0; row < lp->gridRows; ++row) {
            for (int column = 0; column < lp->gridColumns; ++column) {
                const QPointF local(
                    requestedBounds.x() + requestedBounds.width() * column /
                                     static_cast<double>(lp->gridColumns - 1),
                    requestedBounds.y() + requestedBounds.height() * row /
                                     static_cast<double>(lp->gridRows - 1));
                const QString id = QStringLiteral("grid_%1_%2_%3")
                    .arg(layerId.toString()).arg(row).arg(column);
                PinRecord control;
                control.id = id;
                control.engineId = id.toStdString();
                control.layerId = layerId;
                control.canvasPos = localToCanvas.map(local);
                if (!previousGridPositions.empty() &&
                    previousColumns >= 2 && previousRows >= 2) {
                    const double oldX = static_cast<double>(column) *
                        (previousColumns - 1) / (lp->gridColumns - 1);
                    const double oldY = static_cast<double>(row) *
                        (previousRows - 1) / (lp->gridRows - 1);
                    const int oldColumn = std::min(
                        static_cast<int>(oldX), previousColumns - 2);
                    const int oldRow = std::min(
                        static_cast<int>(oldY), previousRows - 2);
                    const double tx = oldX - oldColumn;
                    const double ty = oldY - oldRow;
                    const auto at = [&](int oldRowIndex, int oldColumnIndex) {
                        return previousGridPositions[static_cast<size_t>(
                            oldRowIndex * previousColumns + oldColumnIndex)];
                    };
                    const QPointF top = at(oldRow, oldColumn) * (1.0 - tx) +
                                        at(oldRow, oldColumn + 1) * tx;
                    const QPointF bottom = at(oldRow + 1, oldColumn) * (1.0 - tx) +
                                           at(oldRow + 1, oldColumn + 1) * tx;
                    control.canvasPos = top * (1.0 - ty) + bottom * ty;
                }
                control.originalPos = control.canvasPos;
                control.type = 4;
                lp->pins.push_back(std::move(control));
            }
        }
        lp->gridPositionScratch.reserve(lp->pins.size());
    } else {
        lp->mode = Deformation2DMode::Pins;
    }
    if (previousMode != mode) impl_->selectedPinId.clear();
    persistLayerData(layerId);
    return true;
}

Deformation2DMode ArtifactPuppetTool::deformation2DMode(
    const LayerID& layerId) const
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (selection) {
        const auto layer = selection->currentLayer();
        if (layer && layer->id() == layerId) {
            const_cast<ArtifactPuppetTool*>(this)->ensureLayerLoaded(
                layerId, layer.get());
        }
    }
    const auto* lp = impl_->getLayerPins(layerId);
    return lp ? lp->mode : Deformation2DMode::Pins;
}

ArtifactPuppetTool::ArtifactPuppetTool(QObject* parent)
    : QObject(parent), impl_(new Impl())
{
}

ArtifactPuppetTool::~ArtifactPuppetTool()
{
    delete impl_;
}

void ArtifactPuppetTool::activate()
{
    impl_->active = true;
}

void ArtifactPuppetTool::deactivate()
{
    impl_->active = false;
    impl_->selectedPinId.clear();
}

bool ArtifactPuppetTool::isActive() const
{
    return impl_->active;
}

bool ArtifactPuppetTool::addPin(const LayerID& layerId, const QPointF& canvasPos)
{
    if (!std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) return false;
    ensureLayerLoaded(layerId);
    QPointF authoredCanvas = canvasPos;
    ArtifactAbstractLayer* currentLayer = nullptr;
    if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        const auto layer = selection->currentLayer();
        if (layer && layer->id() == layerId) {
            currentLayer = layer.get();
            rebaseLayerPins(layerId, currentLayer);
            if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(currentLayer)) {
                imageLayer->refreshAnimatedSourceCrop();
                if (auto* lp = impl_->getLayerPins(layerId)) {
                    const auto cropLayout = imageLayer->sourceCropDrawLayout();
                    lp->meshLocalTransform = cropLayout.localTransform;
                    lp->meshDisplayRect = cropLayout.outputLocalRect;
                    authoredCanvas = pinDisplayToAuthoredCanvas(
                        *lp, currentLayer, canvasPos);
                }
            }
        }
    }
    auto* lp = impl_->getOrCreateLayerPins(layerId);
    if (!lp) return false;
    if (lp->mode != Deformation2DMode::Pins) return false;
    if (lp->pins.size() >= 1024) return false;

    int idx = static_cast<int>(lp->pins.size());
    QString pinId;
    do {
        pinId = QStringLiteral("pin_%1_%2").arg(layerId.toString()).arg(idx++);
    } while (impl_->findPin(pinId));

    PinRecord pin;
    pin.id = pinId;
    pin.engineId = pinId.toStdString();
    pin.layerId = layerId;
    pin.canvasPos = authoredCanvas;
    pin.originalPos = authoredCanvas;
    pin.type = 0;

    lp->pins.push_back(pin);
    lp->needsRebind = true;
    lp->needsDeform = true;
    impl_->selectedPinId = pinId;
    persistLayerData(layerId);
    return true;
}

bool ArtifactPuppetTool::removePin(const QString& pinId)
{
    LayerID layerId;
    if (auto* existing = impl_->findPin(pinId)) {
        layerId = existing->layerId;
    } else if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        const auto current = selection->currentLayer();
        if (current) layerId = current->id();
    }
    if (!layerId.isNil()) ensureLayerLoaded(layerId);
    for (auto& [key, lp] : impl_->layerPins) {
        if (lp.mode == Deformation2DMode::Grid) continue;
        auto it = std::remove_if(lp.pins.begin(), lp.pins.end(),
            [&](const PinRecord& p) { return p.id == pinId; });
        if (it != lp.pins.end()) {
            if (!lp.pins.empty()) layerId = lp.pins.front().layerId;
            lp.pins.erase(it, lp.pins.end());
            lp.needsRebind = true;
            lp.needsDeform = true;
            if (lp.engine) {
                lp.engine->reset();
            }
            if (impl_->selectedPinId == pinId) impl_->selectedPinId.clear();
            persistLayerData(layerId);
            return true;
        }
    }
    return false;
}

bool ArtifactPuppetTool::movePin(const QString& pinId, const QPointF& canvasPos)
{
    auto* pin = impl_->findPin(pinId);
    if (!pin || !std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) return false;
    QPointF authoredCanvas = canvasPos;
    if (auto* lp = impl_->getLayerPins(pin->layerId)) {
        auto* selection = ArtifactLayerSelectionManager::instance();
        const auto layer = selection ? selection->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
        if (layer && layer->id() == pin->layerId) {
            authoredCanvas = pinDisplayToAuthoredCanvas(*lp, layer.get(), canvasPos);
        }
    }
    const QPointF previousPos = pin->canvasPos;
    pin->canvasPos = authoredCanvas;

    if (impl_->proportionalEditingEnabled && impl_->proportionalEditRadius > 0.0f) {
        const QPointF delta = authoredCanvas - previousPos;
        const float radius = impl_->proportionalEditRadius;
        for (auto& [key, layerPins] : impl_->layerPins) {
            for (auto& other : layerPins.pins) {
                if (other.id == pinId || other.layerId != pin->layerId) continue;
                const QPointF offset = other.canvasPos - previousPos;
                const float distance = static_cast<float>(std::hypot(offset.x(), offset.y()));
                if (distance >= radius) continue;
                const float t = std::clamp(distance / radius, 0.0f, 1.0f);
                const float weight = (1.0f - t) * (1.0f - t);
                other.canvasPos += delta * weight;
            }
        }
    }

    if (auto* lp = impl_->getLayerPins(pin->layerId)) lp->needsDeform = true;
    return true;
}

bool ArtifactPuppetTool::movePinAtFrame(const QString& pinId,
                                        const QPointF& canvasPos)
{
    auto* pin = impl_->findPin(pinId);
    if (!pin || !std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) {
        return false;
    }
    const auto layer = impl_->getLayerPins(pin->layerId);
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayer = selection ? selection->currentLayer()
                                         : ArtifactAbstractLayerPtr{};
    const auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
        selectedLayer);
    if (!layer || !imageLayer || imageLayer->id() != pin->layerId) {
        return movePin(pinId, canvasPos);
    }
    const QTransform localToCanvas = imageLayer->getGlobalTransform();
    bool transformInvertible = false;
    const QTransform canvasToLocal =
        localToCanvas.inverted(&transformInvertible);
    if (!transformInvertible) return false;
    QPointF targetLocal = canvasToLocal.map(canvasPos);
    if (const auto* pins = impl_->getLayerPins(pin->layerId)) {
        bool cropInvertible = false;
        const QMatrix4x4 displayToLocal =
            pins->meshLocalTransform.inverted(&cropInvertible);
        if (cropInvertible) targetLocal = displayToLocal.map(targetLocal);
    }
    const QString xPath = QStringLiteral("deformation2D.%1.x").arg(pinId);
    const QString yPath = QStringLiteral("deformation2D.%1.y").arg(pinId);
    auto xProperty = imageLayer->persistentLayerProperty(
        xPath, ArtifactCore::PropertyType::Float, targetLocal.x(), 100);
    auto yProperty = imageLayer->persistentLayerProperty(
        yPath, ArtifactCore::PropertyType::Float, targetLocal.y(), 100);
    xProperty->setAnimatable(true);
    yProperty->setAnimatable(true);
    const ArtifactCore::RationalTime time(
        layerTimelineFrame(imageLayer.get()), imageLayer->keyframeTimeScale());
    const QPointF fallbackLocal = canvasToLocal.map(pin->canvasPos);
    const QPointF currentLocal(
        xProperty->isAnimatable() && !xProperty->getKeyFrames().empty()
            ? xProperty->interpolateValue(time).toDouble() : fallbackLocal.x(),
        yProperty->isAnimatable() && !yProperty->getKeyFrames().empty()
            ? yProperty->interpolateValue(time).toDouble() : fallbackLocal.y());
    const QPointF delta = targetLocal - currentLocal;
    if (impl_->proportionalEditingEnabled &&
        impl_->proportionalEditRadius > 0.0f) {
        const float radius = impl_->proportionalEditRadius;
        for (auto& other : layer->pins) {
            if (other.id == pinId) continue;
            const QPointF otherLocal = canvasToLocal.map(other.canvasPos);
            const QPointF offset = otherLocal - currentLocal;
            const float distance = static_cast<float>(
                std::hypot(offset.x(), offset.y()));
            if (distance >= radius) continue;
            const float t = std::clamp(distance / radius, 0.0f, 1.0f);
            const double weight = (1.0 - t) * (1.0 - t);
            other.canvasPos = localToCanvas.map(otherLocal + delta * weight);
        }
    }
    pin->canvasPos = localToCanvas.map(targetLocal);
    if (auto* lp = impl_->getLayerPins(pin->layerId)) {
        lp->needsDeform = true;
    }
    return true;
}

bool ArtifactPuppetTool::commitPinPositionAtFrame(
    const QString& pinId, const QPointF& canvasPos)
{
    auto* pin = impl_->findPin(pinId);
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayer = selection ? selection->currentLayer()
                                         : ArtifactAbstractLayerPtr{};
    const auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
        selectedLayer);
    if (!pin || !imageLayer || imageLayer->id() != pin->layerId ||
        !std::isfinite(canvasPos.x()) ||
        !std::isfinite(canvasPos.y())) return false;
    bool transformInvertible = false;
    const QTransform canvasToLocal =
        imageLayer->getGlobalTransform().inverted(&transformInvertible);
    if (!transformInvertible) return false;
    QPointF local = canvasToLocal.map(canvasPos);
    if (const auto* lp = impl_->getLayerPins(pin->layerId)) {
        bool cropInvertible = false;
        const QMatrix4x4 displayToLocal = lp->meshLocalTransform.inverted(&cropInvertible);
        if (cropInvertible) local = displayToLocal.map(local);
    }
    const ArtifactCore::RationalTime time(
        layerTimelineFrame(imageLayer.get()), imageLayer->keyframeTimeScale());
    for (const auto& pair : {std::pair<QString, double>{
                                 QStringLiteral("deformation2D.%1.x").arg(pinId),
                                 local.x()},
                             std::pair<QString, double>{
                                 QStringLiteral("deformation2D.%1.y").arg(pinId),
                                 local.y()}}) {
        auto property = imageLayer->persistentLayerProperty(
            pair.first, ArtifactCore::PropertyType::Float, pair.second, 100);
        property->setAnimatable(true);
        property->addKeyFrame(time, pair.second);
    }
    persistLayerData(pin->layerId);
    return true;
}

bool ArtifactPuppetTool::restorePinPositionAnimation(
    const LayerID& layerId, const QString& pinId, const QJsonObject& snapshot,
    const QPointF& position)
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayer = selection ? selection->currentLayer()
                                         : ArtifactAbstractLayerPtr{};
    const auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
        selectedLayer);
    if (!imageLayer || imageLayer->id() != layerId) return false;
    QStringList fields{QStringLiteral("x"), QStringLiteral("y")};
    if (snapshot.contains(QStringLiteral("rotation")) ||
        snapshot.contains(QStringLiteral("weight"))) {
        fields.append(QStringLiteral("rotation"));
        fields.append(QStringLiteral("weight"));
    }
    for (const QString& axis : fields) {
        const QString path = QStringLiteral("deformation2D.%1.%2").arg(pinId, axis);
        const bool containsAxis = snapshot.contains(axis);
        const QJsonArray frames = snapshot.value(axis).toArray();
        const double baseValue = snapshot.value(axis + QStringLiteral("Base"))
                                     .toDouble(0.0);
        auto property = imageLayer->persistentLayerProperty(
            path, ArtifactCore::PropertyType::Float, baseValue, 100);
        property->clearKeyFrames();
        for (const QJsonValue& frameValue : frames) {
            if (!frameValue.isObject()) continue;
            const QJsonObject frame = frameValue.toObject();
            const qint64 value = frame.value(QStringLiteral("frame")).toVariant().toLongLong();
            const double coordinate = frame.value(QStringLiteral("value")).toDouble();
            if (!std::isfinite(coordinate)) continue;
            property->addKeyFrame(
                ArtifactCore::RationalTime(value, imageLayer->keyframeTimeScale()),
                coordinate,
                static_cast<ArtifactCore::InterpolationType>(
                    std::clamp(frame.value(QStringLiteral("interpolation"))
                                   .toInt(), 0, 32)),
                static_cast<float>(frame.value(QStringLiteral("cp1_x"))
                                       .toDouble(0.42)),
                static_cast<float>(frame.value(QStringLiteral("cp1_y"))
                                       .toDouble()),
                static_cast<float>(frame.value(QStringLiteral("cp2_x"))
                                       .toDouble(0.58)),
                static_cast<float>(frame.value(QStringLiteral("cp2_y"))
                                       .toDouble(1.0)),
                frame.value(QStringLiteral("roving")).toBool());
            property->setKeyFrameAnchorAt(
                ArtifactCore::RationalTime(value,
                                           imageLayer->keyframeTimeScale()),
                static_cast<ArtifactCore::KeyFrame::Anchor>(
                    std::clamp(frame.value(QStringLiteral("anchor")).toInt(),
                               0, 3)));
            property->setKeyFrameColorLabelAt(
                ArtifactCore::RationalTime(value,
                                           imageLayer->keyframeTimeScale()),
                static_cast<ArtifactCore::KeyFrame::ColorLabel>(
                    std::clamp(frame.value(QStringLiteral("colorLabel"))
                                   .toInt(), 0, 6)));
        }
        property->setAnimatable(containsAxis && !frames.isEmpty());
        imageLayer->syncDeformation2DControlProperty(path);
        if (!containsAxis) {
            imageLayer->removePersistentLayerPropertiesWithPrefix(path);
        }
    }
    if (auto* pin = impl_->findPin(pinId)) {
        pin->canvasPos = position;
        if (snapshot.contains(QStringLiteral("rotationBase"))) {
            const double rotation = snapshot.value(
                QStringLiteral("rotationBase")).toDouble(pin->rotation);
            if (std::isfinite(rotation))
                pin->rotation = static_cast<float>(std::clamp(
                    rotation, -180.0, 180.0));
        }
        if (snapshot.contains(QStringLiteral("weightBase"))) {
            const double weight = snapshot.value(
                QStringLiteral("weightBase")).toDouble(pin->weight);
            if (std::isfinite(weight))
                pin->weight = static_cast<float>(std::clamp(weight, 0.0, 1.0));
        }
    }
    if (auto* lp = impl_->getLayerPins(layerId)) lp->needsDeform = true;
    return true;
}

void ArtifactPuppetTool::discardPinPositionAnimationProperties(
    const LayerID& layerId, const QString& pinId)
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto imageLayer = selection
        ? ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
              selection->currentLayer())
        : ArtifactCore::SharedPtr<ArtifactImageLayer>{};
    if (!imageLayer || imageLayer->id() != layerId) return;
    imageLayer->removePersistentLayerPropertiesWithPrefix(
        QStringLiteral("deformation2D.%1.").arg(pinId));
    imageLayer->setDirty(LayerDirtyFlag::Property);
    imageLayer->changed();
}

void ArtifactPuppetTool::evaluatePinPositionsAtCurrentFrame(
    const LayerID& layerId, ArtifactAbstractLayer* layerOverride)
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto selectedLayer = selection ? selection->currentLayer()
                                         : ArtifactAbstractLayerPtr{};
    ArtifactAbstractLayer* layer =
        layerOverride && layerOverride->id() == layerId
            ? layerOverride : selectedLayer.get();
    auto* lp = impl_->getLayerPins(layerId);
    if (!layer || layer->id() != layerId || !lp) return;
    const qint64 timelineFrame = layerTimelineFrame(layer);
    const QJsonObject deformationState = layer->deformation2DData();
    if (lp->lastEvaluationFrame == timelineFrame &&
        lp->lastEvaluationState == deformationState) return;
    const QTransform localToCanvas = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform canvasToLocal = localToCanvas.inverted(&invertible);
    if (!invertible) return;
    const ArtifactCore::RationalTime time(
        timelineFrame, layer->keyframeTimeScale());
    for (PinRecord& pin : lp->pins) {
        QPointF local = canvasToLocal.map(pin.canvasPos);
        const auto evaluate = [layer, &time, &pin](const QString& axis,
                                                        double fallback) {
            const QString path = QStringLiteral("deformation2D.%1.%2")
                                     .arg(pin.id, axis);
            const auto property = layer->getProperty(path);
            if (property && property->isAnimatable() &&
                !property->getKeyFrames().empty()) {
                const double value = property->interpolateValue(time).toDouble();
                return std::isfinite(value) ? value : fallback;
            }
            const QJsonObject state = layer->deformation2DData();
            const QString mode = state.value(QStringLiteral("mode")).toString();
            const QJsonArray controls = state.value(
                mode == QStringLiteral("grid")
                    ? QStringLiteral("gridControls") : QStringLiteral("pins"))
                .toArray();
            for (const QJsonValue& value : controls) {
                if (!value.isObject()) continue;
                const QJsonObject control = value.toObject();
                if (control.value(QStringLiteral("id")).toString() != pin.id)
                    continue;
                const QJsonArray keys = control.value(
                    axis + QStringLiteral("Keys")).toArray();
                if (keys.isEmpty()) break;
                auto restored = layer->persistentLayerProperty(
                    path, ArtifactCore::PropertyType::Float,
                    control.value(axis).toDouble(fallback), 100);
                restored->setAnimatable(true);
                for (const QJsonValue& keyValue : keys) {
                    if (!keyValue.isObject()) continue;
                    const QJsonObject key = keyValue.toObject();
                    const double keyValueNumber =
                        key.value(QStringLiteral("value")).toDouble();
                    if (!std::isfinite(keyValueNumber)) continue;
                    const qint64 frame = key.value(QStringLiteral("frame"))
                                             .toVariant().toLongLong();
                    const ArtifactCore::RationalTime keyTime(
                        frame, layer->keyframeTimeScale());
                    restored->addKeyFrame(
                        keyTime, keyValueNumber,
                        static_cast<ArtifactCore::InterpolationType>(
                            std::clamp(key.value(QStringLiteral("interpolation"))
                                           .toInt(), 0, 32)),
                        static_cast<float>(key.value(QStringLiteral("cp1_x"))
                                               .toDouble(0.42)),
                        static_cast<float>(key.value(QStringLiteral("cp1_y"))
                                               .toDouble()),
                        static_cast<float>(key.value(QStringLiteral("cp2_x"))
                                               .toDouble(0.58)),
                        static_cast<float>(key.value(QStringLiteral("cp2_y"))
                                               .toDouble(1.0)),
                        key.value(QStringLiteral("roving")).toBool());
                    restored->setKeyFrameAnchorAt(
                        keyTime, static_cast<ArtifactCore::KeyFrame::Anchor>(
                        std::clamp(key.value(QStringLiteral("anchor"))
                                       .toInt(), 0, 3)));
                    restored->setKeyFrameColorLabelAt(
                        keyTime,
                        static_cast<ArtifactCore::KeyFrame::ColorLabel>(
                            std::clamp(key.value(QStringLiteral("colorLabel"))
                                           .toInt(), 0, 6)));
                }
                const double evaluated =
                    restored->interpolateValue(time).toDouble();
                return std::isfinite(evaluated) ? evaluated : fallback;
            }
            return fallback;
        };
        local.setX(evaluate(QStringLiteral("x"), local.x()));
        local.setY(evaluate(QStringLiteral("y"), local.y()));
        if (impl_->getLayerPins(layerId)->mode == Deformation2DMode::Pins) {
            pin.rotation = static_cast<float>(evaluate(
                QStringLiteral("rotation"), pin.rotation));
            pin.weight = static_cast<float>(std::clamp(
                evaluate(QStringLiteral("weight"), pin.weight), 0.0, 1.0));
        }
        pin.canvasPos = localToCanvas.map(local);
    }
    lp->lastEvaluationFrame = timelineFrame;
    lp->lastEvaluationState = deformationState;
    lp->needsDeform = true;
}

QJsonObject ArtifactPuppetTool::pinPositionAnimationSnapshot(
    const LayerID& layerId, const QString& pinId) const
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto imageLayer = selection
        ? ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
              selection->currentLayer())
        : ArtifactCore::SharedPtr<ArtifactImageLayer>{};
    if (!imageLayer || imageLayer->id() != layerId) return {};
    QJsonObject snapshot;
    QStringList fields{QStringLiteral("x"), QStringLiteral("y")};
    const QJsonObject state = imageLayer->deformation2DData();
    if (state.value(QStringLiteral("mode")).toString() !=
        QStringLiteral("grid")) {
        fields.append(QStringLiteral("rotation"));
        fields.append(QStringLiteral("weight"));
    }
    for (const QString& axis : fields) {
        const QString path = QStringLiteral("deformation2D.%1.%2").arg(pinId, axis);
        QJsonArray frames;
        if (const auto property = imageLayer->getProperty(path)) {
            snapshot[axis + QStringLiteral("Base")] =
                QJsonValue::fromVariant(property->getValue());
            for (const auto& key : property->getKeyFrames()) {
                frames.append(QJsonObject{
                    {QStringLiteral("frame"),
                     QString::number(key.time.rescaledTo(
                         imageLayer->keyframeTimeScale()))},
                    {QStringLiteral("value"),
                     QJsonValue::fromVariant(key.value)}});
                QJsonObject savedKey = frames.last().toObject();
                savedKey[QStringLiteral("interpolation")]
                    = static_cast<int>(key.interpolation);
                savedKey[QStringLiteral("cp1_x")] = key.cp1_x;
                savedKey[QStringLiteral("cp1_y")] = key.cp1_y;
                savedKey[QStringLiteral("cp2_x")] = key.cp2_x;
                savedKey[QStringLiteral("cp2_y")] = key.cp2_y;
                savedKey[QStringLiteral("roving")] = key.roving;
                savedKey[QStringLiteral("anchor")] =
                    static_cast<int>(key.anchor);
                savedKey[QStringLiteral("colorLabel")]
                    = static_cast<int>(key.colorLabel);
                frames.replace(frames.size() - 1, savedKey);
            }
            snapshot[axis] = frames;
        }
    }
    return snapshot;
}

bool ArtifactPuppetTool::isProportionalEditingEnabled() const
{
    return impl_->proportionalEditingEnabled;
}

void ArtifactPuppetTool::setProportionalEditingEnabled(bool enabled)
{
    impl_->proportionalEditingEnabled = enabled;
}

float ArtifactPuppetTool::proportionalEditRadius() const
{
    return impl_->proportionalEditRadius;
}

void ArtifactPuppetTool::setProportionalEditRadius(float radius)
{
    if (std::isfinite(radius)) {
        impl_->proportionalEditRadius = std::clamp(radius, 1.0f, 100000.0f);
    }
}

QPointF ArtifactPuppetTool::pinPosition(const QString& pinId) const
{
    if (const auto* pin = impl_->findPin(pinId)) {
        const auto* lp = impl_->getLayerPins(pin->layerId);
        auto* selection = ArtifactLayerSelectionManager::instance();
        const auto layer = selection ? selection->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
        if (lp && layer && layer->id() == pin->layerId) {
            return pinAuthoredToDisplayCanvas(*lp, layer.get(), pin->canvasPos);
        }
        return pin->canvasPos;
    }
    return {};
}

LayerID ArtifactPuppetTool::pinLayerId(const QString& pinId) const
{
    const auto* pin = impl_->findPin(pinId);
    return pin ? pin->layerId : LayerID{};
}

float ArtifactPuppetTool::pinRotation(const QString& pinId) const
{
    if (const auto* pin = impl_->findPin(pinId)) return pin->rotation;
    return 0.0f;
}

void ArtifactPuppetTool::setPinRotation(const QString& pinId, float degrees)
{
    if (auto* pin = impl_->findPin(pinId)) {
        pin->rotation = std::isfinite(degrees)
            ? std::fmod(degrees + 180.0f, 360.0f)
            : 0.0f;
        if (pin->rotation < 0.0f) pin->rotation += 360.0f;
        pin->rotation -= 180.0f;
        if (auto* lp = impl_->getLayerPins(pin->layerId)) lp->needsDeform = true;
        auto* selection = ArtifactLayerSelectionManager::instance();
        const auto layer = selection ? selection->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
        if (layer && layer->id() == pin->layerId) {
            const QString path = QStringLiteral("deformation2D.%1.rotation")
                                     .arg(pinId);
            const auto property = layer->getProperty(path);
            if (property && property->isAnimatable()) {
                property->addKeyFrame(
                    ArtifactCore::RationalTime(layerTimelineFrame(layer.get()),
                                               layer->keyframeTimeScale()),
                    pin->rotation);
                layer->syncDeformation2DControlProperty(path);
            }
            persistLayerData(pin->layerId);
        }
    }
}

float ArtifactPuppetTool::pinWeight(const QString& pinId) const
{
    if (const auto* pin = impl_->findPin(pinId)) return pin->weight;
    return 1.0f;
}

void ArtifactPuppetTool::setPinWeight(const QString& pinId, float weight)
{
    if (auto* pin = impl_->findPin(pinId)) {
        const LayerID layerId = pin->layerId;
        pin->weight = std::isfinite(weight) ? std::clamp(weight, 0.0f, 1.0f) : 1.0f;
        if (auto* lp = impl_->getLayerPins(layerId)) lp->needsDeform = true;
        auto* selection = ArtifactLayerSelectionManager::instance();
        const auto layer = selection ? selection->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
        if (layer && layer->id() == layerId) {
            const QString path = QStringLiteral("deformation2D.%1.weight")
                                     .arg(pinId);
            const auto property = layer->getProperty(path);
            if (property && property->isAnimatable()) {
                property->addKeyFrame(
                    ArtifactCore::RationalTime(layerTimelineFrame(layer.get()),
                                               layer->keyframeTimeScale()),
                    pin->weight);
                layer->syncDeformation2DControlProperty(path);
            }
        }
        persistLayerData(layerId);
    }
}

float ArtifactPuppetTool::pinDepth(const QString& pinId) const
{
    if (const auto* pin = impl_->findPin(pinId)) return pin->depth;
    return 0.0f;
}

void ArtifactPuppetTool::setPinDepth(const QString& pinId, float depth)
{
    if (auto* pin = impl_->findPin(pinId)) {
        const LayerID layerId = pin->layerId;
        pin->depth = std::isfinite(depth) ? std::clamp(depth, -1.0f, 1.0f) : 0.0f;
        if (auto* lp = impl_->getLayerPins(layerId)) lp->needsDeform = true;
        persistLayerData(layerId);
    }
}

QString ArtifactPuppetTool::hitTestPin(const QPointF& canvasPos, float threshold) const
{
    LayerID activeLayerId;
    ArtifactAbstractLayerPtr activeLayer;
    if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        const auto current = selection->currentLayer();
        if (current) {
            activeLayer = current;
            activeLayerId = current->id();
            auto* mutableThis = const_cast<ArtifactPuppetTool*>(this);
            mutableThis->ensureLayerLoaded(current->id(), current.get());
            mutableThis->rebaseLayerPins(current->id(), current.get());
        }
    }
    if (!std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) return {};
    const float safeThreshold = std::isfinite(threshold)
        ? std::clamp(threshold, 0.0f, 100000.0f) : 12.0f;
    const float threshSq = safeThreshold * safeThreshold;
    QString closestId;
    float closestDistSq = threshSq;

    for (const auto& [key, lp] : impl_->layerPins) {
        for (const auto& pin : lp.pins) {
            if (pin.layerId != activeLayerId) continue;
            const QPointF displayPos = pinAuthoredToDisplayCanvas(
                lp, activeLayer.get(), pin.canvasPos);
            const QPointF d = displayPos - canvasPos;
            const float distSq = static_cast<float>(QPointF::dotProduct(d, d));
            if (distSq < closestDistSq) {
                closestDistSq = distSq;
                closestId = pin.id;
            }
        }
    }
    return closestId;
}

void ArtifactPuppetTool::deformLayer(const LayerID& layerId, ArtifactIRenderer* renderer)
{
    Q_UNUSED(renderer);
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) return;
    auto layer = selection->currentLayer();
    if (!layer || layer->id() != layerId) return;
    ensureLayerLoaded(layerId);
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp || !lp->engine) return;
    lp->needsDeform = true;
}

bool ArtifactPuppetTool::renderDeformedLayer(
    ArtifactIRenderer* renderer, ArtifactImageLayer* imageLayer,
    const QMatrix4x4& transform, float opacity)
{
    if (!renderer || !imageLayer || !imageLayer->hasCurrentFrameBuffer()) {
        return false;
    }
    imageLayer->refreshAnimatedSourceCrop();
    const auto& source = imageLayer->currentFrameBuffer();
    const SourceCropDrawLayout cropLayout = imageLayer->sourceCropDrawLayout();
    const QRect cropPixels = cropLayout.sourcePixelRect.intersected(
        QRect(0, 0, source.width(), source.height()));
    if (!cropPixels.isValid() || cropPixels.width() <= 0 ||
        cropPixels.height() <= 0 || !cropLayout.outputLocalRect.isValid() ||
        cropLayout.outputLocalRect.width() <= 0.0 ||
        cropLayout.outputLocalRect.height() <= 0.0) return false;
    const QJsonObject deformerState = imageLayer->deformation2DData();
    if (!deformerState.value(QStringLiteral("enabled")).toBool(true)) {
        return false;
    }

    const LayerID layerId = imageLayer->id();
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) {
        const QJsonObject savedData = imageLayer->deformation2DData();
        const QString mode = savedData.value(QStringLiteral("mode")).toString();
        const QJsonArray savedControls = savedData.value(
            mode == QStringLiteral("grid")
                ? QStringLiteral("gridControls") : QStringLiteral("pins"))
                .toArray();
        if (savedControls.isEmpty()) {
            return false;
        }
        ensureLayerLoaded(layerId, imageLayer);
        lp = impl_->getLayerPins(layerId);
    }
    if (!lp || !lp->engine) return false;
    const size_t expectedGridCount = static_cast<size_t>(lp->gridColumns * lp->gridRows);
    if ((lp->mode == Deformation2DMode::Grid &&
         lp->pins.size() != expectedGridCount) ||
        (lp->mode == Deformation2DMode::Pins && lp->pins.empty())) return false;
    ensureLayerLoaded(layerId, imageLayer);
    rebaseLayerPins(layerId, imageLayer);
    evaluatePinPositionsAtCurrentFrame(layerId, imageLayer);
    const qint64 evaluationFrame = layerTimelineFrame(imageLayer);
    if (lp->lastEvaluationFrame != evaluationFrame) {
        lp->needsDeform = true;
        lp->lastEvaluationFrame = evaluationFrame;
    }

    const std::uint64_t sourceVersion = imageLayer->sourceVersion();
    const qint64 sourceFrameIndex = imageLayer->isImageSequence()
        ? imageLayer->sequenceCachedFrameIndex() : -1;
    const qint64 sourceFrameContentKey = imageLayer->isImageSequence()
        ? imageLayer->sequenceCachedFrameContentKey() : -1;
    const bool cropRectChanged = lp->sourceCropPixels != cropPixels;
    const bool cropMappingChanged = lp->meshDisplayRect != cropLayout.outputLocalRect;
    if (cropMappingChanged) lp->needsDeform = true;
    const bool sourceIdentityChanged = lp->sourceVersion != sourceVersion ||
        lp->sourceFrameIndex != sourceFrameIndex ||
        lp->sourceFrameContentKey != sourceFrameContentKey;
    if (lp->needsRebind || sourceIdentityChanged || cropRectChanged) {
        if (source.width() <= 0 || source.height() <= 0 ||
            source.rgba32fData() == nullptr) {
            lp->needsRebind = true;
            return false;
        }
        const cv::Mat sourceView = source.toCVMat();
        if (sourceView.empty() || sourceView.cols != source.width() ||
            sourceView.rows != source.height() || sourceView.channels() != 4) {
            lp->needsRebind = true;
            return false;
        }
        // Bind topology from the first resolved source. Sequence frames keep
        // their own GPU texture; unchanged dimensions reuse this mesh topology.
        const bool sourceTopologyChanged = lp->needsRebind || cropRectChanged ||
            lp->sourceWidth != cropPixels.width() ||
            lp->sourceHeight != cropPixels.height() ||
            (!imageLayer->isImageSequence() && lp->sourceVersion != sourceVersion);
        if (sourceTopologyChanged) {
            cv::Mat topologySource;
            const cv::Rect cropRoi(cropPixels.x(), cropPixels.y(),
                                   cropPixels.width(), cropPixels.height());
            const cv::Mat sourceCropView = sourceView(cropRoi);
            sourceCropView.convertTo(topologySource, CV_8UC4, 255.0);
            if (topologySource.empty()) {
                lp->needsRebind = true;
                return false;
            }
            if (imageLayer->isImageSequence()) {
                // Sequence silhouettes can move beyond the first frame's
                // alpha contour. Use a bounded full-frame topology for the
                // sequence while the per-frame GPU texture retains authored
                // alpha. This allocation and fill happen only on cold bind.
                for (int y = 0; y < topologySource.rows; ++y) {
                    auto* row = topologySource.ptr<cv::Vec4b>(y);
                    for (int x = 0; x < topologySource.cols; ++x) {
                        row[x][3] = 255;
                    }
                }
            }
            lp->engine->bindImage(topologySource, 10);
            lp->sourceWidth = cropPixels.width();
            lp->sourceHeight = cropPixels.height();
            lp->sourceCropPixels = cropPixels;
            lp->needsDeform = true;
            if (lp->mode == Deformation2DMode::Grid &&
                !lp->engine->configureGrid(lp->gridColumns, lp->gridRows)) {
                lp->needsRebind = true;
                return false;
            }
        }
        lp->sourceVersion = sourceVersion;
        lp->sourceFrameIndex = sourceFrameIndex;
        lp->sourceFrameContentKey = sourceFrameContentKey;
        lp->needsRebind = false;
    }
    lp->meshDisplayRect = cropLayout.outputLocalRect;
    lp->meshLocalTransform = cropLayout.localTransform;

    if (lp->needsDeform) {
        if (lp->mode == Deformation2DMode::Grid &&
            (!lp->engine->hasGrid() ||
             lp->engine->gridColumns() != lp->gridColumns ||
             lp->engine->gridRows() != lp->gridRows)) {
            if (!lp->engine->configureGrid(lp->gridColumns, lp->gridRows)) {
                return false;
            }
        }
        bool invertible = false;
        const QTransform canvasToLocal =
            imageLayer->getGlobalTransform().inverted(&invertible);
        const QRectF displayRect = cropLayout.outputLocalRect;
        if (!invertible || displayRect.width() <= 0.0 ||
            displayRect.height() <= 0.0) {
            return false;
        }
        const auto evaluateControlCoordinate =
            [imageLayer, evaluationFrame](const PinRecord& pin, bool xAxis) {
                bool transformInvertible = false;
                const QTransform localToCanvas = imageLayer->getGlobalTransform();
                const QPointF local = localToCanvas.inverted(&transformInvertible)
                                          .map(pin.canvasPos);
                const double base = transformInvertible
                    ? (xAxis ? local.x() : local.y())
                    : (xAxis ? pin.canvasPos.x() : pin.canvasPos.y());
                const QString propertyPath = QStringLiteral("deformation2D.%1.%2")
                    .arg(pin.id, xAxis ? QStringLiteral("x")
                                       : QStringLiteral("y"));
                double result = base;
                if (const auto property = imageLayer->getProperty(propertyPath);
                    property && property->isAnimatable() &&
                    !property->getKeyFrames().empty()) {
                    const ArtifactCore::RationalTime time(
                        evaluationFrame, imageLayer->keyframeTimeScale());
                    const QVariant value = property->interpolateValue(time);
                    if (value.isValid() && std::isfinite(value.toDouble())) {
                        result = value.toDouble();
                    }
                }
                if (const auto* stack =
                        static_cast<const ArtifactImageLayer*>(imageLayer)
                            ->animationLayerStack(propertyPath);
                    stack && stack->layerCount() > 0) {
                    result = stack->evaluateWithBase(
                        ArtifactCore::FramePosition(evaluationFrame),
                        static_cast<float>(result));
                }
                if (!std::isfinite(result)) {
                    result = base;
                }
                return result;
            };
        const auto toSource = [&](const QPointF& canvasPoint) {
            const QPointF local = canvasToLocal.map(canvasPoint);
            return cv::Point2f(
                static_cast<float>((local.x() - displayRect.x()) * lp->sourceWidth /
                                   displayRect.width()),
                static_cast<float>((local.y() - displayRect.y()) * lp->sourceHeight /
                                   displayRect.height()));
        };
        if (lp->mode == Deformation2DMode::Grid) {
            if (!lp->engine->hasGrid() ||
                lp->engine->gridColumns() != lp->gridColumns ||
                lp->engine->gridRows() != lp->gridRows) {
                if (!lp->engine->configureGrid(lp->gridColumns, lp->gridRows)) {
                    return false;
                }
            }
            const size_t gridCount = static_cast<size_t>(lp->gridColumns * lp->gridRows);
            if (lp->pins.size() != gridCount) return false;
            if (lp->gridPositionScratch.size() != gridCount) {
                lp->gridPositionScratch.resize(gridCount);
            }
            for (size_t i = 0; i < gridCount; ++i) {
                const PinRecord& control = lp->pins[i];
                const QPointF local(
                    evaluateControlCoordinate(control, true),
                    evaluateControlCoordinate(control, false));
                lp->gridPositionScratch[i] = toSource(
                    imageLayer->getGlobalTransform().map(local));
            }
            if (!lp->engine->setGridVertices(lp->gridPositionScratch)) return false;
        } else {
            auto& enginePins = lp->enginePinScratch;
            if (enginePins.size() < lp->pins.size()) {
                enginePins.resize(lp->pins.size());
            }
            size_t outputPin = 0;
            for (const PinRecord& pin : lp->pins) {
                if (!std::isfinite(pin.originalPos.x()) ||
                    !std::isfinite(pin.originalPos.y()) ||
                    !std::isfinite(pin.canvasPos.x()) ||
                    !std::isfinite(pin.canvasPos.y())) continue;
                ArtifactCore::PuppetPin& enginePin = enginePins[outputPin++];
                enginePin.id = pin.engineId;
                enginePin.originalPosition = toSource(pin.originalPos);
                const QPointF evaluatedLocal(
                    evaluateControlCoordinate(pin, true),
                    evaluateControlCoordinate(pin, false));
                enginePin.currentPosition = toSource(
                    imageLayer->getGlobalTransform().map(evaluatedLocal));
                enginePin.type = static_cast<ArtifactCore::PuppetPinType>(pin.type);
                enginePin.weight = std::isfinite(pin.weight)
                    ? std::clamp(pin.weight, 0.0f, 1.0f) : 1.0f;
                enginePin.rotation =
                    (std::isfinite(pin.rotation) ? pin.rotation : 0.0f) *
                    0.017453292519943295f;
                enginePin.depth = std::isfinite(pin.depth)
                    ? std::clamp(pin.depth, -1.0f, 1.0f) : 0.0f;
            }
            enginePins.resize(outputPin);
            lp->engine->setPins(enginePins);
        }
        const ArtifactCore::PuppetMesh& mesh = lp->engine->deformedMeshView();
        lp->triangleOrder.resize(mesh.indices.size() / 3);
        for (size_t triangle = 0; triangle < lp->triangleOrder.size(); ++triangle) {
            lp->triangleOrder[triangle] = triangle;
        }
        if (mesh.zDepth.size() == mesh.vertices.size()) {
            std::sort(lp->triangleOrder.begin(), lp->triangleOrder.end(),
                [&mesh](size_t left, size_t right) {
                    const auto depth = [&mesh](size_t triangle) {
                        const size_t offset = triangle * 3;
                        const int a = mesh.indices[offset];
                        const int b = mesh.indices[offset + 1];
                        const int c = mesh.indices[offset + 2];
                        if (a < 0 || b < 0 || c < 0 ||
                            a >= static_cast<int>(mesh.zDepth.size()) ||
                            b >= static_cast<int>(mesh.zDepth.size()) ||
                            c >= static_cast<int>(mesh.zDepth.size())) return 0.0f;
                        return (mesh.zDepth[static_cast<size_t>(a)] +
                                mesh.zDepth[static_cast<size_t>(b)] +
                                mesh.zDepth[static_cast<size_t>(c)]) / 3.0f;
                    };
                    const float leftDepth = depth(left);
                    const float rightDepth = depth(right);
                    return leftDepth == rightDepth ? left < right
                                                   : leftDepth < rightDepth;
                });
        }
        lp->needsDeform = false;
    }

    Diligent::ITextureView* texture = renderer->textureForImage(source);
    if (!texture) return false;
    const ArtifactCore::PuppetMesh& mesh = lp->engine->deformedMeshView();
    if (mesh.vertices.empty() || mesh.indices.size() < 3 ||
        mesh.texCoords.size() != mesh.vertices.size() ||
        lp->sourceWidth <= 0 || lp->sourceHeight <= 0) return false;

    const auto localPoint = [&](const cv::Point2f& point) {
        return QPointF(cropLayout.outputLocalRect.x() +
                           point.x * cropLayout.outputLocalRect.width() / lp->sourceWidth,
                       cropLayout.outputLocalRect.y() +
                           point.y * cropLayout.outputLocalRect.height() / lp->sourceHeight);
    };
    const QMatrix4x4 croppedTransform = transform * cropLayout.localTransform;
    const float safeOpacity = std::isfinite(opacity)
        ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
    for (const size_t triangle : lp->triangleOrder) {
        const size_t i = triangle * 3;
        if (i + 2 >= mesh.indices.size()) continue;
        const int i0 = mesh.indices[i];
        const int i1 = mesh.indices[i + 1];
        const int i2 = mesh.indices[i + 2];
        if (i0 < 0 || i1 < 0 || i2 < 0 ||
            i0 >= static_cast<int>(mesh.vertices.size()) ||
            i1 >= static_cast<int>(mesh.vertices.size()) ||
            i2 >= static_cast<int>(mesh.vertices.size())) continue;
        const QPointF p0 = localPoint(mesh.vertices[static_cast<size_t>(i0)]);
        const QPointF p1 = localPoint(mesh.vertices[static_cast<size_t>(i1)]);
        const QPointF p2 = localPoint(mesh.vertices[static_cast<size_t>(i2)]);
        const auto textureUv = [&](int index) {
            const cv::Point2f uv = mesh.texCoords[static_cast<size_t>(index)];
            return cv::Point2f(
                (cropPixels.x() + uv.x * cropPixels.width()) / source.width(),
                (cropPixels.y() + uv.y * cropPixels.height()) / source.height());
        };
        const cv::Point2f uv0 = textureUv(i0);
        const cv::Point2f uv1 = textureUv(i1);
        const cv::Point2f uv2 = textureUv(i2);
        renderer->drawTexturedTriangleTransformed(
            {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
            {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
            {static_cast<float>(p2.x()), static_cast<float>(p2.y())},
            {uv0.x, uv0.y}, {uv1.x, uv1.y}, {uv2.x, uv2.y},
            croppedTransform, texture, safeOpacity);
    }
    return true;
}

QPointF ArtifactPuppetTool::mapDeformationPoint(
    ArtifactAbstractLayer* layer, const QPointF& localPoint)
{
    if (!layer || !std::isfinite(localPoint.x()) ||
        !std::isfinite(localPoint.y())) return localPoint;
    const LayerID layerId = layer->id();
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp || !lp->enabled || lp->pins.empty()) return localPoint;
    if (!lp->hasCanvasToLocal) return localPoint;
    const QTransform& canvasToLocal = lp->canvasToLocal;
    const QRectF& bounds = lp->layerBounds;
    if (lp->mode == Deformation2DMode::Pins) {
        QPointF pStar;
        QPointF qStar;
        double sumWeight = 0.0;
        bool exactControl = false;
        QPointF exactTarget;
        const auto visitConstraints = [&](auto&& visit) {
            for (const PinRecord& pin : lp->pins) {
                if (pin.type == 3) continue; // Overlap changes depth only.
                const QPointF p = canvasToLocal.map(pin.originalPos);
                const QPointF q = canvasToLocal.map(
                    pin.type == 1 ? pin.originalPos : pin.canvasPos);
                const double weight = std::clamp(
                    static_cast<double>(std::isfinite(pin.weight) ? pin.weight : 1.0f),
                    0.0, 1.0);
                if (pin.type != 2) {
                    visit(p, q, weight * (pin.type == 1 ? 50.0 : 1.0));
                    continue;
                }
                visit(p, q, weight);
                const double radius = 20.0 * weight;
                const double angle = (std::isfinite(pin.rotation)
                    ? pin.rotation : 0.0f) * 0.017453292519943295;
                const double cosine = std::cos(angle);
                const double sine = std::sin(angle);
                const QPointF offsets[] = {
                    {radius, 0.0}, {-radius, 0.0},
                    {0.0, radius}, {0.0, -radius}};
                for (const QPointF& offset : offsets) {
                    const QPointF rotated(offset.x() * cosine - offset.y() * sine,
                                          offset.x() * sine + offset.y() * cosine);
                    visit(p + offset, q + rotated, weight * 0.5);
                }
            }
        };
        visitConstraints([&](const QPointF& p, const QPointF& q, double weight) {
            const QPointF delta = localPoint - p;
            const double distanceSquared = QPointF::dotProduct(delta, delta);
            if (distanceSquared < 1e-4) {
                exactControl = true;
                exactTarget = q;
                return;
            }
            const double w = weight / (distanceSquared + 1e-8);
            sumWeight += w;
            pStar += p * w;
            qStar += q * w;
        });
        if (exactControl) return exactTarget;
        if (!(sumWeight > 0.0) || !std::isfinite(sumWeight)) return localPoint;
        const double inverseWeight = 1.0 / sumWeight;
        pStar *= inverseWeight;
        qStar *= inverseWeight;
        double mu = 0.0;
        visitConstraints([&](const QPointF& p, const QPointF&, double weight) {
            const QPointF delta = localPoint - p;
            const double distanceSquared = QPointF::dotProduct(delta, delta);
            const double w = (weight / (distanceSquared + 1e-8)) * inverseWeight;
            const QPointF pHat = p - pStar;
            mu += w * QPointF::dotProduct(pHat, pHat);
        });
        const QPointF vHat = localPoint - pStar;
        QPointF mapped = qStar;
        if (mu > 1e-6 && std::isfinite(mu)) {
            double a = 0.0;
            double b = 0.0;
            visitConstraints([&](const QPointF& p, const QPointF& q, double weight) {
                const QPointF delta = localPoint - p;
                const double distanceSquared = QPointF::dotProduct(delta, delta);
                const double w = (weight / (distanceSquared + 1e-8)) * inverseWeight;
                const QPointF pHat = p - pStar;
                const QPointF qHat = q - qStar;
                a += w * QPointF::dotProduct(pHat, qHat);
                b += w * (pHat.x() * qHat.y() - pHat.y() * qHat.x());
            });
            mapped += QPointF((a * vHat.x() - b * vHat.y()) / mu,
                              (b * vHat.x() + a * vHat.y()) / mu);
        } else {
            mapped += vHat;
        }
        return std::isfinite(mapped.x()) && std::isfinite(mapped.y())
            ? mapped : localPoint;
    }
    if (lp->gridColumns < 2 || lp->gridRows < 2 ||
        lp->pins.size() != static_cast<size_t>(lp->gridColumns * lp->gridRows) ||
        !bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0)
        return localPoint;
    const double gx = std::clamp(
        (localPoint.x() - bounds.x()) / bounds.width() *
            static_cast<double>(lp->gridColumns - 1),
        0.0, static_cast<double>(lp->gridColumns - 1));
    const double gy = std::clamp(
        (localPoint.y() - bounds.y()) / bounds.height() *
            static_cast<double>(lp->gridRows - 1),
        0.0, static_cast<double>(lp->gridRows - 1));
    const int column = std::min(static_cast<int>(gx), lp->gridColumns - 2);
    const int row = std::min(static_cast<int>(gy), lp->gridRows - 2);
    const double tx = gx - column;
    const double ty = gy - row;
    const auto at = [&](int y, int x) {
        const QPointF canvas = lp->pins[static_cast<size_t>(y * lp->gridColumns + x)].canvasPos;
        return canvasToLocal.map(canvas);
    };
    const QPointF top = at(row, column) * (1.0 - tx) +
                        at(row, column + 1) * tx;
    const QPointF bottom = at(row + 1, column) * (1.0 - tx) +
                           at(row + 1, column + 1) * tx;
    const QPointF mapped = top * (1.0 - ty) + bottom * ty;
    return std::isfinite(mapped.x()) && std::isfinite(mapped.y())
        ? mapped : localPoint;
}

bool ArtifactPuppetTool::prepareLayerDeformation(ArtifactAbstractLayer* layer)
{
    if (!layer) return false;
    const LayerID layerId = layer->id();
    ensureLayerLoaded(layerId, layer);
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return false;
    lp->enabled = layer->deformation2DData()
                      .value(QStringLiteral("enabled")).toBool(true);
    if (!lp->enabled) return false;
    rebaseLayerPins(layerId, layer);
    bool invertible = false;
    lp->canvasToLocal = layer->getGlobalTransform().inverted(&invertible);
    lp->layerBounds = layer->localBounds();
    lp->hasCanvasToLocal = invertible && lp->layerBounds.isValid() &&
        lp->layerBounds.width() > 0.0 && lp->layerBounds.height() > 0.0;
    if (!lp->hasCanvasToLocal) return false;
    evaluatePinPositionsAtCurrentFrame(layerId, layer);
    if (lp->mode == Deformation2DMode::Grid) {
        const size_t expectedGridCount =
            static_cast<size_t>(lp->gridColumns * lp->gridRows);
        return lp->pins.size() == expectedGridCount;
    }
    return !lp->pins.empty();
}

void ArtifactPuppetTool::clearPins(const LayerID& layerId)
{
    ensureLayerLoaded(layerId);
    auto* lp = impl_->getLayerPins(layerId);
    if (lp) {
        lp->pins.clear();
        lp->mode = Deformation2DMode::Pins;
        lp->needsRebind = true;
        lp->needsDeform = true;
        if (lp->engine) lp->engine->reset();
    }
    impl_->selectedPinId.clear();
    persistLayerData(layerId);
}

QString ArtifactPuppetTool::selectedPinId() const
{
    return impl_->selectedPinId;
}

void ArtifactPuppetTool::setSelectedPinId(const QString& pinId)
{
    const QString normalized = pinId.trimmed().left(256);
    impl_->selectedPinId = normalized.isEmpty() || !impl_->findPin(normalized)
        ? QString()
        : normalized;
}

void ArtifactPuppetTool::renderOverlay(ArtifactIRenderer* renderer, const LayerID& layerId) const
{
    if (!renderer) return;
    if (auto* selection = ArtifactLayerSelectionManager::instance()) {
        const auto layer = selection->currentLayer();
        if (layer && layer->id() == layerId) {
            auto* mutableThis = const_cast<ArtifactPuppetTool*>(this);
            mutableThis->ensureLayerLoaded(layerId, layer.get());
            mutableThis->rebaseLayerPins(layerId, layer.get());
        }
    }

    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return;

    if (lp->engine) {
        const ArtifactCore::PuppetMesh& mesh = lp->engine->deformedMeshView();
        QTransform meshToCanvas;
        QRectF meshBounds;
        if (auto* selection = ArtifactLayerSelectionManager::instance()) {
            const auto layer = selection->currentLayer();
            if (layer && layer->id() == layerId && lp->sourceWidth > 0 &&
                lp->sourceHeight > 0) {
                meshToCanvas = layer->getGlobalTransform();
                meshBounds = lp->meshDisplayRect.isValid()
                    ? lp->meshDisplayRect : layer->localBounds();
            }
        }
        const auto meshPointToCanvas = [&](const cv::Point2f& point) {
            if (lp->sourceWidth <= 0 || lp->sourceHeight <= 0 ||
                !meshBounds.isValid()) return QPointF(point.x, point.y);
            const QPointF local(
                meshBounds.x() + point.x * meshBounds.width() / lp->sourceWidth,
                meshBounds.y() + point.y * meshBounds.height() / lp->sourceHeight);
            return meshToCanvas.map(lp->meshLocalTransform.map(local));
        };
        const ArtifactCore::FloatColor meshColor{0.35f, 0.82f, 1.0f, 0.34f};
        const auto drawEdge = [&](int first, int second) {
            if (first < 0 || second < 0 ||
                first >= static_cast<int>(mesh.vertices.size()) ||
                second >= static_cast<int>(mesh.vertices.size())) {
                return;
            }
            const auto &a = mesh.vertices[static_cast<size_t>(first)];
            const auto &b = mesh.vertices[static_cast<size_t>(second)];
            if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
                !std::isfinite(b.x) || !std::isfinite(b.y)) {
                return;
            }
            const QPointF canvasA = meshPointToCanvas(a);
            const QPointF canvasB = meshPointToCanvas(b);
            renderer->drawSolidLine(
                {static_cast<float>(canvasA.x()), static_cast<float>(canvasA.y())},
                {static_cast<float>(canvasB.x()), static_cast<float>(canvasB.y())},
                meshColor, 0.8f);
        };
        if (lp->mode == Deformation2DMode::Grid && lp->engine->hasGrid()) {
            const auto& grid = lp->engine->gridVerticesView();
            const int columns = lp->engine->gridColumns();
            const int rows = lp->engine->gridRows();
            const auto drawGridEdge = [&](size_t first, size_t second) {
                if (first >= grid.size() || second >= grid.size()) return;
                const QPointF a = meshPointToCanvas(grid[first]);
                const QPointF b = meshPointToCanvas(grid[second]);
                renderer->drawSolidLine(
                    {static_cast<float>(a.x()), static_cast<float>(a.y())},
                    {static_cast<float>(b.x()), static_cast<float>(b.y())},
                    meshColor, 1.0f);
            };
            for (int row = 0; row < rows; ++row) {
                for (int column = 0; column < columns; ++column) {
                    const size_t index = static_cast<size_t>(row * columns + column);
                    if (column + 1 < columns) drawGridEdge(index, index + 1);
                    if (row + 1 < rows) {
                        drawGridEdge(index, index + static_cast<size_t>(columns));
                    }
                }
            }
        } else if (lp->mode == Deformation2DMode::Grid &&
                   lp->gridColumns >= 2 && lp->gridRows >= 2 &&
                   lp->pins.size() == static_cast<size_t>(
                       lp->gridColumns * lp->gridRows)) {
            const auto drawControlEdge = [&](size_t first, size_t second) {
                if (first >= lp->pins.size() || second >= lp->pins.size()) return;
                ArtifactAbstractLayerPtr layer;
                if (auto* selection = ArtifactLayerSelectionManager::instance())
                    layer = selection->currentLayer();
                const QPointF a = pinAuthoredToDisplayCanvas(
                    *lp, layer.get(), lp->pins[first].canvasPos);
                const QPointF b = pinAuthoredToDisplayCanvas(
                    *lp, layer.get(), lp->pins[second].canvasPos);
                renderer->drawSolidLine(
                    {static_cast<float>(a.x()), static_cast<float>(a.y())},
                    {static_cast<float>(b.x()), static_cast<float>(b.y())},
                    meshColor, 1.0f);
            };
            for (int row = 0; row < lp->gridRows; ++row) {
                for (int column = 0; column < lp->gridColumns; ++column) {
                    const size_t index = static_cast<size_t>(
                        row * lp->gridColumns + column);
                    if (column + 1 < lp->gridColumns)
                        drawControlEdge(index, index + 1);
                    if (row + 1 < lp->gridRows)
                        drawControlEdge(index, index +
                            static_cast<size_t>(lp->gridColumns));
                }
            }
        } else {
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const int a = mesh.indices[i];
                const int b = mesh.indices[i + 1];
                const int c = mesh.indices[i + 2];
                drawEdge(a, b);
                drawEdge(b, c);
                drawEdge(c, a);
            }
        }
    }

    for (const auto& pin : lp->pins) {
        if (!std::isfinite(pin.canvasPos.x()) ||
            !std::isfinite(pin.canvasPos.y())) {
            continue;
        }
        ArtifactAbstractLayerPtr layer;
        if (auto* selection = ArtifactLayerSelectionManager::instance())
            layer = selection->currentLayer();
        const QPointF displayPosition = pinAuthoredToDisplayCanvas(
            *lp, layer.get(), pin.canvasPos);
        const float x = static_cast<float>(displayPosition.x());
        const float y = static_cast<float>(displayPosition.y());
        const bool selected = (pin.id == impl_->selectedPinId);

        // Pin color based on type
        ArtifactCore::FloatColor color;
        switch (pin.type) {
        case 0: color = ArtifactCore::FloatColor{1.0f, 1.0f, 0.0f, 1.0f}; break; // Position: yellow
        case 1: color = ArtifactCore::FloatColor{0.0f, 1.0f, 1.0f, 1.0f}; break; // Starch: cyan
        case 2: color = ArtifactCore::FloatColor{1.0f, 0.5f, 0.0f, 1.0f}; break; // Bend: orange
        case 3: color = ArtifactCore::FloatColor{1.0f, 0.0f, 1.0f, 1.0f}; break; // Overlap: magenta
        case 4: color = ArtifactCore::FloatColor{0.25f, 0.85f, 1.0f, 1.0f}; break; // Grid
        default: color = ArtifactCore::FloatColor{1.0f, 1.0f, 0.0f, 1.0f};
        }

        const float rawZoom = renderer->getZoom();
        const float zoom = std::isfinite(rawZoom)
            ? std::clamp(rawZoom, 0.001f, 10000.0f) : 1.0f;
        const float pinSize = (selected ? 11.5f : 9.0f) / zoom;
        const QString label = pin.type == 4
            ? QStringLiteral("Grid")
            : pin.type == 0
            ? QStringLiteral("Pos")
            : pin.type == 1
                ? QStringLiteral("Starch")
                : pin.type == 2
                    ? QStringLiteral("Bend")
                    : QStringLiteral("Overlap");
        const QString displayLabel = pin.type == 4 && !selected
            ? QString()
            : pin.type == 1
            ? QStringLiteral("Starch %1%").arg(
                  QString::number(pin.weight * 100.0f, 'f', 0))
            : pin.type == 2
                ? QStringLiteral("Bend %1°").arg(
                      QString::number(pin.rotation, 'f', 0))
            : pin.type == 3
                ? QStringLiteral("Overlap %1").arg(
                      QString::number(pin.depth, 'f', 2))
            : label;
        drawTrackerPinOverlay(renderer, x, y, pinSize, color,
                              ArtifactCore::FloatColor{1.0f, 1.0f, 1.0f, 1.0f},
                              selected, displayLabel);
        if (selected && pin.type != 4) {
            const float radius = 24.0f / zoom;
            const float angle = pin.rotation *
                                0.017453292519943295f;
            QPointF direction(std::cos(angle), std::sin(angle));
            const QPointF mappedOrigin = lp->meshLocalTransform.map(QPointF());
            const QPointF mappedDirection =
                lp->meshLocalTransform.map(direction) - mappedOrigin;
            const double directionLength = std::hypot(
                mappedDirection.x(), mappedDirection.y());
            if (directionLength > 1e-8) {
                direction = mappedDirection / directionLength;
            }
            const float handleX = x + static_cast<float>(direction.x()) * radius;
            const float handleY = y + static_cast<float>(direction.y()) * radius;
            renderer->drawSolidLine({x, y}, {handleX, handleY}, color, 1.0f);
            renderer->drawCrosshair(handleX, handleY, 5.0f / zoom,
                                    ArtifactCore::FloatColor{1.0f, 0.85f,
                                                             0.35f, 0.95f});
        }
    }
}

void ArtifactPuppetTool::setPinTypeFor(const QString& pinId, int type)
{
    auto* pin = impl_->findPin(pinId);
    if (pin) {
        const LayerID layerId = pin->layerId;
        pin->type = std::clamp(type, 0, 3);
        if (auto* lp = impl_->getLayerPins(layerId)) lp->needsDeform = true;
        persistLayerData(layerId);
    }
}

int ArtifactPuppetTool::pinTypeFor(const QString& pinId) const
{
    for (const auto& [key, lp] : impl_->layerPins) {
        for (const auto& pin : lp.pins) {
            if (pin.id == pinId) return pin.type;
        }
    }
    return 0;
}

} // namespace Artifact
