module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <QList>
module Artifact.Audio.Effects.Distortion;

import Audio.Segment;

namespace Artifact {

// ============================
// Waveshaping Functions
// ============================

float DistortionEffect::softClip(float x) {
    // Tanh soft saturation - warm, musical clipping
    return std::tanh(x);
}

float DistortionEffect::hardClip(float x) {
    // Digital hard clip at +/- 1.0
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

float DistortionEffect::tubeSaturate(float x) {
    // Asymmetric tube-style saturation
    // Positive half: soft saturation, Negative half: harder compression
    if (x >= 0.0f) {
        // Soft positive clip (tube glow)
        return 1.0f - std::exp(-x);
    } else {
        // Harder negative clip (tube bias)
        float absX = -x;
        float sat = 1.0f - std::exp(-absX * 1.5f);
        return -sat * 0.8f; // Asymmetric: negative side is slightly quieter
    }
}

float DistortionEffect::foldback(float x) {
    // Wavefolding: folds the signal back when it exceeds +/- 1.0
    // Creates complex harmonics, great for sound design
    while (x > 1.0f || x < -1.0f) {
        if (x > 1.0f) {
            x = 2.0f - x;
        }
        if (x < -1.0f) {
            x = -2.0f - x;
        }
    }
    return x;
}

float DistortionEffect::bitcrush(float x, float& holdState) {
    // Bit-depth reduction
    float levels = std::pow(2.0f, bitDepth_);
    float crushed = std::round(x * levels) / levels;
    return crushed;
}

// ============================
// Main Processing
// ============================

ArtifactCore::AudioSegment DistortionEffect::process(const ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;

    int numChannels = static_cast<int>(output.channelData.size());
    int numSamples  = (numChannels > 0) ? static_cast<int>(output.channelData[0].size()) : 0;
    if (numSamples == 0) return output;

    float outputGainLinear = std::pow(10.0f, outputGain_ / 20.0f);
    float toneCoeff = tone_ * 0.8f; // Map 0..1 to LP amount (0 = bright, 1 = dark)

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < numChannels; ++ch) {
            float dry = input.channelData[ch][i];
            float driven = dry * drive_;
            float shaped;

            switch (mode_) {
                case Mode::SoftClip:
                    shaped = softClip(driven);
                    break;
                case Mode::HardClip:
                    shaped = hardClip(driven);
                    break;
                case Mode::Tube:
                    shaped = tubeSaturate(driven);
                    break;
                case Mode::Foldback:
                    shaped = foldback(driven);
                    break;
                case Mode::Bitcrush: {
                    // Sample-rate reduction via sample-and-hold
                    holdCounter_ += 1.0f;
                    if (holdCounter_ >= downsample_) {
                        holdCounter_ = 0.0f;
                        if (ch == 0) holdL_ = bitcrush(driven, holdL_);
                        else         holdR_ = bitcrush(driven, holdR_);
                    }
                    shaped = (ch == 0) ? holdL_ : holdR_;
                    break;
                }
                default:
                    shaped = softClip(driven);
                    break;
            }

            // Post-distortion tone filter (one-pole LPF)
            float& toneState = (ch == 0) ? toneStateL_ : toneStateR_;
            toneState = toneState + toneCoeff * (shaped - toneState);
            float toned = shaped - toneCoeff * toneState; // High-pass blend

            // When tone = 0: bright (no filtering), tone = 1: dark (filtered)
            float filtered = shaped * (1.0f - tone_) + toneState * tone_;

            // Dry/wet mix + output gain
            float result = (dry * (1.0f - mix_) + filtered * mix_) * outputGainLinear;

            output.channelData[ch][i] = result;
        }
    }

    return output;
}

std::vector<AudioEffectParameter> DistortionEffect::getParameters() const {
    return {
        {"mode",        "Mode",             AudioEffectParameterType::Enum,  0.0f, 4.0f,  0.0f,
            {"Soft Clip", "Hard Clip", "Tube", "Foldback", "Bitcrush"}},
        {"drive",       "Drive",            AudioEffectParameterType::Float, 1.0f, 50.0f, 1.0f},
        {"tone",        "Tone",             AudioEffectParameterType::Float, 0.0f, 1.0f,  0.5f},
        {"mix",         "Mix",              AudioEffectParameterType::Float, 0.0f, 1.0f,  1.0f},
        {"output_gain", "Output Gain (dB)", AudioEffectParameterType::Float, -24.0f, 6.0f, 0.0f},
        {"bit_depth",   "Bit Depth",        AudioEffectParameterType::Float, 1.0f, 16.0f, 8.0f},
        {"downsample",  "Downsample",       AudioEffectParameterType::Float, 1.0f, 32.0f, 4.0f},
    };
}

void DistortionEffect::setParameter(const std::string& name, float value) {
    if      (name == "mode")        mode_ = static_cast<Mode>(static_cast<int>(value));
    else if (name == "drive")       drive_ = value;
    else if (name == "tone")        tone_ = value;
    else if (name == "mix")         mix_ = value;
    else if (name == "output_gain") outputGain_ = value;
    else if (name == "bit_depth")   bitDepth_ = value;
    else if (name == "downsample")  downsample_ = std::max(1.0f, value);
}

float DistortionEffect::getParameter(const std::string& name) const {
    if      (name == "mode")        return static_cast<float>(static_cast<int>(mode_));
    else if (name == "drive")       return drive_;
    else if (name == "tone")        return tone_;
    else if (name == "mix")         return mix_;
    else if (name == "output_gain") return outputGain_;
    else if (name == "bit_depth")   return bitDepth_;
    else if (name == "downsample")  return downsample_;
    return 0.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createDistortionEffect() {
    return std::make_unique<DistortionEffect>();
}

} // namespace Artifact
