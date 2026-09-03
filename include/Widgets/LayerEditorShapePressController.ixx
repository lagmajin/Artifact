module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapePressController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeEditSession;

export namespace Artifact {

enum class LayerEditorShapePressKind {
 None,
 SelectionOnly,
 DragPolygonVertex,
 DragPathVertex,
 DragPathTangent,
 GeometryChanged
};

struct LayerEditorShapePressHit {
 int polygonVertex = -1;
 int polygonSegment = -1;
 int pathVertex = -1;
 int pathTangentVertex = -1;
 int pathTangentType = 0;
};

struct LayerEditorShapePressResult {
 LayerEditorShapePressKind kind = LayerEditorShapePressKind::None;
 int vertexIndex = -1;
 int tangentType = 0;
 int hoveredPolygonVertex = -1;
 int hoveredPolygonSegment = -1;
 int hoveredPathVertex = -1;
 std::vector<QPointF> polygonDragBefore;
 std::vector<CustomPathVertex> pathDragBefore;
};

class LayerEditorShapePressController {
public:
 LayerEditorShapePressResult handle(
     const ArtifactAbstractLayerPtr& layer, ArtifactShapeLayer& shape,
     const QPointF& canvasPosition, const LayerEditorShapePressHit& hit,
     bool extendSelection,
     std::vector<int>& polygonSelection,
     std::vector<int>& pathSelection,
     LayerEditorShapeEditSession& editSession) const;
};

}
