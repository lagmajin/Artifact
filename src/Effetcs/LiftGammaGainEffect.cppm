module;

#include <QString>
#include <QVariant>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

module Artifact.Effect.LiftGammaGain;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

static void applyLiftGammaGainCore(const ImageF32x4RGBAWithCache& src,
                                   ImageF32x4RGBAWithCache& dst,
                                   float liftR, float liftG, float liftB,
                                   float gammaR, float gammaG, float gammaB,
                                   float gainR, float gainG, float gainB) {
    dst = src;
    cv::Mat mat = dst.image().toCVMat();
    if (mat.empty()) {
        return;
    }

    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
            float& b = p[0];
            float& g = p[1];
            float& r = p[2];

            r += liftR * 0.1f;
            g += liftG * 0.1f;
            b += liftB * 0.1f;

            if (gammaR != 1.0f) r = std::pow(std::max(r, 0.0f), 1.0f / gammaR);
            if (gammaG != 1.0f) g = std::pow(std::max(g, 0.0f), 1.0f / gammaG);
            if (gammaB != 1.0f) b = std::pow(std::max(b, 0.0f), 1.0f / gammaB);

            r *= gainR;
            g *= gainG;
            b *= gainB;

            r = std::clamp(r, 0.0f, 1.0f);
            g = std::clamp(g, 0.0f, 1.0f);
            b = std::clamp(b, 0.0f, 1.0f);
        }
    }

    dst.image().setFromCVMat(mat);
}

class LiftGammaGainCPUImpl : public ArtifactEffectImplBase {
public:
    float liftR_ = 0.0f, liftG_ = 0.0f, liftB_ = 0.0f;
    float gammaR_ = 1.0f, gammaG_ = 1.0f, gammaB_ = 1.0f;
    float gainR_ = 1.0f, gainG_ = 1.0f, gainB_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyLiftGammaGainCore(src, dst, liftR_, liftG_, liftB_, gammaR_, gammaG_, gammaB_, gainR_, gainG_, gainB_);
    }
};

class LiftGammaGainGPUImpl : public ArtifactEffectImplBase {
public:
    float liftR_ = 0.0f, liftG_ = 0.0f, liftB_ = 0.0f;
    float gammaR_ = 1.0f, gammaG_ = 1.0f, gammaB_ = 1.0f;
    float gainR_ = 1.0f, gainG_ = 1.0f, gainB_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyLiftGammaGainCore(src, dst, liftR_, liftG_, liftB_, gammaR_, gammaG_, gammaB_, gainR_, gainG_, gainB_);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

LiftGammaGainEffect::LiftGammaGainEffect() {
    setDisplayName(ArtifactCore::UniString("Lift / Gamma / Gain"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    setCPUImpl(std::make_shared<LiftGammaGainCPUImpl>());
    setGPUImpl(std::make_shared<LiftGammaGainGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

LiftGammaGainEffect::~LiftGammaGainEffect() = default;

void LiftGammaGainEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<LiftGammaGainCPUImpl*>(cpuImpl().get())) {
        cpu->liftR_ = liftR_; cpu->liftG_ = liftG_; cpu->liftB_ = liftB_;
        cpu->gammaR_ = gammaR_; cpu->gammaG_ = gammaG_; cpu->gammaB_ = gammaB_;
        cpu->gainR_ = gainR_; cpu->gainG_ = gainG_; cpu->gainB_ = gainB_;
    }
    if (auto* gpu = dynamic_cast<LiftGammaGainGPUImpl*>(gpuImpl().get())) {
        gpu->liftR_ = liftR_; gpu->liftG_ = liftG_; gpu->liftB_ = liftB_;
        gpu->gammaR_ = gammaR_; gpu->gammaG_ = gammaG_; gpu->gammaB_ = gammaB_;
        gpu->gainR_ = gainR_; gpu->gainG_ = gainG_; gpu->gainB_ = gainB_;
    }
}

std::vector<AbstractProperty> LiftGammaGainEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    props.push_back({}); props.back().setName("Lift R"); props.back().setType(PropertyType::Float); props.back().setValue(liftR_);
    props.push_back({}); props.back().setName("Lift G"); props.back().setType(PropertyType::Float); props.back().setValue(liftG_);
    props.push_back({}); props.back().setName("Lift B"); props.back().setType(PropertyType::Float); props.back().setValue(liftB_);

    props.push_back({}); props.back().setName("Gamma R"); props.back().setType(PropertyType::Float); props.back().setValue(gammaR_);
    props.push_back({}); props.back().setName("Gamma G"); props.back().setType(PropertyType::Float); props.back().setValue(gammaG_);
    props.push_back({}); props.back().setName("Gamma B"); props.back().setType(PropertyType::Float); props.back().setValue(gammaB_);

    props.push_back({}); props.back().setName("Gain R"); props.back().setType(PropertyType::Float); props.back().setValue(gainR_);
    props.push_back({}); props.back().setName("Gain G"); props.back().setType(PropertyType::Float); props.back().setValue(gainG_);
    props.push_back({}); props.back().setName("Gain B"); props.back().setType(PropertyType::Float); props.back().setValue(gainB_);

    return props;
}

void LiftGammaGainEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == "Lift R") setLiftR(value.toFloat());
    else if (key == "Lift G") setLiftG(value.toFloat());
    else if (key == "Lift B") setLiftB(value.toFloat());
    else if (key == "Gamma R") setGammaR(value.toFloat());
    else if (key == "Gamma G") setGammaG(value.toFloat());
    else if (key == "Gamma B") setGammaB(value.toFloat());
    else if (key == "Gain R") setGainR(value.toFloat());
    else if (key == "Gain G") setGainG(value.toFloat());
    else if (key == "Gain B") setGainB(value.toFloat());
}

} // namespace Artifact
