module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Vibrato;

import Audio.Segment;

namespace Artifact {

void VibratoEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> VibratoEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"rate",     "Rate (Hz)",       AudioEffectParameterType::Float, 1.0f, 15.0f, 6.0f},
        {"depth_ms", "Depth (ms)",      AudioEffectParameterType::Float, 0.0f, 10.0f, 2.0f},
        {"base_ms",  "Base Delay (ms)", AudioEffectParameterType::Float, 3.0f, 10.0f, 5.0f},
        {"mix",      "Mix",             AudioEffectParameterType::Float, 0.0f,  1.0f, 1.0f},
    };
}

void VibratoEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "rate")     core_.setRate(value);
    else if (name == "depth_ms") core_.setDepthMs(value);
    else if (name == "base_ms")  core_.setBaseMs(value);
    else if (name == "mix")      core_.setMix(value);
}

float VibratoEffect::getParameter(const String& name) const {
    if      (name == "rate")     return core_.getRate();
    else if (name == "depth_ms") return core_.getDepthMs();
    else if (name == "base_ms")  return core_.getBaseMs();
    else if (name == "mix")      return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createVibratoEffect() {
    return std::make_unique<VibratoEffect>();
}

} // namespace Artifact
