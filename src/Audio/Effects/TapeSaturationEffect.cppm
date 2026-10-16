module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.TapeSaturation;

import Audio.Segment;

namespace Artifact {

void TapeSaturationEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> TapeSaturationEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"drive_db",  "Drive (dB)", AudioEffectParameterType::Float,   0.0f, 24.0f,  6.0f},
        {"warmth",    "Warmth",     AudioEffectParameterType::Float,   0.0f,  1.0f,  0.4f},
        {"output_db", "Output (dB)", AudioEffectParameterType::Float, -12.0f, 12.0f, -6.0f},
        {"mix",       "Mix",        AudioEffectParameterType::Float,   0.0f,  1.0f,  1.0f},
    };
}

void TapeSaturationEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "drive_db")  core_.setDriveDb(value);
    else if (name == "warmth")    core_.setWarmth(value);
    else if (name == "output_db") core_.setOutputDb(value);
    else if (name == "mix")       core_.setMix(value);
}

float TapeSaturationEffect::getParameter(const String& name) const {
    if      (name == "drive_db")  return core_.getDriveDb();
    else if (name == "warmth")    return core_.getWarmth();
    else if (name == "output_db") return core_.getOutputDb();
    else if (name == "mix")       return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createTapeSaturationEffect() {
    return std::make_unique<TapeSaturationEffect>();
}

} // namespace Artifact
