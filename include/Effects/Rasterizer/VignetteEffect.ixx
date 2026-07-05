module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Vignette;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Soft vignette: darkens edges with configurable center,
/// radius, and feathering.
class VignetteEffect : public ArtifactAbstractEffect {
public:
    VignetteEffect();
    ~VignetteEffect() override;

    float amount() const;
    void  setAmount(float v);
    float radius() const;
    void  setRadius(float v);
    float feather() const;
    void  setFeather(float v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float amount_=0.7f,radius_=0.8f,feather_=0.4f,cx_=0.5f,cy_=0.5f;
    void syncImpls();
};

} // namespace Artifact
