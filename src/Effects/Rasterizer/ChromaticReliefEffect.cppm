module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.ChromaticRelief;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact {

using namespace ArtifactCore;

class ChromaticReliefCPUImpl final : public ArtifactEffectImplBase {
public:
    float reliefAmount = 0.7f;
    float chromaticOffset = 4.0f;
    float direction = 45.0f;
    float edgeSoftness = 1.2f;
    float mix = 0.8f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }
        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat rgb, gray, gx, gy;
        cv::cvtColor(input, rgb, cv::COLOR_RGBA2RGB);
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        if (edgeSoftness > 0.0f)
            cv::GaussianBlur(gray, gray, cv::Size(), edgeSoftness, edgeSoftness);
        cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gy, CV_32F, 0, 1, 3);

        const float angle = direction * 0.0174532925f;
        const float dirX = std::cos(angle);
        const float dirY = std::sin(angle);
        const int offsetX = static_cast<int>(std::round(dirX * chromaticOffset));
        const int offsetY = static_cast<int>(std::round(dirY * chromaticOffset));
        cv::Mat output = input.clone();
        ArtifactCore::Parallel::For(0, height, [&](int y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const float* gxRow = gx.ptr<float>(y);
            const float* gyRow = gy.ptr<float>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const int redX = std::clamp(x + offsetX, 0, width - 1);
                const int redY = std::clamp(y + offsetY, 0, height - 1);
                const int blueX = std::clamp(x - offsetX, 0, width - 1);
                const int blueY = std::clamp(y - offsetY, 0, height - 1);
                const float relief = std::clamp(
                    0.5f + (gxRow[x] * dirX + gyRow[x] * dirY) * reliefAmount,
                    0.0f, 1.5f);
                cv::Vec4f styled = sourceRow[x];
                styled[0] = input.at<cv::Vec4f>(redY, redX)[0] * (0.65f + relief * 0.7f);
                styled[1] = sourceRow[x][1] * (0.72f + relief * 0.56f);
                styled[2] = input.at<cv::Vec4f>(blueY, blueX)[2] * (0.8f + (1.0f - relief) * 0.45f);
                for (int c = 0; c < 3; ++c)
                    outputRow[x][c] = sourceRow[x][c] * (1.0f - mix) + styled[c] * mix;
                outputRow[x][3] = sourceRow[x][3];
            }
        });
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

ChromaticReliefEffect::ChromaticReliefEffect() {
    setDisplayName(UniString("Chromatic Relief"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<ChromaticReliefCPUImpl>());
    syncImpl();
}

ChromaticReliefEffect::~ChromaticReliefEffect() = default;

void ChromaticReliefEffect::syncImpl() {
    if (auto* impl = dynamic_cast<ChromaticReliefCPUImpl*>(cpuImpl().get())) {
        impl->reliefAmount = reliefAmount_; impl->chromaticOffset = chromaticOffset_;
        impl->direction = direction_; impl->edgeSoftness = edgeSoftness_; impl->mix = mix_;
    }
}

std::vector<AbstractProperty> ChromaticReliefEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& relief = props.emplace_back(); relief.setName("Relief Amount"); relief.setType(PropertyType::Float); relief.setValue(reliefAmount_);
    auto& chroma = props.emplace_back(); chroma.setName("Chromatic Offset"); chroma.setType(PropertyType::Float); chroma.setValue(chromaticOffset_);
    auto& direction = props.emplace_back(); direction.setName("Direction"); direction.setType(PropertyType::Float); direction.setValue(direction_);
    auto& softness = props.emplace_back(); softness.setName("Edge Softness"); softness.setType(PropertyType::Float); softness.setValue(edgeSoftness_);
    auto& mix = props.emplace_back(); mix.setName("Mix"); mix.setType(PropertyType::Float); mix.setValue(mix_);
    return props;
}

void ChromaticReliefEffect::setPropertyValue(const UniString& name,
                                             const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Relief Amount")) reliefAmount_ = std::clamp(value.toFloat(), 0.0f, 3.0f);
    else if (key == QStringLiteral("Chromatic Offset")) chromaticOffset_ = std::clamp(value.toFloat(), 0.0f, 40.0f);
    else if (key == QStringLiteral("Direction")) direction_ = value.toFloat();
    else if (key == QStringLiteral("Edge Softness")) edgeSoftness_ = std::clamp(value.toFloat(), 0.0f, 10.0f);
    else if (key == QStringLiteral("Mix")) mix_ = std::clamp(value.toFloat(), 0.0f, 1.0f);
    syncImpl();
}

}
