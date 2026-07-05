module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FrameAverage;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Simple multi-frame average (no variance clamping).
/// Produces ghost-free temporal smoothing; less aggressive
/// than TemporalDenoise for static scenes.
class FrameAverageEffect : public ArtifactAbstractEffect {
public:
    FrameAverageEffect();
    ~FrameAverageEffect() override;

    /// Number of past frames to average (1-16).
    int   frameCount() const;
    void  setFrameCount(int v);

    /// Weight decay per older frame (0=equal, 1=current only).
    float temporalWeight() const;
    void  setTemporalWeight(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int   frameCount_ = 4;
    float temporalWeight_ = 0.9f;
    void syncImpls();
};

} // namespace Artifact
