module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.PosterizeTime;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Reduces the effective frame rate by holding the output
/// for N frames before updating. Creates stop-motion / low-FPS look.
class PosterizeTimeEffect : public ArtifactAbstractEffect {
public:
    PosterizeTimeEffect();
    ~PosterizeTimeEffect() override;

    float frameRate() const;
    void  setFrameRate(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float frameRate_ = 12.0f;
    void syncImpls();
};

} // namespace Artifact
