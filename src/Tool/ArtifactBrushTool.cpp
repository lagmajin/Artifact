module;
#include <algorithm>
#include <wobjectimpl.h>
#include <QLineF>
#include <QRectF>
#include <QTransform>
#include <cmath>
module Artifact.Tool.Brush;

import Artifact.Layer.Paint;
import FloatRGBA;

namespace Artifact {

namespace {
constexpr float kRadiansToDegrees = 57.29577951308232f;

bool isFinitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QPointF normalizedSourcePoint(const ArtifactAbstractLayerPtr& layer,
                              const QPointF& canvasPos,
                              const QPointF& fallback = QPointF(0.5, 0.5)) {
    if (!layer) return fallback;
    const QRectF bounds = layer->sourceBounds();
    if (!bounds.isValid() || bounds.width() <= 0.0 ||
        bounds.height() <= 0.0) {
        return fallback;
    }
    bool invertible = false;
    const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
    if (!invertible) return fallback;
    const QPointF local = inverse.map(canvasPos);
    return QPointF(std::clamp((local.x() - bounds.left()) / bounds.width(),
                              0.0, 1.0),
                   std::clamp((local.y() - bounds.top()) / bounds.height(),
                              0.0, 1.0));
}
}

void ArtifactBrushTool::updateFluidSmokeStir(const QPointF& canvasPos) {
    if (!fluidSmokeStirLayer_ || !isFinitePoint(canvasPos)) return;
    const auto layer = fluidSmokeStirLayer_;
    const QRectF bounds = layer->sourceBounds();
    if (!bounds.isValid() || bounds.width() <= 0.0 ||
        bounds.height() <= 0.0) {
        return;
    }
    const QPointF current = normalizedSourcePoint(layer, canvasPos);
    const QPointF previous = normalizedSourcePoint(
        layer, previousFluidSmokeStirCanvasPos_, current);
    constexpr float kVelocityScale = 64.0f;
    const float velocityX = static_cast<float>(current.x() - previous.x()) *
                            kVelocityScale;
    const float velocityY = static_cast<float>(current.y() - previous.y()) *
                            kVelocityScale;
    const float sourceExtent = static_cast<float>(
        std::min(bounds.width(), bounds.height()));
    const float radius = std::max(0.025f,
        static_cast<float>(radius_) / std::max(1.0f, sourceExtent));
    layer->setFluidSmokeStirInput(true,
        static_cast<float>(current.x()), static_cast<float>(current.y()),
        velocityX, velocityY, radius,
        flow_ * (pressureAffectsFlow_ ? pressure_ : 1.0f));
    previousFluidSmokeStirCanvasPos_ = canvasPos;
}

ArtifactBrushTool::ArtifactBrushTool(QObject* parent)
    : QObject(parent) {}
ArtifactBrushTool::~ArtifactBrushTool() = default;

bool ArtifactBrushTool::mousePressEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    const bool fluidSmoke = layer && layer->hasFluidSmokeComponent();
    if ((!paintLayer && !rotoInputMode_ && !fluidSmoke) ||
        !isFinitePoint(canvasPos)) return false;

    if (fluidSmoke) {
        dragging_ = true;
        fluidSmokeStirDragging_ = true;
        fluidSmokeStirLayer_ = layer;
        previousFluidSmokeStirCanvasPos_ = canvasPos;
        updateFluidSmokeStir(canvasPos);
        return true;
    }

