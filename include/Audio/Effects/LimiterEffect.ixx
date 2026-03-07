module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <qlist>
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
export module Artifact.Audio.Effects.Limiter;




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
 * @brief Brick-wall Lookahead Limiter.
 * Prevents output from exceeding the ceiling level.
 * Uses lookahead + smooth gain reduction for transparent limiting.
 */
class LimiterEffect : public ArtifactAbstractAudioEffect {
public:
    LimiterEffect();
    ~LimiterEffect() = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input);
    std::string getName() const { return "Limiter"; }
    std::string getDescription() const {
        return "Brick-wall lookahead limiter with transparent gain reduction";
    }

    std::vector<AudioEffectParameter> getParameters() const;
    void setParameter(const std::string& name, float value);
    float getParameter(const std::string& name) const;

    void setSampleRate(int sampleRate);

private:
    float ceiling_    = -0.3f;   // dB (output ceiling)
    float releaseMs_  = 50.0f;   // ms
    float inputGain_  = 0.0f;    // dB (drive into limiter)

    // Lookahead buffer (circular)
    static constexpr int kMaxLookahead = 512;
    std::vector<std::vector<float>> lookaheadBuf_;
    int lookaheadWritePos_ = 0;
    int lookaheadSamples_  = 64;  // ~1.3ms at 48kHz

    // Gain reduction state
    float currentGain_ = 1.0f;

    void initializeEngine();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createLimiterEffect();

} // namespace Artifact
