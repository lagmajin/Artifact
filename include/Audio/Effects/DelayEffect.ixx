module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Delay;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;

export namespace Artifact {

/**
 * @brief Stereo Delay effect with ping-pong mode, tempo sync capability,
 * and high-cut filtering in the feedback path.
 */
class DelayEffect : public ArtifactAbstractAudioEffect {
public:
    DelayEffect();
    ~DelayEffect() override = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Stereo Delay"; }
    std::string getDescription() const override {
        return "Stereo delay with ping-pong mode and filtered feedback";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    ArtifactCore::Audio::DSP::FractionalDelayLine delayL_;
    ArtifactCore::Audio::DSP::FractionalDelayLine delayR_;

    float delayTimeL_  = 375.0f;   // ms
    float delayTimeR_  = 375.0f;   // ms
    float feedback_    = 0.4f;     // 0..0.95
    float wetLevel_    = 0.3f;
    float dryLevel_    = 0.7f;
    float highCut_     = 0.3f;     // 0..1 (amount of high-freq damping in feedback)
    bool  pingPong_    = false;

    // Feedback filter state
    float fbFilterStateL_ = 0.0f;
    float fbFilterStateR_ = 0.0f;

    void initializeDelays();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect();

} // namespace Artifact
