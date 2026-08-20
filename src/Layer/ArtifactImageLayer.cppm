module;
#include <utility>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <limits>
#include <cmath>

#include <QDebug>
#include <QUuid>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QMatrix4x4>
#include <QTransform>
#include <QPolygonF>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QFont>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QThread>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QtConcurrent>
#include <QSaveFile>
#include <QStandardPaths>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>

module Artifact.Layer.Image;

import Artifact.Layer.CloneEffectSupport;
import Image.DepthMap;
import Geometry.DepthMeshGenerator;
import Material.Material;
import Core.AI.ImageSegmenter;
import Media.ImageSequenceSource;
import Artifact.Color.OCIOManager;
import Artifact.IO.AsyncAssetReadScheduler;

import std;
import Artifact.Layers.Abstract._2D;
import Thread.Helper;
import CvUtils;
import Artifact.Render.IRenderer;
import Artifact.Layer.SourceCrop;
import Core.Diagnostics.FallbackPolicy;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;
import Memory.SharedPtr;
import Size;
import Asset.Manager;
import AssetType;

namespace Artifact {
namespace
{
const QString kImageF32Representation = QStringLiteral("image/f32x4-rgba-linear");

QString imageF32RepresentationKey(const QString& inputColorSpace,
                                  const QString& inputTransferFunction)
{
    const QString colorSpace = inputColorSpace.trimmed();
    const QString transfer = inputTransferFunction.trimmed();
    if (colorSpace.isEmpty() && transfer.isEmpty()) {
        return kImageF32Representation;
    }

    QByteArray interpretation;
    interpretation.reserve(colorSpace.size() + transfer.size() + 1);
    interpretation.append(colorSpace.toUtf8());
    interpretation.append('\0');
    interpretation.append(transfer.toUtf8());
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(interpretation, QCryptographicHash::Sha256)
            .toHex());
    return QStringLiteral("%1/interpretation/%2")
        .arg(kImageF32Representation, digest);
}

ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA>
publishImagePayloadOrKeep(
    const QUuid& sourceAssetId, const std::uint64_t sourceVersion,
    const QString& representation,
    ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> payload)
{
    if (!payload || sourceAssetId.isNull() || sourceVersion == 0) {
        return payload;
    }
    auto published =
        ArtifactCore::staticPointerCast<ArtifactCore::ImageF32x4_RGBA>(
            ArtifactCore::AssetManager::instance().publishDecodedPayload(
                sourceAssetId, sourceVersion, representation, payload));
    return published ? published : payload;
}

ArtifactCore::ImageF32x4_RGBA toFrameBuffer(const QImage& image)
{
    ArtifactCore::ImageF32x4_RGBA buffer;
    if (image.isNull()) {
        return buffer;
    }

    const QImage encodedPremultiplied =
        image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(encodedPremultiplied, true);
    if (mat.empty()) {
        return buffer;
    }

    if (mat.type() != CV_32FC4) {
        mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
    }

    buffer.setFromCVMat(
        mat,
        ArtifactCore::SurfaceColorDescriptor::legacyOpenCvBgra32Float(
            ArtifactCore::TransferFunction::sRGB,
            ArtifactCore::SurfaceAlphaMode::Premultiplied));
    return buffer;
}

QRect sourceCropToRect(const Artifact::SourceCrop& crop, const QSize& sourceSize)
{
    if (!crop.enabled() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        return {};
    }

    const QRectF cropRect = crop.effectiveCropRect(QSizeF(sourceSize));
    if (!cropRect.isValid() || cropRect.width() <= 0.0 || cropRect.height() <= 0.0) {
        return {};
    }

    return cropRect.toAlignedRect().intersected(
        QRect(0, 0, sourceSize.width(), sourceSize.height()));
}

QImage makeTransparentCropCanvas(const QImage& source, const QRect& cropRect)
{
    if (source.isNull() || !cropRect.isValid() || cropRect.width() <= 0 || cropRect.height() <= 0) {
        return source;
    }

    const QImage rgba = source.format() == QImage::Format_RGBA8888
        ? source
        : source.convertToFormat(QImage::Format_RGBA8888);
    const QRect boundedCrop = cropRect.intersected(rgba.rect());
    if (!boundedCrop.isValid() || boundedCrop.isEmpty()) {
        return rgba;
    }

    QImage canvas(rgba.size(), QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);
    constexpr qsizetype bytesPerPixel = 4;
    const qsizetype rowBytes =
        static_cast<qsizetype>(boundedCrop.width()) * bytesPerPixel;
    for (int y = boundedCrop.top(); y <= boundedCrop.bottom(); ++y) {
        auto *dst = canvas.scanLine(y) +
                    static_cast<qsizetype>(boundedCrop.left()) * bytesPerPixel;
        const auto *src = rgba.constScanLine(y) +
                          static_cast<qsizetype>(boundedCrop.left()) * bytesPerPixel;
        std::memcpy(dst, src, static_cast<size_t>(rowBytes));
    }
    return canvas;
}

constexpr int kMaxImageDimension = 16384;
constexpr quint64 kMaxDecodedImagePixels = 64ull * 1024ull * 1024ull;
constexpr int kMaxImageChannels = 64;
constexpr int kMaxIccProfileBytes = 256 * 1024 * 1024;

bool hasSupportedImageDimensions(const int width, const int height)
{
    return width > 0 && height > 0 && width <= kMaxImageDimension &&
           height <= kMaxImageDimension &&
           static_cast<quint64>(width) * static_cast<quint64>(height) <=
               kMaxDecodedImagePixels;
}

bool hasSupportedImageChannelCount(const int channels)
{
    return channels > 0 && channels <= kMaxImageChannels;
}

void normalizeNonFiniteRgba32F(float* data, const std::size_t pixelCount)
{
    if (!data) {
        return;
    }
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            float& value = data[pixel * 4u + channel];
            if (!std::isfinite(value)) {
                value = channel == 3u ? 1.0f : 0.0f;
            }
        }
    }
}

std::array<int, 4> standardCmykChannelOrder(const OIIO::ImageSpec& spec)
{
    if (spec.alpha_channel >= 0 || spec.nchannels < 4 ||
        spec.channelnames.size() < 4) {
        return {-1, -1, -1, -1};
    }
    const auto channelIndex = [&spec](const QStringList& candidates) {
        for (int index = 0; index < spec.nchannels &&
             index < static_cast<int>(spec.channelnames.size()); ++index) {
            const QString name =
                QString::fromStdString(spec.channelnames[index]).trimmed().toLower();
            if (candidates.contains(name)) return index;
        }
        return -1;
    };
    return {channelIndex({QStringLiteral("c"), QStringLiteral("cyan")}),
            channelIndex({QStringLiteral("m"), QStringLiteral("magenta")}),
            channelIndex({QStringLiteral("y"), QStringLiteral("yellow")}),
            channelIndex({QStringLiteral("k"), QStringLiteral("black")})};
}

bool hasStandardCmykChannels(const OIIO::ImageSpec& spec)
{
    const std::array<int, 4> order = standardCmykChannelOrder(spec);
    return std::all_of(order.cbegin(), order.cend(),
                       [](const int channel) { return channel >= 0; });
}

void convertCmykPixelsToRgba(float* pixels, const size_t pixelCount)
{
    if (!pixels) return;
    for (size_t index = 0; index < pixelCount; ++index) {
        float* pixel = pixels + index * 4u;
        const float cyan = std::clamp(pixel[0], 0.0f, 1.0f);
        const float magenta = std::clamp(pixel[1], 0.0f, 1.0f);
        const float yellow = std::clamp(pixel[2], 0.0f, 1.0f);
        const float black = std::clamp(pixel[3], 0.0f, 1.0f);
        pixel[0] = (1.0f - cyan) * (1.0f - black);
        pixel[1] = (1.0f - magenta) * (1.0f - black);
        pixel[2] = (1.0f - yellow) * (1.0f - black);
        pixel[3] = 1.0f;
    }
}

void convertCmykImageToRgba(QImage& image)
{
    if (image.format() != QImage::Format_RGBA8888) return;
    for (int y = 0; y < image.height(); ++y) {
        auto* row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            auto* pixel = row + x * 4;
            const int cyan = pixel[0];
            const int magenta = pixel[1];
            const int yellow = pixel[2];
            const int black = pixel[3];
            pixel[0] = static_cast<uchar>((255 - cyan) * (255 - black) / 255);
            pixel[1] = static_cast<uchar>((255 - magenta) * (255 - black) / 255);
            pixel[2] = static_cast<uchar>((255 - yellow) * (255 - black) / 255);
            pixel[3] = 255;
        }
    }
}

void unpremultiplyRgba(float* pixels, const size_t pixelCount)
{
    if (!pixels) return;
    constexpr float kAlphaEpsilon = 1.0e-6f;
    for (size_t index = 0; index < pixelCount; ++index) {
        float* pixel = pixels + index * 4u;
        const float alpha = pixel[3];
        if (alpha > kAlphaEpsilon) {
            pixel[0] /= alpha;
            pixel[1] /= alpha;
            pixel[2] /= alpha;
        } else {
            pixel[0] = 0.0f;
            pixel[1] = 0.0f;
            pixel[2] = 0.0f;
        }
    }
}

void premultiplyRgba(float* pixels, const size_t pixelCount)
{
    if (!pixels) return;
    for (size_t index = 0; index < pixelCount; ++index) {
        float* pixel = pixels + index * 4u;
        pixel[0] *= pixel[3];
        pixel[1] *= pixel[3];
        pixel[2] *= pixel[3];
    }
}

void diagnoseCmykConversion(const QString& path)
{
    ArtifactCore::FallbackTracker::instance()->record(
        ArtifactCore::FallbackCategory::Image,
        ArtifactCore::FallbackAction::Warning,
        path, "cmyk-to-rgb",
        QStringLiteral("Standard CMYK channels converted to RGB before "
                       "working-space processing"));
}

std::array<int, 4> rgbaChannelOrder(const OIIO::ImageSpec& spec)
{
    if (hasStandardCmykChannels(spec)) return standardCmykChannelOrder(spec);
    const int alpha = spec.alpha_channel >= 0 &&
                              spec.alpha_channel < spec.nchannels
        ? spec.alpha_channel
        : -1;
    if (spec.nchannels <= 1) return {0, 0, 0, -1};
    if (spec.nchannels == 2) {
        if (alpha >= 0) {
            const int color = alpha == 0 ? 1 : 0;
            return {color, color, color, alpha};
        }
        return {0, 1, 1, -1};
    }

    std::array<int, 3> colors{0, 0, 0};
    int colorCount = 0;
    for (int channel = 0;
         channel < spec.nchannels && colorCount < 3; ++channel) {
        if (channel != alpha) {
            colors[static_cast<size_t>(colorCount++)] = channel;
        }
    }
    while (colorCount < 3) {
        colors[static_cast<size_t>(colorCount)] =
            colors[static_cast<size_t>(std::max(0, colorCount - 1))];
        ++colorCount;
    }
    return {colors[0], colors[1], colors[2], alpha};
}

void diagnoseAmbiguousChannels(const OIIO::ImageSpec& spec,
                               const QString& path)
{
    if (spec.nchannels < 4 ||
        (spec.alpha_channel >= 0 && spec.alpha_channel < spec.nchannels)) {
        return;
    }
    qWarning() << "[ArtifactImageLayer] Image has four or more channels but "
                  "no declared alpha channel; additional channels are ignored:"
               << path << "channels=" << spec.nchannels;
    ArtifactCore::FallbackTracker::instance()->record(
        ArtifactCore::FallbackCategory::Image,
        ArtifactCore::FallbackAction::Bypass,
        path, "ambiguous-channel-semantics",
        QStringLiteral(
            "Image has %1 channels and no declared alpha; using the first "
            "three color channels with opaque alpha")
            .arg(spec.nchannels));
}

QString imageSpecStringAttribute(const OIIO::ImageSpec& spec,
                                 const char* name)
{
    const OIIO::ParamValue* attribute =
        spec.find_attribute(name, OIIO::TypeDesc::STRING);
    return attribute ? QString::fromUtf8(attribute->get_string()).left(4096)
                     : QString();
}

struct SourceImageMetadata {
    int channelCount = 0;
    int alphaChannel = -1;
    bool alphaAssociated = false;
    int orientation = 1;
    double pixelAspect = 1.0;
    int bitsPerChannel = 0;
    int iccProfileBytes = 0;
    QString colorSpace;
    QString transferFunction;
    QString primaries;
    QStringList channelNames;

