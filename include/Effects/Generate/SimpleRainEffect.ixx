module;
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Generate.SimpleRain;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class SimpleRainEffect : public ArtifactAbstractEffect {
private:
    float density_ = 0.45f;
    float streakLength_ = 24.0f;
    float speed_ = 1.0f;
    float wind_ = -0.2f;
    float opacity_ = 0.35f;
    float depth_ = 0.65f;
    float splashAmount_ = 0.12f;
    float evolution_ = 0.0f;
    int seed_ = 1337;
    void syncImpl();

public:
    SimpleRainEffect();
    ~SimpleRainEffect() override;

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

}
