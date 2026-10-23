module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.DeEsser;

import Audio.Segment;

namespace Artifact {

void DeEsserEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> DeEsserEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"frequency",    "Frequency (Hz)", AudioEffectParameterType::Float, 2000.0f, 12000.0f, 6500.0f},
        {"threshold_db", "Threshold (dB)", AudioEffectParameterType::Float, -60.0f,    0.0f,  -30.0f},
        {"reduction",    "Reduction",      AudioEffectParameterType::Float,    0.0f,    1.0f,    0.6f},
    };
}

void DeEsserEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "frequency")    core_.setFrequency(value);
    else if (name == "threshold_db") core_.setThresholdDb(value);
    else if (name == "reduction")    core_.setReduction(value);
}

float DeEsserEffect::getParameter(const String& name) const {
    if      (name == "frequency")    return core_.getFrequency();
    else if (name == "threshold_db") return core_.getThresholdDb();
    else if (name == "reduction")    return core_.getReduction();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createDeEsserEffect() {
    return std::make_unique<DeEsserEffect>();
}

} // namespace Artifact
