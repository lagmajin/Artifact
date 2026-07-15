module;

#include <cstdint>
#include <vector>

export module Artifact.FxStudio.Sequence;

export namespace Artifact::FxStudio {

enum class Family : std::uint8_t {
  Glitch,
  Impact,
  Reveal,
  Energy,
  Transition
};

enum class CueKind : std::uint8_t {
  Distort,
  ChannelSplit,
  BlockDisplace,
  Freeze,
  SignalDrop,
  Flash,
  Shake,
  MaskProgress,
  GlowPulse,
  Recovery
};

enum class CueCurve : std::uint8_t {
  Step,
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Pulse
};

struct Cue {
  CueKind kind = CueKind::Distort;
  CueCurve curve = CueCurve::Linear;
  float start = 0.0f;
  float duration = 1.0f;
  float strength = 1.0f;
  float variation = 0.0f;
  std::uint32_t seed = 0;
  bool enabled = true;
};

struct CueSample {
  CueKind kind = CueKind::Distort;
  float progress = 0.0f;
  float weight = 0.0f;
  float strength = 0.0f;
  float variation = 0.0f;
  std::uint32_t seed = 0;
};

struct Sequence {
  Family family = Family::Glitch;
  std::int64_t durationFrames = 12;
  float strength = 1.0f;
  std::uint32_t seed = 0;
  std::vector<Cue> cues;

  std::vector<CueSample> sample(std::int64_t relativeFrame) const;
};

Sequence makeGlitchBurstSequence();

}
