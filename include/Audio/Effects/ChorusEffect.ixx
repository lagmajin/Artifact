module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Chorus;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class ChorusEffect : public ArtifactAbstractAudioEffect {
public:
    ChorusEffect();
    ~ChorusEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Chorus"; }
    std::string getDescription() const override {
        return "Rich stereo chorus with multiple modulated voices";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void setSampleRate(int sampleRate) override;

private:
    static constexpr int kNumVoices = 3;

    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayR_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoR_[kNumVoices];

    float rate_      = 0.8f;
    float depth_     = 0.5f;
    float delayMs_   = 7.0f;
    float wetLevel_  = 0.5f;
    float dryLevel_  = 0.5f;
    float feedback_  = 0.1f;

    void initializeEngine();
};

std::unique_ptr<ArtifactAbstractAudioEffect> createChorusEffect();

} // namespace Artifact
