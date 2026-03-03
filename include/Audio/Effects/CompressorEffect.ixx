module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Compressor;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

/**
 * @brief DAW-grade Dynamic Range Compressor with lookahead envelope follower.
 * Features soft-knee compression, auto make-up gain, and smooth attack/release.
 */
class CompressorEffect : public ArtifactAbstractAudioEffect {
public:
    CompressorEffect() = default;
    ~CompressorEffect() override = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Compressor"; }
    std::string getDescription() const override {
        return "Dynamic range compressor with soft-knee and auto make-up gain";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

private:
    float threshold_   = -20.0f;  // dB
    float ratio_       = 4.0f;    // compression ratio (1:ratio)
    float attackMs_    = 10.0f;   // ms
    float releaseMs_   = 100.0f;  // ms
    float kneeWidth_   = 6.0f;    // dB (soft knee width)
    float makeupGain_  = 0.0f;    // dB (auto or manual)
    bool  autoMakeup_  = true;

    // Envelope follower state
    float envelopeDb_ = -96.0f;
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createCompressorEffect();

} // namespace Artifact
