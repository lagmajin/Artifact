module;
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <limits>
#include <QtGlobal>
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
    String getName() const override { return "Reverb"; }
    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "reverb"; }
    String getDescription() const override;

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;
    void setSampleRate(int sampleRate) override;
    void reinitOnSampleRate() override;

    // Reverb は入力遅延を持たない（pre-delay はWet 経路のみで、出力時刻は動かない）。
    qint64 latencySamples() const override { return 0; }

    // -60dB まで減衰するまでの時間。decay_ が大きいほど長い。
    // decay_ は FDN の fbGain(0.70..0.85) を通した実効 feedback と
    // Dattorro の tankAP 係数 (-decay_*0.7) の両方に効くため、
    // ここでは worst-case の Dattorro 経路で見積もる。
    qint64 tailSamples() const override {
        if (decay_ <= 0.0f) return 0;
        // tankAP のfeedback 係数 = decay_ * 0.7 が 1 に近いほど尾が長い。
        const double feedback = static_cast<double>(decay_) * 0.7;
        if (feedback >= 1.0) return std::numeric_limits<qint64>::max();
        if (feedback <= 0.0) return 0;
        // tank 最長 ≈ 908 samples * size_ @ refRate
        const double loopSamples =
            static_cast<double>(kTankDelay2) * size_ *
            (static_cast<double>(sampleRate_) / kRefSampleRate);
        if (!(loopSamples > 0.0)) return 0;
        const double repeats = std::log(0.001) / std::log(feedback);
        if (!(repeats > 0.0) || !std::isfinite(repeats)) return 0;
        const double tail = loopSamples * repeats;
        if (tail >= static_cast<double>(std::numeric_limits<qint64>::max())) {
            return std::numeric_limits<qint64>::max();
        }
        return static_cast<qint64>(tail);
    }

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
