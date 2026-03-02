module;

#include <QString>
#include <QVariant>

export module HueAndSaturation;

import std;
import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 色相・彩度・明度 (Hue, Saturation, Lightness) エフェクト
class HueAndSaturation : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;

public:
    HueAndSaturation();
    ~HueAndSaturation() override;

    // 色相シフト (-180.0 ~ 180.0, default: 0.0) -> HSV空間のHに加算
    void setHue(float hueShift);
    float hue() const;

    // 彩度スケール (0.0 ~ 2.0, default: 1.0) -> HSV空間のSに乗算
    void setSaturation(float saturationScale);
    float saturation() const;

    // 明度シフト (-1.0 ~ 1.0, default: 0.0) -> HSV空間のVに加算（または Lに乗算等）
    void setLightness(float lightnessShift);
    float lightness() const;

    // カラライズ（単色化）の有効化
    void setColorize(bool colorize);
    bool isColorize() const;

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact
