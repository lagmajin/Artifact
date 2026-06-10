module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <QPointF>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>

module Artifact.Tool.PuppetTool;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Render.IRenderer;
import Event.Bus;
import ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPuppetTool)

struct PinRecord {
    QString id;
    LayerID layerId;
    QPointF canvasPos;  // current position
    QPointF originalPos;
    int type = 0; // 0=Position, 1=Starch, 2=Bend, 3=Overlap
    float rotation = 0.0f;
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
    auto* lp = impl_->getOrCreateLayerPins(layerId);
    if (!lp) return false;

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
            if (impl_->selectedPinId == pinId) impl_->selectedPinId.clear();
            return true;
        }
    }
    return false;
}

bool ArtifactPuppetTool::movePin(const QString& pinId, const QPointF& canvasPos)
{
    auto* pin = impl_->findPin(pinId);
    if (!pin) return false;
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

QString ArtifactPuppetTool::hitTestPin(const QPointF& canvasPos, float threshold) const
{
    const float threshSq = threshold * threshold;
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
    auto layer = selection->layerById(layerId);
    if (!layer) return;

    // Bind image to engine if dirty
    if (lp->needsRebind) {
        QImage qimg = layer->toQImage();
        if (!qimg.isNull()) {
            cv::Mat mat(qimg.height(), qimg.width(), CV_8UC4,
                        const_cast<uchar*>(qimg.constBits()), qimg.bytesPerLine());
            cv::Mat copy = mat.clone();
            lp->engine->bindImage(copy, 10);
        }
        lp->needsRebind = false;
    }

    // Synchronize pins to engine
    for (const auto& pin : lp->pins) {
        ArtifactCore::PuppetPin ppin;
        ppin.id = pin.id.toStdString();
        ppin.originalPosition = cv::Point2f(
            static_cast<float>(pin.originalPos.x()),
            static_cast<float>(pin.originalPos.y()));
        ppin.currentPosition = cv::Point2f(
            static_cast<float>(pin.canvasPos.x()),
            static_cast<float>(pin.canvasPos.y()));
        ppin.type = static_cast<ArtifactCore::PuppetPinType>(pin.type);
        ppin.weight = 1.0f;
        ppin.rotation = pin.rotation;
        ppin.depth = pin.depth;
        lp->engine->addPin(ppin);
    }

    // Render deformed image
    cv::Mat deformed = lp->engine->renderDeformedImage(
        ArtifactCore::PuppetDeformationMethod::MovingLeastSquares);

    if (!deformed.empty() && renderer) {
        QImage result(deformed.data, deformed.cols, deformed.rows,
                      deformed.step, QImage::Format_RGBA8888);
        QImage resultCopy = result.copy(); // detach
        renderer->setLayerOverrideImage(layerId, resultCopy);
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
    impl_->selectedPinId = pinId;
}

void ArtifactPuppetTool::renderOverlay(ArtifactIRenderer* renderer, const LayerID& layerId) const
{
    if (!renderer) return;

    auto* lp = impl_->getLayerPins(layerId);
    if (!lp) return;

    for (const auto& pin : lp->pins) {
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

        const float size = selected ? 10.0f : 7.0f;
        const float half = size * 0.5f;

        // Outer glow for selected
        if (selected) {
            renderer->drawSolidRect(x - half - 2, y - half - 2, size + 4, size + 4,
                                    ArtifactCore::FloatColor{1.0f, 1.0f, 1.0f, 0.3f}, 1.0f);
        }

        // Pin body (circle approximated as square for now)
        renderer->drawSolidRect(x - half, y - half, size, size, color, 1.0f);

        // Outline
        renderer->drawRectOutline(x - half, y - half, size, size,
                                  ArtifactCore::FloatColor{1.0f, 1.0f, 1.0f, 0.8f});

        // Label
        const QString label = QString::number(pin.type);
        renderer->drawText(QRectF(x + half + 2, y - 6, 80, 14), label,
                           QApplication::font(),
                           ArtifactCore::FloatColor{1.0f, 1.0f, 1.0f, 0.7f},
                           Qt::AlignLeft | Qt::AlignVCenter);
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
