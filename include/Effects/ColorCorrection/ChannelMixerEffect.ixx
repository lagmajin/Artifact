module;
#include <utility>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module ChannelMixerEffect;

import Artifact.Render.PointwiseEffectFusion;
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
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);
    void updatePresetFromSettings();

public:
    ChannelMixerEffect();
    ~ChannelMixerEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }
    bool customPresetPinnedForSnapshot() const { return customPresetPinned_; }
    void restoreCustomPresetPinnedForSnapshot(bool pinned) {
        customPresetPinned_ = pinned;
    }

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.channel_mixer.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index >= 2 && index <= 10) {
            constexpr const char* kProperties[] = {
                "Red From Red", "Red From Green", "Red From Blue",
                "Green From Red", "Green From Green", "Green From Blue",
                "Blue From Red", "Blue From Green", "Blue From Blue"};
            constexpr const char* kIds[] = {
                "red_from_red", "red_from_green", "red_from_blue",
                "green_from_red", "green_from_green", "green_from_blue",
                "blue_from_red", "blue_from_green", "blue_from_blue"};
            const auto matrixIndex = index - 2;
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.channel_mixer.%1")
                    .arg(QString::fromLatin1(kIds[matrixIndex])),
                QString::fromLatin1(kProperties[matrixIndex]),
                -2.0, 2.0, 0.01, 0.01, 1.0, QStringLiteral("linear")};
        }
        if (index == 11) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.channel_mixer.preset"),
                QStringLiteral("Preset"), 0.0, 5.0, 1.0, 0.05,
                1.0, QString()};
            parameter.choiceCount = 6;
            return parameter;
        }
        return std::nullopt;
    }

    std::optional<EffectViewportToggleParameter>
    viewportToggleParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::viewportToggleParameter(index);
        }
        if (index == 1) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.channel_mixer.monochrome"),
                QStringLiteral("Monochrome")};
        }
        if (index == 2) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.channel_mixer.preserve_luma"),
                QStringLiteral("Preserve Luma")};
        }
        return std::nullopt;
    }

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
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack,
                                 std::uint32_t& slot) const override {
        const auto& s = settings_;
        if (s.monochrome || s.preserveLuma || std::abs(s.strength - 1.0f) > 1.0e-6f ||
            slot + 4 >= ArtifactCore::PointwiseEffectStack::kParameterSlotCount) return false;
        stack.addNode(ArtifactCore::PointwiseNodeKind::ColorMatrix, slot);
        stack.setParameter(slot++, {s.matrix[0][0], s.matrix[0][1], s.matrix[0][2], 0.0f});
        stack.setParameter(slot++, {s.matrix[1][0], s.matrix[1][1], s.matrix[1][2], 0.0f});
        stack.setParameter(slot++, {s.matrix[2][0], s.matrix[2][1], s.matrix[2][2], 0.0f});
        // The CPU channel mixer clamps RGB after applying the matrix.
        stack.addNode(ArtifactCore::PointwiseNodeKind::Clamp, slot);
        stack.setParameter(slot++, {0.0f, 0.0f, 0.0f, 0.0f});
        stack.setParameter(slot++, {1.0f, 1.0f, 1.0f, 1.0f});
        return true;
    }
};

} // namespace Artifact
