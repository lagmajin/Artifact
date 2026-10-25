module;
#include <algorithm>
#include <cmath>
#include <cstdint>

module Artifact.Layer.Text;
import Artifact.Composition.Abstract;
import Frame.Position;
import Frame.Rate;

namespace Artifact {
int64_t effectiveTextTimelineFrame(const ArtifactTextLayer *layer) {
  if (!layer) {
    return 0;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return composition->framePosition().framePosition();
  }
  return layer->currentFrame();
}

int64_t effectiveTextTimelineFps(const ArtifactTextLayer *layer) {
  if (!layer) {
    return 30;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    const double fps = composition->frameRate().framerate();
    return std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps)));
  }
  return 30;
}

} // namespace Artifact
