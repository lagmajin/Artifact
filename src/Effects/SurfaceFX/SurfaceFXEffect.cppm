module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <opencv2/opencv.hpp>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>

module Artifact.Effect.SurfaceFX;

import Artifact.Effect.ImplBase;
import Core.Parallel;
import Image.ImageF32x4RGBAWithCache;
import Memory.SharedPtr;
import Asset.Database;

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
        cv::Mat dropletMask(height, width, CV_32FC1, cv::Scalar(0));
        cv::Mat blendModeMask(height, width, CV_8UC1, cv::Scalar(0));
        const float left = std::clamp(data.anchorX, 0.0f, 1.0f) * width;
        const float top = std::clamp(data.anchorY, 0.0f, 1.0f) * height;
        const float right = std::clamp(data.anchorX + data.anchorWidth, 0.0f, 1.0f) * width;
        const float bottom = std::clamp(data.anchorY + data.anchorHeight, 0.0f, 1.0f) * height;

        const float currentTime = static_cast<float>(context_.timeSeconds);
        for (const auto& element : data.elements) {
            if (currentTime < element.inTime ||
                (element.outTime >= 0.0f && currentTime > element.outTime))
                continue;
            const float elapsed = std::max(0.0f, currentTime - element.inTime);
            float timingOpacity = 1.0f;
            if (element.fadeIn > 0.0f)
                timingOpacity *= std::clamp(elapsed / element.fadeIn, 0.0f, 1.0f);
            if (element.fadeOut > 0.0f && element.outTime >= 0.0f)
                timingOpacity *= std::clamp(
                    (element.outTime - currentTime) / element.fadeOut, 0.0f, 1.0f);
            const float alpha = std::clamp(element.opacity * element.intensity *
                                               element.tintA * timingOpacity,
                                           0.0f, 1.0f);
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
            const float growthScale = std::max(
                0.05f, 1.0f + element.growth * std::min(elapsed, 1.0f));
            const float w = std::max(1.0f, element.width * width * growthScale);
            const float h = std::max(1.0f, element.height * height * growthScale);
            const float pivotX = element.type == ArtifactCore::SurfaceFXElementType::TextureDecal
                ? element.pivotX : 0.5f;
            const float pivotY = element.type == ArtifactCore::SurfaceFXElementType::TextureDecal
                ? element.pivotY : 0.5f;
            const cv::Point center(static_cast<int>(std::round(x + w * pivotX)),
                                   static_cast<int>(std::round(y + h * pivotY)));
            const cv::Scalar color = element.type == ArtifactCore::SurfaceFXElementType::Dirt
                ? cv::Scalar(0.12f, 0.08f, 0.05f, alpha)
                : element.type == ArtifactCore::SurfaceFXElementType::TextureDecal
                    ? cv::Scalar(element.tintR, element.tintG, element.tintB, alpha)
                    : cv::Scalar(0.82f, 0.9f, 1.0f, alpha);
            const int thickness = std::max(1, static_cast<int>(std::round(1.0f + element.roughness * 2.0f)));
            const std::uint8_t blendMode = element.blendMode == QStringLiteral("multiply") ? 1u
                : element.blendMode == QStringLiteral("screen") ? 2u
                : element.blendMode == QStringLiteral("add") ? 3u
                : element.blendMode == QStringLiteral("darken") ? 4u
                : 0u;
            if (element.type == ArtifactCore::SurfaceFXElementType::TextureDecal &&
                (!element.texturePath.trimmed().isEmpty() || !element.assetId.isNull())) {
                QString resolvedTexturePath = element.texturePath;
                if (!element.assetId.isNull()) {
                    const auto assetInfo = ArtifactCore::AssetDatabase::instance().getAssetInfo(
                        element.assetId);
                    if (!assetInfo.absolutePath.isEmpty())
                        resolvedTexturePath = assetInfo.absolutePath;
                }
                const cv::Mat texture = textureForPath(resolvedTexturePath);
                if (!texture.empty()) {
                    const float radians = element.rotation *
                                          3.14159265358979323846f / 180.0f;
                    const float cosAngle = std::cos(radians);
                    const float sinAngle = std::sin(radians);
                    const int extent = static_cast<int>(std::ceil(
                        std::sqrt(w * w + h * h)));
                    const int minX = std::max(0, center.x - extent);
                    const int maxX = std::min(width - 1, center.x + extent);
                    const int minY = std::max(0, center.y - extent);
                    const int maxY = std::min(height - 1, center.y + extent);
                    for (int dstY = minY; dstY <= maxY; ++dstY) {
                        auto* overlayRow = overlay.ptr<cv::Vec4f>(dstY);
                        auto* modeRow = blendModeMask.ptr<std::uint8_t>(dstY);
                        for (int dstX = minX; dstX <= maxX; ++dstX) {
                            const float dx = static_cast<float>(dstX - center.x);
                            const float dy = static_cast<float>(dstY - center.y);
                            const float localX = cosAngle * dx + sinAngle * dy;
                            const float localY = -sinAngle * dx + cosAngle * dy;
                            const float u = localX / w + pivotX;
                            const float v = localY / h + pivotY;
                            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
                                continue;
                            const int sampleX = std::clamp(
                                static_cast<int>(std::round(u * (texture.cols - 1))),
                                0, texture.cols - 1);
                            const int sampleY = std::clamp(
                                static_cast<int>(std::round(v * (texture.rows - 1))),
                                0, texture.rows - 1);
                            const cv::Vec4f sample = texture.at<cv::Vec4f>(sampleY, sampleX);
                            const float sampleAlpha = std::clamp(sample[3] * alpha, 0.0f, 1.0f);
                            if (sampleAlpha <= 0.0f)
                                continue;
                            overlayRow[dstX] = cv::Vec4f(
                                sample[0] * element.tintR,
                                sample[1] * element.tintG,
                                sample[2] * element.tintB,
                                sampleAlpha);
                            modeRow[dstX] = blendMode;
                        }
                    }
                }
            }
            if (element.type == ArtifactCore::SurfaceFXElementType::TextureDecal &&
                (!element.texturePath.trimmed().isEmpty() || !element.assetId.isNull()))
                continue;
            if (element.type == ArtifactCore::SurfaceFXElementType::Droplet ||
                element.type == ArtifactCore::SurfaceFXElementType::Dirt ||
                element.type == ArtifactCore::SurfaceFXElementType::Condensation ||
                element.type == ArtifactCore::SurfaceFXElementType::TextureDecal) {
                const cv::Size radius(std::max(1, static_cast<int>(w * 0.5f)),
                                      std::max(1, static_cast<int>(h * 0.5f)));
                cv::ellipse(overlay, center, radius, element.rotation, 0.0, 360.0,
                            color, -1, cv::LINE_AA);
                cv::ellipse(blendModeMask, center, radius, element.rotation, 0.0, 360.0,
                            cv::Scalar(blendMode), -1, cv::LINE_8);
                if (element.type == ArtifactCore::SurfaceFXElementType::TextureDecal) {
                    const auto seed = static_cast<std::uint32_t>(data.fieldSeed) ^
                                      static_cast<std::uint32_t>(element.seedOffset * 0x9e3779b9u);
                    const int satelliteCount = 3 + static_cast<int>(seed % 6u);
                    for (int satellite = 0; satellite < satelliteCount; ++satellite) {
                        const float angle = static_cast<float>((seed + satellite * 137u) % 360u) *
                                            3.14159265358979323846f / 180.0f;
                        const float distance = 0.55f + 0.12f * static_cast<float>(satellite % 4);
                        const cv::Point satelliteCenter(
                            center.x + static_cast<int>(std::cos(angle) * radius.width * distance),
                            center.y + static_cast<int>(std::sin(angle) * radius.height * distance));
                        const int satelliteRadius = std::max(
                            1, std::min(radius.width, radius.height) *
                                   (12 + static_cast<int>((seed >> (satellite % 8)) & 7u)) / 100);
                        cv::circle(overlay, satelliteCenter, satelliteRadius, color,
                                   -1, cv::LINE_AA);
                        cv::circle(blendModeMask, satelliteCenter, satelliteRadius,
                                   cv::Scalar(blendMode), -1, cv::LINE_8);
                    }
                }
                if (element.type == ArtifactCore::SurfaceFXElementType::Droplet) {
                    const cv::Scalar rim(0.08f, 0.12f, 0.18f, alpha * 0.55f);
                    cv::ellipse(overlay, center, radius, element.rotation, 0.0, 360.0,
                                rim, thickness, cv::LINE_AA);
                    const cv::Point highlight(
                        center.x - std::max(1, radius.width / 3),
                        center.y - std::max(1, radius.height / 3));
                    cv::ellipse(overlay, highlight,
                                cv::Size(std::max(1, radius.width / 4),
                                         std::max(1, radius.height / 4)),
                                element.rotation, 0.0, 360.0,
                                cv::Scalar(1.0f, 1.0f, 1.0f, alpha * 0.65f),
                                -1, cv::LINE_AA);
                    cv::ellipse(dropletMask, center, radius, element.rotation, 0.0, 360.0,
                                cv::Scalar(alpha), -1, cv::LINE_AA);
                }
            } else {
                const double radians = element.rotation * 3.141592653589793 / 180.0;
                const cv::Point delta(static_cast<int>(std::round(std::cos(radians) * w)),
                                      static_cast<int>(std::round(std::sin(radians) * w)));
                cv::line(overlay, center - delta / 2, center + delta / 2,
                         color, thickness, cv::LINE_AA);
                cv::line(blendModeMask, center - delta / 2, center + delta / 2,
                         cv::Scalar(blendMode), thickness, cv::LINE_8);
            }
        }

        cv::Mat output = input.clone();
        const int yBegin = std::max(0, static_cast<int>(top));
        const int yEnd = std::min(height, static_cast<int>(bottom));
        Parallel::For(yBegin, yEnd, (yEnd - yBegin) * width, [&](int y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const auto* overlayRow = overlay.ptr<cv::Vec4f>(y);
            const auto* dropletMaskRow = dropletMask.ptr<float>(y);
            const auto* blendModeRow = blendModeMask.ptr<std::uint8_t>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = std::max(0, static_cast<int>(left)); x < std::min(width, static_cast<int>(right)); ++x) {
                float featherMask = 1.0f;
                if (data.feather > 0.0f) {
                    const float u = (static_cast<float>(x) / width - data.anchorX) /
                                    std::max(0.001f, data.anchorWidth);
                    const float v = (static_cast<float>(y) / height - data.anchorY) /
                                    std::max(0.001f, data.anchorHeight);
                    const float edgeDistance = std::min({u, v, 1.0f - u, 1.0f - v});
                    featherMask = std::clamp(edgeDistance / data.feather, 0.0f, 1.0f);
                    featherMask = featherMask * featherMask * (3.0f - 2.0f * featherMask);
                }
                const float a = std::clamp(overlayRow[x][3] * featherMask, 0.0f, 1.0f);
                const float dropletAlpha = std::clamp(
                    dropletMaskRow[x] * featherMask, 0.0f, 1.0f);
                const int refractionOffset = dropletAlpha > 0.0f
                    ? std::clamp(static_cast<int>(std::round((dropletAlpha - 0.5f) * 4.0f)), -2, 2)
                    : 0;
                const int sampleX = std::clamp(x + refractionOffset, 0, width - 1);
                const auto* refractedSource = sourceRow + sampleX;
                for (int c = 0; c < 3; ++c) {
                    const float source = refractedSource[0][c];
                    const float decal = overlayRow[x][c];
                    float blended = decal;
                    switch (blendModeRow[x]) {
                    case 1u:
                        blended = source * decal;
                        break;
                    case 2u:
                        blended = 1.0f - (1.0f - source) * (1.0f - decal);
                        break;
                    case 3u:
                        blended = source + decal;
                        break;
                    case 4u:
                        blended = std::min(source, decal);
                        break;
                    default:
                        break;
                    }
                    outputRow[x][c] = std::clamp(source * (1.0f - a) + blended * a,
                                                 0.0f, 1.0f);
                }
                outputRow[x][3] = sourceRow[x][3];
            }
        });
        dst = src;
        dst.image().setFromCVMat(output, image.colorDescriptor());
    }

