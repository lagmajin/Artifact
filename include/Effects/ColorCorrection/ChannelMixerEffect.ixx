module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module ChannelMixerEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ChannelMixer;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class ChannelMixerEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Identity = 1,
        Warm = 2,
        Cool = 3,
        CrossProcess = 4,
        Monochrome = 5,
    };

    ArtifactCore::ChannelMixerSettings settings_;
    Preset preset_ = Preset::Identity;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    ChannelMixerEffect();
    ~ChannelMixerEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setStrength(float value);
    void setMonochrome(bool value);
    void setPreserveLuma(bool value);
    void setMatrix(float rr, float rg, float rb,
                   float gr, float gg, float gb,
                   float br, float bg, float bb);

    const ArtifactCore::ChannelMixerSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack,
                                 std::uint32_t& slot) const override {
        const auto& s = settings_;
        if (s.monochrome || s.preserveLuma || std::abs(s.strength - 1.0f) > 1.0e-6f ||
            slot + 2 >= ArtifactCore::PointwiseEffectStack::kParameterSlotCount) return false;
        stack.addNode(ArtifactCore::PointwiseNodeKind::ColorMatrix, slot);
        stack.setParameter(slot++, {s.matrix[0][0], s.matrix[0][1], s.matrix[0][2], 0.0f});
        stack.setParameter(slot++, {s.matrix[1][0], s.matrix[1][1], s.matrix[1][2], 0.0f});
        stack.setParameter(slot++, {s.matrix[2][0], s.matrix[2][1], s.matrix[2][2], 0.0f});
        return true;
    }
};

} // namespace Artifact
