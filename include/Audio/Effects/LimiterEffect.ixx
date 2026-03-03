module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Limiter;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

/**
 * @brief Brick-wall Lookahead Limiter.
 * Prevents output from exceeding the ceiling level.
 * Uses lookahead + smooth gain reduction for transparent limiting.
 */
class LimiterEffect : public ArtifactAbstractAudioEffect {
public:
    LimiterEffect();
    ~LimiterEffect() override = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Limiter"; }
    std::string getDescription() const override {
        return "Brick-wall lookahead limiter with transparent gain reduction";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    float ceiling_    = -0.3f;   // dB (output ceiling)
    float releaseMs_  = 50.0f;   // ms
    float inputGain_  = 0.0f;    // dB (drive into limiter)

    // Lookahead buffer (circular)
    static constexpr int kMaxLookahead = 512;
    std::vector<std::vector<float>> lookaheadBuf_;
    int lookaheadWritePos_ = 0;
    int lookaheadSamples_  = 64;  // ~1.3ms at 48kHz

    // Gain reduction state
    float currentGain_ = 1.0f;

    void initializeEngine();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect();

} // namespace Artifact
