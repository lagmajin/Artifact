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

bool LayerEditorMaskDragController::transformMasks(
    const ArtifactAbstractLayerPtr& layer, const MaskAffineTransform& transform,
    const int maskIndex) const
{
 if (!layer || transform.isIdentity()) return false;

 // rotation が混ざると feather の軸方向が回ってしまうため、長さ系
 // (feather / expansion) は平均スケールで一段だけスケールし、頂点だけを
 // 完全アフィン変換する。
 const float lengthFactor = transform.lengthScale();

 bool changed = false;
 for (int m = 0; m < layer->maskCount(); ++m) {
  if (maskIndex >= 0 && m != maskIndex) continue;
  LayerMask mask = layer->mask(m);
  bool maskChanged = false;
  for (int p = 0; p < mask.maskPathCount(); ++p) {
   MaskPath path = mask.maskPath(p);
   std::vector<MaskVertex> vertices;
   vertices.reserve(static_cast<size_t>(path.vertexCount()));
   for (int v = 0; v < path.vertexCount(); ++v)
    vertices.push_back(path.vertex(v));
   if (vertices.empty()) continue;
   for (MaskVertex& vertex : vertices) {
    vertex.position = transform.map(vertex.position);
    vertex.inTangent = transform.mapVector(vertex.inTangent);
    vertex.outTangent = transform.mapVector(vertex.outTangent);
   }
   for (int v = 0; v < path.vertexCount(); ++v)
    path.setVertex(v, vertices[static_cast<size_t>(v)]);
   path.setFeather(scaleMaskLength(path.feather(), lengthFactor));
   path.setFeatherHorizontal(
       scaleMaskLength(path.featherHorizontal(), lengthFactor));
   path.setFeatherVertical(
       scaleMaskLength(path.featherVertical(), lengthFactor));
   path.setFeatherInner(scaleMaskLength(path.featherInner(), lengthFactor));
   path.setFeatherOuter(scaleMaskLength(path.featherOuter(), lengthFactor));
   path.setExpansion(scaleMaskLength(path.expansion(), lengthFactor));
   mask.setMaskPath(p, path);
   maskChanged = true;
  }
  if (maskChanged) {
   layer->setMask(m, mask);
   changed = true;
  }
 }
 return changed;
}

}
