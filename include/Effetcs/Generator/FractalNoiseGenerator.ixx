module;
#include <QString>

export module Artifact.Effect.Generator.FractalNoise;

import std;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;
import Property.Group;

export namespace Artifact {

    using namespace ArtifactCore;

    class FractalNoiseGenerator : public ArtifactAbstractEffect {
    private:
        int seed_ = 0;
        int octaves_ = 6;
        float frequency_ = 1.0f;
        float amplitude_ = 1.0f;

    public:
        FractalNoiseGenerator() {
            setDisplayName(ArtifactCore::UniString("Fractal Noise (Generator)"));
            setPipelineStage(EffectPipelineStage::Generator);
        }
        virtual ~FractalNoiseGenerator() = default;

        int seed() const { return seed_; }
        void setSeed(int seed) { seed_ = seed; }

        int octaves() const { return octaves_; }
        void setOctaves(int octaves) { octaves_ = octaves; }

        float frequency() const { return frequency_; }
        void setFrequency(float freq) { frequency_ = freq; }

        float amplitude() const { return amplitude_; }
        void setAmplitude(float amp) { amplitude_ = amp; }

        // Future: expose these properties to ArtifactPropertyWidget
    };

}
