module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ChromaticAberration;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Chromatic aberration: shifts R/B channels outward/inward
/// from center, simulating lens color fringing.
class ChromaticAberrationEffect : public ArtifactAbstractEffect {
public:
    ChromaticAberrationEffect();
    ~ChromaticAberrationEffect() override;

    float redShift() const;
    void  setRedShift(float v);
    float blueShift() const;
    void  setBlueShift(float v);
    float centerX() const;
    void  setCenterX(float v);
    float centerY() const;
    void  setCenterY(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float redShift_=2.0f,blueShift_=2.0f,cx_=0.5f,cy_=0.5f;
    void syncImpls();
};

} // namespace Artifact
