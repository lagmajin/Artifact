module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <QList>
module Artifact.Audio.Effects.Compressor;

import Audio.Segment;

namespace Artifact {

// ============================================================================
// Utility Functions
// ============================================================================

static inline float linearToDb(float linear) {
    if (linear <= 0.0f) return -96.0f;
    return 20.0f * std::log10(linear);
}

static inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

// ============================================================================
// CompressorEffect Implementation
// ============================================================================

::ArtifactCore::AudioSegment CompressorEffect::process(const ::ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;
    float sr = static_cast<float>(sampleRate_);

    int numChannels = static_cast<int>(output.channelData.size());
    int numSamples  = (numChannels > 0) ? static_cast<int>(output.channelData[0].size()) : 0;
    if (numSamples == 0) return output;

    // Calculate attack and release coefficients (one-pole smoother)
    float attackCoeff  = std::exp(-1.0f / (attackMs_ * 0.001f * sr));
    float releaseCoeff = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    // Calculate auto make-up gain
    float effectiveMakeup = makeupGain_;
    if (autoMakeup_) {
        // Estimate gain reduction at threshold
        float gainReductionAtThreshold = threshold_ - (threshold_ / ratio_);
        effectiveMakeup = gainReductionAtThreshold * 0.5f; // Compensate ~half
    }
    float makeupLinear = dbToLinear(effectiveMakeup);

    // Half knee width
    float halfKnee = kneeWidth_ * 0.5f;

    for (int i = 0; i < numSamples; ++i) {
        // 1. Detect peak level across all channels
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            float absSample = std::fabs(input.channelData[ch][i]);
            if (absSample > peak) peak = absSample;
        }

        float inputDb = linearToDb(peak);

        // 2. Envelope follower (smooth the level detection)
        if (inputDb > envelopeDb_) {
            envelopeDb_ = attackCoeff * envelopeDb_ + (1.0f - attackCoeff) * inputDb;
        } else {
            envelopeDb_ = releaseCoeff * envelopeDb_ + (1.0f - releaseCoeff) * inputDb;
        }

        // 3. Gain computation with soft knee
        float gainDb = 0.0f;
        float overDb = envelopeDb_ - threshold_;

        if (overDb <= -halfKnee) {
            // Below knee: no compression
            gainDb = 0.0f;
        } else if (overDb >= halfKnee) {
            // Above knee: full compression
            gainDb = overDb * (1.0f / ratio_ - 1.0f);
        } else {
            // In the knee: quadratic interpolation
            float x = overDb + halfKnee;
            gainDb = (1.0f / ratio_ - 1.0f) * x * x / (2.0f * kneeWidth_);
        }

        float gainLinear = dbToLinear(gainDb) * makeupLinear;

        // 4. Apply gain to all channels
        for (int ch = 0; ch < numChannels; ++ch) {
            output.channelData[ch][i] = input.channelData[ch][i] * gainLinear;
        }
    }

    return output;
}

std::vector<Parameter> CompressorEffect::getParameters() const {
    return {
        {"threshold",  "Threshold",    ParameterType::Float, -60.0f, 0.0f,  -20.0f},
        {"ratio",      "Ratio",        ParameterType::Float, 1.0f,   20.0f, 4.0f},
        {"attack",     "Attack (ms)",  ParameterType::Float, 0.1f,   100.0f, 10.0f},
        {"release",    "Release (ms)", ParameterType::Float, 10.0f,  1000.0f, 100.0f},
        {"knee",       "Knee (dB)",    ParameterType::Float, 0.0f,   24.0f, 6.0f},
        {"makeup",     "Makeup Gain",  ParameterType::Float, 0.0f,   24.0f, 0.0f},
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

std::unique_ptr<IAudioEffect> createCompressorEffect() {
    return std::make_unique<CompressorEffect>();
}

} // namespace Artifact
