module;

#include <QPointF>

#include <algorithm>
#include <vector>

module Artifact.Widgets.LayerEditor.ShapeDeleteController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Memory.SharedPtr;

namespace Artifact {

LayerEditorShapeDeleteResult LayerEditorShapeDeleteController::handle(
    const ArtifactAbstractLayerPtr& layer,
    LayerEditorShapeHoverController& hoverController,
    LayerEditorShapeEditSession& editSession) const
{
 if (!layer) return {};
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || !shape->hasCustomPolygon()) return {};

 const auto hover = hoverController.state();
 auto points = shape->customPolygonPoints();
 const bool closed = shape->customPolygonClosed();
 if (hover.polygonVertex >= 0 &&
     hover.polygonVertex < static_cast<int>(points.size())) {
  editSession.beginPolygon(layer);
  points.erase(points.begin() + hover.polygonVertex);
  if (points.size() >= 3) shape->setCustomPolygonPoints(points, closed);
  else shape->clearCustomPolygonPoints();
  editSession.markPolygonDirty();
  hoverController.setPolygon(-1, -1);
  return {true, false, -1};
 }

 if (hover.polygonSegment < 0 || points.size() < 2) return {};
 const int segmentCount = closed ? static_cast<int>(points.size())
                                 : static_cast<int>(points.size()) - 1;
 if (segmentCount <= 0) return {};
 const int segment = std::clamp(hover.polygonSegment, 0, segmentCount - 1);
 const int next = closed
     ? (segment + 1) % static_cast<int>(points.size()) : segment + 1;
 if (next < 0 || next >= static_cast<int>(points.size())) return {};
 const QPointF midpoint =
     (points[static_cast<size_t>(segment)] +
      points[static_cast<size_t>(next)]) * 0.5;
 editSession.beginPolygon(layer);
 const int inserted = std::clamp(
     segment + 1, 0, static_cast<int>(points.size()));
 points.insert(points.begin() + inserted, midpoint);
 shape->setCustomPolygonPoints(points, closed);
 editSession.markPolygonDirty();
 hoverController.setPolygon(inserted, inserted - 1);
 return {true, true, inserted};
}

}
