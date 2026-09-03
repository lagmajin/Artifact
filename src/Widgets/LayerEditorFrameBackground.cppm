module;

#include <algorithm>

module Artifact.Widgets.LayerEditor.FrameBackground;

import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ContextMenu;

namespace Artifact {

void drawLayerEditorFrameBackground(
    ArtifactIRenderer& renderer,
    const LayerEditorFrameBackgroundState& state)
{
 const float viewportWidth = static_cast<float>(
     std::max(1, state.viewportSize.width()));
 const float viewportHeight = static_cast<float>(
     std::max(1, state.viewportSize.height()));
 if (state.mode == LayerEditorBackgroundMode::Alpha) {
  renderer.drawCheckerboard(0.0f, 0.0f, viewportWidth, viewportHeight, 56.0f,
                            FloatColor(0.33f, 0.34f, 0.35f, 1.0f),
                            FloatColor(0.26f, 0.27f, 0.28f, 1.0f));
 } else if (state.mode == LayerEditorBackgroundMode::MayaGradient) {
  if (state.mayaGradientSprite && !state.mayaGradientSprite->isNull()) {
   renderer.drawSprite(0.0f, 0.0f, viewportWidth, viewportHeight,
                       *state.mayaGradientSprite, 1.0f);
  }
 } else {
  renderer.drawRectLocal(
      0.0f, 0.0f, viewportWidth, viewportHeight, state.clearColor);
 }
}

}
