module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.InteractionStateController;

import Artifact.Layer.Shape;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.MaskHoverController;
import Artifact.Widgets.LayerEditor.MaskMoveController;
import Artifact.Widgets.LayerEditor.MaskPressInteractionController;
import Artifact.Widgets.LayerEditor.ReleaseController;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Artifact.Widgets.LayerEditor.ShapeMoveController;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.LayerEditor.ShapePressInteractionController;

export namespace Artifact {

struct LayerEditorInteractionStateBindings {
 bool* draggingMaskVertex = nullptr;
 bool* draggingMaskHandle = nullptr;
 int* draggingMaskIndex = nullptr;
 int* draggingMaskPathIndex = nullptr;
 int* draggingMaskVertexIndex = nullptr;
 int* draggingMaskHandleType = nullptr;
 bool* draggingPolygonVertex = nullptr;
 int* draggingPolygonVertexIndex = nullptr;
 bool* draggingPathVertex = nullptr;
 bool* draggingPathTangent = nullptr;
 int* draggingPathVertexIndex = nullptr;
 int* draggingPathTangentType = nullptr;
 bool* proportionalDragActive = nullptr;
 QPointF* proportionalDragOrigin = nullptr;
 std::vector<MaskVertex>* proportionalMaskBefore = nullptr;
 std::vector<QPointF>* proportionalPolygonBefore = nullptr;
 std::vector<CustomPathVertex>* proportionalPathBefore = nullptr;
 std::vector<int>* selectedPolygonIndices = nullptr;
 std::vector<QPointF>* selectedPolygonBefore = nullptr;
 std::vector<int>* selectedPathIndices = nullptr;
 std::vector<CustomPathVertex>* selectedPathBefore = nullptr;
 LayerEditorMaskHoverController* maskHover = nullptr;
 LayerEditorShapeHoverController* shapeHover = nullptr;
 LayerEditorShapeParameterController* shapeParameter = nullptr;
};

class LayerEditorInteractionStateController {
public:
 void bind(LayerEditorInteractionStateBindings bindings) noexcept;
 void resetForClearTarget();
 void resetForTargetChange();
 void resetForNonMaskMode();
 LayerEditorShapePressInteractionState shapePressState(
     bool proportionalEditingEnabled) const;
 LayerEditorMaskPressInteractionState maskPressState(
     bool proportionalEditingEnabled) const;
 LayerEditorShapeMoveState shapeMoveState(
     bool proportionalEditingEnabled, float proportionalEditRadius) const;
 LayerEditorMaskMoveState maskMoveState(float proportionalEditRadius) const;
 LayerEditorReleaseState releaseState(
     bool modalActive, bool maskEditing, bool shapeEditing,
     bool parameterActive, bool gizmoDragging) const;

private:
 void resetProportionalState();
 LayerEditorInteractionStateBindings bindings_;
};

}
