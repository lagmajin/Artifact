module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.FilmDamage;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class FilmDamageEffect : public ArtifactAbstractEffect {
private:
    float grain_ = 0.12f;
    float dust_ = 0.08f;
    float scratches_ = 0.1f;
    float gateWeave_ = 1.5f;
    float flicker_ = 0.08f;
    float filmBurn_ = 0.12f;
    float evolution_ = 0.0f;
    int seed_ = 1977;
    void syncImpl();

public:
    FilmDamageEffect();
    ~FilmDamageEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
