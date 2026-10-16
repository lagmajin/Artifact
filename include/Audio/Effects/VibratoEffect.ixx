module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Vibrato;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.Vibrato;

export namespace Artifact {

// ArtifactCore の AudioVibrato をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioVibrato::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class VibratoEffect : public ArtifactAbstractAudioEffect {
public:
    VibratoEffect() = default;
    ~VibratoEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Vibrato"; }
    String getDescription() const override {
        return "Pitch vibrato with modulated short delay";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "vibrato"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // 中心遅延ぶんだけ出力は遅れる。tail は生まない。
    qint64 latencySamples() const override { return core_.latencySamples(); }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioVibrato core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createVibratoEffect();

} // namespace Artifact
