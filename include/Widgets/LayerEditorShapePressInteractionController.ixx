module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.ShapePressInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;

export namespace Artifact {

struct LayerEditorShapePressInteractionState {
 bool proportionalEditingEnabled = false;
 bool* proportionalDragActive = nullptr;
 QPointF* proportionalDragOrigin = nullptr;
 std::vector<MaskVertex>* proportionalMaskBefore = nullptr;
 std::vector<QPointF>* proportionalPolygonBefore = nullptr;
 std::vector<CustomPathVertex>* proportionalPathBefore = nullptr;
 bool* draggingPolygonVertex = nullptr;
 bool* draggingPathVertex = nullptr;
 bool* draggingPathTangent = nullptr;
 int* draggingPolygonVertexIndex = nullptr;
 int* draggingPathVertexIndex = nullptr;
 int* draggingPathTangentType = nullptr;
 std::vector<int>* selectedPolygonIndices = nullptr;
 std::vector<QPointF>* selectedPolygonBefore = nullptr;
 std::vector<int>* selectedPathIndices = nullptr;
 std::vector<CustomPathVertex>* selectedPathBefore = nullptr;
};

struct LayerEditorShapePressInteractionResult {
 bool consumed = false;
 bool requestRender = false;
 bool useMoveCursor = false;
};

class LayerEditorShapePressInteractionController {
public:
 LayerEditorShapePressInteractionResult handle(
     const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
     float zoom, bool extendSelection,
     LayerEditorShapePressInteractionState state,
     LayerEditorShapeHoverController& hoverController,
     LayerEditorShapeEditSession& editSession) const;
};

}
