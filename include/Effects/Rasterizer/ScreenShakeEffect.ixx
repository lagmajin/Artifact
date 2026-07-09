module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ScreenShake;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Animated 2D screen shake (post-shake) for the Rasterizer stage.
/// Offsets the frame using Distortion::makeOffset driven by value noise.
class ScreenShakeEffect : public ArtifactAbstractEffect {
public:
    ScreenShakeEffect();
    ~ScreenShakeEffect() override;

    float amplitudeX() const;
    void  setAmplitudeX(float v);
    float amplitudeY() const;
    void  setAmplitudeY(float v);
    float frequency() const;
    void  setFrequency(float v);
    float decay() const;
    void  setDecay(float v);
    int   seed() const;
    void  setSeed(int v);
    int   wrapMode() const;
    void  setWrapMode(int v);   // 0=clamp, 1=wrap, 2=mirror

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float amplitudeX_=20.0f, amplitudeY_=20.0f, frequency_=2.0f, decay_=0.0f;
    int   seed_=0, wrapMode_=1;
    void syncImpls();
};

} // namespace Artifact
