module;
#include <opencv2/opencv.hpp>

module Artifact.Effect.GauusianBlur;

import std;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

namespace Artifact {

void GaussianBlurCPUImpl::applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    const ImageF32x4_RGBA& srcImage = src.image();
    cv::Mat srcMat = srcImage.toCVMat();
    cv::Mat dstMat;

    // OpenCVのガウシアンブラーを適用
    cv::GaussianBlur(srcMat, dstMat, cv::Size(kernelSize_, kernelSize_), sigma_);

    // 結果をdstに設定
    ImageF32x4_RGBA dstImage;
    dstImage.setFromCVMat(dstMat);
    dst = ImageF32x4RGBAWithCache(dstImage);
}

void GaussianBlurGPUImpl::applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // 現在はCPUバックエンドにフォールバック
    // TODO: HLSLシェーダの実装
    GaussianBlurCPUImpl cpuImpl(sigma_);
    cpuImpl.applyCPU(src, dst);
}

class GaussianBlur::Impl {
public:
    std::shared_ptr<GaussianBlurCPUImpl> cpuImpl_;
    std::shared_ptr<GaussianBlurGPUImpl> gpuImpl_;

    Impl() {
        cpuImpl_ = std::make_shared<GaussianBlurCPUImpl>();
        gpuImpl_ = std::make_shared<GaussianBlurGPUImpl>();
    }
};

GaussianBlur::GaussianBlur() : impl_(new Impl()) {
    setCPUImpl(impl_->cpuImpl_);
    setGPUImpl(impl_->gpuImpl_);
}

GaussianBlur::~GaussianBlur() {
    delete impl_;
}

void GaussianBlur::setSigma(float sigma) {
    if (impl_->cpuImpl_) {
        impl_->cpuImpl_->setSigma(sigma);
    }
    if (impl_->gpuImpl_) {
        impl_->gpuImpl_->setSigma(sigma);
    }
}

float GaussianBlur::sigma() const {
    if (impl_->cpuImpl_) {
        return impl_->cpuImpl_->sigma();
    }
    return 0.0f;
}

}