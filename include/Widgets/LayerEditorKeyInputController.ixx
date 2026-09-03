module;

#include <functional>
#include <vector>

export module Artifact.Widgets.LayerEditor.KeyInputController;

import Artifact.Layer.Abstract;
import Artifact.Widgets.LayerEditor.ModalTransformController;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeInputController;

export namespace Artifact {

enum class LayerEditorKeyCursor {
 Unchanged,
 Unset,
 Move,
 Cross,
 HorizontalResize
};

struct LayerEditorKeyInputState {
 int key = 0;
 bool controlModifier = false;
 bool maskEditing = false;
 bool shapeEditing = false;
 bool rendererAvailable = false;
 bool* proportionalEditingEnabled = nullptr;
 float* proportionalEditRadius = nullptr;
 ArtifactAbstractLayerPtr layer;
 std::vector<int>* selectedPolygonIndices = nullptr;
 std::vector<int>* selectedPathIndices = nullptr;
};

struct LayerEditorKeyInputCallbacks {
 std::function<bool(LayerEditorModalTransformMode)> beginModalTransform;
 std::function<void()> commitModalTransform;
 std::function<void()> cancelModalTransform;
 std::function<bool()> deleteMaskVertex;
 std::function<bool()> deleteShapeGeometry;
};

struct LayerEditorKeyInputResult {
 bool consumed = false;
 bool requestRender = false;
 LayerEditorKeyCursor cursor = LayerEditorKeyCursor::Unchanged;
};

class LayerEditorKeyInputController {
public:
 LayerEditorKeyInputResult handle(
     const LayerEditorKeyInputState& state,
     LayerEditorKeyInputCallbacks callbacks,
     LayerEditorModalTransformController& modalTransform,
     LayerEditorShapeInputController& shapeInput,
     LayerEditorShapeEditSession& shapeEditSession) const;
};

}
