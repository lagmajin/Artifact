module;
#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Effect.GauusianBlur;




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
    setEffectID(UniString("effect.blur.gaussian"));
    setDisplayName(UniString("Gaussian Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
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
