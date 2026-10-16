module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Expander;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.Expander;

export namespace Artifact {

// ArtifactCore の AudioExpander をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioExpander::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class ExpanderEffect : public ArtifactAbstractAudioEffect {
public:
    ExpanderEffect() = default;
    ~ExpanderEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Expander"; }
    String getDescription() const override {
        return "Downward expander for noise floor and bleed control";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "expander"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioExpander core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createExpanderEffect();

} // namespace Artifact
