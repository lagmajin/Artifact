module;

#include <QPointF>

#include <algorithm>

module Artifact.Widgets.LayerEditor.MaskHoverController;

import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;

namespace Artifact {
namespace {

bool sameState(const LayerEditorMaskHoverState& left,
               const LayerEditorMaskHoverState& right)
{
 return left.maskIndex == right.maskIndex &&
        left.pathIndex == right.pathIndex &&
        left.vertexIndex == right.vertexIndex &&
        left.handleType == right.handleType;
}

}

bool LayerEditorMaskHoverController::update(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom)
{
 LayerEditorMaskHoverState next;
 const float safeZoom = std::max(0.1f, zoom);
 if (!hitTestMaskHandle(
         layer, canvasPosition, 10.0f / safeZoom,
         next.maskIndex, next.pathIndex, next.vertexIndex, next.handleType)) {
  hitTestMaskVertexGeometry(
      layer, canvasPosition, 8.0f / safeZoom,
      next.maskIndex, next.pathIndex, next.vertexIndex);
 }
 const bool changed = !sameState(state_, next);
 state_ = next;
 return changed;
}

bool LayerEditorMaskHoverController::hitVertex(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom,
    int& maskIndex, int& pathIndex, int& vertexIndex) const
{
 return hitTestMaskVertexGeometry(
     layer, canvasPosition, 8.0f / std::max(0.1f, zoom),
     maskIndex, pathIndex, vertexIndex);
}

void LayerEditorMaskHoverController::set(
    const LayerEditorMaskHoverState& state) noexcept
{
 state_ = state;
}

void LayerEditorMaskHoverController::clear() noexcept
{
 state_ = {};
}

bool LayerEditorMaskHoverController::deleteHoveredVertex(
    const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || !hasVertex() || state_.maskIndex >= layer->maskCount()) return false;
 LayerMask mask = layer->mask(state_.maskIndex);
 if (state_.pathIndex < 0 || state_.pathIndex >= mask.maskPathCount()) return false;
 MaskPath path = mask.maskPath(state_.pathIndex);
 if (state_.vertexIndex < 0 || state_.vertexIndex >= path.vertexCount()) return false;
 path.removeVertex(state_.vertexIndex);
 if (path.vertexCount() <= 0) {
  mask.removeMaskPath(state_.pathIndex);
  if (mask.maskPathCount() <= 0) layer->removeMask(state_.maskIndex);
  else layer->setMask(state_.maskIndex, mask);
 } else {
  mask.setMaskPath(state_.pathIndex, path);
  layer->setMask(state_.maskIndex, mask);
 }
 clear();
 return true;
}

const LayerEditorMaskHoverState& LayerEditorMaskHoverController::state() const noexcept
{
 return state_;
}

bool LayerEditorMaskHoverController::hasVertex() const noexcept
{
 return state_.maskIndex >= 0 && state_.pathIndex >= 0 && state_.vertexIndex >= 0;
}

}
