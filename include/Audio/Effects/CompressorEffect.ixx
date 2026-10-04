module;
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <QtGlobal>

export module Artifact.Audio.Effects.Compressor;

import Audio.Segment;
import Artifact.Audio.Effects.Base;

export namespace Artifact {

class CompressorEffect : public ArtifactAbstractAudioEffect {
public:
    CompressorEffect() = default;
    ~CompressorEffect() override = default;

    void process(ArtifactCore::AudioSegment& segment, const ArtifactCore::AudioSegment* sideChain = nullptr) override;
    String getName() const override { return "Compressor"; }
    // 保存・復元の識別キー。登録済みの id と一致させる。
    String effectType() const override { return "compressor"; }
    String getDescription() const override {
        return "Dynamic range compressor with soft-knee and auto make-up gain";
    }

    std::vector<AudioEffectParameter> getUiParameters() const override;
    void setParameter(const String& name, float value) override;
    float getParameter(const String& name) const override;

    // レートが変わると attack/release 係数が変わるため envelope を破棄する。
    void reinitOnSampleRate() override { envelopeDb_ = -96.0f; }

    // 圧縮は時間シフトを起こさない。tail なし。
    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override { return 0; }

private:
    float threshold_   = -20.0f;
    float ratio_       = 4.0f;
    float attackMs_    = 10.0f;
    float releaseMs_   = 100.0f;
    float kneeWidth_   = 6.0f;
    float makeupGain_  = 0.0f;
    bool  autoMakeup_  = true;

    float envelopeDb_  = -96.0f;
};

std::unique_ptr<ArtifactAbstractAudioEffect> createCompressorEffect();

} // namespace Artifact
