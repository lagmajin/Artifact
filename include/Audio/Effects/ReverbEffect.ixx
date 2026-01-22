module;
#include <vector>
#include <string>
#include <memory>
export module Artifact.Audio.Effects.Reverb;

import std;
import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class ReverbEffect : public ArtifactAbstractAudioEffect {
public:
    ReverbEffect();
    ~ReverbEffect() override;

    // ArtifactAbstractAudioEffect interface
    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) override;
    std::string getName() const override { return "Reverb"; }
    std::string getDescription() const override { return "Reverb effect with configurable parameters"; }
    
    std::vector<AudioEffectParameter> getParameters() const override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;

    void setSampleRate(int sampleRate) override;

private:
    struct DelayLine {
        std::vector<float> buffer;
        size_t writePos = 0;
        
        void resize(size_t size) {
            buffer.resize(size, 0.0f);
            writePos = 0;
        }
        
        float read(size_t delay) const {
            size_t readPos = (writePos + buffer.size() - delay) % buffer.size();
            return buffer[readPos];
        }
        
        void write(float value) {
            buffer[writePos] = value;
            writePos = (writePos + 1) % buffer.size();
        }
    };

    struct CombFilter {
        DelayLine delayLine;
        float feedback = 0.0f;
        
        float process(float input) {
            float delayed = delayLine.read(static_cast<size_t>(delayLine.buffer.size() / 2));
            float output = input + delayed * feedback;
            delayLine.write(output);
            return output;
        }
    };

    struct AllpassFilter {
        DelayLine delayLine;
        float feedback = 0.0f;
        
        float process(float input) {
            float delayed = delayLine.read(static_cast<size_t>(delayLine.buffer.size() / 2));
            float output = delayed + input * feedback;
            delayLine.write(input - delayed * feedback);
            return output;
        }
    };

    std::vector<CombFilter> combFilters_;
    std::vector<AllpassFilter> allpassFilters_;
    
    float roomSize_ = 0.5f;
    float damping_ = 0.5f;
    float wetLevel_ = 0.3f;
    float dryLevel_ = 0.7f;
    
    void updateFilterParameters();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createReverbEffect();

} // namespace Artifact
