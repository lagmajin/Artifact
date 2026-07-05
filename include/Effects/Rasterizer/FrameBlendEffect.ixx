module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FrameBlend;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Blends the current frame with the previous frame using opacity.
/// Simple temporal cross-fade; useful as a light alternative to
/// motion blur or for creating ghosting transitions.
///
/// Uses EffectTemporalSampleMode::PreviousFrame internally.
class FrameBlendEffect : public ArtifactAbstractEffect {
public:
    FrameBlendEffect();
    ~FrameBlendEffect() override;

    /// 0.0 = current only, 1.0 = previous only.
    float blend() const;
    void  setBlend(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float blend_ = 0.5f;
    void syncImpls();
};

} // namespace Artifact
