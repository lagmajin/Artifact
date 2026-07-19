module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.LiquidGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius)
{
    const int halfWidth = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    return halfWidth * 2 + 1;
}

class LiquidGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold = 0.55f;
    float radius = 16.0f;
    float intensity = 1.1f;
    float flowScale = 42.0f;
    float distortion = 8.0f;
    float phase = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override
    {
        const auto& source = src.image();
        const float* sourcePixels = source.rgba32fData();
        if (!sourcePixels || source.width() <= 0 || source.height() <= 0) {
            dst = src.DeepCopy();
            return;
        }

        cv::Mat sourceMat(source.height(), source.width(), CV_32FC4,
                          const_cast<float*>(sourcePixels));
        std::vector<cv::Mat> channels;
        cv::split(sourceMat, channels);

        cv::Mat luminance =
            channels[0] * 0.114f + channels[1] * 0.587f + channels[2] * 0.299f;
        cv::Mat brightMask;
        cv::subtract(luminance, cv::Scalar::all(threshold), brightMask);
        cv::threshold(brightMask, brightMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold < 0.999f) {
            brightMask *= 1.0f / std::max(0.001f, 1.0f - threshold);
        }

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{brightMask, brightMask, brightMask}, mask3);
        cv::Mat glow = color.mul(mask3);
        cv::GaussianBlur(glow, glow,
                         cv::Size(kernelSizeForRadius(radius), kernelSizeForRadius(radius)),
                         std::max(0.1f, radius), std::max(0.1f, radius),
                         cv::BORDER_REPLICATE);

        cv::Mat mapX(source.height(), source.width(), CV_32FC1);
        cv::Mat mapY(source.height(), source.width(), CV_32FC1);
        const float scale = std::max(4.0f, flowScale);
        ArtifactCore::Parallel::For(0, source.height(), [&](int y) {
            float* xRow = mapX.ptr<float>(y);
            float* yRow = mapY.ptr<float>(y);
            for (int x = 0; x < source.width(); ++x) {
                const float nx = static_cast<float>(x) / scale;
                const float ny = static_cast<float>(y) / scale;
                const float flowX =
                    std::sin(ny * 1.73f + phase) +
                    std::sin((nx + ny) * 0.71f - phase * 0.63f) * 0.5f;
                const float flowY =
                    std::cos(nx * 1.37f - phase * 0.81f) +
                    std::cos((nx - ny) * 0.83f + phase) * 0.5f;
                xRow[x] = static_cast<float>(x) + flowX * distortion;
                yRow[x] = static_cast<float>(y) + flowY * distortion;
            }
        });

        cv::Mat flowedGlow(glow.rows, glow.cols, glow.type());
        auto reflect101 = [](int v, int limit) {
            if (limit <= 1) return 0;
            while (v < 0 || v >= limit) {
                if (v < 0) v = -v;
                if (v >= limit) v = 2 * limit - v - 2;
            }
            return v;
        };
        ArtifactCore::Parallel::For(0, glow.rows, [&](int y) {
            const float* xRow = mapX.ptr<float>(y);
            const float* yRow = mapY.ptr<float>(y);
            const cv::Vec3f* source = glow.ptr<cv::Vec3f>(0);
            cv::Vec3f* output = flowedGlow.ptr<cv::Vec3f>(y);
            for (int x = 0; x < glow.cols; ++x) {
                const float sx = xRow[x];
                const float sy = yRow[x];
                const int x0 = static_cast<int>(std::floor(sx));
                const int y0 = static_cast<int>(std::floor(sy));
                const int x1 = x0 + 1;
                const int y1 = y0 + 1;
                const float tx = sx - static_cast<float>(x0);
                const float ty = sy - static_cast<float>(y0);
                const cv::Vec3f& p00 = source[reflect101(y0, glow.rows) * glow.cols + reflect101(x0, glow.cols)];
                const cv::Vec3f& p10 = source[reflect101(y0, glow.rows) * glow.cols + reflect101(x1, glow.cols)];
                const cv::Vec3f& p01 = source[reflect101(y1, glow.rows) * glow.cols + reflect101(x0, glow.cols)];
                const cv::Vec3f& p11 = source[reflect101(y1, glow.rows) * glow.cols + reflect101(x1, glow.cols)];
                output[x] = p00 * ((1.0f - tx) * (1.0f - ty)) +
                            p10 * (tx * (1.0f - ty)) +
                            p01 * ((1.0f - tx) * ty) + p11 * (tx * ty);
            }
        });

        cv::Mat result = sourceMat.clone();
        std::vector<cv::Mat> flowedChannels;
        cv::split(flowedGlow, flowedChannels);
        for (int channel = 0; channel < 3; ++channel) {
            cv::Mat combined = channels[channel] + flowedChannels[channel] * intensity;
            cv::min(combined, 1.0, channels[channel]);
        }
        cv::merge(channels, result);
        dst.image().setFromRGBA32F(
            result.ptr<float>(), result.cols, result.rows,
            src.image().colorDescriptor());
    }
};

} // namespace

