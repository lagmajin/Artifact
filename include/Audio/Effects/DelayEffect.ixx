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
export module Artifact.Audio.Effects.Delay;




import Audio.Segment;
import Audio.DSP.DelayLine;

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
 * @brief Stereo Delay effect with ping-pong mode, tempo sync capability,
 * and high-cut filtering in the feedback path.
 */
class DelayEffect : public ArtifactAbstractAudioEffect {
public:
    DelayEffect();
    ~DelayEffect() = default;

    ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input);
    std::string getName() const { return "Stereo Delay"; }
    std::string getDescription() const {
        return "Stereo delay with ping-pong mode and filtered feedback";
    }

    std::vector<AudioEffectParameter> getParameters() const;
    void setParameter(const std::string& name, float value);
    float getParameter(const std::string& name) const;

    void setSampleRate(int sampleRate);

private:
    ArtifactCore::Audio::DSP::FractionalDelayLine delayL_;
    ArtifactCore::Audio::DSP::FractionalDelayLine delayR_;

    float delayTimeL_  = 375.0f;   // ms
    float delayTimeR_  = 375.0f;   // ms
    float feedback_    = 0.4f;     // 0..0.95
    float wetLevel_    = 0.3f;
    float dryLevel_    = 0.7f;
    float highCut_     = 0.3f;     // 0..1 (amount of high-freq damping in feedback)
    bool  pingPong_    = false;

    // Feedback filter state
    float fbFilterStateL_ = 0.0f;
    float fbFilterStateR_ = 0.0f;

    void initializeDelays();
};

// ファクトリー関数
std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect();

} // namespace Artifact
