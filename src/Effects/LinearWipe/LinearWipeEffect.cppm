module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <cmath>

module Artifact.Effect.Rasterizer.LinearWipe;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class LinearWipeEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float angle_ = 0.0f;
    float softness_ = 10.0f;
    float feather_ = 0.0f;

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

        const float rad = angle_ * 3.14159265f / 180.0f;
        const float ca = std::cos(rad);
        const float sa = std::sin(rad);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float proj = x * ca + y * sa;
                // Normalize to 0..1 across image extent
                const float minProj = std::min(0.0f, std::min(w * ca, h * sa));
                const float maxProj = std::max(0.0f, std::max(w * ca, h * sa));
                float t = (proj - minProj) / std::max(1e-5f, maxProj - minProj);
                t = std::clamp(t, 0.0f, 1.0f);

                const float edge = std::clamp((t - 0.5f) * 2.0f, 0.0f, 1.0f);
                float alpha = 1.0f - edge;
                if (feather_ > 0.0f) {
                    if (alpha < 0.0f) alpha = 0.0f;
                    else alpha = std::clamp(alpha / std::max(1e-5f, feather_), 0.0f, 1.0f);
                }

                cv::Vec4f p = mat.at<cv::Vec4f>(y, x);
                p[3] *= alpha;
                mat.at<cv::Vec4f>(y, x) = p;
            }
        }
    }
};

class LinearWipeEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    LinearWipeEffectCPUImpl cpuImpl_;
};

LinearWipeEffect::LinearWipeEffect() {
    setDisplayName(UniString("Linear Wipe"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LinearWipeEffectCPUImpl>());
    setGPUImpl(std::make_shared<LinearWipeEffectGPUImpl>());
}
LinearWipeEffect::~LinearWipeEffect() = default;

float LinearWipeEffect::angle() const { return angle_; }
void LinearWipeEffect::setAngle(float v) { angle_ = std::fmod(v, 360.0f); if (angle_ < 0) angle_ += 360.0f; syncImpls(); }
float LinearWipeEffect::softness() const { return softness_; }
void LinearWipeEffect::setSoftness(float v) { softness_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float LinearWipeEffect::feather() const { return feather_; }
void LinearWipeEffect::setFeather(float v) { feather_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

void LinearWipeEffect::syncImpls() {
    if (auto* c = dynamic_cast<LinearWipeEffectCPUImpl*>(cpuImpl().get())) {
        c->angle_ = angle_;
        c->softness_ = softness_;
        c->feather_ = feather_;
    }
    if (auto* g = dynamic_cast<LinearWipeEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.angle_ = angle_;
        g->cpuImpl_.softness_ = softness_;
        g->cpuImpl_.feather_ = feather_;
    }
}

std::vector<AbstractProperty> LinearWipeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Angle"); a.setType(PropertyType::Float); a.setValue(angle_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& f = props.emplace_back(); f.setName("Feather"); f.setType(PropertyType::Float); f.setValue(feather_);
    return props;
}

void LinearWipeEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Angle") setAngle(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Feather") setFeather(v.toFloat());
}

} // namespace Artifact
