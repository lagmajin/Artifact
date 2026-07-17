module;
#include <QList>
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Chorus;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;

namespace Artifact {

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
    int numSamples  = (numChannels > 0) ? static_cast<int>(segment.channelData[0].size()) : 0;
    if (numSamples == 0) return;

    float centerDelaySamples = delayMs_ * 0.001f * sr;
    float depthSamples = depth_ * centerDelaySamples * 0.5f;

    for (int i = 0; i < numSamples; ++i) {
        float inL = segment.channelData[0][i];
        float inR = (numChannels > 1) ? segment.channelData[1][i] : inL;

        float chorusL = 0.0f;
        float chorusR = 0.0f;

        for (int v = 0; v < kNumVoices; ++v) {
            float modL = lfoL_[v].process() * depthSamples;
            float modR = lfoR_[v].process() * depthSamples;

            float readDelayL = centerDelaySamples + modL;
            float readDelayR = centerDelaySamples + modR;

            if (readDelayL < 1.0f) readDelayL = 1.0f;
            if (readDelayR < 1.0f) readDelayR = 1.0f;

            float delL = delayL_[v].read(readDelayL);
            float delR = delayR_[v].read(readDelayR);

            chorusL += delL;
            chorusR += delR;

            delayL_[v].write(inL + delL * feedback_);
            delayR_[v].write(inR + delR * feedback_);
        }

        float voiceScale = 1.0f / static_cast<float>(kNumVoices);
        chorusL *= voiceScale;
        chorusR *= voiceScale;

        if (numChannels >= 2) {
            segment.channelData[0][i] = inL * dryLevel_ + chorusL * wetLevel_;
            segment.channelData[1][i] = inR * dryLevel_ + chorusR * wetLevel_;
        } else {
            segment.channelData[0][i] = inL * dryLevel_ + (chorusL + chorusR) * 0.5f * wetLevel_;
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

void ChorusEffect::setParameter(const std::string& name, float value) {
    if      (name == "rate")      rate_     = value;
    else if (name == "depth")     depth_    = value;
    else if (name == "delay")     delayMs_  = value;
    else if (name == "feedback")  feedback_ = std::min(value, 0.7f);
    else if (name == "wet_level") wetLevel_ = value;
    else if (name == "dry_level") dryLevel_ = value;

    initializeEngine();
}

float ChorusEffect::getParameter(const std::string& name) const {
    if      (name == "rate")      return rate_;
    else if (name == "depth")     return depth_;
    else if (name == "delay")     return delayMs_;
    else if (name == "feedback")  return feedback_;
    else if (name == "wet_level") return wetLevel_;
    else if (name == "dry_level") return dryLevel_;
    return 0.0f;
}

void ChorusEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    initializeEngine();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createChorusEffect() {
    return std::make_unique<ChorusEffect>();
}

} // namespace Artifact