    dragging_ = true;
    undoRecorded_ = false;
    currentStroke_ = BrushStroke{};
    previewStrokePoints_.clear();
    activeStrokePoints_.clear();
    if (currentStroke_.points.empty() ||
        QLineF(currentStroke_.points.back(), canvasPos).length() >=
            std::max(0.5f, radius_ * spacing_)) {
        currentStroke_.points.push_back(canvasPos);
    }
    if (currentStroke_.points.empty()) return true;
    previewStrokePoints_.push_back(canvasPos);
    activeStrokePoints_.push_back(canvasPos);
    currentStroke_.radius = radius_ *
        (pressureAffectsSize_ ? pressure_ : 1.0f);
    currentStroke_.opacity = opacity_ *
        (pressureAffectsOpacity_ ? pressure_ : 1.0f);
    currentStroke_.flow = flow_ *
        (pressureAffectsFlow_ ? pressure_ : 1.0f);
    currentStroke_.hardness = hardness_;
    const float tiltMagnitude = std::min(60.0f, std::hypot(tiltX_, tiltY_));
    currentStroke_.angle = angle_ +
        (tiltAffectsAngle_ && tiltMagnitude > 0.01f
             ? std::atan2(tiltY_, tiltX_) * kRadiansToDegrees
             : 0.0f);
    currentStroke_.roundness = roundness_ *
        (tiltAffectsRoundness_
             ? std::max(0.25f, 1.0f - (tiltMagnitude / 60.0f) * 0.5f)
             : 1.0f);
    currentStroke_.sizeJitter = sizeJitter_;
    currentStroke_.opacityJitter = opacityJitter_;
    currentStroke_.scatter = scatter_;
    currentStroke_.angleJitter = angleJitter_;
    currentStroke_.roundnessJitter = roundnessJitter_;
    currentStroke_.flowJitter = flowJitter_;
    currentStroke_.color = color_;
    currentStroke_.eraser = eraserMode_;
    return true;
}

bool ArtifactBrushTool::mouseMoveEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    if (!dragging_) return false;
    if (!isFinitePoint(canvasPos)) return true;
    if (fluidSmokeStirDragging_) {
        if (!layer || layer.get() != fluidSmokeStirLayer_.get() ||
            !fluidSmokeStirLayer_->hasFluidSmokeComponent()) {
            if (fluidSmokeStirLayer_) {
                fluidSmokeStirLayer_->setFluidSmokeStirInput(
                    false, 0.5f, 0.5f, 0.0f, 0.0f, 0.035f, 0.0f);
            }
            fluidSmokeStirLayer_.reset();
            fluidSmokeStirDragging_ = false;
            dragging_ = false;
            return false;
        }
        updateFluidSmokeStir(canvasPos);
        return true;
    }
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer && !rotoInputMode_) {
        cancelStroke(layer);
        return false;
    }

    if (!currentStroke_.points.empty() &&
        QLineF(currentStroke_.points.back(), canvasPos).length() <
            std::max(0.5f, radius_ * spacing_)) {
        return true;
    }
    currentStroke_.points.push_back(canvasPos);
    activeStrokePoints_.push_back(canvasPos);
    currentStroke_.radius = radius_ *
        (pressureAffectsSize_ ? pressure_ : 1.0f);
    currentStroke_.opacity = opacity_ *
        (pressureAffectsOpacity_ ? pressure_ : 1.0f);
    currentStroke_.flow = flow_ *
        (pressureAffectsFlow_ ? pressure_ : 1.0f);
    const float tiltMagnitude = std::min(60.0f, std::hypot(tiltX_, tiltY_));
    currentStroke_.angle = angle_ +
        (tiltAffectsAngle_ && tiltMagnitude > 0.01f
             ? std::atan2(tiltY_, tiltX_) * kRadiansToDegrees
             : 0.0f);
    currentStroke_.roundness = roundness_ *
        (tiltAffectsRoundness_
             ? std::max(0.25f, 1.0f - (tiltMagnitude / 60.0f) * 0.5f)
             : 1.0f);
    currentStroke_.sizeJitter = sizeJitter_;
    currentStroke_.opacityJitter = opacityJitter_;
    currentStroke_.scatter = scatter_;
    currentStroke_.angleJitter = angleJitter_;
    currentStroke_.roundnessJitter = roundnessJitter_;
    currentStroke_.flowJitter = flowJitter_;
    previewStrokePoints_.push_back(canvasPos);
    if (previewStrokePoints_.size() > 4096) {
        std::vector<QPointF> compacted;
        compacted.reserve(2049);
        for (size_t i = 0; i < previewStrokePoints_.size(); i += 2) {
            compacted.push_back(previewStrokePoints_[i]);
        }
        if (compacted.back() != previewStrokePoints_.back()) {
            compacted.push_back(previewStrokePoints_.back());
        }
        previewStrokePoints_.swap(compacted);
    }
    // リアルタイム適用（点が溜まりすぎる前に逐次適用）
    if (currentStroke_.points.size() >= 5) {
        currentStroke_.recordUndo = !undoRecorded_;
        if (paintLayer) paintLayer->applyStroke(currentStroke_);
        undoRecorded_ = true;
        currentStroke_.points.clear();
        currentStroke_.points.push_back(canvasPos);
    }
    return true;
}

