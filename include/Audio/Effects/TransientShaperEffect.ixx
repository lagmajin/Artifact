module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.TransientShaper;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.TransientShaper;

export namespace Artifact {

// ArtifactCore の AudioTransientShaper をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioTransientShaper::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class TransientShaperEffect : public ArtifactAbstractAudioEffect {
public:
    TransientShaperEffect() = default;
    ~TransientShaperEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Transient Shaper"; }
    String getDescription() const override {
        return "Attack and sustain designer with dual envelope followers";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "transient_shaper"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioTransientShaper core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createTransientShaperEffect();

} // namespace Artifact
