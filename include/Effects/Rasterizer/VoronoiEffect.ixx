module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Voronoi;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Voronoi / cellular noise pattern generator.
/// mode: 0=distance to nearest, 1=2nd minus 1st (edge),
/// 2=distance to 2nd, 3=cell value (random per cell).
class VoronoiEffect : public ArtifactAbstractEffect {
public:
    VoronoiEffect();
    ~VoronoiEffect() override;

    float scale() const;
    void  setScale(float v);
    float jitter() const;
    void  setJitter(float v);
    int   mode() const;
    void  setMode(int v);
    int   seed() const;
    void  setSeed(int v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float scale_=20.0f,jitter_=1.0f; int mode_=0,seed_=0;
    void syncImpls();
};

} // namespace Artifact