bool ArtifactBrushTool::mouseReleaseEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    if (!dragging_) return false;
    if (fluidSmokeStirDragging_) {
        if (fluidSmokeStirLayer_) {
            updateFluidSmokeStir(canvasPos);
            fluidSmokeStirLayer_->setFluidSmokeStirInput(
                false, 0.5f, 0.5f, 0.0f, 0.0f, 0.035f, 0.0f);
        }
        fluidSmokeStirLayer_.reset();
        fluidSmokeStirDragging_ = false;
        dragging_ = false;
        return true;
    }
    dragging_ = false;

    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer && !rotoInputMode_) {
        cancelStroke(layer);
        return false;
    }

    if (!currentStroke_.points.empty()) {
        currentStroke_.radius = radius_ *
            (pressureAffectsSize_ ? pressure_ : 1.0f);
        currentStroke_.opacity = opacity_ *
            (pressureAffectsOpacity_ ? pressure_ : 1.0f);
        currentStroke_.flow = flow_ *
            (pressureAffectsFlow_ ? pressure_ : 1.0f);
        const float tiltMagnitude = std::min(60.0f, std::hypot(tiltX_, tiltY_));
        currentStroke_.angle = angle_ +
            (tiltAffectsAngle_ && tiltMagnitude > 0.01f
                 ? std::atan2(tiltY_, tiltX_) * kRadiansToDegrees
                 : 0.0f);
        currentStroke_.roundness = roundness_ *
            (tiltAffectsRoundness_
                 ? std::max(0.25f, 1.0f - (tiltMagnitude / 60.0f) * 0.5f)
                 : 1.0f);
        currentStroke_.sizeJitter = sizeJitter_;
        currentStroke_.opacityJitter = opacityJitter_;
        currentStroke_.scatter = scatter_;
        currentStroke_.angleJitter = angleJitter_;
        currentStroke_.roundnessJitter = roundnessJitter_;
        currentStroke_.flowJitter = flowJitter_;
        if (isFinitePoint(canvasPos) &&
            QLineF(currentStroke_.points.back(), canvasPos).length() >=
            std::max(0.5f, radius_ * spacing_)) {
            currentStroke_.points.push_back(canvasPos);
            previewStrokePoints_.push_back(canvasPos);
            activeStrokePoints_.push_back(canvasPos);
        }
        currentStroke_.recordUndo = !undoRecorded_;
        if (paintLayer) paintLayer->applyStroke(currentStroke_);
        lastStrokePoints_ = activeStrokePoints_;
    }
    currentStroke_.points.clear();
    activeStrokePoints_.clear();
    previewStrokePoints_.clear();
    return true;
}

void ArtifactBrushTool::cancelStroke(const ArtifactAbstractLayerPtr& layer)
{
    if (fluidSmokeStirDragging_ && fluidSmokeStirLayer_) {
        fluidSmokeStirLayer_->setFluidSmokeStirInput(
            false, 0.5f, 0.5f, 0.0f, 0.0f, 0.035f, 0.0f);
    }
    fluidSmokeStirLayer_.reset();
    fluidSmokeStirDragging_ = false;
    if (undoRecorded_) {
        if (auto *paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get())) {
            paintLayer->undoLastStroke();
        }
    }
    dragging_ = false;
    undoRecorded_ = false;
    currentStroke_.points.clear();
    activeStrokePoints_.clear();
    previewStrokePoints_.clear();
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactBrushTool)