    static SourceImageMetadata fromSpec(const OIIO::ImageSpec& spec)
    {
        SourceImageMetadata metadata;
        metadata.channelCount = std::clamp(spec.nchannels, 0,
                                           kMaxImageChannels);
        metadata.alphaChannel =
            spec.alpha_channel >= 0 &&
                spec.alpha_channel < metadata.channelCount
            ? spec.alpha_channel
            : -1;
        metadata.alphaAssociated = metadata.alphaChannel >= 0 &&
            spec.get_int_attribute("oiio:UnassociatedAlpha", 1) == 0;
        metadata.orientation = std::clamp(
            spec.get_int_attribute("Orientation", 1), 1, 8);
        const float aspect =
            spec.get_float_attribute("PixelAspectRatio", 1.0f);
        metadata.pixelAspect = std::isfinite(aspect) && aspect > 0.0f
            ? std::clamp(static_cast<double>(aspect), 0.001, 1000.0)
            : 1.0;
        const size_t formatBytes = spec.format.size();
        const size_t bitsPerChannel = formatBytes >
                static_cast<size_t>(std::numeric_limits<int>::max() / 8)
            ? static_cast<size_t>(std::numeric_limits<int>::max())
            : formatBytes * 8u;
        metadata.bitsPerChannel = spec.format == OIIO::TypeDesc::FLOAT
            ? 32
            : std::clamp(static_cast<int>(bitsPerChannel), 0, 1024);
        metadata.colorSpace =
            imageSpecStringAttribute(spec, "oiio:ColorSpace");
        if (metadata.colorSpace.isEmpty()) {
            metadata.colorSpace = imageSpecStringAttribute(spec, "Colorspace");
        }
        metadata.transferFunction =
            imageSpecStringAttribute(spec, "TransferFunction");
        metadata.primaries = imageSpecStringAttribute(spec, "chromaticities");
        metadata.channelNames.reserve(static_cast<qsizetype>(
            std::min<size_t>(spec.channelnames.size(), 64u)));
        for (size_t i = 0; i < spec.channelnames.size() && i < 64u; ++i) {
            metadata.channelNames.append(
                QString::fromStdString(spec.channelnames[i]).left(256));
        }
        if (const OIIO::ParamValue* icc = spec.find_attribute(
                "ICCProfile", OIIO::TypeDesc::UINT8)) {
            metadata.iccProfileBytes = static_cast<int>(std::min<size_t>(
                icc->datasize(), static_cast<size_t>(kMaxIccProfileBytes)));
        }
        return metadata;
    }

    QJsonObject toJson() const
    {
        QJsonObject object;
        const int safeChannelCount =
            std::clamp(channelCount, 0, kMaxImageChannels);
        const int safeAlphaChannel =
            alphaChannel >= 0 && alphaChannel < safeChannelCount
            ? alphaChannel
            : -1;
        const double safePixelAspect =
            std::isfinite(pixelAspect) && pixelAspect > 0.0
            ? std::clamp(pixelAspect, 0.001, 1000.0)
            : 1.0;
        object["channelCount"] = safeChannelCount;
        object["alphaChannel"] = safeAlphaChannel;
        object["alphaAssociated"] = alphaAssociated;
        object["orientation"] = std::clamp(orientation, 1, 8);
        object["pixelAspect"] = safePixelAspect;
        object["bitsPerChannel"] = std::clamp(bitsPerChannel, 0, 1024);
        object["iccProfileBytes"] = std::clamp(iccProfileBytes, 0,
                                                kMaxIccProfileBytes);
        object["colorSpace"] = colorSpace.left(4096);
        object["transferFunction"] = transferFunction.left(4096);
        object["primaries"] = primaries.left(4096);
        QJsonArray names;
        for (qsizetype i = 0; i < channelNames.size() && i < 64; ++i) {
            names.append(channelNames.at(i).left(256));
        }
        object["channelNames"] = names;
        return object;
    }

    static SourceImageMetadata fromJson(const QJsonObject& object)
    {
        SourceImageMetadata metadata;
        metadata.channelCount =
            std::clamp(object.value("channelCount").toInt(), 0,
                       kMaxImageChannels);
        metadata.alphaChannel = object.value("alphaChannel").toInt(-1);
        if (metadata.alphaChannel < 0 ||
            metadata.alphaChannel >= metadata.channelCount) {
            metadata.alphaChannel = -1;
        }
        metadata.alphaAssociated = metadata.alphaChannel >= 0 &&
            object.value("alphaAssociated").toBool(false);
        metadata.orientation =
            std::clamp(object.value("orientation").toInt(1), 1, 8);
        const double aspect = object.value("pixelAspect").toDouble(1.0);
        metadata.pixelAspect = std::isfinite(aspect) && aspect > 0.0
            ? std::clamp(aspect, 0.001, 1000.0)
            : 1.0;
        metadata.bitsPerChannel =
            std::clamp(object.value("bitsPerChannel").toInt(), 0, 1024);
        metadata.iccProfileBytes =
            std::clamp(object.value("iccProfileBytes").toInt(), 0,
                       kMaxIccProfileBytes);
        metadata.colorSpace =
            object.value("colorSpace").toString().left(4096);
        metadata.transferFunction =
            object.value("transferFunction").toString().left(4096);
        metadata.primaries =
            object.value("primaries").toString().left(4096);
        const QJsonArray names = object.value("channelNames").toArray();
        for (qsizetype i = 0; i < names.size() && i < 64; ++i) {
            if (names.at(i).isString()) {
                metadata.channelNames.append(
                    names.at(i).toString().left(256));
            }
        }
        return metadata;
    }
};

QImage loadImageViaOIIO(const QString& path, QSize* sizeOut = nullptr, QString* errorOut = nullptr,
                        int subimageIndex = -1)
{
    const std::string utf8Path = path.toUtf8().toStdString();

    OIIO::ImageBuf source(utf8Path);
    const int readSubimage = std::max(0, subimageIndex);
    if (!source.read(readSubimage, 0, true, OIIO::TypeDesc::UINT8)) {
        if (errorOut) {
            *errorOut = QString::fromStdString(source.geterror());
        }
        return {};
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (!hasSupportedImageDimensions(spec.width, spec.height) ||
        !hasSupportedImageChannelCount(spec.nchannels)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Unsupported image dimensions or channel count.");
        }
        return {};
    }

    const bool isCmyk = hasStandardCmykChannels(spec);
    if (isCmyk) {
        diagnoseCmykConversion(path);
    } else {
        diagnoseAmbiguousChannels(spec, path);
    }
    const std::array<int, 4> channelOrder = rgbaChannelOrder(spec);
    const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
    OIIO::ImageBuf rgba = OIIO::ImageBufAlgo::channels(
        oriented, 4, channelOrder, channelValues);

    QImage image(spec.width, spec.height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to allocate output image.");
        }
        return {};
    }

    if (!rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, image.bits())) {
        if (errorOut) {
            *errorOut = QString::fromStdString(rgba.geterror());
        }
        return {};
    }
    if (isCmyk) convertCmykImageToRgba(image);

    if (sizeOut) {
        *sizeOut = QSize(spec.width, spec.height);
    }
    return image;
}

ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> loadFloatImageViaOIIO(
    const QString& path, int subimageIndex = -1)
{
    auto image = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>();
    OIIO::ImageBuf source(path.toUtf8().toStdString());
    const int readSubimage = std::max(0, subimageIndex);
    if (!source.read(readSubimage, 0, true, OIIO::TypeDesc::FLOAT) ||
        !hasSupportedImageDimensions(source.spec().width,
                                     source.spec().height) ||
        !hasSupportedImageChannelCount(source.spec().nchannels)) {
        return {};
    }
    const auto& spec = source.spec();
    const bool isCmyk = hasStandardCmykChannels(spec);
    if (isCmyk) {
        diagnoseCmykConversion(path);
    } else {
        diagnoseAmbiguousChannels(spec, path);
    }
    const std::array<int, 4> order = rgbaChannelOrder(spec);
    const std::array<float, 4> values{0.0f, 0.0f, 0.0f, 1.0f};
    OIIO::ImageBuf rgba =
        OIIO::ImageBufAlgo::channels(source, 4, order, values);
    std::vector<float> pixels(static_cast<size_t>(spec.width) *
                              static_cast<size_t>(spec.height) * 4u);
    if (!rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT, pixels.data())) return {};
    normalizeNonFiniteRgba32F(
        pixels.data(), static_cast<std::size_t>(spec.width) *
                           static_cast<std::size_t>(spec.height));
    if (isCmyk) {
        convertCmykPixelsToRgba(pixels.data(),
                                static_cast<size_t>(spec.width) *
                                    static_cast<size_t>(spec.height));
    }
    image->setFromRGBA32F(pixels.data(), spec.width, spec.height);
    return image;
}

struct LoadedImagePair {
    QImage image;
    ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> floatImage;
    SourceImageMetadata sourceMetadata;
    bool hasSourceMetadata = false;
};

constexpr quint32 kDerivedImageMagic = 0x41534449u; // ASDI
constexpr quint32 kDerivedImageVersion = 2u;

QString derivedImageCachePath(const QFileInfo& sourceInfo, int subimageIndex)
{
    const QByteArray identity =
        sourceInfo.absoluteFilePath().toUtf8() + '\n' +
        QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch()) + '\n' +
        QByteArray::number(sourceInfo.size()) + '\n' +
        QByteArray::number(subimageIndex) + '\n' +
        QByteArrayLiteral("f32x4-linear-v2-channel-semantics");
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
    QDir root(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    root.mkpath(QStringLiteral("derived-images"));
    return root.filePath(QStringLiteral("derived-images/%1.asdi").arg(digest));
}

LoadedImagePair decodeDerivedImage(const QByteArray& bytes,
                                   std::uint64_t expectedRevision)
{
    QByteArray storage = bytes;
    QDataStream stream(&storage, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0;
    quint32 version = 0;
    quint64 revision = 0;
    qint32 width = 0;
    qint32 height = 0;
    quint64 payloadBytes = 0;
    stream >> magic >> version >> revision >> width >> height >> payloadBytes;
    const quint64 expectedBytes = width > 0 && height > 0
        ? static_cast<quint64>(width) * static_cast<quint64>(height) *
              4ull * sizeof(float)
        : 0;
    if (stream.status() != QDataStream::Ok || magic != kDerivedImageMagic ||
        version != kDerivedImageVersion || revision != expectedRevision ||
        !hasSupportedImageDimensions(width, height) ||
        payloadBytes != expectedBytes ||
        payloadBytes > static_cast<quint64>(storage.size())) {
        return {};
    }
    const qint64 payloadOffset = stream.device()->pos();
    if (payloadOffset < 0 || payloadOffset > storage.size() || payloadBytes >
            static_cast<quint64>(storage.size() - payloadOffset)) return {};
    std::vector<float> pixels(static_cast<size_t>(width) *
                              static_cast<size_t>(height) * 4u);
    std::memcpy(pixels.data(), storage.constData() + payloadOffset,
                static_cast<size_t>(payloadBytes));
    normalizeNonFiniteRgba32F(
        pixels.data(), static_cast<std::size_t>(width) *
                           static_cast<std::size_t>(height));
    LoadedImagePair result;
    result.floatImage = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>();
    result.floatImage->setFromRGBA32F(
        pixels.data(), width, height);
    if (result.floatImage->isEmpty()) return {};
    result.image = result.floatImage->toQImage();
    return result;
}

void writeDerivedImage(const QString& cachePath,
                       std::uint64_t sourceRevision,
                       const ArtifactCore::ImageF32x4_RGBA& image)
{
    if (image.isEmpty() || !image.rgba32fData()) return;
    const quint64 payloadBytes = static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height()) * 4ull * sizeof(float);
    if (payloadBytes > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) return;
    QSaveFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << kDerivedImageMagic << kDerivedImageVersion
           << static_cast<quint64>(sourceRevision)
           << static_cast<qint32>(image.width())
           << static_cast<qint32>(image.height()) << payloadBytes;
    if (stream.status() != QDataStream::Ok ||
        file.write(reinterpret_cast<const char*>(image.rgba32fData()),
                   static_cast<qint64>(payloadBytes)) !=
            static_cast<qint64>(payloadBytes) ||
        !file.commit()) {
        file.cancelWriting();
    }
}

