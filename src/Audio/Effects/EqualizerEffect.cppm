module;
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#define M_PI 3.14159265358979323846
module Artifact.Audio.Effects.Equalizer;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

namespace Artifact {

EqualizerEffect::EqualizerEffect() : sampleRate_(44100.0f) {
    bands_ = {
        {60.0f, 0.0f, 1.0f},
        {250.0f, 0.0f, 1.0f},
        {1000.0f, 0.0f, 1.0f},
        {4000.0f, 0.0f, 1.0f},
        {10000.0f, 0.0f, 1.0f}
    };
}

void EqualizerEffect::process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment*) {
    if (!enabled_) return;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    if (frames <= 0 || channels <= 0) return;

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= static_cast<int>(segment.channelData.size())) break;
        auto& channelData = segment.channelData[ch];

        for (const auto& band : bands_) {
            if (std::abs(band.gain) > 0.001f) {
                float a0, a1, a2, b0, b1, b2;
                calculateBiquadCoefficients(band.frequency, band.gain, band.q,
                                           a0, a1, a2, b0, b1, b2);

                float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
                for (int i = 0; i < frames; ++i) {
                    float x0 = channelData[i];
                    float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    channelData[i] = y0;
                    x2 = x1; x1 = x0;
                    y2 = y1; y1 = y0;
                }
            }
        }
    }
}

std::vector<AudioEffectParameter> EqualizerEffect::getUiParameters() const {
    std::vector<AudioEffectParameter> params;
    const std::vector<std::string> bandNames = {"Low", "LowMid", "Mid", "HighMid", "High"};
    for (size_t i = 0; i < bands_.size() && i < bandNames.size(); ++i) {
        params.push_back({
            "gain_" + bandNames[i],
            bandNames[i] + " Gain",
            AudioEffectParameterType::Float,
            -12.0f, 12.0f, 0.0f
        });
    }
    return params;
}

void EqualizerEffect::setParameter(const std::string& name, float value) {
    if (name == "gain_Low" && bands_.size() > 0) bands_[0].gain = value;
    else if (name == "gain_LowMid" && bands_.size() > 1) bands_[1].gain = value;
    else if (name == "gain_Mid" && bands_.size() > 2) bands_[2].gain = value;
    else if (name == "gain_HighMid" && bands_.size() > 3) bands_[3].gain = value;
    else if (name == "gain_High" && bands_.size() > 4) bands_[4].gain = value;
}

float EqualizerEffect::getParameter(const std::string& name) const {
    if (name == "gain_Low" && bands_.size() > 0) return bands_[0].gain;
    else if (name == "gain_LowMid" && bands_.size() > 1) return bands_[1].gain;
    else if (name == "gain_Mid" && bands_.size() > 2) return bands_[2].gain;
    else if (name == "gain_HighMid" && bands_.size() > 3) return bands_[3].gain;
    else if (name == "gain_High" && bands_.size() > 4) return bands_[4].gain;
    return 0.0f;
}

void EqualizerEffect::calculateBiquadCoefficients(float frequency, float gain, float q,
                                                  float& a0, float& a1, float& a2,
                                                  float& b0, float& b1, float& b2)
{
    const float sr = std::max(1.0f, sampleRate_);
    const float nyquistSafeFreq = std::clamp(frequency, 1.0f, sr * 0.49f);
    const float A = std::pow(10.0f, gain / 40.0f);
    const float omega = 2.0f * static_cast<float>(M_PI) * (nyquistSafeFreq / sr);
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * std::max(0.001f, q));

    b0 = 1.0f + alpha * A;
    b1 = -2.0f * cs;
    b2 = 1.0f - alpha * A;
    a0 = 1.0f + alpha / A;
    a1 = -2.0f * cs;
    a2 = 1.0f - alpha / A;

    const float invA0 = 1.0f / a0;
    b0 *= invA0;
    b1 *= invA0;
    b2 *= invA0;
    a1 *= invA0;
    a2 *= invA0;
    a0 = 1.0f;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createEqualizerEffect() {
    return std::make_unique<EqualizerEffect>();
}

} // namespace Artifact
