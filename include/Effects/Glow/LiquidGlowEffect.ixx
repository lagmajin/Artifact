module;
#include <algorithm>
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.LiquidGlow;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class LiquidGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.55f;
    float radius_ = 16.0f;
    float intensity_ = 1.1f;
    float flowScale_ = 42.0f;
    float distortion_ = 8.0f;
    float phase_ = 0.0f;

    void syncImpls();

public:
    LiquidGlowEffect();
    ~LiquidGlowEffect() override;

    void setThreshold(float value);
    void setRadius(float value);
    void setIntensity(float value);
    void setFlowScale(float value);
    void setDistortion(float value);
    void setPhase(float value);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact
