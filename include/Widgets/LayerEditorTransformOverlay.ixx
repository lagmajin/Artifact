module;

#include <QRectF>
#include <QSize>

export module Artifact.Widgets.LayerEditor.TransformOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

void drawLayerEditorTransformHud(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const QRectF& activeBounds, const QSize& viewportSize,
    const QSize& restoreCanvasSize);

}
