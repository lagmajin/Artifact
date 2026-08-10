module;
#include <QList>
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Chorus;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float sanitizeChorusSample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}
}

ChorusEffect::ChorusEffect() {
    initializeEngine();
}

void ChorusEffect::initializeEngine() {
    float sr = static_cast<float>(sampleRate_);

    for (int v = 0; v < kNumVoices; ++v) {
        float maxDelay = 0.05f;
        delayL_[v].initialize(maxDelay, sr);
        delayR_[v].initialize(maxDelay, sr);

        float voiceRate = rate_ * (1.0f + 0.15f * static_cast<float>(v));
        lfoL_[v].initialize(voiceRate, sr);

        float rRate = voiceRate * 1.03f;
        lfoR_[v].initialize(rRate, sr);
    }
}

void ChorusEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_ || segment.channelData.isEmpty()) return;

    float sr = static_cast<float>(sampleRate_);
    int numChannels = static_cast<int>(segment.channelData.size());
    int numSamples = (numChannels > 0)
        ? static_cast<int>(segment.channelData[0].size()) : 0;
    if (numChannels > 1) {
        numSamples = std::min(numSamples,
                              static_cast<int>(segment.channelData[1].size()));
    }
    if (numSamples == 0 || sr <= 0.0f) return;

    float centerDelaySamples = delayMs_ * 0.001f * sr;
    float depthSamples = depth_ * centerDelaySamples * 0.5f;

    for (int i = 0; i < numSamples; ++i) {
        float inL = segment.channelData[0][i];
        float inR = (numChannels > 1) ? segment.channelData[1][i] : inL;
        inL = finiteOr(inL, 0.0f);
        inR = finiteOr(inR, inL);

        float chorusL = 0.0f;
        float chorusR = 0.0f;

        for (int v = 0; v < kNumVoices; ++v) {
            float modL = lfoL_[v].process() * depthSamples;
            float modR = lfoR_[v].process() * depthSamples;

            float readDelayL = centerDelaySamples + modL;
            float readDelayR = centerDelaySamples + modR;

            if (readDelayL < 1.0f) readDelayL = 1.0f;
            if (readDelayR < 1.0f) readDelayR = 1.0f;

            float delL = sanitizeChorusSample(delayL_[v].read(readDelayL));
            float delR = sanitizeChorusSample(delayR_[v].read(readDelayR));

            chorusL = sanitizeChorusSample(chorusL + delL);
            chorusR = sanitizeChorusSample(chorusR + delR);

            delayL_[v].write(sanitizeChorusSample(inL + delL * feedback_));
            delayR_[v].write(sanitizeChorusSample(inR + delR * feedback_));
        }

        float voiceScale = 1.0f / static_cast<float>(kNumVoices);
        chorusL *= voiceScale;
        chorusR *= voiceScale;

        if (numChannels >= 2) {
            segment.channelData[0][i] = sanitizeChorusSample(
                inL * dryLevel_ + chorusL * wetLevel_);
            segment.channelData[1][i] = sanitizeChorusSample(
                inR * dryLevel_ + chorusR * wetLevel_);
        } else {
            segment.channelData[0][i] = sanitizeChorusSample(
                inL * dryLevel_ + (chorusL + chorusR) * 0.5f * wetLevel_);
        }
    }
}

std::vector<AudioEffectParameter> ChorusEffect::getUiParameters() const {
    return {
        {"rate",      "Rate (Hz)",    AudioEffectParameterType::Float, 0.1f,  5.0f,  0.8f},
        {"depth",     "Depth",        AudioEffectParameterType::Float, 0.0f,  1.0f,  0.5f},
        {"delay",     "Delay (ms)",   AudioEffectParameterType::Float, 1.0f,  30.0f, 7.0f},
        {"feedback",  "Feedback",     AudioEffectParameterType::Float, 0.0f,  0.7f,  0.1f},
        {"wet_level", "Wet Level",    AudioEffectParameterType::Float, 0.0f,  1.0f,  0.5f},
        {"dry_level", "Dry Level",    AudioEffectParameterType::Float, 0.0f,  1.0f,  0.5f},
    };
}

void ChorusEffect::setParameter(const String& name, float value) {
    if      (name == "rate")      rate_     = std::clamp(finiteOr(value, 0.8f), 0.1f, 5.0f);
    else if (name == "depth")     depth_    = std::clamp(finiteOr(value, 0.5f), 0.0f, 1.0f);
    else if (name == "delay")     delayMs_  = std::clamp(finiteOr(value, 7.0f), 1.0f, 30.0f);
    else if (name == "feedback")  feedback_ = std::clamp(finiteOr(value, 0.1f), 0.0f, 0.7f);
    else if (name == "wet_level") wetLevel_ = std::clamp(finiteOr(value, 0.5f), 0.0f, 1.0f);
    else if (name == "dry_level") dryLevel_ = std::clamp(finiteOr(value, 0.5f), 0.0f, 1.0f);

    initializeEngine();
}

float ChorusEffect::getParameter(const String& name) const {
    if      (name == "rate")      return rate_;
    else if (name == "depth")     return depth_;
    else if (name == "delay")     return delayMs_;
    else if (name == "feedback")  return feedback_;
    else if (name == "wet_level") return wetLevel_;
    else if (name == "dry_level") return dryLevel_;
    return 0.0f;
}

void ChorusEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 44100;
    initializeEngine();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createChorusEffect() {
    return std::make_unique<ChorusEffect>();
}

} // namespace Artifact
