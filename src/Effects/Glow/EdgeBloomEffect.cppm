module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.EdgeBloom;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius) {
    const int estimated = static_cast<int>(std::ceil(std::max(0.5f, radius) * 3.0f));
    return std::max(3, (estimated * 2) + 1);
}

} // namespace

class EdgeBloomEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.65f;
    float radius_ = 10.0f;
    float amount_ = 1.15f;
    float edgeBoost_ = 1.8f;
    float tintMix_ = 0.35f;

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

        cv::Mat edge;
        cv::Laplacian(gray, edge, CV_32F, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
        cv::absdiff(edge, cv::Scalar::all(0.0), edge);

        cv::Mat highlight;
        cv::subtract(gray, cv::Scalar::all(threshold_), highlight);
        cv::threshold(highlight, highlight, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold_ < 0.999f) {
            highlight *= 1.0f / std::max(0.001f, 1.0f - threshold_);
        }

        cv::Mat sourceMask = edge * edgeBoost_ + highlight;
        cv::threshold(sourceMask, sourceMask, 1.0, 1.0, cv::THRESH_TRUNC);

        const int ksize = kernelSizeForRadius(radius_);
        cv::GaussianBlur(sourceMask, sourceMask, cv::Size(ksize, ksize),
                         std::max(0.1f, radius_), std::max(0.1f, radius_),
                         cv::BORDER_REPLICATE);

        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{sourceMask, sourceMask, sourceMask}, mask3);

        cv::Mat colorBloom = color.mul(mask3);
        cv::Mat whiteBloom = mask3;
        cv::Mat glow = colorBloom * tintMix_ + whiteBloom * (1.0f - tintMix_);

        cv::Mat result = color + glow * amount_;
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

class EdgeBloomEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.65f;
    float radius_ = 10.0f;
    float amount_ = 1.15f;
    float edgeBoost_ = 1.8f;
    float tintMix_ = 0.35f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    EdgeBloomEffectCPUImpl cpuImpl_;
};

EdgeBloomEffect::EdgeBloomEffect() {
    setEffectID(UniString("edge_bloom"));
    setDisplayName(UniString("Edge Bloom"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<EdgeBloomEffectCPUImpl>());
    setGPUImpl(std::make_shared<EdgeBloomEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

EdgeBloomEffect::~EdgeBloomEffect() = default;

void EdgeBloomEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<EdgeBloomEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->radius_ = radius_;
        cpu->amount_ = amount_;
        cpu->edgeBoost_ = edgeBoost_;
        cpu->tintMix_ = tintMix_;
    }
    if (auto gpu = std::dynamic_pointer_cast<EdgeBloomEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->radius_ = radius_;
        gpu->amount_ = amount_;
        gpu->edgeBoost_ = edgeBoost_;
        gpu->tintMix_ = tintMix_;
    }
}

std::vector<AbstractProperty> EdgeBloomEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(5);

    auto& thresholdProp = props.emplace_back();
    thresholdProp.setName("Threshold");
    thresholdProp.setType(PropertyType::Float);
    thresholdProp.setValue(threshold_);

    auto& radiusProp = props.emplace_back();
    radiusProp.setName("Radius");
    radiusProp.setType(PropertyType::Float);
    radiusProp.setValue(radius_);

    auto& amountProp = props.emplace_back();
    amountProp.setName("Amount");
    amountProp.setType(PropertyType::Float);
    amountProp.setValue(amount_);

    auto& edgeBoostProp = props.emplace_back();
    edgeBoostProp.setName("Edge Boost");
    edgeBoostProp.setType(PropertyType::Float);
    edgeBoostProp.setValue(edgeBoost_);

    auto& tintMixProp = props.emplace_back();
    tintMixProp.setName("Tint Mix");
    tintMixProp.setType(PropertyType::Float);
    tintMixProp.setValue(tintMix_);

    return props;
}

void EdgeBloomEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) {
        setThreshold(value.toFloat());
    } else if (key == QStringLiteral("Radius")) {
        setRadius(value.toFloat());
    } else if (key == QStringLiteral("Amount")) {
        setAmount(value.toFloat());
    } else if (key == QStringLiteral("Edge Boost")) {
        setEdgeBoost(value.toFloat());
    } else if (key == QStringLiteral("Tint Mix")) {
        setTintMix(value.toFloat());
    }
}

} // namespace Artifact
