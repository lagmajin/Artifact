module;
#include <vector>
#include <string>
#include <memory>
export module Artifact.Audio.Effects.Equalizer;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class EqualizerEffect : public ArtifactAbstractAudioEffect {
public:
    EqualizerEffect();
    ~EqualizerEffect() override = default;

    // ArtifactAbstractAudioEffect interface
    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Equalizer"; }
    std::string getDescription() const override { return "Multi-band equalizer effect"; }
    
    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

private:
    struct Band {
        float frequency;
        float gain;
        float q;
    };

    std::vector<Band> bands_;
    
    // バンドパスフィルタの適用
    void applyBandFilter(std::vector<float>& channelData, const Band& band);

    // バイクアッドフィルタ係数の計算
    void calculateBiquadCoefficients(float frequency, float gain, float q,
                                   float& a0, float& a1, float& a2,
                                   float& b0, float& b1, float& b2);
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createEqualizerEffect();

} // namespace Artifact