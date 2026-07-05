module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FrameAccumulation;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Long-exposure style frame accumulation.
/// Continuously adds incoming frames into a persistent buffer
/// with configurable decay. Produces light trails, star trails,
/// and motion smear effects.
///
/// Uses EffectTemporalSampleMode::PreviousFrame internally
/// to access the previous accumulator state.
class FrameAccumulationEffect : public ArtifactAbstractEffect {
public:
    FrameAccumulationEffect();
    ~FrameAccumulationEffect() override;

    /// How fast the accumulated buffer fades (0=instant, 1=infinite).
    float persistence() const;
    void  setPersistence(float v);

    /// 0.0 = only accumulation, 1.0 = only current frame.
    float blend() const;
    void  setBlend(float v);

    /// Reset the accumulation buffer to black.
    void resetAccumulation();

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float persistence_ = 0.95f;
    float blend_       = 0.3f;
    void syncImpls();
};

} // namespace Artifact
