module;

#include <QPointF>

module Artifact.Widgets.LayerEditor.ViewMoveController;

import Core.ArtifactMath;
import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ModalTransformController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.TransformGizmo;

namespace Artifact {

LayerEditorViewMoveResult LayerEditorViewMoveController::handle(
    const LayerEditorViewMoveState& state, ArtifactIRenderer& renderer,
    LayerEditorModalTransformController& modalTransform,
    LayerEditorShapeEditSession& shapeEditSession,
    LayerEditorShapeParameterController& parameterController,
    TransformGizmo* transformGizmo) const
{
 if (modalTransform.active()) {
  const auto target = modalTransform.update(
      ArtifactCore::Coordinates::toQPointF(state.viewportPosition),
      ArtifactCore::artifactMax(0.001, static_cast<double>(renderer.getZoom())),
      state.precision, state.snap);
  if (target == LayerEditorModalTransformTarget::Path)
   shapeEditSession.markPathDirty();
  else if (target == LayerEditorModalTransformTarget::Polygon)
   shapeEditSession.markPolygonDirty();
  return {true, true};
 }

 bool requestRender = false;
 const auto canvas = renderer.viewportToCanvas(
     {state.viewportPosition.x, state.viewportPosition.y});
 const QPointF canvasPosition(canvas.x, canvas.y);
 if (parameterController.active() &&
     parameterController.update(
         canvasPosition,
         ArtifactCore::Coordinates::toQPointF(state.viewportPosition))) {
  return {true, true};
 }
 if (state.transformViewEnabled && state.layer &&
     state.layer->isVisible() && !state.layer->isLocked() &&
     parameterController.updateHover(
         state.layer, canvasPosition, renderer.getZoom())) {
  requestRender = true;
 }

 if (!state.transformViewEnabled || !transformGizmo)
  return {false, requestRender};
 if (!state.layer || !state.layer->isVisible() || state.layer->isLocked()) {
  return {false, requestRender, LayerEditorViewMoveCursor::Unset};
 }
 if (transformGizmo->isDragging()) {
  if (transformGizmo->handleMouseMove(
          ArtifactCore::Coordinates::toQPointF(state.viewportPosition), &renderer)) {
   return {true, true, LayerEditorViewMoveCursor::Gizmo,
           transformGizmo->activeHandle(), true};
  }
  return {false, requestRender};
 }
 return {false, requestRender, LayerEditorViewMoveCursor::Gizmo,
         transformGizmo->handleAtViewportPos(
             ArtifactCore::Coordinates::toQPointF(state.viewportPosition), &renderer),
         false};
}

}