LiquidGlowEffect::LiquidGlowEffect()
{
    setEffectID(UniString("liquid_glow"));
    setDisplayName(UniString("Liquid Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LiquidGlowEffectCPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

LiquidGlowEffect::~LiquidGlowEffect() = default;

void LiquidGlowEffect::setThreshold(float value)
{
    threshold_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void LiquidGlowEffect::setRadius(float value)
{
    radius_ = std::clamp(value, 0.5f, 64.0f);
    syncImpls();
}

void LiquidGlowEffect::setIntensity(float value)
{
    intensity_ = std::clamp(value, 0.0f, 4.0f);
    syncImpls();
}

void LiquidGlowEffect::setFlowScale(float value)
{
    flowScale_ = std::clamp(value, 4.0f, 256.0f);
    syncImpls();
}

void LiquidGlowEffect::setDistortion(float value)
{
    distortion_ = std::clamp(value, 0.0f, 64.0f);
    syncImpls();
}

void LiquidGlowEffect::setPhase(float value)
{
    phase_ = value;
    syncImpls();
}

void LiquidGlowEffect::syncImpls()
{
    if (auto impl = std::dynamic_pointer_cast<LiquidGlowEffectCPUImpl>(cpuImpl())) {
        impl->threshold = threshold_;
        impl->radius = radius_;
        impl->intensity = intensity_;
        impl->flowScale = flowScale_;
        impl->distortion = distortion_;
        impl->phase = phase_;
    }
}

std::vector<AbstractProperty> LiquidGlowEffect::getProperties() const
{
    std::vector<AbstractProperty> properties;
    auto& threshold = properties.emplace_back();
    threshold.setName("Threshold");
    threshold.setType(PropertyType::Float);
    threshold.setValue(threshold_);
    auto& radius = properties.emplace_back();
    radius.setName("Radius");
    radius.setType(PropertyType::Float);
    radius.setValue(radius_);
    auto& intensity = properties.emplace_back();
    intensity.setName("Intensity");
    intensity.setType(PropertyType::Float);
    intensity.setValue(intensity_);
    auto& flowScale = properties.emplace_back();
    flowScale.setName("Flow Scale");
    flowScale.setType(PropertyType::Float);
    flowScale.setValue(flowScale_);
    auto& distortion = properties.emplace_back();
    distortion.setName("Distortion");
    distortion.setType(PropertyType::Float);
    distortion.setValue(distortion_);
    auto& phase = properties.emplace_back();
    phase.setName("Phase");
    phase.setType(PropertyType::Float);
    phase.setValue(phase_);
    return properties;
}

void LiquidGlowEffect::setPropertyValue(const UniString& name, const QVariant& value)
{
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) setThreshold(value.toFloat());
    else if (key == QStringLiteral("Radius")) setRadius(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Flow Scale")) setFlowScale(value.toFloat());
    else if (key == QStringLiteral("Distortion")) setDistortion(value.toFloat());
    else if (key == QStringLiteral("Phase")) setPhase(value.toFloat());
}

} // namespace Artifact
