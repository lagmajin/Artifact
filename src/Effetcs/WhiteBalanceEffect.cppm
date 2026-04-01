module;
#include <QString>
#include <QVariant>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

module Artifact.Effect.WhiteBalance;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// Kelvin → RGB 変換 (approximation)
// ─────────────────────────────────────────────────────────

static void kelvinToRGB(float kelvin, float& r, float& g, float& b) {
    float temp = kelvin / 100.0f;

    // Red
    if (temp <= 66.0f) r = 1.0f;
    else r = std::clamp(1.2929361860603f * std::pow(temp - 60.0f, -0.1332047592f), 0.0f, 1.0f);

    // Green
    if (temp <= 66.0f) g = std::clamp(0.39008157876902f * std::log(temp) - 0.63184144378863f, 0.0f, 1.0f);
    else g = std::clamp(1.1298908608953f * std::pow(temp - 60.0f, -0.0755148492f), 0.0f, 1.0f);

    // Blue
    if (temp >= 66.0f) b = 1.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = std::clamp(0.54320678911019f * std::log(temp - 10.0f) - 1.19625408914f, 0.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────
// CPU 実装: White Balance
// ─────────────────────────────────────────────────────────

class WhiteBalanceCPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        cv::Mat mat = dst.toCvMat();
        if (mat.empty()) return;

        // Calculate color temperature correction
        float refR, refG, refB;
        kelvinToRGB(6500.0f, refR, refG, refB); // Reference D65

        float targetR, targetG, targetB;
        kelvinToRGB(temperature_, targetR, targetG, targetB);

        // Compute correction factors
        float corrR = targetR / std::max(refR, 0.001f);
        float corrG = targetG / std::max(refG, 0.001f);
        float corrB = targetB / std::max(refB, 0.001f);

        // Tint correction (green ←→ magenta)
        float tintG = 1.0f + tint_ * 0.5f;
        float tintM = 1.0f - tint_ * 0.5f;

        // Brightness
        float brightMul = std::pow(2.0f, brightness_);

        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                // BGR order
                p[2] = std::clamp(p[2] * corrR * tintM * brightMul, 0.0f, 1.0f); // R
                p[1] = std::clamp(p[1] * corrG * tintG * brightMul, 0.0f, 1.0f); // G
                p[0] = std::clamp(p[0] * corrB * tintM * brightMul, 0.0f, 1.0f); // B
            }
        }

        dst.fromCvMat(mat);
    }
};

// ─────────────────────────────────────────────────────────
// GPU 実装 (CPU fallback)
// ─────────────────────────────────────────────────────────

class WhiteBalanceGPUImpl : public ArtifactEffectImplBase {
public:
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float brightness_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // TODO: Diligent Engine による GPU 実装
        applyCPU(src, dst);
    }

private:
    WhiteBalanceCPUImpl cpuImpl_;
};

// ─────────────────────────────────────────────────────────
// WhiteBalanceEffect 本体
// ─────────────────────────────────────────────────────────

WhiteBalanceEffect::WhiteBalanceEffect() {
    setDisplayName(ArtifactCore::UniString("White Balance"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<WhiteBalanceCPUImpl>();
    auto gpuImpl = std::make_shared<WhiteBalanceGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

WhiteBalanceEffect::~WhiteBalanceEffect() = default;

void WhiteBalanceEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<WhiteBalanceCPUImpl*>(cpuImpl_.get())) {
        cpu->temperature_ = temperature_;
        cpu->tint_ = tint_;
        cpu->brightness_ = brightness_;
    }
    if (auto* gpu = dynamic_cast<WhiteBalanceGPUImpl*>(gpuImpl_.get())) {
        gpu->temperature_ = temperature_;
        gpu->tint_ = tint_;
        gpu->brightness_ = brightness_;
    }
}

void WhiteBalanceEffect::setPreset(const QString& preset) {
    if (preset == "Daylight") setTemperature(5500.0f);
    else if (preset == "Cloudy") setTemperature(6500.0f);
    else if (preset == "Shade") setTemperature(7500.0f);
    else if (preset == "Tungsten") setTemperature(3200.0f);
    else if (preset == "Fluorescent") setTemperature(4000.0f);
    else if (preset == "Flash") setTemperature(6000.0f);
}

std::vector<AbstractProperty> WhiteBalanceEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    props.push_back({}); props.back().setName("Temperature (K)"); props.back().setType(PropertyType::Float); props.back().setValue(temperature_);
    props.push_back({}); props.back().setName("Tint"); props.back().setType(PropertyType::Float); props.back().setValue(tint_);
    props.push_back({}); props.back().setName("Brightness"); props.back().setType(PropertyType::Float); props.back().setValue(brightness_);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Enum);
    presetProp.setValue(0);
    presetProp.setEnumLabels({"Custom", "Daylight", "Cloudy", "Shade", "Tungsten", "Fluorescent", "Flash"});
    props.push_back(presetProp);

    return props;
}

void WhiteBalanceEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == "Temperature (K)") setTemperature(value.toFloat());
    else if (key == "Tint") setTint(value.toFloat());
    else if (key == "Brightness") setBrightness(value.toFloat());
    else if (key == "Preset") {
        QStringList presets = {"Custom", "Daylight", "Cloudy", "Shade", "Tungsten", "Fluorescent", "Flash"};
        int idx = value.toInt();
        if (idx > 0 && idx < presets.size()) setPreset(presets[idx]);
    }
}

} // namespace Artifact
