module;
#include <QString>
#include <QVariant>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

module Artifact.Effect.DirectionalGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import CvUtils;

namespace Artifact {

// ─────────────────────────────────────────────────────────
// Directional 1D Blur (CPU)
// ─────────────────────────────────────────────────────────

static cv::Mat directionalBlur1D(const cv::Mat& src, float angleDeg, float length) {
    if (src.empty()) return src;

    int radius = std::max(1, static_cast<int>(length * 0.5f));
    int samples = radius * 2 + 1;

    float angleRad = angleDeg * (3.14159265f / 180.0f);
    float dx = std::cos(angleRad);
    float dy = std::sin(angleRad);

    cv::Mat result = cv::Mat::zeros(src.size(), src.type());

    for (int y = 0; y < src.rows; ++y) {
        for (int x = 0; x < src.cols; ++x) {
            cv::Vec4f sum(0, 0, 0, 0);
            float totalWeight = 0.0f;

            for (int s = -radius; s <= radius; ++s) {
                float sx = x + dx * s;
                float sy = y + dy * s;

                // Clamp to edges
                sx = std::clamp(sx, 0.0f, static_cast<float>(src.cols - 1));
                sy = std::clamp(sy, 0.0f, static_cast<float>(src.rows - 1));

                int ix = static_cast<int>(sx);
                int iy = static_cast<int>(sy);

                // Bilinear interpolation
                float fx = sx - ix;
                float fy = sy - iy;
                int ix2 = std::min(ix + 1, src.cols - 1);
                int iy2 = std::min(iy + 1, src.rows - 1);

                cv::Vec4f p00 = src.at<cv::Vec4f>(iy, ix);
                cv::Vec4f p10 = src.at<cv::Vec4f>(iy, ix2);
                cv::Vec4f p01 = src.at<cv::Vec4f>(iy2, ix);
                cv::Vec4f p11 = src.at<cv::Vec4f>(iy2, ix2);

                cv::Vec4f sample = p00 * (1 - fx) * (1 - fy) +
                                   p10 * fx * (1 - fy) +
                                   p01 * (1 - fx) * fy +
                                   p11 * fx * fy;

                // Gaussian weight along the streak
                float weight = std::exp(-0.5f * (s * s) / (radius * radius * 0.25f));
                sum += sample * weight;
                totalWeight += weight;
            }

            result.at<cv::Vec4f>(y, x) = totalWeight > 0 ? sum / totalWeight : cv::Vec4f(0, 0, 0, 0);
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
        // Use custom angles
        break;
    }
    return angles;
}

// ─────────────────────────────────────────────────────────
// CPU 実装
// ─────────────────────────────────────────────────────────

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
        cv::Mat mat = src.toCvMat();
        if (mat.empty()) { dst = src; return; }

        // 1. Brightness threshold mask
        cv::Mat bright = cv::Mat::zeros(mat.size(), CV_32FC4);
        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f p = mat.at<cv::Vec4f>(y, x);
                float lum = 0.299f * p[2] + 0.587f * p[1] + 0.114f * p[0]; // RGB luminance
                if (lum > threshold_) {
                    float scale = (lum - threshold_) / (1.0f - threshold_);
                    bright.at<cv::Vec4f>(y, x) = p * scale;
                }
            }
        }

        // 2. Get streak angles
        QVector<float> angles = customAngles_.isEmpty()
            ? getAnglesForPattern(pattern_, angleOffset_)
            : customAngles_;

        if (angles.isEmpty()) { dst = src; return; }

        // 3. Apply directional blur for each angle and accumulate
        cv::Mat streaks = cv::Mat::zeros(mat.size(), CV_32FC4);

        for (float angle : angles) {
            cv::Mat s1 = directionalBlur1D(bright, angle, length1_);
            cv::Mat s2 = directionalBlur1D(bright, angle, length2_);
            streaks += s1 * weight1_ + s2 * weight2_;
        }

        // 4. Composite: src + streaks * intensity
        cv::Mat result = mat.clone();
        for (int y = 0; y < mat.rows; ++y) {
            for (int x = 0; x < mat.cols; ++x) {
                cv::Vec4f& dstP = result.at<cv::Vec4f>(y, x);
                cv::Vec4f streakP = streaks.at<cv::Vec4f>(y, x) * intensity_;
                dstP += streakP;
                dstP[0] = std::clamp(dstP[0], 0.0f, 1.0f);
                dstP[1] = std::clamp(dstP[1], 0.0f, 1.0f);
                dstP[2] = std::clamp(dstP[2], 0.0f, 1.0f);
            }
        }

        dst.fromCvMat(result);
    }
};

// ─────────────────────────────────────────────────────────
// GPU 実装 (CPU fallback)
// ─────────────────────────────────────────────────────────

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
        // TODO: Diligent Engine による GPU 実装
        // 1. Brightness threshold pass
        // 2. Directional blur passes per angle (compute shader or separable PS)
        // 3. Composite pass
        applyCPU(src, dst);
    }

private:
    DirectionalGlowCPUImpl cpuImpl_;
};

// ─────────────────────────────────────────────────────────
// DirectionalGlowEffect 本体
// ─────────────────────────────────────────────────────────

DirectionalGlowEffect::DirectionalGlowEffect() {
    setDisplayName(ArtifactCore::UniString("Directional Glow / Streaks"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpuImpl = std::make_shared<DirectionalGlowCPUImpl>();
    auto gpuImpl = std::make_shared<DirectionalGlowGPUImpl>();
    setCPUImpl(cpuImpl);
    setGPUImpl(gpuImpl);
    setComputeMode(ComputeMode::AUTO);
}

DirectionalGlowEffect::~DirectionalGlowEffect() = default;

void DirectionalGlowEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<DirectionalGlowCPUImpl*>(cpuImpl_.get())) {
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
    if (auto* gpu = dynamic_cast<DirectionalGlowGPUImpl*>(gpuImpl_.get())) {
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
    patternProp.setType(PropertyType::Enum);
    patternProp.setValue(static_cast<int>(pattern_));
    patternProp.setEnumLabels({"Horizontal", "Cross", "Star", "Custom"});
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
