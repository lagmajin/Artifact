module;

#include <algorithm>
#include <cmath>

#include <QWidget>

module Artifact.Widgets.LayerEditor.FrameViewState;

import Artifact.Render.IRenderer;

namespace Artifact {

QSize layerEditorPhysicalViewportSize(const QWidget* widget)
{
 if (!widget) return {};
 const qreal dpr = widget->devicePixelRatio();
 return QSize(
     std::max(1, static_cast<int>(std::lround(widget->width() * dpr))),
     std::max(1, static_cast<int>(std::lround(widget->height() * dpr))));
}

LayerEditorFrameViewState beginLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    const QSize& viewportSize)
{
 LayerEditorFrameViewState state;
 state.zoom = renderer.getZoom();
 renderer.getPan(state.panX, state.panY);
 const float viewportWidth = static_cast<float>(
     std::max(1, viewportSize.width()));
 const float viewportHeight = static_cast<float>(
     std::max(1, viewportSize.height()));
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
 renderer.setZoom(state.zoom);
 renderer.setPan(state.panX, state.panY);
}

}
