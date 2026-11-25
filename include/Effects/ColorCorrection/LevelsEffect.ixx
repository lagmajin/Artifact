module;
#include <utility>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module LevelsEffect;

import Artifact.Render.PointwiseEffectFusion;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.LevelsCurves;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LevelsEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Normal = 1,
        HighContrast = 2,
        LowContrast = 3,
        Brighten = 4,
        Darken = 5,
    };

    ArtifactCore::LevelsSettings settings_;
    Preset preset_ = Preset::Normal;
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);
    void updatePresetFromSettings();

public:
    LevelsEffect();
    ~LevelsEffect() override;

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
                QStringLiteral("effect.levels.input_black"),
                QStringLiteral("Input Black"), 0.0, 255.0, 1.0,
                255.0 / 400.0, 1.0, QString()};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.levels.preset"),
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
                QStringLiteral("effect.levels.per_channel"),
                QStringLiteral("Per Channel")};
        }
        return std::nullopt;
    }

    void setInputBlack(float value);
    void setInputWhite(float value);
    void setInputGamma(float value);
    void setOutputBlack(float value);
    void setOutputWhite(float value);
    void setPerChannel(bool value);

    const ArtifactCore::LevelsSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Pointwise;
    }
    bool appendGpuPointwiseNodes(ArtifactCore::PointwiseEffectStack& stack,
                                 std::uint32_t& slot) const override {
        constexpr double kEpsilon = 1.0e-6;
        if (settings_.perChannel || std::abs(settings_.inputGamma - 1.0) > kEpsilon ||
            std::abs(settings_.outputBlack) > kEpsilon ||
            std::abs(settings_.outputWhite - 255.0) > kEpsilon ||
            settings_.inputWhite <= settings_.inputBlack + kEpsilon ||
            slot + 1 >= ArtifactCore::PointwiseEffectStack::kParameterSlotCount) return false;
        stack.addNode(ArtifactCore::PointwiseNodeKind::Levels, slot);
        stack.setParameter(slot++, static_cast<float>(std::clamp(settings_.inputBlack / 255.0, 0.0, 1.0)));
        stack.setParameter(slot++, static_cast<float>(std::clamp(settings_.inputWhite / 255.0, 0.0, 1.0)));
        return true;
    }
};

} // namespace Artifact
