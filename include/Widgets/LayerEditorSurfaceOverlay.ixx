module;

#include <vector>
#include <QSize>

export module Artifact.Widgets.LayerEditor.SurfaceOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

struct LayerEditorImpactLayers {
 std::vector<ArtifactAbstractLayerPtr> parents;
 std::vector<ArtifactAbstractLayerPtr> children;
 std::vector<ArtifactAbstractLayerPtr> mattes;
 std::vector<ArtifactAbstractLayerPtr> dependents;
};

void drawLayerEditorSurfaceOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    bool impactMode, const LayerEditorImpactLayers& impactLayers);

void drawLayerEditorCompositionGuides(
    ArtifactIRenderer* renderer, const QSize& compositionSize,
    bool showGrid, bool showSafeMargins);

}
