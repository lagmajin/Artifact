module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FindEdges;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class FindEdgesEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 1.0f;
    bool invert_ = false;
    void syncImpls();

public:
    FindEdgesEffect();
    ~FindEdgesEffect() override;

    float amount() const;
    void setAmount(float v);
    bool invert() const;
    void setInvert(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}
