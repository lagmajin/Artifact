module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QApplication>
#include <QPointF>
#include <QRectF>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>

module Artifact.Tool.PuppetTool;

import std;
import Utils.Id;
import Color.Float;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Composition.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Render.IRenderer;
import Artifact.Widgets.CompositionRenderOverlay;
import Event.Bus;
import ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;
import Memory.SharedPtr;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPuppetTool)

struct PinRecord {
    QString id;
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
    std::unique_ptr<ArtifactCore::OpenCVPuppetEngine> engine;
    bool needsRebind = false;
};

class ArtifactPuppetTool::Impl {
public:
    bool active = false;
    QString selectedPinId;
    std::map<QString, LayerPins> layerPins; // layerId.toString() -> LayerPins

    LayerPins* getOrCreateLayerPins(const LayerID& layerId) {
        const QString key = layerId.toString();
        auto it = layerPins.find(key);
        if (it == layerPins.end()) {
            LayerPins lp;
            lp.engine = std::make_unique<ArtifactCore::OpenCVPuppetEngine>();
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
    auto* lp = impl_->getOrCreateLayerPins(layerId);
    if (!lp) return false;
    if (lp->pins.size() >= 1024) return false;

    const int idx = static_cast<int>(lp->pins.size());
    const QString pinId = QStringLiteral("pin_%1_%2").arg(layerId.toString()).arg(idx);

    PinRecord pin;
    pin.id = pinId;
    pin.layerId = layerId;
    pin.canvasPos = canvasPos;
    pin.originalPos = canvasPos;
    pin.type = 0;

    lp->pins.push_back(pin);
    lp->needsRebind = true;
    impl_->selectedPinId = pinId;
    return true;
}

bool ArtifactPuppetTool::removePin(const QString& pinId)
{
    for (auto& [key, lp] : impl_->layerPins) {
        auto it = std::remove_if(lp.pins.begin(), lp.pins.end(),
            [&](const PinRecord& p) { return p.id == pinId; });
        if (it != lp.pins.end()) {
            lp.pins.erase(it, lp.pins.end());
            lp.needsRebind = true;
            if (lp.engine) {
                lp.engine->reset();
            }
            if (impl_->selectedPinId == pinId) impl_->selectedPinId.clear();
            return true;
        }
    }
    return false;
}

bool ArtifactPuppetTool::movePin(const QString& pinId, const QPointF& canvasPos)
{
    auto* pin = impl_->findPin(pinId);
    if (!pin || !std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) return false;
    pin->canvasPos = canvasPos;

    auto* lp = impl_->getLayerPins(pin->layerId);
    if (lp && lp->engine) {
        std::string id = pin->id.toStdString();
        lp->engine->updatePinPosition(id, cv::Point2f(
            static_cast<float>(canvasPos.x()),
            static_cast<float>(canvasPos.y())));
    }
    return true;
}

QPointF ArtifactPuppetTool::pinPosition(const QString& pinId) const
{
    if (const auto* pin = impl_->findPin(pinId)) return pin->canvasPos;
    return {};
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
        pin->weight = std::isfinite(weight) ? std::clamp(weight, 0.0f, 1.0f) : 1.0f;
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
        pin->depth = std::isfinite(depth) ? std::clamp(depth, -1.0f, 1.0f) : 0.0f;
    }
}

QString ArtifactPuppetTool::hitTestPin(const QPointF& canvasPos, float threshold) const
{
    if (!std::isfinite(canvasPos.x()) || !std::isfinite(canvasPos.y())) return {};
    const float safeThreshold = std::isfinite(threshold)
        ? std::clamp(threshold, 0.0f, 100000.0f) : 12.0f;
    const float threshSq = safeThreshold * safeThreshold;
    QString closestId;
    float closestDistSq = threshSq;

    for (const auto& [key, lp] : impl_->layerPins) {
        for (const auto& pin : lp.pins) {
            const QPointF d = pin.canvasPos - canvasPos;
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
    auto* lp = impl_->getLayerPins(layerId);
    if (!lp || !lp->engine) return;

    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) return;
    auto layer = selection->currentLayer();
    if (!layer || layer->id() != layerId) return;
    auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
    if (!imageLayer) return;

    // Bind image to engine if dirty
    if (lp->needsRebind) {
        bool rebound = false;
        QImage qimg = imageLayer->toQImage();
        if (!qimg.isNull() && qimg.width() > 0 && qimg.height() > 0) {
            QImage safe = qimg.convertToFormat(QImage::Format_RGBA8888).copy();
            if (!safe.isNull()) {
                cv::Mat mat(safe.height(), safe.width(), CV_8UC4);
                const int rowBytes = safe.bytesPerLine();
                const int copyBytes = std::min(static_cast<int>(mat.step), rowBytes);
                for (int y = 0; y < safe.height(); ++y) {
                    std::memcpy(mat.ptr(y), safe.constScanLine(y), static_cast<size_t>(copyBytes));
                }
                lp->engine->bindImage(mat, 10);
                rebound = true;
            }
        }
        lp->needsRebind = !rebound;
    }

    // Synchronize pins to engine
    for (const auto& pin : lp->pins) {
        if (!std::isfinite(pin.originalPos.x()) ||
            !std::isfinite(pin.originalPos.y()) ||
            !std::isfinite(pin.canvasPos.x()) ||
            !std::isfinite(pin.canvasPos.y())) {
            continue;
        }
        ArtifactCore::PuppetPin ppin;
        ppin.id = pin.id.toStdString();
        ppin.originalPosition = cv::Point2f(
            static_cast<float>(pin.originalPos.x()),
            static_cast<float>(pin.originalPos.y()));
        ppin.currentPosition = cv::Point2f(
            static_cast<float>(pin.canvasPos.x()),
            static_cast<float>(pin.canvasPos.y()));
        ppin.type = static_cast<ArtifactCore::PuppetPinType>(pin.type);
        ppin.weight = std::isfinite(pin.weight)
            ? std::clamp(pin.weight, 0.0f, 1.0f) : 1.0f;
        // PinRecord/UI rotation is stored in degrees; PuppetEngine applies
        // std::sin/cos directly and therefore consumes radians.
        const float safeRotation = std::isfinite(pin.rotation) ? pin.rotation : 0.0f;
        ppin.rotation = safeRotation * 0.017453292519943295f;
        ppin.depth = std::isfinite(pin.depth) ? std::clamp(pin.depth, -1.0f, 1.0f) : 0.0f;
        lp->engine->addPin(ppin);
    }

    // Render deformed image
    cv::Mat deformed = lp->engine->renderDeformedImage(
        ArtifactCore::PuppetDeformationMethod::MovingLeastSquares);

    if (!deformed.empty() && renderer) {
        QImage result(deformed.data, deformed.cols, deformed.rows,
                      deformed.step, QImage::Format_RGBA8888);
        QImage resultCopy = result.copy(); // detach
        imageLayer->setFromQImage(resultCopy);
    }
}

void ArtifactPuppetTool::clearPins(const LayerID& layerId)
{
    auto* lp = impl_->getLayerPins(layerId);
    if (lp) {
        lp->pins.clear();
        lp->needsRebind = true;
        if (lp->engine) lp->engine->reset();
    }
    impl_->selectedPinId.clear();
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

    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return;

    if (lp->engine) {
        const ArtifactCore::PuppetMesh mesh = lp->engine->getDeformedMesh();
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
            renderer->drawSolidLine({a.x, a.y}, {b.x, b.y}, meshColor, 0.8f);
        };
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const int a = mesh.indices[i];
            const int b = mesh.indices[i + 1];
            const int c = mesh.indices[i + 2];
            drawEdge(a, b);
            drawEdge(b, c);
            drawEdge(c, a);
        }
    }

