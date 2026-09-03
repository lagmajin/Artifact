module;

#include <vector>

module Artifact.Widgets.LayerEditor.MaskMoveController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.MaskDragController;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.MaskHoverController;

namespace Artifact {
namespace {

const std::vector<MaskVertex>& verticesOrEmpty(
    const std::vector<MaskVertex>* vertices)
{
 static const std::vector<MaskVertex> empty;
 return vertices ? *vertices : empty;
}

}

LayerEditorMaskMoveResult LayerEditorMaskMoveController::handle(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, const LayerEditorMaskMoveState& state,
    LayerEditorMaskDragController& dragController,
    LayerEditorMaskHoverController& hoverController,
    LayerEditorMaskEditSession& editSession) const
{
 if (!layer || !layer->isVisible() || layer->isLocked()) return {};

 if (state.draggingHandle && dragController.dragHandle(
         layer, canvasPosition, state.maskIndex, state.pathIndex,
         state.vertexIndex, state.handleType)) {
  editSession.markDirty();
  return {LayerEditorMaskMoveKind::GeometryChanged, false, true};
 }
 if (state.draggingVertex && dragController.dragVertex(
         layer, canvasPosition, state.maskIndex, state.pathIndex,
         state.vertexIndex, state.proportionalDragActive,
         verticesOrEmpty(state.proportionalBefore), state.proportionalOrigin,
         state.proportionalRadius)) {
  editSession.markDirty();
  return {LayerEditorMaskMoveKind::GeometryChanged, false, true};
 }
 if (state.draggingVertex) {
  return {LayerEditorMaskMoveKind::None, false, true};
 }

 const bool hoverChanged = hoverController.update(layer, canvasPosition, zoom);
 return {hoverChanged ? LayerEditorMaskMoveKind::HoverChanged
                      : LayerEditorMaskMoveKind::None,
         hoverController.hasVertex(), true};
}

}