LoadedImagePair loadImagePairViaAsyncReader(const QString& path,
                                            std::uint64_t generation,
                                            int subimageIndex = -1)
{
    static AsyncAssetReadScheduler scheduler(3);
    const QFileInfo sourceInfo(path);
    AsyncAssetReadRequest request;
    request.key = QStringLiteral("still:%1:subimage:%2")
                      .arg(sourceInfo.absoluteFilePath())
                      .arg(subimageIndex);
    request.path = sourceInfo.absoluteFilePath();
    request.generation = generation;
    request.sourceRevision =
        static_cast<std::uint64_t>(sourceInfo.lastModified().toMSecsSinceEpoch()) ^
        static_cast<std::uint64_t>(std::max<qint64>(0, sourceInfo.size()));
    request.priority = AsyncAssetReadPriority::PlaybackNext;
    const QString cachePath = derivedImageCachePath(sourceInfo, subimageIndex);
    if (QFileInfo::exists(cachePath)) {
        AsyncAssetReadRequest cacheRequest = request;
        cacheRequest.key.prepend(QStringLiteral("derived:"));
        cacheRequest.path = cachePath;
        const AsyncAssetReadTicket cacheTicket = scheduler.enqueue(cacheRequest);
        if (cacheTicket.isValid()) {
            AsyncAssetReadResult cacheRead;
            const bool cacheCompleted =
                scheduler.waitForResult(cacheTicket, cacheRead);
            scheduler.release(cacheTicket);
            if (cacheCompleted && cacheRead.succeeded()) {
                LoadedImagePair cached =
                    decodeDerivedImage(cacheRead.bytes, request.sourceRevision);
                if (cached.floatImage) return cached;
            }
        }
    }
    const AsyncAssetReadTicket ticket = scheduler.enqueue(request);
    if (!ticket.isValid()) {
        return {};
    }
    AsyncAssetReadResult readResult;
    const bool completed = scheduler.waitForResult(ticket, readResult);
    scheduler.release(ticket);
    if (!completed || !readResult.succeeded() || readResult.bytes.isEmpty()) {
        return {};
    }

    OIIO::Filesystem::IOMemReader memoryReader(
        readResult.bytes.constData(),
        static_cast<size_t>(readResult.bytes.size()));
    const std::string utf8Path = path.toUtf8().toStdString();
    const int readSubimage = std::max(0, subimageIndex);
    OIIO::ImageBuf source(utf8Path, readSubimage, 0, {}, nullptr,
                          &memoryReader);
    if (!source.read(readSubimage, 0, true, OIIO::TypeDesc::FLOAT)) {
        return {};
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (!hasSupportedImageDimensions(spec.width, spec.height) ||
        !hasSupportedImageChannelCount(spec.nchannels)) {
        return {};
    }

    const bool isCmyk = hasStandardCmykChannels(spec);
    if (isCmyk) {
        diagnoseCmykConversion(path);
    } else {
        diagnoseAmbiguousChannels(spec, path);
    }
    const std::array<int, 4> order = rgbaChannelOrder(spec);
    const std::array<float, 4> values{0.0f, 0.0f, 0.0f, 1.0f};
    OIIO::ImageBuf rgba =
        OIIO::ImageBufAlgo::channels(oriented, 4, order, values);

    LoadedImagePair result;
    result.sourceMetadata = SourceImageMetadata::fromSpec(source.spec());
    result.hasSourceMetadata = true;
    result.image = QImage(spec.width, spec.height, QImage::Format_RGBA8888);
    if (result.image.isNull() ||
        !rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8,
                         result.image.bits())) {
        return {};
    }
    if (isCmyk) convertCmykImageToRgba(result.image);

    std::vector<float> pixels(static_cast<size_t>(spec.width) *
                              static_cast<size_t>(spec.height) * 4u);
    if (!rgba.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT,
                         pixels.data())) {
        return {};
    }
    normalizeNonFiniteRgba32F(
        pixels.data(), static_cast<std::size_t>(spec.width) *
                           static_cast<std::size_t>(spec.height));
    if (isCmyk) {
        convertCmykPixelsToRgba(pixels.data(),
                                static_cast<size_t>(spec.width) *
                                    static_cast<size_t>(spec.height));
    }
    result.floatImage =
        ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>();
    result.floatImage->setFromRGBA32F(pixels.data(), spec.width, spec.height);
    if (result.floatImage->isEmpty()) {
        return {};
    }
    writeDerivedImage(cachePath, request.sourceRevision, *result.floatImage);
    return result;
}

QImage makeMissingImagePlaceholder(const QSize& size = QSize(256, 256), const QString& label = QStringLiteral("Image unavailable"))
{
    const QSize safeSize = size.isValid() ? size.expandedTo(QSize(64, 64)) : QSize(256, 256);
    QImage placeholder(safeSize, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(QColor(34, 38, 46));

    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(180, 82, 82), 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(placeholder.rect().adjusted(2, 2, -3, -3));
    const QRect rect = placeholder.rect();
    painter.drawLine(rect.topLeft() + QPoint(10, 10), rect.bottomRight() - QPoint(10, 10));
    painter.drawLine(rect.topRight() + QPoint(-10, 10), rect.bottomLeft() + QPoint(10, -10));

    QFont font;
    font.setBold(true);
    font.setPointSizeF(std::max<qreal>(10.0, safeSize.height() * 0.08));
    painter.setFont(font);
    painter.setPen(QColor(235, 235, 235));
    painter.drawText(placeholder.rect().adjusted(12, 12, -12, -12),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     label);
    return placeholder;
}

}

class ArtifactImageLayer::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    void resetCacheBuffer() const {
        cacheBuffer_.reset();
        cacheBufferQImageSource_ = nullptr;
        cacheBufferQImage_ = {};
        cacheBufferCropSource_ = nullptr;
        cacheBufferCropSignature_.clear();
        cacheBufferCroppedImage_ = {};
    }

    bool hasImage_ = false;
    bool fitToLayer_ = true;
    int width_ = 0;
    int height_ = 0;
    QString sourcePath_;
    int psdSubimageIndex_ = -1;
    QUuid sourceAssetId_;
    QStringList sequencePaths_;
    double sequenceFrameRate_ = 0.0;
    mutable std::unique_ptr<ArtifactCore::ImageSequenceSource> sequenceSource_;
    mutable qint64 sequenceCachedIndex_ = -1;
    mutable std::uint64_t cachedSourceVersion_ = 0;
    SourceCrop sourceCrop_;
    SourceImageMetadata sourceMetadata_;
    mutable ArtifactCore::SharedPtr<QImage> cache_;
    mutable ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> cacheBuffer_;
    mutable const ArtifactCore::ImageF32x4_RGBA *cacheBufferQImageSource_ = nullptr;
    mutable QImage cacheBufferQImage_;
    mutable const ArtifactCore::ImageF32x4_RGBA *cacheBufferCropSource_ = nullptr;
    mutable QString cacheBufferCropSignature_;
    mutable QImage cacheBufferCroppedImage_;
    ArtifactCore::DepthMap depthMap_;
    ArtifactCore::Mesh depthMesh_;
    ArtifactCore::DepthMeshOptions depthMeshOptions_;
    QString depthMapPath_;
    bool depthEnabled_ = false;
    QString inputColorSpace_;
    QString inputTransferFunction_;
    // [Fix 1] バックグラウンド先読み用
    struct PrefetchResult {
        std::uint64_t generation = 0;
        std::uint64_t sourceVersion = 0;
        QImage image;
        ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> floatImage;
        SourceImageMetadata sourceMetadata;
        bool hasSourceMetadata = false;
    };
    mutable QFuture<PrefetchResult> prefetchFuture_;
    mutable QFutureWatcher<PrefetchResult> prefetchWatcher_;
    mutable std::uint64_t prefetchGeneration_ = 0;
    mutable bool prefetchDone_ = false;

    bool updateInputInterpretationValues(
        const QString& colorSpace, const QString& transferFunction,
        const QString& diagnosticSource)
    {
        const QString normalizedColorSpace = colorSpace.trimmed().left(4096);
        const QString normalizedTransferFunction =
            transferFunction.trimmed().left(1024);
        const auto* ocio = Artifact::ArtifactOCIOManager::instance();
        const QStringList availableSpaces = ocio->availableWorkingSpaces();
        QString resolvedColorSpace = normalizedColorSpace;
        bool colorSpaceAvailable = normalizedColorSpace.isEmpty() ||
                                   availableSpaces.isEmpty();
        if (!normalizedColorSpace.isEmpty() && !availableSpaces.isEmpty()) {
            const auto canonical = std::find_if(
                availableSpaces.cbegin(), availableSpaces.cend(),
                [&normalizedColorSpace](const QString& candidate) {
                    return candidate.compare(normalizedColorSpace,
                                             Qt::CaseInsensitive) == 0;
                });
            if (canonical != availableSpaces.cend()) {
                resolvedColorSpace = *canonical;
                colorSpaceAvailable = true;
            }
        }
        if (inputColorSpace_ == resolvedColorSpace &&
            inputTransferFunction_ == normalizedTransferFunction) {
            return false;
        }
        if (!colorSpaceAvailable) {
            qWarning() << "[ArtifactImageLayer] Input color space is unavailable; "
                          "preserving interpretation for project portability:"
                       << resolvedColorSpace;
            ArtifactCore::FallbackTracker::instance()->record(
                ArtifactCore::FallbackCategory::Image,
                ArtifactCore::FallbackAction::Bypass,
                diagnosticSource, "input-color-space-unavailable",
                QStringLiteral(
                    "Input color space '%1' is unavailable; preserving setting")
                    .arg(resolvedColorSpace));
        }
        inputColorSpace_ = resolvedColorSpace;
        inputTransferFunction_ = normalizedTransferFunction;
        return true;
    }

    QString effectiveInputColorSpace() const
    {
        return inputColorSpace_.trimmed().isEmpty()
            ? sourceMetadata_.colorSpace
            : inputColorSpace_;
    }

    QString effectiveInputTransferFunction() const
    {
        if (!inputTransferFunction_.trimmed().isEmpty()) {
            return inputTransferFunction_;
        }
        if (!sourceMetadata_.transferFunction.trimmed().isEmpty()) {
            return sourceMetadata_.transferFunction;
        }
        const QString colorSpace = effectiveInputColorSpace().toLower();
        if (colorSpace.contains(QStringLiteral("srgb"))) {
            return QStringLiteral("srgb");
        }
        if (colorSpace.contains(QStringLiteral("rec709")) ||
            colorSpace.contains(QStringLiteral("bt709"))) {
            return QStringLiteral("rec709");
        }
        if (colorSpace.contains(QStringLiteral("pq")) ||
            colorSpace.contains(QStringLiteral("st2084"))) {
            return QStringLiteral("pq");
        }
        if (colorSpace.contains(QStringLiteral("hlg"))) {
            return QStringLiteral("hlg");
        }
        if (colorSpace.contains(QStringLiteral("rec2020")) ||
            colorSpace.contains(QStringLiteral("bt2020"))) {
            return QStringLiteral("rec2020");
        }
        if (colorSpace.contains(QStringLiteral("acescct"))) {
            return QStringLiteral("acescct");
        }
        if (colorSpace.contains(QStringLiteral("acescc"))) {
            return QStringLiteral("acescc");
        }
        return {};
    }

    qreal displayPixelAspect() const
    {
        if (fitToLayer_ || sourceMetadata_.pixelAspect <= 0.0 ||
            !std::isfinite(sourceMetadata_.pixelAspect)) {
            return 1.0;
        }
        return std::clamp(static_cast<qreal>(sourceMetadata_.pixelAspect),
                          0.001, 1000.0);
    }

    QRectF displayRectForPixels(const QRectF& pixelRect) const
    {
        const qreal aspect = displayPixelAspect();
        return QRectF(pixelRect.x() * aspect, pixelRect.y(),
                      pixelRect.width() * aspect, pixelRect.height());
    }

    void applyInputInterpretation() const
    {
        const QString colorSpace = effectiveInputColorSpace();
        const QString transferFunction = effectiveInputTransferFunction();
        if (!cacheBuffer_ ||
            (colorSpace.trimmed().isEmpty() &&
             transferFunction.trimmed().isEmpty())) {
            return;
        }
        auto converted = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
            *cacheBuffer_);
        const bool alphaAssociated = sourceMetadata_.alphaAssociated;
        const size_t pixelCount = static_cast<size_t>(converted->width()) *
            static_cast<size_t>(converted->height());
        if (alphaAssociated) {
            unpremultiplyRgba(converted->rgba32fData(), pixelCount);
        }
        Artifact::ArtifactOCIOManager::instance()->applyInputTransformToWorkingImage(
            *converted, colorSpace, transferFunction);
        if (alphaAssociated) {
            premultiplyRgba(converted->rgba32fData(), pixelCount);
        }
        cacheBuffer_ = std::move(converted);
    }

    bool adoptPrefetchResult(const PrefetchResult& result)
    {
        if (result.generation != prefetchGeneration_) {
            return false;
        }

        const auto currentSourceVersion =
            ArtifactCore::AssetManager::instance().sourceVersion(
                sourceAssetId_);
        if (result.sourceVersion > 0 &&
            currentSourceVersion != result.sourceVersion) {
            cache_.reset();
            resetCacheBuffer();
            prefetchDone_ = currentSourceVersion == 0;
            if (currentSourceVersion > 0 && !sourcePath_.isEmpty()) {
                cachedSourceVersion_ = currentSourceVersion;
                startPrefetch();
            }
            return false;
        }

        if (result.image.isNull() && !result.floatImage) {
            cache_ = ArtifactCore::makeShared<QImage>(
                makeMissingImagePlaceholder(
                    QSize(256, 256), QStringLiteral("Image decode failed")));
            cacheBuffer_ =
                ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                    toFrameBuffer(*cache_));
            prefetchDone_ = true;
            ArtifactCore::FallbackTracker::instance()->record(
                ArtifactCore::FallbackCategory::Image,
                ArtifactCore::FallbackAction::Fallback,
                sourcePath_, "placeholder",
                QStringLiteral(
                    "Asynchronous image decode failed; using placeholder"));
            qWarning() << "[ArtifactImageLayer] Asynchronous decode failed:"
                       << sourcePath_;
            return false;
        }

        cache_ = result.image.isNull()
            ? ArtifactCore::makeShared<QImage>(result.floatImage->toQImage())
            : ArtifactCore::makeShared<QImage>(result.image);
        cacheBuffer_ = result.floatImage
            ? result.floatImage
            : ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                  toFrameBuffer(*cache_));
        if (result.hasSourceMetadata) {
            sourceMetadata_ = result.sourceMetadata;
        }
        applyInputInterpretation();
        const auto version = ArtifactCore::AssetManager::instance().sourceVersion(
            sourceAssetId_);
        if (!sourceAssetId_.isNull() && version > 0) {
            cacheBuffer_ = publishImagePayloadOrKeep(
                sourceAssetId_, version,
                imageF32RepresentationKey(effectiveInputColorSpace(),
                                          effectiveInputTransferFunction()),
                cacheBuffer_);
        }
        width_ = cache_ ? cache_->width() : result.floatImage->width();
        height_ = cache_ ? cache_->height() : result.floatImage->height();
        prefetchDone_ = true;
        return true;
    }

    bool refreshSequenceFrame(qint64 frameIndex, double timelineFrameRate) const
    {
        const auto clearSequenceFrameCache = [this]() {
            cache_.reset();
            resetCacheBuffer();
            sequenceCachedIndex_ = -1;
        };
        if (sequencePaths_.size() <= 1) {
            return false;
        }
        if (!sequenceSource_) {
            sequenceSource_ = std::make_unique<ArtifactCore::ImageSequenceSource>();
            const bool opened = sequencePaths_.size() > 1
                ? sequenceSource_->openFramePaths(sequencePaths_)
                : sequenceSource_->open(sequencePaths_.front());
            if (!opened) {
                sequenceSource_.reset();
                clearSequenceFrameCache();
                return false;
            }
            if (sequenceFrameRate_ > 0.0) {
                sequenceSource_->setFrameRate(sequenceFrameRate_);
            }
        }
        const qint64 frameCount = sequenceSource_->frameCount();
        if (frameCount <= 0) {
            clearSequenceFrameCache();
            return false;
        }
        frameIndex = sequenceFrameRate_ > 0.0
            ? sequenceSource_->frameIndexAtTime(frameIndex, timelineFrameRate)
            : std::clamp<qint64>(frameIndex, 0, frameCount - 1);
        // Hold the nearest valid source frame while the layer remains visible
        // beyond the discovered sequence range. This keeps an image-sequence
        // layer stable at its in/out boundaries instead of flashing blank.
        const qint64 resolvedFrame = std::clamp<qint64>(
            frameIndex, 0, frameCount - 1);
        if (sequenceCachedIndex_ == resolvedFrame && cache_) {
            return true;
        }
        QImage frame;
        if (!sequenceSource_->tryFrameAt(resolvedFrame, frame)) {
            // A clamped out-of-range frame is intentionally held above, but a
            // frame that exists in the sequence and fails to decode must not
            // keep displaying the previous frame.  Returning the old cache
            // here makes missing/corrupt sources look valid and violates the
            // stale-pixel failure contract.
            clearSequenceFrameCache();
            return false;
        }
        if (!hasSupportedImageDimensions(frame.width(), frame.height())) {
            clearSequenceFrameCache();
            return false;
        }
        if (width_ > 0 && height_ > 0 &&
            (frame.width() != width_ || frame.height() != height_)) {
            clearSequenceFrameCache();
            return false;
        }
        cache_ = ArtifactCore::makeShared<QImage>(frame);
        cacheBuffer_ = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
            toFrameBuffer(frame));
        applyInputInterpretation();
        sequenceCachedIndex_ = resolvedFrame;
        return true;
    }

    void startPrefetch()
    {
        const auto generation = ++prefetchGeneration_;
        prefetchDone_ = false;
        const QString path = sourcePath_;
        const int subimageIndex = psdSubimageIndex_;
        const auto sourceVersion =
            ArtifactCore::AssetManager::instance().sourceVersion(
                sourceAssetId_);
        prefetchFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(),
            [path, generation, sourceVersion,
             subimageIndex]() -> PrefetchResult {
                ArtifactCore::ScopedThreadName threadName(
                    QStringLiteral("ImageLayer/prefetch:%1").arg(QFileInfo(path).fileName()));
                LoadedImagePair loaded =
                    loadImagePairViaAsyncReader(path, generation,
                                                subimageIndex);
                return PrefetchResult{generation, sourceVersion,
                                      std::move(loaded.image),
                                      std::move(loaded.floatImage),
                                      std::move(loaded.sourceMetadata),
                                      loaded.hasSourceMetadata};
            });
        prefetchWatcher_.setFuture(prefetchFuture_);
    }

    bool refreshSourceVersionIfNeeded()
    {
        if (sourceAssetId_.isNull()) {
            return false;
        }
        auto& assetManager = ArtifactCore::AssetManager::instance();
        auto currentVersion = assetManager.sourceVersion(sourceAssetId_);
        if (currentVersion == 0) {
            const QUuid recoveredId = sourcePath_.isEmpty()
                ? QUuid()
                : assetManager.acquireSource(sourcePath_,
                                             ArtifactCore::AssetType::Image);
            if (recoveredId.isNull()) {
                sourceAssetId_ = QUuid();
                cachedSourceVersion_ = 0;
                ++prefetchGeneration_;
                prefetchDone_ = true;
                cache_ = ArtifactCore::makeShared<QImage>(
                    makeMissingImagePlaceholder(
                        QSize(256, 256),
                        QStringLiteral("Image source unavailable")));
                cacheBuffer_ =
                    ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                        toFrameBuffer(*cache_));
                ArtifactCore::FallbackTracker::instance()->record(
                    ArtifactCore::FallbackCategory::Image,
                    ArtifactCore::FallbackAction::Fallback,
                    sourcePath_, "placeholder",
                    QStringLiteral(
                        "Image source registry recovery failed; using placeholder"));
                return true;
            }
            sourceAssetId_ = recoveredId;
            currentVersion = assetManager.sourceVersion(recoveredId);
        }
        if (currentVersion == cachedSourceVersion_) {
            return false;
        }

        cachedSourceVersion_ = currentVersion;
        cache_.reset();
        resetCacheBuffer();
        sequenceSource_.reset();
        sequenceCachedIndex_ = -1;
        prefetchDone_ = false;
        if (!sourcePath_.isEmpty()) {
            startPrefetch();
        } else {
            prefetchDone_ = true;
        }
        return true;
    }
};

