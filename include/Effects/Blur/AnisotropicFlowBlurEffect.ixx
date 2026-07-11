module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.AnisotropicFlowBlur;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class AnisotropicFlowBlurEffect : public ArtifactAbstractEffect {
private:
    float blurAmount_ = 10.0f;
    float tensorNoiseScale_ = 1.0f;
    float tensorIntegrationScale_ = 4.0f;
    float edgeAdherence_ = 0.8f;
    void syncImpl();

public:
    AnisotropicFlowBlurEffect();
    ~AnisotropicFlowBlurEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
