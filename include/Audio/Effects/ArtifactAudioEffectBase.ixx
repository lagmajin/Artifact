module;
#include <QVector>
#include <memory>
#include <vector>
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
export module Artifact.Audio.Effects.Base;




import Audio.Segment;
import Audio.Effect;

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
class ArtifactAbstractAudioEffect : public ArtifactCore::AudioEffect {
public:
    virtual ~ArtifactAbstractAudioEffect() = default;

    // ArtifactCore::AudioEffect インターフェース
    // 既存の process(const AudioSegment&) は削除、またはブリッジとして残す
    virtual void process(ArtifactCore::AudioSegment& segment) override = 0;

    // エフェクト名と説明
    virtual std::string getName() const override = 0;
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

// エフェクトファクトリーの型エイリアス
using AudioEffectFactory = std::unique_ptr<ArtifactAbstractAudioEffect>(*)();

} // namespace Artifact