module;

#include <algorithm>
#include <cmath>
#include <vector>

module Artifact.Widgets.LayerEditor.MaskMoveController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;
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

 if (state.boundsHandle != MaskBoundsHandle::None) {
  bool invertible = false;
  const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
  if (!invertible) return {};
  const QPointF local = inverse.map(canvasPosition);
  const QRectF before = state.boundsBefore;
  if (before.width() <= 0.0 || before.height() <= 0.0) return {};
  const QPointF handleStart = maskBoundsHandlePosition(before, state.boundsHandle);
  const QPointF anchor = state.boundsAnchor;
  // Center は平行移動、それ以外はアンカーを不動点としたスケール。
  if (state.boundsHandle == MaskBoundsHandle::Center) {
   const QPointF delta = local - anchor;
   if (delta.isNull()) return {LayerEditorMaskMoveKind::None, false, true};
   if (dragController.transformMasks(
           layer, MaskAffineTransform::translating(delta), state.maskIndex)) {
    editSession.markDirty();
    return {LayerEditorMaskMoveKind::GeometryChanged, false, true};
   }
   return {LayerEditorMaskMoveKind::None, false, true};
  }
  // 基準 bounds のハンドル位置からアンカーへのベクトルを軸ごとの基準長で割る。
  float factorX = 1.0f;
  float factorY = 1.0f;
  if (std::abs(handleStart.x() - anchor.x()) > 1e-6)
   factorX = static_cast<float>((local.x() - anchor.x()) / (handleStart.x() - anchor.x()));
  if (std::abs(handleStart.y() - anchor.y()) > 1e-6)
   factorY = static_cast<float>((local.y() - anchor.y()) / (handleStart.y() - anchor.y()));
  // 反転（負スケール）を許すと feather が負になり破綻するので、0.01 にクランプ。
  const QPointF factors(std::max(factorX, 0.01f), std::max(factorY, 0.01f));
  const MaskAffineTransform transform =
   MaskAffineTransform::scaling(anchor, factors);
  if (dragController.transformMasks(layer, transform, state.maskIndex)) {
   editSession.markDirty();
   return {LayerEditorMaskMoveKind::GeometryChanged, false, true};
  }
  return {LayerEditorMaskMoveKind::None, false, true};
 }

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
