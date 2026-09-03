module;

module Artifact.Widgets.LayerEditor.InteractionStateController;

import Artifact.Widgets.LayerEditor.MaskHoverController;
import Artifact.Widgets.LayerEditor.MaskMoveController;
import Artifact.Widgets.LayerEditor.MaskPressInteractionController;
import Artifact.Widgets.LayerEditor.ReleaseController;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Artifact.Widgets.LayerEditor.ShapeMoveController;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.LayerEditor.ShapePressInteractionController;

namespace Artifact {

void LayerEditorInteractionStateController::bind(
    LayerEditorInteractionStateBindings bindings) noexcept
{
 bindings_ = bindings;
}

void LayerEditorInteractionStateController::resetProportionalState()
{
 if (bindings_.proportionalDragActive) *bindings_.proportionalDragActive = false;
 if (bindings_.proportionalDragOrigin) *bindings_.proportionalDragOrigin = {};
 if (bindings_.proportionalMaskBefore) bindings_.proportionalMaskBefore->clear();
 if (bindings_.proportionalPolygonBefore) bindings_.proportionalPolygonBefore->clear();
 if (bindings_.proportionalPathBefore) bindings_.proportionalPathBefore->clear();
}

void LayerEditorInteractionStateController::resetForClearTarget()
{
 if (bindings_.draggingPolygonVertex) *bindings_.draggingPolygonVertex = false;
 if (bindings_.draggingPolygonVertexIndex) *bindings_.draggingPolygonVertexIndex = -1;
 if (bindings_.shapeHover) bindings_.shapeHover->clear();
 if (bindings_.shapeParameter) {
  bindings_.shapeParameter->cancel();
  bindings_.shapeParameter->clearHover();
 }
 resetProportionalState();
 if (bindings_.draggingPathVertex) *bindings_.draggingPathVertex = false;
 if (bindings_.draggingPathTangent) *bindings_.draggingPathTangent = false;
 if (bindings_.draggingPathVertexIndex) *bindings_.draggingPathVertexIndex = -1;
 if (bindings_.selectedPathBefore) bindings_.selectedPathBefore->clear();
}

void LayerEditorInteractionStateController::resetForTargetChange()
{
 if (bindings_.draggingPolygonVertex) *bindings_.draggingPolygonVertex = false;
 if (bindings_.draggingPolygonVertexIndex) *bindings_.draggingPolygonVertexIndex = -1;
 if (bindings_.shapeHover) bindings_.shapeHover->clear();
 if (bindings_.selectedPolygonIndices) bindings_.selectedPolygonIndices->clear();
 if (bindings_.selectedPolygonBefore) bindings_.selectedPolygonBefore->clear();
 if (bindings_.selectedPathIndices) bindings_.selectedPathIndices->clear();
 if (bindings_.selectedPathBefore) bindings_.selectedPathBefore->clear();
}

void LayerEditorInteractionStateController::resetForNonMaskMode()
{
 if (bindings_.draggingMaskVertex) *bindings_.draggingMaskVertex = false;
 if (bindings_.draggingMaskIndex) *bindings_.draggingMaskIndex = -1;
 if (bindings_.draggingMaskPathIndex) *bindings_.draggingMaskPathIndex = -1;
 if (bindings_.draggingMaskVertexIndex) *bindings_.draggingMaskVertexIndex = -1;
 if (bindings_.draggingMaskHandle) *bindings_.draggingMaskHandle = false;
 if (bindings_.draggingMaskHandleType) *bindings_.draggingMaskHandleType = -1;
 if (bindings_.draggingPolygonVertex) *bindings_.draggingPolygonVertex = false;
 if (bindings_.draggingPolygonVertexIndex) *bindings_.draggingPolygonVertexIndex = -1;
 if (bindings_.shapeHover) bindings_.shapeHover->clear();
 if (bindings_.selectedPolygonIndices) bindings_.selectedPolygonIndices->clear();
 if (bindings_.selectedPolygonBefore) bindings_.selectedPolygonBefore->clear();
 if (bindings_.selectedPathIndices) bindings_.selectedPathIndices->clear();
 if (bindings_.selectedPathBefore) bindings_.selectedPathBefore->clear();
 if (bindings_.maskHover) bindings_.maskHover->clear();
}

LayerEditorShapePressInteractionState
LayerEditorInteractionStateController::shapePressState(
    bool proportionalEditingEnabled) const
{
 return {
     .proportionalEditingEnabled = proportionalEditingEnabled,
     .proportionalDragActive = bindings_.proportionalDragActive,
     .proportionalDragOrigin = bindings_.proportionalDragOrigin,
     .proportionalMaskBefore = bindings_.proportionalMaskBefore,
     .proportionalPolygonBefore = bindings_.proportionalPolygonBefore,
     .proportionalPathBefore = bindings_.proportionalPathBefore,
     .draggingPolygonVertex = bindings_.draggingPolygonVertex,
     .draggingPathVertex = bindings_.draggingPathVertex,
     .draggingPathTangent = bindings_.draggingPathTangent,
     .draggingPolygonVertexIndex = bindings_.draggingPolygonVertexIndex,
     .draggingPathVertexIndex = bindings_.draggingPathVertexIndex,
     .draggingPathTangentType = bindings_.draggingPathTangentType,
     .selectedPolygonIndices = bindings_.selectedPolygonIndices,
     .selectedPolygonBefore = bindings_.selectedPolygonBefore,
     .selectedPathIndices = bindings_.selectedPathIndices,
     .selectedPathBefore = bindings_.selectedPathBefore};
}

LayerEditorMaskPressInteractionState
LayerEditorInteractionStateController::maskPressState(
    bool proportionalEditingEnabled) const
{
 return {
     .proportionalEditingEnabled = proportionalEditingEnabled,
     .draggingVertex = bindings_.draggingMaskVertex,
     .draggingHandle = bindings_.draggingMaskHandle,
     .draggingMaskIndex = bindings_.draggingMaskIndex,
     .draggingPathIndex = bindings_.draggingMaskPathIndex,
     .draggingVertexIndex = bindings_.draggingMaskVertexIndex,
     .draggingHandleType = bindings_.draggingMaskHandleType,
     .proportionalDragActive = bindings_.proportionalDragActive,
     .proportionalDragOrigin = bindings_.proportionalDragOrigin,
     .proportionalMaskBefore = bindings_.proportionalMaskBefore,
     .proportionalPolygonBefore = bindings_.proportionalPolygonBefore,
     .proportionalPathBefore = bindings_.proportionalPathBefore};
}

LayerEditorShapeMoveState LayerEditorInteractionStateController::shapeMoveState(
    bool proportionalEditingEnabled, float proportionalEditRadius) const
{
 return {
     .draggingPathVertex = bindings_.draggingPathVertex &&
         *bindings_.draggingPathVertex,
     .draggingPathTangent = bindings_.draggingPathTangent &&
         *bindings_.draggingPathTangent,
     .draggingPathVertexIndex = bindings_.draggingPathVertexIndex
         ? *bindings_.draggingPathVertexIndex : -1,
     .draggingPathTangentType = bindings_.draggingPathTangentType
         ? *bindings_.draggingPathTangentType : 0,
     .draggingPolygonVertexIndex = bindings_.draggingPolygonVertexIndex
         ? *bindings_.draggingPolygonVertexIndex : -1,
     .proportionalEditingEnabled = proportionalEditingEnabled,
     .proportionalDragActive = bindings_.proportionalDragActive &&
         *bindings_.proportionalDragActive,
     .proportionalEditRadius = proportionalEditRadius,
     .proportionalDragOrigin = bindings_.proportionalDragOrigin
         ? *bindings_.proportionalDragOrigin : QPointF{},
     .selectedPolygonIndices = bindings_.selectedPolygonIndices,
     .selectedPolygonBefore = bindings_.selectedPolygonBefore,
     .proportionalPolygonBefore = bindings_.proportionalPolygonBefore,
     .selectedPathIndices = bindings_.selectedPathIndices,
     .selectedPathBefore = bindings_.selectedPathBefore,
     .proportionalPathBefore = bindings_.proportionalPathBefore};
}

LayerEditorMaskMoveState LayerEditorInteractionStateController::maskMoveState(
    float proportionalEditRadius) const
{
 return {
     .draggingHandle = bindings_.draggingMaskHandle &&
         *bindings_.draggingMaskHandle,
     .draggingVertex = bindings_.draggingMaskVertex &&
         *bindings_.draggingMaskVertex,
     .maskIndex = bindings_.draggingMaskIndex ? *bindings_.draggingMaskIndex : -1,
     .pathIndex = bindings_.draggingMaskPathIndex
         ? *bindings_.draggingMaskPathIndex : -1,
     .vertexIndex = bindings_.draggingMaskVertexIndex
         ? *bindings_.draggingMaskVertexIndex : -1,
     .handleType = bindings_.draggingMaskHandleType
         ? static_cast<MaskHandleType>(*bindings_.draggingMaskHandleType)
         : MaskHandleType::None,
     .proportionalDragActive = bindings_.proportionalDragActive &&
         *bindings_.proportionalDragActive,
     .proportionalBefore = bindings_.proportionalMaskBefore,
     .proportionalOrigin = bindings_.proportionalDragOrigin
         ? *bindings_.proportionalDragOrigin : QPointF{},
     .proportionalRadius = proportionalEditRadius};
}

LayerEditorReleaseState LayerEditorInteractionStateController::releaseState(
    bool modalActive, bool maskEditing, bool shapeEditing,
    bool parameterActive, bool gizmoDragging) const
{
 return {
     .modalActive = modalActive,
     .maskEditing = maskEditing,
     .shapeEditing = shapeEditing,
     .draggingMaskHandle = bindings_.draggingMaskHandle &&
         *bindings_.draggingMaskHandle,
     .draggingMaskVertex = bindings_.draggingMaskVertex &&
         *bindings_.draggingMaskVertex,
     .parameterActive = parameterActive,
     .draggingPathVertex = bindings_.draggingPathVertex &&
         *bindings_.draggingPathVertex,
     .draggingPathTangent = bindings_.draggingPathTangent &&
         *bindings_.draggingPathTangent,
     .draggingPolygonVertex = bindings_.draggingPolygonVertex &&
         *bindings_.draggingPolygonVertex,
     .maskHandleDragState = bindings_.draggingMaskHandle,
     .maskVertexDragState = bindings_.draggingMaskVertex,
     .pathVertexDragState = bindings_.draggingPathVertex,
     .pathTangentDragState = bindings_.draggingPathTangent,
     .polygonVertexDragState = bindings_.draggingPolygonVertex,
     .maskIndex = bindings_.draggingMaskIndex,
     .maskPathIndex = bindings_.draggingMaskPathIndex,
     .maskVertexIndex = bindings_.draggingMaskVertexIndex,
     .maskHandleType = bindings_.draggingMaskHandleType,
     .pathVertexIndex = bindings_.draggingPathVertexIndex,
     .polygonVertexIndex = bindings_.draggingPolygonVertexIndex,
     .selectedPolygonBefore = bindings_.selectedPolygonBefore};
}

}
