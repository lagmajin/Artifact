module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.DCBlocker;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.DCBlocker;

export namespace Artifact {

// ArtifactCore の AudioDCBlocker をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioDCBlocker::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class DCBlockerEffect : public ArtifactAbstractAudioEffect {
public:
    DCBlockerEffect() = default;
    ~DCBlockerEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "DC Blocker"; }
    String getDescription() const override {
        return "Removes DC offset below the cutoff frequency";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "dc_blocker"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioDCBlocker core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createDCBlockerEffect();

} // namespace Artifact
