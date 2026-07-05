module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TemporalDenoise;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Temporal noise reduction using multi-frame averaging.
/// Averages the current frame with the previous N frames
/// using per-pixel variance clamping to avoid ghosting on
/// fast-moving objects.
///
/// Uses EffectTemporalSampleMode::PreviousFrame + relative offset.
class TemporalDenoiseEffect : public ArtifactAbstractEffect {
public:
    TemporalDenoiseEffect();
    ~TemporalDenoiseEffect() override;

    /// 0.0 = no denoise, 1.0 = max blend with history.
    float strength() const;
    void  setStrength(float v);

    /// Number of past frames to reference (1–8).
    int   frameCount() const;
    void  setFrameCount(int v);

    /// Variance threshold: below this, pixel is considered static
    /// and gets full temporal blend; above, blend is reduced.
    float varianceThreshold() const;
    void  setVarianceThreshold(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float strength_          = 0.5f;
    int   frameCount_        = 3;
    float varianceThreshold_ = 0.05f;
    void syncImpls();
};

} // namespace Artifact
