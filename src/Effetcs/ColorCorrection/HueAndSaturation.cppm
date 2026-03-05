module;

#include <cmath>
#include <algorithm>

module HueAndSaturation;

import std;
import Color.Conversion;

namespace Artifact {

class HueAndSaturation::Impl {
public:
    float hue = 0.0f;          // -180.0 ~ 180.0
    float saturation = 1.0f;   // 0.0 ~ 2.0
    float lightness = 0.0f;    // -1.0 ~ 1.0
    bool colorize = false;     // 単色化を有効にするか
};

HueAndSaturation::HueAndSaturation()
    : ArtifactAbstractEffect(), impl_(new Impl()) {
    setEffectID(UniString("effect.colorcorrection.hsl"));
    setDisplayName(UniString("Hue / Saturation"));
}

HueAndSaturation::~HueAndSaturation() {
    delete impl_;
}

void HueAndSaturation::setHue(float hueShift) {
    impl_->hue = std::clamp(hueShift, -180.0f, 180.0f);
}

float HueAndSaturation::hue() const {
    return impl_->hue;
}

void HueAndSaturation::setSaturation(float saturationScale) {
    impl_->saturation = std::clamp(saturationScale, 0.0f, 2.0f);
}

float HueAndSaturation::saturation() const {
    return impl_->saturation;
}

void HueAndSaturation::setLightness(float lightnessShift) {
    impl_->lightness = std::clamp(lightnessShift, -1.0f, 1.0f);
}

float HueAndSaturation::lightness() const {
    return impl_->lightness;
}

void HueAndSaturation::setColorize(bool colorize) {
    impl_->colorize = colorize;
}

bool HueAndSaturation::isColorize() const {
    return impl_->colorize;
}

void HueAndSaturation::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // Clone source to destination
    dst = src;

    // ピクセルごとの処理の骨格:
    // TODO: r,g,b を ColorConversion::RGBToHSL または RGBToHSV で変換
    // if (colorize) {
    //     // 色相を hue, 彩度を saturation, 明度は元のグレースケール輝度などに依存させる
    // } else {
    //     H = H + hue
    //     S = S * saturation
    //     L/V = L/V + lightness (あるいは乗算・加算の複合)
    // }
    // HSLToRGB / HSVToRGB で r,g,b に戻す。
    // alpha は元のまま。
    //
    // GPU実装はHLSLシェーダ側にて並列処理を実施。
}

std::vector<AbstractProperty> HueAndSaturation::getProperties() const {
    return {
        {"Hue", impl_->hue},
        {"Saturation", impl_->saturation},
        {"Lightness", impl_->lightness},
        {"Colorize", impl_->colorize}
    };
}

void HueAndSaturation::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Hue") {
        setHue(value.toFloat());
    } else if (name == "Saturation") {
        setSaturation(value.toFloat());
    } else if (name == "Lightness") {
        setLightness(value.toFloat());
    } else if (name == "Colorize") {
        setColorize(value.toBool());
    }
}

} // namespace Artifact
