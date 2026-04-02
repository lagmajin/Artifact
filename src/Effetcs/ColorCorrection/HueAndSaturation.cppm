module;
#include <cmath>
#include <opencv2/opencv.hpp>
#include <memory>

#include <QList>

module HueAndSaturation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class HueAndSaturationCPUImpl : public ArtifactEffectImplBase {
public:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cv::Mat srcMat = src.image().toCVMat();
        if (srcMat.empty()) {
            dst = src;
            return;
        }

        cv::Mat floatMat;
        if (srcMat.depth() == CV_32F) {
            floatMat = srcMat;
        } else if (srcMat.depth() == CV_8U) {
            srcMat.convertTo(floatMat, CV_32F, 1.0 / 255.0);
        } else if (srcMat.depth() == CV_16U) {
            srcMat.convertTo(floatMat, CV_32F, 1.0 / 65535.0);
        } else {
            srcMat.convertTo(floatMat, CV_32F);
        }

        if (floatMat.channels() == 1) {
            cv::cvtColor(floatMat, floatMat, cv::COLOR_GRAY2BGRA);
        } else if (floatMat.channels() == 3) {
            cv::cvtColor(floatMat, floatMat, cv::COLOR_BGR2BGRA);
        } else if (floatMat.channels() != 4) {
            dst.image().setFromCVMat(srcMat);
            return;
        }

        std::vector<cv::Mat> channels;
        cv::split(floatMat, channels);
        cv::Mat bgr;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, bgr);
        cv::Mat alpha = channels[3];

        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

        for (int y = 0; y < hsv.rows; ++y) {
            for (int x = 0; x < hsv.cols; ++x) {
                cv::Vec3f& pixel = hsv.at<cv::Vec3f>(y, x);
                if (colorize_) {
                    pixel[0] = std::fmod(hueShift_ + 360.0f, 360.0f);
                    pixel[1] = std::clamp(saturationScale_, 0.0f, 2.0f);
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                } else {
                    pixel[0] = std::fmod(pixel[0] + hueShift_ + 360.0f, 360.0f);
                    pixel[1] = std::clamp(pixel[1] * saturationScale_, 0.0f, 2.0f);
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                }
            }
        }

        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
        cv::merge(std::vector<cv::Mat>{bgr, alpha}, floatMat);
        dst.image().setFromCVMat(floatMat);
    }
};

class HueAndSaturationGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

    void setHue(float v) { cpuImpl_.hueShift_ = v; }
    void setSaturation(float v) { cpuImpl_.saturationScale_ = v; }
    void setLightness(float v) { cpuImpl_.lightnessShift_ = v; }
    void setColorize(bool v) { cpuImpl_.colorize_ = v; }

private:
    HueAndSaturationCPUImpl cpuImpl_;
};

HueAndSaturation::HueAndSaturation() {
    setEffectID(UniString("effect.colorcorrection.hsl"));
    setDisplayName(UniString("Hue / Saturation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<HueAndSaturationCPUImpl>());
    setGPUImpl(std::make_shared<HueAndSaturationGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

HueAndSaturation::~HueAndSaturation() = default;

void HueAndSaturation::syncImpls() {
    if (auto* cpu = dynamic_cast<HueAndSaturationCPUImpl*>(cpuImpl().get())) {
        cpu->hueShift_ = hueShift_;
        cpu->saturationScale_ = saturationScale_;
        cpu->lightnessShift_ = lightnessShift_;
        cpu->colorize_ = colorize_;
    }
    if (auto* gpu = dynamic_cast<HueAndSaturationGPUImpl*>(gpuImpl().get())) {
        gpu->setHue(hueShift_);
        gpu->setSaturation(saturationScale_);
        gpu->setLightness(lightnessShift_);
        gpu->setColorize(colorize_);
    }
}

std::vector<AbstractProperty> HueAndSaturation::getProperties() const {
    std::vector<AbstractProperty> props(4);

    props[0].setName("Hue");
    props[0].setType(PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(hueShift_)));

    props[1].setName("Saturation");
    props[1].setType(PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(saturationScale_)));

    props[2].setName("Lightness");
    props[2].setType(PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(lightnessShift_)));

    props[3].setName("Colorize");
    props[3].setType(PropertyType::Boolean);
    props[3].setValue(colorize_);

    return props;
}

void HueAndSaturation::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QString("Hue")) {
        hueShift_ = std::clamp(value.toFloat(), -180.0f, 180.0f);
        syncImpls();
    } else if (key == QString("Saturation")) {
        saturationScale_ = std::clamp(value.toFloat(), 0.0f, 2.0f);
        syncImpls();
    } else if (key == QString("Lightness")) {
        lightnessShift_ = std::clamp(value.toFloat(), -1.0f, 1.0f);
        syncImpls();
    } else if (key == QString("Colorize")) {
        colorize_ = value.toBool();
        syncImpls();
    }
}

} // namespace Artifact
