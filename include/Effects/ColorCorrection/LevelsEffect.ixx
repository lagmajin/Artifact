module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module LevelsEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.LevelsCurves;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class LevelsEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Normal = 1,
        HighContrast = 2,
        LowContrast = 3,
        Brighten = 4,
        Darken = 5,
    };

    ArtifactCore::LevelsSettings settings_;
    Preset preset_ = Preset::Normal;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    LevelsEffect();
    ~LevelsEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setInputBlack(float value);
    void setInputWhite(float value);
    void setInputGamma(float value);
    void setOutputBlack(float value);
    void setOutputWhite(float value);
    void setPerChannel(bool value);

    const ArtifactCore::LevelsSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
