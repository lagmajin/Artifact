module;

#include <Qt>

module Artifact.Widgets.LayerEditor.ViewPressController;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.TransformGizmo;

namespace Artifact {

LayerEditorViewPressResult LayerEditorViewPressController::handle(
    const LayerEditorViewPressState& state,
    const LayerEditorViewPressCallbacks& callbacks,
    ArtifactIRenderer* renderer,
    LayerEditorShapeParameterController& parameterController,
    TransformGizmo* transformGizmo) const
{
 if (state.button == Qt::LeftButton && callbacks.pressViewportChrome &&
     callbacks.pressViewportChrome(state.viewportPosition)) {
  return {true};
 }
 if (state.button == Qt::MiddleButton ||
     (state.button == Qt::RightButton && state.altModifier)) {
  if (callbacks.clearViewportChromeHover) callbacks.clearViewportChromeHover();
  if (state.panning) *state.panning = true;
  if (state.lastMousePosition) *state.lastMousePosition = state.viewportPosition;
  return {true, false, LayerEditorViewPressCursor::Pan};
 }
 if (state.button == Qt::LeftButton && state.layer &&
     (!state.layer->isVisible() || state.layer->isLocked())) {
  return {true};
 }
 if (!state.transformViewEnabled || state.button != Qt::LeftButton ||
     !renderer || !transformGizmo) return {};

 if (state.layer && state.layer->isVisible() && !state.layer->isLocked()) {
  const auto canvas = renderer->viewportToCanvas(
      {static_cast<float>(state.viewportPosition.x()),
       static_cast<float>(state.viewportPosition.y())});
  if (parameterController.begin(
          state.layer, QPointF(canvas.x, canvas.y), state.viewportPosition,
          renderer->getZoom())) {
   const bool corner = parameterController.activeHandle() ==
                       LayerEditorShapeParameterHandle::CornerRadius;
   return {true, false,
           corner ? LayerEditorViewPressCursor::ParameterHorizontal
                  : LayerEditorViewPressCursor::ParameterVertical};
  }
 }
 if (state.layer && transformGizmo->handleMousePress(
         state.viewportPosition, renderer)) {
  return {true, true, LayerEditorViewPressCursor::Gizmo,
          transformGizmo->activeHandle()};
 }
 return {};
}

}
