module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Glitch;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Digital glitch: random scanline corruption, color channel
/// shifting, and block displacement. Different from DataMosh
/// (which freezes blocks) — this corrupts scanlines.
class GlitchEffect : public ArtifactAbstractEffect {
public:
    GlitchEffect();
    ~GlitchEffect() override;

    float intensity() const;
    void  setIntensity(float v);
    float colorShift() const;
    void  setColorShift(float v);
    float scanlines() const;
    void  setScanlines(float v);
    int   seed() const;
    void  setSeed(int v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float intensity_=0.3f,colorShift_=0.5f,scanlines_=0.5f; int seed_=0;
    void syncImpls();
};

} // namespace Artifact
