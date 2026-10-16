module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.PingPongDelay;

import Audio.Segment;

namespace Artifact {

void PingPongDelayEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> PingPongDelayEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"time_ms",  "Time (ms)", AudioEffectParameterType::Float, 1.0f, 2000.0f, 375.0f},
        {"feedback", "Feedback",  AudioEffectParameterType::Float, 0.0f,   0.95f,   0.4f},
        {"mix",      "Mix",       AudioEffectParameterType::Float, 0.0f,   1.0f,   0.35f},
    };
}

void PingPongDelayEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "time_ms")  core_.setTimeMs(value);
    else if (name == "feedback") core_.setFeedback(value);
    else if (name == "mix")      core_.setMix(value);
}

float PingPongDelayEffect::getParameter(const String& name) const {
    if      (name == "time_ms")  return core_.getTimeMs();
    else if (name == "feedback") return core_.getFeedback();
    else if (name == "mix")      return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createPingPongDelayEffect() {
    return std::make_unique<PingPongDelayEffect>();
}

} // namespace Artifact
