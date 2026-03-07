module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Reverb;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.DSP.DelayLine;
import Audio.DSP.AllPassFilter;
import Audio.DSP.LFO;

export namespace Artifact {

/**
 * @brief High-end Dattorro Plate Reverb.
 * Based on Jon Dattorro's "Effect Design, Part 1: Reverberator and Other Filters" (1997).
 * Features modulated all-pass diffusers, absorptive low-pass filters in the tank,
 * and cross-coupled delay lines for a lush, wide stereo image.
 */
class ReverbEffect : public ArtifactAbstractAudioEffect {
public:
    ReverbEffect();
    ~ReverbEffect() override = default;

    // ArtifactAbstractAudioEffect interface
    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Dattorro Plate Reverb"; }
    std::string getDescription() const override {
        return "High-end algorithmic plate reverb with modulated diffusion";
    }
    
    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    // === Input Diffusion Stage ===
    // 4 series all-pass filters to smear the input into a dense cloud
    ArtifactCore::Audio::DSP::AllPassFilter inputDiffusion1_[2];
    ArtifactCore::Audio::DSP::AllPassFilter inputDiffusion2_[2];

    // === Tank (Reverb Tail) ===
    // Two cross-coupled delay lines with modulated all-pass + damping
    ArtifactCore::Audio::DSP::FractionalDelayLine tankDelay_[2];
    ArtifactCore::Audio::DSP::AllPassFilter tankAllPass_[2];
    ArtifactCore::Audio::DSP::LFO tankLFO_[2];

    // Pre-delay line
    ArtifactCore::Audio::DSP::FractionalDelayLine preDelay_;

    // One-pole low-pass for tank damping (simple inline)
    float dampState_[2] = {0.0f, 0.0f};

    // === Parameters ===
    float decay_      = 0.75f;   // 0.0 .. 1.0  (reverb tail length)
    float preDelayMs_ = 20.0f;   // 0 .. 200 ms
    float damping_    = 0.5f;    // 0.0 .. 1.0  (high-freq absorption)
    float diffusion_  = 0.75f;   // 0.0 .. 1.0  (input smear amount)
    float modDepth_   = 0.5f;    // 0.0 .. 1.0  (chorus inside tank)
    float modRate_    = 0.8f;    // Hz (LFO speed for tank modulation)
    float wetLevel_   = 0.35f;
    float dryLevel_   = 0.65f;
    float size_       = 1.0f;    // 0.5 .. 2.0  (scale all delay times)

    // Internal state
    float tankAccumL_ = 0.0f;
    float tankAccumR_ = 0.0f;

    void initializeEngine();

    // Dattorro-style delay lengths in samples (at 29761 Hz reference rate, scaled to actual rate)
    static constexpr float kRefSampleRate = 29761.0f;

    // Input diffusion delay times (in samples at reference rate)
    static constexpr float kInputDiff1a = 142.0f;
    static constexpr float kInputDiff1b = 107.0f;
    static constexpr float kInputDiff2a = 379.0f;
    static constexpr float kInputDiff2b = 277.0f;

    // Tank delay times
    static constexpr float kTankDelay1 = 672.0f;
    static constexpr float kTankDelay2 = 908.0f;
    static constexpr float kTankAP1 = 908.0f;
    static constexpr float kTankAP2 = 672.0f;

    float scaleDelay(float refSamples) const {
        return refSamples * (static_cast<float>(sampleRate_) / kRefSampleRate) * size_;
    }
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect();

} // namespace Artifact
