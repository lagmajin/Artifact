module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ReactionDiffusionBlur;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

class ReactionDiffusionBlurCPUImpl final : public ArtifactEffectImplBase {
public:
    float blurRadius = 8.0f;
    float feed = 0.055f;
    float kill = 0.062f;
    float patternStrength = 0.65f;
    int iterations = 18;
    float evolution = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat blurred;
        const double sigma = std::max(0.1f, blurRadius * 0.35f);
        cv::GaussianBlur(input, blurred, cv::Size(), sigma, sigma,
                         cv::BORDER_REFLECT101);

        cv::Mat rgb, gray;
        cv::cvtColor(input, rgb, cv::COLOR_RGBA2RGB);
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        const int gridWidth = std::max(16, width / 4);
        const int gridHeight = std::max(16, height / 4);
        cv::Mat seed;
        cv::resize(gray, seed, cv::Size(gridWidth, gridHeight), 0, 0,
                   cv::INTER_AREA);

        cv::Mat u(gridHeight, gridWidth, CV_32F, cv::Scalar(1.0f));
        cv::Mat v = (seed > 0.45f);
        v.convertTo(v, CV_32F, 0.8 / 255.0);
        const float phase = evolution * 0.0174532925f;
        for (int y = 0; y < gridHeight; ++y) {
            float* row = v.ptr<float>(y);
            for (int x = 0; x < gridWidth; ++x) {
                const float organicSeed = 0.08f *
                    (std::sin(x * 0.173f + phase) +
                     std::cos(y * 0.137f - phase));
                row[x] = std::clamp(row[x] + organicSeed, 0.0f, 1.0f);
            }
        }

        cv::Mat lapU, lapV, uvv, nextU, nextV;
        for (int i = 0; i < iterations; ++i) {
            cv::Laplacian(u, lapU, CV_32F, 3, 1.0, 0.0, cv::BORDER_REFLECT101);
            cv::Laplacian(v, lapV, CV_32F, 3, 1.0, 0.0, cv::BORDER_REFLECT101);
            uvv = u.mul(v.mul(v));
            nextU = u + 0.16f * (0.16f * lapU - uvv + feed * (1.0f - u));
            nextV = v + 0.16f * (0.08f * lapV + uvv - (feed + kill) * v);
            cv::max(nextU, 0.0, nextU); cv::min(nextU, 1.0, nextU);
            cv::max(nextV, 0.0, nextV); cv::min(nextV, 1.0, nextV);
            u = nextU; v = nextV;
        }

        cv::Mat pattern;
        cv::resize(v, pattern, cv::Size(width, height), 0, 0,
                   cv::INTER_CUBIC);
        cv::GaussianBlur(pattern, pattern, cv::Size(), 0.8, 0.8);
        cv::Mat output = blurred.clone();
        for (int y = 0; y < height; ++y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const auto* blurRow = blurred.ptr<cv::Vec4f>(y);
            const float* patternRow = pattern.ptr<float>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float cellular = std::clamp(patternRow[x] * 1.8f, 0.0f, 1.0f);
                const float mix = std::clamp(patternStrength * cellular, 0.0f, 1.0f);
                for (int c = 0; c < 3; ++c) {
                    const float organic = blurRow[x][c] * (0.82f + cellular * 0.36f);
                    outputRow[x][c] = sourceRow[x][c] * (1.0f - mix) + organic * mix;
                }
                outputRow[x][3] = sourceRow[x][3];
            }
        }
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

ReactionDiffusionBlurEffect::ReactionDiffusionBlurEffect() {
    setDisplayName(UniString("Reaction Diffusion Blur"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ReactionDiffusionBlurCPUImpl>());
    syncImpl();
}

ReactionDiffusionBlurEffect::~ReactionDiffusionBlurEffect() = default;

void ReactionDiffusionBlurEffect::syncImpl() {
    if (auto* impl = dynamic_cast<ReactionDiffusionBlurCPUImpl*>(cpuImpl().get())) {
        impl->blurRadius = blurRadius_; impl->feed = feed_; impl->kill = kill_;
        impl->patternStrength = patternStrength_; impl->iterations = iterations_;
        impl->evolution = evolution_;
    }
}

std::vector<AbstractProperty> ReactionDiffusionBlurEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& blur = props.emplace_back(); blur.setName("Blur Radius"); blur.setType(PropertyType::Float); blur.setValue(blurRadius_);
    auto& feed = props.emplace_back(); feed.setName("Feed"); feed.setType(PropertyType::Float); feed.setValue(feed_);
    auto& kill = props.emplace_back(); kill.setName("Kill"); kill.setType(PropertyType::Float); kill.setValue(kill_);
    auto& strength = props.emplace_back(); strength.setName("Pattern Strength"); strength.setType(PropertyType::Float); strength.setValue(patternStrength_);
    auto& iterations = props.emplace_back(); iterations.setName("Iterations"); iterations.setType(PropertyType::Integer); iterations.setValue(iterations_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    return props;
}

void ReactionDiffusionBlurEffect::setPropertyValue(const UniString& name,
                                                    const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Blur Radius")) blurRadius_ = std::clamp(value.toFloat(), 0.0f, 80.0f);
    else if (key == QStringLiteral("Feed")) feed_ = std::clamp(value.toFloat(), 0.01f, 0.1f);
    else if (key == QStringLiteral("Kill")) kill_ = std::clamp(value.toFloat(), 0.01f, 0.1f);
    else if (key == QStringLiteral("Pattern Strength")) patternStrength_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Iterations")) iterations_ = std::clamp(value.toInt(), 1, 96);
    else if (key == QStringLiteral("Evolution")) evolution_ = value.toFloat();
    syncImpl();
}

}
