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

    void syncImpls();
    void applyPreset(Preset preset);

public:
    ColoramaEffect();
    ~ColoramaEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setSourceMode(ColoramaSourceMode mode);
    void setPalette(ColoramaPalette palette);
    void setPhase(float value);
    void setSpread(float value);
    void setStrength(float value);
    void setSaturationBoost(float value);
    void setContrast(float value);
    void setPreserveLuma(bool value);

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
