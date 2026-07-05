module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Bricks;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Brick / tile pattern generator with configurable offset.
class BricksEffect : public ArtifactAbstractEffect {
public:
    BricksEffect();
    ~BricksEffect() override;

    float brickWidth() const;
    void  setBrickWidth(float v);
    float brickHeight() const;
    void  setBrickHeight(float v);
    float mortarWidth() const;
    void  setMortarWidth(float v);
    float offset() const;
    void  setOffset(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float bw_=64.0f,bh_=32.0f,mortar_=3.0f,offset_=0.5f;
    void syncImpls();
};

} // namespace Artifact