private:
    struct CachedTexture {
        qint64 modifiedAt = -1;
        cv::Mat rgba32f;
    };

    cv::Mat textureForPath(const QString& rawPath) {
        const QFileInfo info(rawPath.trimmed());
        if (!info.exists() || !info.isFile())
            return {};
        const QString path = info.absoluteFilePath();
        const qint64 modifiedAt = info.lastModified().toMSecsSinceEpoch();
        const auto cached = textureCache_.constFind(path);
        if (cached != textureCache_.cend() && cached->modifiedAt == modifiedAt)
            return cached->rgba32f;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        const QByteArray bytes = file.readAll();
        if (bytes.isEmpty() || bytes.size() > 64 * 1024 * 1024)
            return {};
        const cv::Mat encoded(1, static_cast<int>(bytes.size()), CV_8UC1,
                              const_cast<char*>(bytes.constData()));
        const cv::Mat decoded = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
        if (decoded.empty())
            return {};

        cv::Mat rgba;
        if (decoded.channels() == 4)
            cv::cvtColor(decoded, rgba, cv::COLOR_BGRA2RGBA);
        else if (decoded.channels() == 3)
            cv::cvtColor(decoded, rgba, cv::COLOR_BGR2RGBA);
        else if (decoded.channels() == 1)
            cv::cvtColor(decoded, rgba, cv::COLOR_GRAY2RGBA);
        else
            return {};
        rgba.convertTo(rgba, CV_32FC4, 1.0 / 255.0);
        textureCache_.insert(path, CachedTexture{modifiedAt, rgba});
        return rgba;
    }

    QHash<QString, CachedTexture> textureCache_;
};

}

SurfaceFXEffect::SurfaceFXEffect() {
    setEffectID(UniString(QStringLiteral("surfacefx")));
    setDisplayName(UniString(QStringLiteral("Lens Surface")));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<SurfaceFXCPUImpl>());
    setComputeMode(ComputeMode::CPU);
    syncImpl();
}

void SurfaceFXEffect::syncImpl() {
    if (auto* impl = dynamic_cast<SurfaceFXCPUImpl*>(cpuImpl().get()))
        impl->data = data_;
}

}
