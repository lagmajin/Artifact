module;

#include <QPointF>

#include <functional>

export module Artifact.Widgets.LayerEditor.ViewPressController;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.TransformGizmo;

export namespace Artifact {

enum class LayerEditorViewPressCursor {
 Unchanged,
 Pan,
 ParameterHorizontal,
 ParameterVertical,
 Gizmo
};

struct LayerEditorViewPressState {
 int button = 0;
 bool altModifier = false;
 QPointF viewportPosition;
 bool transformViewEnabled = false;
 ArtifactAbstractLayerPtr layer;
 bool* panning = nullptr;
 QPointF* lastMousePosition = nullptr;
};

struct LayerEditorViewPressCallbacks {
 std::function<bool(const QPointF&)> pressViewportChrome;
 std::function<void()> clearViewportChromeHover;
};

struct LayerEditorViewPressResult {
 bool consumed = false;
 bool requestRender = false;
 LayerEditorViewPressCursor cursor = LayerEditorViewPressCursor::Unchanged;
 TransformGizmo::HandleType gizmoHandle = TransformGizmo::HandleType::None;
};

class LayerEditorViewPressController {
public:
 LayerEditorViewPressResult handle(
     const LayerEditorViewPressState& state,
     const LayerEditorViewPressCallbacks& callbacks,
     ArtifactIRenderer* renderer,
     LayerEditorShapeParameterController& parameterController,
     TransformGizmo* transformGizmo) const;
};

}
