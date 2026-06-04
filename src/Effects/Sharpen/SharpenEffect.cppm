module;
#include <algorithm>
#include <memory>
#include <utility>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QVector>

module Artifact.Effect.Rasterizer.Sharpen;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class SharpenEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 1.0f;
    float sigma_ = 1.0f;
    float threshold_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;

        cv::Mat floatMat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::vector<cv::Mat> channels;
        cv::split(floatMat, channels);
        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        const int ksize = std::max(3, static_cast<int>(sigma_ * 6.0f) | 1);
        cv::Mat blurred;
        cv::GaussianBlur(color, blurred, cv::Size(ksize, ksize), std::max(0.1f, sigma_), std::max(0.1f, sigma_), cv::BORDER_REPLICATE);

        cv::Mat result = color + (color - blurred) * amount_;
        if (threshold_ > 0.0f) {
            cv::Mat diff = cv::abs(color - blurred) * amount_;
            cv::Mat mask;
            cv::compare(diff, threshold_, diff, cv::CMP_GT);
            color.copyTo(result, ~diff);
        }
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> outChannels;
        cv::split(result, outChannels);
        outChannels.push_back(alpha);
        cv::merge(outChannels, floatMat);
    }
};

class SharpenEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

    SharpenEffectCPUImpl cpuImpl_;
};

SharpenEffect::SharpenEffect() {
    setDisplayName(UniString("Sharpen"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SharpenEffectCPUImpl>());
    setGPUImpl(std::make_shared<SharpenEffectGPUImpl>());
}

SharpenEffect::~SharpenEffect() = default;

float SharpenEffect::amount() const { return amount_; }
void SharpenEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 10.0f); syncImpls(); }
float SharpenEffect::sigma() const { return sigma_; }
void SharpenEffect::setSigma(float v) { sigma_ = std::clamp(v, 0.0f, 10.0f); syncImpls(); }
float SharpenEffect::threshold() const { return threshold_; }
void SharpenEffect::setThreshold(float v) { threshold_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void SharpenEffect::syncImpls() {
    if (auto* c = dynamic_cast<SharpenEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->sigma_ = sigma_;
        c->threshold_ = threshold_;
    }
    if (auto* g = dynamic_cast<SharpenEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.sigma_ = sigma_;
        g->cpuImpl_.threshold_ = threshold_;
    }
}

std::vector<AbstractProperty> SharpenEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& s = props.emplace_back(); s.setName("Sigma"); s.setType(PropertyType::Float); s.setValue(sigma_);
    auto& t = props.emplace_back(); t.setName("Threshold"); t.setType(PropertyType::Float); t.setValue(threshold_);
    return props;
}

void SharpenEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Sigma") setSigma(v.toFloat());
    else if (k == "Threshold") setThreshold(v.toFloat());
}

} // namespace Artifact
