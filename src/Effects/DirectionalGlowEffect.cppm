module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QVariant>
#include <vector>
#include <opencv2/opencv.hpp>

module Artifact.Effect.DirectionalGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

static cv::Mat directionalBlur1D(const cv::Mat& src, float angleDeg, float length) {
    if (src.empty()) {
        return src;
    }

    const int radius = std::max(1, static_cast<int>(length * 0.5f));
    const float angleRad = angleDeg * (3.14159265f / 180.0f);
    const float dx = std::cos(angleRad);
    const float dy = std::sin(angleRad);

    cv::Mat result = cv::Mat::zeros(src.size(), src.type());

    for (int y = 0; y < src.rows; ++y) {
        for (int x = 0; x < src.cols; ++x) {
            cv::Vec4f sum(0, 0, 0, 0);
            float totalWeight = 0.0f;

            for (int s = -radius; s <= radius; ++s) {
                float sx = x + dx * s;
                float sy = y + dy * s;
                sx = std::clamp(sx, 0.0f, static_cast<float>(src.cols - 1));
                sy = std::clamp(sy, 0.0f, static_cast<float>(src.rows - 1));

                const int ix = static_cast<int>(sx);
                const int iy = static_cast<int>(sy);
                const float fx = sx - ix;
                const float fy = sy - iy;
                const int ix2 = std::min(ix + 1, src.cols - 1);
                const int iy2 = std::min(iy + 1, src.rows - 1);

                const cv::Vec4f p00 = src.at<cv::Vec4f>(iy, ix);
                const cv::Vec4f p10 = src.at<cv::Vec4f>(iy, ix2);
                const cv::Vec4f p01 = src.at<cv::Vec4f>(iy2, ix);
                const cv::Vec4f p11 = src.at<cv::Vec4f>(iy2, ix2);

                const cv::Vec4f sample = p00 * (1.0f - fx) * (1.0f - fy) +
                                         p10 * fx * (1.0f - fy) +
                                         p01 * (1.0f - fx) * fy +
                                         p11 * fx * fy;

                const float weight = std::exp(-0.5f * (s * s) / (radius * radius * 0.25f));
                sum += sample * weight;
                totalWeight += weight;
            }

            result.at<cv::Vec4f>(y, x) = totalWeight > 0.0f ? sum / totalWeight : cv::Vec4f(0, 0, 0, 0);
        }
    }

    return result;
}

static QVector<float> getAnglesForPattern(StreakPattern pattern, float angleOffset) {
    QVector<float> angles;
    switch (pattern) {
    case StreakPattern::Horizontal:
        angles.append(0.0f + angleOffset);
        break;
    case StreakPattern::Cross:
        angles.append(0.0f + angleOffset);
        angles.append(90.0f + angleOffset);
        break;
    case StreakPattern::Star:
        angles.append(0.0f + angleOffset);
        angles.append(45.0f + angleOffset);
        angles.append(90.0f + angleOffset);
        angles.append(135.0f + angleOffset);
        break;
    case StreakPattern::Custom:
        break;
    }
    return angles;
}

class DirectionalGlowCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_;
    float angleOffset_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cv::Mat mat = src.image().toCVMat();
        if (mat.empty()) {
            dst = src;
            return;
        }

        cv::Mat bright = cv::Mat::zeros(mat.size(), CV_32FC4);
        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                const cv::Vec4f p = mat.at<cv::Vec4f>(y, x);
                const float lum = 0.299f * p[2] + 0.587f * p[1] + 0.114f * p[0];
                if (lum > threshold_) {
                    const float scale = (lum - threshold_) / (1.0f - threshold_);
                    bright.at<cv::Vec4f>(y, x) = p * scale;
                }
            }
        }

        const QVector<float> angles = customAngles_.isEmpty()
            ? getAnglesForPattern(pattern_, angleOffset_)
            : customAngles_;
        if (angles.isEmpty()) {
            dst = src;
            return;
        }

        cv::Mat streaks = cv::Mat::zeros(mat.size(), CV_32FC4);
        for (float angle : angles) {
            const cv::Mat s1 = directionalBlur1D(bright, angle, length1_);
            const cv::Mat s2 = directionalBlur1D(bright, angle, length2_);
            streaks += s1 * weight1_ + s2 * weight2_;
        }

        cv::Mat result = mat.clone();
        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& dstP = result.at<cv::Vec4f>(y, x);
                const cv::Vec4f streakP = streaks.at<cv::Vec4f>(y, x) * intensity_;
                dstP += streakP;
                dstP[0] = std::clamp(dstP[0], 0.0f, 1.0f);
                dstP[1] = std::clamp(dstP[1], 0.0f, 1.0f);
                dstP[2] = std::clamp(dstP[2], 0.0f, 1.0f);
            }
        }

        dst.image().setFromCVMat(result);
    }
};

