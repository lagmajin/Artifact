module;

#include <algorithm>
#include <cmath>

module Artifact.FxStudio.Sequence;

namespace Artifact::FxStudio {

namespace {

float sampleCurve(const CueCurve curve, const float progress) {
  const float t = std::clamp(progress, 0.0f, 1.0f);
  switch (curve) {
  case CueCurve::Step:
    return t < 1.0f ? 1.0f : 0.0f;
  case CueCurve::EaseIn:
    return t * t;
  case CueCurve::EaseOut:
    return 1.0f - (1.0f - t) * (1.0f - t);
  case CueCurve::EaseInOut:
    return t < 0.5f ? 2.0f * t * t
                    : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
  case CueCurve::Pulse:
    return std::sin(t * 3.14159265358979323846f);
  case CueCurve::Linear:
  default:
    return t;
  }
}

}

std::vector<CueSample> Sequence::sample(const std::int64_t relativeFrame) const {
  std::vector<CueSample> result;
  if (durationFrames <= 0 || cues.empty()) {
    return result;
  }

  const float sequenceProgress = std::clamp(
      static_cast<float>(relativeFrame) / static_cast<float>(durationFrames), 0.0f, 1.0f);
  result.reserve(cues.size());
  for (const Cue& cue : cues) {
    if (!cue.enabled || cue.duration <= 0.0f || sequenceProgress < cue.start ||
        sequenceProgress > cue.start + cue.duration) {
      continue;
    }

    const float localProgress =
        std::clamp((sequenceProgress - cue.start) / cue.duration, 0.0f, 1.0f);
    const float weight = sampleCurve(cue.curve, localProgress);
    result.push_back({cue.kind,
                      localProgress,
                      weight,
                      cue.strength * strength * weight,
                      cue.variation,
                      cue.seed != 0 ? cue.seed : seed});
  }
  return result;
}

Sequence makeGlitchBurstSequence() {
  Sequence sequence;
  sequence.family = Family::Glitch;
  sequence.durationFrames = 12;
  sequence.cues = {
      {CueKind::SignalDrop, CueCurve::Step, 0.00f, 0.08f, 0.70f, 0.35f, 11, true},
      {CueKind::BlockDisplace, CueCurve::Pulse, 0.05f, 0.45f, 0.90f, 0.60f, 23, true},
      {CueKind::Freeze, CueCurve::Step, 0.18f, 0.18f, 1.00f, 0.20f, 37, true},
      {CueKind::ChannelSplit, CueCurve::Pulse, 0.22f, 0.58f, 0.75f, 0.45f, 41, true},
      {CueKind::Recovery, CueCurve::EaseOut, 0.72f, 0.28f, 0.55f, 0.15f, 53, true},
  };
  return sequence;
}

}
