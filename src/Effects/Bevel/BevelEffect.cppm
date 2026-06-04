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

module Artifact.Effect.Rasterizer.Bevel;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class BevelEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    float softness_ = 2.0f;
    bool edgeMode_ = false; // false = alpha bevel, true = edge bevel
    QColor highlightColor_ = QColor(255, 255, 255, 255);
    QColor shadowColor_ = QColor(0, 0, 0, 255);

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::vector<cv::Mat> channels;
        cv::split(mat, channels);
        cv::Mat color;
        cv::Mat alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);

        // grayscale for edge detection
        cv::Mat srcGray;
        {
            std::vector<cv::Mat> tmp;
            cv::split(color, tmp);
            srcGray = tmp[0] * 0.299f + tmp[1] * 0.587f + tmp[2] * 0.114f;
            if (!alpha.empty()) srcGray = srcGray.mul(alpha);
        }

        const int ksize = std::max(1, static_cast<int>(std::round(softness_ * 2.0f + 1.0f))) | 1;
        cv::Mat blurred;
        cv::GaussianBlur(srcGray, blurred, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);
        cv::Mat edge = srcGray - blurred;
        cv::Mat highlight, shadow;
        cv::threshold(edge, highlight, 0.0, 1.0, cv::THRESH_BINARY);
        cv::threshold(edge, shadow, 0.0, 1.0, cv::THRESH_BINARY_INV);
        cv::GaussianBlur(highlight, highlight, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);
        cv::GaussianBlur(shadow, shadow, cv::Size(ksize, ksize), softness_, softness_, cv::BORDER_REPLICATE);

        cv::Mat result = color.clone();
        cv::Mat hc(highlight.size(), CV_32FC3);
        cv::Mat sc(shadow.size(), CV_32FC3);
        for (int y = 0; y < hc.rows; ++y) {
            for (int x = 0; x < hc.cols; ++x) {
                float h = std::clamp(highlight.at<float>(y, x) * strength_, 0.0f, 1.0f);
                float s = std::clamp(shadow.at<float>(y, x) * strength_, 0.0f, 1.0f);
                hc.at<cv::Vec3f>(y, x) = cv::Vec3f(
                    highlightColor_.blueF(),
                    highlightColor_.greenF(),
                    highlightColor_.redF()
                ) * h;
                sc.at<cv::Vec3f>(y, x) = cv::Vec3f(
                    shadowColor_.blueF(),
                    shadowColor_.greenF(),
                    shadowColor_.redF()
                ) * s;
            }
        }
        result = result + hc + sc;
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> out;
        cv::split(result, out);
        out.push_back(alpha);
        cv::merge(out, mat);
    }
};

class BevelEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
public:
    BevelEffectCPUImpl cpuImpl_;
};

BevelEffect::BevelEffect() {
    setDisplayName(UniString("Bevel"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BevelEffectCPUImpl>());
    setGPUImpl(std::make_shared<BevelEffectGPUImpl>());
}
BevelEffect::~BevelEffect() = default;

float BevelEffect::strength() const { return strength_; }
void BevelEffect::setStrength(float v) { strength_ = std::clamp(v, 0.0f, 5.0f); syncImpls(); }
float BevelEffect::softness() const { return softness_; }
void BevelEffect::setSoftness(float v) { softness_ = std::max(0.0f, v); syncImpls(); }
bool BevelEffect::edgeMode() const { return edgeMode_; }
void BevelEffect::setEdgeMode(bool v) { edgeMode_ = v; syncImpls(); }

void BevelEffect::syncImpls() {
    if (auto* c = dynamic_cast<BevelEffectCPUImpl*>(cpuImpl().get())) {
        c->strength_ = strength_;
        c->softness_ = softness_;
        c->edgeMode_ = edgeMode_;
    }
    if (auto* g = dynamic_cast<BevelEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.strength_ = strength_;
        g->cpuImpl_.softness_ = softness_;
        g->cpuImpl_.edgeMode_ = edgeMode_;
    }
}

std::vector<AbstractProperty> BevelEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Strength"); a.setType(PropertyType::Float); a.setValue(strength_);
    auto& s = props.emplace_back(); s.setName("Softness"); s.setType(PropertyType::Float); s.setValue(softness_);
    auto& e = props.emplace_back(); e.setName("Edge Mode"); e.setType(PropertyType::Boolean); e.setValue(edgeMode_);
    return props;
}

void BevelEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Strength") setStrength(v.toFloat());
    else if (k == "Softness") setSoftness(v.toFloat());
    else if (k == "Edge Mode") setEdgeMode(v.toBool());
}

}
