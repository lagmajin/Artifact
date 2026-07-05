module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.DifferenceMatte;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Extracts the pixel difference between current and a reference
/// frame. Useful for detecting motion or isolating changes.
class DifferenceMatteEffect : public ArtifactAbstractEffect {
public:
    DifferenceMatteEffect();
    ~DifferenceMatteEffect() override;

    /// How many frames back to use as reference (1-N).
    int   referenceOffset() const;
    void  setReferenceOffset(int v);

    /// 0=raw difference, 1=thresholded binary matte.
    float threshold() const;
    void  setThreshold(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    int   refOffset_ = 1;
    float threshold_ = 0.0f;
    void syncImpls();
};

} // namespace Artifact
