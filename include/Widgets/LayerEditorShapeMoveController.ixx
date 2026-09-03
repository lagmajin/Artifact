module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeMoveController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.ShapeDragController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;

export namespace Artifact {

struct LayerEditorShapeMoveState {
 bool draggingPathVertex = false;
 bool draggingPathTangent = false;
 int draggingPathVertexIndex = -1;
 int draggingPathTangentType = 0;
 int draggingPolygonVertexIndex = -1;
 bool proportionalEditingEnabled = false;
 bool proportionalDragActive = false;
 float proportionalEditRadius = 96.0f;
 QPointF proportionalDragOrigin;
 const std::vector<int>* selectedPolygonIndices = nullptr;
 const std::vector<QPointF>* selectedPolygonBefore = nullptr;
 const std::vector<QPointF>* proportionalPolygonBefore = nullptr;
 const std::vector<int>* selectedPathIndices = nullptr;
 const std::vector<CustomPathVertex>* selectedPathBefore = nullptr;
 const std::vector<CustomPathVertex>* proportionalPathBefore = nullptr;
};

enum class LayerEditorShapeMoveKind {
 Ignored,
 PathConsumed,
 GeometryChanged,
 HoverChanged
};

struct LayerEditorShapeMoveResult {
 LayerEditorShapeMoveKind kind = LayerEditorShapeMoveKind::Ignored;
 bool polygonHandleHovered = false;
 bool pathConsumed = false;
 bool polygonCursorRelevant = false;
};

class LayerEditorShapeMoveController {
public:
 LayerEditorShapeMoveResult handle(
     const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
     float zoom, bool independentTangent,
     const LayerEditorShapeMoveState& state,
     LayerEditorShapeDragController& dragController,
     LayerEditorShapeHoverController& hoverController,
     LayerEditorShapeEditSession& editSession) const;
};

}
