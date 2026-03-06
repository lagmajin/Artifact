module;

#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

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
    dst = src;
    cv::Mat mat = dst.image().toCVMat();
    if (mat.empty()) return;

    // Convert from RGBA/BGRA float to HSV
    // OpenCV expects BGR float to be in 0.0 - 1.0 range, H will be 0-360, S,V 0-1
    cv::Mat bgr, hsv;
    int from_to[] = { 0,2, 1,1, 2,0 };
    bgr.create(mat.size(), CV_32FC3);
    cv::mixChannels(&mat, 1, &bgr, 1, from_to, 3);
    
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    for (int y = 0; y < hsv.rows; ++y) {
        for (int x = 0; x < hsv.cols; ++x) {
            cv::Vec3f& pixel = hsv.at<cv::Vec3f>(y, x);
            
            if (impl_->colorize) {
                pixel[0] = std::fmod(impl_->hue + 360.0f, 360.0f);
                pixel[1] = impl_->saturation;
                pixel[2] = std::clamp(pixel[2] + impl_->lightness, 0.0f, 1.0f);
            } else {
                pixel[0] = std::fmod(pixel[0] + impl_->hue + 360.0f, 360.0f);
                pixel[1] = std::clamp(pixel[1] * impl_->saturation, 0.0f, 1.0f);
                pixel[2] = std::clamp(pixel[2] + impl_->lightness, 0.0f, 1.0f);
            }
        }
    }

    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    
    // Copy back to RGBA, preserving original alpha
    int back_from_to[] = { 2,0, 1,1, 0,2 };
    cv::mixChannels(&bgr, 1, &mat, 1, back_from_to, 3);
    
    dst.image().setFromCVMat(mat);
    dst.UpdateGpuTextureFromCpuData();
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
