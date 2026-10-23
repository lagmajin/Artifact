module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Phaser;

import Audio.Segment;

namespace Artifact {

void PhaserEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> PhaserEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"rate",     "Rate (Hz)",   AudioEffectParameterType::Float, 0.05f, 10.0f,   0.5f},
        {"depth",    "Depth",       AudioEffectParameterType::Float, 0.0f,   1.0f,    0.6f},
        {"center",   "Center (Hz)", AudioEffectParameterType::Float, 200.0f, 4000.0f, 800.0f},
        {"feedback", "Feedback",    AudioEffectParameterType::Float, 0.0f,   0.9f,    0.4f},
        {"stages",   "Stages",      AudioEffectParameterType::Float, 2.0f,   8.0f,    4.0f},
        {"mix",      "Mix",         AudioEffectParameterType::Float, 0.0f,   1.0f,    0.5f},
    };
}

void PhaserEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "rate")     core_.setRate(value);
    else if (name == "depth")    core_.setDepth(value);
    else if (name == "center")   core_.setCenter(value);
    else if (name == "feedback") core_.setFeedback(value);
    else if (name == "stages")   core_.setStages(value);
    else if (name == "mix")      core_.setMix(value);
}

float PhaserEffect::getParameter(const String& name) const {
    if      (name == "rate")     return core_.getRate();
    else if (name == "depth")    return core_.getDepth();
    else if (name == "center")   return core_.getCenter();
    else if (name == "feedback") return core_.getFeedback();
    else if (name == "stages")   return core_.getStages();
    else if (name == "mix")      return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createPhaserEffect() {
    return std::make_unique<PhaserEffect>();
}

} // namespace Artifact
