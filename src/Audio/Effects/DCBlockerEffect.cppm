module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.DCBlocker;

import Audio.Segment;

namespace Artifact {

void DCBlockerEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> DCBlockerEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"frequency", "Frequency (Hz)", AudioEffectParameterType::Float,  2.0f, 100.0f, 10.0f},
        {"mix",       "Mix",            AudioEffectParameterType::Float,  0.0f,   1.0f,  1.0f},
    };
}

void DCBlockerEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "frequency") core_.setFrequency(value);
    else if (name == "mix")       core_.setMix(value);
}

float DCBlockerEffect::getParameter(const String& name) const {
    if      (name == "frequency") return core_.getFrequency();
    else if (name == "mix")       return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createDCBlockerEffect() {
    return std::make_unique<DCBlockerEffect>();
}

} // namespace Artifact
