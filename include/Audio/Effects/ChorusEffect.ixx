module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Chorus;

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
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;

export namespace Artifact {

// ============================================================================
// Parameter Type Definitions
// ============================================================================

enum class ParameterType {
    Float,
    Int,
    Bool,
    Enum
};

struct Parameter {
    std::string name;
    std::string displayName;
    ParameterType type;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::vector<std::string> enumValues;
};

// ============================================================================
// Abstract Base Class
// ============================================================================

class IAudioEffect {
public:
    virtual ~IAudioEffect() = default;

    // Core processing
    virtual ::ArtifactCore::AudioSegment process(const ::ArtifactCore::AudioSegment& input) = 0;

    // Metadata
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // Parameter management
    virtual std::vector<Parameter> getParameters() const = 0;
    virtual void setParameter(const std::string& name, float value) = 0;
    virtual float getParameter(const std::string& name) const = 0;

    // State management
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual bool isEnabled() const { return enabled_; }

    // Sample rate configuration
    virtual void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    virtual int getSampleRate() const { return sampleRate_; }

protected:
    bool enabled_ = true;
    int sampleRate_ = 44100;
};

// ============================================================================
// Chorus Effect Implementation
// ============================================================================

/**
 * @brief Rich stereo Chorus effect using multiple modulated delay taps.
 *
 * Features:
 * - Multiple modulated delay lines (default 3 voices per channel)
 * - Stereo processing with independent LFOs for each voice
 * - Configurable rate, depth, delay time, and feedback
 * - Can behave as Flanger at lower delay times
 */
class ChorusEffect : public IAudioEffect {
public:
    ChorusEffect();
    ~ChorusEffect() override = default;

    // IAudioEffect implementation
    ::ArtifactCore::AudioSegment process(const ::ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Chorus"; }
    std::string getDescription() const override {
        return "Rich stereo chorus with multiple modulated voices";
    }

    std::vector<Parameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void setSampleRate(int sampleRate) override;

private:
    // Number of modulated delay taps per channel
    static constexpr int kNumVoices = 3;

    // DSP components
    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayR_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoR_[kNumVoices];

    // Chorus parameters
    float rate_      = 0.8f;   // LFO rate (Hz)
    float depth_     = 0.5f;   // Modulation depth (0..1)
    float delayMs_   = 7.0f;   // Center delay time (ms)
    float wetLevel_  = 0.5f;   // Wet signal level
    float dryLevel_  = 0.5f;   // Dry signal level
    float feedback_  = 0.1f;   // Feedback coefficient

    // Initialization
    void initializeEngine();
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IAudioEffect> createChorusEffect();

} // namespace Artifact
