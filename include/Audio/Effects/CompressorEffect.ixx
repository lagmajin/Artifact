module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
export module Artifact.Audio.Effects.Compressor;

import std;
import Audio.Segment;

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
// Compressor Effect Implementation
// ============================================================================

/**
 * @brief DAW-grade Dynamic Range Compressor with lookahead envelope follower.
 *
 * Features:
 * - Soft-knee compression for natural sound
 * - Auto make-up gain to compensate for gain reduction
 * - Smooth attack/release envelope following
 * - Configurable threshold, ratio, attack, release, and knee width
 */
class CompressorEffect : public IAudioEffect {
public:
    CompressorEffect() = default;
    ~CompressorEffect() override = default;

    // IAudioEffect implementation
    ::ArtifactCore::AudioSegment process(const ::ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Compressor"; }
    std::string getDescription() const override {
        return "Dynamic range compressor with soft-knee and auto make-up gain";
    }

    std::vector<Parameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

private:
    // Compressor parameters
    float threshold_   = -20.0f;  // dB
    float ratio_       = 4.0f;    // compression ratio (1:ratio)
    float attackMs_    = 10.0f;   // attack time (ms)
    float releaseMs_   = 100.0f;  // release time (ms)
    float kneeWidth_   = 6.0f;    // soft knee width (dB)
    float makeupGain_  = 0.0f;    // makeup gain (dB)
    bool  autoMakeup_  = true;    // automatic makeup gain

    // State
    float envelopeDb_  = -96.0f;  // current envelope level (dB)
};

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IAudioEffect> createCompressorEffect();

} // namespace Artifact
