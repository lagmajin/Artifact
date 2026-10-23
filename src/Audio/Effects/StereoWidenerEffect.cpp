module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.StereoWidener;

import Audio.Segment;

namespace Artifact {

void StereoWidenerEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> StereoWidenerEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"width",          "Width",                 AudioEffectParameterType::Float,   0.0f,   2.0f, 1.2f},
        {"bass_mono_freq", "Bass Mono (Hz, 0=Off)", AudioEffectParameterType::Float,   0.0f, 500.0f, 120.0f},
        {"trim_db",        "Trim (dB)",             AudioEffectParameterType::Float, -12.0f, 12.0f,  0.0f},
    };
}

void StereoWidenerEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "width")          core_.setWidth(value);
    else if (name == "bass_mono_freq") core_.setBassMonoFreq(value);
    else if (name == "trim_db")        core_.setTrimDb(value);
}

float StereoWidenerEffect::getParameter(const String& name) const {
    if      (name == "width")          return core_.getWidth();
    else if (name == "bass_mono_freq") return core_.getBassMonoFreq();
    else if (name == "trim_db")        return core_.getTrimDb();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createStereoWidenerEffect() {
    return std::make_unique<StereoWidenerEffect>();
}

} // namespace Artifact
