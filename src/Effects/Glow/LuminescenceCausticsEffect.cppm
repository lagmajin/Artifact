module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Glow.LuminescenceCaustics;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

class LuminescenceCausticsCPUImpl final : public ArtifactEffectImplBase {
public:
    float threshold = 0.55f;
    float edgeWeight = 0.8f;
    float scale = 22.0f;
    float intensity = 0.75f;
    float evolution = 0.0f;
    float colorShift = 0.35f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat rgb, gray, gradX, gradY, edge;
        cv::cvtColor(input, rgb, cv::COLOR_RGBA2RGB);
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
        cv::magnitude(gradX, gradY, edge);
        cv::GaussianBlur(edge, edge, cv::Size(), 1.2, 1.2);

        cv::Mat output = input.clone();
        const float phase = evolution * 0.0174532925f;
        const float invScale = 1.0f / std::max(scale, 1.0f);
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const float* grayRow = gray.ptr<float>(y);
            const float* edgeRow = edge.ptr<float>(y);
            const float* gxRow = gradX.ptr<float>(y);
            const float* gyRow = gradY.ptr<float>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float highlight = std::clamp(
                    (grayRow[x] - threshold) / std::max(0.001f, 1.0f - threshold),
                    0.0f, 1.0f);
                const float direction = std::atan2(gyRow[x], gxRow[x]);
                const float px = x * invScale;
                const float py = y * invScale;
                const float interference = std::abs(
                    std::sin(px * 1.73f + py * 1.17f + phase + direction) +
                    std::sin(px * -1.11f + py * 2.03f - phase * 0.73f));
                const float ridge = std::pow(std::clamp(interference * 0.5f, 0.0f, 1.0f), 5.0f);
                const float sourceMask = std::clamp(highlight + edgeRow[x] * edgeWeight, 0.0f, 1.0f);
                const float caustic = ridge * sourceMask * intensity * sourceRow[x][3];
                const float warm = 1.0f - colorShift * 0.25f;
                outputRow[x][0] = sourceRow[x][0] + caustic * warm;
                outputRow[x][1] = sourceRow[x][1] + caustic;
                outputRow[x][2] = sourceRow[x][2] + caustic * (1.0f + colorShift * 0.65f);
                outputRow[x][3] = sourceRow[x][3];
            }
        });
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

LuminescenceCausticsEffect::LuminescenceCausticsEffect() {
    setDisplayName(UniString("Luminescence Caustics"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<LuminescenceCausticsCPUImpl>());
    syncImpl();
}

LuminescenceCausticsEffect::~LuminescenceCausticsEffect() = default;

void LuminescenceCausticsEffect::syncImpl() {
    if (auto* impl = dynamic_cast<LuminescenceCausticsCPUImpl*>(cpuImpl().get())) {
        impl->threshold = threshold_; impl->edgeWeight = edgeWeight_;
        impl->scale = scale_; impl->intensity = intensity_;
        impl->evolution = evolution_; impl->colorShift = colorShift_;
    }
}

std::vector<AbstractProperty> LuminescenceCausticsEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& threshold = props.emplace_back(); threshold.setName("Threshold"); threshold.setType(PropertyType::Float); threshold.setValue(threshold_);
    auto& edge = props.emplace_back(); edge.setName("Edge Weight"); edge.setType(PropertyType::Float); edge.setValue(edgeWeight_);
    auto& scale = props.emplace_back(); scale.setName("Scale"); scale.setType(PropertyType::Float); scale.setValue(scale_);
    auto& intensity = props.emplace_back(); intensity.setName("Intensity"); intensity.setType(PropertyType::Float); intensity.setValue(intensity_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    auto& color = props.emplace_back(); color.setName("Color Shift"); color.setType(PropertyType::Float); color.setValue(colorShift_);
    return props;
}

void LuminescenceCausticsEffect::setPropertyValue(const UniString& name,
                                                  const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) threshold_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    else if (key == QStringLiteral("Edge Weight")) edgeWeight_ = std::clamp(value.toFloat(), 0.0f, 5.0f);
    else if (key == QStringLiteral("Scale")) scale_ = std::clamp(value.toFloat(), 2.0f, 200.0f);
    else if (key == QStringLiteral("Intensity")) intensity_ = std::clamp(value.toFloat(), 0.0f, 5.0f);
    else if (key == QStringLiteral("Evolution")) evolution_ = value.toFloat();
    else if (key == QStringLiteral("Color Shift")) colorShift_ = std::clamp(value.toFloat(), -1.0f, 1.0f);
    syncImpl();
}

}
