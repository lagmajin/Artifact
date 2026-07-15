module;

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

module Artifact.FxStudio.PresetCatalog;

namespace Artifact::FxStudio {

namespace {

constexpr std::string_view kGlitchBurstId = "fx.glitch.burst";
constexpr std::string_view kImpactPunchId = "fx.impact.punch";
constexpr std::string_view kRevealSweepId = "fx.reveal.sweep";
constexpr std::string_view kEnergySurgeId = "fx.energy.surge";
constexpr std::string_view kGlitchTransitionId = "fx.transition.glitch";

}

const std::vector<PresetDescriptor>& PresetCatalog::descriptors() {
  static const std::vector<PresetDescriptor> entries = {
      {kGlitchBurstId, "Glitch Burst", Family::Glitch,
       "Signal loss, block displacement, freeze, channel split, and recovery."},
      {kImpactPunchId, "Impact Punch", Family::Impact,
       "A short flash and shake burst followed by a controlled recovery."},
      {kRevealSweepId, "Reveal Sweep", Family::Reveal,
       "A mask-driven reveal with a restrained glow at the moving edge."},
      {kEnergySurgeId, "Energy Surge", Family::Energy,
       "A rising glow pulse with distortion at peak energy."},
      {kGlitchTransitionId, "Glitch Transition", Family::Transition,
       "A two-input transition driven by signal loss and channel separation."},
  };
  return entries;
}

const PresetDescriptor* PresetCatalog::find(const std::string_view id) {
  const auto& entries = descriptors();
  const auto it = std::find_if(entries.begin(), entries.end(),
                               [id](const PresetDescriptor& entry) { return entry.id == id; });
  return it != entries.end() ? &*it : nullptr;
}

std::optional<Sequence> PresetCatalog::create(const std::string_view id) {
  if (id == kGlitchBurstId) {
    return makeGlitchBurstSequence();
  }
  if (id == kImpactPunchId) {
    return makeImpactPunchSequence();
  }
  if (id == kRevealSweepId) {
    return makeRevealSweepSequence();
  }
  if (id == kEnergySurgeId) {
    return makeEnergySurgeSequence();
  }
  if (id == kGlitchTransitionId) {
    return makeGlitchTransitionSequence();
  }
  return std::nullopt;
}

Sequence makeImpactPunchSequence() {
  Sequence sequence;
  sequence.family = Family::Impact;
  sequence.durationFrames = 10;
  sequence.cues = {
      {CueKind::Flash, CueCurve::Pulse, 0.00f, 0.28f, 0.90f, 0.10f, 101, true},
      {CueKind::Shake, CueCurve::Pulse, 0.04f, 0.62f, 1.00f, 0.45f, 103, true},
      {CueKind::Distort, CueCurve::Pulse, 0.08f, 0.42f, 0.55f, 0.20f, 107, true},
      {CueKind::Recovery, CueCurve::EaseOut, 0.58f, 0.42f, 0.40f, 0.10f, 109, true},
  };
  return sequence;
}

Sequence makeRevealSweepSequence() {
  Sequence sequence;
  sequence.family = Family::Reveal;
  sequence.durationFrames = 18;
  sequence.cues = {
      {CueKind::MaskProgress, CueCurve::EaseInOut, 0.00f, 1.00f, 1.00f, 0.00f, 201, true},
      {CueKind::GlowPulse, CueCurve::Pulse, 0.08f, 0.84f, 0.45f, 0.15f, 211, true},
  };
  return sequence;
}

Sequence makeEnergySurgeSequence() {
  Sequence sequence;
  sequence.family = Family::Energy;
  sequence.durationFrames = 16;
  sequence.cues = {
      {CueKind::GlowPulse, CueCurve::Pulse, 0.00f, 1.00f, 1.00f, 0.30f, 307, true},
      {CueKind::Distort, CueCurve::Pulse, 0.24f, 0.54f, 0.60f, 0.45f, 311, true},
      {CueKind::Flash, CueCurve::Pulse, 0.42f, 0.20f, 0.50f, 0.10f, 313, true},
  };
  return sequence;
}

Sequence makeGlitchTransitionSequence() {
  Sequence sequence;
  sequence.family = Family::Transition;
  sequence.durationFrames = 14;
  sequence.cues = {
      {CueKind::MaskProgress, CueCurve::EaseInOut, 0.00f, 1.00f, 1.00f, 0.00f, 401, true},
      {CueKind::SignalDrop, CueCurve::Pulse, 0.18f, 0.54f, 0.65f, 0.50f, 409, true},
      {CueKind::BlockDisplace, CueCurve::Pulse, 0.20f, 0.52f, 0.85f, 0.60f, 419, true},
      {CueKind::ChannelSplit, CueCurve::Pulse, 0.28f, 0.58f, 0.75f, 0.45f, 421, true},
      {CueKind::Recovery, CueCurve::EaseOut, 0.78f, 0.22f, 0.45f, 0.10f, 431, true},
  };
  return sequence;
}

}
