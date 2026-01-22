module;
#include <cmath>
#include <vector>
#include <algorithm>
module Artifact.Audio.Effects.Reverb;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

namespace Artifact {

ReverbEffect::ReverbEffect() {
    // コムフィルターとオールパスフィルターを初期化
    combFilters_.resize(8);
    allpassFilters_.resize(4);
    updateFilterParameters();
}

ReverbEffect::~ReverbEffect() = default;

ArtifactCore::AudioSegment ReverbEffect::process(const ArtifactCore::AudioSegment& input) {
    if (!enabled_ || input.channelData.isEmpty()) {
        return input;
    }

    ArtifactCore::AudioSegment output = input;

    // 各チャンネルに対してエフェクトを適用
    for (int ch = 0; ch < output.channelData.size(); ++ch) {
        std::vector<float> processedData;
        processedData.reserve(output.channelData[ch].size());
        
        const auto& inputData = output.channelData[ch];
        
        for (float sample : inputData) {
            // コムフィルターを適用
            float combSum = 0.0f;
            for (auto& comb : combFilters_) {
                combSum += comb.process(sample);
            }
            combSum /= combFilters_.size();
            
            // オールパスフィルターを適用
            float reverbSignal = combSum;
            for (auto& allpass : allpassFilters_) {
                reverbSignal = allpass.process(reverbSignal);
            }
            
            // ドライ/ウェットミックス
            float result = sample * dryLevel_ + reverbSignal * wetLevel_;
            processedData.push_back(result);
        }
        
        output.channelData[ch] = QVector<float>(processedData.begin(), processedData.end());
    }

    return output;
}

std::vector<AudioEffectParameter> ReverbEffect::getParameters() const {
    return {
        {"room_size", "Room Size", AudioEffectParameterType::Float, 0.1f, 1.0f, 0.5f},
        {"damping", "Damping", AudioEffectParameterType::Float, 0.0f, 1.0f, 0.5f},
        {"wet_level", "Wet Level", AudioEffectParameterType::Float, 0.0f, 1.0f, 0.3f},
        {"dry_level", "Dry Level", AudioEffectParameterType::Float, 0.0f, 1.0f, 0.7f}
    };
}

void ReverbEffect::setParameter(const std::string& name, float value) {
    if (name == "room_size") roomSize_ = value;
    else if (name == "damping") damping_ = value;
    else if (name == "wet_level") wetLevel_ = value;
    else if (name == "dry_level") dryLevel_ = value;
    
    updateFilterParameters();
}

float ReverbEffect::getParameter(const std::string& name) const {
    if (name == "room_size") return roomSize_;
    else if (name == "damping") return damping_;
    else if (name == "wet_level") return wetLevel_;
    else if (name == "dry_level") return dryLevel_;
    return 0.0f;
}

void ReverbEffect::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
    updateFilterParameters();
}

void ReverbEffect::updateFilterParameters() {
    // コムフィルターの遅延時間（ミリ秒）
    const std::vector<int> combDelays = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    
    for (size_t i = 0; i < combFilters_.size(); ++i) {
        int delaySamples = static_cast<int>(combDelays[i] * roomSize_ * sampleRate_ / 1000.0f);
        combFilters_[i].delayLine.resize(std::max(1, delaySamples));
        combFilters_[i].feedback = std::pow(0.001f, combDelays[i] / (sampleRate_ * roomSize_)) * (1.0f - damping_);
    }
    
    // オールパスフィルターの遅延時間
    const std::vector<int> allpassDelays = {225, 556, 441, 341};
    
    for (size_t i = 0; i < allpassFilters_.size(); ++i) {
        int delaySamples = static_cast<int>(allpassDelays[i] * sampleRate_ / 1000.0f);
        allpassFilters_[i].delayLine.resize(std::max(1, delaySamples));
        allpassFilters_[i].feedback = 0.5f;
    }
}

std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect() {
    return std::make_unique<ReverbEffect>();
}

} // namespace Artifact