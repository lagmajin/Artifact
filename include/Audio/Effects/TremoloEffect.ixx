module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Tremolo;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.Tremolo;

export namespace Artifact {

// ArtifactCore の AudioTremolo をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioTremolo::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class TremoloEffect : public ArtifactAbstractAudioEffect {
public:
    TremoloEffect() = default;
    ~TremoloEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Tremolo"; }
    String getDescription() const override {
        return "Amplitude modulation tremolo with auto-pan width";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "tremolo"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioTremolo core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createTremoloEffect();

} // namespace Artifact
