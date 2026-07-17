module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <QList>
module Artifact.Audio.Effects.Compressor;

import Audio.Segment;

namespace Artifact {

static inline float linearToDb(float linear) {
    if (linear <= 0.0f) return -96.0f;
    return 20.0f * std::log10(linear);
}

static inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

void CompressorEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_ || segment.channelData.isEmpty()) return;

    float sr = static_cast<float>(sampleRate_);
    int numChannels = static_cast<int>(segment.channelData.size());
    int numSamples  = (numChannels > 0) ? static_cast<int>(segment.channelData[0].size()) : 0;
    if (numSamples == 0) return;

    float attackCoeff  = std::exp(-1.0f / (attackMs_ * 0.001f * sr));
    float releaseCoeff = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    float effectiveMakeup = makeupGain_;
    if (autoMakeup_) {
        float gainReductionAtThreshold = threshold_ - (threshold_ / ratio_);
        effectiveMakeup = gainReductionAtThreshold * 0.5f;
    }
    float makeupLinear = dbToLinear(effectiveMakeup);
    float halfKnee = kneeWidth_ * 0.5f;

    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            float absSample = std::fabs(segment.channelData[ch][i]);
            if (absSample > peak) peak = absSample;
        }

        float inputDb = linearToDb(peak);

        if (inputDb > envelopeDb_) {
            envelopeDb_ = attackCoeff * envelopeDb_ + (1.0f - attackCoeff) * inputDb;
        } else {
            envelopeDb_ = releaseCoeff * envelopeDb_ + (1.0f - releaseCoeff) * inputDb;
        }

        float gainDb = 0.0f;
        float overDb = envelopeDb_ - threshold_;

        if (overDb <= -halfKnee) {
            gainDb = 0.0f;
        } else if (overDb >= halfKnee) {
            gainDb = overDb * (1.0f / ratio_ - 1.0f);
        } else {
            float x = overDb + halfKnee;
            gainDb = (1.0f / ratio_ - 1.0f) * x * x / (2.0f * kneeWidth_);
        }

        float gainLinear = dbToLinear(gainDb) * makeupLinear;

        for (int ch = 0; ch < numChannels; ++ch) {
            segment.channelData[ch][i] *= gainLinear;
        }
    }
}

std::vector<AudioEffectParameter> CompressorEffect::getUiParameters() const {
    return {
        {"threshold",  "Threshold",    AudioEffectParameterType::Float, -60.0f, 0.0f,  -20.0f},
        {"ratio",      "Ratio",        AudioEffectParameterType::Float, 1.0f,   20.0f, 4.0f},
        {"attack",     "Attack (ms)",  AudioEffectParameterType::Float, 0.1f,   100.0f, 10.0f},
        {"release",    "Release (ms)", AudioEffectParameterType::Float, 10.0f,  1000.0f, 100.0f},
        {"knee",       "Knee (dB)",    AudioEffectParameterType::Float, 0.0f,   24.0f, 6.0f},
        {"makeup",     "Makeup Gain",  AudioEffectParameterType::Float, 0.0f,   24.0f, 0.0f},
    };
}

void CompressorEffect::setParameter(const std::string& name, float value) {
    if      (name == "threshold") threshold_  = value;
    else if (name == "ratio")     ratio_      = value;
    else if (name == "attack")    attackMs_   = value;
    else if (name == "release")   releaseMs_  = value;
    else if (name == "knee")      kneeWidth_  = value;
    else if (name == "makeup")  { makeupGain_ = value; autoMakeup_ = (value == 0.0f); }
}

float CompressorEffect::getParameter(const std::string& name) const {
    if      (name == "threshold") return threshold_;
    else if (name == "ratio")     return ratio_;
    else if (name == "attack")    return attackMs_;
    else if (name == "release")   return releaseMs_;
    else if (name == "knee")      return kneeWidth_;
    else if (name == "makeup")    return makeupGain_;
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createCompressorEffect() {
    return std::make_unique<CompressorEffect>();
}

} // namespace Artifact
