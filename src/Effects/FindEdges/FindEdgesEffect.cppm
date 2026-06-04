module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.FindEdges;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class FindEdgesEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 1.0f;
    bool invert_ = false;

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

        cv::Mat gray;
        {
            std::vector<cv::Mat> tmp;
            cv::split(color, tmp);
            gray = tmp[0] * 0.299f + tmp[1] * 0.587f + tmp[2] * 0.114f;
        }

        cv::Mat edge;
        cv::Laplacian(gray, edge, CV_32F, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
        edge = cv::abs(edge);
        cv::Mat edgeNorm;
        cv::normalize(edge, edgeNorm, 0.0, 1.0, cv::NORM_MINMAX);
        if (invert_) {
            edgeNorm = 1.0f - edgeNorm;
        }

        cv::Mat edge3;
        cv::merge(std::vector<cv::Mat>{edgeNorm, edgeNorm, edgeNorm}, edge3);

        cv::Mat result = color * (1.0f - amount_) + edge3 * amount_;
        result = cv::max(cv::Mat::zeros(result.size(), result.type()), result);

        std::vector<cv::Mat> out;
        cv::split(result, out);
        out.push_back(alpha);
        cv::merge(out, mat);
    }
};

class FindEdgesEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

    FindEdgesEffectCPUImpl cpuImpl_;
};

FindEdgesEffect::FindEdgesEffect() {
    setDisplayName(UniString("Find Edges"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<FindEdgesEffectCPUImpl>());
    setGPUImpl(std::make_shared<FindEdgesEffectGPUImpl>());
}

FindEdgesEffect::~FindEdgesEffect() = default;

float FindEdgesEffect::amount() const { return amount_; }
void FindEdgesEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 5.0f); syncImpls(); }
bool FindEdgesEffect::invert() const { return invert_; }
void FindEdgesEffect::setInvert(bool v) { invert_ = v; syncImpls(); }

void FindEdgesEffect::syncImpls() {
    if (auto* c = dynamic_cast<FindEdgesEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->invert_ = invert_;
    }
    if (auto* g = dynamic_cast<FindEdgesEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.invert_ = invert_;
    }
}

std::vector<AbstractProperty> FindEdgesEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& i = props.emplace_back(); i.setName("Invert"); i.setType(PropertyType::Boolean); i.setValue(invert_);
    return props;
}

void FindEdgesEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Invert") setInvert(v.toBool());
}

} // namespace Artifact
