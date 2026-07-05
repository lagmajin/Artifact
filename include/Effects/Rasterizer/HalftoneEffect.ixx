module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Halftone;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Halftone dots pattern. Converts luminance to variable-size
/// dots simulating comic-book / pop-art screen printing.
class HalftoneEffect : public ArtifactAbstractEffect {
public:
    HalftoneEffect();
    ~HalftoneEffect() override;

    float dotSize() const;
    void  setDotSize(float v);
    float angle() const;
    void  setAngle(float v);
    float contrast() const;
    void  setContrast(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float dotSize_=8.0f,angle_=45.0f,contrast_=1.0f;
    void syncImpls();
};

} // namespace Artifact
