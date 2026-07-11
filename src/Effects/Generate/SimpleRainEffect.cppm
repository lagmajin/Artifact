module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Generate.SimpleRain;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

namespace {

std::uint32_t rainHash(std::uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float unitHash(std::uint32_t value) {
    return static_cast<float>(rainHash(value) & 0x00ffffffu) / 16777215.0f;
}

}

class SimpleRainCPUImpl final : public ArtifactEffectImplBase {
public:
    float density = 0.45f;
    float streakLength = 24.0f;
    float speed = 1.0f;
    float wind = -0.2f;
    float opacity = 0.35f;
    float depth = 0.65f;
    float splashAmount = 0.12f;
    float evolution = 0.0f;
    int seed = 1337;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat rain(height, width, CV_32FC4, cv::Scalar(0, 0, 0, 0));
        const int drops = std::clamp(
            static_cast<int>((width * height / 2600.0f) * density), 0, 12000);
        const float travel = evolution * speed;
        for (int i = 0; i < drops; ++i) {
            const std::uint32_t base = static_cast<std::uint32_t>(seed) ^
                                       static_cast<std::uint32_t>(i * 0x9e3779b9u);
            const float layer = unitHash(base + 1u);
            const float depthScale = 0.35f + layer * (0.65f + depth);
            const float x0 = unitHash(base + 2u) * (width + 80.0f) - 40.0f;
            const float y0 = unitHash(base + 3u) * (height + 120.0f) - 60.0f;
            const float phase = std::fmod(y0 + travel * (40.0f + 95.0f * depthScale),
                                          height + 120.0f) - 60.0f;
            const float length = streakLength * depthScale *
                                 (0.65f + unitHash(base + 4u) * 0.7f);
            const float dx = wind * length;
            const cv::Point start(static_cast<int>(std::round(x0 + wind * travel * 12.0f)),
                                  static_cast<int>(std::round(phase)));
            const cv::Point end(static_cast<int>(std::round(start.x + dx)),
                                static_cast<int>(std::round(start.y + length)));
            const float alpha = opacity * (0.25f + 0.75f * depthScale);
            const int thickness = std::clamp(static_cast<int>(std::round(depthScale)), 1, 3);
            cv::line(rain, start, end, cv::Scalar(0.72f, 0.82f, 0.92f, alpha),
                     thickness, cv::LINE_AA);

            if (splashAmount > 0.0f && end.y >= height - 4.0f &&
                unitHash(base + 5u) < splashAmount) {
                const int radius = std::max(1, static_cast<int>(2.5f * depthScale));
                cv::ellipse(rain, cv::Point(static_cast<int>(end.x), height - 2),
                            cv::Size(radius * 2, radius), 0.0, 195.0, 345.0,
                            cv::Scalar(0.7f, 0.82f, 0.95f, alpha * 0.7f),
                            1, cv::LINE_AA);
            }
        }

        cv::Mat output = input.clone();
        for (int y = 0; y < height; ++y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const auto* rainRow = rain.ptr<cv::Vec4f>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float alpha = std::clamp(rainRow[x][3], 0.0f, 1.0f);
                for (int c = 0; c < 3; ++c) {
                    outputRow[x][c] = sourceRow[x][c] * (1.0f - alpha) +
                                      rainRow[x][c] * alpha;
                }
                outputRow[x][3] = sourceRow[x][3];
            }
        }
        dst = src;
        dst.image().setFromCVMat(output);
    }
};

SimpleRainEffect::SimpleRainEffect() {
    setDisplayName(UniString("Simple Rain"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SimpleRainCPUImpl>());
    syncImpl();
}

SimpleRainEffect::~SimpleRainEffect() = default;

void SimpleRainEffect::syncImpl() {
    if (auto* impl = dynamic_cast<SimpleRainCPUImpl*>(cpuImpl().get())) {
        impl->density = density_; impl->streakLength = streakLength_;
        impl->speed = speed_; impl->wind = wind_; impl->opacity = opacity_;
        impl->depth = depth_; impl->splashAmount = splashAmount_;
        impl->evolution = evolution_; impl->seed = seed_;
    }
}

std::vector<AbstractProperty> SimpleRainEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& density = props.emplace_back(); density.setName("Density"); density.setType(PropertyType::Float); density.setValue(density_);
    auto& length = props.emplace_back(); length.setName("Streak Length"); length.setType(PropertyType::Float); length.setValue(streakLength_);
    auto& speed = props.emplace_back(); speed.setName("Speed"); speed.setType(PropertyType::Float); speed.setValue(speed_);
    auto& wind = props.emplace_back(); wind.setName("Wind"); wind.setType(PropertyType::Float); wind.setValue(wind_);
    auto& opacity = props.emplace_back(); opacity.setName("Opacity"); opacity.setType(PropertyType::Float); opacity.setValue(opacity_);
    auto& depth = props.emplace_back(); depth.setName("Depth"); depth.setType(PropertyType::Float); depth.setValue(depth_);
    auto& splash = props.emplace_back(); splash.setName("Splash Amount"); splash.setType(PropertyType::Float); splash.setValue(splashAmount_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    auto& seed = props.emplace_back(); seed.setName("Seed"); seed.setType(PropertyType::Integer); seed.setValue(seed_);
    return props;
}

void SimpleRainEffect::setPropertyValue(const UniString& name,
                                        const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Density")) density_ = std::clamp(value.toFloat(), 0.0f, 4.0f);
    else if (key == QStringLiteral("Streak Length")) streakLength_ = std::clamp(value.toFloat(), 1.0f, 200.0f);
    else if (key == QStringLiteral("Speed")) speed_ = std::clamp(value.toFloat(), 0.0f, 10.0f);
    else if (key == QStringLiteral("Wind")) wind_ = std::clamp(value.toFloat(), -3.0f, 3.0f);
    else if (key == QStringLiteral("Opacity")) opacity_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Depth")) depth_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Splash Amount")) splashAmount_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Evolution")) evolution_ = value.toFloat();
    else if (key == QStringLiteral("Seed")) seed_ = value.toInt();
    syncImpl();
}

}
