module;
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Reverb;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;
import Audio.DSP.AllPassFilter;
import Audio.DSP.LFO;

namespace Artifact {

ReverbEffect::ReverbEffect() {
    initializeEngine();
}

void ReverbEffect::initializeEngine() {
    float sr = static_cast<float>(sampleRate_);
    float maxDelay = 2.0f; // 2 seconds max

    // Pre-delay (up to 200ms)
    preDelay_.initialize(0.25f, sr);

    // Input Diffusion: 4 all-pass filters in series (2 pairs)
    for (int i = 0; i < 2; ++i) {
        inputDiffusion1_[i].initialize(0.05f, sr);
        inputDiffusion2_[i].initialize(0.05f, sr);
    }
    inputDiffusion1_[0].setParametersSamples(scaleDelay(kInputDiff1a), diffusion_ * 0.75f);
    inputDiffusion1_[1].setParametersSamples(scaleDelay(kInputDiff1b), diffusion_ * 0.75f);
    inputDiffusion2_[0].setParametersSamples(scaleDelay(kInputDiff2a), diffusion_ * 0.625f);
    inputDiffusion2_[1].setParametersSamples(scaleDelay(kInputDiff2b), diffusion_ * 0.625f);

    // Tank: two cross-coupled delay lines
    for (int i = 0; i < 2; ++i) {
        tankDelay_[i].initialize(maxDelay, sr);
        tankAllPass_[i].initialize(0.1f, sr);
    }
    tankAllPass_[0].setParametersSamples(scaleDelay(kTankAP1), -decay_ * 0.7f);
    tankAllPass_[1].setParametersSamples(scaleDelay(kTankAP2), -decay_ * 0.7f);

    // Tank LFOs for modulation (slightly detuned for stereo richness)
    tankLFO_[0].initialize(modRate_, sr);
    tankLFO_[1].initialize(modRate_ * 1.07f, sr); // Slightly different rate for L/R decorrelation

    // Reset state
    dampState_[0] = 0.0f;
    dampState_[1] = 0.0f;
    tankAccumL_ = 0.0f;
    tankAccumR_ = 0.0f;
}

ArtifactCore::AudioSegment ReverbEffect::process(const ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;
    float sr = static_cast<float>(sampleRate_);

    int numChannels = output.channelData.size();
    int numSamples  = (numChannels > 0) ? output.channelData[0].size() : 0;
    if (numSamples == 0) return output;

    // Work buffers for stereo output
    std::vector<float> wetL(numSamples, 0.0f);
    std::vector<float> wetR(numSamples, 0.0f);

    // Get mono input (average all channels)
    std::vector<float> monoInput(numSamples, 0.0f);
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            monoInput[i] += input.channelData[ch][i];
        }
    }
    float invChannels = 1.0f / static_cast<float>(numChannels);
    for (int i = 0; i < numSamples; ++i) monoInput[i] *= invChannels;

    float preDelaySamples = preDelayMs_ * 0.001f * sr;
    float modDepthSamples = modDepth_ * 12.0f; // Max 12 samples of modulation excursion
    float dampCoeff = damping_ * 0.4f + 0.05f; // Map 0..1 -> 0.05..0.45

    // Per-sample processing
    for (int i = 0; i < numSamples; ++i) {
        // 1. Pre-delay
        preDelay_.write(monoInput[i]);
        float sig = preDelay_.read(preDelaySamples);

        // 2. Input diffusion (4 series all-pass filters)
        sig = inputDiffusion1_[0].process(sig);
        sig = inputDiffusion1_[1].process(sig);
        sig = inputDiffusion2_[0].process(sig);
        sig = inputDiffusion2_[1].process(sig);

        // 3. Tank processing (two cross-coupled delay channels)
        // Read LFO for modulation
        float lfo0 = tankLFO_[0].process() * modDepthSamples;
        float lfo1 = tankLFO_[1].process() * modDepthSamples;

        // Tank Channel 0 (feeds into Channel 1)
        float tankRead0 = tankDelay_[0].read(scaleDelay(kTankDelay1) + lfo0);
        // Apply one-pole lowpass (damping)
        dampState_[0] = dampState_[0] + dampCoeff * (tankRead0 - dampState_[0]);
        float tankAPOut0 = tankAllPass_[0].process(dampState_[0], lfo0 * 0.3f);

        // Tank Channel 1 (feeds into Channel 0)
        float tankRead1 = tankDelay_[1].read(scaleDelay(kTankDelay2) + lfo1);
        dampState_[1] = dampState_[1] + dampCoeff * (tankRead1 - dampState_[1]);
        float tankAPOut1 = tankAllPass_[1].process(dampState_[1], lfo1 * 0.3f);

        // Cross-couple: write input + decayed signal from opposite channel
        tankDelay_[0].write(sig + tankAPOut1 * decay_);
        tankDelay_[1].write(sig + tankAPOut0 * decay_);

        // 4. Tap output from tank (multiple taps for dense reverb tail)
        wetL[i] = tankAPOut0 * 0.6f + tankRead1 * 0.4f;
        wetR[i] = tankAPOut1 * 0.6f + tankRead0 * 0.4f;
    }

    // 5. Mix dry/wet into output channels
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            float dry = input.channelData[ch][i] * dryLevel_;
            float wet;
            if (ch % 2 == 0) {
                wet = wetL[i] * wetLevel_;
            } else {
                wet = wetR[i] * wetLevel_;
            }
            output.channelData[ch][i] = dry + wet;
        }
    }

    return output;
}

