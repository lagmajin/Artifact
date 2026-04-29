module;
#include <utility>

#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <vector>
#include <opencv2/opencv.hpp>

module ExposureEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ExposureEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float exposure_ = 0.0f;
    float offset_ = 0.0f;
    float gammaCorrection_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float exposureMultiplier = std::pow(2.0f, exposure_);
        const float gammaInv = 1.0f / std::max(0.0001f, gammaCorrection_);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                for (int c = 0; c < 3; ++c) {
                    float val = pixel[c] * exposureMultiplier + offset_;
                    val = std::max(0.0f, val);
                    if (gammaInv != 1.0f) {
                        val = std::pow(val, gammaInv);
                    }
                    pixel[c] = std::clamp(val, 0.0f, 1.0f);
                }
            }
        }
    }
};

class ExposureEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float exposure_ = 0.0f;
    float offset_ = 0.0f;
    float gammaCorrection_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    ExposureEffectCPUImpl cpuImpl_;
};

ExposureEffect::ExposureEffect() {
    setEffectID(UniString("effect.colorcorrection.exposure"));
    setDisplayName(UniString("Exposure"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ExposureEffectCPUImpl>());
    setGPUImpl(std::make_shared<ExposureEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ExposureEffect::~ExposureEffect() = default;

void ExposureEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<ExposureEffectCPUImpl>(cpuImpl())) {
        cpu->exposure_ = exposure_;
        cpu->offset_ = offset_;
        cpu->gammaCorrection_ = gammaCorrection_;
    }
    if (auto gpu = std::dynamic_pointer_cast<ExposureEffectGPUImpl>(gpuImpl())) {
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
