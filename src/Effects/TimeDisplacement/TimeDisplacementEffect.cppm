module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Distort.TimeDisplacement;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

namespace {

inline float sampleChannel(const float* pixel4, const TimeDisplaceChannel channel) {
    switch (channel) {
        case TimeDisplaceChannel::Red:       return pixel4[0];
        case TimeDisplaceChannel::Green:     return pixel4[1];
        case TimeDisplaceChannel::Blue:      return pixel4[2];
        case TimeDisplaceChannel::Alpha:     return pixel4[3];
        case TimeDisplaceChannel::Luminance:
        default:
            return pixel4[0] * 0.299f + pixel4[1] * 0.587f + pixel4[2] * 0.114f;
    }
}

inline cv::Vec4f pixelAtClamped(const cv::Mat& mat, int x, int y) {
    if (mat.empty()) {
        return cv::Vec4f(0.f, 0.f, 0.f, 0.f);
    }

    const int clampedX = std::clamp(x, 0, mat.cols - 1);
    const int clampedY = std::clamp(y, 0, mat.rows - 1);
    return mat.at<cv::Vec4f>(clampedY, clampedX);
}

inline cv::Vec4f lerpPixel(const cv::Vec4f& a, const cv::Vec4f& b, const float t) {
    return a * (1.0f - t) + b * t;
}

} // namespace

class TimeDisplacementCPUImpl : public ArtifactEffectImplBase {
public:
    float maxOffsetFrames_ = 12.0f;
    TimeDisplaceChannel sourceChannel_ = TimeDisplaceChannel::Luminance;
    bool frameBlend_ = true;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const ImageF32x4_RGBA& srcImage = src.image();
        const float* srcData = srcImage.rgba32fData();
        if (!srcData || srcImage.width() <= 0 || srcImage.height() <= 0) {
            dst = src;
            return;
        }

        if (!context_.sampler) {
            dst = src.DeepCopy();
            return;
        }

        const int width = srcImage.width();
        const int height = srcImage.height();
        cv::Mat mapMat(height, width, CV_32FC4, const_cast<float*>(srcData));

        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();
        cv::Mat dstMat(height, width, CV_32FC4, dstData);

        std::unordered_map<std::int64_t, ImageF32x4RGBAWithCache> sampledFrames;

        auto fetchFrame = [&](std::int64_t frameOffset) -> const ImageF32x4RGBAWithCache* {
            auto it = sampledFrames.find(frameOffset);
            if (it == sampledFrames.end()) {
                ImageF32x4RGBAWithCache sampled;
                const std::int64_t targetFrame = context_.compositionFrame + frameOffset;
                bool ok = context_.sampler->sampleCurrentLayerFrame(targetFrame, sampled);
                if (!ok) {
                    ok = context_.sampler->sampleCurrentLayerFrameRelative(frameOffset, sampled);
                }
                if (!ok || sampled.width() <= 0 || sampled.height() <= 0 ||
                    !sampled.image().rgba32fData()) {
                    return nullptr;
                }
                it = sampledFrames.emplace(frameOffset, std::move(sampled)).first;
            }

            return &it->second;
        };

