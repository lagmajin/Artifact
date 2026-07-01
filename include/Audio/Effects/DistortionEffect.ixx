module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>

export module Artifact.Audio.Effects.Distortion;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class DistortionEffect : public ArtifactAbstractAudioEffect {
public:
    DistortionEffect() = default;
    ~DistortionEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Distortion"; }
    std::string getDescription() const override {
        return "Multi-mode distortion with soft clip, tube, foldback, and bitcrush";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    enum class Mode {
        SoftClip = 0,
        HardClip,
        Tube,
        Foldback,
        Bitcrush
    };

private:
    Mode  mode_       = Mode::SoftClip;
    float drive_      = 1.0f;
    float tone_       = 0.5f;
    float mix_        = 1.0f;
    float outputGain_ = 0.0f;
    float bitDepth_   = 8.0f;
    float downsample_ = 4.0f;

    float toneStateL_ = 0.0f;
    float toneStateR_ = 0.0f;
    float holdL_ = 0.0f;
    float holdR_ = 0.0f;
    float holdCounter_ = 0.0f;

    static float softClip(float x);
    static float hardClip(float x);
    static float tubeSaturate(float x);
    static float foldback(float x);
    float bitcrush(float x, float& holdState);
};

std::unique_ptr<ArtifactAbstractAudioEffect> createDistortionEffect();

} // namespace Artifact