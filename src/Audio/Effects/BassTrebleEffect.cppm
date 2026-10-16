module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.BassTreble;

import Audio.Segment;

namespace Artifact {

void BassTrebleEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> BassTrebleEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"bass",   "Bass (dB)",   AudioEffectParameterType::Float, -24.0f, 24.0f, 0.0f},
        {"treble", "Treble (dB)", AudioEffectParameterType::Float, -24.0f, 24.0f, 0.0f},
    };
}

void BassTrebleEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "bass")   core_.setBass(value);
    else if (name == "treble") core_.setTreble(value);
}

float BassTrebleEffect::getParameter(const String& name) const {
    if      (name == "bass")   return core_.getBass();
    else if (name == "treble") return core_.getTreble();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createBassTrebleEffect() {
    return std::make_unique<BassTrebleEffect>();
}

} // namespace Artifact
