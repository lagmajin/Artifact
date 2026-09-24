module;
#include <cstdint>
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module PhotoFilterEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.PhotoFilter;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class PhotoFilterEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Warm = 1,
        Cool = 2,
        Sepia = 3,
        Cyan = 4,
        Rose = 5,
    };

    PhotoFilterSettings settings_;
    Preset preset_ = Preset::Warm;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    PhotoFilterEffect();
    ~PhotoFilterEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setColor(const QColor& color);
    void setDensity(float value);
    void setBrightness(float value);
    void setContrast(float value);
    void setSaturationBoost(float value);
    void setPreserveLuma(bool value);

    const PhotoFilterSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    static constexpr const char* kGpuGenericKeyString = "photo_filter";
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
        node.parameters[0] = settings_.color.r;
        node.parameters[1] = settings_.color.g;
        node.parameters[2] = settings_.color.b;
        node.parameters[3] = settings_.density;
        node.parameters[4] = settings_.brightness;
        node.parameters[5] = settings_.contrast;
        node.parameters[6] = settings_.saturationBoost;
        node.parameters[7] = settings_.preserveLuma ? 1.0f : 0.0f;
        return stack.append(node);
    }

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
