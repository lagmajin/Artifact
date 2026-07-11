module;
#include <cstdint>
#include <wobjectdefs.h>
export module Artifact.Animation.LayerEffectEnvelope;

export namespace Artifact {

enum class LayerEnvelopeTiming : std::uint8_t {
  Simultaneous,
  OpacityLead,
  EffectLead
};

enum class LayerEnvelopeCurve : std::uint8_t {
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Step
};

struct LayerEnvelopeSample {
  float opacity = 1.0f;
  float effectStrength = 0.0f;
};

struct LayerEffectEnvelope {
  bool enabled = false;
  bool entry = false;
  bool exit = false;
  LayerEnvelopeTiming timing = LayerEnvelopeTiming::Simultaneous;
  LayerEnvelopeCurve curve = LayerEnvelopeCurve::Linear;
  std::int64_t durationFrames = 8;
  float effectStart = 0.0f;
  float effectEnd = 1.0f;

  LayerEnvelopeSample sample(std::int64_t relativeFrame) const;
  LayerEnvelopeSample sample(std::int64_t relativeFrame, bool reverse) const;
};

}
