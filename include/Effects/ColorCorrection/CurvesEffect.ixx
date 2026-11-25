module;
#include <utility>
#include <cstdint>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module CurvesEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Memory.SharedPtr;
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
        Posterize = 5,
        Custom = 6
    };

    Preset preset_ = Preset::SCurve;
    float strength_ = 0.5f;
    int posterizeLevels_ = 4;
    std::vector<ArtifactCore::CurvePoint> customPoints_;
    ArtifactCore::SharedPtr<const GpuEffectLookupTable> lookupTable_;

    void syncImpls();
    void applyPreset(ArtifactCore::ColorCurves& curves) const;

public:
    CurvesEffect();
    ~CurvesEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.curves.strength"),
                QStringLiteral("Strength"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        }
        if (index == 2) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.curves.posterize_levels"),
                QStringLiteral("Posterize Levels"), 2.0, 256.0, 1.0, 0.635,
                1.0, QStringLiteral("levels")};
        }
        if (index == 3) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.curves.preset"),
                QStringLiteral("Preset"), 0.0, 6.0, 1.0, 0.05,
                1.0, QString()};
            parameter.choiceCount = 7;
            return parameter;
        }
        return std::nullopt;
    }

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setStrength(float strength);
    float strength() const { return strength_; }

    void setPosterizeLevels(int levels);
    int posterizeLevels() const { return posterizeLevels_; }

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
