module;

#include <QSize>

class QWidget;

export module Artifact.Widgets.LayerEditor.FrameViewState;

import Artifact.Render.IRenderer;

export namespace Artifact {

struct LayerEditorFrameViewState {
 float zoom = 1.0f;
 float panX = 0.0f;
 float panY = 0.0f;
};

QSize layerEditorPhysicalViewportSize(const QWidget* widget);
LayerEditorFrameViewState beginLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    const QSize& viewportSize);
void restoreLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameViewState& state);

}
