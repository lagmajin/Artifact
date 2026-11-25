module;
#include <optional>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ApertureShapeBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Render.ROI;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class ApertureShapeBlurEffect : public ArtifactAbstractEffect {
private:
    float radius_ = 18.0f;
    int shape_ = 0;
    float rotation_ = 0.0f;
    float edgeBrightness_ = 0.2f;
    float highlightBoost_ = 0.35f;
    QString psfImagePath_;
    void syncImpl();

public:
    ApertureShapeBlurEffect();
    ~ApertureShapeBlurEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.aperture_shape_blur.radius"),
                QStringLiteral("Radius"),
                0.0, 256.0, 0.1, 0.1, 1.0, QStringLiteral("px")};
        }
        return std::nullopt;
    }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

class DepthBokehEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
protected:
    void apply(const ImageF32x4RGBAWithCache& src,
               ImageF32x4RGBAWithCache& dst) override;
    void onContextUpdated(const EffectContext& context) override;
public:
    DepthBokehEffect();
    ~DepthBokehEffect() override;

    std::optional<EffectScalarGizmoParameter>
    scalarGizmoParameter(unsigned int index) const override {
        if (index == 0) {
            return ArtifactAbstractEffect::scalarGizmoParameter(index);
        }
        if (index == 1) {
            return EffectScalarGizmoParameter{
                QStringLiteral("effect.depth_bokeh.focus_distance"),
                QStringLiteral("focusDistance"),
                0.0, 1.0, 0.001, 0.0025, 1.0, QString()};
        }
        return std::nullopt;
    }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

}
