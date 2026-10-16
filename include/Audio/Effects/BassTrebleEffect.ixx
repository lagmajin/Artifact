module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.BassTreble;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.BassTreble;

export namespace Artifact {

// ArtifactCore の AudioBassTreble をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioBassTreble::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class BassTrebleEffect : public ArtifactAbstractAudioEffect {
public:
    BassTrebleEffect() = default;
    ~BassTrebleEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Bass & Treble"; }
    String getDescription() const override {
        return "Simple bass and treble tone control";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "bass_treble"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioBassTreble core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createBassTrebleEffect();

} // namespace Artifact
