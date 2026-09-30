module;

#include <compare>
#include <QImage>

export module Artifact.Widgets.LayerEditor.FrameBackground;

import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ContextMenu;
import Color.Float;
import Math.Vec;

export namespace Artifact {

struct LayerEditorFrameBackgroundState {
 ArtifactCore::Coordinates::ScreenPhysicalExtent2 viewportSize;
 LayerEditorBackgroundMode mode = LayerEditorBackgroundMode::Alpha;
 const QImage* mayaGradientSprite = nullptr;
 ArtifactCore::FloatColor clearColor;
};

void drawLayerEditorFrameBackground(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameBackgroundState& state);

}
