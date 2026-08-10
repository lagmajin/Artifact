module;
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>
#include <QList>
module Artifact.Audio.Effects.Compressor;

import Audio.Segment;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float sanitizeCompressorSample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}
}

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
    int numSamples = (numChannels > 0)
        ? static_cast<int>(segment.channelData[0].size()) : 0;
    for (int ch = 1; ch < numChannels; ++ch) {
        numSamples = std::min(numSamples,
                              static_cast<int>(segment.channelData[ch].size()));
    }
    if (numSamples == 0 || sr <= 0.0f) return;

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
            const float sample = finiteOr(segment.channelData[ch][i], 0.0f);
            segment.channelData[ch][i] = sample;
            float absSample = std::fabs(sample);
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
            segment.channelData[ch][i] = sanitizeCompressorSample(
                segment.channelData[ch][i] * gainLinear);
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

void CompressorEffect::setParameter(const String& name, float value) {
    if      (name == "threshold") threshold_  = std::clamp(finiteOr(value, -20.0f), -60.0f, 0.0f);
    else if (name == "ratio")     ratio_      = std::clamp(finiteOr(value, 4.0f), 1.0f, 20.0f);
    else if (name == "attack")    attackMs_   = std::clamp(finiteOr(value, 10.0f), 0.1f, 100.0f);
    else if (name == "release")   releaseMs_  = std::clamp(finiteOr(value, 100.0f), 10.0f, 1000.0f);
    else if (name == "knee")      kneeWidth_  = std::clamp(finiteOr(value, 6.0f), 0.0f, 24.0f);
    else if (name == "makeup")  {
        makeupGain_ = std::clamp(finiteOr(value, 0.0f), 0.0f, 24.0f);
        autoMakeup_ = (makeupGain_ == 0.0f);
    }
}

float CompressorEffect::getParameter(const String& name) const {
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
