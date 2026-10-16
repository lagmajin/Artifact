module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.BitCrusher;

import Audio.Segment;

namespace Artifact {

void BitCrusherEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> BitCrusherEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"bits",       "Bits",       AudioEffectParameterType::Float,  2.0f, 16.0f, 8.0f},
        {"downsample", "Downsample", AudioEffectParameterType::Float,  1.0f, 40.0f, 1.0f},
        {"mix",        "Mix",        AudioEffectParameterType::Float,  0.0f,  1.0f, 1.0f},
    };
}

void BitCrusherEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "bits")       core_.setBits(value);
    else if (name == "downsample") core_.setDownsample(value);
    else if (name == "mix")        core_.setMix(value);
}

float BitCrusherEffect::getParameter(const String& name) const {
    if      (name == "bits")       return core_.getBits();
    else if (name == "downsample") return core_.getDownsample();
    else if (name == "mix")        return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createBitCrusherEffect() {
    return std::make_unique<BitCrusherEffect>();
}

} // namespace Artifact
