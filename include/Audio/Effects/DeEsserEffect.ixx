module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.DeEsser;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.DeEsser;

export namespace Artifact {

// ArtifactCore の AudioDeEsser をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioDeEsser::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class DeEsserEffect : public ArtifactAbstractAudioEffect {
public:
    DeEsserEffect() = default;
    ~DeEsserEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "De-Esser"; }
    String getDescription() const override {
        return "Sibilance tamer with split-band high-frequency reduction";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "de_esser"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioDeEsser core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createDeEsserEffect();

} // namespace Artifact
