module;
#include <vector>
#include <string>
#include <memory>
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Audio.Effects.Equalizer;




import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class EqualizerEffect {
public:
    EqualizerEffect();
    ~EqualizerEffect() = default;

    // Audio processing
    void process(float* buffer, int samples, int channels);
    std::string name() const { return "Equalizer"; }
    std::string getDescription() const { return "Multi-band equalizer effect"; }

    std::vector<AudioEffectParameter> getParameters() const;
    void setParameter(const std::string& name, float value);
    float getParameter(const std::string& name) const;

private:
    struct Band {
        float frequency;
        float gain;
        float q;
    };

    std::vector<Band> bands_;
    float sampleRate_ = 44100.0f;

    // バンドパスフィルタの適用
    void applyBandFilter(std::vector<float>& channelData, const Band& band);

    // バイクアッドフィルタ係数の計算
    void calculateBiquadCoefficients(float frequency, float gain, float q,
                                   float& a0, float& a1, float& a2,
                                   float& b0, float& b1, float& b2);
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createEqualizerEffect();

} // namespace Artifact