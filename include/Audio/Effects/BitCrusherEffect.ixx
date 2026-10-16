module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.BitCrusher;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.BitCrusher;

export namespace Artifact {

// ArtifactCore の AudioBitCrusher をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioBitCrusher::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class BitCrusherEffect : public ArtifactAbstractAudioEffect {
public:
    BitCrusherEffect() = default;
    ~BitCrusherEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Bit Crusher"; }
    String getDescription() const override {
        return "Lo-fi bit depth reduction with downsampling";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "bit_crusher"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioBitCrusher core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createBitCrusherEffect();

} // namespace Artifact