        for (int y = 0; y < height; ++y) {
            const float* mapRow = mapMat.ptr<float>(y);
            cv::Vec4f* outRow = dstMat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float* mapPixel = mapRow + x * 4;
                const float normalizedOffset =
                    (sampleChannel(mapPixel, sourceChannel_) - 0.5f) * 2.0f;
                const float frameOffsetFloat = normalizedOffset * maxOffsetFrames_;

                if (!frameBlend_) {
                    const auto frameOffset =
                        static_cast<std::int64_t>(std::llround(frameOffsetFloat));
                    if (const auto* sampled = fetchFrame(frameOffset)) {
                        const auto& sampledImage = sampled->image();
                        const cv::Mat sampledMat(sampledImage.height(), sampledImage.width(),
                                                 CV_32FC4,
                                                 const_cast<float*>(sampledImage.rgba32fData()));
                        outRow[x] = pixelAtClamped(sampledMat, x, y);
                    }
                    continue;
                }

                const float floorOffset = std::floor(frameOffsetFloat);
                const float ceilOffset = std::ceil(frameOffsetFloat);
                const auto lowerOffset = static_cast<std::int64_t>(floorOffset);
                const auto upperOffset = static_cast<std::int64_t>(ceilOffset);
                const float mix = std::clamp(frameOffsetFloat - floorOffset, 0.0f, 1.0f);

                const auto* lower = fetchFrame(lowerOffset);
                const auto* upper = fetchFrame(upperOffset);
                if (lower && upper) {
                    const auto& lowerImage = lower->image();
                    const auto& upperImage = upper->image();
                    const cv::Mat lowerMat(lowerImage.height(), lowerImage.width(), CV_32FC4,
                                           const_cast<float*>(lowerImage.rgba32fData()));
                    const cv::Mat upperMat(upperImage.height(), upperImage.width(), CV_32FC4,
                                           const_cast<float*>(upperImage.rgba32fData()));
                    outRow[x] = lerpPixel(pixelAtClamped(lowerMat, x, y),
                                          pixelAtClamped(upperMat, x, y),
                                          mix);
                } else if (lower) {
                    const auto& lowerImage = lower->image();
                    const cv::Mat lowerMat(lowerImage.height(), lowerImage.width(), CV_32FC4,
                                           const_cast<float*>(lowerImage.rgba32fData()));
                    outRow[x] = pixelAtClamped(lowerMat, x, y);
                } else if (upper) {
                    const auto& upperImage = upper->image();
                    const cv::Mat upperMat(upperImage.height(), upperImage.width(), CV_32FC4,
                                           const_cast<float*>(upperImage.rgba32fData()));
                    outRow[x] = pixelAtClamped(upperMat, x, y);
                }
            }
        }
    }
};

TimeDisplacementEffect::TimeDisplacementEffect() {
    setDisplayName(UniString("Time Displacement"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<TimeDisplacementCPUImpl>();
    setCPUImpl(cpu);
    syncImpls();
}

TimeDisplacementEffect::~TimeDisplacementEffect() = default;

float TimeDisplacementEffect::maxOffsetFrames() const { return maxOffsetFrames_; }

void TimeDisplacementEffect::setMaxOffsetFrames(float value) {
    maxOffsetFrames_ = std::max(0.0f, value);
    syncImpls();
}

TimeDisplaceChannel TimeDisplacementEffect::sourceChannel() const { return sourceChannel_; }

void TimeDisplacementEffect::setSourceChannel(TimeDisplaceChannel channel) {
    sourceChannel_ = channel;
    syncImpls();
}

bool TimeDisplacementEffect::frameBlend() const { return frameBlend_; }

void TimeDisplacementEffect::setFrameBlend(bool value) {
    frameBlend_ = value;
    syncImpls();
}

std::vector<AbstractProperty> TimeDisplacementEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(3);

    auto& offsetProp = props.emplace_back();
    offsetProp.setName("Max Offset Frames");
    offsetProp.setType(PropertyType::Float);
    offsetProp.setValue(maxOffsetFrames_);

    auto& channelProp = props.emplace_back();
    channelProp.setName("Source Channel");
    channelProp.setType(PropertyType::Integer);
    channelProp.setValue(static_cast<int>(sourceChannel_));

    auto& blendProp = props.emplace_back();
    blendProp.setName("Frame Blend");
    blendProp.setType(PropertyType::Boolean);
    blendProp.setValue(frameBlend_);

    return props;
}

void TimeDisplacementEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Max Offset Frames")) {
        setMaxOffsetFrames(value.toFloat());
    } else if (key == QStringLiteral("Source Channel")) {
        setSourceChannel(static_cast<TimeDisplaceChannel>(value.toInt()));
    } else if (key == QStringLiteral("Frame Blend")) {
        setFrameBlend(value.toBool());
    }
}

void TimeDisplacementEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<TimeDisplacementCPUImpl*>(cpuImpl().get())) {
        cpu->maxOffsetFrames_ = maxOffsetFrames_;
        cpu->sourceChannel_ = sourceChannel_;
        cpu->frameBlend_ = frameBlend_;
    }
}

} // namespace Artifact
