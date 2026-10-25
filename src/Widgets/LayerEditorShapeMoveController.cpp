module;

#include <QTransform>

#include <vector>

module Artifact.Widgets.LayerEditor.ShapeMoveController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeDragController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Memory.SharedPtr;

namespace Artifact {
namespace {

template<typename T>
const std::vector<T>& valuesOrEmpty(const std::vector<T>* values)
{
 static const std::vector<T> empty;
 return values ? *values : empty;
}

}

LayerEditorShapeMoveResult LayerEditorShapeMoveController::handle(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, bool independentTangent,
    const LayerEditorShapeMoveState& state,
    LayerEditorShapeDragController& dragController,
    LayerEditorShapeHoverController& hoverController,
    LayerEditorShapeEditSession& editSession) const
{
 auto shape = layer && layer->isVisible() && !layer->isLocked()
     ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
           ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer))
     : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
 if (!shape) return {};

 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);

 if (shape->hasCustomPath()) {
  if (invertible && state.draggingPathVertex &&
      state.draggingPathVertexIndex >= 0 &&
      dragController.dragPathVertex(
          *shape, inverse.map(canvasPosition), state.draggingPathVertexIndex,
          valuesOrEmpty(state.selectedPathIndices),
          valuesOrEmpty(state.selectedPathBefore),
          state.proportionalEditingEnabled, state.proportionalDragActive,
          valuesOrEmpty(state.proportionalPathBefore),
          state.proportionalDragOrigin, state.proportionalEditRadius)) {
   editSession.markPathDirty();
   return {LayerEditorShapeMoveKind::GeometryChanged, false, true, false};
  }
  if (invertible && state.draggingPathTangent &&
      state.draggingPathVertexIndex >= 0 &&
      dragController.dragPathTangent(
          *shape, inverse.map(canvasPosition), state.draggingPathVertexIndex,
          state.draggingPathTangentType, independentTangent)) {
   editSession.markPathDirty();
   return {LayerEditorShapeMoveKind::GeometryChanged, false, true, false};
  }
  if (hoverController.updatePath(layer, canvasPosition, zoom)) {
   return {LayerEditorShapeMoveKind::HoverChanged, false, true, false};
  }
  return {LayerEditorShapeMoveKind::PathConsumed, false, true, false};
 }

 if (shape->hasCustomPolygon() && invertible &&
     state.draggingPolygonVertexIndex >= 0) {
  editSession.beginPolygon(layer);
  if (dragController.dragPolygonVertex(
          *shape, inverse.map(canvasPosition), state.draggingPolygonVertexIndex,
          valuesOrEmpty(state.selectedPolygonIndices),
          valuesOrEmpty(state.selectedPolygonBefore),
          state.proportionalEditingEnabled, state.proportionalDragActive,
          valuesOrEmpty(state.proportionalPolygonBefore),
          state.proportionalDragOrigin, state.proportionalEditRadius)) {
   editSession.markPolygonDirty();
   return {LayerEditorShapeMoveKind::GeometryChanged, false, false, true};
  }
 }

 const bool hoverChanged = hoverController.updatePolygon(
     layer, canvasPosition, zoom);
 const auto& hover = hoverController.state();
 return {hoverChanged ? LayerEditorShapeMoveKind::HoverChanged
                      : LayerEditorShapeMoveKind::Ignored,
         hover.polygonVertex >= 0 || hover.polygonSegment >= 0,
         false, true};
}

}
