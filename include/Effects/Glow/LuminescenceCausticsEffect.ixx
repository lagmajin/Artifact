module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.LuminescenceCaustics;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LuminescenceCausticsEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.55f;
    float edgeWeight_ = 0.8f;
    float scale_ = 22.0f;
    float intensity_ = 0.75f;
    float evolution_ = 0.0f;
    float colorShift_ = 0.35f;
    void syncImpl();

public:
    LuminescenceCausticsEffect();
    ~LuminescenceCausticsEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