W_OBJECT_IMPL(ArtifactImageLayer)

ArtifactImageLayer::ArtifactImageLayer() : impl_(new Impl()) {
    QObject::connect(&impl_->prefetchWatcher_, &QFutureWatcher<Impl::PrefetchResult>::finished, this, [this]() {
        if (!impl_) {
            return;
        }

        const auto result = impl_->prefetchWatcher_.result();
        if (result.generation != impl_->prefetchGeneration_) {
            return;
        }
        const bool decoded = !result.image.isNull() || result.floatImage;
        if (!impl_->prefetchDone_) {
            (void)impl_->adoptPrefetchResult(result);
        }
        if (decoded && impl_->cache_) {
            setSourceSize(Size_2D(impl_->width_, impl_->height_));
            impl_->sourceCrop_.clampToSource(QSizeF(impl_->width_, impl_->height_));
        }
        Q_EMIT changed();
    });
}

ArtifactImageLayer::~ArtifactImageLayer() {
    // During static Qt teardown the AssetManager mutex may already reference
    // destroyed Qt synchronization state. At that point the whole process is
    // exiting, so releasing the source is unnecessary and unsafe.
    if (!QCoreApplication::closingDown()) {
        ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
    }
    delete impl_;
}

bool ArtifactImageLayer::loadFromPath(const QString& path)
{
    const QString normalizedPath = path.trimmed().left(32768);
    if (normalizedPath.isEmpty()) {
        const bool stateChanged = !impl_->sourcePath_.isEmpty() ||
                                  !impl_->sourceAssetId_.isNull() ||
                                  impl_->hasImage_ || impl_->cache_ ||
                                  impl_->cacheBuffer_ ||
                                  !impl_->sequencePaths_.isEmpty();
        ArtifactCore::AssetManager::instance().releaseSource(
            impl_->sourceAssetId_);
        impl_->sourceAssetId_ = QUuid();
        impl_->cachedSourceVersion_ = 0;
        impl_->sourcePath_.clear();
        impl_->sequencePaths_.clear();
        impl_->sequenceFrameRate_ = 0.0;
        impl_->sequenceSource_.reset();
        impl_->sequenceCachedIndex_ = -1;
        impl_->sourceMetadata_ = {};
        ++impl_->prefetchGeneration_;
        impl_->prefetchDone_ = true;
        impl_->hasImage_ = false;
        impl_->cache_.reset();
        impl_->resetCacheBuffer();
        impl_->width_ = 0;
        impl_->height_ = 0;
        setSourceSize(Size_2D(0, 0));
        if (stateChanged) {
            setDirty(LayerDirtyFlag::Source);
            Q_EMIT changed();
        }
        return true;
    }
    const auto enterLoadFailureState = [this, &normalizedPath](
        const QString& diagnostic) {
        ArtifactCore::AssetManager::instance().releaseSource(
            impl_->sourceAssetId_);
        impl_->sourceAssetId_ = QUuid();
        impl_->cachedSourceVersion_ = 0;
        impl_->sourcePath_ = normalizedPath;
        impl_->sequenceSource_.reset();
        impl_->sequenceCachedIndex_ = -1;
        impl_->sourceMetadata_ = {};
        ++impl_->prefetchGeneration_;
        impl_->prefetchDone_ = true;
        impl_->hasImage_ = true;
        impl_->cache_ = ArtifactCore::makeShared<QImage>(
            makeMissingImagePlaceholder(QSize(256, 256),
                                        QStringLiteral("Image unavailable")));
        impl_->cacheBuffer_ =
            ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                toFrameBuffer(*impl_->cache_));
        impl_->width_ = impl_->cache_->width();
        impl_->height_ = impl_->cache_->height();
        setSourceSize(Size_2D(impl_->width_, impl_->height_));
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Fallback,
            normalizedPath, "placeholder", diagnostic);
        Q_EMIT changed();
        return false;
    };
    // 連番シーケンス外のパスを読み込んだ場合はシーケンス関係を解消する
    if (!impl_->sequencePaths_.isEmpty() && !impl_->sequencePaths_.contains(normalizedPath)) {
        impl_->sequencePaths_.clear();
        impl_->sequenceFrameRate_ = 0.0;
        impl_->sequenceSource_.reset();
        impl_->sequenceCachedIndex_ = -1;
    }
    const QByteArray utf8Path = QFileInfo(normalizedPath).absoluteFilePath().toUtf8();
    auto headerInput = OIIO::ImageInput::open(utf8Path.constData());
    if (!headerInput) {
        const QString error = QString::fromStdString(OIIO::geterror());
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << normalizedPath
                   << "error=" << error;
        return enterLoadFailureState(
            error.isEmpty()
                ? QStringLiteral("Image header open failed; using placeholder")
                : QStringLiteral("Image header open failed: %1").arg(error));
    }
    const OIIO::ImageSpec spec = headerInput->spec();
    headerInput->close();
    if (!hasSupportedImageDimensions(spec.width, spec.height) ||
        !hasSupportedImageChannelCount(spec.nchannels)) {
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << normalizedPath
                   << "error=unsupported image specification";
        return enterLoadFailureState(QStringLiteral(
            "Unsupported image specification %1x%2 (%3 channels); using "
            "placeholder")
            .arg(spec.width)
            .arg(spec.height)
            .arg(spec.nchannels));
    }

    const QUuid nextAssetId = ArtifactCore::AssetManager::instance().acquireSource(
        normalizedPath, ArtifactCore::AssetType::Image);
    if (nextAssetId.isNull()) {
        qWarning() << "[ArtifactImageLayer] Failed to register image source:"
                   << normalizedPath;
        return enterLoadFailureState(QStringLiteral(
            "Image source registration failed; using placeholder"));
    }
    ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
    impl_->sourceAssetId_ = nextAssetId;
    impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(nextAssetId);
    impl_->sourcePath_ = normalizedPath;
    impl_->sourceMetadata_ = SourceImageMetadata::fromSpec(spec);
    impl_->cache_.reset();
    impl_->sequenceCachedIndex_ = -1;
    const auto version = ArtifactCore::AssetManager::instance().sourceVersion(nextAssetId);
    impl_->cacheBuffer_ = ArtifactCore::staticPointerCast<ArtifactCore::ImageF32x4_RGBA>(
        ArtifactCore::AssetManager::instance().decodedPayload(
            nextAssetId, version,
            imageF32RepresentationKey(impl_->effectiveInputColorSpace(),
                                      impl_->effectiveInputTransferFunction())));
    impl_->prefetchDone_ = false;
    impl_->hasImage_ = true;
    impl_->width_ = spec.width;
    impl_->height_ = spec.height;
    setSourceSize(Size_2D(spec.width, spec.height));

    // [Fix 1] OIIO 経由でバックグラウンド先読みし、初回 draw() 呼び出し時の
    // メインスレッドブロックを排除する
    impl_->startPrefetch();
    impl_->sourceCrop_.clampToSource(QSizeF(impl_->width_, impl_->height_));

    qDebug() << "[ArtifactImageLayer] OIIO prefetch started:" << normalizedPath
             << "sizeHint=" << QSize(spec.width, spec.height);
    Q_EMIT changed();
    return true;
}

QString ArtifactImageLayer::sourcePath() const
{
    return impl_->sourcePath_;
}

void ArtifactImageLayer::setPsdSubimageIndex(const int index)
{
    const int normalized = index < 0 ? -1 : std::min(index, 100000);
    if (impl_->psdSubimageIndex_ == normalized) return;
    impl_->psdSubimageIndex_ = normalized;
    impl_->cache_.reset();
    impl_->resetCacheBuffer();
    ++impl_->prefetchGeneration_;
    if (!impl_->sourcePath_.isEmpty()) impl_->startPrefetch();
    setDirty(LayerDirtyFlag::Source);
}

int ArtifactImageLayer::psdSubimageIndex() const
{
    return impl_->psdSubimageIndex_;
}

void ArtifactImageLayer::setInputInterpretation(
    const QString& colorSpace, const QString& transferFunction)
{
    if (!impl_->updateInputInterpretationValues(
            colorSpace, transferFunction, impl_->sourcePath_)) {
        return;
    }
    if (impl_->cacheBuffer_ && !impl_->sourcePath_.trimmed().isEmpty()) {
        // Re-read the raw source before applying a changed interpretation;
        // applying a second interpretation to an already converted buffer
        // would compound transfer and gamut transforms.
        impl_->cache_.reset();
        impl_->resetCacheBuffer();
        loadFromPath(impl_->sourcePath_);
    }
    setDirty(LayerDirtyFlag::Source);
    Q_EMIT changed();
}

