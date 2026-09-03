module;

#include <QPointF>

export module Artifact.Widgets.LayerEditor.ViewMoveController;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ModalTransformController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.TransformGizmo;

export namespace Artifact {

enum class LayerEditorViewMoveCursor { Unchanged, Unset, Gizmo };

struct LayerEditorViewMoveState {
 QPointF viewportPosition;
 bool precision = false;
 bool snap = false;
 bool transformViewEnabled = false;
 ArtifactAbstractLayerPtr layer;
};

struct LayerEditorViewMoveResult {
 bool consumed = false;
 bool requestRender = false;
 LayerEditorViewMoveCursor cursor = LayerEditorViewMoveCursor::Unchanged;
 TransformGizmo::HandleType gizmoHandle = TransformGizmo::HandleType::None;
 bool gizmoDragging = false;
};

class LayerEditorViewMoveController {
public:
 LayerEditorViewMoveResult handle(
     const LayerEditorViewMoveState& state, ArtifactIRenderer& renderer,
     LayerEditorModalTransformController& modalTransform,
     LayerEditorShapeEditSession& shapeEditSession,
     LayerEditorShapeParameterController& parameterController,
     TransformGizmo* transformGizmo) const;
};

}
