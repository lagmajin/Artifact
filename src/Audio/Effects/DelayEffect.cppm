module;
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>
#include <QList>
module Artifact.Audio.Effects.Delay;

import Audio.Segment;
import Audio.DSP.DelayLine;

namespace Artifact {

namespace {
float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float sanitizeDelaySample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}
}

DelayEffect::DelayEffect() {
    initializeDelays();
}

void DelayEffect::initializeDelays() {
    float sr = static_cast<float>(sampleRate_);
    delayL_.initialize(2.0f, sr);
    delayR_.initialize(2.0f, sr);
    fbFilterStateL_ = 0.0f;
    fbFilterStateR_ = 0.0f;
}

void DelayEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
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

    float delaySamplesL = delayTimeL_ * 0.001f * sr;
    float delaySamplesR = delayTimeR_ * 0.001f * sr;
    float dampCoeff = highCut_ * 0.6f;

    for (int i = 0; i < numSamples; ++i) {
        float inL = segment.channelData[0][i];
        float inR = (numChannels > 1) ? segment.channelData[1][i] : inL;
        inL = finiteOr(inL, 0.0f);
        inR = finiteOr(inR, inL);

        float delayedL = sanitizeDelaySample(delayL_.read(delaySamplesL));
        float delayedR = sanitizeDelaySample(delayR_.read(delaySamplesR));

        fbFilterStateL_ = sanitizeDelaySample(
            fbFilterStateL_ + dampCoeff * (delayedL - fbFilterStateL_));
        fbFilterStateR_ = sanitizeDelaySample(
            fbFilterStateR_ + dampCoeff * (delayedR - fbFilterStateR_));

        float filteredFbL = sanitizeDelaySample(delayedL - fbFilterStateL_ * dampCoeff);
        float filteredFbR = sanitizeDelaySample(delayedR - fbFilterStateR_ * dampCoeff);

        if (pingPong_) {
            delayL_.write(sanitizeDelaySample(inL + filteredFbR * feedback_));
            delayR_.write(sanitizeDelaySample(inR + filteredFbL * feedback_));
        } else {
            delayL_.write(sanitizeDelaySample(inL + filteredFbL * feedback_));
            delayR_.write(sanitizeDelaySample(inR + filteredFbR * feedback_));
        }

        if (numChannels >= 2) {
            segment.channelData[0][i] = sanitizeDelaySample(
                inL * dryLevel_ + delayedL * wetLevel_);
            segment.channelData[1][i] = sanitizeDelaySample(
                inR * dryLevel_ + delayedR * wetLevel_);
        } else {
            segment.channelData[0][i] = sanitizeDelaySample(
                inL * dryLevel_ + (delayedL + delayedR) * 0.5f * wetLevel_);
        }
    }
}

std::vector<AudioEffectParameter> DelayEffect::getUiParameters() const {
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

void DelayEffect::setParameter(const String& name, float value) {
    if      (name == "delay_l")   delayTimeL_ = std::clamp(finiteOr(value, 375.0f), 1.0f, 2000.0f);
    else if (name == "delay_r")   delayTimeR_ = std::clamp(finiteOr(value, 375.0f), 1.0f, 2000.0f);
    else if (name == "feedback")  feedback_   = std::clamp(finiteOr(value, 0.4f), 0.0f, 0.95f);
    else if (name == "high_cut")  highCut_    = std::clamp(finiteOr(value, 0.3f), 0.0f, 1.0f);
    else if (name == "wet_level") wetLevel_   = std::clamp(finiteOr(value, 0.3f), 0.0f, 1.0f);
    else if (name == "dry_level") dryLevel_   = std::clamp(finiteOr(value, 0.7f), 0.0f, 1.0f);
    else if (name == "ping_pong") pingPong_   = (value > 0.5f);
}

float DelayEffect::getParameter(const String& name) const {
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
    sampleRate_ = sampleRate > 0 ? sampleRate : 44100;
    initializeDelays();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect() {
    return std::make_unique<DelayEffect>();
}

} // namespace Artifact
