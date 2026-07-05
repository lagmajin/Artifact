module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Glow;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Classic bloom/glow effect: extracts bright areas and blurs
/// them back over the original for a soft luminous look.
class GlowEffect : public ArtifactAbstractEffect {
public:
    GlowEffect();
    ~GlowEffect() override;

    float threshold() const;
    void  setThreshold(float v);
    float radius() const;
    void  setRadius(float v);
    float intensity() const;
    void  setIntensity(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float threshold_=0.5f,radius_=20.0f,intensity_=1.0f;
    void syncImpls();
};

} // namespace Artifact
