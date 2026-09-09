module;

#include <QPointF>
#include <vector>

export module Artifact.Widgets.LayerEditor.MaskOverlay;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Render.IRenderer;

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
};

void drawLayerEditorMaskOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorMaskOverlayState& state);

}
