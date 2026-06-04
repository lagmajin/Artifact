module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.TurbulentDisplace;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class TurbulentDisplaceEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 20.0f;
    float size_ = 30.0f;
    int octaves_ = 4;
    int seed_ = 0;
    void syncImpls();

public:
    TurbulentDisplaceEffect();
    ~TurbulentDisplaceEffect() override;

    float amount() const;
    void setAmount(float v);
    float size() const;
    void setSize(float v);
    int octaves() const;
    void setOctaves(int v);
    int seed() const;
    void setSeed(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
