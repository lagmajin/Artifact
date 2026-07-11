module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.VectorFlowGlitch;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class VectorFlowGlitchEffect : public ArtifactAbstractEffect {
private:
    float glitchAmount_ = 20.0f;
    float frequency_ = 0.05f;
    float chromaticAberration_ = 5.0f;
    float edgeFlowInfluence_ = 0.7f;
    float seed_ = 0.0f;
    void syncImpl();

public:
    VectorFlowGlitchEffect();
    ~VectorFlowGlitchEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
