module;

#include <QPointF>

#include <vector>

module Artifact.Widgets.LayerEditor.MaskPressController;

import Core.ArtifactMath;
import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;
import Artifact.Widgets.LayerEditor.MaskEditSession;

namespace Artifact {

LayerEditorMaskPressResult LayerEditorMaskPressController::handle(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom,
    bool proportionalEditingEnabled,
    int boundsMaskIndex,
    LayerEditorMaskEditSession& editSession) const
{
  LayerEditorMaskPressResult result;
  if (!layer) return result;
  const float safeZoom = ArtifactCore::artifactMax(0.1f, zoom);

  // ==== bounds ギズモのハンドル ====
  // 頂点ハンドルより先に判定する。ギズモは枠の外側にあるため、
  // 先に頂点判定すると頂点ドラッグがギズモを奪ってしまう。
  if (boundsMaskIndex >= 0) {
   QRectF localBounds;
   if (maskBounds(layer, boundsMaskIndex, localBounds) &&
       !localBounds.isEmpty()) {
    const MaskBoundsHandle handle = hitTestMaskBoundsHandle(
        localBounds, layer->getGlobalTransform(), canvasPosition,
        10.0f / safeZoom);
    if (handle != MaskBoundsHandle::None) {
     editSession.begin(layer);
     result.kind = LayerEditorMaskPressKind::DragBounds;
     result.maskIndex = boundsMaskIndex;
     result.boundsHandle = handle;
     result.boundsBefore = localBounds;
     // 反対側の点（角 → 対角の隅、辺 → 反対の辺、Center → 中心）を
     // 不動点アンカーにする。
     const qreal cx = localBounds.center().x();
     const qreal cy = localBounds.center().y();
     switch (handle) {
     case MaskBoundsHandle::TopLeft: result.boundsAnchor = localBounds.bottomRight(); break;
     case MaskBoundsHandle::TopRight: result.boundsAnchor = localBounds.bottomLeft(); break;
     case MaskBoundsHandle::BottomLeft: result.boundsAnchor = localBounds.topRight(); break;
     case MaskBoundsHandle::BottomRight: result.boundsAnchor = localBounds.topLeft(); break;
     case MaskBoundsHandle::Top: result.boundsAnchor = QPointF(cx, localBounds.bottom()); break;
     case MaskBoundsHandle::Bottom: result.boundsAnchor = QPointF(cx, localBounds.top()); break;
     case MaskBoundsHandle::Left: result.boundsAnchor = QPointF(localBounds.right(), cy); break;
     case MaskBoundsHandle::Right: result.boundsAnchor = QPointF(localBounds.left(), cy); break;
     case MaskBoundsHandle::Center:
     case MaskBoundsHandle::None:
      result.boundsAnchor = localBounds.center();
      break;
     }
     return result;
    }
   }
  }

  if (hitTestMaskHandle(
         layer, canvasPosition, 10.0f / safeZoom,
         result.maskIndex, result.pathIndex, result.vertexIndex,
         result.handleType)) {
  editSession.begin(layer);
  result.kind = LayerEditorMaskPressKind::DragHandle;
  return result;
 }

 if (!hitTestMaskVertexGeometry(
         layer, canvasPosition, 8.0f / safeZoom,
         result.maskIndex, result.pathIndex, result.vertexIndex)) {
  return result;
 }

 LayerMask mask = layer->mask(result.maskIndex);
 MaskPath path = mask.maskPath(result.pathIndex);
 if (result.vertexIndex == 0 && !path.isClosed() && path.vertexCount() > 2) {
  editSession.begin(layer);
  path.setClosed(true);
  mask.setMaskPath(result.pathIndex, path);
  layer->setMask(result.maskIndex, mask);
  editSession.markDirty();
  editSession.commit();
  result.kind = LayerEditorMaskPressKind::GeometryChanged;
  return result;
 }

 editSession.begin(layer);
 result.kind = LayerEditorMaskPressKind::DragVertex;
 if (proportionalEditingEnabled && result.vertexIndex >= 0 &&
     result.vertexIndex < path.vertexCount()) {
  result.proportionalBefore.reserve(static_cast<size_t>(path.vertexCount()));
  for (int index = 0; index < path.vertexCount(); ++index)
   result.proportionalBefore.push_back(path.vertex(index));
  result.proportionalOrigin = path.vertex(result.vertexIndex).position;
 }
 return result;
}

bool LayerEditorMaskPressController::closeOpenPathOnDoubleClick(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom,
    LayerEditorMaskEditSession& editSession) const
{
 if (!layer) return false;
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 if (!hitTestMaskVertexGeometry(
         layer, canvasPosition, 8.0f / ArtifactCore::artifactMax(0.1f, zoom),
         maskIndex, pathIndex, vertexIndex)) return false;
 LayerMask mask = layer->mask(maskIndex);
 MaskPath path = mask.maskPath(pathIndex);
 if (vertexIndex != 0 || path.isClosed() || path.vertexCount() <= 2)
  return false;
 editSession.begin(layer);
 path.setClosed(true);
 mask.setMaskPath(pathIndex, path);
 layer->setMask(maskIndex, mask);
 editSession.markDirty();
 return true;
}

}
