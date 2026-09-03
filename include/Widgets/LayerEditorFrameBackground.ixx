module;

#include <QImage>
#include <QSize>

export module Artifact.Widgets.LayerEditor.FrameBackground;

import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ContextMenu;
import Color.Float;

export namespace Artifact {

struct LayerEditorFrameBackgroundState {
 QSize viewportSize;
 LayerEditorBackgroundMode mode = LayerEditorBackgroundMode::Alpha;
 const QImage* mayaGradientSprite = nullptr;
 ArtifactCore::FloatColor clearColor;
};

void drawLayerEditorFrameBackground(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameBackgroundState& state);

}
