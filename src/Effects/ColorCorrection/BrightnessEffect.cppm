module;
#include <utility>

#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <vector>
#include <opencv2/opencv.hpp>

module BrightnessEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class BrightnessEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float brightness_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float contrastFactor = (contrast_ != 1.0f) ? (1.0f + contrast_) / (1.0f - contrast_) : 100.0f;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                for (int c = 0; c < 3; ++c) {
                    float val = pixel[c];
                    val += brightness_;
                    val = contrastFactor * (val - 0.5f) + 0.5f;
                    if (val > 0.5f) {
                        const float highlightWeight = (val - 0.5f) * 2.0f;
                        val += highlights_ * highlightWeight * 0.5f;
                    }
                    if (val < 0.5f) {
                        const float shadowWeight = (0.5f - val) * 2.0f;
                        val += shadows_ * shadowWeight * 0.5f;
                    }
                    pixel[c] = std::clamp(val, 0.0f, 1.0f);
                }
            }
        }
    }
};

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
        applyCPU(src, dst);
    }

private:
    BrightnessEffectCPUImpl cpuImpl_;
};

BrightnessEffect::BrightnessEffect() {
    setEffectID(UniString("effect.colorcorrection.brightness"));
    setDisplayName(UniString("Brightness / Contrast"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BrightnessEffectCPUImpl>());
    setGPUImpl(std::make_shared<BrightnessEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

BrightnessEffect::~BrightnessEffect() = default;

void BrightnessEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<BrightnessEffectCPUImpl>(cpuImpl())) {
        cpu->brightness_ = brightness_;
        cpu->contrast_ = contrast_;
        cpu->highlights_ = highlights_;
        cpu->shadows_ = shadows_;
    }
    if (auto gpu = std::dynamic_pointer_cast<BrightnessEffectGPUImpl>(gpuImpl())) {
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
