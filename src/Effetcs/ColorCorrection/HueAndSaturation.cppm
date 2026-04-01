module;

#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QVector>
#include <vector>
#include <memory>

module HueAndSaturation;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// CPU 実装: Hue / Saturation / Lightness / Colorize
// OpenCV HSV 変換を使用（既存アルゴリズム維持）
// ─────────────────────────────────────────────────────────

class HueAndSaturationCPUImpl : public ArtifactEffectImplBase {
public:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        cv::Mat mat = dst.toCvMat();
        if (mat.empty()) return;

        cv::Mat bgr, hsv;
        int from_to[] = { 0,2, 1,1, 2,0 };
        bgr.create(mat.size(), CV_32FC3);
        cv::mixChannels(&mat, 1, &bgr, 1, from_to, 3);
        
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

        for (int y = 0; y < hsv.rows; ++y) {
            for (int x = 0; x < hsv.cols; ++x) {
                cv::Vec3f& pixel = hsv.at<cv::Vec3f>(y, x);
                
                if (colorize_) {
                    pixel[0] = std::fmod(hueShift_ + 360.0f, 360.0f);
                    pixel[1] = saturationScale_;
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                } else {
                    pixel[0] = std::fmod(pixel[0] + hueShift_ + 360.0f, 360.0f);
                    pixel[1] = std::clamp(pixel[1] * saturationScale_, 0.0f, 1.0f);
                    pixel[2] = std::clamp(pixel[2] + lightnessShift_, 0.0f, 1.0f);
                }
            }
        }

        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
        
        int back_from_to[] = { 2,0, 1,1, 0,2 };
        cv::mixChannels(&bgr, 1, &mat, 1, back_from_to, 3);
        
        dst.fromCvMat(mat);
    }
};

// ─────────────────────────────────────────────────────────
// GPU 実装 (HLSL) — 現在は CPU フォールバック
// ─────────────────────────────────────────────────────────

class HueAndSaturationGPUImpl : public ArtifactEffectImplBase {
public:
    float hueShift_ = 0.0f;
    float saturationScale_ = 1.0f;
    float lightnessShift_ = 0.0f;
    bool colorize_ = false;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // TODO: Diligent Engine による GPU 実装
        applyCPU(src, dst);
    }

private:
    HueAndSaturationCPUImpl cpuImpl_;
};

// ─────────────────────────────────────────────────────────
// HueAndSaturation 本体
// ─────────────────────────────────────────────────────────

HueAndSaturation::HueAndSaturation() {
    setEffectID(UniString("effect.colorcorrection.hsl"));
    setDisplayName(UniString("Hue / Saturation"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<HueAndSaturationCPUImpl>();
    auto gpuImpl = std::make_shared<HueAndSaturationGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

HueAndSaturation::~HueAndSaturation() = default;

void HueAndSaturation::syncImpls() {
    if (auto* cpu = dynamic_cast<HueAndSaturationCPUImpl*>(cpuImpl_.get())) {
        cpu->hueShift_ = hueShift_;
        cpu->saturationScale_ = saturationScale_;
        cpu->lightnessShift_ = lightnessShift_;
        cpu->colorize_ = colorize_;
    }
    if (auto* gpu = dynamic_cast<HueAndSaturationGPUImpl*>(gpuImpl_.get())) {
        gpu->hueShift_ = hueShift_;
        gpu->saturationScale_ = saturationScale_;
        gpu->lightnessShift_ = lightnessShift_;
        gpu->colorize_ = colorize_;
    }
}

std::vector<AbstractProperty> HueAndSaturation::getProperties() const {
    std::vector<AbstractProperty> props(4);

    props[0].setName("Hue");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(hueShift_)));

    props[1].setName("Saturation");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(saturationScale_)));

    props[2].setName("Lightness");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(lightnessShift_)));

    props[3].setName("Colorize");
    props[3].setType(ArtifactCore::PropertyType::Boolean);
    props[3].setValue(colorize_);

    return props;
}

void HueAndSaturation::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Hue") setHue(value.toFloat());
    else if (name == "Saturation") setSaturation(value.toFloat());
    else if (name == "Lightness") setLightness(value.toFloat());
    else if (name == "Colorize") setColorize(value.toBool());
}

} // namespace Artifact
