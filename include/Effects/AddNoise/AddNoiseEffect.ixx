module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module Artifact.Effect.Rasterizer.AddNoise;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class AddNoiseEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 0.15f;
    float size_ = 1.0f;
    bool colorNoise_ = true;
    bool monochrome_ = false;
    int seed_ = 0;
    void syncImpls();

public:
    AddNoiseEffect();
    ~AddNoiseEffect() override;

    float amount() const;
    void setAmount(float v);
    float size() const;
    void setSize(float v);
    bool colorNoise() const;
    void setColorNoise(bool v);
    bool monochrome() const;
    void setMonochrome(bool v);
    int seed() const;
    void setSeed(int v);

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}
