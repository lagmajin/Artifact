module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.SpringReverb;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.SpringReverb;

export namespace Artifact {

// ArtifactCore の AudioSpringReverb をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioSpringReverb::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
// 既存 ReverbEffect とは別物。effectType は "spring_reverb"。
class SpringReverbEffect : public ArtifactAbstractAudioEffect {
public:
    SpringReverbEffect() = default;
    ~SpringReverbEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Spring Reverb"; }
    String getDescription() const override {
        return "Metallic spring reverb with tension control";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "spring_reverb"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return core_.tailSamples(); }

private:
    ArtifactCore::AudioSpringReverb core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createSpringReverbEffect();

} // namespace Artifact
