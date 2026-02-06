module;
export module Artifact.Effect.GauusianBlur;

import std;
import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

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
    float sigma_ = 5.0f;

public:
    GaussianBlurGPUImpl() = default;
    explicit GaussianBlurGPUImpl(float sigma) : sigma_(sigma) {}

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

    bool supportsGPU() const override {
        return true;
    }
};

};