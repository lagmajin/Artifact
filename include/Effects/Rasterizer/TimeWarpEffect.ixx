module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TimeWarp;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Per-pixel temporal remapping: bright pixels pull from
/// future frames, dark pixels pull from past frames.
/// Creates morph-like temporal distortion.
class TimeWarpEffect : public ArtifactAbstractEffect {
public:
    TimeWarpEffect();
    ~TimeWarpEffect() override;

    float maxOffsetFrames() const;
    void  setMaxOffsetFrames(float v);
    int   channel() const;
    void  setChannel(int v); // 0=luma,1=R,2=G,3=B,4=A
    float smoothness() const;
    void  setSmoothness(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float maxOffset_=8.0f; int channel_=0; float smoothness_=0.5f;
    void syncImpls();
};

} // namespace Artifact
