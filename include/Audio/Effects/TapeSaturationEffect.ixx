module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.TapeSaturation;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.TapeSaturation;

export namespace Artifact {

// ArtifactCore の AudioTapeSaturation をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioTapeSaturation::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class TapeSaturationEffect : public ArtifactAbstractAudioEffect {
public:
    TapeSaturationEffect() = default;
    ~TapeSaturationEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Tape Saturation"; }
    String getDescription() const override {
        return "Warm tape-style saturation with drive and damping";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "tape_saturation"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioTapeSaturation core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createTapeSaturationEffect();

} // namespace Artifact
