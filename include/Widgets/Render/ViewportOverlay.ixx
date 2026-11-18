module;

#include <QVector>

export module Artifact.Widgets.ViewportOverlay;

import Artifact.Render.IRenderer;

export namespace Artifact {
namespace ViewportOverlay {
void drawSafeAreaAndOrigin(ArtifactIRenderer *renderer, float canvasWidth,
                           float canvasHeight, bool showSafeArea,
                           bool showOrigin);
void drawGuides(ArtifactIRenderer *renderer, const QVector<float> &verticals,
                const QVector<float> &horizontals, bool showGuides,
                float canvasWidth, float canvasHeight);
void drawPixelGrid(ArtifactIRenderer *renderer, float canvasWidth,
                   float canvasHeight, float zoom, bool visible);
void drawCompositionBoundary(ArtifactIRenderer *renderer, float canvasWidth,
                             float canvasHeight, bool visible);
// P1-14: display-only aspect mask. aspectRatio <= 0 disables it.
void drawAspectMask(ArtifactIRenderer *renderer, float canvasWidth,
                    float canvasHeight, float aspectRatio);
} // namespace ViewportOverlay
} // namespace Artifact