std::vector<AudioEffectParameter> ReverbEffect::getParameters() const {
    return {
        {"decay",      "Decay",      AudioEffectParameterType::Float, 0.1f,  0.99f, 0.75f},
        {"pre_delay",  "Pre-Delay",  AudioEffectParameterType::Float, 0.0f,  200.0f, 20.0f},
        {"damping",    "Damping",    AudioEffectParameterType::Float, 0.0f,  1.0f,  0.5f},
        {"diffusion",  "Diffusion",  AudioEffectParameterType::Float, 0.0f,  1.0f,  0.75f},
        {"mod_depth",  "Mod Depth",  AudioEffectParameterType::Float, 0.0f,  1.0f,  0.5f},
        {"mod_rate",   "Mod Rate",   AudioEffectParameterType::Float, 0.1f,  3.0f,  0.8f},
        {"size",       "Size",       AudioEffectParameterType::Float, 0.5f,  2.0f,  1.0f},
        {"wet_level",  "Wet Level",  AudioEffectParameterType::Float, 0.0f,  1.0f,  0.35f},
        {"dry_level",  "Dry Level",  AudioEffectParameterType::Float, 0.0f,  1.0f,  0.65f},
    };
}

void ReverbEffect::setParameter(const std::string& name, float value) {
    if      (name == "decay")      decay_      = value;
    else if (name == "pre_delay")  preDelayMs_ = value;
    else if (name == "damping")    damping_    = value;
    else if (name == "diffusion")  diffusion_  = value;
    else if (name == "mod_depth")  modDepth_   = value;
    else if (name == "mod_rate")   modRate_    = value;
    else if (name == "size")       size_       = value;
    else if (name == "wet_level")  wetLevel_   = value;
    else if (name == "dry_level")  dryLevel_   = value;

    // Re-initialize with new parameters
    initializeEngine();
}

float ReverbEffect::getParameter(const std::string& name) const {
    if      (name == "decay")      return decay_;
    else if (name == "pre_delay")  return preDelayMs_;
    else if (name == "damping")    return damping_;
    else if (name == "diffusion")  return diffusion_;
    else if (name == "mod_depth")  return modDepth_;
    else if (name == "mod_rate")   return modRate_;
    else if (name == "size")       return size_;
    else if (name == "wet_level")  return wetLevel_;
    else if (name == "dry_level")  return dryLevel_;
    return 0.0f;
}

void ReverbEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    initializeEngine();
}

std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect() {
    return std::make_unique<ReverbEffect>();
}

} // namespace Artifact