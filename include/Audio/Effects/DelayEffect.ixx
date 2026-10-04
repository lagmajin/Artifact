module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <limits>
#include <QtGlobal>

export module Artifact.Audio.Effects.Delay;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class DelayEffect : public ArtifactAbstractAudioEffect {
public:
    DelayEffect();
    ~DelayEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Stereo Delay"; }
    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "delay"; }
    String getDescription() const override {
        return "Stereo delay with ping-pong mode and filtered feedback";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    void setSampleRate(int sampleRate) override;
    void reinitOnSampleRate() override;

    // 設定された遅延時間ぶんだけ出力が遅れる。
    qint64 latencySamples() const override {
        return std::max<qint64>(samplesFromMs(delayTimeL_),
                                samplesFromMs(delayTimeR_));
    }
    // feedback で繰り返すたびに減衰するため、入力終了後も尾が残る。
    // 尾が -60dB まで減衰するまでの反復回数を、从 delay 間隔で積算する。
    // これが 0 のままだとオフライン書き出しで最後の繰り返しが切れる。
    qint64 tailSamples() const override {
        if (feedback_ <= 0.0f || feedback_ >= 1.0f) return 0;
        const double repeats = std::log(0.001) / std::log(static_cast<double>(feedback_));
        if (!(repeats > 0.0) || !std::isfinite(repeats)) return 0;
        const qint64 spacing = latencySamples();
        if (spacing <= 0) return 0;
        const double tail = static_cast<double>(spacing) * repeats;
        if (tail >= static_cast<double>(std::numeric_limits<qint64>::max())) {
            return std::numeric_limits<qint64>::max();
        }
        return static_cast<qint64>(tail);
    }

private:
    ArtifactCore::Audio::DSP::FractionalDelayLine delayL_;
    ArtifactCore::Audio::DSP::FractionalDelayLine delayR_;

    float delayTimeL_  = 375.0f;
    float delayTimeR_  = 375.0f;
    float feedback_    = 0.4f;
    float wetLevel_    = 0.3f;
    float dryLevel_    = 0.7f;
    float highCut_     = 0.3f;
    bool  pingPong_    = false;

    float fbFilterStateL_ = 0.0f;
    float fbFilterStateR_ = 0.0f;

    void initializeDelays();
};

std::unique_ptr<ArtifactAbstractAudioEffect> createDelayEffect();

} // namespace Artifact
