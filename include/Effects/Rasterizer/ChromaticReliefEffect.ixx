module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ChromaticRelief;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class ChromaticReliefEffect : public ArtifactAbstractEffect {
private:
    float reliefAmount_ = 0.7f;
    float chromaticOffset_ = 4.0f;
    float direction_ = 45.0f;
    float edgeSoftness_ = 1.2f;
    float mix_ = 0.8f;
    void syncImpl();

public:
    ChromaticReliefEffect();
    ~ChromaticReliefEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
