module;

#include <QPointF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapePressController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeEditSession;

namespace Artifact {
namespace {

bool updateSelection(std::vector<int>& selection, int index, bool extend)
{
 auto found = std::find(selection.begin(), selection.end(), index);
 if (extend) {
  if (found == selection.end()) selection.push_back(index);
  else selection.erase(found);
 } else if (found == selection.end()) {
  selection = {index};
 }
 return !selection.empty();
}

int insertPolygonPoint(ArtifactShapeLayer& shape,
                       const ArtifactAbstractLayerPtr& layer,
                       int segmentIndex, const QPointF& canvasPosition)
{
 auto points = shape.customPolygonPoints();
 if (points.size() < 2 || segmentIndex < 0) return -1;
 const bool closed = shape.customPolygonClosed();
 const int segmentCount = closed ? static_cast<int>(points.size())
                                 : static_cast<int>(points.size()) - 1;
 if (segmentCount <= 0) return -1;
 segmentIndex = std::clamp(segmentIndex, 0, segmentCount - 1);
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return -1;
 const QPointF raw = inverse.map(canvasPosition);
 const QPointF local(
     std::clamp(raw.x(), 0.0, static_cast<double>(shape.shapeWidth())),
     std::clamp(raw.y(), 0.0, static_cast<double>(shape.shapeHeight())));
 const int inserted = std::clamp(segmentIndex + 1, 0, static_cast<int>(points.size()));
 points.insert(points.begin() + inserted, local);
 shape.setCustomPolygonPoints(points, closed);
 return inserted;
}

}

LayerEditorShapePressResult LayerEditorShapePressController::handle(
    const ArtifactAbstractLayerPtr& layer, ArtifactShapeLayer& shape,
    const QPointF& canvasPosition, const LayerEditorShapePressHit& hit,
    bool extendSelection, std::vector<int>& polygonSelection,
    std::vector<int>& pathSelection,
    LayerEditorShapeEditSession& editSession) const
{
 LayerEditorShapePressResult result;
 if (!layer) return result;

 if (shape.hasCustomPath()) {
  polygonSelection.clear();
  if (hit.pathTangentVertex >= 0) {
   editSession.beginPath(layer);
   result.kind = LayerEditorShapePressKind::DragPathTangent;
   result.vertexIndex = hit.pathTangentVertex;
   result.tangentType = hit.pathTangentType;
   return result;
  }
  if (hit.pathVertex >= 0) {
   if (!updateSelection(pathSelection, hit.pathVertex, extendSelection)) {
    result.kind = LayerEditorShapePressKind::SelectionOnly;
    return result;
   }
   editSession.beginPath(layer);
   result.kind = LayerEditorShapePressKind::DragPathVertex;
   result.vertexIndex = hit.pathVertex;
   result.pathDragBefore = shape.customPathVertices();
   return result;
  }

  pathSelection.clear();
  bool invertible = false;
  const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
  if (!invertible) return result;
  editSession.beginPath(layer);
  auto vertices = shape.customPathVertices();
  CustomPathVertex vertex;
  vertex.pos = inverse.map(canvasPosition);
  vertices.push_back(vertex);
  shape.setCustomPathVertices(vertices, shape.customPathClosed());
  editSession.markPathDirty();
  result.kind = LayerEditorShapePressKind::GeometryChanged;
  result.hoveredPathVertex = static_cast<int>(vertices.size()) - 1;
  return result;
 }

 pathSelection.clear();
 if (hit.polygonVertex >= 0) {
  if (!updateSelection(polygonSelection, hit.polygonVertex, extendSelection)) {
   result.kind = LayerEditorShapePressKind::SelectionOnly;
   return result;
  }
  editSession.beginPolygon(layer);
  result.kind = LayerEditorShapePressKind::DragPolygonVertex;
  result.vertexIndex = hit.polygonVertex;
  result.polygonDragBefore = shape.customPolygonPoints();
  return result;
 }
 if (hit.polygonSegment >= 0) {
  editSession.beginPolygon(layer);
  const int inserted = insertPolygonPoint(
      shape, layer, std::max(0, hit.polygonSegment - 1), canvasPosition);
  if (inserted < 0) {
   editSession.commitPolygon();
   return result;
  }
  editSession.markPolygonDirty();
  polygonSelection = {inserted};
  result.kind = LayerEditorShapePressKind::DragPolygonVertex;
  result.vertexIndex = inserted;
  result.hoveredPolygonVertex = inserted;
  result.hoveredPolygonSegment = inserted - 1;
  result.polygonDragBefore = shape.customPolygonPoints();
  return result;
 }

 polygonSelection.clear();
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return result;
 editSession.beginPolygon(layer);
 auto points = shape.customPolygonPoints();
 const QPointF raw = inverse.map(canvasPosition);
 const QPointF local(
     std::clamp(raw.x(), 0.0, static_cast<double>(shape.shapeWidth())),
     std::clamp(raw.y(), 0.0, static_cast<double>(shape.shapeHeight())));
 if (points.size() < 3) {
  points.clear();
  const int sides = std::max(3, shape.polygonSides());
  const double width = std::max(1, shape.shapeWidth());
  const double height = std::max(1, shape.shapeHeight());
  const QPointF center(width * 0.5, height * 0.5);
  const double radius = std::min(width, height) * 0.45;
  for (int index = 0; index < sides; ++index) {
   const double angle = static_cast<double>(index) * 2.0 * std::numbers::pi /
                        static_cast<double>(sides) - std::numbers::pi * 0.5;
   points.push_back(center + QPointF(std::cos(angle) * radius,
                                     std::sin(angle) * radius));
  }
 } else {
  points.push_back(local);
 }
 shape.setCustomPolygonPoints(points, shape.customPolygonClosed());
 editSession.markPolygonDirty();
 result.kind = LayerEditorShapePressKind::GeometryChanged;
 result.hoveredPolygonVertex = static_cast<int>(points.size()) - 1;
 result.hoveredPolygonSegment = static_cast<int>(points.size()) - 2;
 return result;
}

}
