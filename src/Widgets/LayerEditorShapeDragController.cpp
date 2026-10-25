module;

#include <QPointF>

#include <algorithm>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapeDragController;

import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.Geometry;

namespace Artifact {

bool LayerEditorShapeDragController::dragPolygonVertex(
    ArtifactShapeLayer& shape, const QPointF& localPosition,
    int vertexIndex, const std::vector<int>& selectedIndices,
    const std::vector<QPointF>& dragBefore,
    bool proportionalEditingEnabled, bool proportionalDragActive,
    const std::vector<QPointF>& proportionalBefore,
    const QPointF& proportionalOrigin, float proportionalRadius) const
{
 auto points = shape.customPolygonPoints();
 if (vertexIndex < 0 || vertexIndex >= static_cast<int>(points.size()))
  return false;

 if (!proportionalEditingEnabled && selectedIndices.size() > 1 &&
     dragBefore.size() == points.size()) {
  const QPointF delta = localPosition - dragBefore[static_cast<size_t>(vertexIndex)];
  for (int selectedIndex : selectedIndices) {
   if (selectedIndex < 0 || selectedIndex >= static_cast<int>(points.size())) continue;
   const QPointF moved = dragBefore[static_cast<size_t>(selectedIndex)] + delta;
   points[static_cast<size_t>(selectedIndex)] = QPointF(
       std::clamp(moved.x(), 0.0, static_cast<double>(shape.shapeWidth())),
       std::clamp(moved.y(), 0.0, static_cast<double>(shape.shapeHeight())));
  }
 } else if (proportionalDragActive && !proportionalBefore.empty() &&
            vertexIndex < static_cast<int>(proportionalBefore.size())) {
  points = proportionalShapePoints(
      proportionalBefore, proportionalOrigin, localPosition,
      proportionalRadius, shape.shapeWidth(), shape.shapeHeight());
 } else {
  points[static_cast<size_t>(vertexIndex)] = QPointF(
      std::clamp(localPosition.x(), 0.0, static_cast<double>(shape.shapeWidth())),
      std::clamp(localPosition.y(), 0.0, static_cast<double>(shape.shapeHeight())));
 }
 shape.setCustomPolygonPoints(points, shape.customPolygonClosed());
 return true;
}

bool LayerEditorShapeDragController::dragPathVertex(
    ArtifactShapeLayer& shape, const QPointF& localPosition,
    int vertexIndex, const std::vector<int>& selectedIndices,
    const std::vector<CustomPathVertex>& dragBefore,
    bool proportionalEditingEnabled, bool proportionalDragActive,
    const std::vector<CustomPathVertex>& proportionalBefore,
    const QPointF& proportionalOrigin, float proportionalRadius) const
{
 auto vertices = shape.customPathVertices();
 if (vertexIndex < 0 || vertexIndex >= static_cast<int>(vertices.size()))
  return false;

 if (!proportionalEditingEnabled && selectedIndices.size() > 1 &&
     dragBefore.size() == vertices.size()) {
  const QPointF delta = localPosition - dragBefore[static_cast<size_t>(vertexIndex)].pos;
  for (int selectedIndex : selectedIndices) {
   if (selectedIndex < 0 || selectedIndex >= static_cast<int>(vertices.size())) continue;
   vertices[static_cast<size_t>(selectedIndex)].pos =
       dragBefore[static_cast<size_t>(selectedIndex)].pos + delta;
  }
 } else if (proportionalDragActive && !proportionalBefore.empty() &&
            vertexIndex < static_cast<int>(proportionalBefore.size())) {
  vertices = proportionalPathVertices(
      proportionalBefore, proportionalOrigin, localPosition, proportionalRadius);
 } else {
  vertices[static_cast<size_t>(vertexIndex)].pos = localPosition;
 }
 shape.setCustomPathVertices(vertices, shape.customPathClosed());
 return true;
}

bool LayerEditorShapeDragController::dragPathTangent(
    ArtifactShapeLayer& shape, const QPointF& localPosition,
    int vertexIndex, int tangentType, bool independentTangent) const
{
 auto vertices = shape.customPathVertices();
 if (vertexIndex < 0 || vertexIndex >= static_cast<int>(vertices.size()))
  return false;
 auto& vertex = vertices[static_cast<size_t>(vertexIndex)];
 const QPointF delta = localPosition - vertex.pos;
 if (tangentType == 1) {
  vertex.outTangent = delta;
  if (vertex.smooth && !independentTangent) vertex.inTangent = -delta;
 } else {
  vertex.inTangent = delta;
  if (vertex.smooth && !independentTangent) vertex.outTangent = -delta;
 }
 shape.setCustomPathVertices(vertices, shape.customPathClosed());
 return true;
}

}
