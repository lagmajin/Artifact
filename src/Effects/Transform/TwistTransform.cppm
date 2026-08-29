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
module Artifact.Effect.Transform.Twist;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

void TwistTransformCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
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

    const float angle = angle_;
    const float cx = centerX_ * static_cast<float>(width - 1);
    const float cy = centerY_ * static_cast<float>(height - 1);
    const float maxR = std::sqrt(cx * cx + cy * cy);
    if (maxR <= 0.0f) {
        dst = src;
        return;
    }
    const float radians = angle * (3.14159265358979323846f / 180.0f);

    ArtifactCore::Parallel::For(0, height, [&](int y) {
        const cv::Vec4f* srcRow = srcMat.ptr<cv::Vec4f>(y);
        cv::Vec4f* dstRow = dstMat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; x++) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float r = std::sqrt(dx * dx + dy * dy);
            const float factor = 1.0f - r / maxR;
            const float theta = radians * factor;
            const float c = std::cos(theta);
            const float s = std::sin(theta);
            const float srcX = cx + dx * c - dy * s;
            const float srcY = cy + dx * s + dy * c;

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

void TwistTransformGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    TwistTransformCPUImpl cpu;
    cpu.setAngle(angle_);
    cpu.setCenterX(centerX_);
    cpu.setCenterY(centerY_);
    cpu.applyCPU(src, dst);
}

class TwistTransform::Impl {
public:
    ArtifactCore::SharedPtr<TwistTransformCPUImpl> cpuImpl_;
    ArtifactCore::SharedPtr<TwistTransformGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = ArtifactCore::makeShared<TwistTransformCPUImpl>();
        gpuImpl_ = ArtifactCore::makeShared<TwistTransformGPUImpl>();
    }
};

TwistTransform::TwistTransform() : impl_(new Impl()) {
    setDisplayName(ArtifactCore::UniString("Twist (Geo Transform)"));
    setPipelineStage(EffectPipelineStage::GeometryTransform);
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

TwistTransform::~TwistTransform() {
    delete impl_;
}

void TwistTransform::setAngle(float angle) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setAngle(angle);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setAngle(angle);
}

float TwistTransform::angle() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->angle();
    return 45.0f;
}

void TwistTransform::setCenterX(float cx) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterX(cx);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterX(cx);
}

float TwistTransform::centerX() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->centerX();
    return 0.5f;
}

void TwistTransform::setCenterY(float cy) {
    if (impl_->cpuImpl_) impl_->cpuImpl_->setCenterY(cy);
    if (impl_->gpuImpl_) impl_->gpuImpl_->setCenterY(cy);
}

float TwistTransform::centerY() const {
    if (impl_->cpuImpl_) return impl_->cpuImpl_->centerY();
    return 0.5f;
}

std::vector<ArtifactCore::AbstractProperty> TwistTransform::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;
    props.reserve(3);

    auto& angleProp = props.emplace_back();
    angleProp.setName("angle");
    angleProp.setType(ArtifactCore::PropertyType::Float);
    angleProp.setDefaultValue(QVariant(static_cast<double>(angle())));
    angleProp.setValue(QVariant(static_cast<double>(angle())));
    angleProp.setMinValue(QVariant(-720.0));
    angleProp.setMaxValue(QVariant(720.0));

    auto& cxProp = props.emplace_back();
    cxProp.setName("centerX");
    cxProp.setType(ArtifactCore::PropertyType::Float);
    cxProp.setDefaultValue(QVariant(static_cast<double>(centerX())));
    cxProp.setValue(QVariant(static_cast<double>(centerX())));
    cxProp.setMinValue(QVariant(0.0));
    cxProp.setMaxValue(QVariant(1.0));

    auto& cyProp = props.emplace_back();
    cyProp.setName("centerY");
    cyProp.setType(ArtifactCore::PropertyType::Float);
    cyProp.setDefaultValue(QVariant(static_cast<double>(centerY())));
    cyProp.setValue(QVariant(static_cast<double>(centerY())));
    cyProp.setMinValue(QVariant(0.0));
    cyProp.setMaxValue(QVariant(1.0));

    return props;
}

void TwistTransform::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    const QString n = name.toQString();
    if (n == "angle") {
        setAngle(static_cast<float>(value.toDouble()));
    } else if (n == "centerX") {
        setCenterX(static_cast<float>(value.toDouble()));
    } else if (n == "centerY") {
        setCenterY(static_cast<float>(value.toDouble()));
    }
}

}
