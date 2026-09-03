module;

#include <Qt>
#include <QString>

#include <algorithm>

module Artifact.Widgets.LayerEditor.ViewportChromeInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Tool;

namespace Artifact {

LayerEditorViewportChromePressResult
LayerEditorViewportChromeInteractionController::press(
    const LayerEditorViewportChromeInteractionState& state,
    const LayerEditorViewportChromeCallbacks& callbacks,
    ArtifactIRenderer& renderer) const
{
 const int control = layerEditorChromeControlAt(
     state.viewportPosition * state.devicePixelRatio,
     state.physicalViewportSize, state.surfaceMode,
     state.hasLayerIdentity);
 if (control < 0) return {};
 if (control == 43)
  return {callbacks.toggleLayerState ? callbacks.toggleLayerState(2) : false};
 if (control >= 30 && control <= 32) {
  const LayerEditorSurfaceMode modes[] = {
      LayerEditorSurfaceMode::Edit,
      LayerEditorSurfaceMode::Inspect,
      LayerEditorSurfaceMode::Impact};
  if (callbacks.setSurfaceMode) callbacks.setSurfaceMode(modes[control - 30]);
  return {true};
 }
 if (control >= 0 && control <= 3) {
  const EditMode modes[] = {
      EditMode::View, EditMode::Transform, EditMode::Shape, EditMode::Mask};
  if (layerEditorEditModeAvailable(state.layer, modes[control]) &&
      callbacks.setEditMode) callbacks.setEditMode(modes[control]);
  return {true};
 }
 if (control >= 10 && control <= 13) {
  const DisplayMode modes[] = {
      DisplayMode::Color, DisplayMode::Alpha,
      DisplayMode::Mask, DisplayMode::Wireframe};
  if (callbacks.setDisplayMode) callbacks.setDisplayMode(modes[control - 10]);
  return {true};
 }
 if (control >= 40 && control <= 42)
  return {callbacks.toggleLayerState
              ? callbacks.toggleLayerState(control - 40) : false};
 if (control < 20 || control > 23 || !state.zoomLevel) return {};

 if (control == 20)
  *state.zoomLevel = std::clamp(renderer.getZoom() / 1.1f, 0.05f, 32.0f);
 else if (control == 21)
  *state.zoomLevel = 1.0f;
 else if (control == 22)
  *state.zoomLevel = std::clamp(renderer.getZoom() * 1.1f, 0.05f, 32.0f);
 if (control <= 22) {
  renderer.zoomAroundViewportPoint(
      {static_cast<float>(state.physicalViewportCenter.x()),
       static_cast<float>(state.physicalViewportCenter.y())},
      *state.zoomLevel);
 } else {
  renderer.fitToViewport();
  *state.zoomLevel = renderer.getZoom();
 }
 return {true, true};
}

LayerEditorViewportChromeHoverResult
LayerEditorViewportChromeInteractionController::hover(
    const LayerEditorViewportChromeInteractionState& state) const
{
 const int nextControl = layerEditorChromeControlAt(
     state.viewportPosition * state.devicePixelRatio,
     state.physicalViewportSize, state.surfaceMode,
     state.hasLayerIdentity);
 bool enabled = true;
 if (nextControl >= 0 && nextControl <= 3) {
  const EditMode modes[] = {
      EditMode::View, EditMode::Transform, EditMode::Shape, EditMode::Mask};
  enabled = layerEditorEditModeAvailable(state.layer, modes[nextControl]);
 }

 LayerEditorViewportChromeHoverResult result;
 result.overChrome = nextControl >= 0;
 const int previous = state.hoveredControl ? *state.hoveredControl : -1;
 if (previous != nextControl) {
  if (state.hoveredControl) *state.hoveredControl = nextControl;
  result.requestRender = true;
  result.updateToolTip = true;
  result.toolTip = layerEditorChromeToolTip(nextControl);
  if (nextControl >= 0 && nextControl <= 3 && !enabled) {
   result.toolTip = !state.layer
       ? QStringLiteral("Select a layer to edit")
       : !state.layer->isVisible() || state.layer->isLocked()
           ? QStringLiteral("Unlock and show the layer to edit it")
           : nextControl == 2
               ? QStringLiteral("Shape editing is unavailable for this layer")
               : QStringLiteral("Add a mask before entering mask edit mode");
  }
 }
 if (nextControl >= 0) {
  result.cursor = enabled ? LayerEditorChromeCursor::Pointing
                          : LayerEditorChromeCursor::Arrow;
 } else if (previous >= 0) {
  result.cursor = state.editMode == EditMode::Mask && state.layer &&
                          state.layer->isVisible() && !state.layer->isLocked()
      ? LayerEditorChromeCursor::Cross : LayerEditorChromeCursor::Unset;
 }
 return result;
}

}
