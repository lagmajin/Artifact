module;
#include <QString>
#include <QVariant>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

module Artifact.Effect.Rasterizer.Blur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Utils.String.UniString;
import Property.Abstract;
import Image.ImageF32x4RGBAWithCache;
import CvUtils;

namespace Artifact {

BlurEffect::BlurEffect() {
    setDisplayName(ArtifactCore::UniString("Blur (Rasterizer)"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<BlurEffectCPUImpl>();
    auto gpuImpl = std::make_shared<BlurEffectGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

void BlurEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<BlurEffectCPUImpl*>(cpuImpl_.get())) {
        cpu->setSigma(sigma_);
        cpu->setIterations(iterations_);
        cpu->setMode(mode_);
        cpu->setPremultiplied(premultiplied_);
        cpu->setEdgeThreshold(edgeThreshold_);
    }
    if (auto* gpu = dynamic_cast<BlurEffectGPUImpl*>(gpuImpl_.get())) {
        gpu->setSigma(sigma_);
        gpu->setIterations(iterations_);
        gpu->setMode(mode_);
        gpu->setPremultiplied(premultiplied_);
        gpu->setEdgeThreshold(edgeThreshold_);
    }
}

void BlurEffect::setSigma(float s) {
    sigma_ = std::max(0.1f, s);
    syncImpls();
}

void BlurEffect::setIterations(int n) {
    iterations_ = std::max(1, n);
    syncImpls();
}

void BlurEffect::setMode(BlurMode m) {
    mode_ = m;
    syncImpls();
}

void BlurEffect::setPremultiplied(bool p) {
    premultiplied_ = p;
    syncImpls();
}

void BlurEffect::setEdgeThreshold(float t) {
    edgeThreshold_ = std::clamp(t, 0.0f, 1.0f);
    syncImpls();
}

// ─────────────────────────────────────────────────────────
// BlurEffectCPUImpl
//
// 実装方針:
// 1. separable Gaussian（水平+垂直の2パス）で高速化
// 2. エッジクランプ（黒にじみ防止）— BORDER_REPLICATE
// 3. premultiplied alpha 対応
// 4. edge-preserving モード（軽量 bilateral）
// ─────────────────────────────────────────────────────────

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
        cv::Mat srcMat = src.toCvMat();
        if (srcMat.empty()) {
            dst = src;
            return;
        }

        cv::Mat workingMat;
        if (srcMat.channels() == 4) {
            // BGRA → BGR + A に分離
            std::vector<cv::Mat> channels;
            cv::split(srcMat, channels);
            cv::Mat bgr, alpha;
            cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, bgr);
            alpha = channels[3];

            // Premultiplied の場合: アルファで除算して線形RGBに戻す
            if (premultiplied_) {
                for (int y = 0; y < bgr.rows; ++y) {
                    for (int x = 0; x < bgr.cols; ++x) {
                        float a = alpha.at<uchar>(y, x) / 255.0f;
                        if (a > 0.001f) {
                            cv::Vec3b& pixel = bgr.at<cv::Vec3b>(y, x);
                            pixel[0] = static_cast<uchar>(std::min(255, static_cast<int>(pixel[0] / a)));
                            pixel[1] = static_cast<uchar>(std::min(255, static_cast<int>(pixel[1] / a)));
                            pixel[2] = static_cast<uchar>(std::min(255, static_cast<int>(pixel[2] / a)));
                        }
                    }
                }
            }

            // 複数回ブラー（iterations_）
            cv::Mat blurred = bgr;
            int ksize = std::max(3, static_cast<int>(sigma_ * 6.0f) | 1); // 奇数に

            for (int i = 0; i < iterations_; ++i) {
                if (mode_ == BlurMode::Gaussian) {
                    // Separable Gaussian: 水平 + 垂直
                    cv::Mat hBlur, vBlur;
                    cv::GaussianBlur(blurred, hBlur, cv::Size(ksize, 1), sigma_, 0, cv::BORDER_REPLICATE);
                    cv::GaussianBlur(hBlur, vBlur, cv::Size(1, ksize), 0, sigma_, cv::BORDER_REPLICATE);
                    blurred = vBlur;
                } else {
                    // Edge-preserving: 軽量 bilateral（近似）
                    // 本格的 bilateral は重いため、guided filter 近似を使用
                    cv::Mat guided;
                    cv::ximgproc::guidedFilter(blurred, blurred, guided, static_cast<int>(sigma_ * 4), edgeThreshold_ * 255, cv::BORDER_REPLICATE);
                    blurred = guided;
                }
            }

            // アルファチャンネルを再度乗算
            if (premultiplied_) {
                for (int y = 0; y < blurred.rows; ++y) {
                    for (int x = 0; x < blurred.cols; ++x) {
                        float a = alpha.at<uchar>(y, x) / 255.0f;
                        cv::Vec3b& pixel = blurred.at<cv::Vec3b>(y, x);
                        pixel[0] = static_cast<uchar>(pixel[0] * a);
                        pixel[1] = static_cast<uchar>(pixel[1] * a);
                        pixel[2] = static_cast<uchar>(pixel[2] * a);
                    }
                }
            }

            // 再結合
            cv::merge(std::vector<cv::Mat>{blurred, alpha}, workingMat);
        } else {
            // 3チャンネル以下はそのままブラー
            cv::Mat blurred = srcMat;
            int ksize = std::max(3, static_cast<int>(sigma_ * 6.0f) | 1);
            for (int i = 0; i < iterations_; ++i) {
                if (mode_ == BlurMode::Gaussian) {
                    cv::Mat hBlur, vBlur;
                    cv::GaussianBlur(blurred, hBlur, cv::Size(ksize, 1), sigma_, 0, cv::BORDER_REPLICATE);
                    cv::GaussianBlur(hBlur, vBlur, cv::Size(1, ksize), 0, sigma_, cv::BORDER_REPLICATE);
                    blurred = vBlur;
                } else {
                    cv::Mat guided;
                    cv::ximgproc::guidedFilter(blurred, blurred, guided, static_cast<int>(sigma_ * 4), edgeThreshold_ * 255, cv::BORDER_REPLICATE);
                    blurred = guided;
                }
            }
            workingMat = blurred;
        }

        dst.fromCvMat(workingMat);
    }
};

class BlurEffectGPUImpl : public ArtifactEffectImplBase {
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

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // TODO: Diligent Engine による GPU 実装
        // 1. 入力テクスチャを GPU にアップロード
        // 2. 水平ブラー PSO を適用
        // 3. 垂直ブラー PSO を適用
        // 4. 結果を dst に書き戻し
        // 現在は CPU フォールバック
        applyCPU(src, dst);
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

private:
    BlurEffectCPUImpl cpuImpl_;
};

} // namespace Artifact
