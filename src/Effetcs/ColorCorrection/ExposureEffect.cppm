module;

#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QVector>
#include <vector>
#include <memory>

module ExposureEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// CPU 実装: Exposure/Offset/Gamma (同一アルゴリズム)
// ─────────────────────────────────────────────────────────

class ExposureEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float exposure_ = 0.0f;
    float offset_ = 0.0f;
    float gammaCorrection_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        cv::Mat mat = dst.toCvMat();
        if (mat.empty()) return;

        float exposureMultiplier = std::pow(2.0f, exposure_);
        float offsetVal = offset_;
        float gammaInv = 1.0f / std::max(0.0001f, gammaCorrection_);

        // CV_32FC4 前提
        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& pixel = mat.at<cv::Vec4f>(y, x);
                for (int c = 0; c < 3; ++c) {
                    float val = pixel[c] * exposureMultiplier;
                    val += offsetVal;
                    val = std::max(0.0f, val);
                    if (gammaInv != 1.0f) {
                        val = std::pow(val, gammaInv);
                    }
                    pixel[c] = std::clamp(val, 0.0f, 1.0f);
                }
            }
        }

        dst.fromCvMat(mat);
    }
};

// ─────────────────────────────────────────────────────────
// GPU 実装 (HLSL) — 現在は CPU フォールバック
// ─────────────────────────────────────────────────────────

class ExposureEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float exposure_ = 0.0f;
    float offset_ = 0.0f;
    float gammaCorrection_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // TODO: Diligent Engine による GPU 実装
        // 1. 入力テクスチャをバインド
        // 2. Constant Buffer に exposureMultiplier, offset, gammaInv を設定
        // 3. ExposurePSO で全画面クアッド描画
        // 現在は CPU フォールバック（同一アルゴリズム）
        applyCPU(src, dst);
    }

private:
    ExposureEffectCPUImpl cpuImpl_;
};

// ─────────────────────────────────────────────────────────
// ExposureEffect 本体
// ─────────────────────────────────────────────────────────

ExposureEffect::ExposureEffect() {
    setEffectID(UniString("effect.colorcorrection.exposure"));
    setDisplayName(UniString("Exposure"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<ExposureEffectCPUImpl>();
    auto gpuImpl = std::make_shared<ExposureEffectGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

ExposureEffect::~ExposureEffect() = default;

void ExposureEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ExposureEffectCPUImpl*>(cpuImpl_.get())) {
        cpu->exposure_ = exposure_;
        cpu->offset_ = offset_;
        cpu->gammaCorrection_ = gammaCorrection_;
    }
    if (auto* gpu = dynamic_cast<ExposureEffectGPUImpl*>(gpuImpl_.get())) {
        gpu->exposure_ = exposure_;
        gpu->offset_ = offset_;
        gpu->gammaCorrection_ = gammaCorrection_;
    }
}

std::vector<AbstractProperty> ExposureEffect::getProperties() const {
    std::vector<AbstractProperty> props(3);

    props[0].setName("Exposure");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(exposure_)));

    props[1].setName("Offset");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(offset_)));

    props[2].setName("Gamma");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(gammaCorrection_)));

    return props;
}

void ExposureEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Exposure") setExposure(value.toFloat());
    else if (name == "Offset") setOffset(value.toFloat());
    else if (name == "Gamma") setGammaCorrection(value.toFloat());
}

} // namespace Artifact
