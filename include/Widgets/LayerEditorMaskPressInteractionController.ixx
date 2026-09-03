module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskPressInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.MaskHoverController;

export namespace Artifact {

struct LayerEditorMaskPressInteractionState {
 bool proportionalEditingEnabled = false;
 bool* draggingVertex = nullptr;
 bool* draggingHandle = nullptr;
 int* draggingMaskIndex = nullptr;
 int* draggingPathIndex = nullptr;
 int* draggingVertexIndex = nullptr;
 int* draggingHandleType = nullptr;
 bool* proportionalDragActive = nullptr;
 QPointF* proportionalDragOrigin = nullptr;
 std::vector<MaskVertex>* proportionalMaskBefore = nullptr;
 std::vector<QPointF>* proportionalPolygonBefore = nullptr;
 std::vector<CustomPathVertex>* proportionalPathBefore = nullptr;
};

struct LayerEditorMaskPressInteractionResult {
 bool consumed = false;
 bool requestRender = false;
 bool useMoveCursor = false;
};

class LayerEditorMaskPressInteractionController {
public:
 LayerEditorMaskPressInteractionResult handle(
     const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
     float zoom, LayerEditorMaskPressInteractionState state,
     LayerEditorMaskHoverController& hoverController,
     LayerEditorMaskEditSession& editSession) const;
};

}
