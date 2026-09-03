module;

#include <Qt>

#include <algorithm>

module Artifact.Widgets.LayerEditor.ViewKeyInputController;

import Artifact.Render.IRenderer;
import Tool;
import Artifact.Widgets.LayerEditor.ViewportChrome;

namespace Artifact {

LayerEditorViewKeyInputResult LayerEditorViewKeyInputController::handle(
    const LayerEditorViewKeyInputState& state,
    const LayerEditorViewKeyInputCallbacks& callbacks,
    ArtifactIRenderer& renderer) const
{
 if (!state.zoomLevel) return {};
 if (state.altModifier) {
  int layerState = -1;
  if (state.key == Qt::Key_V) layerState = 0;
  if (state.key == Qt::Key_L) layerState = 1;
  if (state.key == Qt::Key_S) layerState = 2;
  if (layerState >= 0 && state.hasTargetLayer) {
   if (!state.autoRepeat && callbacks.toggleLayerState)
    callbacks.toggleLayerState(layerState);
   return {true, false};
  }
  if (state.key >= Qt::Key_1 && state.key <= Qt::Key_3) {
   const LayerEditorSurfaceMode modes[] = {
       LayerEditorSurfaceMode::Edit,
       LayerEditorSurfaceMode::Inspect,
       LayerEditorSurfaceMode::Impact};
   if (callbacks.setSurfaceMode)
    callbacks.setSurfaceMode(modes[state.key - Qt::Key_1]);
   return {true, false};
  }
  DisplayMode displayMode = DisplayMode::Color;
  bool hasDisplayMode = true;
  switch (state.key) {
   case Qt::Key_C: displayMode = DisplayMode::Color; break;
   case Qt::Key_A: displayMode = DisplayMode::Alpha; break;
   case Qt::Key_M: displayMode = DisplayMode::Mask; break;
   case Qt::Key_W: displayMode = DisplayMode::Wireframe; break;
   default: hasDisplayMode = false; break;
  }
  if (hasDisplayMode) {
   if (callbacks.setDisplayMode) callbacks.setDisplayMode(displayMode);
   return {true, false};
  }
 }
 switch (state.key) {
  case Qt::Key_F:
   renderer.fitToViewport();
   *state.zoomLevel = renderer.getZoom();
   return {true, true};
  case Qt::Key_R:
   renderer.resetView();
   *state.zoomLevel = 1.0f;
   return {true, true};
  case Qt::Key_1:
   *state.zoomLevel = 1.0f;
   renderer.zoomAroundViewportPoint(
       {static_cast<float>(state.viewportCenter.x()),
        static_cast<float>(state.viewportCenter.y())}, *state.zoomLevel);
   return {true, true};
  case Qt::Key_Plus:
  case Qt::Key_Equal:
   *state.zoomLevel = std::clamp(*state.zoomLevel * 1.1f, 0.05f, 32.0f);
   renderer.zoomAroundViewportPoint(
       {static_cast<float>(state.viewportCenter.x()),
        static_cast<float>(state.viewportCenter.y())}, *state.zoomLevel);
   return {true, true};
  case Qt::Key_Minus:
  case Qt::Key_Underscore:
   *state.zoomLevel = std::clamp(*state.zoomLevel / 1.1f, 0.05f, 32.0f);
   renderer.zoomAroundViewportPoint(
       {static_cast<float>(state.viewportCenter.x()),
        static_cast<float>(state.viewportCenter.y())}, *state.zoomLevel);
   return {true, true};
  case Qt::Key_Left: renderer.panBy(24.0f, 0.0f); return {true, true};
  case Qt::Key_Right: renderer.panBy(-24.0f, 0.0f); return {true, true};
  case Qt::Key_Up: renderer.panBy(0.0f, 24.0f); return {true, true};
  case Qt::Key_Down: renderer.panBy(0.0f, -24.0f); return {true, true};
  default: return {};
 }
}

}
