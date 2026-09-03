module;

#include <QPointF>

#include <algorithm>
#include <vector>

module Artifact.Widgets.LayerEditor.MaskPressController;

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
    LayerEditorMaskEditSession& editSession) const
{
 LayerEditorMaskPressResult result;
 if (!layer) return result;
 const float safeZoom = std::max(0.1f, zoom);
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
         layer, canvasPosition, 8.0f / std::max(0.1f, zoom),
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
