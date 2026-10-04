module;

#include <algorithm>
#include <utility>

module Artifact.Widgets.LayerEditor.MaskPressInteractionController;

import Core.ArtifactTuple;
import Artifact.Layer.Abstract;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.MaskHoverController;
import Artifact.Widgets.LayerEditor.MaskPressController;

namespace Artifact {

LayerEditorMaskPressInteractionResult
LayerEditorMaskPressInteractionController::handle(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, LayerEditorMaskPressInteractionState state,
    LayerEditorMaskHoverController& hoverController,
    LayerEditorMaskEditSession& editSession) const
{
 if (!layer || !layer->isVisible() || layer->isLocked()) return {};

LayerEditorMaskPressController pressController;
  auto result = pressController.handle(
      layer, canvasPosition, zoom, state.proportionalEditingEnabled,
      state.boundsMaskIndex, editSession);
  if (result.kind == LayerEditorMaskPressKind::Empty) return {};

  if (result.kind == LayerEditorMaskPressKind::DragBounds) {
   // bounds ドラッグは頂点選択の操作ではなく、選択中のマスク全体を変形する。
   if (state.draggingVertex) *state.draggingVertex = false;
   if (state.draggingHandle) *state.draggingHandle = false;
   if (state.draggingMaskIndex) *state.draggingMaskIndex = result.maskIndex;
   if (state.draggingPathIndex) *state.draggingPathIndex = -1;
   if (state.draggingVertexIndex) *state.draggingVertexIndex = -1;
   if (state.boundsHandle) *state.boundsHandle = result.boundsHandle;
   if (state.boundsBefore) *state.boundsBefore = result.boundsBefore;
   if (state.boundsAnchor) *state.boundsAnchor = result.boundsAnchor;
   return {true, false, true};
  }

  if (state.selectedVertices && result.vertexIndex >= 0) {
  const auto address = ArtifactCore::artifactMakeTuple(
      result.maskIndex, result.pathIndex, result.vertexIndex);
  const auto found = std::find(state.selectedVertices->begin(),
                               state.selectedVertices->end(), address);
  if (!state.additiveSelection) {
   state.selectedVertices->clear();
   state.selectedVertices->push_back(address);
  } else if (found == state.selectedVertices->end()) {
   state.selectedVertices->push_back(address);
  }
 }
 if (result.kind == LayerEditorMaskPressKind::DragHandle) {
  if (state.draggingVertex) *state.draggingVertex = false;
  if (state.draggingHandle) *state.draggingHandle = true;
  if (state.draggingHandleType)
   *state.draggingHandleType = static_cast<int>(result.handleType);
 } else if (result.kind == LayerEditorMaskPressKind::DragVertex) {
  if (state.draggingHandle) *state.draggingHandle = false;
  if (state.draggingVertex) *state.draggingVertex = true;
  if (state.proportionalDragActive) *state.proportionalDragActive = false;
  if (state.proportionalDragOrigin) *state.proportionalDragOrigin = {};
  if (state.proportionalMaskBefore) state.proportionalMaskBefore->clear();
  if (state.proportionalPolygonBefore) state.proportionalPolygonBefore->clear();
  if (state.proportionalPathBefore) state.proportionalPathBefore->clear();
  if (state.proportionalMaskBefore)
   *state.proportionalMaskBefore = std::move(result.proportionalBefore);
  if (state.proportionalDragOrigin)
   *state.proportionalDragOrigin = result.proportionalOrigin;
  if (state.proportionalDragActive && state.proportionalMaskBefore)
   *state.proportionalDragActive = !state.proportionalMaskBefore->empty();
 }

 const bool startsDrag = result.kind == LayerEditorMaskPressKind::DragHandle ||
                         result.kind == LayerEditorMaskPressKind::DragVertex;
 if (startsDrag) {
  if (state.draggingMaskIndex) *state.draggingMaskIndex = result.maskIndex;
  if (state.draggingPathIndex) *state.draggingPathIndex = result.pathIndex;
  if (state.draggingVertexIndex) *state.draggingVertexIndex = result.vertexIndex;
  hoverController.set({
      .maskIndex = result.maskIndex,
      .pathIndex = result.pathIndex,
      .vertexIndex = result.vertexIndex,
      .handleType = result.kind == LayerEditorMaskPressKind::DragHandle
          ? result.handleType : MaskHandleType::None});
 }
 return {result.kind != LayerEditorMaskPressKind::Empty,
         result.kind == LayerEditorMaskPressKind::GeometryChanged,
         startsDrag};
}

}
