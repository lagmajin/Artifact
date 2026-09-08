module;
#include <algorithm>
#include <cmath>
#include <cstdint>

export module Artifact.Layer.Abstract.Utilities;

export namespace Artifact::LayerAbstractUtilities {

float finiteClampedValue(const double raw, const double fallback,
                         const double minimum, const double maximum) {
  const double safeFallback = std::isfinite(fallback)
                                  ? std::clamp(fallback, minimum, maximum)
                                  : minimum;
  return static_cast<float>(std::isfinite(raw)
                                ? std::clamp(raw, minimum, maximum)
                                : safeFallback);
}

} // namespace Artifact::LayerAbstractUtilities
