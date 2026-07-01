module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Kuwahara;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class KuwaharaEffect : public ArtifactAbstractEffect {
private:
    float radius_ = 5.0f;
    float sharpness_ = 0.5f;
    bool anisotropic_ = false;
    void syncImpls();

public:
    KuwaharaEffect();
    ~KuwaharaEffect() override;

    float radius() const;
    void setRadius(float v);
    float sharpness() const;
    void setSharpness(float v);
    bool anisotropic() const;
    void setAnisotropic(bool v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}

