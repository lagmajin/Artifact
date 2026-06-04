module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.Sharpen;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class SharpenEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 1.0f;
    float sigma_ = 1.0f;
    float threshold_ = 0.0f;

    void syncImpls();

public:
    SharpenEffect();
    ~SharpenEffect() override;

    float amount() const;
    void setAmount(float v);
    float sigma() const;
    void setSigma(float v);
    float threshold() const;
    void setThreshold(float v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
