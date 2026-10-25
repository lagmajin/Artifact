module;

#include <algorithm>
#include <cmath>
#include <cstdint>

module Artifact.Layer.Shape;

import Artifact.Composition.Abstract;
import Frame.Position;
import Frame.Rate;
import Time.Rational;

namespace Artifact {

std::int64_t shapeLayerEffectiveFrameRate(const ArtifactShapeLayer* layer)
{
    if (layer) {
        if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
                layer->compositionObject())) {
            const double rawFps = composition->frameRate().framerate();
            return std::isfinite(rawFps) && rawFps > 0.0
                ? std::max<std::int64_t>(
                    1, static_cast<std::int64_t>(std::llround(
                        std::clamp(rawFps, 1.0, 10000.0))))
                : 30;
        }
    }
    return 30;
}

ArtifactCore::RationalTime shapeLayerTimelineTime(
    const ArtifactShapeLayer* layer)
{
    std::int64_t frame = 0;
    if (layer) {
        if (auto* composition = dynamic_cast<ArtifactAbstractComposition*>(
                layer->compositionObject())) {
            frame = composition->framePosition().framePosition();
        } else {
            frame = layer->currentFrame();
        }
    }
    return ArtifactCore::RationalTime(
        frame, shapeLayerEffectiveFrameRate(layer));
}

} // namespace Artifact
