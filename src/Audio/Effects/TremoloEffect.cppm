module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Tremolo;

import Audio.Segment;

namespace Artifact {

void TremoloEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> TremoloEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"rate",  "Rate (Hz)",             AudioEffectParameterType::Float, 0.1f, 20.0f, 5.0f},
        {"depth", "Depth",                 AudioEffectParameterType::Float, 0.0f,  1.0f, 0.8f},
        {"shape", "Shape (Sine-Square)",   AudioEffectParameterType::Float, 0.0f,  1.0f, 0.0f},
        {"width", "Stereo Width (Auto-Pan)", AudioEffectParameterType::Float, 0.0f, 1.0f, 0.0f},
        {"mix",   "Mix",                   AudioEffectParameterType::Float, 0.0f,  1.0f, 1.0f},
    };
}

void TremoloEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "rate")  core_.setRate(value);
    else if (name == "depth") core_.setDepth(value);
    else if (name == "shape") core_.setShape(value);
    else if (name == "width") core_.setWidth(value);
    else if (name == "mix")   core_.setMix(value);
}

float TremoloEffect::getParameter(const String& name) const {
    if      (name == "rate")  return core_.getRate();
    else if (name == "depth") return core_.getDepth();
    else if (name == "shape") return core_.getShape();
    else if (name == "width") return core_.getWidth();
    else if (name == "mix")   return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createTremoloEffect() {
    return std::make_unique<TremoloEffect>();
}

} // namespace Artifact
