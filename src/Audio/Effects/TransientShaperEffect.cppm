module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.TransientShaper;

import Audio.Segment;

namespace Artifact {

void TransientShaperEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> TransientShaperEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"attack",   "Attack",     AudioEffectParameterType::Float, -1.0f,  1.0f,  0.3f},
        {"sustain",  "Sustain",    AudioEffectParameterType::Float, -1.0f,  1.0f,  0.0f},
        {"speed_ms", "Speed (ms)", AudioEffectParameterType::Float,  1.0f, 100.0f, 20.0f},
    };
}

void TransientShaperEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "attack")   core_.setAttack(value);
    else if (name == "sustain")  core_.setSustain(value);
    else if (name == "speed_ms") core_.setSpeedMs(value);
}

float TransientShaperEffect::getParameter(const String& name) const {
    if      (name == "attack")   return core_.getAttack();
    else if (name == "sustain")  return core_.getSustain();
    else if (name == "speed_ms") return core_.getSpeedMs();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createTransientShaperEffect() {
    return std::make_unique<TransientShaperEffect>();
}

} // namespace Artifact
