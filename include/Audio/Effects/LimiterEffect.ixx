module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Limiter;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class LimiterEffect : public ArtifactAbstractAudioEffect {
public:
    LimiterEffect();
    ~LimiterEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Limiter"; }
    String getDescription() const override {
        return "Brick-wall lookahead limiter with transparent gain reduction";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    float ceiling_    = -0.3f;
    float releaseMs_  = 50.0f;
    float inputGain_  = 0.0f;

    static constexpr int kMaxLookahead = 512;
    std::vector<std::vector<float>> lookaheadBuf_;
    int lookaheadWritePos_ = 0;
    int lookaheadSamples_  = 64;

    float currentGain_ = 1.0f;

    void initializeEngine();
};

std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect();

} // namespace Artifact
