module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.ReactionDiffusionBlur;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class ReactionDiffusionBlurEffect : public ArtifactAbstractEffect {
private:
    float blurRadius_ = 8.0f;
    float feed_ = 0.055f;
    float kill_ = 0.062f;
    float patternStrength_ = 0.65f;
    int iterations_ = 18;
    float evolution_ = 0.0f;
    void syncImpl();

public:
    ReactionDiffusionBlurEffect();
    ~ReactionDiffusionBlurEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
