module;
#include <memory>
#include <vector>
#include <QVector>
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
#include <limits>
#include <QtGlobal>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
export module Artifact.Audio.Effects.Base;




import Audio.Segment;
import Audio.Effect;
import Core.ArtifactString;

export namespace Artifact {

using ArtifactCore::String;

// エフェクトパラメータの基本型
enum class AudioEffectParameterType {
    Float,
    Int,
    Bool,
    Enum
};

// エフェクトパラメータ記述子
struct AudioEffectParameter {
    String name;
    String displayName;
    AudioEffectParameterType type;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::vector<String> enumValues;
};

// 抽象オーディオエフェクト基底クラス
class ArtifactAbstractAudioEffect : public ArtifactCore::AudioEffect {
public:
    virtual ~ArtifactAbstractAudioEffect() = default;

    // ArtifactCore::AudioEffect インターフェース
    virtual void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override = 0;

    // エフェクト名と説明
    virtual String getName() const override = 0;
    virtual String getDescription() const = 0;

    // パラメータ管理（UI向けの拡張表現）
    virtual std::vector<AudioEffectParameter> getUiParameters() const = 0;
    virtual void setParameter(const String& name, float value) = 0;
    virtual float getParameter(const String& name) const = 0;

    // エフェクトの有効/無効
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual bool isEnabled() const { return enabled_; }

    // 保存・復元の既定実装。Core の AudioEffect::toJson は type と bypass しか
    // 書かないため、UI パラメータを反映させないとパラメータ調整が保存されない。
    // 派生クラスは effectType() と getUiParameters()/getParameter() を実装するだけで
    // 値が往復する。
    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["type"] = QString::fromUtf8(
            ArtifactCore::toStdString(effectType()).data());
        obj["bypass"] = bypass_;
        obj["enabled"] = enabled_;
        for (const auto& p : getUiParameters()) {
            obj[QString::fromUtf8(ArtifactCore::toStdString(p.name).data())] =
                getParameter(p.name);
        }
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("bypass")) bypass_ = obj["bypass"].toBool(false);
        if (obj.contains("enabled")) enabled_ = obj["enabled"].toBool(true);
        for (const auto& p : getUiParameters()) {
            const QString key = QString::fromUtf8(ArtifactCore::toStdString(p.name).data());
            if (obj.contains(key)) setParameter(p.name, static_cast<float>(obj[key].toDouble()));
        }
    }

    // サンプルレート設定
    virtual void setSampleRate(int sampleRate) {
        sampleRate_ = sampleRate > 0 ? sampleRate : 44100;
    }
    virtual int getSampleRate() const { return sampleRate_; }

    // 再生対象の AudioSegment が持つ実サンプルレートを同期する。
    // オフライン書き出しは 48kHz、リアルタイム再生は音声デバイスに依存するため、
    // エフェクトがハードコードされたレートで計算するとピッチと長さが変わる。
    // process() の先頭で必ず呼び、値が変化したときだけ派生状態を再構築する。
    // 戻り値は true のとき sampleRate_ が変化し、reinitOnSampleRate() が呼ばれた。
    bool syncSampleRate(const ArtifactCore::AudioSegment& segment) {
        const int rate = segment.sampleRate;
        if (rate <= 0 || rate == sampleRate_) return false;
        sampleRate_ = rate;
        reinitOnSampleRate();
        return true;
    }

    // サンプルレート変更時に遅延バッファなどの派生状態を再構築する。
    // 依存するエフェクトだけが override する。bounded 確保であり、
    // process() 内の毎ブロック確保ではない。
    virtual void reinitOnSampleRate() {}

    // 末尾鳴り（tail）とレイテンシをサンプル数で申告する。
    // バスは全エフェクトの最大値を使い、オフライン書き出しは
    // baseSamples + graphTailSamples() まで描画する。
    virtual qint64 latencySamples() const override { return 0; }
    virtual qint64 tailSamples() const override { return 0; }

protected:
    bool enabled_ = true;
    int sampleRate_ = 44100;

    // 秒をサンプル数へ変換する。sampleRate_ <= 0 は 0 サンプル扱い。
    qint64 samplesFromMs(float ms) const {
        if (sampleRate_ <= 0 || !(ms > 0.0f)) return 0;
        const double samples = static_cast<double>(ms) * 0.001 * sampleRate_;
        if (samples >= static_cast<double>(std::numeric_limits<qint64>::max())) {
            return std::numeric_limits<qint64>::max();
        }
        return static_cast<qint64>(samples);
    }
};

// エフェクトファクトリーの型エイリアス
using AudioEffectFactory = std::unique_ptr<ArtifactAbstractAudioEffect>(*)();

} // namespace Artifact
