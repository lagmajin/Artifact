module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskMoveController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;
import Artifact.Widgets.LayerEditor.MaskDragController;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.MaskHoverController;

export namespace Artifact {

struct LayerEditorMaskMoveState {
 bool draggingHandle = false;
 bool draggingVertex = false;
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 MaskHandleType handleType = MaskHandleType::None;
 bool proportionalDragActive = false;
 const std::vector<MaskVertex>* proportionalBefore = nullptr;
 QPointF proportionalOrigin;
 float proportionalRadius = 96.0f;
};

enum class LayerEditorMaskMoveKind {
 None,
 GeometryChanged,
 HoverChanged
};

struct LayerEditorMaskMoveResult {
 LayerEditorMaskMoveKind kind = LayerEditorMaskMoveKind::None;
 bool vertexHovered = false;
 bool cursorRelevant = false;
};

class LayerEditorMaskMoveController {
public:
 LayerEditorMaskMoveResult handle(
     const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
     float zoom, const LayerEditorMaskMoveState& state,
     LayerEditorMaskDragController& dragController,
     LayerEditorMaskHoverController& hoverController,
     LayerEditorMaskEditSession& editSession) const;
};

}
