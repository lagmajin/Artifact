module;

#include <QPointF>

#include <functional>
#include <vector>

export module Artifact.Widgets.LayerEditor.ReleaseController;

export namespace Artifact {

struct LayerEditorReleaseState {
 int button = 0;
 bool modalActive = false;
 bool maskEditing = false;
 bool shapeEditing = false;
 bool draggingMaskHandle = false;
 bool draggingMaskVertex = false;
 bool parameterActive = false;
 bool draggingPathVertex = false;
 bool draggingPathTangent = false;
 bool pathEditPending = false;
 bool draggingPolygonVertex = false;
 bool polygonEditPending = false;
 bool gizmoDragging = false;
 bool* maskHandleDragState = nullptr;
 bool* maskVertexDragState = nullptr;
 bool* pathVertexDragState = nullptr;
 bool* pathTangentDragState = nullptr;
 bool* polygonVertexDragState = nullptr;
 int* maskIndex = nullptr;
 int* maskPathIndex = nullptr;
 int* maskVertexIndex = nullptr;
 int* maskHandleType = nullptr;
 int* pathVertexIndex = nullptr;
 int* polygonVertexIndex = nullptr;
 std::vector<QPointF>* selectedPolygonBefore = nullptr;
};

struct LayerEditorReleaseCallbacks {
 std::function<void()> commitModal;
 std::function<void()> cancelModal;
 std::function<void()> resetProportionalState;
 std::function<void()> commitMaskEdit;
 std::function<void()> commitPathEdit;
 std::function<void()> commitPolygonEdit;
 std::function<void()> commitParameterEdit;
 std::function<void()> releaseGizmo;
};

struct LayerEditorReleaseResult {
 bool consumed = false;
 bool requestRender = false;
 bool unsetCursor = false;
};

class LayerEditorReleaseController {
public:
 LayerEditorReleaseResult handle(
     LayerEditorReleaseState state,
     const LayerEditorReleaseCallbacks& callbacks) const;
};

}
