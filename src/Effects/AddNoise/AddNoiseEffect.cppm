module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <QColor>
#include <random>

module Artifact.Effect.Rasterizer.AddNoise;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class AddNoiseEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 0.15f;
    float size_ = 1.0f;
    bool colorNoise_ = true;
    bool monochrome_ = false;
    int seed_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        dst = src;
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        std::mt19937 rng(seed_);
        std::uniform_real_distribution<float> dist(-amount_, amount_);

        if (monochrome_) {
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    float n = dist(rng);
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + n, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + n, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + n, 0.0f, 1.0f);
                }
            }
        } else if (colorNoise_) {
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + dist(rng), 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + dist(rng), 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + dist(rng), 0.0f, 1.0f);
                }
            }
        } else {
            // luminance-only noise
            for (int y = 0; y < mat.rows; ++y) {
                for (int x = 0; x < mat.cols; ++x) {
                    float luma = mat.at<cv::Vec4f>(y, x)[0] * 0.299f
                               + mat.at<cv::Vec4f>(y, x)[1] * 0.587f
                               + mat.at<cv::Vec4f>(y, x)[2] * 0.114f;
                    float n = dist(rng);
                    float newLuma = std::clamp(luma + n, 0.0f, 1.0f);
                    float diff = newLuma - luma;
                    cv::Vec4f& p = mat.at<cv::Vec4f>(y, x);
                    p[0] = std::clamp(p[0] + diff, 0.0f, 1.0f);
                    p[1] = std::clamp(p[1] + diff, 0.0f, 1.0f);
                    p[2] = std::clamp(p[2] + diff, 0.0f, 1.0f);
                }
            }
        }
    }
};

class AddNoiseEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    AddNoiseEffectCPUImpl cpuImpl_;
};

AddNoiseEffect::AddNoiseEffect() {
    setDisplayName(UniString("Add Noise"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<AddNoiseEffectCPUImpl>());
    setGPUImpl(std::make_shared<AddNoiseEffectGPUImpl>());
}
AddNoiseEffect::~AddNoiseEffect() = default;

float AddNoiseEffect::amount() const { return amount_; }
void AddNoiseEffect::setAmount(float v) { amount_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }
float AddNoiseEffect::size() const { return size_; }
void AddNoiseEffect::setSize(float v) { size_ = std::max(0.1f, v); syncImpls(); }
bool AddNoiseEffect::colorNoise() const { return colorNoise_; }
void AddNoiseEffect::setColorNoise(bool v) { colorNoise_ = v; syncImpls(); }
bool AddNoiseEffect::monochrome() const { return monochrome_; }
void AddNoiseEffect::setMonochrome(bool v) { monochrome_ = v; syncImpls(); }
int AddNoiseEffect::seed() const { return seed_; }
void AddNoiseEffect::setSeed(int v) { seed_ = v; syncImpls(); }

void AddNoiseEffect::syncImpls() {
    if (auto* c = dynamic_cast<AddNoiseEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->size_ = size_;
        c->colorNoise_ = colorNoise_;
        c->monochrome_ = monochrome_;
        c->seed_ = seed_;
    }
    if (auto* g = dynamic_cast<AddNoiseEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.size_ = size_;
        g->cpuImpl_.colorNoise_ = colorNoise_;
        g->cpuImpl_.monochrome_ = monochrome_;
        g->cpuImpl_.seed_ = seed_;
    }
}

std::vector<AbstractProperty> AddNoiseEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& s = props.emplace_back(); s.setName("Size"); s.setType(PropertyType::Float); s.setValue(size_);
    auto& c = props.emplace_back(); c.setName("Color Noise"); c.setType(PropertyType::Boolean); c.setValue(colorNoise_);
    auto& m = props.emplace_back(); m.setName("Monochrome"); m.setType(PropertyType::Boolean); m.setValue(monochrome_);
    auto& sd = props.emplace_back(); sd.setName("Seed"); sd.setType(PropertyType::Integer); sd.setValue(seed_);
    return props;
}

void AddNoiseEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Size") setSize(v.toFloat());
    else if (k == "Color Noise") setColorNoise(v.toBool());
    else if (k == "Monochrome") setMonochrome(v.toBool());
    else if (k == "Seed") setSeed(v.toInt());
}

}
