module;
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Mosaic;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class MosaicEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float cellSize_ = 8.0f;
    bool shapeMode_ = false;

    static cv::Vec4f avgColor(const cv::Mat& mat, int x0, int y0, int x1, int y1) {
        cv::Vec4f sum(0, 0, 0, 0);
        int count = 0;
        for (int y = y0; y < y1 && y < mat.rows; ++y) {
            for (int x = x0; x < x1 && x < mat.cols; ++x) {
                sum += mat.at<cv::Vec4f>(y, x);
                ++count;
            }
        }
        if (count > 0) sum /= static_cast<float>(count);
        return sum;
    }

    void fillRect(cv::Mat& mat, int x0, int y0, int x1, int y1, const cv::Vec4f& color) {
        for (int y = y0; y < y1 && y < mat.rows; ++y) {
            for (int x = x0; x < x1 && x < mat.cols; ++x) {
                mat.at<cv::Vec4f>(y, x) = color;
            }
        }
    }

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
        cv::Mat mat(dst.image().height(), dst.image().width(), CV_32FC4, dst.image().rgba32fData());

        const int cell = std::max(1, static_cast<int>(cellSize_));
        // cache: key=cell origin, value=average color
        // lazy recompute each cell (cheap for small images)
        for (int by = 0; by < h; by += cell) {
            for (int bx = 0; bx < w; bx += cell) {
                const int x1 = std::min(bx + cell, w);
                const int y1 = std::min(by + cell, h);
                if (shapeMode_) {
                    // Diamond shape sample: Manhattan distance center
                    const int cx = (bx + x1) / 2;
                    const int cy = (by + y1) / 2;
                    const int radius = std::min(x1 - bx, y1 - by) / 2;
                    cv::Vec4f sum(0, 0, 0, 0);
                    int count = 0;
                    // diamond loop
                    for (int y = by; y < y1; ++y) {
                        for (int x = bx; x < x1; ++x) {
                            const int adx = std::abs(x - cx);
                            const int ady = std::abs(y - cy);
                            if (adx + ady <= radius) {
                                sum += mat.at<cv::Vec4f>(y, x);
                                ++count;
                            }
                        }
                    }
                    if (count > 0) sum /= static_cast<float>(count);
                    // fill diamond shape with averaged color
                    for (int y = by; y < y1; ++y) {
                        for (int x = bx; x < x1; ++x) {
                            const int adx = std::abs(x - cx);
                            const int ady = std::abs(y - cy);
                            if (adx + ady <= radius) {
                                mat.at<cv::Vec4f>(y, x) = sum;
                            }
                        }
                    }
                } else {
                    const cv::Vec4f c = avgColor(mat, bx, by, x1, y1);
                    fillRect(mat, bx, by, x1, y1, c);
                }
            }
        }
    }
};

class MosaicEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
public:
    MosaicEffectCPUImpl cpuImpl_;
};

MosaicEffect::MosaicEffect() {
    setDisplayName(UniString("Mosaic"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<MosaicEffectCPUImpl>());
    setGPUImpl(std::make_shared<MosaicEffectGPUImpl>());
}
MosaicEffect::~MosaicEffect() = default;

float MosaicEffect::cellSize() const { return cellSize_; }
void MosaicEffect::setCellSize(float v) { cellSize_ = std::max(1.0f, v); syncImpls(); }
bool MosaicEffect::shapeMode() const { return shapeMode_; }
void MosaicEffect::setShapeMode(bool v) { shapeMode_ = v; syncImpls(); }

void MosaicEffect::syncImpls() {
    if (auto* c = dynamic_cast<MosaicEffectCPUImpl*>(cpuImpl().get())) {
        c->cellSize_ = cellSize_;
        c->shapeMode_ = shapeMode_;
    }
    if (auto* g = dynamic_cast<MosaicEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.cellSize_ = cellSize_;
        g->cpuImpl_.shapeMode_ = shapeMode_;
    }
}

std::vector<AbstractProperty> MosaicEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& c = props.emplace_back(); c.setName("Cell Size"); c.setType(PropertyType::Float); c.setValue(cellSize_);
    auto& s = props.emplace_back(); s.setName("Shape Mode"); s.setType(PropertyType::Boolean); s.setValue(shapeMode_);
    return props;
}

void MosaicEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Cell Size") setCellSize(v.toFloat());
    else if (k == "Shape Mode") setShapeMode(v.toBool());
}

} // namespace Artifact
