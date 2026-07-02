module;
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <array>
export module Artifact.Audio.Effects.Reverb;


import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;
import Audio.DSP.AllPassFilter;
import Audio.DSP.LFO;

export namespace Artifact {

enum class ReverbAlgorithm {
    DattorroPlate = 0,
    FDNHall = 1,
    Hybrid = 2
};

class ReverbEffect final : public ArtifactAbstractAudioEffect {
public:
    ReverbEffect();
    ~ReverbEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    std::string getName() const override { return "Reverb"; }
    std::string getDescription() const override;

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void setSampleRate(int sampleRate) override;

private:
    void initEngine();
    void initDattorro();
    void initFDN();
    void processDattorroSample(float inL, float inR, float& outL, float& outR);
    void processFDNSample(float inL, float inR, float& outL, float& outR);
    void processHybridSample(float inL, float inR, float& outL, float& outR);
    float scaleDelay(float refSamples) const;
    static void fwht8(float* x);

    // Algorithm selection
    ReverbAlgorithm algorithm_ = ReverbAlgorithm::DattorroPlate;

    // Common parameters
    float preDelayMs_  = 20.0f;
    float decay_       = 0.75f;
    float decayLFMult_ = 1.0f;
    float decayHF_     = 0.5f;
    float dampingFreq_ = 8000.0f;
    float diffusion_   = 0.75f;
    float density_     = 0.7f;
    float modDepth_    = 0.5f;
    float modRate_     = 0.8f;
    float size_        = 1.0f;
    float stereoWidth_ = 1.0f;
    float erLevel_     = 0.3f;
    float erDelay_     = 0.5f;
    float wetLevel_    = 0.35f;
    float dryLevel_    = 0.65f;

    // Dattorro: input diffusers (2 x 2-ch)
    ArtifactCore::Audio::DSP::AllPassFilter inputDiff1_[2];
    ArtifactCore::Audio::DSP::AllPassFilter inputDiff2_[2];
    // Dattorro: tank delays + all-passes
    ArtifactCore::Audio::DSP::FractionalDelayLine tankDelay_[2];
    ArtifactCore::Audio::DSP::AllPassFilter tankAP_[2];
    // Pre-delay line
    ArtifactCore::Audio::DSP::FractionalDelayLine preDelay_;
    // Tank LFO phases
    float lfoPhase_[2] = {0.0f, 0.0f};
    // Tank state accumulators (cross-coupling)
    float tankAccum_[2] = {0.0f, 0.0f};
    // Damping one-pole state
    float dampState_[2] = {0.0f, 0.0f};

    // FDN: 8 delay lines
    static constexpr int kNumFDNLines = 8;
    struct FDNLine {
        std::vector<float> buffer;
        int writeIndex = 0;
        int length = 2048;
        float fbGain = 0.7f;
        float dampCoeff = 0.5f;
        float state = 0.0f;
    };
    FDNLine fdnLines_[kNumFDNLines];
    float fdnInputMix_[kNumFDNLines];
    float fdnOutputMix_[kNumFDNLines];
    float erTaps_[kNumFDNLines];

    // Reference rate for Dattorro delay scaling
    static constexpr float kRefSampleRate = 29761.0f;
    static constexpr float kInDiff1a = 142.0f;
    static constexpr float kInDiff1b = 107.0f;
    static constexpr float kInDiff2a = 379.0f;
    static constexpr float kInDiff2b = 277.0f;
    static constexpr float kTankDelay1 = 672.0f;
    static constexpr float kTankDelay2 = 908.0f;
    static constexpr float kTankAP1 = 908.0f;
    static constexpr float kTankAP2 = 672.0f;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect();

} // namespace Artifact
