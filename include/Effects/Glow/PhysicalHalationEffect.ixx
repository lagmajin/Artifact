module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.PhysicalHalation;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class PhysicalHalationEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.8f;
    float spread_ = 15.0f;
    float intensity_ = 0.5f;
    float redDiffusion_ = 2.0f;
    float softness_ = 0.5f;
    void syncImpl();

public:
    PhysicalHalationEffect();
    ~PhysicalHalationEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
