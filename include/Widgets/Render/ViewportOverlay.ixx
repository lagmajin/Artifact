module;

#include <QVector>

export module Artifact.Widgets.ViewportOverlay;

export namespace Artifact {
class ArtifactIRenderer;

namespace ViewportOverlay {
void drawSafeAreaAndOrigin(ArtifactIRenderer *renderer, float canvasWidth,
                           float canvasHeight, bool showSafeArea,
                           bool showOrigin);
void drawGuides(ArtifactIRenderer *renderer, const QVector<float> &verticals,
                const QVector<float> &horizontals, bool showGuides,
                float canvasWidth, float canvasHeight);
} // namespace ViewportOverlay
} // namespace Artifact
