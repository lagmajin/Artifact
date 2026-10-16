module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.PingPongDelay;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.PingPongDelay;

export namespace Artifact {

// ArtifactCore の AudioPingPongDelay をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioPingPongDelay::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class PingPongDelayEffect : public ArtifactAbstractAudioEffect {
public:
    PingPongDelayEffect() = default;
    ~PingPongDelayEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Ping-Pong Delay"; }
    String getDescription() const override {
        return "Stereo ping-pong delay with cross feedback";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "ping_pong_delay"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return core_.tailSamples(); }

private:
    ArtifactCore::AudioPingPongDelay core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createPingPongDelayEffect();

} // namespace Artifact
