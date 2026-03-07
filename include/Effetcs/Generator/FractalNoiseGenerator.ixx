module;
#include <QString>

export module Artifact.Effect.Generator.FractalNoise;

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
