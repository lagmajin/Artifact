module;

#include <Qt>

#include <algorithm>
#include <utility>

module Artifact.Widgets.LayerEditor.KeyInputController;

import Artifact.Widgets.LayerEditor.ModalTransformController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeInputController;

namespace Artifact {
namespace {

constexpr float kMinProportionalRadius = 8.0f;
constexpr float kMaxProportionalRadius = 4096.0f;

bool isDeleteKey(int key)
{
 return key == Qt::Key_Delete || key == Qt::Key_Backspace;
}

LayerEditorKeyInputResult beginModal(
    LayerEditorModalTransformMode mode, LayerEditorKeyCursor cursor,
    bool requestRender, const LayerEditorKeyInputCallbacks& callbacks)
{
 if (!callbacks.beginModalTransform || !callbacks.beginModalTransform(mode))
  return {};
 return {true, requestRender, cursor};
}

}

LayerEditorKeyInputResult LayerEditorKeyInputController::handle(
    const LayerEditorKeyInputState& state,
    LayerEditorKeyInputCallbacks callbacks,
    LayerEditorModalTransformController& modalTransform,
    LayerEditorShapeInputController& shapeInput,
    LayerEditorShapeEditSession& shapeEditSession) const
{
 if (modalTransform.active() &&
     (state.key == Qt::Key_X || state.key == Qt::Key_Y)) {
  modalTransform.toggleAxis(state.key == Qt::Key_X
      ? LayerEditorModalTransformAxis::X : LayerEditorModalTransformAxis::Y);
  return {true, false, LayerEditorKeyCursor::Unchanged};
 }
 if (modalTransform.active() &&
     (state.key == Qt::Key_Return || state.key == Qt::Key_Enter)) {
  if (callbacks.commitModalTransform) callbacks.commitModalTransform();
  return {true, true, LayerEditorKeyCursor::Unset};
 }
 if (modalTransform.active() && state.key == Qt::Key_Escape) {
  if (callbacks.cancelModalTransform) callbacks.cancelModalTransform();
  return {true, true, LayerEditorKeyCursor::Unset};
 }

 if (!state.rendererAvailable) return {};
 const bool editingGeometry = state.maskEditing || state.shapeEditing;
 if (editingGeometry && state.key == Qt::Key_O &&
     state.proportionalEditingEnabled) {
  *state.proportionalEditingEnabled = !*state.proportionalEditingEnabled;
  return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 if (editingGeometry &&
     (state.key == Qt::Key_BracketLeft || state.key == Qt::Key_BracketRight) &&
     state.proportionalEditRadius) {
  const float scale = state.key == Qt::Key_BracketLeft ? 0.85f : 1.15f;
  *state.proportionalEditRadius = std::clamp(
      *state.proportionalEditRadius * scale,
      kMinProportionalRadius, kMaxProportionalRadius);
  return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 if (state.maskEditing && isDeleteKey(state.key) &&
     callbacks.deleteMaskVertex && callbacks.deleteMaskVertex()) {
  return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 if (!state.shapeEditing ||
     !state.selectedPolygonIndices || !state.selectedPathIndices) return {};

 if (state.key == Qt::Key_E && !state.controlModifier) {
  const auto result = shapeInput.handle(
      LayerEditorShapeKeyAction::Extrude, state.layer,
      *state.selectedPolygonIndices, *state.selectedPathIndices,
      shapeEditSession);
  if (result == LayerEditorShapeKeyResult::ExtrudedPolygon ||
      result == LayerEditorShapeKeyResult::ExtrudedPath) {
   if (callbacks.beginModalTransform &&
       callbacks.beginModalTransform(LayerEditorModalTransformMode::Grab)) {
    return {true, true, LayerEditorKeyCursor::Move};
   }
   if (result == LayerEditorShapeKeyResult::ExtrudedPath)
    shapeEditSession.commitPath();
   else
    shapeEditSession.commitPolygon();
   return {true, true, LayerEditorKeyCursor::Unchanged};
  }
 }
 if (state.key == Qt::Key_F && !state.controlModifier) {
  const auto result = shapeInput.handle(
      LayerEditorShapeKeyAction::ToggleClosed, state.layer,
      *state.selectedPolygonIndices, *state.selectedPathIndices,
      shapeEditSession);
  if (result == LayerEditorShapeKeyResult::GeometryChanged)
   return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 if (state.key == Qt::Key_A && !state.controlModifier) {
  const auto result = shapeInput.handle(
      LayerEditorShapeKeyAction::ToggleSelectAll, state.layer,
      *state.selectedPolygonIndices, *state.selectedPathIndices,
      shapeEditSession);
  if (result == LayerEditorShapeKeyResult::SelectionChanged)
   return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 if (state.key == Qt::Key_G && !state.controlModifier)
  return beginModal(LayerEditorModalTransformMode::Grab,
                    LayerEditorKeyCursor::Move, true, callbacks);
 if (state.key == Qt::Key_R && !state.controlModifier)
  return beginModal(LayerEditorModalTransformMode::Rotate,
                    LayerEditorKeyCursor::Cross, false, callbacks);
 if (state.key == Qt::Key_S && !state.controlModifier)
  return beginModal(LayerEditorModalTransformMode::Scale,
                    LayerEditorKeyCursor::Move, true, callbacks);
 if (state.key == Qt::Key_I && !state.controlModifier)
  return beginModal(LayerEditorModalTransformMode::Inset,
                    LayerEditorKeyCursor::HorizontalResize, true, callbacks);
 if (isDeleteKey(state.key) && callbacks.deleteShapeGeometry &&
     callbacks.deleteShapeGeometry()) {
  return {true, true, LayerEditorKeyCursor::Unchanged};
 }
 return {};
}

}
