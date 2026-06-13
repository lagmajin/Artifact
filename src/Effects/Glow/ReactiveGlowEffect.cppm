module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.ReactiveGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius) {
    const int estimated = static_cast<int>(std::ceil(std::max(0.5f, radius) * 2.5f));
    return std::max(3, (estimated * 2) + 1);
}

} // namespace

class ReactiveGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;
    float radius_ = 14.0f;
    float intensity_ = 1.0f;
    float reaction_ = 0.8f;
    float saturationWeight_ = 0.65f;
    float tintMix_ = 0.25f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4,
                       const_cast<float*>(srcData));
        std::vector<cv::Mat> channels;
        cv::split(srcMat, channels);

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        cv::Mat gray = channels[0] * 0.114f +
                       channels[1] * 0.587f +
                       channels[2] * 0.299f;

        cv::Mat maxCh;
        cv::Mat minCh;
        cv::max(channels[0], channels[1], maxCh);
        cv::max(maxCh, channels[2], maxCh);
        cv::min(channels[0], channels[1], minCh);
        cv::min(minCh, channels[2], minCh);

        cv::Mat saturation = maxCh - minCh;
        cv::Mat reactionMask;
        cv::subtract(gray, cv::Scalar::all(threshold_), reactionMask);
        cv::threshold(reactionMask, reactionMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold_ < 0.999f) {
            reactionMask *= 1.0f / std::max(0.001f, 1.0f - threshold_);
        }
        reactionMask = reactionMask * reaction_ + saturation * saturationWeight_;
        cv::threshold(reactionMask, reactionMask, 1.0, 1.0, cv::THRESH_TRUNC);

        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{reactionMask, reactionMask, reactionMask}, mask3);
        cv::Mat bloom = color.mul(mask3);

        const int ksize = kernelSizeForRadius(radius_);
        cv::GaussianBlur(bloom, bloom, cv::Size(ksize, ksize),
                         std::max(0.1f, radius_), std::max(0.1f, radius_),
                         cv::BORDER_REPLICATE);

        cv::Mat result = color + bloom * intensity_ * (0.5f + tintMix_);
        cv::Mat desat = color * (1.0f - tintMix_) + bloom * tintMix_;
        result = result * (1.0f - 0.35f * tintMix_) + desat * (0.35f * tintMix_);
        cv::min(result, cv::Scalar::all(1.0), result);
        cv::max(result, cv::Scalar::all(0.0), result);

        std::vector<cv::Mat> outChannels;
        cv::split(result, outChannels);
        outChannels.push_back(alpha);
        cv::Mat outMat;
        cv::merge(outChannels, outMat);
        dst.image().setFromRGBA32F(outMat.ptr<float>(), outMat.cols, outMat.rows);
    }
};

class ReactiveGlowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.5f;
    float radius_ = 14.0f;
    float intensity_ = 1.0f;
    float reaction_ = 0.8f;
    float saturationWeight_ = 0.65f;
    float tintMix_ = 0.25f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    ReactiveGlowEffectCPUImpl cpuImpl_;
};

ReactiveGlowEffect::ReactiveGlowEffect() {
    setEffectID(UniString("reactive_glow"));
    setDisplayName(UniString("Reactive Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ReactiveGlowEffectCPUImpl>());
    setGPUImpl(std::make_shared<ReactiveGlowEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ReactiveGlowEffect::~ReactiveGlowEffect() = default;

void ReactiveGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<ReactiveGlowEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->radius_ = radius_;
        cpu->intensity_ = intensity_;
        cpu->reaction_ = reaction_;
        cpu->saturationWeight_ = saturationWeight_;
        cpu->tintMix_ = tintMix_;
    }
    if (auto gpu = std::dynamic_pointer_cast<ReactiveGlowEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->radius_ = radius_;
        gpu->intensity_ = intensity_;
        gpu->reaction_ = reaction_;
        gpu->saturationWeight_ = saturationWeight_;
        gpu->tintMix_ = tintMix_;
    }
}

std::vector<AbstractProperty> ReactiveGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

    auto& thresholdProp = props.emplace_back();
    thresholdProp.setName("Threshold");
    thresholdProp.setType(PropertyType::Float);
    thresholdProp.setValue(threshold_);

    auto& radiusProp = props.emplace_back();
    radiusProp.setName("Radius");
    radiusProp.setType(PropertyType::Float);
    radiusProp.setValue(radius_);

    auto& intensityProp = props.emplace_back();
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);

    auto& reactionProp = props.emplace_back();
    reactionProp.setName("Reaction");
    reactionProp.setType(PropertyType::Float);
    reactionProp.setValue(reaction_);

    auto& saturationProp = props.emplace_back();
    saturationProp.setName("Saturation Weight");
    saturationProp.setType(PropertyType::Float);
    saturationProp.setValue(saturationWeight_);

    auto& tintProp = props.emplace_back();
    tintProp.setName("Tint Mix");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tintMix_);

    return props;
}

void ReactiveGlowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) {
        setThreshold(value.toFloat());
    } else if (key == QStringLiteral("Radius")) {
        setRadius(value.toFloat());
    } else if (key == QStringLiteral("Intensity")) {
        setIntensity(value.toFloat());
    } else if (key == QStringLiteral("Reaction")) {
        setReaction(value.toFloat());
    } else if (key == QStringLiteral("Saturation Weight")) {
        setSaturationWeight(value.toFloat());
    } else if (key == QStringLiteral("Tint Mix")) {
        setTintMix(value.toFloat());
    }
}

} // namespace Artifact
