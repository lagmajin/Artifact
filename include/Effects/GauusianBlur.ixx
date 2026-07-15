module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Effect.GauusianBlur;




import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;

export namespace Artifact {

class GaussianBlurCPUImpl : public ArtifactEffectImplBase {
private:
    float sigma_ = 5.0f;
    int kernelSize_ = 0;

    void calculateKernelSize() {
        kernelSize_ = static_cast<int>(6 * sigma_ + 1);
        if (kernelSize_ % 2 == 0) {
            kernelSize_++;
        }
    }

public:
    GaussianBlurCPUImpl() {
        calculateKernelSize();
    }

    explicit GaussianBlurCPUImpl(float sigma) : sigma_(sigma) {
        calculateKernelSize();
    }

    void setSigma(float sigma) {
        sigma_ = sigma;
        calculateKernelSize();
    }

    float sigma() const {
        return sigma_;
    }

    int kernelSize() const {
        return kernelSize_;
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class GaussianBlurGPUImpl : public ArtifactEffectImplBase {
private:
    class Resources;
    float sigma_ = 5.0f;
    std::unique_ptr<Resources> resources_;

public:
    GaussianBlurGPUImpl();
    explicit GaussianBlurGPUImpl(float sigma);
    ~GaussianBlurGPUImpl() override;

    void setSigma(float sigma) {
        sigma_ = sigma;
    }

    float sigma() const {
        return sigma_;
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;
};

class GaussianBlur : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;
public:
    GaussianBlur();
    ~GaussianBlur();

    void setSigma(float sigma);
    float sigma() const;

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;

    bool supportsGPU() const override {
        return true;
    }

    /**
     * @brief ROI 拡張ヒント
     *
     * ガウスブラーのカーネル有効範囲は 3σ。
     * sigma が動的に変わるため、インスタンスから取得する。
     */
    EffectROIHint roiHint() const override;
};

};
