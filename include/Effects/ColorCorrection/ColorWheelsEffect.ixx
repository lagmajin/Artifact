module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module ColorWheelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ColorCollection.ColorGrading;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ColorWheelsEffect : public ArtifactAbstractEffect {
private:
    ColorWheelType wheelType_ = ColorWheelType::LiftGammaGain;
    ColorWheelParams wheels_;

    void syncImpls();

public:
    ColorWheelsEffect();
    ~ColorWheelsEffect() override;

    void setWheelType(ColorWheelType type) { wheelType_ = type; syncImpls(); }
    ColorWheelType wheelType() const { return wheelType_; }

    void setLift(float r, float g, float b);
    void setGamma(float r, float g, float b);
    void setGain(float r, float g, float b);
    void setOffset(float r, float g, float b);
    void setLiftMaster(float v);
    void setGammaMaster(float v);
    void setGainMaster(float v);
    void setOffsetMaster(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
