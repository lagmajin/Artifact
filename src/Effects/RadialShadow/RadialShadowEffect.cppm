module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
#include <cmath>

module Artifact.Effect.Rasterizer.RadialShadow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class RadialShadowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    QColor color_ = QColor(0, 0, 0, 180);
    float distance_ = 10.0f;
    float softness_ = 8.0f;
    float opacity_ = 0.75f;
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        const int w = mat.cols;
        const int h = mat.rows;
        const float cx = centerX_ * w;
        const float cy = centerY_ * h;

        cv::Mat colorMat(h, w, CV_32FC4);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                float shadow = dist / std::max(1.0f, distance_ + softness_);
                float alpha = 1.0f - std::clamp(shadow, 0.0f, 1.0f);
                alpha *= opacity_;
                colorMat.at<cv::Vec4f>(y, x) = cv::Vec4f(
                    color_.blueF(),
                    color_.greenF(),
                    color_.redF(),
                    alpha
                );
            }
        }

        // Alpha compositing: shadow over src
        cv::Mat srcMat = mat.clone();
        cv::addWeighted(srcMat, 1.0, colorMat, 1.0, 0.0, mat);
    }
};

class RadialShadowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    RadialShadowEffectCPUImpl cpuImpl_;
};

RadialShadowEffect::RadialShadowEffect() {
    setDisplayName(UniString("Radial Shadow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<RadialShadowEffectCPUImpl>());
    setGPUImpl(std::make_shared<RadialShadowEffectGPUImpl>());
}
RadialShadowEffect::~RadialShadowEffect() = default;

QColor RadialShadowEffect::color() const { return color_; }
void RadialShadowEffect::setColor(const QColor& v) { color_ = v; syncImpls(); }
float RadialShadowEffect::distance() const { return distance_; }
void RadialShadowEffect::setDistance(float v) { distance_ = std::max(0.0f, v); syncImpls(); }
float RadialShadowEffect::softness() const { return softness_; }
void RadialShadowEffect::setSoftness(float v) { softness_ = std::max(0.0f, v); syncImpls(); }
float RadialShadowEffect::opacity() const { return opacity_; }
void RadialShadowEffect::setOpacity(float v) { opacity_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialShadowEffect::centerX() const { return centerX_; }
void RadialShadowEffect::setCenterX(float v) { centerX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float RadialShadowEffect::centerY() const { return centerY_; }
void RadialShadowEffect::setCenterY(float v) { centerY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void RadialShadowEffect::syncImpls() {
    if (auto* c = dynamic_cast<RadialShadowEffectCPUImpl*>(cpuImpl().get())) {
        c->color_ = color_;
        c->distance_ = distance_;
        c->softness_ = softness_;
        c->opacity_ = opacity_;
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
    }
    if (auto* g = dynamic_cast<RadialShadowEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.color_ = color_;
        g->cpuImpl_.distance_ = distance_;
        g->cpuImpl_.softness_ = softness_;
        g->cpuImpl_.opacity_ = opacity_;
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
    }
}

std::vector<AbstractProperty> RadialShadowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& c = props.emplace_back(); c.setName("Color"); c.setType(PropertyType::Color); c.setValue(color_);
    auto& d = props.emplace_back(); d.setName("Distance"); d.setType(PropertyType::Float); d.setValue(distance_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& o = props.emplace_back(); o.setName("Opacity"); o.setType(PropertyType::Float); o.setValue(opacity_);
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_);
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_);
    return props;
}

void RadialShadowEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Color") setColor(v.value<QColor>());
    else if (k == "Distance") setDistance(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Opacity") setOpacity(v.toFloat());
    else if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
}

} // namespace Artifact
