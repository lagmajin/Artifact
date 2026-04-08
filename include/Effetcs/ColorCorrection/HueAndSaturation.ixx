module;
#include <utility>
#include <vector>
#include <QString>
#include <QVariant>

export module HueAndSaturation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 色相・彩度・明度 (Hue, Saturation, Lightness) エフェクト
// CPU/GPU 両対応、同一アルゴリズム
class HueAndSaturation : public ArtifactAbstractEffect {
private:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void syncImpls();

public:
    HueAndSaturation();
    ~HueAndSaturation() override;

    void setHue(float v) { hueShift_ = std::clamp(v, -180.0f, 180.0f); syncImpls(); }
    float hue() const { return hueShift_; }

    void setSaturation(float v) { saturationScale_ = std::clamp(v, 0.0f, 2.0f); syncImpls(); }
    float saturation() const { return saturationScale_; }

    void setLightness(float v) { lightnessShift_ = std::clamp(v, -1.0f, 1.0f); syncImpls(); }
    float lightness() const { return lightnessShift_; }

    void setColorize(bool v) { colorize_ = v; syncImpls(); }
    bool isColorize() const { return colorize_; }

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override { return true; }
};

} // namespace Artifact
