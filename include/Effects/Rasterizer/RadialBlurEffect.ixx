module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.RadialBlur;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Radial / zoom blur: blurs outward from a center point.
/// mode=0: zoom, mode=1: spin.
class RadialBlurEffect : public ArtifactAbstractEffect {
public:
    RadialBlurEffect();
    ~RadialBlurEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.radial_blur.amount"), QStringLiteral("amount"),
                0.0, 2.0, 0.01, 0.01, 1.0, QString()};
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.radial_blur.quality"), QStringLiteral("quality"),
                2.0, 32.0, 1.0, 1.0, 1.0, QStringLiteral("samples")};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.radial_blur.center_x"), QStringLiteral("centerX"),
                0.0, 1.0, 0.001, 0.0025, 100.0, QStringLiteral("%")};
        case 4:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.radial_blur.center_y"), QStringLiteral("centerY"),
                0.0, 1.0, 0.001, 0.0025, 100.0, QStringLiteral("%")};
        case 5: {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.radial_blur.mode"), QStringLiteral("mode"),
                0.0, 1.0, 1.0, 0.05, 1.0, QString()};
            parameter.choiceCount = 2;
            return parameter;
        }
        default:
            return std::nullopt;
        }
    }

    float amount() const;
    void  setAmount(float v);
    int   quality() const;
    void  setQuality(int v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);
    float mode() const;
    void  setMode(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float amount_=0.5f,cx_=0.5f,cy_=0.5f,mode_=0.0f; int quality_=8;
    void syncImpls();
};

} // namespace Artifact
