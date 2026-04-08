module;
#include <utility>

export module Artifact.Render.CompositionRenderer;

import Color.Float;
import Artifact.Render.IRenderer;

export namespace Artifact {

class CompositionRenderer {
public:
 explicit CompositionRenderer(ArtifactIRenderer& renderer);

 void SetCompositionSize(float width, float height);
 void ApplyCompositionSpace();

 void DrawCompositionRect(float x, float y, float w, float h, const ArtifactCore::FloatColor& color, float opacity = 1.0f);
 void DrawCompositionBackground(const ArtifactCore::FloatColor& color, float opacity = 1.0f);

private:
 ArtifactIRenderer* renderer_ = nullptr;
 float compositionWidth_ = 1920.0f;
 float compositionHeight_ = 1080.0f;
};

}
