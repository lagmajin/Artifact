module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module SelectiveColorEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.SelectiveColor;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class SelectiveColorEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Neutral = 1,
        Warm = 2,
        Cool = 3,
        Vivid = 4,
        Film = 5,
    };

    ArtifactCore::SelectiveColorSettings settings_;
    Preset preset_ = Preset::Neutral;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    SelectiveColorEffect();
    ~SelectiveColorEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setStrength(float value);
    void setRelativeMode(bool value);
    void setPreserveLuma(bool value);
    void setAdjustment(ArtifactCore::SelectiveColorGroup group, float c, float m, float y, float k);

    const ArtifactCore::SelectiveColorSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
