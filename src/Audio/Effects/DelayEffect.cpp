module;
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Delay;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;

namespace Artifact {

DelayEffect::DelayEffect() {
    initializeDelays();
}

void DelayEffect::initializeDelays() {
    float sr = static_cast<float>(sampleRate_);
    // Max 2 seconds of delay
    delayL_.initialize(2.0f, sr);
    delayR_.initialize(2.0f, sr);
    fbFilterStateL_ = 0.0f;
    fbFilterStateR_ = 0.0f;
}

ArtifactCore::AudioSegment DelayEffect::process(const ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;
    float sr = static_cast<float>(sampleRate_);

    int numChannels = output.channelData.size();
    int numSamples  = (numChannels > 0) ? output.channelData[0].size() : 0;
    if (numSamples == 0) return output;

    float delaySamplesL = delayTimeL_ * 0.001f * sr;
    float delaySamplesR = delayTimeR_ * 0.001f * sr;
    float dampCoeff = highCut_ * 0.6f; // Maps 0..1 -> 0..0.6

    for (int i = 0; i < numSamples; ++i) {
        // Get input (mono for single channel, stereo for 2+)
        float inL = input.channelData[0][i];
        float inR = (numChannels > 1) ? input.channelData[1][i] : inL;

        // Read from delay lines
        float delayedL = delayL_.read(delaySamplesL);
        float delayedR = delayR_.read(delaySamplesR);

        // Apply high-cut filter in feedback path (one-pole LPF)
        fbFilterStateL_ = fbFilterStateL_ + dampCoeff * (delayedL - fbFilterStateL_);
        fbFilterStateR_ = fbFilterStateR_ + dampCoeff * (delayedR - fbFilterStateR_);

        float filteredFbL = delayedL - fbFilterStateL_ * dampCoeff;
        float filteredFbR = delayedR - fbFilterStateR_ * dampCoeff;

        // Write to delay lines with feedback
        if (pingPong_) {
            // Ping-pong: L feeds R, R feeds L
            delayL_.write(inL + filteredFbR * feedback_);
            delayR_.write(inR + filteredFbL * feedback_);
        } else {
            // Normal stereo delay
            delayL_.write(inL + filteredFbL * feedback_);
            delayR_.write(inR + filteredFbR * feedback_);
        }

        // Mix output
        if (numChannels >= 2) {
            output.channelData[0][i] = inL * dryLevel_ + delayedL * wetLevel_;
            output.channelData[1][i] = inR * dryLevel_ + delayedR * wetLevel_;
        } else {
            output.channelData[0][i] = inL * dryLevel_ + (delayedL + delayedR) * 0.5f * wetLevel_;
        }
    }

    return output;
}

std::vector<AudioEffectParameter> DelayEffect::getParameters() const {
    return {
        {"delay_l",    "Delay L (ms)",  AudioEffectParameterType::Float, 1.0f,   2000.0f, 375.0f},
        {"delay_r",    "Delay R (ms)",  AudioEffectParameterType::Float, 1.0f,   2000.0f, 375.0f},
        {"feedback",   "Feedback",      AudioEffectParameterType::Float, 0.0f,   0.95f,   0.4f},
        {"high_cut",   "High Cut",      AudioEffectParameterType::Float, 0.0f,   1.0f,    0.3f},
        {"wet_level",  "Wet Level",     AudioEffectParameterType::Float, 0.0f,   1.0f,    0.3f},
        {"dry_level",  "Dry Level",     AudioEffectParameterType::Float, 0.0f,   1.0f,    0.7f},
        {"ping_pong",  "Ping-Pong",     AudioEffectParameterType::Bool,  0.0f,   1.0f,    0.0f},
    };
}

void DelayEffect::setParameter(const std::string& name, float value) {
    if      (name == "delay_l")   delayTimeL_ = value;
    else if (name == "delay_r")   delayTimeR_ = value;
    else if (name == "feedback")  feedback_   = std::min(value, 0.95f);
    else if (name == "high_cut")  highCut_    = value;
    else if (name == "wet_level") wetLevel_   = value;
    else if (name == "dry_level") dryLevel_   = value;
    else if (name == "ping_pong") pingPong_   = (value > 0.5f);
}

float DelayEffect::getParameter(const std::string& name) const {
    if      (name == "delay_l")   return delayTimeL_;
    else if (name == "delay_r")   return delayTimeR_;
    else if (name == "feedback")  return feedback_;
    else if (name == "high_cut")  return highCut_;
    else if (name == "wet_level") return wetLevel_;
    else if (name == "dry_level") return dryLevel_;
    else if (name == "ping_pong") return pingPong_ ? 1.0f : 0.0f;
    return 0.0f;
}

void DelayEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    initializeDelays();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect() {
    return std::make_unique<DelayEffect>();
}

} // namespace Artifact
