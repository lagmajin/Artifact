module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.LinearWipe;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LinearWipeEffect : public ArtifactAbstractEffect {
private:
    float angle_ = 0.0f;
    float softness_ = 0.1f;
    float feather_ = 0.0f;
    void syncImpls();

public:
    LinearWipeEffect();
    ~LinearWipeEffect() override;

    float angle() const;
    void setAngle(float v);
    float softness() const;
    void setSoftness(float v);
    float feather() const;
    void setFeather(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}
