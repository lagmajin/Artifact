module;

#include <QPointF>
#include <QRectF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskPressInteractionController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;
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
 std::vector<MaskVertexAddress>* selectedVertices = nullptr;
 bool additiveSelection = false;
 // ==== bounds ギズモ ====
 // boundsMaskIndex は選択中のマスク番号（< 0 なら bounds ギズモ無効）。
 int boundsMaskIndex = -1;
 // DragBounds 開始時に Press 側の判定結果を書き込む先。
 MaskBoundsHandle* boundsHandle = nullptr;
 QRectF* boundsBefore = nullptr;
 QPointF* boundsAnchor = nullptr;
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
