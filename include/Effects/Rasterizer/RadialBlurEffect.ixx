module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.RadialBlur;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Radial / zoom blur: blurs outward from a center point.
/// mode=0: zoom, mode=1: spin.
class RadialBlurEffect : public ArtifactAbstractEffect {
public:
    RadialBlurEffect();
    ~RadialBlurEffect() override;

    float amount() const;
    void  setAmount(float v);
    int   quality() const;
    void  setQuality(int v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);
    float mode() const;
    void  setMode(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float amount_=0.5f,cx_=0.5f,cy_=0.5f,mode_=0.0f; int quality_=8;
    void syncImpls();
};

} // namespace Artifact
