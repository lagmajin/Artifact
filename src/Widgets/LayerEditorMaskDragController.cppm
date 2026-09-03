module;

#include <QPointF>
#include <QTransform>

#include <vector>

module Artifact.Widgets.LayerEditor.MaskDragController;

import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;

namespace Artifact {
namespace {

bool localPosition(const ArtifactAbstractLayerPtr& layer,
                   const QPointF& canvasPosition, QPointF& result)
{
 if (!layer) return false;
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return false;
 result = inverse.map(canvasPosition);
 return true;
}

bool validAddress(const ArtifactAbstractLayerPtr& layer,
                  int maskIndex, int pathIndex, int vertexIndex)
{
 if (!layer || maskIndex < 0 || maskIndex >= layer->maskCount()) return false;
 const LayerMask mask = layer->mask(maskIndex);
 if (pathIndex < 0 || pathIndex >= mask.maskPathCount()) return false;
 const MaskPath path = mask.maskPath(pathIndex);
 return vertexIndex >= 0 && vertexIndex < path.vertexCount();
}

}

bool LayerEditorMaskDragController::dragHandle(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    int maskIndex, int pathIndex, int vertexIndex,
    MaskHandleType handleType) const
{
 if (!validAddress(layer, maskIndex, pathIndex, vertexIndex)) return false;
 QPointF local;
 if (!localPosition(layer, canvasPosition, local)) return false;
 LayerMask mask = layer->mask(maskIndex);
 MaskPath path = mask.maskPath(pathIndex);
 MaskVertex vertex = path.vertex(vertexIndex);
 const QPointF delta = local - vertex.position;
 if (handleType == MaskHandleType::InTangent) vertex.inTangent = delta;
 else if (handleType == MaskHandleType::OutTangent) vertex.outTangent = delta;
 else return false;
 path.setVertex(vertexIndex, vertex);
 mask.setMaskPath(pathIndex, path);
 layer->setMask(maskIndex, mask);
 return true;
}

bool LayerEditorMaskDragController::dragVertex(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    int maskIndex, int pathIndex, int vertexIndex,
    bool proportionalDragActive,
    const std::vector<MaskVertex>& proportionalBefore,
    const QPointF& proportionalOrigin, float proportionalRadius) const
{
 if (!validAddress(layer, maskIndex, pathIndex, vertexIndex)) return false;
 QPointF local;
 if (!localPosition(layer, canvasPosition, local)) return false;
 LayerMask mask = layer->mask(maskIndex);
 MaskPath path = mask.maskPath(pathIndex);
 if (proportionalDragActive &&
     proportionalBefore.size() == static_cast<size_t>(path.vertexCount())) {
  const auto vertices = proportionalMaskVertices(
      proportionalBefore, proportionalOrigin, local, proportionalRadius);
  for (int index = 0; index < path.vertexCount(); ++index)
   path.setVertex(index, vertices[static_cast<size_t>(index)]);
 } else {
  MaskVertex vertex = path.vertex(vertexIndex);
  vertex.position = local;
  path.setVertex(vertexIndex, vertex);
 }
 mask.setMaskPath(pathIndex, path);
 layer->setMask(maskIndex, mask);
 return true;
}

}
