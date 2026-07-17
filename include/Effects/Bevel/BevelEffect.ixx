module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Bevel;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class BevelEffect : public ArtifactAbstractEffect {
private:
    float strength_ = 1.0f;
    float softness_ = 2.0f;
    bool edgeMode_ = false;
    void syncImpls();

public:
    BevelEffect();
    ~BevelEffect() override;

    float strength() const;
    void setStrength(float v);
    float softness() const;
    void setSoftness(float v);
    bool edgeMode() const;
    void setEdgeMode(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

}
