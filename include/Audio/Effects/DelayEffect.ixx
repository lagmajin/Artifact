module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Delay;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class DelayEffect : public ArtifactAbstractAudioEffect {
public:
    DelayEffect();
    ~DelayEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Stereo Delay"; }
    std::string getDescription() const override {
        return "Stereo delay with ping-pong mode and filtered feedback";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    ArtifactCore::Audio::DSP::FractionalDelayLine delayL_;
    ArtifactCore::Audio::DSP::FractionalDelayLine delayR_;

    float delayTimeL_  = 375.0f;
    float delayTimeR_  = 375.0f;
    float feedback_    = 0.4f;
    float wetLevel_    = 0.3f;
    float dryLevel_    = 0.7f;
    float highCut_     = 0.3f;
    bool  pingPong_    = false;

    float fbFilterStateL_ = 0.0f;
    float fbFilterStateR_ = 0.0f;

    void initializeDelays();
};

std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect();

} // namespace Artifact
