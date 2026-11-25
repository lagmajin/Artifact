module;
#include <memory>
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Rasterizer.Edge;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Sobel edge detection effect. Optionally uses temporal
/// frame difference to highlight only moving edges.
class EdgeEffect : public ArtifactAbstractEffect {
public:
    EdgeEffect();
    ~EdgeEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1: {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.edge.mode"), QStringLiteral("mode"),
                0.0, 1.0, 1.0, 0.05, 1.0, QString()};
            parameter.choiceCount = 2;
            return parameter;
        }
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.edge.intensity"), QStringLiteral("intensity"),
                0.0, 10.0, 0.01, 0.1, 1.0, QStringLiteral("x")};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.edge.threshold"), QStringLiteral("threshold"),
                0.0, 1.0, 0.01, 0.005, 100.0, QStringLiteral("%")};
        case 4: {
            EffectScalarGizmoParameter parameter{
                QStringLiteral("effect.edge.invert"), QStringLiteral("invert"),
                0.0, 1.0, 1.0, 0.05, 1.0, QString()};
            parameter.choiceCount = 2;
            return parameter;
        }
        default:
            return std::nullopt;
        }
    }

    /// 0=Sobel spatial edges, 1=motion edges (frame diff).
    float mode() const;
    void  setMode(float v);

    /// Edge intensity multiplier.
    float intensity() const;
    void  setIntensity(float v);

    /// Threshold below which edges are suppressed.
    float threshold() const;
    void  setThreshold(float v);

    /// Invert: 0=dark edges on white, 1=white edges on dark.
    float invert() const;
    void  setInvert(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float mode_=0.0f,intensity_=1.0f,threshold_=0.1f,invert_=0.0f;
    void syncImpls();
};

/// Directional inner rim generated from the layer alpha boundary.
/// Angle 0 points right and 90 points down in layer pixel space.
class RimLightEffect : public ArtifactAbstractEffect {
public:
    RimLightEffect();
    ~RimLightEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        switch (index) {
        case 1:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.rim_light.angle"),
                QStringLiteral("Angle"), 0.0, 359.9, 0.1, 0.25,
                1.0, QStringLiteral("°")};
        case 2:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.rim_light.width"),
                QStringLiteral("Width"), 1.0, 64.0, 0.25, 0.125,
                1.0, QStringLiteral("px")};
        case 3:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.rim_light.softness"),
                QStringLiteral("Softness"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        case 4:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.rim_light.intensity"),
                QStringLiteral("Intensity"), 0.0, 5.0, 0.01, 0.0125,
                1.0, QStringLiteral("x")};
        case 5:
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.rim_light.local_mix"),
                QStringLiteral("Mix"), 0.0, 1.0, 0.01, 0.0025,
                100.0, QStringLiteral("%")};
        default:
            return std::nullopt;
        }
    }

    float angle() const;
    void setAngle(float v);
    float width() const;
    void setWidth(float v);
    float softness() const;
    void setSoftness(float v);
    float intensity() const;
    void setIntensity(float v);
    QColor color() const;
    void setColor(const QColor& v);
    float mix() const;
    void setMix(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float angle_ = 315.0f;
    float width_ = 8.0f;
    float softness_ = 0.55f;
    float intensity_ = 1.5f;
    QColor color_{255, 238, 190, 255};
    float mix_ = 1.0f;
    void syncImpl();
};

} // namespace Artifact
