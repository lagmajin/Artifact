module;
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.PlateReverb;

import Audio.Segment;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}
}

void PlateReverbEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain) {
    if (!enabled_) return;
    // オフライン書き出しとリアルタイム再生で実レートが異なるため、
    // バスから渡された segment のレートへ同期する。
    // Core 側は ensureCapacity で自前再構築するため reinit override は不要。
    syncSampleRate(segment);
    // Core 実装は自前で bypass_ を読むため、false を引き継いで二重 bypass を避ける。
    core_.setBypass(false);
    core_.process(segment, sideChain);
}

std::vector<AudioEffectParameter> PlateReverbEffect::getUiParameters() const {
    // 範囲・既定値は Core 側の clamp に揃える。
    return {
        {"pre_delay_ms", "Pre-Delay (ms)", AudioEffectParameterType::Float, 0.0f, 100.0f, 10.0f},
        {"decay",        "Decay",          AudioEffectParameterType::Float, 0.0f,   0.99f,  0.75f},
        {"damping",      "Damping",        AudioEffectParameterType::Float, 0.0f,   1.0f,   0.4f},
        {"diffusion",    "Diffusion",      AudioEffectParameterType::Float, 0.0f,   1.0f,   0.7f},
        {"size",         "Size",           AudioEffectParameterType::Float, 0.5f,   2.0f,   1.0f},
        {"mod_depth",    "Mod Depth",      AudioEffectParameterType::Float, 0.0f,   1.0f,   0.3f},
        {"mod_rate",     "Mod Rate (Hz)",  AudioEffectParameterType::Float, 0.1f,   5.0f,   0.8f},
        {"width",        "Stereo Width",   AudioEffectParameterType::Float, 0.0f,   1.0f,   0.8f},
        {"mix",          "Mix",            AudioEffectParameterType::Float, 0.0f,   1.0f,   0.3f},
    };
}

void PlateReverbEffect::setParameter(const String& name, float value) {
    // clamp は Core 側 setter が担うためここでは素通しする。
    if      (name == "pre_delay_ms") core_.setPreDelayMs(value);
    else if (name == "decay")        core_.setDecay(finiteOr(value, 0.75f));
    else if (name == "damping")      core_.setDamping(value);
    else if (name == "diffusion")    core_.setDiffusion(value);
    else if (name == "size")         core_.setSize(value);
    else if (name == "mod_depth")    core_.setModDepth(value);
    else if (name == "mod_rate")     core_.setModRate(value);
    else if (name == "width")        core_.setWidth(value);
    else if (name == "mix")          core_.setMix(value);
}

float PlateReverbEffect::getParameter(const String& name) const {
    if      (name == "pre_delay_ms") return core_.getPreDelayMs();
    else if (name == "decay")        return core_.getDecay();
    else if (name == "damping")      return core_.getDamping();
    else if (name == "diffusion")    return core_.getDiffusion();
    else if (name == "size")         return core_.getSize();
    else if (name == "mod_depth")    return core_.getModDepth();
    else if (name == "mod_rate")     return core_.getModRate();
    else if (name == "width")        return core_.getWidth();
    else if (name == "mix")          return core_.getMix();
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createPlateReverbEffect() {
    return std::make_unique<PlateReverbEffect>();
}

} // namespace Artifact
