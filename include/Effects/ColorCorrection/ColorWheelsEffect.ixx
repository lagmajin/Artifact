module;
#include <utility>
#include <cstdint>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module ColorWheelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ColorWheelsEffect : public ArtifactAbstractEffect {
private:
    ColorWheelType wheelType_ = ColorWheelType::LiftGammaGain;
    ColorWheelParams wheels_;

    void syncImpls();

public:
    ColorWheelsEffect();
    ~ColorWheelsEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.colorcorrection.colorwheels.lift_master"),
                QStringLiteral("Lift Master"), -2.0, 2.0, 0.01, 0.01,
                1.0, QStringLiteral("linear")};
        }
        if (index == 2) {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.color.wheels.wheel_model"),
                QStringLiteral("Wheel Type"), 0.0, 3.0, 1.0, 0.05,
                1.0, QString()};
            parameter.choiceCount = 4;
            return parameter;
        }
        return std::nullopt;
    }

    void setWheelType(ColorWheelType type) { wheelType_ = type; syncImpls(); }
    ColorWheelType wheelType() const { return wheelType_; }

    void setLift(float r, float g, float b);
    void setGamma(float r, float g, float b);
    void setGain(float r, float g, float b);
    void setOffset(float r, float g, float b);
    void setLiftMaster(float v);
    void setGammaMaster(float v);
    void setGainMaster(float v);
    void setOffsetMaster(float v);

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
