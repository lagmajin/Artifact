module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.SpringReverb;

import Audio.Segment;

namespace Artifact {

void SpringReverbEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> SpringReverbEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"decay",      "Decay",      AudioEffectParameterType::Float, 0.0f, 0.99f, 0.8f},
        {"brightness", "Brightness", AudioEffectParameterType::Float, 0.0f,  1.0f, 0.5f},
        {"tension",    "Tension",    AudioEffectParameterType::Float, 0.5f,  1.5f, 1.0f},
        {"mix",        "Mix",        AudioEffectParameterType::Float, 0.0f,  1.0f, 0.3f},
    };
}

void SpringReverbEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "decay")      core_.setDecay(value);
    else if (name == "brightness") core_.setBrightness(value);
    else if (name == "tension")    core_.setTension(value);
    else if (name == "mix")        core_.setMix(value);
}

float SpringReverbEffect::getParameter(const String& name) const {
    if      (name == "decay")      return core_.getDecay();
    else if (name == "brightness") return core_.getBrightness();
    else if (name == "tension")    return core_.getTension();
    else if (name == "mix")        return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createSpringReverbEffect() {
    return std::make_unique<SpringReverbEffect>();
}

} // namespace Artifact
