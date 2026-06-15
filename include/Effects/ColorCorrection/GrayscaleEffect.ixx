module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module GrayscaleEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class GrayscaleEffect : public ArtifactAbstractEffect {
private:
    enum class Mode {
        Perceptual = 0,
        LinearLuminance = 1,
        Desaturate = 2,
    };

    float strength_ = 1.0f;
    Mode mode_ = Mode::LinearLuminance;

    void syncImpls();

public:
    GrayscaleEffect();
    ~GrayscaleEffect() override;

    void setMode(int value);
    int mode() const { return static_cast<int>(mode_); }

    void setStrength(float value);
    float strength() const { return strength_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
