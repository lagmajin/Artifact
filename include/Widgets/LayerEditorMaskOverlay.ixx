module;

#include <QPointF>
#include <vector>

export module Artifact.Widgets.LayerEditor.MaskOverlay;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.Geometry;

export namespace Artifact {

struct LayerEditorMaskOverlayState {
 bool draggingVertex = false;
 bool draggingHandle = false;
 int draggingMask = -1;
 int draggingPath = -1;
 int draggingVertexIndex = -1;
 int draggingHandleType = -1;
 int hoveredMask = -1;
 int hoveredPath = -1;
 int hoveredVertex = -1;
 int hoveredHandleType = -1;
 const std::vector<MaskVertexAddress>* selectedVertices = nullptr;
 bool rubberBandSelecting = false;
 QPointF rubberBandStart;
 QPointF rubberBandCurrent;
 // ==== マスク全体 (bounding box) ギズモ ====
 // boundsIndex < 0 は bounds ギズモ非表示（既存の頂点編集のみ）。
 int boundsMaskIndex = -1;
 // 選択中マスクの bounding box 上で掴んだハンドル種別。None は未操作。
 MaskBoundsHandle boundsHandle = MaskBoundsHandle::None;
 int hoveredBoundsHandle = -1;
};

void drawLayerEditorMaskOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorMaskOverlayState& state);

}
