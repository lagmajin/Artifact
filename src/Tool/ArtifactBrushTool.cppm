module;
#include <wobjectimpl.h>
module Artifact.Tool.Brush;

import Artifact.Layer.Paint;
import FloatRGBA;

namespace Artifact {

ArtifactBrushTool::ArtifactBrushTool(QObject* parent)
    : QObject(parent) {}
ArtifactBrushTool::~ArtifactBrushTool() = default;

bool ArtifactBrushTool::mousePressEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer) return false;

    dragging_ = true;
    currentStroke_ = BrushStroke{};
    currentStroke_.points.push_back(canvasPos);
    currentStroke_.radius = radius_;
    currentStroke_.opacity = opacity_;
    currentStroke_.eraser = eraserMode_;
    // color is set externally via the tool options
    return true;
}

bool ArtifactBrushTool::mouseMoveEvent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos)
{
    if (!dragging_) return false;
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(layer.get());
    if (!paintLayer) return false;

    currentStroke_.points.push_back(canvasPos);
    // リアルタイム適用（点が溜まりすぎる前に逐次適用）
    if (currentStroke_.points.size() >= 5) {
        paintLayer->applyStroke(currentStroke_);
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
    if (!paintLayer) return false;

    if (!currentStroke_.points.empty()) {
        currentStroke_.points.push_back(canvasPos);
        paintLayer->applyStroke(currentStroke_);
    }
    currentStroke_.points.clear();
    return true;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactBrushTool)
