module;
#include <optional>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module GradientRampEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.GradientRamp;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class GradientRampEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Sunrise = 0,
        Ocean = 1,
        Neon = 2,
        Mono = 3,
        Custom = 4
    };

    ArtifactCore::GradientRampSettings settings_;
    Preset preset_ = Preset::Sunrise;
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);
    void updatePresetFromSettings();

public:
    GradientRampEffect();
    ~GradientRampEffect() override;

    void setPreset(int preset);
    void setStartColor(const QColor& color);
    void setEndColor(const QColor& color);
    void setStartPoint(float x, float y);
    void setEndPoint(float x, float y);
    void setOpacity(float value);
    void setPreserveAlpha(bool value);

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.gradientramp.opacity"),
                QStringLiteral("Opacity"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.gradient_ramp.preset"),
                QStringLiteral("Preset"), 0.0, 4.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 5;
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
                QStringLiteral("effect.colorcorrection.gradientramp.preserve_alpha"),
                QStringLiteral("Preserve Alpha")};
        }
        return std::nullopt;
    }

    int preset() const { return static_cast<int>(preset_); }
    bool customPresetPinnedForSnapshot() const { return customPresetPinned_; }
    void restoreCustomPresetPinnedForSnapshot(bool pinned) {
        customPresetPinned_ = pinned;
        if (pinned) {
            preset_ = Preset::Custom;
        } else {
            updatePresetFromSettings();
        }
    }
    const ArtifactCore::GradientRampSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
