module;

#include <QRectF>

export module Artifact.Widgets.LayerEditor.TransformOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Math.Vec;

export namespace Artifact {

void drawLayerEditorTransformHud(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const QRectF& activeBounds,
    ArtifactCore::Coordinates::ScreenPhysicalExtent2 viewportSize,
    ArtifactCore::Coordinates::CompositionExtent2 restoreCanvasSize);

}
