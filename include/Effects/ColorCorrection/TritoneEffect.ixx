module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module TritoneEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Tritone;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class TritoneEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        Custom = 0,
        Cinematic = 1,
        TealAndOrange = 2,
        WarmGold = 3,
        ColdBlue = 4,
    };

    TritoneSettings settings_;
    Preset preset_ = Preset::Cinematic;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    TritoneEffect();
    ~TritoneEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setShadowColor(const QColor& color);
    void setMidtoneColor(const QColor& color);
    void setHighlightColor(const QColor& color);
    void setBalance(float value);
    void setSoftness(float value);
    void setMasterStrength(float value);
    void setColorMix(float value);
    void setPreserveLuma(bool value);

    const TritoneSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
