module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.ChromaticGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius) {
    const int estimated = static_cast<int>(std::ceil(std::max(0.5f, radius) * 2.5f));
    return std::max(3, (estimated * 2) + 1);
}

cv::Vec4f sampleRGBA(const cv::Mat &mat, float x, float y) {
    const float fx = std::clamp(x, 0.0f, static_cast<float>(mat.cols - 1));
    const float fy = std::clamp(y, 0.0f, static_cast<float>(mat.rows - 1));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, mat.cols - 1);
    const int y1 = std::min(y0 + 1, mat.rows - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const cv::Vec4f p00 = mat.at<cv::Vec4f>(y0, x0);
    const cv::Vec4f p10 = mat.at<cv::Vec4f>(y0, x1);
    const cv::Vec4f p01 = mat.at<cv::Vec4f>(y1, x0);
    const cv::Vec4f p11 = mat.at<cv::Vec4f>(y1, x1);

    const cv::Vec4f top = p00 * (1.0f - tx) + p10 * tx;
    const cv::Vec4f bottom = p01 * (1.0f - tx) + p11 * tx;
    return top * (1.0f - ty) + bottom * ty;
}

} // namespace

class ChromaticGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.62f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float dispersion_ = 0.35f;
    float angle_ = 35.0f;
    float tintMix_ = 0.2f;

    void applyCPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        const auto &srcImage = src.image();
        const float *srcData = srcImage.rgba32fData();
        if (!srcData) {
            dst = src;
            return;
        }

        cv::Mat srcMat(srcImage.height(), srcImage.width(), CV_32FC4,
                       const_cast<float *>(srcData));
        std::vector<cv::Mat> channels;
        cv::split(srcMat, channels);

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat alpha = channels[3];

        cv::Mat gray = channels[0] * 0.114f +
                       channels[1] * 0.587f +
                       channels[2] * 0.299f;

        cv::Mat brightMask;
        cv::subtract(gray, cv::Scalar::all(threshold_), brightMask);
        cv::threshold(brightMask, brightMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold_ < 0.999f) {
            brightMask *= 1.0f / std::max(0.001f, 1.0f - threshold_);
        }

        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{brightMask, brightMask, brightMask}, mask3);

        cv::Mat bloomSource = color.mul(mask3);
        const int ksize = kernelSizeForRadius(radius_);
        cv::GaussianBlur(bloomSource, bloomSource, cv::Size(ksize, ksize),
                         std::max(0.1f, radius_), std::max(0.1f, radius_),
                         cv::BORDER_REPLICATE);

        cv::Mat bloom4;
        {
            std::vector<cv::Mat> bloomChannels;
            cv::split(bloomSource, bloomChannels);
            bloomChannels.push_back(alpha);
            cv::merge(bloomChannels, bloom4);
        }

        const float angleRad = angle_ * 3.14159265f / 180.0f;
        const float dirX = std::cos(angleRad);
        const float dirY = std::sin(angleRad);
        const float shift = std::max(0.0f, dispersion_) * std::max(1.0f, radius_ * 0.25f);
        const float shiftX = dirX * shift;
        const float shiftY = dirY * shift;

        cv::Mat result = srcMat.clone();
        for (int y = 0; y < srcMat.rows; ++y) {
            for (int x = 0; x < srcMat.cols; ++x) {
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const cv::Vec4f rSample = sampleRGBA(bloom4, fx + shiftX, fy + shiftY);
                const cv::Vec4f gSample = sampleRGBA(bloom4, fx, fy);
                const cv::Vec4f bSample = sampleRGBA(bloom4, fx - shiftX, fy - shiftY);

                cv::Vec4f &dstPx = result.at<cv::Vec4f>(y, x);
                const cv::Vec3f spectral(
                    bSample[0] * (1.0f - tintMix_) + gSample[0] * tintMix_,
                    gSample[1],
                    rSample[2] * (1.0f - tintMix_) + gSample[2] * tintMix_);
                dstPx[0] = std::clamp(dstPx[0] + spectral[0] * intensity_, 0.0f, 1.0f);
                dstPx[1] = std::clamp(dstPx[1] + spectral[1] * intensity_, 0.0f, 1.0f);
                dstPx[2] = std::clamp(dstPx[2] + spectral[2] * intensity_, 0.0f, 1.0f);
            }
        }

        dst.image().setFromRGBA32F(result.ptr<float>(), result.cols, result.rows);
    }
};

class ChromaticGlowEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.62f;
    float radius_ = 12.0f;
    float intensity_ = 1.0f;
    float dispersion_ = 0.35f;
    float angle_ = 35.0f;
    float tintMix_ = 0.2f;

    void applyCPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache &src, ImageF32x4RGBAWithCache &dst) override {
        applyCPU(src, dst);
    }

private:
    ChromaticGlowEffectCPUImpl cpuImpl_;
};

ChromaticGlowEffect::ChromaticGlowEffect() {
    setEffectID(UniString("chromatic_glow"));
    setDisplayName(UniString("Chromatic Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ChromaticGlowEffectCPUImpl>());
    setGPUImpl(std::make_shared<ChromaticGlowEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

ChromaticGlowEffect::~ChromaticGlowEffect() = default;

void ChromaticGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<ChromaticGlowEffectCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->radius_ = radius_;
        cpu->intensity_ = intensity_;
        cpu->dispersion_ = dispersion_;
        cpu->angle_ = angle_;
        cpu->tintMix_ = tintMix_;
    }
    if (auto gpu = std::dynamic_pointer_cast<ChromaticGlowEffectGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->radius_ = radius_;
        gpu->intensity_ = intensity_;
        gpu->dispersion_ = dispersion_;
        gpu->angle_ = angle_;
        gpu->tintMix_ = tintMix_;
    }
}

std::vector<AbstractProperty> ChromaticGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(6);

    auto &thresholdProp = props.emplace_back();
    thresholdProp.setName("Threshold");
    thresholdProp.setType(PropertyType::Float);
    thresholdProp.setValue(threshold_);

    auto &radiusProp = props.emplace_back();
    radiusProp.setName("Radius");
    radiusProp.setType(PropertyType::Float);
    radiusProp.setValue(radius_);

    auto &intensityProp = props.emplace_back();
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);

    auto &dispersionProp = props.emplace_back();
    dispersionProp.setName("Dispersion");
    dispersionProp.setType(PropertyType::Float);
    dispersionProp.setValue(dispersion_);

    auto &angleProp = props.emplace_back();
    angleProp.setName("Angle");
    angleProp.setType(PropertyType::Float);
    angleProp.setValue(angle_);

    auto &tintProp = props.emplace_back();
    tintProp.setName("Tint Mix");
    tintProp.setType(PropertyType::Float);
    tintProp.setValue(tintMix_);

    return props;
}

void ChromaticGlowEffect::setPropertyValue(const UniString &name, const QVariant &value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) {
        setThreshold(value.toFloat());
    } else if (key == QStringLiteral("Radius")) {
        setRadius(value.toFloat());
    } else if (key == QStringLiteral("Intensity")) {
        setIntensity(value.toFloat());
    } else if (key == QStringLiteral("Dispersion")) {
        setDispersion(value.toFloat());
    } else if (key == QStringLiteral("Angle")) {
        setAngle(value.toFloat());
    } else if (key == QStringLiteral("Tint Mix")) {
        setTintMix(value.toFloat());
    }
}

} // namespace Artifact
