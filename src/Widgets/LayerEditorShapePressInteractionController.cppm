module;

#include <utility>
#include <vector>
#include <type_traits>

module Artifact.Widgets.LayerEditor.ShapePressInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Artifact.Widgets.LayerEditor.ShapePressController;
import Memory.SharedPtr;

namespace Artifact {
namespace {

void resetProportionalState(LayerEditorShapePressInteractionState& state)
{
 if (state.proportionalDragActive) *state.proportionalDragActive = false;
 if (state.proportionalDragOrigin) *state.proportionalDragOrigin = {};
 if (state.proportionalMaskBefore) state.proportionalMaskBefore->clear();
 if (state.proportionalPolygonBefore) state.proportionalPolygonBefore->clear();
 if (state.proportionalPathBefore) state.proportionalPathBefore->clear();
}

template<typename T>
void beginProportionalSnapshot(
    LayerEditorShapePressInteractionState& state,
    const std::vector<T>& before, int vertexIndex,
    std::vector<T>* destination)
{
 resetProportionalState(state);
 if (!state.proportionalEditingEnabled || !destination ||
     vertexIndex < 0 || vertexIndex >= static_cast<int>(before.size())) return;
 *destination = before;
 if (state.proportionalDragOrigin) {
  if constexpr (std::is_same_v<T, QPointF>)
   *state.proportionalDragOrigin = before[static_cast<size_t>(vertexIndex)];
  else
   *state.proportionalDragOrigin = before[static_cast<size_t>(vertexIndex)].pos;
 }
 if (state.proportionalDragActive) *state.proportionalDragActive = true;
}

}

LayerEditorShapePressInteractionResult
LayerEditorShapePressInteractionController::handle(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, bool extendSelection,
    LayerEditorShapePressInteractionState state,
    LayerEditorShapeHoverController& hoverController,
    LayerEditorShapeEditSession& editSession) const
{
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || shape->shapeType() == ShapeType::Line ||
     !state.selectedPolygonIndices || !state.selectedPathIndices) return {};

 LayerEditorShapePressHit hit;
 if (shape->hasCustomPath()) {
  hoverController.hitPathTangent(
      layer, canvasPosition, zoom, hit.pathTangentVertex, hit.pathTangentType);
  if (hit.pathTangentVertex < 0)
   hoverController.hitPathVertex(
       layer, canvasPosition, zoom, hit.pathVertex);
 } else {
  hoverController.hitPolygonVertex(
      layer, canvasPosition, zoom, hit.polygonVertex);
  if (hit.polygonVertex < 0)
   hoverController.hitPolygonSegment(
       layer, canvasPosition, zoom, hit.polygonSegment);
 }

 LayerEditorShapePressController pressController;
 auto result = pressController.handle(
     layer, *shape, canvasPosition, hit, extendSelection,
     *state.selectedPolygonIndices, *state.selectedPathIndices, editSession);
 switch (result.kind) {
  case LayerEditorShapePressKind::SelectionOnly:
   return {true, true, false};
  case LayerEditorShapePressKind::DragPathTangent:
   if (state.draggingPathTangent) *state.draggingPathTangent = true;
   if (state.draggingPathVertexIndex)
    *state.draggingPathVertexIndex = result.vertexIndex;
   if (state.draggingPathTangentType)
    *state.draggingPathTangentType = result.tangentType;
   break;
  case LayerEditorShapePressKind::DragPathVertex:
   if (state.draggingPathVertex) *state.draggingPathVertex = true;
   if (state.draggingPathVertexIndex)
    *state.draggingPathVertexIndex = result.vertexIndex;
   if (state.selectedPathBefore)
    *state.selectedPathBefore = std::move(result.pathDragBefore);
   beginProportionalSnapshot(
       state, state.selectedPathBefore ? *state.selectedPathBefore
                                      : std::vector<CustomPathVertex>{},
       result.vertexIndex, state.proportionalPathBefore);
   break;
  case LayerEditorShapePressKind::DragPolygonVertex:
   if (state.draggingPolygonVertex) *state.draggingPolygonVertex = true;
   if (state.draggingPolygonVertexIndex)
    *state.draggingPolygonVertexIndex = result.vertexIndex;
   if (state.selectedPolygonBefore)
    *state.selectedPolygonBefore = std::move(result.polygonDragBefore);
   beginProportionalSnapshot(
       state, state.selectedPolygonBefore ? *state.selectedPolygonBefore
                                         : std::vector<QPointF>{},
       result.vertexIndex, state.proportionalPolygonBefore);
   if (result.hoveredPolygonVertex >= 0)
    hoverController.setPolygon(
        result.hoveredPolygonVertex, result.hoveredPolygonSegment);
   break;
  case LayerEditorShapePressKind::GeometryChanged:
   if (shape->hasCustomPath())
    hoverController.setPath(result.hoveredPathVertex, -1, 0);
   else
    hoverController.setPolygon(
        result.hoveredPolygonVertex, result.hoveredPolygonSegment);
   return {true, true, false};
  case LayerEditorShapePressKind::None:
   return {};
 }
 return {true, true, true};
}

}
