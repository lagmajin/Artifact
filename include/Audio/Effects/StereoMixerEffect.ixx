module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.StereoMixer;

import Audio.Segment;
import Artifact.Audio.Effects.Base;
import Audio.Effect.StereoMixer;

export namespace Artifact {

// ArtifactCore の AudioStereoMixer をミキサーの FX レジストリから利用できるようにする
// アダプタ。Core の process が実際に読むのは leftRight_ のみ
// (center_/leftDelay_/rightDelay_ は未使用)のため、UI には balance だけ出す。
class StereoMixerEffect : public ArtifactAbstractAudioEffect {
public:
    StereoMixerEffect() = default;
    ~StereoMixerEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Stereo Mixer"; }
    String getDescription() const override {
        return "Stereo left-right balance control";
    }

    // 保存・復元の識別キーに使われる。登録済みの id と一致させる。
    String effectType() const override { return "stereo_mixer"; }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    ArtifactCore::AudioStereoMixer core_;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createStereoMixerEffect();

} // namespace Artifact
