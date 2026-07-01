module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Compressor;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class CompressorEffect : public ArtifactAbstractAudioEffect {
public:
    CompressorEffect() = default;
    ~CompressorEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Compressor"; }
    std::string getDescription() const override {
        return "Dynamic range compressor with soft-knee and auto make-up gain";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

private:
    float threshold_   = -20.0f;
    float ratio_       = 4.0f;
    float attackMs_    = 10.0f;
    float releaseMs_   = 100.0f;
    float kneeWidth_   = 6.0f;
    float makeupGain_  = 0.0f;
    bool  autoMakeup_  = true;

    float envelopeDb_  = -96.0f;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createCompressorEffect();

} // namespace Artifact