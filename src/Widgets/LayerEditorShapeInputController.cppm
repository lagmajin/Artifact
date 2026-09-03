module;

#include <numeric>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapeInputController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.Geometry;
import Artifact.Widgets.LayerEditor.ShapeEditSession;

namespace Artifact {
namespace {

void toggleAll(std::vector<int>& selection, int count)
{
 if (static_cast<int>(selection.size()) == count) {
  selection.clear();
  return;
 }
 selection.resize(static_cast<size_t>(count));
 std::iota(selection.begin(), selection.end(), 0);
}

}

LayerEditorShapeKeyResult LayerEditorShapeInputController::handle(
    LayerEditorShapeKeyAction action,
    const ArtifactAbstractLayerPtr& layer,
    std::vector<int>& polygonSelection,
    std::vector<int>& pathSelection,
    LayerEditorShapeEditSession& editSession) const
{
 if (!layer) return LayerEditorShapeKeyResult::Ignored;
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape) return LayerEditorShapeKeyResult::Ignored;

 if (action == LayerEditorShapeKeyAction::ToggleSelectAll) {
  if (shape->hasCustomPath()) {
   toggleAll(pathSelection, static_cast<int>(shape->customPathVertices().size()));
   polygonSelection.clear();
  } else {
   toggleAll(polygonSelection, static_cast<int>(shape->customPolygonPoints().size()));
   pathSelection.clear();
  }
  return LayerEditorShapeKeyResult::SelectionChanged;
 }

 if (!layer->isVisible() || layer->isLocked())
  return LayerEditorShapeKeyResult::Ignored;

 if (action == LayerEditorShapeKeyAction::Extrude) {
  if (shape->hasCustomPath()) {
   if (pathSelection.empty()) return LayerEditorShapeKeyResult::Ignored;
   auto vertices = shape->customPathVertices();
   const int source = pathSelection.back();
   if (source < 0 || source >= static_cast<int>(vertices.size()))
    return LayerEditorShapeKeyResult::Ignored;
   editSession.beginPath(layer);
   const int inserted = extrudePathVertex(vertices, source);
   shape->setCustomPathVertices(vertices, shape->customPathClosed());
   pathSelection = {inserted};
   polygonSelection.clear();
   editSession.markPathDirty();
   return LayerEditorShapeKeyResult::ExtrudedPath;
  }
  if (polygonSelection.empty()) return LayerEditorShapeKeyResult::Ignored;
  auto points = shape->customPolygonPoints();
  const int source = polygonSelection.back();
  if (source < 0 || source >= static_cast<int>(points.size()))
   return LayerEditorShapeKeyResult::Ignored;
  editSession.beginPolygon(layer);
  const int inserted = extrudePolygonVertex(points, source);
  shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
  polygonSelection = {inserted};
  pathSelection.clear();
  editSession.markPolygonDirty();
  return LayerEditorShapeKeyResult::ExtrudedPolygon;
 }

 if (action == LayerEditorShapeKeyAction::ToggleClosed) {
  if (shape->hasCustomPath()) {
   editSession.beginPath(layer);
   shape->setCustomPathVertices(shape->customPathVertices(),
                                !shape->customPathClosed());
   editSession.markPathDirty();
   editSession.commitPath();
  } else {
   editSession.beginPolygon(layer);
   shape->setCustomPolygonPoints(shape->customPolygonPoints(),
                                 !shape->customPolygonClosed());
   editSession.markPolygonDirty();
   editSession.commitPolygon();
  }
  return LayerEditorShapeKeyResult::GeometryChanged;
 }

 return LayerEditorShapeKeyResult::Ignored;
}

}
