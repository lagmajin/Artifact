module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.StereoMixer;

import Audio.Segment;

namespace Artifact {

void StereoMixerEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> StereoMixerEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"left_right", "Balance (L-R)", AudioEffectParameterType::Float, -1.0f, 1.0f, 0.0f},
    };
}

void StereoMixerEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if (name == "left_right") core_.setLeftRight(value);
}

float StereoMixerEffect::getParameter(const String& name) const {
    if (name == "left_right") return core_.getLeftRight();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createStereoMixerEffect() {
    return std::make_unique<StereoMixerEffect>();
}

} // namespace Artifact
