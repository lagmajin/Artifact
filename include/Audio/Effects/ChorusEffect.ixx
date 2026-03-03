module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Chorus;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;

export namespace Artifact {

/**
 * @brief Rich stereo Chorus effect using multiple modulated delay taps.
 * Can also behave as a Flanger at lower delay times.
 */
class ChorusEffect : public ArtifactAbstractAudioEffect {
public:
    ChorusEffect();
    ~ChorusEffect() override = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Chorus"; }
    std::string getDescription() const override {
        return "Rich stereo chorus with multiple modulated voices";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    static constexpr int kNumVoices = 3;

    ArtifactCore::Audio::DSP::FractionalDelayLine delayL_[kNumVoices];
    ArtifactCore::Audio::DSP::FractionalDelayLine delayR_[kNumVoices];
    ArtifactCore::Audio::DSP::LFO lfoL_[kNumVoices];
    ArtifactCore::Audio::DSP::LFO lfoR_[kNumVoices];

    float rate_      = 0.8f;   // Hz
    float depth_     = 0.5f;   // 0..1
    float delayMs_   = 7.0f;   // ms (center delay)
    float wetLevel_  = 0.5f;
    float dryLevel_  = 0.5f;
    float feedback_  = 0.1f;

    void initializeEngine();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createChorusEffect();

} // namespace Artifact
