module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.PlateReverb;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.PlateReverb;

export namespace Artifact {

// ArtifactCore の AudioPlateReverb をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioPlateReverb::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
// 既存 ReverbEffect(Dattorro/FDN/Hybrid の独自実装)とは別物。effectType は "plate_reverb"。
class PlateReverbEffect : public ArtifactAbstractAudioEffect {
public:
    PlateReverbEffect() = default;
    ~PlateReverbEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Plate Reverb"; }
    String getDescription() const override {
        return "Dattorro-style plate reverb with modulated tank and damping";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "plate_reverb"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // Pre-delay は Wet 経路のみで出力時刻は動かさない。tail は Core に委譲する。
    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return core_.tailSamples(); }

private:
    ArtifactCore::AudioPlateReverb core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createPlateReverbEffect();

} // namespace Artifact
