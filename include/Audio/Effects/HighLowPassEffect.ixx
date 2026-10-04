module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.HighLowPass;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.HighLowPass;

export namespace Artifact {

// ArtifactCore の AudioHighLowPass をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioHighLowPass::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class HighLowPassEffect : public ArtifactAbstractAudioEffect {
public:
    HighLowPassEffect() = default;
    ~HighLowPassEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "High-Low Pass"; }
    String getDescription() const override {
        return "12dB/octave low-pass and high-pass filter pair";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "high_low_pass"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // フィルタは群遅延を持つが、固定の tail は生まない。latency 0。
    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioHighLowPass core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createHighLowPassEffect();

} // namespace Artifact