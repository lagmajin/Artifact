module;
#include <QVector>
#include <memory>
#include <vector>
export module Artifact.Audio.Effects.Base;

import std;
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
    virtual ArtifactCore::AudioSegment process(const ArtifactCore::AudioSegment& input) = 0;

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

// エフェクトファクトリーの型エイリアス
using AudioEffectFactory = std::unique_ptr<ArtifactAbstractAudioEffect>(*)();

} // namespace Artifact