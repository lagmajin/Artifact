module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.ResidualGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius)
{
    const int halfWidth = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    return halfWidth * 2 + 1;
}

class ResidualGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold = 0.6f;
    float radius = 12.0f;
    float intensity = 1.0f;
    float decay = 0.82f;
    float historyMix = 0.75f;
    cv::Mat history;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override
    {
        const auto& source = src.image();
        const float* sourcePixels = source.rgba32fData();
        if (!sourcePixels || source.width() <= 0 || source.height() <= 0) {
            history.release();
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
        cv::Mat currentGlow = color.mul(mask3);
        cv::GaussianBlur(
            currentGlow, currentGlow,
            cv::Size(kernelSizeForRadius(radius), kernelSizeForRadius(radius)),
            std::max(0.1f, radius), std::max(0.1f, radius),
            cv::BORDER_REPLICATE);

        if (history.empty() || history.size() != currentGlow.size() ||
            history.type() != currentGlow.type()) {
            history = currentGlow.clone();
        } else {
            cv::Mat decayedHistory = history * decay;
            cv::max(currentGlow, decayedHistory, history);
        }

        std::vector<cv::Mat> historyChannels;
        cv::split(history, historyChannels);
        const float contribution = intensity * historyMix;
        for (int channel = 0; channel < 3; ++channel) {
            cv::Mat combined =
                channels[channel] + historyChannels[channel] * contribution;
            cv::min(combined, 1.0, channels[channel]);
        }

        cv::Mat result;
        cv::merge(channels, result);
        dst.image().setFromRGBA32F(
            result.ptr<float>(), result.cols, result.rows,
            src.image().colorDescriptor());
    }
};

} // namespace

ResidualGlowEffect::ResidualGlowEffect()
{
    setEffectID(UniString("residual_glow"));
    setDisplayName(UniString("Residual Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ResidualGlowEffectCPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
}

ResidualGlowEffect::~ResidualGlowEffect() = default;

void ResidualGlowEffect::setThreshold(float value)
{
    threshold_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ResidualGlowEffect::setRadius(float value)
{
    radius_ = std::clamp(value, 0.5f, 64.0f);
    syncImpls();
}

void ResidualGlowEffect::setIntensity(float value)
{
    intensity_ = std::clamp(value, 0.0f, 4.0f);
    syncImpls();
}

void ResidualGlowEffect::setDecay(float value)
{
    decay_ = std::clamp(value, 0.0f, 0.995f);
    syncImpls();
}

void ResidualGlowEffect::setHistoryMix(float value)
{
    historyMix_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void ResidualGlowEffect::syncImpls()
{
    if (auto impl = std::dynamic_pointer_cast<ResidualGlowEffectCPUImpl>(cpuImpl())) {
        impl->threshold = threshold_;
        impl->radius = radius_;
        impl->intensity = intensity_;
        impl->decay = decay_;
        impl->historyMix = historyMix_;
        impl->history.release();
    }
}

std::vector<AbstractProperty> ResidualGlowEffect::getProperties() const
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
    auto& decay = properties.emplace_back();
    decay.setName("Decay");
    decay.setType(PropertyType::Float);
    decay.setValue(decay_);
    auto& historyMix = properties.emplace_back();
    historyMix.setName("History Mix");
    historyMix.setType(PropertyType::Float);
    historyMix.setValue(historyMix_);
    return properties;
}

void ResidualGlowEffect::setPropertyValue(const UniString& name, const QVariant& value)
{
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) setThreshold(value.toFloat());
    else if (key == QStringLiteral("Radius")) setRadius(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Decay")) setDecay(value.toFloat());
    else if (key == QStringLiteral("History Mix")) setHistoryMix(value.toFloat());
}

} // namespace Artifact
