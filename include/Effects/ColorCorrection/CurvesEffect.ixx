module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module CurvesEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class CurvesEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Identity = 0,
        SCurve = 1,
        FadeIn = 2,
        FadeOut = 3,
        Invert = 4,
        Posterize = 5
    };

    Preset preset_ = Preset::SCurve;
    float strength_ = 0.5f;
    int posterizeLevels_ = 4;

    void syncImpls();
    void applyPreset(ArtifactCore::ColorCurves& curves) const;

public:
    CurvesEffect();
    ~CurvesEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setStrength(float strength);
    float strength() const { return strength_; }

    void setPosterizeLevels(int levels);
    int posterizeLevels() const { return posterizeLevels_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
