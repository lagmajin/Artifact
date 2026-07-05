module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

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

} // namespace Artifact
