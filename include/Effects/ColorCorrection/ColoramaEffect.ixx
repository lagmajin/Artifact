module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module ColoramaEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Colorama;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ColoramaEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Rainbow = 1,
        Fire = 2,
        Ocean = 3,
        Neon = 4,
        Sunset = 5,
    };

    ColoramaSettings settings_;
    Preset preset_ = Preset::Rainbow;
    bool customPresetPinned_ = false;

    void syncImpls();
    void applyPreset(Preset preset);
    void updatePresetFromSettings();
    static ColoramaSettings settingsForPreset(Preset preset);

public:
    ColoramaEffect();
    ~ColoramaEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }
    bool customPresetPinnedForSnapshot() const { return customPresetPinned_; }
    void restoreCustomPresetPinnedForSnapshot(bool pinned) {
        customPresetPinned_ = pinned;
    }

    void setSourceMode(ColoramaSourceMode mode);
    void setPalette(ColoramaPalette palette);
    void setPhase(float value);
    void setSpread(float value);
    void setStrength(float value);
    void setSaturationBoost(float value);
    void setContrast(float value);
    void setPreserveLuma(bool value);

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.colorama.phase"),
                QStringLiteral("Phase"), 0.0, 1.0, 0.01, 0.0025, 1.0,
                QStringLiteral("cycle")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.colorama.source_mode"),
                QStringLiteral("Source Mode"), 0.0, 1.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 2;
            return parameter;
        }
        if (index == 3) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.colorama.palette"),
                QStringLiteral("Palette"), 0.0, 4.0, 1.0, 0.05, 1.0,
                QString()};
            parameter.choiceCount = 5;
            return parameter;
        }
        if (index == 4) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.colorama.preset"),
                QStringLiteral("Preset"), 0.0, 5.0, 1.0, 0.05, 1.0,
                QString()};
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
                QStringLiteral("effect.colorcorrection.colorama.preserve_luma"),
                QStringLiteral("Preserve Luma")};
        }
        return std::nullopt;
    }

    const ColoramaSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "colorama";
    static constexpr std::uint32_t kGpuGenericKey =
        gpuGenericKeyFromString(kGpuGenericKeyString);
    std::uint32_t gpuGenericKey() const override { return kGpuGenericKey; }

    GpuRasterEffectDomain gpuRasterEffectDomain() const override {
        return GpuRasterEffectDomain::Spatial;
    }

    bool appendGpuSpatialNodes(GpuSpatialEffectStack& stack) const override {
        GpuSpatialEffectNode node;
        node.kind = GpuSpatialEffectKind::Generic;
        node.genericKey = kGpuGenericKey;
        node.parameters[0] = static_cast<float>(settings_.sourceMode);
        node.parameters[1] = static_cast<float>(settings_.palette);
        node.parameters[2] = settings_.phase;
        node.parameters[3] = settings_.spread;
        node.parameters[4] = settings_.strength;
        node.parameters[5] = settings_.saturationBoost;
        node.parameters[6] = settings_.contrast;
        node.parameters[7] = settings_.preserveLuma ? 1.0f : 0.0f;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
