module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ApertureShapeBlur;

import Artifact.Effect.Abstract;
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
    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    EffectROIHint roiHint() const override;
};

}