QString ArtifactImageLayer::inputColorSpace() const
{
    return impl_->inputColorSpace_;
}

QString ArtifactImageLayer::inputTransferFunction() const
{
    return impl_->inputTransferFunction_;
}

bool ArtifactImageLayer::setImageSequence(const QStringList& framePaths, double frameRate)
{
    constexpr qsizetype kMaxSequenceFrames = 100000;
    QStringList normalizedPaths;
    normalizedPaths.reserve(std::min(framePaths.size(), kMaxSequenceFrames));
    for (const QString& path : framePaths) {
        if (normalizedPaths.size() >= kMaxSequenceFrames) {
            break;
        }
        const QString normalized = path.trimmed().left(32768);
        if (!normalized.isEmpty() && !normalizedPaths.contains(normalized)) {
            normalizedPaths.append(normalized);
        }
    }
    if (normalizedPaths.isEmpty()) {
        return false;
    }
    impl_->sequencePaths_ = normalizedPaths;
    impl_->sequenceFrameRate_ = std::isfinite(frameRate) && frameRate > 0.0
        ? std::clamp(frameRate, 0.001, 1000.0)
        : 0.0;
    impl_->sequenceSource_.reset();
    impl_->sequenceCachedIndex_ = -1;
    // 代表フレーム（先頭）を読み込んで表示・サイズを確定させる。
    // draw() 側で currentFrame() に応じた ImageSequenceSource のフレーム切替を行う。
    return loadFromPath(normalizedPaths.first());
}

QStringList ArtifactImageLayer::sequenceFramePaths() const
{
    return impl_->sequencePaths_;
}

bool ArtifactImageLayer::isImageSequence() const
{
    return impl_->sequencePaths_.size() > 1;
}

double ArtifactImageLayer::sequenceFrameRate() const
{
    return impl_->sequenceFrameRate_;
}

QUuid ArtifactImageLayer::sourceAssetId() const
{
    return impl_->sourceAssetId_;
}

std::uint64_t ArtifactImageLayer::sourceVersion() const
{
    return ArtifactCore::AssetManager::instance().sourceVersion(impl_->sourceAssetId_);
}

bool ArtifactImageLayer::canShareSourceGpuTexture() const
{
    return !isImageSequence() &&
           !impl_->sourceAssetId_.isNull() &&
           !impl_->sourceCrop_.enabled() &&
           !hasMasks() && getEffects().empty();
}

bool ArtifactImageLayer::sourceCropEnabled() const
{
    return impl_->sourceCrop_.enabled();
}

QString ArtifactImageLayer::sourceCropSignature() const
{
    const QRectF rect = impl_->sourceCrop_.cropRect();
    const QPointF pan = impl_->sourceCrop_.pan();
    const QPointF anchor = impl_->sourceCrop_.anchor();
    return QStringLiteral("enabled=%1|rect=%2,%3,%4,%5|pan=%6,%7|zoom=%8|rotation=%9|anchor=%10,%11|preserve=%12")
        .arg(impl_->sourceCrop_.enabled() ? 1 : 0)
        .arg(rect.x(), 0, 'g', 12)
        .arg(rect.y(), 0, 'g', 12)
        .arg(rect.width(), 0, 'g', 12)
        .arg(rect.height(), 0, 'g', 12)
        .arg(pan.x(), 0, 'g', 12)
        .arg(pan.y(), 0, 'g', 12)
        .arg(impl_->sourceCrop_.zoom(), 0, 'g', 12)
        .arg(impl_->sourceCrop_.rotation(), 0, 'g', 12)
        .arg(anchor.x(), 0, 'g', 12)
        .arg(anchor.y(), 0, 'g', 12)
        .arg(impl_->sourceCrop_.preserveAspect() ? 1 : 0);
}

bool ArtifactImageLayer::localizeSourceIdentity()
{
    if (impl_->sourceAssetId_.isNull() || isSourceIdentityLocalized()) {
        return false;
    }
    const QUuid localizedId = ArtifactCore::AssetManager::instance().localizeSource(
        impl_->sourceAssetId_);
    if (localizedId.isNull()) {
        return false;
    }
    impl_->sourceAssetId_ = localizedId;
    impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(localizedId);
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
    return true;
}

bool ArtifactImageLayer::isSourceIdentityLocalized() const
{
    return ArtifactCore::AssetManager::instance().isLocalizedSource(
        impl_->sourceAssetId_);
}

bool ArtifactImageLayer::relinkSourceIdentityToShared()
{
    if (!isSourceIdentityLocalized() || impl_->sourcePath_.isEmpty()) {
        return false;
    }
    const QUuid sharedId = ArtifactCore::AssetManager::instance().acquireSource(
        impl_->sourcePath_, ArtifactCore::AssetType::Image);
    if (sharedId.isNull()) {
        return false;
    }
    ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
    impl_->sourceAssetId_ = sharedId;
    impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(sharedId);
    if (impl_->cacheBuffer_) {
        const auto version = ArtifactCore::AssetManager::instance().sourceVersion(sharedId);
        impl_->cacheBuffer_ = publishImagePayloadOrKeep(
            sharedId, version,
            imageF32RepresentationKey(impl_->effectiveInputColorSpace(),
                                      impl_->effectiveInputTransferFunction()),
            impl_->cacheBuffer_);
    }
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
    return true;
}

QJsonObject ArtifactImageLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstract2DLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Image);
    obj["image.sourcePath"] = impl_->sourcePath_;
    obj["image.psdSubimageIndex"] = impl_->psdSubimageIndex_;
    obj["image.inputColorSpace"] = impl_->inputColorSpace_;
    obj["image.inputTransferFunction"] = impl_->inputTransferFunction_;
    obj["image.sourceAssetId"] = impl_->sourceAssetId_.toString(QUuid::WithoutBraces);
    obj["image.sourceLocalized"] = isSourceIdentityLocalized();
    obj["image.fitToLayer"] = impl_->fitToLayer_;
    obj["image.width"] = std::clamp(impl_->width_, 0, kMaxImageDimension);
    obj["image.height"] = std::clamp(impl_->height_, 0, kMaxImageDimension);
    obj["image.sourceMetadata"] = impl_->sourceMetadata_.toJson();
    obj["image.depthMapPath"] = impl_->depthMapPath_;
    obj["image.depthMeshEnabled"] = impl_->depthEnabled_;
    obj["image.depthMeshColumns"] = impl_->depthMeshOptions_.columns;
    obj["image.depthMeshRows"] = impl_->depthMeshOptions_.rows;
    obj["image.depthMeshWidth"] = impl_->depthMeshOptions_.width;
    obj["image.depthMeshHeight"] = impl_->depthMeshOptions_.height;
    obj["image.depthMeshDepthScale"] = impl_->depthMeshOptions_.depthScale;
    obj["image.depthMeshInvert"] = impl_->depthMeshOptions_.invertDepth;
    if (!impl_->sequencePaths_.isEmpty()) {
        QJsonArray sequenceArray;
        for (const QString& framePath : impl_->sequencePaths_) {
            sequenceArray.append(framePath);
        }
        obj["image.sequencePaths"] = sequenceArray;
        obj["image.sequenceFrameRate"] = impl_->sequenceFrameRate_;
    }
    obj["sourceCrop.enabled"] = impl_->sourceCrop_.enabled();
    obj["sourceCrop.cropX"] = impl_->sourceCrop_.cropRect().x();
    obj["sourceCrop.cropY"] = impl_->sourceCrop_.cropRect().y();
    obj["sourceCrop.cropWidth"] = impl_->sourceCrop_.cropRect().width();
    obj["sourceCrop.cropHeight"] = impl_->sourceCrop_.cropRect().height();
    obj["sourceCrop.panX"] = impl_->sourceCrop_.pan().x();
    obj["sourceCrop.panY"] = impl_->sourceCrop_.pan().y();
    obj["sourceCrop.zoom"] = impl_->sourceCrop_.zoom();
    obj["sourceCrop.rotation"] = impl_->sourceCrop_.rotation();
    obj["sourceCrop.anchorX"] = impl_->sourceCrop_.anchor().x();
    obj["sourceCrop.anchorY"] = impl_->sourceCrop_.anchor().y();
    obj["sourceCrop.preserveAspect"] = impl_->sourceCrop_.preserveAspect();
    obj["sourceCrop"] = impl_->sourceCrop_.toJson();
    return obj;
}

void ArtifactImageLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
    ++impl_->prefetchGeneration_;
    // シーケンス情報は sourcePath 読み込みより先に復元する
    // （loadFromPath がシーケンス外パスでフィールドを解消するため）
    impl_->sequencePaths_.clear();
    impl_->sequenceFrameRate_ = 0.0;
    impl_->sequenceSource_.reset();
    impl_->sequenceCachedIndex_ = -1;
    impl_->sourceMetadata_ = {};
    impl_->cache_.reset();
    impl_->resetCacheBuffer();
    const int serializedSubimage = obj.value(QStringLiteral("image.psdSubimageIndex")).toInt(-1);
    impl_->psdSubimageIndex_ = serializedSubimage < 0
        ? -1
        : std::min(serializedSubimage, 100000);
    if (obj.value(QStringLiteral("image.sequencePaths")).isArray()) {
        constexpr qsizetype kMaxSequenceFrames = 100000;
        const QJsonArray sequenceArray =
            obj.value(QStringLiteral("image.sequencePaths")).toArray();
        for (const QJsonValue& value : sequenceArray) {
            if (impl_->sequencePaths_.size() >= kMaxSequenceFrames) {
                break;
            }
            if (!value.isString()) {
                continue;
            }
            const QString framePath =
                value.toString().trimmed().left(32768);
            if (!framePath.isEmpty() && !impl_->sequencePaths_.contains(framePath)) {
                impl_->sequencePaths_.append(framePath);
            }
        }
        const double frameRate =
            obj.value(QStringLiteral("image.sequenceFrameRate")).toDouble(0.0);
        impl_->sequenceFrameRate_ = std::isfinite(frameRate) && frameRate > 0.0
            ? std::clamp(frameRate, 0.001, 1000.0)
            : 0.0;
    }
    const QString sourcePath = obj.value(QStringLiteral("image.sourcePath"))
                                   .toString().trimmed().left(32768);
    impl_->depthMapPath_ = obj.value(QStringLiteral("image.depthMapPath"))
                               .toString().trimmed().left(32768);
    impl_->depthMeshOptions_.columns = std::clamp(
        obj.value(QStringLiteral("image.depthMeshColumns")).toInt(64), 2, 512);
    impl_->depthMeshOptions_.rows = std::clamp(
        obj.value(QStringLiteral("image.depthMeshRows")).toInt(64), 2, 512);
    impl_->depthMeshOptions_.width = std::clamp(
        static_cast<float>(obj.value(QStringLiteral("image.depthMeshWidth")).toDouble(1.0)),
        0.001f, 100000.0f);
    impl_->depthMeshOptions_.height = std::clamp(
        static_cast<float>(obj.value(QStringLiteral("image.depthMeshHeight")).toDouble(1.0)),
        0.001f, 100000.0f);
    impl_->depthMeshOptions_.depthScale = std::clamp(
        static_cast<float>(obj.value(QStringLiteral("image.depthMeshDepthScale")).toDouble(0.25)),
        -100000.0f, 100000.0f);
    impl_->depthMeshOptions_.invertDepth = obj.value(QStringLiteral("image.depthMeshInvert")).toBool(false);
    const QString inputColorSpace =
        obj.value(QStringLiteral("image.inputColorSpace")).toString();
    const QString inputTransferFunction =
        obj.value(QStringLiteral("image.inputTransferFunction")).toString();
    const bool hasSerializedSourceMetadata =
        obj.value(QStringLiteral("image.sourceMetadata")).isObject();
    const SourceImageMetadata serializedSourceMetadata =
        hasSerializedSourceMetadata
        ? SourceImageMetadata::fromJson(
              obj.value(QStringLiteral("image.sourceMetadata")).toObject())
        : SourceImageMetadata{};
    // Project restore establishes serialized state without taking the
    // user-edit path. In particular, do not dirty the layer, emit a change, or
    // start a redundant decode of the source that was present before restore.
    impl_->updateInputInterpretationValues(
        inputColorSpace, inputTransferFunction, sourcePath);
    bool metadataReadFromSource = false;
    if (!sourcePath.isEmpty() && sourcePath != impl_->sourcePath_) {
        metadataReadFromSource = loadFromPath(sourcePath);
    } else if (!sourcePath.isEmpty()) {
        if (hasSerializedSourceMetadata) {
            impl_->sourceMetadata_ = serializedSourceMetadata;
        }
        impl_->prefetchDone_ = false;
        impl_->startPrefetch();
    } else {
        ArtifactCore::AssetManager::instance().releaseSource(
            impl_->sourceAssetId_);
        impl_->sourceAssetId_ = QUuid();
        impl_->cachedSourceVersion_ = 0;
        impl_->sourcePath_.clear();
        impl_->sequencePaths_.clear();
        impl_->sequenceFrameRate_ = 0.0;
        impl_->sequenceSource_.reset();
        impl_->sequenceCachedIndex_ = -1;
        impl_->hasImage_ = false;
        impl_->prefetchDone_ = true;
        impl_->width_ = 0;
        impl_->height_ = 0;
        impl_->sourceMetadata_ = {};
        setSourceSize(Size_2D(0, 0));
    }
    if (!sourcePath.isEmpty() && !metadataReadFromSource &&
        hasSerializedSourceMetadata) {
        // Keep the last known source description when a project opens while
        // the file is missing. Relink can replace it with fresh header data.
        impl_->sourceMetadata_ = serializedSourceMetadata;
    }
    impl_->fitToLayer_ = obj.value(QStringLiteral("image.fitToLayer"))
                            .toBool(impl_->fitToLayer_);
    if (obj.value(QStringLiteral("sourceCrop")).isObject()) {
        impl_->sourceCrop_.fromJson(obj.value(QStringLiteral("sourceCrop")).toObject());
        impl_->sourceCrop_.clampToSource(
            QSizeF(impl_->width_, impl_->height_));
    } else if (obj.contains(QStringLiteral("sourceCrop.enabled")) ||
               obj.contains(QStringLiteral("sourceCrop.cropX")) ||
               obj.contains(QStringLiteral("sourceCrop.cropY")) ||
               obj.contains(QStringLiteral("sourceCrop.cropWidth")) ||
               obj.contains(QStringLiteral("sourceCrop.cropHeight")) ||
               obj.contains(QStringLiteral("sourceCrop.panX")) ||
               obj.contains(QStringLiteral("sourceCrop.panY")) ||
               obj.contains(QStringLiteral("sourceCrop.zoom")) ||
               obj.contains(QStringLiteral("sourceCrop.rotation")) ||
               obj.contains(QStringLiteral("sourceCrop.anchorX")) ||
               obj.contains(QStringLiteral("sourceCrop.anchorY")) ||
               obj.contains(QStringLiteral("sourceCrop.preserveAspect"))) {
        QJsonObject legacyCrop;
        legacyCrop[QStringLiteral("enabled")] =
            obj.value(QStringLiteral("sourceCrop.enabled")).toBool(false);
        legacyCrop[QStringLiteral("cropRect")] = QJsonArray{
            obj.value(QStringLiteral("sourceCrop.cropX")).toDouble(0.0),
            obj.value(QStringLiteral("sourceCrop.cropY")).toDouble(0.0),
            obj.value(QStringLiteral("sourceCrop.cropWidth")).toDouble(0.0),
            obj.value(QStringLiteral("sourceCrop.cropHeight")).toDouble(0.0)};
        legacyCrop[QStringLiteral("pan")] = QJsonArray{
            obj.value(QStringLiteral("sourceCrop.panX")).toDouble(0.0),
            obj.value(QStringLiteral("sourceCrop.panY")).toDouble(0.0)};
        legacyCrop[QStringLiteral("zoom")] =
            obj.value(QStringLiteral("sourceCrop.zoom")).toDouble(1.0);
        legacyCrop[QStringLiteral("rotation")] =
            obj.value(QStringLiteral("sourceCrop.rotation")).toDouble(0.0);
        legacyCrop[QStringLiteral("anchor")] = QJsonArray{
            obj.value(QStringLiteral("sourceCrop.anchorX")).toDouble(0.5),
            obj.value(QStringLiteral("sourceCrop.anchorY")).toDouble(0.5)};
        legacyCrop[QStringLiteral("preserveAspect")] =
            obj.value(QStringLiteral("sourceCrop.preserveAspect")).toBool(true);
        impl_->sourceCrop_.fromJson(legacyCrop);
        impl_->sourceCrop_.clampToSource(
            QSizeF(impl_->width_, impl_->height_));
    }

    if (!sourcePath.isEmpty() &&
        obj.value(QStringLiteral("image.sourceLocalized")).toBool(false)) {
        const QUuid savedId(obj.value(QStringLiteral("image.sourceAssetId")).toString());
        bool restored = false;
        if (!savedId.isNull() &&
            ArtifactCore::AssetManager::instance().acquireExistingSource(savedId)) {
            ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
            impl_->sourceAssetId_ = savedId;
            impl_->cachedSourceVersion_ = ArtifactCore::AssetManager::instance().sourceVersion(savedId);
            restored = true;
        }
        if (!restored) {
            localizeSourceIdentity();
        }
    }
    if (obj.value(QStringLiteral("image.depthMeshEnabled")).toBool(false) &&
        !impl_->depthMapPath_.isEmpty()) {
        setDepthMapPath(impl_->depthMapPath_);
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactImageLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup imageGroup(QStringLiteral("Image"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type,
                           const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    imageGroup.addProperty(makeProp(QStringLiteral("image.sourcePath"),
                                    ArtifactCore::PropertyType::String,
                                    sourcePath(), -150));
    auto inputColorSpaceProp = makeProp(
        QStringLiteral("image.inputColorSpace"),
        ArtifactCore::PropertyType::String, inputColorSpace(), -145);
    inputColorSpaceProp->setDisplayLabel(QStringLiteral("Input Color Space"));
    const QStringList workingSpaces =
        Artifact::ArtifactOCIOManager::instance()->availableWorkingSpaces();
    if (!workingSpaces.isEmpty()) {
        inputColorSpaceProp->setTooltip(
            QStringLiteral("OCIO working spaces: %1")
                .arg(workingSpaces.join(QStringLiteral(", "))));
    }
    imageGroup.addProperty(inputColorSpaceProp);
    auto inputTransferFunctionProp = makeProp(
        QStringLiteral("image.inputTransferFunction"),
        ArtifactCore::PropertyType::String, inputTransferFunction(), -144);
    inputTransferFunctionProp->setDisplayLabel(QStringLiteral("Input Transfer"));
    inputTransferFunctionProp->setTooltip(
        QStringLiteral("Supported: linear, sRGB, gamma22, gamma24, gamma26, "
                       "Rec709, PQ/ST2084, HLG, ACEScc, ACEScct, SLog3"));
    imageGroup.addProperty(inputTransferFunctionProp);
    auto localizedProp = makeProp(QStringLiteral("source.localized"),
                                  ArtifactCore::PropertyType::Boolean,
                                  isSourceIdentityLocalized(), -149);
    localizedProp->setDisplayLabel(QStringLiteral("Localized Source"));
    localizedProp->setTooltip(QStringLiteral("Detach this layer from the shared source identity"));
    imageGroup.addProperty(localizedProp);
    auto useCountProp = makeProp(QStringLiteral("source.sharedUseCount"),
                                 ArtifactCore::PropertyType::Integer,
                                 ArtifactCore::AssetManager::instance().useCount(impl_->sourceAssetId_), -148);
    useCountProp->setDisplayLabel(QStringLiteral("Source Uses"));
    useCountProp->setTooltip(QStringLiteral("Loaded layers using this source identity"));
    imageGroup.addProperty(useCountProp);
    imageGroup.addProperty(makeProp(QStringLiteral("image.fitToLayer"),
                                    ArtifactCore::PropertyType::Boolean,
                                    impl_->fitToLayer_, -140));
    if (isImageSequence()) {
        auto sequenceRateProp = makeProp(
            QStringLiteral("image.sequenceFrameRate"),
            ArtifactCore::PropertyType::Float,
            impl_->sequenceFrameRate_, -139);
        sequenceRateProp->setDisplayLabel(QStringLiteral("Sequence FPS"));
        sequenceRateProp->setSoftRange(1.0, 120.0);
        sequenceRateProp->setHardRange(0.001, 1000.0);
        sequenceRateProp->setUnit(QStringLiteral("fps"));
        imageGroup.addProperty(sequenceRateProp);
    }

    groups.push_back(imageGroup);

    ArtifactCore::PropertyGroup sourceCropGroup(QStringLiteral("Source Reframe"));
    auto enabledProp = makeProp(QStringLiteral("sourceCrop.enabled"),
                                ArtifactCore::PropertyType::Boolean,
                                impl_->sourceCrop_.enabled(), -240);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    sourceCropGroup.addProperty(enabledProp);

    auto cropXProp = makeProp(QStringLiteral("sourceCrop.cropX"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().x(), -239);
    cropXProp->setDisplayLabel(QStringLiteral("Crop X"));
    cropXProp->setUnit(QStringLiteral("px"));
    cropXProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(cropXProp);

    auto cropYProp = makeProp(QStringLiteral("sourceCrop.cropY"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().y(), -238);
    cropYProp->setDisplayLabel(QStringLiteral("Crop Y"));
    cropYProp->setUnit(QStringLiteral("px"));
    cropYProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(cropYProp);

    auto cropWProp = makeProp(QStringLiteral("sourceCrop.cropWidth"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().width(), -237);
    cropWProp->setDisplayLabel(QStringLiteral("Crop W"));
    cropWProp->setUnit(QStringLiteral("px"));
    cropWProp->setSoftRange(0.0, 10000.0);
    sourceCropGroup.addProperty(cropWProp);

    auto cropHProp = makeProp(QStringLiteral("sourceCrop.cropHeight"),
                              ArtifactCore::PropertyType::Float,
                              impl_->sourceCrop_.cropRect().height(), -236);
    cropHProp->setDisplayLabel(QStringLiteral("Crop H"));
    cropHProp->setUnit(QStringLiteral("px"));
    cropHProp->setSoftRange(0.0, 10000.0);
    sourceCropGroup.addProperty(cropHProp);

    auto panXProp = makeProp(QStringLiteral("sourceCrop.panX"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.pan().x(), -235);
    panXProp->setDisplayLabel(QStringLiteral("Pan X"));
    panXProp->setUnit(QStringLiteral("px"));
    panXProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(panXProp);

    auto panYProp = makeProp(QStringLiteral("sourceCrop.panY"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.pan().y(), -234);
    panYProp->setDisplayLabel(QStringLiteral("Pan Y"));
    panYProp->setUnit(QStringLiteral("px"));
    panYProp->setSoftRange(-10000.0, 10000.0);
    sourceCropGroup.addProperty(panYProp);

    auto zoomProp = makeProp(QStringLiteral("sourceCrop.zoom"),
                             ArtifactCore::PropertyType::Float,
                             impl_->sourceCrop_.zoom(), -233);
    zoomProp->setDisplayLabel(QStringLiteral("Zoom"));
    zoomProp->setUnit(QStringLiteral("x"));
    zoomProp->setSoftRange(0.1, 8.0);
    zoomProp->setStep(0.05);
    sourceCropGroup.addProperty(zoomProp);

    auto rotationProp = makeProp(QStringLiteral("sourceCrop.rotation"),
                                 ArtifactCore::PropertyType::Float,
                                 impl_->sourceCrop_.rotation(), -232);
    rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
    rotationProp->setUnit(QStringLiteral("deg"));
    rotationProp->setSoftRange(-360.0, 360.0);
    rotationProp->setStep(0.5);
    sourceCropGroup.addProperty(rotationProp);

    auto anchorXProp = makeProp(QStringLiteral("sourceCrop.anchorX"),
                                ArtifactCore::PropertyType::Float,
                                impl_->sourceCrop_.anchor().x(), -231);
    anchorXProp->setDisplayLabel(QStringLiteral("Anchor X"));
    anchorXProp->setSoftRange(0.0, 1.0);
    anchorXProp->setStep(0.01);
    sourceCropGroup.addProperty(anchorXProp);

    auto anchorYProp = makeProp(QStringLiteral("sourceCrop.anchorY"),
                                ArtifactCore::PropertyType::Float,
                                impl_->sourceCrop_.anchor().y(), -230);
    anchorYProp->setDisplayLabel(QStringLiteral("Anchor Y"));
    anchorYProp->setSoftRange(0.0, 1.0);
    anchorYProp->setStep(0.01);
    sourceCropGroup.addProperty(anchorYProp);

    auto preserveProp = makeProp(QStringLiteral("sourceCrop.preserveAspect"),
                                 ArtifactCore::PropertyType::Boolean,
                                 impl_->sourceCrop_.preserveAspect(), -229);
    preserveProp->setDisplayLabel(QStringLiteral("Preserve Aspect"));
    sourceCropGroup.addProperty(preserveProp);

    groups.push_back(sourceCropGroup);
    return groups;
}

bool ArtifactImageLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("source.localized") ||
        propertyPath == QStringLiteral("source.sharedUseCount")) {
        return false;
    }
    if (propertyPath == QStringLiteral("image.sourcePath") || propertyPath == QStringLiteral("sourcePath")) {
        const QString requestedPath = value.toString().trimmed().left(32768);
        const QString previousPath = impl_->sourcePath_;
        const bool hadSourceState = !impl_->sourceAssetId_.isNull() ||
                                    impl_->hasImage_ || impl_->cache_ ||
                                    impl_->cacheBuffer_ ||
                                    !impl_->sequencePaths_.isEmpty();
        const bool loaded = loadFromPath(requestedPath);
        const bool accepted = loaded || impl_->sourcePath_ == requestedPath;
        const bool sourcePropertyChanged = requestedPath != previousPath ||
            (requestedPath.isEmpty() && hadSourceState);
        if (accepted && sourcePropertyChanged) {
            setDirty(LayerDirtyFlag::Source);
        }
        return accepted;
    }
    if (propertyPath == QStringLiteral("image.sequenceFrameRate")) {
        const double frameRate = value.toDouble();
        if (!std::isfinite(frameRate) || frameRate < 0.0) {
            return false;
        }
        impl_->sequenceFrameRate_ = frameRate > 0.0
            ? std::clamp(frameRate, 0.001, 1000.0)
            : 0.0;
        if (impl_->sequenceSource_) {
            impl_->sequenceSource_->setFrameRate(impl_->sequenceFrameRate_);
        }
        setDirty(LayerDirtyFlag::Source);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("image.fitToLayer")) {
        setFitToLayer(value.toBool());
        return true;
    }
    if (propertyPath == QStringLiteral("image.inputColorSpace") ||
        propertyPath == QStringLiteral("image.inputTransferFunction")) {
        QString colorSpace = inputColorSpace();
        QString transferFunction = inputTransferFunction();
        if (propertyPath == QStringLiteral("image.inputColorSpace")) {
            colorSpace = value.toString();
        } else {
            transferFunction = value.toString();
        }
        setInputInterpretation(colorSpace, transferFunction);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.enabled")) {
        impl_->sourceCrop_.setEnabled(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.cropX") ||
        propertyPath == QStringLiteral("sourceCrop.cropY") ||
        propertyPath == QStringLiteral("sourceCrop.cropWidth") ||
        propertyPath == QStringLiteral("sourceCrop.cropHeight")) {
        const auto size = sourceSize();
        QRectF rect = impl_->sourceCrop_.cropRect();
        if (!rect.isValid() || rect.width() <= 0.0 || rect.height() <= 0.0) {
            rect = QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
        }
        if (propertyPath == QStringLiteral("sourceCrop.cropX")) {
            const double next = value.toDouble();
            rect.moveLeft(std::isfinite(next) ? std::clamp(next, -1000000.0, 1000000.0) : rect.left());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropY")) {
            const double next = value.toDouble();
            rect.moveTop(std::isfinite(next) ? std::clamp(next, -1000000.0, 1000000.0) : rect.top());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropWidth")) {
            const double next = value.toDouble();
            rect.setWidth(std::isfinite(next) ? std::clamp(next, 1.0, 1000000.0) : rect.width());
        } else {
            const double next = value.toDouble();
            rect.setHeight(std::isfinite(next) ? std::clamp(next, 1.0, 1000000.0) : rect.height());
        }
        impl_->sourceCrop_.setCropRect(rect);
        impl_->sourceCrop_.clampToSource(QSizeF(size.width, size.height));
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.panX") ||
        propertyPath == QStringLiteral("sourceCrop.panY")) {
        QPointF pan = impl_->sourceCrop_.pan();
        if (propertyPath == QStringLiteral("sourceCrop.panX")) {
            const double next = value.toDouble();
            pan.setX(std::isfinite(next) ? std::clamp(next, -1000000.0, 1000000.0) : pan.x());
        } else {
            const double next = value.toDouble();
            pan.setY(std::isfinite(next) ? std::clamp(next, -1000000.0, 1000000.0) : pan.y());
        }
        impl_->sourceCrop_.setPan(pan);
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.zoom")) {
        const double next = value.toDouble();
        impl_->sourceCrop_.setZoom(std::isfinite(next) ? std::clamp(next, 0.001, 1000.0) : 1.0);
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.rotation")) {
        const double next = value.toDouble();
        impl_->sourceCrop_.setRotation(std::isfinite(next) ? std::clamp(next, -360000.0, 360000.0) : 0.0);
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.anchorX") ||
        propertyPath == QStringLiteral("sourceCrop.anchorY")) {
        QPointF anchor = impl_->sourceCrop_.anchor();
        if (propertyPath == QStringLiteral("sourceCrop.anchorX")) {
            const double next = value.toDouble();
            anchor.setX(std::isfinite(next) ? std::clamp(next, -100.0, 100.0) : anchor.x());
        } else {
            const double next = value.toDouble();
            anchor.setY(std::isfinite(next) ? std::clamp(next, -100.0, 100.0) : anchor.y());
        }
        impl_->sourceCrop_.setAnchor(anchor);
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.preserveAspect")) {
        impl_->sourceCrop_.setPreserveAspect(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactImageLayer::setFromCvMat(const cv::Mat& mat)
{
    if (mat.empty()) {
        setFromQImage(QImage());
        return;
    }

    setFromQImage(ArtifactCore::CvUtils::cvMatToQImage(mat));
}

void ArtifactImageLayer::setFromCvMat()
{
    if (impl_->cache_) {
        setFromQImage(*impl_->cache_);
    }
}

void ArtifactImageLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer) return;

    if (isImageSequence()) {
        const qint64 layerFrame =
            currentFrame() - startTime().framePosition();
        impl_->refreshSequenceFrame(layerFrame, compositionFrameRate());
    }

    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }

    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(size.width, size.height));
    const bool useCrop = cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0;

    // Source Crop reframes content inside the layer's fixed output frame.
    // Its source-window parameters must never become the layer/gizmo bounds.
    const QRectF outputDrawRect = impl_->displayRectForPixels(
        QRectF(0.0, 0.0, size.width, size.height));
    const auto cropOutputRect = [&]() {
        if (!useCrop || !impl_->sourceCrop_.preserveAspect()) {
            return outputDrawRect;
        }
        const QRectF cropDisplayRect =
            impl_->displayRectForPixels(QRectF(cropRect));
        if (cropDisplayRect.width() <= 0.0 || cropDisplayRect.height() <= 0.0) {
            return outputDrawRect;
        }
        const qreal scale = std::min(outputDrawRect.width() / cropDisplayRect.width(),
                                     outputDrawRect.height() / cropDisplayRect.height());
        const QSizeF fittedSize(cropDisplayRect.width() * scale,
                                cropDisplayRect.height() * scale);
        return QRectF(outputDrawRect.center() - QPointF(fittedSize.width() * 0.5,
                                                        fittedSize.height() * 0.5),
                      fittedSize);
    };
    const QMatrix4x4 baseTransform = getGlobalTransform4x4();

    // Depth-enabled file-backed images use the existing 3D mesh renderer.
    // In-memory/sequence sources continue through the regular sprite path
    // until a GPU texture-view material overload is available.
    if (impl_->depthEnabled_ && impl_->depthMesh_.vertexCount() > 0 &&
        !impl_->sourcePath_.isEmpty()) {
        ArtifactCore::Material material = ArtifactCore::Material::makeDefault();
        material.setBaseColorTexture(
            ArtifactCore::UniString::fromQString(impl_->sourcePath_));
        renderer->drawMesh(QStringLiteral("depth-image:%1").arg(id().toString()),
                           impl_->depthMesh_, material, baseTransform,
                           opacity(), 3, nullptr);
        return;
    }

    if (hasCurrentFrameBuffer()) {
        const ArtifactCore::ImageF32x4_RGBA& buffer = currentFrameBuffer();
        const QRectF uvRect = useCrop
            ? QRectF(static_cast<qreal>(cropRect.x()) / buffer.width(),
                     static_cast<qreal>(cropRect.y()) / buffer.height(),
                     static_cast<qreal>(cropRect.width()) / buffer.width(),
                     static_cast<qreal>(cropRect.height()) / buffer.height())
            : QRectF(0.0, 0.0, 1.0, 1.0);
        const QRectF drawRect = cropOutputRect();
        QMatrix4x4 cropTransform = baseTransform;
        if (useCrop && std::abs(impl_->sourceCrop_.rotation()) > 1e-6) {
            const QPointF anchor = impl_->sourceCrop_.anchor();
            const QPointF pivot(
                drawRect.x() + drawRect.width() * anchor.x(),
                drawRect.y() + drawRect.height() * anchor.y());
            cropTransform.translate(static_cast<float>(pivot.x()),
                                    static_cast<float>(pivot.y()), 0.0f);
            cropTransform.rotate(static_cast<float>(impl_->sourceCrop_.rotation()),
                                 0.0f, 0.0f, 1.0f);
            cropTransform.translate(static_cast<float>(-pivot.x()),
                                    static_cast<float>(-pivot.y()), 0.0f);
        }
        drawWithClonerEffect(this, cropTransform, [renderer, &buffer, drawRect, uvRect, this](const QMatrix4x4& transform, float weight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(drawRect.x()),
                static_cast<float>(drawRect.y()),
                static_cast<float>(drawRect.width()),
                static_cast<float>(drawRect.height()),
                transform, buffer, this->opacity() * weight, uvRect);
        });
        return;
    }

    // Prefer the raw cache so the fallback follows the same UV reframe path
    // as the buffer renderer. toQImage() remains the compatibility fallback.
    const bool fallbackHasRawSource = static_cast<bool>(impl_->cache_);
    QImage img = fallbackHasRawSource ? *impl_->cache_ : toQImage();
    if (img.isNull()) return;
    const bool fallbackCanSampleCrop = fallbackHasRawSource && useCrop &&
        img.width() > 0 && img.height() > 0;
    const QRectF uvRect = fallbackCanSampleCrop
        ? QRectF(static_cast<qreal>(cropRect.x()) / img.width(),
                 static_cast<qreal>(cropRect.y()) / img.height(),
                 static_cast<qreal>(cropRect.width()) / img.width(),
                 static_cast<qreal>(cropRect.height()) / img.height())
        : QRectF(0.0, 0.0, 1.0, 1.0);
    const QRectF drawRect = cropOutputRect();
    QMatrix4x4 cropTransform = baseTransform;
    if (useCrop && std::abs(impl_->sourceCrop_.rotation()) > 1e-6) {
        const QPointF anchor = impl_->sourceCrop_.anchor();
        const QPointF pivot(
            drawRect.x() + drawRect.width() * anchor.x(),
            drawRect.y() + drawRect.height() * anchor.y());
        cropTransform.translate(static_cast<float>(pivot.x()),
                                static_cast<float>(pivot.y()), 0.0f);
        cropTransform.rotate(static_cast<float>(impl_->sourceCrop_.rotation()),
                             0.0f, 0.0f, 1.0f);
        cropTransform.translate(static_cast<float>(-pivot.x()),
                                static_cast<float>(-pivot.y()), 0.0f);
    }

    drawWithClonerEffect(this, cropTransform, [renderer, img, drawRect, uvRect, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(
            static_cast<float>(drawRect.x()),
            static_cast<float>(drawRect.y()),
            static_cast<float>(drawRect.width()),
            static_cast<float>(drawRect.height()),
            transform, img, this->opacity() * weight, uvRect);
    });

    drawFractureOverlay(renderer, baseTransform, QSizeF(size.width, size.height), opacity());
}

QImage ArtifactImageLayer::toQImage() const
{
    impl_->refreshSourceVersionIfNeeded();
    if (!impl_->hasImage_) {
        return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Missing image"));
    }

    if (isImageSequence()) {
        const qint64 layerFrame =
            currentFrame() - startTime().framePosition();
        impl_->refreshSequenceFrame(layerFrame, compositionFrameRate());
    }

    const bool isMainThread = (QThread::currentThread() == qApp->thread());

    // メインスレッドのみ: 完了済みプリフェッチをキャッシュに取り込む
    // (バックグラウンドスレッドは impl_ を書かず future から直接返す)
    if (isMainThread && !impl_->prefetchDone_ && impl_->prefetchFuture_.isFinished()) {
        const auto result = impl_->prefetchFuture_.result();
        (void)impl_->adoptPrefetchResult(result);
    }

    // The viewport path consumes cacheBuffer_, which may contain the OCIO
    // input interpretation applied by applyInputInterpretation(). Convert
    // that working-space buffer at this explicit Qt compatibility boundary so
    // thumbnails and software/export consumers use the same color source.
    QImage compatibleImage;
    const auto *source = impl_->cacheBuffer_ ? impl_->cacheBuffer_.get() : nullptr;
    if (impl_->cacheBuffer_ && !impl_->cacheBuffer_->isEmpty()) {
        if (isMainThread) {
            if (impl_->cacheBufferQImageSource_ != source) {
                impl_->cacheBufferQImage_ = impl_->cacheBuffer_->toQImage();
                impl_->cacheBufferQImageSource_ = source;
            }
            compatibleImage = impl_->cacheBufferQImage_;
        } else {
            compatibleImage = impl_->cacheBuffer_->toQImage();
        }
    } else if (impl_->cache_) {
        if (isMainThread) {
            impl_->cacheBufferQImageSource_ = nullptr;
            impl_->cacheBufferQImage_ = {};
            impl_->cacheBufferCropSource_ = nullptr;
            impl_->cacheBufferCropSignature_.clear();
            impl_->cacheBufferCroppedImage_ = {};
        }
        compatibleImage = *impl_->cache_;
    } else {
        if (isMainThread) {
            impl_->cacheBufferQImageSource_ = nullptr;
            impl_->cacheBufferQImage_ = {};
            impl_->cacheBufferCropSource_ = nullptr;
            impl_->cacheBufferCropSignature_.clear();
            impl_->cacheBufferCroppedImage_ = {};
        }
    }
    if (!compatibleImage.isNull()) {
        const QImage& base = compatibleImage;
        const QRect cropRect =
            sourceCropToRect(impl_->sourceCrop_, QSize(base.width(), base.height()));
        if (cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0) {
            if (!isMainThread) {
                return makeTransparentCropCanvas(base, cropRect);
            }
            const QString cropSignature = sourceCropSignature();
            if (impl_->cacheBufferCropSource_ != source ||
                impl_->cacheBufferCropSignature_ != cropSignature) {
                impl_->cacheBufferCroppedImage_ =
                    makeTransparentCropCanvas(base, cropRect);
                impl_->cacheBufferCropSource_ = source;
                impl_->cacheBufferCropSignature_ = cropSignature;
            }
            return impl_->cacheBufferCroppedImage_;
        }
        if (isMainThread) {
            impl_->cacheBufferCropSource_ = nullptr;
            impl_->cacheBufferCropSignature_.clear();
            impl_->cacheBufferCroppedImage_ = {};
        }
        return base;
    }

    // プリフェッチがまだ完了していない場合も、UI threadでは同期decodeしない。
    if (!impl_->sourcePath_.isEmpty()) {
        if (!impl_->prefetchDone_) {
            if (!isMainThread) {
                // バックグラウンドスレッド: futureがある場合は待機して直接返す (impl_書き込み不要)
                if (impl_->prefetchFuture_.isRunning() || impl_->prefetchFuture_.isFinished()) {
                    impl_->prefetchFuture_.waitForFinished();
                    const auto result = impl_->prefetchFuture_.result();
                    const bool resultIsCurrent =
                        result.generation == impl_->prefetchGeneration_ &&
                        (result.sourceVersion == 0 ||
                         result.sourceVersion ==
                             ArtifactCore::AssetManager::instance().sourceVersion(
                                 impl_->sourceAssetId_));
                    QImage img = resultIsCurrent ? result.image : QImage();
                    if (!img.isNull()) return img;
                }
                // futureが無い / 結果がnull: バックグラウンドで同期ロード (impl_非書き込み)
                return loadImageViaOIIO(impl_->sourcePath_, nullptr, nullptr,
                                        impl_->psdSubimageIndex_);
            }
            // メインスレッド: プリフェッチ実行中はブロックせず次フレームで再試行
            if (impl_->prefetchFuture_.isRunning()) {
                return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Loading image"));
            }
            impl_->cache_ = ArtifactCore::makeShared<QImage>(
                makeMissingImagePlaceholder(
                    QSize(256, 256),
                    QStringLiteral("Image prefetch unavailable")));
            impl_->cacheBuffer_ =
                ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                    toFrameBuffer(*impl_->cache_));
            impl_->prefetchDone_ = true;
            ArtifactCore::FallbackTracker::instance()->record(
                ArtifactCore::FallbackCategory::Image,
                ArtifactCore::FallbackAction::Fallback,
                impl_->sourcePath_, "placeholder",
                QStringLiteral(
                    "Image prefetch was unavailable; synchronous UI decode suppressed"));
            qWarning() << "[ArtifactImageLayer] Prefetch unavailable; "
                          "synchronous UI decode suppressed:"
                       << impl_->sourcePath_;
        }
    }

    if (!impl_->cache_) {
        impl_->cache_ = ArtifactCore::makeShared<QImage>(
            makeMissingImagePlaceholder(
                QSize(256, 256), QStringLiteral("Image unavailable")));
        impl_->cacheBuffer_ =
            ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
                toFrameBuffer(*impl_->cache_));
        impl_->prefetchDone_ = true;
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Fallback,
            impl_->sourcePath_, "placeholder",
            QStringLiteral("No cache available, using placeholder"));
        return *impl_->cache_;
    }
    const QImage& base = *impl_->cache_;
    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(base.width(), base.height()));
    if (cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0) {
        return makeTransparentCropCanvas(base, cropRect);
    }
    return base;
}

