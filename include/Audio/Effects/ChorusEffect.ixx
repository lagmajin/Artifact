module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QtGlobal>

export module Artifact.Audio.Effects.Chorus;

import Audio.Segment;
import Audio.DSP.DelayLine;
import Audio.DSP.LFO;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class ChorusEffect : public ArtifactAbstractAudioEffect {
public:
    ChorusEffect();
    ~ChorusEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Chorus"; }
    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "chorus"; }
    String getDescription() const override {
        return "Rich stereo chorus with multiple modulated voices";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;
    void setSampleRate(int sampleRate) override;
    void reinitOnSampleRate() override;

    // 中心遅延 + LFO 振幅ぶんだけ、出力は入力より遅れる。
    qint64 latencySamples() const override {
        const float center = delayMs_ * 0.001f * static_cast<float>(sampleRate_);
        const float depth = depth_ * center * 0.5f;
        return static_cast<qint64>(std::max(1.0f, center + depth));
    }
    // feedback < 1 なので減衰し、有限時間で 0 へ収束する。tail なし。
    qint64 tailSamples() const override { return 0; }

private:
    static constexpr int kNumVoices = 3;

    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::FractionalDelayLine delayR_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoL_[kNumVoices];
    ::ArtifactCore::Audio::DSP::LFO lfoR_[kNumVoices];

    float rate_      = 0.8f;
    float depth_     = 0.5f;
    float delayMs_   = 7.0f;
    float wetLevel_  = 0.5f;
    float dryLevel_  = 0.5f;
    float feedback_  = 0.1f;

    // 初回構築時だけ LFO 位相を 0 から始めるフラグ。
    // 以降の再初期化では位相を維持する。
    bool initialized_ = false;

    void initializeEngine();
};

std::unique_ptr<ArtifactAbstractAudioEffect> createChorusEffect();

} // namespace Artifact
