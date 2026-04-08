module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
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
export module Artifact.Audio.Effects.Distortion;




import Audio.Segment;

export namespace Artifact {

// エフェクトパラメータの基本型
enum class AudioEffectParameterType {
    Float,
    Int,
    Bool,
    Enum
};

// エフェクトパラメータ記述子
struct AudioEffectParameter {
    std::string name;
    std::string displayName;
    AudioEffectParameterType type;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::vector<std::string> enumValues;
};

// 抽象オーディオエフェクト基底クラス
class ArtifactAbstractAudioEffect {
public:
    virtual ~ArtifactAbstractAudioEffect() = default;

    // エフェクト処理の実行
    virtual ::ArtifactCore::AudioSegment process(const ::ArtifactCore::AudioSegment& input) = 0;

    // エフェクト名と説明
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // パラメータ管理
    virtual std::vector<AudioEffectParameter> getParameters() const = 0;
    virtual void setParameter(const std::string& name, float value) = 0;
    virtual float getParameter(const std::string& name) const = 0;

    // エフェクトの有効/無効
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual bool isEnabled() const { return enabled_; }

    // サンプルレート設定
    virtual void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    virtual int getSampleRate() const { return sampleRate_; }

protected:
    bool enabled_ = true;
    int sampleRate_ = 44100;
};

/**
 * @brief Multi-mode Distortion / Saturation effect.
 * Supports: Soft Clip (Tanh), Hard Clip, Tube Saturation, Foldback, Bitcrush.
 */
class DistortionEffect : public ArtifactAbstractAudioEffect {
public:
    DistortionEffect() = default;
    ~DistortionEffect() = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input);
    std::string getName() const { return "Distortion"; }
    std::string getDescription() const {
        return "Multi-mode distortion with soft clip, tube, foldback, and bitcrush";
    }

    std::vector<AudioEffectParameter> getParameters() const;
    void setParameter(const std::string& name, float value);
    float getParameter(const std::string& name) const;

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
