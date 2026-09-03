module;

#include <algorithm>

module Artifact.Widgets.LayerEditor.ViewMoveController;

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
      state.viewportPosition,
      std::max(0.001, static_cast<double>(renderer.getZoom())),
      state.precision, state.snap);
  if (target == LayerEditorModalTransformTarget::Path)
   shapeEditSession.markPathDirty();
  else if (target == LayerEditorModalTransformTarget::Polygon)
   shapeEditSession.markPolygonDirty();
  return {true, true};
 }

 bool requestRender = false;
 const auto canvas = renderer.viewportToCanvas(
     {static_cast<float>(state.viewportPosition.x()),
      static_cast<float>(state.viewportPosition.y())});
 const QPointF canvasPosition(canvas.x, canvas.y);
 if (parameterController.active() &&
     parameterController.update(canvasPosition, state.viewportPosition)) {
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
  if (transformGizmo->handleMouseMove(state.viewportPosition, &renderer)) {
   return {true, true, LayerEditorViewMoveCursor::Gizmo,
           transformGizmo->activeHandle(), true};
  }
  return {false, requestRender};
 }
 return {false, requestRender, LayerEditorViewMoveCursor::Gizmo,
         transformGizmo->handleAtViewportPos(state.viewportPosition, &renderer),
         false};
}

}
