module;
#include <algorithm>
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.ResidualGlow;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ResidualGlowEffect : public ArtifactAbstractEffect {
private:
    float threshold_ = 0.6f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float decay_ = 0.82f;
    float historyMix_ = 0.75f;

    void syncImpls();

public:
    ResidualGlowEffect();
    ~ResidualGlowEffect() override;

    void setThreshold(float value);
    void setRadius(float value);
    void setIntensity(float value);
    void setDecay(float value);
    void setHistoryMix(float value);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact
