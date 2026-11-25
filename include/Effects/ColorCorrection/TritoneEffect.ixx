module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module TritoneEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Tritone;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class TritoneEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Cinematic = 1,
        TealAndOrange = 2,
        WarmGold = 3,
        ColdBlue = 4,
    };

    TritoneSettings settings_;
    bool customPresetPinned_ = false;

    void syncImpls();
    static TritoneSettings settingsForPreset(Preset preset);
    void applyPreset(Preset preset);

public:
    TritoneEffect();
    ~TritoneEffect() override;

    void setPreset(int preset);
    int preset() const;
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
                QStringLiteral("effect.tritone.master_strength"),
                QStringLiteral("Master Strength"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.tritone.preset"),
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
                QStringLiteral("effect.tritone.preserve_luma"),
                QStringLiteral("Preserve Luma")};
        }
        return std::nullopt;
    }

    void setShadowColor(const QColor& color);
    void setMidtoneColor(const QColor& color);
    void setHighlightColor(const QColor& color);
    void setBalance(float value);
    void setSoftness(float value);
    void setMasterStrength(float value);
    void setColorMix(float value);
    void setPreserveLuma(bool value);

    const TritoneSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
