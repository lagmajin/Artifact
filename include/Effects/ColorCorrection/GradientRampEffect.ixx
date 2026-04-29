module;
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

    void syncImpls();
    void applyPreset(Preset preset);

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

    int preset() const { return static_cast<int>(preset_); }
    const ArtifactCore::GradientRampSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
