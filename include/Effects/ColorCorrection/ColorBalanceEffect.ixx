module;
#include <utility>
#include <cstdint>
#include <vector>
#include <QString>
#include <QVariant>

export module ColorBalanceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ColorBalance;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ColorBalanceEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Neutral = 1,
        CoolShadows = 2,
        WarmHighlights = 3,
        Cinematic = 4,
    };

    ColorBalanceSettings settings_;
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    ColorBalanceEffect();
    ~ColorBalanceEffect() override;

    void setPreset(int preset);
    int preset() const;
    // Keep explicit Custom separate from a Custom value derived from the settings.
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
                QStringLiteral("effect.color.balance.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025, 100.0,
                QStringLiteral("%")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.balance.preset"),
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
                QStringLiteral("effect.color.balance.preserve_luma"),
                QStringLiteral("Preserve Luma")};
        }
        return std::nullopt;
    }

    void setShadowBalance(float r, float g, float b);
    void setMidtoneBalance(float r, float g, float b);
    void setHighlightBalance(float r, float g, float b);
    void setShadowRange(float value);
    void setHighlightRange(float value);
    void setMasterStrength(float value);
    void setPreserveLuma(bool value);

    const ColorBalanceSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
    std::uint32_t gpuGenericKey() const override;
    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }
    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override;
};

} // namespace Artifact
