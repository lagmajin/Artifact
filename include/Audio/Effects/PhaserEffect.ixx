module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Phaser;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.Phaser;

export namespace Artifact {

// ArtifactCore の AudioPhaser をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioPhaser::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class PhaserEffect : public ArtifactAbstractAudioEffect {
public:
    PhaserEffect() = default;
    ~PhaserEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Phaser"; }
    String getDescription() const override {
        return "Sweeping notch phaser with up to 8 all-pass stages";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "phaser"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioPhaser core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createPhaserEffect();

} // namespace Artifact
