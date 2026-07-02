module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Equalizer;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class EqualizerEffect : public ArtifactAbstractAudioEffect {
public:
    EqualizerEffect();
    ~EqualizerEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Equalizer"; }
    std::string getDescription() const override { return "Multi-band equalizer effect"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

private:
    struct Band {
        float frequency;
        float gain;
        float q;
    };

    std::vector<Band> bands_;
    float sampleRate_ = 44100.0f;

    void applyBandFilter(std::vector<float>& channelData, const Band& band);
    void calculateBiquadCoefficients(float frequency, float gain, float q,
                                   float& a0, float& a1, float& a2,
                                   float& b0, float& b1, float& b2);
};

std::unique_ptr<ArtifactAbstractAudioEffect> createEqualizerEffect();

} // namespace Artifact
