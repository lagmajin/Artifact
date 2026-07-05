module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.LightTrails;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Creates light trails by smearing bright pixels along
/// their motion direction. Like MotionTrail but only applies
/// to pixels above a luminance threshold.
class LightTrailsEffect : public ArtifactAbstractEffect {
public:
    LightTrailsEffect();
    ~LightTrailsEffect() override;

    int   trailLength() const;
    void  setTrailLength(int v);
    float decay() const;
    void  setDecay(float v);
    float luminanceThreshold() const;
    void  setLuminanceThreshold(float v);
    float velocityScale() const;
    void  setVelocityScale(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int   trailLen_ = 6;
    float decay_    = 0.6f, lumaThresh_ = 0.5f, vScale_ = 1.0f;
    void syncImpls();
};

} // namespace Artifact
