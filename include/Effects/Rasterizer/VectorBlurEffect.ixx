module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.VectorBlur;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// True motion blur using per-pixel velocity vectors.
/// Designed to consume the velocity buffer from
/// ArtifactRenderLayerPipeline (velocityRTV/velocitySRV).
/// GPU compute shader path preferred; CPU fallback uses
/// block matching velocity estimation.
class VectorBlurEffect : public ArtifactAbstractEffect {
public:
    VectorBlurEffect();
    ~VectorBlurEffect() override;

    /// Shutter angle in degrees (0-720). 180=1 frame blur.
    float shutterAngle() const;
    void  setShutterAngle(float v);

    /// Number of motion samples (2-32).
    int   samples() const;
    void  setSamples(int v);

    /// Brightness boost to compensate darkening.
    float exposureCompensation() const;
    void  setExposureCompensation(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return true; }

private:
    float shutterAngle_=180.0f; int samples_=8; float exposureComp_=1.0f;
    void syncImpls();
};

} // namespace Artifact