    for (const auto& pin : lp->pins) {
        if (!std::isfinite(pin.canvasPos.x()) ||
            !std::isfinite(pin.canvasPos.y())) {
            continue;
        }
        const float x = static_cast<float>(pin.canvasPos.x());
        const float y = static_cast<float>(pin.canvasPos.y());
        const bool selected = (pin.id == impl_->selectedPinId);

        // Pin color based on type
        ArtifactCore::FloatColor color;
        switch (pin.type) {
        case 0: color = ArtifactCore::FloatColor{1.0f, 1.0f, 0.0f, 1.0f}; break; // Position: yellow
        case 1: color = ArtifactCore::FloatColor{0.0f, 1.0f, 1.0f, 1.0f}; break; // Starch: cyan
        case 2: color = ArtifactCore::FloatColor{1.0f, 0.5f, 0.0f, 1.0f}; break; // Bend: orange
        case 3: color = ArtifactCore::FloatColor{1.0f, 0.0f, 1.0f, 1.0f}; break; // Overlap: magenta
        default: color = ArtifactCore::FloatColor{1.0f, 1.0f, 0.0f, 1.0f};
        }

        const float rawZoom = renderer->getZoom();
        const float zoom = std::isfinite(rawZoom)
            ? std::clamp(rawZoom, 0.001f, 10000.0f) : 1.0f;
        const float pinSize = (selected ? 11.5f : 9.0f) / zoom;
        const QString label = pin.type == 0
            ? QStringLiteral("Pos")
            : pin.type == 1
                ? QStringLiteral("Starch")
                : pin.type == 2
                    ? QStringLiteral("Bend")
                    : QStringLiteral("Overlap");
        const QString displayLabel = pin.type == 1
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
        if (selected) {
            const float radius = 24.0f / zoom;
            const float angle = pin.rotation *
                                0.017453292519943295f;
            const float handleX = x + std::cos(angle) * radius;
            const float handleY = y + std::sin(angle) * radius;
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
        pin->type = std::clamp(type, 0, 3);
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
