module;

#include <Qt>

module Artifact.Widgets.LayerEditor.ReleaseController;

namespace Artifact {
namespace {

void clearMaskDrag(LayerEditorReleaseState& state, bool handle)
{
 if (handle && state.maskHandleDragState) *state.maskHandleDragState = false;
 if (!handle && state.maskVertexDragState) *state.maskVertexDragState = false;
 if (state.maskIndex) *state.maskIndex = -1;
 if (state.maskPathIndex) *state.maskPathIndex = -1;
 if (state.maskVertexIndex) *state.maskVertexIndex = -1;
 if (handle && state.maskHandleType) *state.maskHandleType = -1;
}

}

LayerEditorReleaseResult LayerEditorReleaseController::handle(
    LayerEditorReleaseState state,
    const LayerEditorReleaseCallbacks& callbacks) const
{
 if (state.modalActive && state.button == Qt::LeftButton) {
  if (callbacks.commitModal) callbacks.commitModal();
  return {true, true, true};
 }
 if (state.modalActive && state.button == Qt::RightButton) {
  if (callbacks.cancelModal) callbacks.cancelModal();
  return {true, true, true};
 }
 if (state.maskEditing && state.button == Qt::LeftButton) {
  if (state.draggingMaskHandle || state.draggingMaskVertex) {
   clearMaskDrag(state, state.draggingMaskHandle);
   if (callbacks.resetProportionalState) callbacks.resetProportionalState();
   if (callbacks.commitMaskEdit) callbacks.commitMaskEdit();
   return {true, false, true};
  }
 }
 if (state.parameterActive && state.button == Qt::LeftButton) {
  if (callbacks.commitParameterEdit) callbacks.commitParameterEdit();
  return {true, true, true};
 }
 if (state.shapeEditing && state.button == Qt::LeftButton) {
  if (state.draggingPathVertex || state.draggingPathTangent) {
   if (state.pathVertexDragState) *state.pathVertexDragState = false;
   if (state.pathTangentDragState) *state.pathTangentDragState = false;
   if (state.pathVertexIndex) *state.pathVertexIndex = -1;
   if (callbacks.resetProportionalState) callbacks.resetProportionalState();
   if (callbacks.commitPathEdit) callbacks.commitPathEdit();
   return {true, false, true};
  }
  if (state.pathEditPending) {
   if (callbacks.commitPathEdit) callbacks.commitPathEdit();
   return {true, false, false};
  }
  if (state.draggingPolygonVertex) {
   if (state.polygonVertexDragState) *state.polygonVertexDragState = false;
   if (state.polygonVertexIndex) *state.polygonVertexIndex = -1;
   if (state.selectedPolygonBefore) state.selectedPolygonBefore->clear();
   if (callbacks.resetProportionalState) callbacks.resetProportionalState();
   if (callbacks.commitPolygonEdit) callbacks.commitPolygonEdit();
   return {true, false, true};
  }
  if (state.polygonEditPending) {
   if (callbacks.commitPolygonEdit) callbacks.commitPolygonEdit();
   return {true, false, false};
  }
 }
 if (state.gizmoDragging && state.button == Qt::LeftButton) {
  if (callbacks.releaseGizmo) callbacks.releaseGizmo();
  return {true, true, true};
 }
 return {};
}

}