QImage ArtifactImageLayer::getThumbnail(int width, int height) const
{
    const QSize targetSize(std::clamp(width, 1, 16384),
                           std::clamp(height, 1, 16384));
    QImage image = toQImage();
    if (image.isNull()) {
        return ArtifactAbstractLayer::getThumbnail(targetSize.width(), targetSize.height());
    }
    return image.scaled(targetSize, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
}

const ArtifactCore::ImageF32x4_RGBA& ArtifactImageLayer::currentFrameBuffer() const
{
    static ArtifactCore::ImageF32x4_RGBA empty;
    impl_->refreshSourceVersionIfNeeded();
    if (isImageSequence()) {
        // Sequence refresh is shared with the QImage path so effect/GPU
        // consumers do not keep using the previously drawn frame.
        (void)toQImage();
    }
    if (impl_ && !impl_->cacheBuffer_ && impl_->cache_) {
        impl_->cacheBuffer_ = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
        const auto version = ArtifactCore::AssetManager::instance().sourceVersion(
            impl_->sourceAssetId_);
        if (version > 0) {
            impl_->cacheBuffer_ = publishImagePayloadOrKeep(
                impl_->sourceAssetId_, version,
                imageF32RepresentationKey(impl_->effectiveInputColorSpace(),
                                          impl_->effectiveInputTransferFunction()),
                impl_->cacheBuffer_);
        }
    }
    if (impl_ && impl_->cacheBuffer_) {
        return *impl_->cacheBuffer_;
    }
    return empty;
}

bool ArtifactImageLayer::hasCurrentFrameBuffer() const
{
    impl_->refreshSourceVersionIfNeeded();
    return impl_ && ((impl_->cacheBuffer_ && !impl_->cacheBuffer_->isEmpty()) || impl_->cache_);
}

void ArtifactImageLayer::setFromQImage(const QImage& image)
{
    if (!image.isNull() &&
        !hasSupportedImageDimensions(image.width(), image.height())) {
        qWarning() << "[ArtifactImageLayer] Rejected oversized in-memory image:"
                   << image.size();
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Warning, QString(),
            "in-memory-image-size",
            QStringLiteral("Rejected in-memory image exceeding the decoded "
                           "pixel safety limit"));
        return;
    }
    // A QImage supplied by an editing tool is an in-memory result, not a new
    // decode of the current file.  Drop the old source identity so a later
    // source-version refresh cannot silently replace the edited pixels.
    ++impl_->prefetchGeneration_;
    if (!impl_->sourceAssetId_.isNull()) {
        ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
        impl_->sourceAssetId_ = QUuid();
    }
    impl_->sourcePath_.clear();
    impl_->cachedSourceVersion_ = 0;
    impl_->sequencePaths_.clear();
    impl_->sequenceFrameRate_ = 0.0;
    impl_->sequenceSource_.reset();
    impl_->sequenceCachedIndex_ = -1;
    impl_->sourceMetadata_ = {};

    if (image.isNull()) {
        impl_->hasImage_ = false;
        impl_->cache_.reset();
        impl_->resetCacheBuffer();
        impl_->width_ = 0;
        impl_->height_ = 0;
        setSourceSize(Size_2D(0, 0));
        setDirty(LayerDirtyFlag::Source);
        Q_EMIT changed();
        return;
    }

    impl_->width_ = image.width();
    impl_->height_ = image.height();
    impl_->cache_ = ArtifactCore::makeShared<QImage>(image);
    impl_->cacheBuffer_ = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(image));
    impl_->hasImage_ = true;
    impl_->sourceMetadata_.channelCount = 4;
    impl_->sourceMetadata_.alphaChannel = image.hasAlphaChannel() ? 3 : -1;
    impl_->sourceMetadata_.bitsPerChannel = 8;
    impl_->sourceMetadata_.channelNames = image.hasAlphaChannel()
        ? QStringList{QStringLiteral("R"), QStringLiteral("G"),
                      QStringLiteral("B"), QStringLiteral("A")}
        : QStringList{QStringLiteral("R"), QStringLiteral("G"),
                      QStringLiteral("B")};

    setSourceSize(Size_2D(image.width(), image.height()));
    impl_->sourceCrop_.clampToSource(QSizeF(image.width(), image.height()));
    setDirty(LayerDirtyFlag::Source);
    Q_EMIT changed();
}

