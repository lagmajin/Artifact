module;

#include <QSize>

class QWidget;

export module Artifact.Widgets.LayerEditor.FrameViewState;

import Artifact.Render.IRenderer;
import Math.Vec;

export namespace Artifact {

struct LayerEditorFrameViewState {
 ArtifactCore::Units::ScaleFactor zoom{};
 ArtifactCore::Coordinates::ScreenPhysicalVector2 pan{};
};

QSize layerEditorPhysicalViewportSize(const QWidget* widget);
LayerEditorFrameViewState beginLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    ArtifactCore::Coordinates::ScreenPhysicalExtent2 viewportSize);
void restoreLayerEditorFrameView(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameViewState& state);

}
