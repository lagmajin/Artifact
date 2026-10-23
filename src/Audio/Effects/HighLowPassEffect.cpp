module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.HighLowPass;

import Audio.Segment;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}
}

void HighLowPassEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    // オフライン書き出しとリアルタイム再生で実レートが異なるため、
    // バスから渡された segment のレートへ同期する。
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> HighLowPassEffect::getUiParameters() const {
    // UI の上限は Core 側の clamp 上限 (24000Hz) に揃える。下限 0 は「無効」を表す。
    return {
        {"high_pass", "High Pass (Hz)", AudioEffectParameterType::Float, 0.0f, 24000.0f, 0.0f},
        {"low_pass",  "Low Pass (Hz)",  AudioEffectParameterType::Float, 0.0f, 24000.0f, 0.0f},
    };
}

void HighLowPassEffect::setParameter(const String& name, float value) {
    if      (name == "high_pass") core_.setHighPassFreq(std::clamp(finiteOr(value, 0.0f), 0.0f, 24000.0f));
    else if (name == "low_pass")  core_.setLowPassFreq(std::clamp(finiteOr(value, 0.0f), 0.0f, 24000.0f));
}

float HighLowPassEffect::getParameter(const String& name) const {
    if      (name == "high_pass") return core_.getHighPassFreq();
    else if (name == "low_pass")  return core_.getLowPassFreq();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createHighLowPassEffect() {
    return std::make_unique<HighLowPassEffect>();
}

} // namespace Artifact
