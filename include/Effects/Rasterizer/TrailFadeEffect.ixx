module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TrailFade;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Distance-based trail fading: samples along velocity but
/// weights by distance from original pixel.
/// Closer = brighter, farther = dimmer.
class TrailFadeEffect : public ArtifactAbstractEffect {
public:
    TrailFadeEffect();
    ~TrailFadeEffect() override;

    int   trailLength() const;
    void  setTrailLength(int v);
    float fadePower() const;  // 1=linear, 2=quadratic, 3=cubic
    void  setFadePower(float v);
    float velocityScale() const;
    void  setVelocityScale(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int trailLen_=8; float fadePower_=2.0f, vScale_=1.0f;
    void syncImpls();
};

} // namespace Artifact
