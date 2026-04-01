module;

#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <QVariant>
#include <QVector>
#include <vector>
#include <memory>

module BrightnessEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// CPU 実装: Brightness / Contrast / Highlights / Shadows
// ─────────────────────────────────────────────────────────

class BrightnessEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        cv::Mat mat = dst.toCvMat();
        if (mat.empty()) return;

        // Contrast factor: factor = (1 + contrast) / (1 - contrast)
        float contrastFactor = (contrast_ != 1.0f) ? (1.0f + contrast_) / (1.0f - contrast_) : 100.0f;

        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& pixel = mat.at<cv::Vec4f>(y, x);
                for (int c = 0; c < 3; ++c) {
                    float val = pixel[c];

                    // 1. Brightness: 単純加算
                    val += brightness_;

                    // 2. Contrast: 中間値(0.5)を基準にスケーリング
                    val = contrastFactor * (val - 0.5f) + 0.5f;

                    // 3. Highlights: 明るい部分 (val > 0.5) のみ
                    if (val > 0.5f) {
                        float highlightWeight = (val - 0.5f) * 2.0f; // 0..1
                        val += highlights_ * highlightWeight * 0.5f;
                    }

                    // 4. Shadows: 暗い部分 (val < 0.5) のみ
                    if (val < 0.5f) {
                        float shadowWeight = (0.5f - val) * 2.0f; // 0..1
                        val += shadows_ * shadowWeight * 0.5f;
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

class BrightnessEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // TODO: Diligent Engine による GPU 実装
        // Constant Buffer に brightness, contrastFactor, highlights, shadows
        // 全画面クアッドで per-pixel 処理
        applyCPU(src, dst);
    }

private:
    BrightnessEffectCPUImpl cpuImpl_;
};

// ─────────────────────────────────────────────────────────
// BrightnessEffect 本体
// ─────────────────────────────────────────────────────────

BrightnessEffect::BrightnessEffect() {
    setEffectID(UniString("effect.colorcorrection.brightness"));
    setDisplayName(UniString("Brightness / Contrast"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<BrightnessEffectCPUImpl>();
    auto gpuImpl = std::make_shared<BrightnessEffectGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

BrightnessEffect::~BrightnessEffect() = default;

void BrightnessEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<BrightnessEffectCPUImpl*>(cpuImpl_.get())) {
        cpu->brightness_ = brightness_;
        cpu->contrast_ = contrast_;
        cpu->highlights_ = highlights_;
        cpu->shadows_ = shadows_;
    }
    if (auto* gpu = dynamic_cast<BrightnessEffectGPUImpl*>(gpuImpl_.get())) {
        gpu->brightness_ = brightness_;
        gpu->contrast_ = contrast_;
        gpu->highlights_ = highlights_;
        gpu->shadows_ = shadows_;
    }
}

std::vector<AbstractProperty> BrightnessEffect::getProperties() const {
    std::vector<AbstractProperty> props(4);

    props[0].setName("Brightness");
    props[0].setType(ArtifactCore::PropertyType::Float);
    props[0].setValue(QVariant(static_cast<double>(brightness_)));

    props[1].setName("Contrast");
    props[1].setType(ArtifactCore::PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(contrast_)));

    props[2].setName("Highlights");
    props[2].setType(ArtifactCore::PropertyType::Float);
    props[2].setValue(QVariant(static_cast<double>(highlights_)));

    props[3].setName("Shadows");
    props[3].setType(ArtifactCore::PropertyType::Float);
    props[3].setValue(QVariant(static_cast<double>(shadows_)));

    return props;
}

void BrightnessEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Brightness") setBrightness(value.toFloat());
    else if (name == "Contrast") setContrast(value.toFloat());
    else if (name == "Highlights") setHighlights(value.toFloat());
    else if (name == "Shadows") setShadows(value.toFloat());
}

} // namespace Artifact
