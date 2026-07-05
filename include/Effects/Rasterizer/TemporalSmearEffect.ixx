module;
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TemporalSmear;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact
{
using namespace ArtifactCore;

/// GPU velocity-buffer temporal smear effect.
///
/// Reads the current frame color buffer and the velocity buffer
/// (RFloat32x2, pixel/frame motion vectors) from the RenderPipeline.
/// Smears pixels along their velocity direction to produce
/// motion trails, temporal blur, and streak effects.
///
/// Uses EffectTemporalSampleMode::None for input (no extra
/// temporal sampling needed — velocity is pre-computed) but
/// relies on host-side velocity buffer provision.
///
/// GPU path: compute shader TemporalSmearCS.hlsl.
/// CPU path: approximate linear sampling fallback.
class TemporalSmearEffect : public ArtifactAbstractEffect
{
public:
    TemporalSmearEffect();
    ~TemporalSmearEffect() override;

    /// 0.0 = no smear, 1.0 = full velocity trail length.
    float smearAmount() const;
    void  setSmearAmount(float value);

    /// Number of samples along velocity (2–32, higher = smoother).
    int   sampleCount() const;
    void  setSampleCount(int value);

    /// Jitter noise to reduce banding (0 = off, 1 = max).
    float sampleJitter() const;
    void  setSampleJitter(float value);

    /// Scale factor applied to velocity vectors (1.0 = raw pixel/frame).
    float velocityScale() const;
    void  setVelocityScale(float value);

    // ---- Property interface ----
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }

private:
    float smearAmount_    = 0.5f;
    int   sampleCount_    = 8;
    float sampleJitter_   = 0.3f;
    float velocityScale_  = 1.0f;

    void syncImpls();
};

} // namespace Artifact
