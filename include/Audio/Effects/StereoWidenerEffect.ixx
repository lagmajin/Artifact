module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.StereoWidener;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.StereoWidener;

export namespace Artifact {

// ArtifactCore の AudioStereoWidener をミキサーの FX レジストリから利用できるようにする
// アダプタ。DSP 自体は AudioStereoWidener::process が持ち、このクラスは
// ArtifactAbstractAudioEffect -required な UI パラメータ面と保存用の effectType だけを担う。
class StereoWidenerEffect : public ArtifactAbstractAudioEffect {
public:
    StereoWidenerEffect() = default;
    ~StereoWidenerEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Stereo Widener"; }
    String getDescription() const override {
        return "Mid/Side stereo width with bass mono protection";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "stereo_widener"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioStereoWidener core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createStereoWidenerEffect();

} // namespace Artifact
