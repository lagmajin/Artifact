module;
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Echo;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact
{
using namespace ArtifactCore;

/// Echo / Afterimage temporal effect.
///
/// Blends the current frame with N past frames using a configurable
/// decay factor, producing a motion-trail / afterimage look.
/// Uses EffectTemporalSampleMode::PreviousFrame internally.
///
/// Compatible with the existing EffectInputRequest temporal contract:
///   - temporalSampleMode = PreviousFrame
///   - temporalLookback   = echoCount
///   - requiresFrameCache  = true
class EchoEffect : public ArtifactAbstractEffect
{
public:
    EchoEffect();
    ~EchoEffect() override;

    /// Number of past frames to blend (1–16).
    int  echoCount() const;
    void setEchoCount(int value);

    /// Decay per echo step. 1.0 = no decay, 0.0 = invisible.
    float decay() const;
    void  setDecay(float value);

    /// Blend operator: 0.0 = additive (recommended default), 1.0 = mix.
    float blendOperator() const;
    void  setBlendOperator(float value);

    // ---- Property interface ----
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return false; }

private:
    int   echoCount_      = 4;
    float decay_          = 0.5f;
    float blendOperator_  = 0.0f;

    void syncImpls();
};

} // namespace Artifact
