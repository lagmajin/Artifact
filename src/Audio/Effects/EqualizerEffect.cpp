module;
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Equalizer;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

namespace Artifact {

EqualizerEffect::EqualizerEffect() {
    // デフォルトのバンド設定
    bands_ = {
        {60.0f, 0.0f, 1.0f},    // Low
        {250.0f, 0.0f, 1.0f},   // Low-Mid
        {1000.0f, 0.0f, 1.0f},  // Mid
        {4000.0f, 0.0f, 1.0f},  // High-Mid
        {10000.0f, 0.0f, 1.0f}  // High
    };
}

ArtifactCore::AudioSegment EqualizerEffect::process(const ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;

    // 各チャンネルに対してエフェクトを適用
    for (int ch = 0; ch < output.channelData.size(); ++ch) {
        std::vector<float> channelData(output.channelData[ch].begin(), 
                                      output.channelData[ch].end());
        
        // 各バンドのフィルタを適用
        for (const auto& band : bands_) {
            if (std::abs(band.gain) > 0.001f) {
                applyBandFilter(channelData, band);
            }
        }
        
        // 処理後のデータを設定
        output.channelData[ch] = QVector<float>(channelData.begin(), channelData.end());
    }

    return output;
}

std::vector<AudioEffectParameter> EqualizerEffect::getParameters() const {
    std::vector<AudioEffectParameter> params;
    
    const std::vector<std::string> bandNames = {
        "Low", "LowMid", "Mid", "HighMid", "High"
    };
    
    for (size_t i = 0; i < bands_.size() && i < bandNames.size(); ++i) {
        AudioEffectParameter param;
        param.name = "gain_" + bandNames[i];
        param.displayName = bandNames[i] + " Gain";
        param.type = AudioEffectParameterType::Float;
        param.minValue = -12.0f;
        param.maxValue = 12.0f;
        param.defaultValue = 0.0f;
        params.push_back(param);
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

void EqualizerEffect::applyBandFilter(std::vector<float>& channelData, const Band& band) {
    if (channelData.empty() || std::abs(band.gain) < 0.001f) {
        return;
    }

    float a0, a1, a2, b0, b1, b2;
    calculateBiquadCoefficients(band.frequency, band.gain, band.q, 
                               a0, a1, a2, b0, b1, b2);

    // バイクアッドフィルタの適用
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    
    for (size_t i = 0; i < channelData.size(); ++i) {
        float x0 = channelData[i];
        float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        
        channelData[i] = y0;
        
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;
    }
}

void EqualizerEffect::calculateBiquadCoefficients(float frequency, float gain, float q,
                                                float& a0, float& a1, float& a2,
                                                float& b0, float& b1, float& b2) {
    const float sampleRate = static_cast<float>(sampleRate_);
    const float omega = 2.0f * M_PI * frequency / sampleRate;
    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);
    const float alpha = sinOmega / (2.0f * q);
    
    const float A = std::pow(10.0f, gain / 40.0f);
    
    b0 = 1.0f + alpha * A;
    b1 = -2.0f * cosOmega;
    b2 = 1.0f - alpha * A;
    a0 = 1.0f + alpha / A;
    a1 = -2.0f * cosOmega;
    a2 = 1.0f - alpha / A;
    
    // 正規化
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

std::unique_ptr<ArtifactAbstractAudioEffect> createEqualizerEffect() {
    return std::make_unique<EqualizerEffect>();
}

} // namespace Artifact