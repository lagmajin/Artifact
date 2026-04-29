module;
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

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
