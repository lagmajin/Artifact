module;
#include <QList>
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Limiter;

import Audio.Segment;

namespace Artifact {

static inline float linearToDb(float linear) {
    if (linear <= 0.0f) return -96.0f;
    return 20.0f * std::log10(linear);
}

static inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

LimiterEffect::LimiterEffect() {
    initializeEngine();
}

void LimiterEffect::initializeEngine() {
    float sr = static_cast<float>(sampleRate_);
    lookaheadSamples_ = static_cast<int>(0.005f * sr);
    if (lookaheadSamples_ < 1) lookaheadSamples_ = 1;
    if (lookaheadSamples_ > kMaxLookahead) lookaheadSamples_ = kMaxLookahead;
    lookaheadWritePos_ = 0;
    currentGain_ = 1.0f;
}

void LimiterEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_ || segment.channelData.isEmpty()) return;

    float sr = static_cast<float>(sampleRate_);
    int numChannels = static_cast<int>(segment.channelData.size());
    int numSamples  = (numChannels > 0) ? static_cast<int>(segment.channelData[0].size()) : 0;
    if (numSamples == 0) return;

    float ceilingLinear = dbToLinear(ceiling_);
    float inputGainLinear = dbToLinear(inputGain_);
    float releaseCoeff = std::exp(-1.0f / (releaseMs_ * 0.001f * sr));

    std::vector<float> peakLevels(numSamples, 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            float val = std::fabs(segment.channelData[ch][i] * inputGainLinear);
            if (val > peak) peak = val;
        }
        peakLevels[i] = peak;
    }

    std::vector<float> lookaheadPeak(numSamples, 0.0f);
    {
        float runningMax = 0.0f;
        for (int i = numSamples - 1; i >= 0; --i) {
            runningMax = peakLevels[i];
            int endLookahead = std::min(i + lookaheadSamples_, numSamples);
            for (int j = i; j < endLookahead; ++j) {
                if (peakLevels[j] > runningMax) runningMax = peakLevels[j];
            }
            lookaheadPeak[i] = runningMax;
        }
    }

    for (int i = 0; i < numSamples; ++i) {
        float targetGain = 1.0f;
        if (lookaheadPeak[i] > ceilingLinear) {
            targetGain = ceilingLinear / lookaheadPeak[i];
        }

        if (targetGain < currentGain_) {
            currentGain_ = targetGain;
        } else {
            currentGain_ = releaseCoeff * currentGain_ + (1.0f - releaseCoeff) * targetGain;
        }

        float totalGain = inputGainLinear * currentGain_;
        for (int ch = 0; ch < numChannels; ++ch) {
            segment.channelData[ch][i] *= totalGain;
        }
    }
}

std::vector<AudioEffectParameter> LimiterEffect::getParameters() const {
    return {
        {"ceiling",    "Ceiling (dB)",     AudioEffectParameterType::Float, -6.0f,  0.0f,   -0.3f},
        {"release",    "Release (ms)",     AudioEffectParameterType::Float, 5.0f,   500.0f, 50.0f},
        {"input_gain", "Input Gain (dB)",  AudioEffectParameterType::Float, 0.0f,   24.0f,  0.0f},
    };
}

void LimiterEffect::setParameter(const std::string& name, float value) {
    if      (name == "ceiling")    ceiling_   = value;
    else if (name == "release")    releaseMs_ = value;
    else if (name == "input_gain") inputGain_ = value;
}

float LimiterEffect::getParameter(const std::string& name) const {
    if      (name == "ceiling")    return ceiling_;
    else if (name == "release")    return releaseMs_;
    else if (name == "input_gain") return inputGain_;
    return 0.0f;
}

void LimiterEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    initializeEngine();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect() {
    return std::make_unique<LimiterEffect>();
}

} // namespace Artifact