module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>
#include <QColor>

export module FillEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.Fill;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class FillEffect : public ArtifactAbstractEffect {
private:
    enum class Preset {
        White = 0,
        Black = 1,
        Red = 2,
        Blue = 3,
        Green = 4,
        Custom = 5
    };

    ArtifactCore::SolidFillSettings settings_;
    Preset preset_ = Preset::White;

    void syncImpls();
    void applyPreset(Preset preset);

public:
    FillEffect();
    ~FillEffect() override;

    void setPreset(int preset);
    int preset() const { return static_cast<int>(preset_); }

    void setColor(const QColor& color);
    void setOpacity(float value);
    void setPreserveAlpha(bool value);

    const ArtifactCore::SolidFillSettings& settings() const { return settings_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

class PatternOverlayEffect : public ArtifactAbstractEffect {
private:
    float amount_ = 0.5f;
    float scale_ = 8.0f;
    int seed_ = 0;
    int pattern_ = 0;

public:
    PatternOverlayEffect();
    ~PatternOverlayEffect() override;

    void setAmount(float value);
    void setScale(float value);
    void setSeed(int value);
    void setPattern(int value);
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return false; }
};

} // namespace Artifact
