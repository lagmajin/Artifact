module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>
#include <random>

module Artifact.Effect.Rasterizer.TurbulentDisplace;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

namespace {
static inline float hash2D(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                    + static_cast<std::uint32_t>(y) * 668265263u
                    + static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

static float valueNoise2D(float x, float y, int seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    const float v00 = hash2D(x0,     y0,     seed);
    const float v10 = hash2D(x0 + 1, y0,     seed);
    const float v01 = hash2D(x0,     y0 + 1, seed);
    const float v11 = hash2D(x0 + 1, y0 + 1, seed);

    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float ix0 = v00 + (v10 - v00) * sx;
    const float ix1 = v01 + (v11 - v01) * sx;
    return ix0 + (ix1 - ix0) * sy;
}
} // namespace

class TurbulentDisplaceEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float amount_ = 20.0f;
    float size_ = 30.0f;
    int octaves_ = 4;
    int seed_ = 0;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        const int w = srcImage.width();
        const int h = srcImage.height();
        dst = src;
        cv::Mat dstMat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());
        const cv::Mat srcMat(h, w, CV_32FC4, const_cast<float*>(srcImage.rgba32fData()));

        cv::Mat mapX(h, w, CV_32FC1);
        cv::Mat mapY(h, w, CV_32FC1);
        const float scale = 1.0f / std::max(1.0f, size_);
        std::mt19937 rng(seed_);
        std::uniform_real_distribution<float> dist(0.0f, 1000.0f);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float fx = x * scale;
                const float fy = y * scale;
                float dx = 0.0f;
                float dy = 0.0f;
                float amp = amount_;
                float freq = 1.0f;
                for (int o = 0; o < octaves_; ++o) {
                    const float nx = fx * freq + dist(rng);
                    const float ny = fy * freq + dist(rng);
                    dx += valueNoise2D(nx, ny, seed_ + o) * amp;
                    dy += valueNoise2D(nx + 100.0f, ny + 100.0f, seed_ + o) * amp;
                    amp *= 0.5f;
                    freq *= 2.0f;
                }
                mapX.at<float>(y, x) = std::clamp(x + dx, 0.0f, static_cast<float>(w - 1));
                mapY.at<float>(y, x) = std::clamp(y + dy, 0.0f, static_cast<float>(h - 1));
            }
        }

        cv::remap(srcMat, dstMat, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    }
};

class TurbulentDisplaceEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    TurbulentDisplaceEffectCPUImpl cpuImpl_;
};

TurbulentDisplaceEffect::TurbulentDisplaceEffect() {
    setDisplayName(UniString("Turbulent Displace"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<TurbulentDisplaceEffectCPUImpl>());
    setGPUImpl(std::make_shared<TurbulentDisplaceEffectGPUImpl>());
}
TurbulentDisplaceEffect::~TurbulentDisplaceEffect() = default;

float TurbulentDisplaceEffect::amount() const { return amount_; }
void TurbulentDisplaceEffect::setAmount(float v) { amount_ = std::max(0.0f, v); syncImpls(); }
float TurbulentDisplaceEffect::size() const { return size_; }
void TurbulentDisplaceEffect::setSize(float v) { size_ = std::max(1.0f, v); syncImpls(); }
int TurbulentDisplaceEffect::octaves() const { return octaves_; }
void TurbulentDisplaceEffect::setOctaves(int v) { octaves_ = std::max(1, v); syncImpls(); }
int TurbulentDisplaceEffect::seed() const { return seed_; }
void TurbulentDisplaceEffect::setSeed(int v) { seed_ = v; syncImpls(); }

void TurbulentDisplaceEffect::syncImpls() {
    if (auto* c = dynamic_cast<TurbulentDisplaceEffectCPUImpl*>(cpuImpl().get())) {
        c->amount_ = amount_;
        c->size_ = size_;
        c->octaves_ = octaves_;
        c->seed_ = seed_;
    }
    if (auto* g = dynamic_cast<TurbulentDisplaceEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.amount_ = amount_;
        g->cpuImpl_.size_ = size_;
        g->cpuImpl_.octaves_ = octaves_;
        g->cpuImpl_.seed_ = seed_;
    }
}

std::vector<AbstractProperty> TurbulentDisplaceEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_);
    auto& s = props.emplace_back(); s.setName("Size"); s.setType(PropertyType::Float); s.setValue(size_);
    auto& o = props.emplace_back(); o.setName("Octaves"); o.setType(PropertyType::Integer); o.setValue(octaves_);
    auto& sd = props.emplace_back(); sd.setName("Seed"); sd.setType(PropertyType::Integer); sd.setValue(seed_);
    return props;
}

void TurbulentDisplaceEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Amount") setAmount(v.toFloat());
    else if (k == "Size") setSize(v.toFloat());
    else if (k == "Octaves") setOctaves(v.toInt());
    else if (k == "Seed") setSeed(v.toInt());
} // namespace Artifact

}
