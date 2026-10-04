module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.NoiseGate;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

// ノイズゲート。閾値より下った入力を減衰させ、無音区間を閉じる。
// envelope は Compressor と同じ attack/release の係数を持つ 1 次系だが、
// Compressor は閾値超過で gain を落とし、このエフェクトは閾値以下で gain を落とす。
class NoiseGateEffect : public ArtifactAbstractAudioEffect {
public:
    NoiseGateEffect() = default;
    ~NoiseGateEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Noise Gate"; }
    String getDescription() const override {
        return "Attenuates signal below a threshold using attack/release envelope";
    }

    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "noise_gate"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // ゲートは時間シフトを起こさない。tail なし。
    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

    // ゲートは envelope に rate 依存の係数を持つ。rate が変わ으면静止状態から
    // やり直す。Bus から渡された segment の rate へ同期する。
    void reinitOnSampleRate() override { envelopeDb_ = -96.0f; }

private:
    float thresholdDb_   = -50.0f;
    float rangeDb_       = -60.0f;
    float attackMs_      = 1.0f;
    float releaseMs_     = 50.0f;
    float holdMs_        = 0.0f;

    float envelopeDb_    = -96.0f;
    float holdCounterMs_ = 0.0f;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createNoiseGateEffect();

} // namespace Artifact