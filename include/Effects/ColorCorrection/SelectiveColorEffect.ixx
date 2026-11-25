module;
#include <optional>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module SelectiveColorEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.SelectiveColor;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class SelectiveColorEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Neutral = 1,
        Warm = 2,
        Cool = 3,
        Vivid = 4,
        Film = 5,
    };

    ArtifactCore::SelectiveColorSettings settings_;
    Preset preset_ = Preset::Neutral;
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);
    void updatePresetFromSettings();
    static ArtifactCore::SelectiveColorSettings settingsForPreset(Preset preset);

public:
    SelectiveColorEffect();
    ~SelectiveColorEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }
    bool customPresetPinnedForSnapshot() const { return customPresetPinned_; }
    void restoreCustomPresetPinnedForSnapshot(bool pinned) {
        customPresetPinned_ = pinned;
        updatePresetFromSettings();
    }

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.color.selective_color.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025, 100.0,
                QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.selective_color.preset"),
                QStringLiteral("Preset"), 0.0, 5.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 6;
            return parameter;
        }
        if (index >= 3 && index < 39) {
            static constexpr const char* kGroupNames[] = {
                "Reds", "Yellows", "Greens", "Cyans", "Blues",
                "Magentas", "Whites", "Neutrals", "Blacks"};
            static constexpr const char* kGroupKeys[] = {
                "reds", "yellows", "greens", "cyans", "blues",
                "magentas", "whites", "neutrals", "blacks"};
            static constexpr const char* kChannelNames[] = {
                "Cyan", "Magenta", "Yellow", "Black"};
            static constexpr const char* kChannelKeys[] = {
                "cyan", "magenta", "yellow", "black"};
            const unsigned int adjustmentIndex = index - 3;
            const unsigned int groupIndex = adjustmentIndex / 4;
            const unsigned int channelIndex = adjustmentIndex % 4;
            const QString propertyName = QStringLiteral("%1 %2").arg(
                QString::fromLatin1(kGroupNames[groupIndex]),
                QString::fromLatin1(kChannelNames[channelIndex]));
            const QString id = QStringLiteral(
                "effect.color.selective_color.%1.%2").arg(
                    QString::fromLatin1(kGroupKeys[groupIndex]),
                    QString::fromLatin1(kChannelKeys[channelIndex]));
            return EffectScalarGizmoParameter{
                id, propertyName, -1.0, 1.0, 0.01, 0.005, 100.0,
                QStringLiteral("%")};
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
                QStringLiteral("effect.color.selective_color.relative_mode"),
                QStringLiteral("Relative Mode")};
        }
        if (index == 2) {
            return EffectViewportToggleParameter{
                QStringLiteral("effect.color.selective_color.preserve_luma"),
                QStringLiteral("Preserve Luma")};
        }
        return std::nullopt;
    }
    void setStrength(float value);
    void setRelativeMode(bool value);
    void setPreserveLuma(bool value);
    void setAdjustment(ArtifactCore::SelectiveColorGroup group, float c, float m, float y, float k);

    const ArtifactCore::SelectiveColorSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
