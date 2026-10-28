module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.VHSTape;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class VHSTapeEffect : public ArtifactAbstractEffect {
private:
    float trackingError_ = 0.3f;
    float noiseAmount_ = 0.15f;
    float colorBleed_ = 0.5f;
    float scanlineGap_ = 2.0f;
    float scanlineIntensity_ = 0.2f;
    float wobble_ = 0.2f;
    float saturation_ = 0.8f;
    float sharpness_ = 0.6f;
    float bottomJitter_ = 0.5f;
    float evolution_ = 0.0f;
    int seed_ = 42;
    bool showOsd_ = false;
    int osdMode_ = 0;

    void syncImpl();

public:
    VHSTapeEffect();
    ~VHSTapeEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
