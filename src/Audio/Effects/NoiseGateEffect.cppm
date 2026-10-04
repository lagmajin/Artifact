module;
#include <cmath>
#include <limits>
#include <memory>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.NoiseGate;

import Audio.Segment;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}
}

static inline float linearToDb(float linear) {
    if (linear <= 0.0f) return -96.0f;
    return 20.0f * std::log10(linear);
}

static inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

void NoiseGateEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_ || segment.channelData.isEmpty()) return;
    // オフライン書き出しとリアルタイム再生で実レートが異なるため、
    // バスから渡された segment のレートへ同期する。
    syncSampleRate(segment);

    const float sr = static_cast<float>(sampleRate_);
    const int numChannels = static_cast<int>(segment.channelData.size());
    int numSamples = (numChannels > 0)
        ? static_cast<int>(segment.channelData[0].size()) : 0;
    for (int ch = 1; ch < numChannels; ++ch) {
        numSamples = std::min(numSamples,
                              static_cast<int>(segment.channelData[ch].size()));
    }
    if (numSamples == 0 || sr <= 0.0f) return;

    const float attackCoeff  = std::exp(-1.0f / (attackMs_  * 0.001f * sr));
    const float releaseCoeff = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    // 閾値以下で最終 gain へ収束する量。rangeDb_ が -∞ に近づくと完全ミュート。
    const float rangeLinear = dbToLinear(rangeDb_);
    const float invSr = 1.0f / sr;

    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            const float sample = finiteOr(segment.channelData[ch][i], 0.0f);
            segment.channelData[ch][i] = sample;
            const float absSample = std::fabs(sample);
            if (absSample > peak) peak = absSample;
        }

        const float inputDb = linearToDb(peak);

        if (inputDb > envelopeDb_) {
            envelopeDb_ = attackCoeff * envelopeDb_ + (1.0f - attackCoeff) * inputDb;
            holdCounterMs_ = 0.0f;
        } else {
            envelopeDb_ = releaseCoeff * envelopeDb_ + (1.0f - releaseCoeff) * inputDb;
            holdCounterMs_ += invSr * 1000.0f;
        }

        // hold 時間だけゲートを保持し、attack が小さな子音で閉じてしまうのを防ぐ。
        float gainLinear = 1.0f;
        if (holdCounterMs_ >= holdMs_) {
            const float belowDb = thresholdDb_ - envelopeDb_;
            if (belowDb > 0.0f) {
                // rangeDb_ は最大減衰。閾値から下へ下がるほど減衰が深くなり、
                // 無音 (-96dB 床) では rangeDb_ に収束する。
                const float floorDb = -96.0f;
                const float spanDb = thresholdDb_ - floorDb;
                const float ratio = (spanDb > 0.0f)
                    ? std::clamp(belowDb / spanDb, 0.0f, 1.0f)
                    : 0.0f;
                gainLinear = 1.0f + (rangeLinear - 1.0f) * ratio;
            }
        }
        for (int ch = 0; ch < numChannels; ++ch) {
            const float out = segment.channelData[ch][i] * gainLinear;
            segment.channelData[ch][i] = std::isfinite(out) ? out : 0.0f;
        }
    }
}

std::vector<AudioEffectParameter> NoiseGateEffect::getUiParameters() const {
    return {
        {"threshold", "Threshold (dB)", AudioEffectParameterType::Float, -80.0f, 0.0f, -50.0f},
        {"range",     "Range (dB)",     AudioEffectParameterType::Float, -80.0f, 0.0f, -60.0f},
        {"attack",    "Attack (ms)",    AudioEffectParameterType::Float, 0.1f,  100.0f, 1.0f},
        {"hold",      "Hold (ms)",      AudioEffectParameterType::Float, 0.0f,  500.0f, 0.0f},
        {"release",   "Release (ms)",   AudioEffectParameterType::Float, 10.0f, 1000.0f, 50.0f},
    };
}

void NoiseGateEffect::setParameter(const String& name, float value) {
    if      (name == "threshold") thresholdDb_ = std::clamp(finiteOr(value, -50.0f), -80.0f, 0.0f);
    else if (name == "range")     rangeDb_     = std::clamp(finiteOr(value, -60.0f), -80.0f, 0.0f);
    else if (name == "attack")    attackMs_    = std::clamp(finiteOr(value, 1.0f),   0.1f, 100.0f);
    else if (name == "hold")      holdMs_      = std::clamp(finiteOr(value, 0.0f),   0.0f, 500.0f);
    else if (name == "release")   releaseMs_   = std::clamp(finiteOr(value, 50.0f),  10.0f, 1000.0f);
}

float NoiseGateEffect::getParameter(const String& name) const {
    if      (name == "threshold") return thresholdDb_;
    else if (name == "range")     return rangeDb_;
    else if (name == "attack")    return attackMs_;
    else if (name == "hold")      return holdMs_;
    else if (name == "release")   return releaseMs_;
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createNoiseGateEffect() {
    return std::make_unique<NoiseGateEffect>();
}

} // namespace Artifact
