module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TemporalMedian;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Temporal median filter: replaces each pixel with the median
/// value across N past frames. Removes moving objects while
/// preserving static background (e.g. tourist removal).
class TemporalMedianEffect : public ArtifactAbstractEffect {
public:
    TemporalMedianEffect();
    ~TemporalMedianEffect() override;

    /// Number of past frames to sample (3-16, odd recommended).
    int   frameCount() const;
    void  setFrameCount(int v);

    /// Blend between median result and original (0=median only).
    float blend() const;
    void  setBlend(float v);

    /// Apply to which channel: 0=all, 1=R, 2=G, 3=B, 4=luma.
    int   channel() const;
    void  setChannel(int v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int frameCount_=5; float blend_=1.0f; int channel_=0;
    void syncImpls();
};

} // namespace Artifact