void ArtifactImageLayer::setFromImageBuffer(
    const ArtifactCore::ImageF32x4_RGBA& image)
{
    if (!image.isEmpty() &&
        !hasSupportedImageDimensions(image.width(), image.height())) {
        qWarning() << "[ArtifactImageLayer] Rejected oversized in-memory buffer:"
                   << image.width() << "x" << image.height();
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Warning, QString(),
            "in-memory-buffer-size",
            QStringLiteral("Rejected in-memory buffer exceeding the decoded "
                           "pixel safety limit"));
        return;
    }
    ++impl_->prefetchGeneration_;
    if (!impl_->sourceAssetId_.isNull()) {
        ArtifactCore::AssetManager::instance().releaseSource(impl_->sourceAssetId_);
        impl_->sourceAssetId_ = QUuid();
    }
    impl_->sourcePath_.clear();
    impl_->cachedSourceVersion_ = 0;
    impl_->sequencePaths_.clear();
    impl_->sequenceFrameRate_ = 0.0;
    impl_->sequenceSource_.reset();
    impl_->sequenceCachedIndex_ = -1;
    impl_->sourceMetadata_ = {};

    if (image.isEmpty()) {
        impl_->hasImage_ = false;
        impl_->cache_.reset();
        impl_->resetCacheBuffer();
        impl_->width_ = 0;
        impl_->height_ = 0;
        setSourceSize(Size_2D(0, 0));
    } else {
        impl_->width_ = image.width();
        impl_->height_ = image.height();
        impl_->cacheBuffer_ = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
            image.DeepCopy());
        impl_->cache_ = ArtifactCore::makeShared<QImage>(image.toQImage());
        impl_->hasImage_ = true;
        impl_->sourceMetadata_.channelCount = 4;
        impl_->sourceMetadata_.alphaChannel = 3;
        impl_->sourceMetadata_.bitsPerChannel = 32;
        impl_->sourceMetadata_.channelNames = {
            QStringLiteral("R"), QStringLiteral("G"),
            QStringLiteral("B"), QStringLiteral("A")};
        setSourceSize(Size_2D(impl_->width_, impl_->height_));
        impl_->sourceCrop_.clampToSource(QSizeF(impl_->width_, impl_->height_));
    }
    setDirty(LayerDirtyFlag::Source);
    Q_EMIT changed();
}

void ArtifactImageLayer::setDepthMap(const ArtifactCore::DepthMap& depthMap)
{
    if (depthMap.isEmpty()) {
        clearDepthMap();
        return;
    }
    impl_->depthMap_ = depthMap;
    impl_->depthMapPath_.clear();
    impl_->depthMesh_ = ArtifactCore::DepthMeshGenerator::generate(
        impl_->depthMap_, impl_->depthMeshOptions_);
    impl_->depthEnabled_ = true;
    setIs3D(true);
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
}

bool ArtifactImageLayer::setDepthMapPath(const QString& path)
{
    const QString normalizedPath = path.trimmed().left(32768);
    ArtifactCore::DepthMap depthMap;
    if (normalizedPath.isEmpty() || !depthMap.load(normalizedPath)) {
        return false;
    }
    impl_->depthMapPath_ = normalizedPath;
    impl_->depthMap_ = std::move(depthMap);
    impl_->depthMesh_ = ArtifactCore::DepthMeshGenerator::generate(
        impl_->depthMap_, impl_->depthMeshOptions_);
    impl_->depthEnabled_ = true;
    setIs3D(true);
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
    return true;
}

QString ArtifactImageLayer::depthMapPath() const
{
    return impl_->depthMapPath_;
}

void ArtifactImageLayer::clearDepthMap()
{
    if (!impl_->depthEnabled_ && impl_->depthMap_.isEmpty()) return;
    impl_->depthMap_ = {};
    impl_->depthMesh_ = {};
    impl_->depthMapPath_.clear();
    impl_->depthEnabled_ = false;
    setIs3D(false);
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
}

bool ArtifactImageLayer::hasDepthMap() const
{
    return impl_->depthEnabled_ && !impl_->depthMap_.isEmpty();
}

const ArtifactCore::Mesh& ArtifactImageLayer::depthMesh() const
{
    return impl_->depthMesh_;
}

void ArtifactImageLayer::setDepthMeshOptions(
    const ArtifactCore::DepthMeshOptions& options)
{
    impl_->depthMeshOptions_ = options;
    if (hasDepthMap()) {
        impl_->depthMesh_ = ArtifactCore::DepthMeshGenerator::generate(
            impl_->depthMap_, impl_->depthMeshOptions_);
        setDirty(LayerDirtyFlag::Property);
        Q_EMIT changed();
    }
}

ArtifactCore::DepthMeshOptions ArtifactImageLayer::depthMeshOptions() const
{
    return impl_->depthMeshOptions_;
}

bool ArtifactImageLayer::applySegmentationMask(
    const ArtifactCore::DepthMap& mask, float opacity)
{
    if (mask.isEmpty() || !impl_->cacheBuffer_ || impl_->cacheBuffer_->isEmpty()) {
        return false;
    }
    auto processed = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(
        impl_->cacheBuffer_->DeepCopy());
    ArtifactCore::applySegmentationMask(*processed, mask, opacity);
    impl_->cacheBuffer_ = processed;
    impl_->cache_ = ArtifactCore::makeShared<QImage>(processed->toQImage());
    impl_->hasImage_ = true;
    setDirty(LayerDirtyFlag::Source);
    Q_EMIT changed();
    return true;
}

void ArtifactImageLayer::setFitToLayer(bool fit)
{
    if (impl_->fitToLayer_ == fit) {
        return;
    }
    impl_->fitToLayer_ = fit;
    setDirty(LayerDirtyFlag::Property);
    Q_EMIT changed();
}

bool ArtifactImageLayer::fitToLayer() const
{
    return impl_->fitToLayer_;
}

QRectF ArtifactImageLayer::localBounds() const
{
    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }
    if (size.width <= 0 || size.height <= 0) {
        return QRectF();
    }

    // Crop/Pan reframes source pixels within this fixed layer frame. Keeping
    // localBounds independent of sourceCrop prevents transform/gizmo jumps.
    return impl_->displayRectForPixels(
        QRectF(0.0, 0.0, static_cast<qreal>(size.width),
               static_cast<qreal>(size.height)));
}

} // namespace Artifact
