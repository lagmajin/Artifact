module;

#include <QString>
#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

struct LayerEditorShapeOverlayState {
 bool draggingVertex = false;
 int draggingVertexIndex = -1;
 int hoveredVertexIndex = -1;
 int hoveredSegmentIndex = -1;
 std::vector<int> selectedVertexIndices;
 QString proportionalStatus;
 bool draggingPathVertex = false;
 bool draggingPathTangent = false;
 int hoveredPathVertexIndex = -1;
 int hoveredPathTangentIndex = -1;
 int hoveredPathTangentType = 0;
 std::vector<int> selectedPathVertexIndices;
};

void drawLayerEditorShapeOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorShapeOverlayState& state);

void drawLayerEditorCustomPathOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorShapeOverlayState& state);

void drawLayerEditorShapeParameterHandles(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    bool hoverCornerRadius, bool hoverStarInnerRadius);

}
