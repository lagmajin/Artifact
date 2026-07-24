module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <opencv2/opencv.hpp>

module Artifact.Effect.SurfaceFX;

import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;

namespace Artifact {

namespace {

class SurfaceFXCPUImpl final : public ArtifactEffectImplBase {
public:
    ArtifactCore::SurfaceFXData data;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) {
            dst = src;
            return;
        }

        cv::Mat input(height, width, CV_32FC4,
                      const_cast<float*>(pixels));
        cv::Mat overlay(height, width, CV_32FC4, cv::Scalar(0, 0, 0, 0));
        const float left = std::clamp(data.anchorX, 0.0f, 1.0f) * width;
        const float top = std::clamp(data.anchorY, 0.0f, 1.0f) * height;
        const float right = std::clamp(data.anchorX + data.anchorWidth, 0.0f, 1.0f) * width;
        const float bottom = std::clamp(data.anchorY + data.anchorHeight, 0.0f, 1.0f) * height;

        const float currentTime = static_cast<float>(context_.timeSeconds);
        for (const auto& element : data.elements) {
            if (currentTime < element.inTime ||
                (element.outTime >= 0.0f && currentTime > element.outTime))
                continue;
            const float alpha = std::clamp(element.opacity * element.intensity, 0.0f, 1.0f);
            if (alpha <= 0.0f) continue;
            const float x = std::clamp(element.x, data.anchorX, data.anchorX + data.anchorWidth) * width;
            float normalizedY = element.y;
            if (element.type == ArtifactCore::SurfaceFXElementType::Droplet ||
                element.type == ArtifactCore::SurfaceFXElementType::Streak) {
                const auto seed = static_cast<std::uint32_t>(data.fieldSeed) ^
                                  static_cast<std::uint32_t>(element.seedOffset * 0x9e3779b9u);
                const float speed = 0.04f + static_cast<float>(seed % 7u) * 0.01f;
                const float anchorHeight = std::max(0.001f, data.anchorHeight);
                const float localY = (element.y - data.anchorY) +
                                     std::max(0.0f, currentTime - element.inTime) * speed;
                normalizedY = data.anchorY + std::fmod(localY, anchorHeight);
            }
            const float y = std::clamp(normalizedY, data.anchorY,
                                       data.anchorY + data.anchorHeight) * height;
            const float w = std::max(1.0f, element.width * width);
            const float h = std::max(1.0f, element.height * height);
            const cv::Point center(static_cast<int>(std::round(x + w * 0.5f)),
                                   static_cast<int>(std::round(y + h * 0.5f)));
            const cv::Scalar color = element.type == ArtifactCore::SurfaceFXElementType::Dirt
                ? cv::Scalar(0.12f, 0.08f, 0.05f, alpha)
                : cv::Scalar(0.82f, 0.9f, 1.0f, alpha);
            const int thickness = std::max(1, static_cast<int>(std::round(1.0f + element.roughness * 2.0f)));
            if (element.type == ArtifactCore::SurfaceFXElementType::Droplet ||
                element.type == ArtifactCore::SurfaceFXElementType::Dirt ||
                element.type == ArtifactCore::SurfaceFXElementType::Condensation) {
                cv::ellipse(overlay, center,
                            cv::Size(std::max(1, static_cast<int>(w * 0.5f)),
                                     std::max(1, static_cast<int>(h * 0.5f))),
                            element.rotation, 0.0, 360.0, color, -1, cv::LINE_AA);
            } else {
                const double radians = element.rotation * 3.141592653589793 / 180.0;
                const cv::Point delta(static_cast<int>(std::round(std::cos(radians) * w)),
                                      static_cast<int>(std::round(std::sin(radians) * w)));
                cv::line(overlay, center - delta / 2, center + delta / 2,
                         color, thickness, cv::LINE_AA);
            }
        }

        cv::Mat output = input.clone();
        for (int y = std::max(0, static_cast<int>(top)); y < std::min(height, static_cast<int>(bottom)); ++y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const auto* overlayRow = overlay.ptr<cv::Vec4f>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = std::max(0, static_cast<int>(left)); x < std::min(width, static_cast<int>(right)); ++x) {
                const float a = std::clamp(overlayRow[x][3], 0.0f, 1.0f);
                for (int c = 0; c < 3; ++c)
                    outputRow[x][c] = sourceRow[x][c] * (1.0f - a) + overlayRow[x][c] * a;
                outputRow[x][3] = sourceRow[x][3];
            }
        }
        dst = src;
        dst.image().setFromCVMat(output, image.colorDescriptor());
    }
};

}

SurfaceFXEffect::SurfaceFXEffect() {
    setEffectID(UniString(QStringLiteral("surfacefx")));
    setDisplayName(UniString(QStringLiteral("SurfaceFX")));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SurfaceFXCPUImpl>());
    syncImpl();
}

void SurfaceFXEffect::syncImpl() {
    if (auto* impl = dynamic_cast<SurfaceFXCPUImpl*>(cpuImpl().get()))
        impl->data = data_;
}

}
