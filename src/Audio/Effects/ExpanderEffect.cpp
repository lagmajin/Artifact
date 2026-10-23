module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Expander;

import Audio.Segment;

namespace Artifact {

void ExpanderEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> ExpanderEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"threshold_db", "Threshold (dB)", AudioEffectParameterType::Float, -60.0f,  0.0f, -40.0f},
        {"ratio",        "Ratio",            AudioEffectParameterType::Float,   1.0f, 10.0f,   2.0f},
        {"attack_ms",    "Attack (ms)",      AudioEffectParameterType::Float,   0.1f, 100.0f, 10.0f},
        {"release_ms",   "Release (ms)",     AudioEffectParameterType::Float,  10.0f, 1000.0f, 150.0f},
    };
}

void ExpanderEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "threshold_db") core_.setThresholdDb(value);
    else if (name == "ratio")        core_.setRatio(value);
    else if (name == "attack_ms")    core_.setAttackMs(value);
    else if (name == "release_ms")   core_.setReleaseMs(value);
}

float ExpanderEffect::getParameter(const String& name) const {
    if      (name == "threshold_db") return core_.getThresholdDb();
    else if (name == "ratio")        return core_.getRatio();
    else if (name == "attack_ms")    return core_.getAttackMs();
    else if (name == "release_ms")   return core_.getReleaseMs();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createExpanderEffect() {
    return std::make_unique<ExpanderEffect>();
}

} // namespace Artifact
