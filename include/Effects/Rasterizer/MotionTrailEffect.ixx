module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.MotionTrail;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Directional motion trail: samples past frames backward along
/// estimated velocity and blends them into the current frame.
/// Produces clean directional trails (smear, comet tails).
///
/// Similar to Echo but the trail direction follows per-pixel
/// motion instead of a simple temporal decay.
///
/// Uses EffectTemporalSampleMode::PreviousFrame + block matching
/// for velocity estimation, then samples along velocity direction.
class MotionTrailEffect : public ArtifactAbstractEffect {
public:
    MotionTrailEffect();
    ~MotionTrailEffect() override;

    /// Trail length (number of echo steps, 1–16).
    int   trailLength() const;
    void  setTrailLength(int v);

    /// Decay per step (0 = instant fade, 1 = full).
    float decay() const;
    void  setDecay(float v);

    /// Scale velocity estimate (1.0 = raw).
    float velocityScale() const;
    void  setVelocityScale(float v);

    /// 0.0 = additive blend, 1.0 = mix blend.
    float blendMode() const;
    void  setBlendMode(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int   trailLength_    = 6;
    float decay_          = 0.6f;
    float velocityScale_  = 1.0f;
    float blendMode_      = 0.0f;
    void syncImpls();
};

} // namespace Artifact
