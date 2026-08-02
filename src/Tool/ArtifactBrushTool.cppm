module;
#include <wobjectimpl.h>
#include <QLineF>
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
}

ArtifactBrushTool::ArtifactBrushTool(QObject* parent)
    : QObject(parent) {}
ArtifactBrushTool::~ArtifactBrushTool() = default;

bool ArtifactBrushTool::mousePressEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer || !isFinitePoint(canvasPos)) return false;

    dragging_ = true;
    undoRecorded_ = false;
    currentStroke_ = BrushStroke{};
    previewStrokePoints_.clear();
    if (currentStroke_.points.empty() ||
        QLineF(currentStroke_.points.back(), canvasPos).length() >=
            std::max(0.5f, radius_ * spacing_)) {
        currentStroke_.points.push_back(canvasPos);
    }
    if (currentStroke_.points.empty()) return true;
    previewStrokePoints_.push_back(canvasPos);
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
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer) {
        cancelStroke(layer);
        return false;
    }

    if (!currentStroke_.points.empty() &&
        QLineF(currentStroke_.points.back(), canvasPos).length() <
            std::max(0.5f, radius_ * spacing_)) {
        return true;
    }
    currentStroke_.points.push_back(canvasPos);
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
        paintLayer->applyStroke(currentStroke_);
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
    dragging_ = false;

    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer) {
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
        }
        currentStroke_.recordUndo = !undoRecorded_;
        paintLayer->applyStroke(currentStroke_);
    }
    currentStroke_.points.clear();
    previewStrokePoints_.clear();
    return true;
}

void ArtifactBrushTool::cancelStroke(const ArtifactAbstractLayerPtr& layer)
{
    if (undoRecorded_) {
        if (auto *paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get())) {
            paintLayer->undoLastStroke();
        }
    }
    dragging_ = false;
    undoRecorded_ = false;
    currentStroke_.points.clear();
    previewStrokePoints_.clear();
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactBrushTool)
