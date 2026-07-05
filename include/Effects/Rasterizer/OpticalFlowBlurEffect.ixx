module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.OpticalFlowBlur;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// High-quality directional blur using dense optical flow
/// estimation (Lucas-Kanade per-pixel). More accurate than
/// TemporalSmear's block matching at the cost of CPU time.
class OpticalFlowBlurEffect : public ArtifactAbstractEffect {
public:
    OpticalFlowBlurEffect();
    ~OpticalFlowBlurEffect() override;

    float blurAmount() const;
    void  setBlurAmount(float v);
    int   sampleCount() const;
    void  setSampleCount(int v);
    float flowSmoothness() const;
    void  setFlowSmoothness(float v);
    float velocityScale() const;
    void  setVelocityScale(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float blurAmount_ = 0.5f;
    int   sampleCount_ = 8;
    float flowSmoothness_ = 0.5f;
    float velocityScale_ = 1.0f;
    void syncImpls();
};

} // namespace Artifact
