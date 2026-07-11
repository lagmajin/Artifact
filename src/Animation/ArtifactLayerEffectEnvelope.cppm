module;
#include <algorithm>
#include <cmath>
module Artifact.Animation.LayerEffectEnvelope;

namespace Artifact {

LayerEnvelopeSample LayerEffectEnvelope::sample(const std::int64_t relativeFrame) const {
  return sample(relativeFrame, false);
}

LayerEnvelopeSample LayerEffectEnvelope::sample(const std::int64_t relativeFrame,
                                                const bool reverse) const {
  if (!enabled || durationFrames <= 0) {
    return {1.0f, effectEnd};
  }

  float t = std::clamp(static_cast<float>(relativeFrame) /
                           static_cast<float>(durationFrames),
                       0.0f, 1.0f);
  if (reverse) {
    t = 1.0f - t;
  }
  switch (curve) {
  case LayerEnvelopeCurve::EaseIn: t *= t; break;
  case LayerEnvelopeCurve::EaseOut: t = 1.0f - (1.0f - t) * (1.0f - t); break;
  case LayerEnvelopeCurve::EaseInOut:
    t = t < 0.5f ? 2.0f * t * t
                 : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    break;
  case LayerEnvelopeCurve::Step: t = t >= 1.0f ? 1.0f : 0.0f; break;
  case LayerEnvelopeCurve::Linear: break;
  }
  float opacityT = t;
  float effectT = t;
  switch (timing) {
  case LayerEnvelopeTiming::OpacityLead:
    effectT = std::clamp(t - 0.2f, 0.0f, 1.0f);
    break;
  case LayerEnvelopeTiming::EffectLead:
    opacityT = std::clamp(t - 0.2f, 0.0f, 1.0f);
    break;
  case LayerEnvelopeTiming::Simultaneous:
    break;
  }

  return {opacityT, effectStart + (effectEnd - effectStart) * effectT};
}

}
