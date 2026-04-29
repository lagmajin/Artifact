module;
#include <utility>
#include <memory>
#include <opencv2/opencv.hpp>
#include <QList>

module Artifact.Effect.Rasterizer.Blur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Utils.String.UniString;

namespace Artifact {

class BlurEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float sigma_ = 5.0f;
    int iterations_ = 1;
    BlurMode mode_ = BlurMode::Gaussian;
    bool premultiplied_ = true;
    float edgeThreshold_ = 0.1f;

    void setSigma(float s) { sigma_ = s; }
    void setIterations(int n) { iterations_ = n; }
    void setMode(BlurMode m) { mode_ = m; }
    void setPremultiplied(bool p) { premultiplied_ = p; }
    void setEdgeThreshold(float t) { edgeThreshold_ = t; }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        auto& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat floatMat(srcImage.height(), srcImage.width(), CV_32FC4, const_cast<float*>(srcData));

        std::vector<cv::Mat> channels;
        cv::split(floatMat, channels);
        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        if (premultiplied_) {
            for (int y = 0; y < color.rows; ++y) {
                for (int x = 0; x < color.cols; ++x) {
                    const float a = alpha.at<float>(y, x);
                    if (a > 0.001f) {
                        cv::Vec3f& pixel = color.at<cv::Vec3f>(y, x);
                        pixel[0] /= a;
                        pixel[1] /= a;
                        pixel[2] /= a;
                    }
                }
            }
        }

        const int ksize = std::max(3, static_cast<int>(sigma_ * 6.0f) | 1);
        for (int i = 0; i < iterations_; ++i) {
            cv::GaussianBlur(color, color, cv::Size(ksize, ksize),
                             std::max(0.1f, sigma_),
                             std::max(0.1f, sigma_),
                             cv::BORDER_REPLICATE);
            if (mode_ == BlurMode::EdgePreserving) {
                cv::GaussianBlur(color, color, cv::Size(ksize, ksize),
                                 std::max(0.1f, sigma_ * 0.6f),
                                 std::max(0.1f, sigma_ * 0.6f),
                                 cv::BORDER_REPLICATE);
            }
        }

        if (premultiplied_) {
            for (int y = 0; y < color.rows; ++y) {
                for (int x = 0; x < color.cols; ++x) {
                    const float a = alpha.at<float>(y, x);
                    cv::Vec3f& pixel = color.at<cv::Vec3f>(y, x);
                    pixel[0] *= a;
                    pixel[1] *= a;
                    pixel[2] *= a;
                }
            }
        }

        std::vector<cv::Mat> outChannels;
        cv::split(color, outChannels);
        outChannels.push_back(alpha);
        cv::merge(outChannels, floatMat);
        dst.image().setFromRGBA32F(floatMat.ptr<float>(), floatMat.cols, floatMat.rows);
    }
};

class BlurEffectGPUImpl : public ArtifactEffectImplBase {
public:
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

    void setSigma(float s) { cpuImpl_.setSigma(s); }
    void setIterations(int n) { cpuImpl_.setIterations(n); }
    void setMode(BlurMode m) { cpuImpl_.setMode(m); }
    void setPremultiplied(bool p) { cpuImpl_.setPremultiplied(p); }
    void setEdgeThreshold(float t) { cpuImpl_.setEdgeThreshold(t); }

private:
    BlurEffectCPUImpl cpuImpl_;
};

BlurEffect::BlurEffect() {
    setDisplayName(UniString("Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<BlurEffectCPUImpl>());
    setGPUImpl(std::make_shared<BlurEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

void BlurEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<BlurEffectCPUImpl*>(cpuImpl().get())) {
        cpu->setSigma(sigma_);
        cpu->setIterations(iterations_);
        cpu->setMode(mode_);
        cpu->setPremultiplied(premultiplied_);
        cpu->setEdgeThreshold(edgeThreshold_);
    }
    if (auto* gpu = dynamic_cast<BlurEffectGPUImpl*>(gpuImpl().get())) {
        gpu->setSigma(sigma_);
        gpu->setIterations(iterations_);
        gpu->setMode(mode_);
        gpu->setPremultiplied(premultiplied_);
        gpu->setEdgeThreshold(edgeThreshold_);
    }
}

} // namespace Artifact
