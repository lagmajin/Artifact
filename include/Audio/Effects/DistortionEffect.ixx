module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Distortion;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

/**
 * @brief Multi-mode Distortion / Saturation effect.
 * Supports: Soft Clip (Tanh), Hard Clip, Tube Saturation, Foldback, Bitcrush.
 */
class DistortionEffect : public ArtifactAbstractAudioEffect {
public:
    DistortionEffect() = default;
    ~DistortionEffect() override = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Distortion"; }
    std::string getDescription() const override {
        return "Multi-mode distortion with soft clip, tube, foldback, and bitcrush";
    }

    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    enum class Mode {
        SoftClip = 0,   // tanh saturation
        HardClip,       // digital hard clip
        Tube,           // asymmetric tube emulation
        Foldback,       // wavefolding
        Bitcrush        // bit-depth + sample-rate reduction
    };

private:
    Mode  mode_       = Mode::SoftClip;
    float drive_      = 1.0f;    // 1.0 .. 50.0 (gain before waveshaper)
    float tone_       = 0.5f;    // 0..1 (post-distortion tone filter)
    float mix_        = 1.0f;    // 0..1 (dry/wet blend)
    float outputGain_ = 0.0f;    // dB (compensate volume)
    float bitDepth_   = 8.0f;    // for Bitcrush mode
    float downsample_ = 4.0f;    // for Bitcrush mode (factor)

    // One-pole tone filter state per channel
    float toneStateL_ = 0.0f;
    float toneStateR_ = 0.0f;

    // Bitcrush hold state
    float holdL_ = 0.0f;
    float holdR_ = 0.0f;
    float holdCounter_ = 0.0f;

    // Waveshaping functions
    static float softClip(float x);
    static float hardClip(float x);
    static float tubeSaturate(float x);
    static float foldback(float x);
    float bitcrush(float x, float& holdState);
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createDistortionEffect();

} // namespace Artifact
