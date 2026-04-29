module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module ColorBalanceEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.ColorBalance;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class ColorBalanceEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Neutral = 1,
        CoolShadows = 2,
        WarmHighlights = 3,
        Cinematic = 4,
    };

    ColorBalanceSettings settings_;
    Preset preset_ = Preset::Cinematic;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    ColorBalanceEffect();
    ~ColorBalanceEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setShadowBalance(float r, float g, float b);
    void setMidtoneBalance(float r, float g, float b);
    void setHighlightBalance(float r, float g, float b);
    void setShadowRange(float value);
    void setHighlightRange(float value);
    void setMasterStrength(float value);
    void setPreserveLuma(bool value);

    const ColorBalanceSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
