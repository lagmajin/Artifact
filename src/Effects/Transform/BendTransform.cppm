module;
#include <QList>
#include <QVariant>
#include <cmath>
#include <opencv2/core/mat.hpp>
#include <opencv2/opencv.hpp>
#include <cstring>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <type_traits>
#include <variant>
#include <any>
#include <mutex>
module Artifact.Effect.Transform.Bend;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

void BendTransformCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    const float* srcData = srcImage.rgba32fData();
    if (!srcData) {
        dst = src;
        return;
    }
    cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

    const int height = srcMat.rows;
    const int width = srcMat.cols;
    cv::Mat dstMat(height, width, CV_32FC4);

    const float amount = angle_;
    const float size = size_;
    if (size <= 0.0f || amount == 0.0f) {
        dst = src;
        return;
    }
    const float dirRad = direction_ * (3.14159265358979323846f / 180.0f);
    const float cosD = std::cos(dirRad);
    const float sinD = std::sin(dirRad);
    const float twoPi = 2.0f * 3.14159265358979323846f;
    const float k = twoPi / size;

    ArtifactCore::Parallel::For(0, height, [&](int y) {
        const cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        cv::Vec4f* dstRow = dstMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; x++) {
            const float nx = std::sin(static_cast<float>(y) * k) * amount;
            const float ny = std::sin(static_cast<float>(x) * k) * amount;
            const float srcX = static_cast<float>(x) + nx * cosD - ny * sinD;
            const float srcY = static_cast<float>(y) + nx * sinD + ny * cosD;

            const int x0 = static_cast<int>(std::floor(srcX));
            const int y0 = static_cast<int>(std::floor(srcY));
            if (x0 < 0 || y0 < 0 || x0 >= width - 1 || y0 >= height - 1) {
                const int cxClamp = std::max(0, std::min(width - 1, static_cast<int>(std::round(srcX))));
                const int cyClamp = std::max(0, std::min(height - 1, static_cast<int>(std::round(srcY))));
                dstRow[x] = srcMat.ptr<cv::Vec4f>(cyClamp)[cxClamp];
                continue;
            }

            const float fx = srcX - static_cast<float>(x0);
            const float fy = srcY - static_cast<float>(y0);
            const cv::Vec4f& a = srcMat.ptr<cv::Vec4f>(y0)[x0];
            const cv::Vec4f& b = srcMat.ptr<cv::Vec4f>(y0)[x0 + 1];
            const cv::Vec4f& c2 = srcMat.ptr<cv::Vec4f>(y0 + 1)[x0];
            const cv::Vec4f& d = srcMat.ptr<cv::Vec4f>(y0 + 1)[x0 + 1];
            dstRow[x] = a * ((1.0f - fx) * (1.0f - fy))
                      + b * (fx * (1.0f - fy))
                      + c2 * ((1.0f - fx) * fy)
                      + d * (fx * fy);
        }
    });

    ImageF32x4_RGBA dstImage;
    dstImage.setFromRGBA32F(
        dstMat.ptr<float>(), dstMat.cols, dstMat.rows,
        srcImage.colorDescriptor());
    dst = ImageF32x4RGBAWithCache(dstImage);
}

void BendTransformGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    BendTransformCPUImpl cpu;
    cpu.setAngle(angle_);
    cpu.setDirection(direction_);
    cpu.setSize(size_);
    cpu.applyCPU(src, dst);
}

class BendTransform::Impl {
public:
    ArtifactCore::SharedPtr<BendTransformCPUImpl> cpuImpl_;
    ArtifactCore::SharedPtr<BendTransformGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = ArtifactCore::makeShared<BendTransformCPUImpl>();
        gpuImpl_ = ArtifactCore::makeShared<BendTransformGPUImpl>();
    }
};

BendTransform::BendTransform() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Bend (Geo Transform)"));
    setPipelineStage(EffectPipelineStage::GeometryTransform);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

BendTransform::~BendTransform() {
    delete impl_;
}

void BendTransform::setAngle(float angle) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setAngle(angle);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setAngle(angle);
}

float BendTransform::angle() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->angle();
    return 0.0f;
}

void BendTransform::setDirection(float dir) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setDirection(dir);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setDirection(dir);
}

float BendTransform::direction() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->direction();
    return 0.0f;
}

void BendTransform::setSize(float s) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setSize(s);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setSize(s);
}

float BendTransform::size() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->size();
    return 100.0f;
}

std::vector<ArtifactCore::AbstractProperty> BendTransform::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(3);

    auto& angleProp = props.emplace_back();
    angleProp.setName("angle");
    angleProp.setType(ArtifactCore::PropertyType::Float);
    angleProp.setDefaultValue(QVariant(static_cast<double>(angle())));
    angleProp.setValue(QVariant(static_cast<double>(angle())));
    angleProp.setMinValue(QVariant(-720.0));
    angleProp.setMaxValue(QVariant(720.0));

    auto& dirProp = props.emplace_back();
    dirProp.setName("direction");
    dirProp.setType(ArtifactCore::PropertyType::Float);
    dirProp.setDefaultValue(QVariant(static_cast<double>(direction())));
    dirProp.setValue(QVariant(static_cast<double>(direction())));
    dirProp.setMinValue(QVariant(-360.0));
    dirProp.setMaxValue(QVariant(360.0));

    auto& sizeProp = props.emplace_back();
    sizeProp.setName("size");
    sizeProp.setType(ArtifactCore::PropertyType::Float);
    sizeProp.setDefaultValue(QVariant(static_cast<double>(size())));
    sizeProp.setValue(QVariant(static_cast<double>(size())));
    sizeProp.setMinValue(QVariant(0.01));
    sizeProp.setMaxValue(QVariant(100000.0));

    return props;
}

void BendTransform::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    const QString n = name.toQString();
    if (n == "angle") {
        setAngle(static_cast<float>(value.toDouble()));
    } else if (n == "direction") {
        setDirection(static_cast<float>(value.toDouble()));
    } else if (n == "size") {
        setSize(static_cast<float>(value.toDouble()));
    }
}

}
