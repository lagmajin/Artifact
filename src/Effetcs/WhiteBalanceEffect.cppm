module;

#include <algorithm>
#include <cmath>
#include <memory>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QStringList>
#include <vector>

module Artifact.Effect.WhiteBalance;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

static void kelvinToRGB(float kelvin, float& r, float& g, float& b) {
    const float temp = kelvin / 100.0f;

    if (temp <= 66.0f) r = 1.0f;
    else r = std::clamp(1.2929361860603f * std::pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

    if (temp <= 66.0f) g = std::clamp(0.39008157876902f * std::log(temp) - 0.63184144378863f, 0.0f, 1.0f);
    else g = std::clamp(1.1298908608953f * std::pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

    if (temp >= 66.0f) b = 1.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = std::clamp(0.54320678911019f * std::log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);
}

class WhiteBalanceCPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        cv::Mat mat = dst.image().toCVMat();
        if (mat.empty()) {
            return;
        }

        float refR = 1.0f;
        float refG = 1.0f;
        float refB = 1.0f;
        kelvinToRGB(6500.0f, refR, refG, refB);

        float targetR = 1.0f;
        float targetG = 1.0f;
        float targetB = 1.0f;
        kelvinToRGB(temperature_, targetR, targetG, targetB);

        const float corrR = targetR / std::max(refR, 0.001f);
        const float corrG = targetG / std::max(refG, 0.001f);
        const float corrB = targetB / std::max(refB, 0.001f);
        const float tintG = 1.0f + tint_ * 0.5f;
        const float tintM = 1.0f - tint_ * 0.5f;
        const float brightMul = std::pow(2.0f, brightness_);

        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                p[2] = std::clamp(p[2] * corrR * tintM * brightMul, 0.0f, 1.0f);
                p[1] = std::clamp(p[1] * corrG * tintG * brightMul, 0.0f, 1.0f);
                p[0] = std::clamp(p[0] * corrB * tintM * brightMul, 0.0f, 1.0f);
            }
        }

        dst.image().setFromCVMat(mat);
    }
};

class WhiteBalanceGPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    WhiteBalanceCPUImpl cpuImpl_;
};

WhiteBalanceEffect::WhiteBalanceEffect() {
    setDisplayName(UniString("White Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<WhiteBalanceCPUImpl>());
    setGPUImpl(std::make_shared<WhiteBalanceGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

WhiteBalanceEffect::~WhiteBalanceEffect() = default;

void WhiteBalanceEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<WhiteBalanceCPUImpl>(cpuImpl())) {
        cpu->temperature_ = temperature_;
        cpu->tint_ = tint_;
        cpu->brightness_ = brightness_;
    }
    if (auto gpu = std::dynamic_pointer_cast<WhiteBalanceGPUImpl>(gpuImpl())) {
        gpu->temperature_ = temperature_;
        gpu->tint_ = tint_;
        gpu->brightness_ = brightness_;
    }
}

void WhiteBalanceEffect::setPreset(const QString& preset) {
    if (preset == QStringLiteral("Daylight")) setTemperature(5500.0f);
    else if (preset == QStringLiteral("Cloudy")) setTemperature(6500.0f);
    else if (preset == QStringLiteral("Shade")) setTemperature(7500.0f);
    else if (preset == QStringLiteral("Tungsten")) setTemperature(3200.0f);
    else if (preset == QStringLiteral("Fluorescent")) setTemperature(4000.0f);
    else if (preset == QStringLiteral("Flash")) setTemperature(6000.0f);
}

std::vector<AbstractProperty> WhiteBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty tempProp;
    tempProp.setName("Temperature (K)");
    tempProp.setType(PropertyType::Float);
    tempProp.setValue(temperature_);
    props.push_back(tempProp);

    AbstractProperty tintProp;
    tintProp.setName("Tint");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tint_);
    props.push_back(tintProp);

    AbstractProperty brightnessProp;
    brightnessProp.setName("Brightness");
    brightnessProp.setType(PropertyType::Float);
    brightnessProp.setValue(brightness_);
    props.push_back(brightnessProp);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(0);
    props.push_back(presetProp);

    return props;
}

void WhiteBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Temperature (K)")) setTemperature(value.toFloat());
    else if (key == QStringLiteral("Tint")) setTint(value.toFloat());
    else if (key == QStringLiteral("Brightness")) setBrightness(value.toFloat());
    else if (key == QStringLiteral("Preset")) {
        const QStringList presets = {
            QStringLiteral("Custom"),
            QStringLiteral("Daylight"),
            QStringLiteral("Cloudy"),
            QStringLiteral("Shade"),
            QStringLiteral("Tungsten"),
            QStringLiteral("Fluorescent"),
            QStringLiteral("Flash")
        };
        const int idx = value.toInt();
        if (idx > 0 && idx < presets.size()) {
            setPreset(presets[idx]);
        }
    }
}

} // namespace Artifact
