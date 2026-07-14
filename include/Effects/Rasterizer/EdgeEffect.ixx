module;
#include <memory>
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
