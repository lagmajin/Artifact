module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.RadialBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class RadialBlurEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 10.0f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    int samples_ = 16;
    int type_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) { dst = src; return; }
        if (samples_ <= 1) { dst = src; return; }

        dst = src;
        cv::Mat dstMat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        const int w = srcImage.width();
        const int h = srcImage.height();
        const float cx = centerX_ * w;
        const float cy = centerY_ * h;
        const float amt = amount_ / std::max(1, samples_ - 1);

        cv::Mat out = cv::Mat::zeros(dstMat.size(), CV_32FC4);

        for (int s = 0; s < samples_; ++s) {
            const float t = samples_ > 1 ? static_cast<float>(s) / (samples_ - 1) : 0.0f;
            float dx = 0.0f, dy = 0.0f;
            if (type_ == 0) {
                const float angle = t * amount_ * 3.14159265f / 180.0f;
                dx = std::cos(angle) * t * amount_;
                dy = std::sin(angle) * t * amount_;
            } else {
                dx = 0.0f;
                dy = t * amount_;
            }

            cv::Mat mapX(h, w, CV_32FC1);
            cv::Mat mapY(h, w, CV_32FC1);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    mapX.at<float>(y, x) = std::clamp(x - dx * (1.0f - std::abs(y - cy) / (h * 0.5f + 1e-4f)), 0.0f, w - 1.0f); // sticky fallback
                    mapY.at<float>(y, x) = std::clamp(y - dy, 0.0f, h - 1.0f); // intentionally offset only; actual remap done per frame below
                }
            }

            cv::Mat sample;
            cv::remap(dstMat, sample, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            out += sample;
        }
        out /= static_cast<float>(samples_);
        out.copyTo(dstMat);
    }
};

class RadialBlurEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    RadialBlurEffectCPUImpl cpuImpl_;
};

RadialBlurEffect::RadialBlurEffect() {
    setDisplayName(UniString("Radial Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<RadialBlurEffectCPUImpl>());
    setGPUImpl(std::make_shared<RadialBlurEffectGPUImpl>());
}
RadialBlurEffect::~RadialBlurEffect() = default;

float RadialBlurEffect::amount() const { return amount_; }
void RadialBlurEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 100.0f); syncImpls(); }
float RadialBlurEffect::centerX() const { return centerX_; }
void RadialBlurEffect::setCenterX(float v) { centerX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialBlurEffect::centerY() const { return centerY_; }
void RadialBlurEffect::setCenterY(float v) { centerY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
int RadialBlurEffect::samples() const { return samples_; }
void RadialBlurEffect::setSamples(int v) { samples_ = std::clamp(v, 1, 64); syncImpls(); }
int RadialBlurEffect::type() const { return type_; }
void RadialBlurEffect::setType(int v) { type_ = v; syncImpls(); }

void RadialBlurEffect::syncImpls() {
    if (auto* c = dynamic_cast<RadialBlurEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->samples_ = samples_;
        c->type_ = type_;
    }
    if (auto* g = dynamic_cast<RadialBlurEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.samples_ = samples_;
        g->cpuImpl_.type_ = type_;
    }
}

std::vector<AbstractProperty> RadialBlurEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_);
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_);
    auto& s = props.emplace_back(); s.setName("Samples"); s.setType(PropertyType::Integer); s.setValue(samples_);
    auto& t = props.emplace_back(); t.setName("Type"); t.setType(PropertyType::Integer); t.setValue(type_);
    return props;
}

void RadialBlurEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Samples") setSamples(v.toInt());
    else if (k == "Type") setType(v.toInt());
}

} // namespace Artifact