class DirectionalGlowGPUImpl : public ArtifactEffectImplBase {
public:
    float threshold_ = 0.8f;
    float intensity_ = 1.0f;
    float length1_ = 64.0f;
    float length2_ = 128.0f;
    float weight1_ = 0.6f;
    float weight2_ = 0.4f;
    StreakPattern pattern_ = StreakPattern::Horizontal;
    QVector<float> customAngles_;
    float angleOffset_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    DirectionalGlowCPUImpl cpuImpl_;
};

DirectionalGlowEffect::DirectionalGlowEffect() {
    setDisplayName(UniString("Directional Glow / Streaks"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<DirectionalGlowCPUImpl>());
    setGPUImpl(std::make_shared<DirectionalGlowGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

DirectionalGlowEffect::~DirectionalGlowEffect() = default;

void DirectionalGlowEffect::syncImpls() {
    if (auto cpu = std::dynamic_pointer_cast<DirectionalGlowCPUImpl>(cpuImpl())) {
        cpu->threshold_ = threshold_;
        cpu->intensity_ = intensity_;
        cpu->length1_ = length1_;
        cpu->length2_ = length2_;
        cpu->weight1_ = weight1_;
        cpu->weight2_ = weight2_;
        cpu->pattern_ = pattern_;
        cpu->customAngles_ = customAngles_;
        cpu->angleOffset_ = angleOffset_;
    }
    if (auto gpu = std::dynamic_pointer_cast<DirectionalGlowGPUImpl>(gpuImpl())) {
        gpu->threshold_ = threshold_;
        gpu->intensity_ = intensity_;
        gpu->length1_ = length1_;
        gpu->length2_ = length2_;
        gpu->weight1_ = weight1_;
        gpu->weight2_ = weight2_;
        gpu->pattern_ = pattern_;
        gpu->customAngles_ = customAngles_;
        gpu->angleOffset_ = angleOffset_;
    }
}

std::vector<AbstractProperty> DirectionalGlowEffect::getProperties() const {
    std::vector<AbstractProperty> props;

    AbstractProperty threshProp;
    threshProp.setName("Threshold");
    threshProp.setType(PropertyType::Float);
    threshProp.setValue(threshold_);
    props.push_back(threshProp);

    AbstractProperty intensityProp;
    intensityProp.setName("Intensity");
    intensityProp.setType(PropertyType::Float);
    intensityProp.setValue(intensity_);
    props.push_back(intensityProp);

    AbstractProperty len1Prop;
    len1Prop.setName("Length 1 (Inner)");
    len1Prop.setType(PropertyType::Float);
    len1Prop.setValue(length1_);
    props.push_back(len1Prop);

    AbstractProperty len2Prop;
    len2Prop.setName("Length 2 (Outer)");
    len2Prop.setType(PropertyType::Float);
    len2Prop.setValue(length2_);
    props.push_back(len2Prop);

    AbstractProperty w1Prop;
    w1Prop.setName("Weight 1");
    w1Prop.setType(PropertyType::Float);
    w1Prop.setValue(weight1_);
    props.push_back(w1Prop);

    AbstractProperty w2Prop;
    w2Prop.setName("Weight 2");
    w2Prop.setType(PropertyType::Float);
    w2Prop.setValue(weight2_);
    props.push_back(w2Prop);

    AbstractProperty patternProp;
    patternProp.setName("Pattern");
    patternProp.setType(PropertyType::Integer);
    patternProp.setValue(static_cast<int>(pattern_));
    props.push_back(patternProp);

    AbstractProperty angleProp;
    angleProp.setName("Angle Offset");
    angleProp.setType(PropertyType::Float);
    angleProp.setValue(angleOffset_);
    props.push_back(angleProp);

    return props;
}

void DirectionalGlowEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) setThreshold(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Length 1 (Inner)")) setLength1(value.toFloat());
    else if (key == QStringLiteral("Length 2 (Outer)")) setLength2(value.toFloat());
    else if (key == QStringLiteral("Weight 1")) setWeight1(value.toFloat());
    else if (key == QStringLiteral("Weight 2")) setWeight2(value.toFloat());
    else if (key == QStringLiteral("Pattern")) setPattern(static_cast<StreakPattern>(value.toInt()));
    else if (key == QStringLiteral("Angle Offset")) setAngleOffset(value.toFloat());
}

} // namespace Artifact
