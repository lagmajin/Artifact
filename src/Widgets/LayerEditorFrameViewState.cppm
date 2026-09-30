module;


#include <QWidget>

module Artifact.Widgets.LayerEditor.FrameViewState;

import Core.ArtifactMath;
import Artifact.Render.IRenderer;
import Math.Vec;

namespace Artifact {

QSize layerEditorPhysicalViewportSize(const QWidget* widget)
{
 if (!widget) return {};
 const qreal dpr = widget->devicePixelRatio();
 return QSize(
     ArtifactCore::artifactMax(1, static_cast<int>(ArtifactCore::artifactLround(widget->width() * dpr))),
     ArtifactCore::artifactMax(1, static_cast<int>(ArtifactCore::artifactLround(widget->height() * dpr))));
}

LayerEditorFrameViewState beginLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    ArtifactCore::Coordinates::ScreenPhysicalExtent2 viewportSize)
{
 LayerEditorFrameViewState state;
 state.zoom = {renderer.getZoom()};
 float panX = 0.0f;
 float panY = 0.0f;
 renderer.getPan(panX, panY);
 state.pan = {panX, panY};
 const float viewportWidth = static_cast<float>(
     ArtifactCore::artifactMax(1.0f, viewportSize.width));
 const float viewportHeight = static_cast<float>(
     ArtifactCore::artifactMax(1.0f, viewportSize.height));
 renderer.setViewportSize(viewportWidth, viewportHeight);
 renderer.setCanvasSize(viewportWidth, viewportHeight);
 renderer.setZoom(1.0f);
 renderer.setPan(0.0f, 0.0f);
 renderer.setUseExternalMatrices(false);
 renderer.resetGizmoCameraMatrices();
 renderer.reset3DCameraMatrices();
 return state;
}

void restoreLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameViewState& state)
{
 renderer.setZoom(state.zoom.value);
 renderer.setPan(state.pan.x, state.pan.y);
}

}
