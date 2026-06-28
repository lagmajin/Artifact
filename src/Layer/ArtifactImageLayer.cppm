module;
#include <utility>
#include <array>
#include <algorithm>

#include <QDebug>
#include <QImage>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QFont>
#include <QFutureWatcher>
#include <QThread>
#include <QCoreApplication>
#include <QtConcurrent>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <opencv2/opencv.hpp>
#include <wobjectimpl.h>
#include <Layer/ArtifactCloneEffectSupport.hpp>

module Artifact.Layer.Image;

import std;
import Artifact.Layers.Abstract._2D;
import Thread.Helper;
import CvUtils;
import Artifact.Render.IRenderer;
import Artifact.Layer.SourceCrop;
import Core.Diagnostics.FallbackPolicy;
import Image.ImageF32x4_RGBA;
import Size;

namespace Artifact {
namespace
{
ArtifactCore::ImageF32x4_RGBA toFrameBuffer(const QImage& image)
{
    ArtifactCore::ImageF32x4_RGBA buffer;
    if (image.isNull()) {
        return buffer;
    }

    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(image, true);
    if (mat.empty()) {
        return buffer;
    }

    if (mat.type() != CV_32FC4) {
        mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
    }

    buffer.setFromCVMat(mat);
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

    QImage canvas(source.size(), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.drawImage(cropRect.topLeft(), source.copy(cropRect));
    return canvas;
}

ArtifactCore::ImageF32x4_RGBA makeTransparentCropCanvas(
    const ArtifactCore::ImageF32x4_RGBA& source, const QRect& cropRect)
{
    if (source.isEmpty() || !cropRect.isValid() || cropRect.width() <= 0 || cropRect.height() <= 0) {
        return source;
    }

    ArtifactCore::ImageF32x4_RGBA canvas;
    canvas.resize(source.width(), source.height());
    canvas.fill(ArtifactCore::FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
    for (int y = 0; y < cropRect.height(); ++y) {
        for (int x = 0; x < cropRect.width(); ++x) {
            canvas.setPixel(cropRect.x() + x, cropRect.y() + y,
                            source.getPixel(cropRect.x() + x, cropRect.y() + y));
        }
    }
    return canvas;
}

QImage loadImageViaOIIO(const QString& path, QSize* sizeOut = nullptr, QString* errorOut = nullptr)
{
    const std::string utf8Path = path.toUtf8().toStdString();

    OIIO::ImageBuf source(utf8Path);
    if (!source.read(0, 0, true, OIIO::TypeDesc::UINT8)) {
        if (errorOut) {
            *errorOut = QString::fromStdString(source.geterror());
        }
        return {};
    }

    OIIO::ImageBuf oriented = OIIO::ImageBufAlgo::reorient(source);
    const OIIO::ImageSpec& spec = oriented.spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid image dimensions or channel count.");
        }
        return {};
    }

    OIIO::ImageBuf rgba;
    if (spec.nchannels >= 4) {
        const std::array<int, 4> channelOrder{0, 1, 2, 3};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);
    } else if (spec.nchannels == 3) {
        const std::array<int, 4> channelOrder{0, 1, 2, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else if (spec.nchannels == 2) {
        const std::array<int, 4> channelOrder{0, 0, 0, 1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    } else {
        const std::array<int, 4> channelOrder{0, 0, 0, -1};
        const std::array<float, 4> channelValues{0.0f, 0.0f, 0.0f, 1.0f};
        rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, channelValues);
    }

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

    if (sizeOut) {
        *sizeOut = QSize(spec.width, spec.height);
    }
    return image;
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

    bool hasImage_ = false;
    bool fitToLayer_ = true;
    int width_ = 0;
    int height_ = 0;
    QString sourcePath_;
    SourceCrop sourceCrop_;
    mutable std::shared_ptr<QImage> cache_;
    mutable std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> cacheBuffer_;
    // [Fix 1] バックグラウンド先読み用
    mutable QFuture<QImage> prefetchFuture_;
    mutable QFutureWatcher<QImage> prefetchWatcher_;
    mutable bool prefetchDone_ = false;
};

W_OBJECT_IMPL(ArtifactImageLayer)

ArtifactImageLayer::ArtifactImageLayer() : impl_(new Impl()) {
    QObject::connect(&impl_->prefetchWatcher_, &QFutureWatcher<QImage>::finished, this, [this]() {
        if (!impl_) {
            return;
        }

        QImage loaded = impl_->prefetchWatcher_.result();
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(std::move(loaded));
            impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
            impl_->width_ = impl_->cache_->width();
            impl_->height_ = impl_->cache_->height();
            setSourceSize(Size_2D(impl_->width_, impl_->height_));
            impl_->sourceCrop_.clampToSource(QSizeF(impl_->width_, impl_->height_));
        }
        impl_->prefetchDone_ = true;
        Q_EMIT changed();
    });
}

ArtifactImageLayer::~ArtifactImageLayer() {
    delete impl_;
}

bool ArtifactImageLayer::loadFromPath(const QString& path)
{
    const std::string utf8Path = path.toUtf8().toStdString();
    OIIO::ImageBuf headerOnly(utf8Path);
    if (!headerOnly.read(0, 0, true, OIIO::TypeDesc::UINT8)) {
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << path
                   << "error=" << QString::fromStdString(headerOnly.geterror());
        impl_->hasImage_ = true;
        impl_->cache_ = std::make_shared<QImage>(makeMissingImagePlaceholder(QSize(256, 256),
                                                                             QStringLiteral("Missing image")));
        impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
        impl_->width_ = impl_->cache_->width();
        impl_->height_ = impl_->cache_->height();
        setSourceSize(Size_2D(impl_->width_, impl_->height_));
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Fallback,
            path, "placeholder",
            QStringLiteral("Image missing, using placeholder"));
        return false;
    }
    const OIIO::ImageSpec& spec = headerOnly.spec();
    if (spec.width <= 0 || spec.height <= 0) {
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << path
                   << "error=" << QString::fromStdString(headerOnly.geterror());
        impl_->hasImage_ = true;
        impl_->cache_ = std::make_shared<QImage>(makeMissingImagePlaceholder(QSize(256, 256),
                                                                             QStringLiteral("Missing image")));
        impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
        impl_->width_ = impl_->cache_->width();
        impl_->height_ = impl_->cache_->height();
        setSourceSize(Size_2D(impl_->width_, impl_->height_));
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Fallback,
            path, "placeholder",
            QStringLiteral("Image missing, using placeholder"));
        return false;
    }

    impl_->sourcePath_ = path;
    impl_->cache_.reset();
    impl_->cacheBuffer_.reset();
    impl_->prefetchDone_ = false;
    impl_->hasImage_ = true;
    impl_->width_ = spec.width;
    impl_->height_ = spec.height;
    setSourceSize(Size_2D(spec.width, spec.height));

    // [Fix 1] OIIO 経由でバックグラウンド先読みし、初回 draw() 呼び出し時の
    // メインスレッドブロックを排除する
    impl_->prefetchFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [path]() -> QImage {
        ArtifactCore::ScopedThreadName threadName(
            QStringLiteral("ImageLayer/prefetch:%1").arg(QFileInfo(path).fileName()));
        return loadImageViaOIIO(path);
    });
    impl_->prefetchWatcher_.setFuture(impl_->prefetchFuture_);
    impl_->sourceCrop_.clampToSource(QSizeF(impl_->width_, impl_->height_));

    qDebug() << "[ArtifactImageLayer] OIIO prefetch started:" << path
             << "sizeHint=" << QSize(spec.width, spec.height);
    return true;
}

QString ArtifactImageLayer::sourcePath() const
{
    return impl_->sourcePath_;
}

QJsonObject ArtifactImageLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstract2DLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Image);
    obj["image.sourcePath"] = impl_->sourcePath_;
    obj["image.fitToLayer"] = impl_->fitToLayer_;
    obj["image.width"] = impl_->width_;
    obj["image.height"] = impl_->height_;
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
    imageGroup.addProperty(makeProp(QStringLiteral("image.fitToLayer"),
                                    ArtifactCore::PropertyType::Boolean,
                                    impl_->fitToLayer_, -140));

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
    if (propertyPath == QStringLiteral("image.sourcePath") || propertyPath == QStringLiteral("sourcePath")) {
        return loadFromPath(value.toString());
    }
    if (propertyPath == QStringLiteral("image.fitToLayer")) {
        setFitToLayer(value.toBool());
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.enabled")) {
        impl_->sourceCrop_.setEnabled(value.toBool());
        setDirty(LayerDirtyFlag::Property);
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
            rect.moveLeft(value.toDouble());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropY")) {
            rect.moveTop(value.toDouble());
        } else if (propertyPath == QStringLiteral("sourceCrop.cropWidth")) {
            rect.setWidth(std::max(1.0, value.toDouble()));
        } else {
            rect.setHeight(std::max(1.0, value.toDouble()));
        }
        impl_->sourceCrop_.setCropRect(rect);
        impl_->sourceCrop_.clampToSource(QSizeF(size.width, size.height));
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.panX") ||
        propertyPath == QStringLiteral("sourceCrop.panY")) {
        QPointF pan = impl_->sourceCrop_.pan();
        if (propertyPath == QStringLiteral("sourceCrop.panX")) {
            pan.setX(value.toDouble());
        } else {
            pan.setY(value.toDouble());
        }
        impl_->sourceCrop_.setPan(pan);
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.zoom")) {
        impl_->sourceCrop_.setZoom(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.rotation")) {
        impl_->sourceCrop_.setRotation(value.toDouble());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.anchorX") ||
        propertyPath == QStringLiteral("sourceCrop.anchorY")) {
        QPointF anchor = impl_->sourceCrop_.anchor();
        if (propertyPath == QStringLiteral("sourceCrop.anchorX")) {
            anchor.setX(value.toDouble());
        } else {
            anchor.setY(value.toDouble());
        }
        impl_->sourceCrop_.setAnchor(anchor);
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    if (propertyPath == QStringLiteral("sourceCrop.preserveAspect")) {
        impl_->sourceCrop_.setPreserveAspect(value.toBool());
        setDirty(LayerDirtyFlag::Property);
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactImageLayer::setFromCvMat(const cv::Mat& mat)
{
    if (mat.empty()) {
        impl_->hasImage_ = false;
        impl_->cache_.reset();
        impl_->cacheBuffer_.reset();
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

    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }

    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(size.width, size.height));
    const bool useCrop = cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0;

    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    if (hasCurrentFrameBuffer()) {
        const ArtifactCore::ImageF32x4_RGBA& buffer = currentFrameBuffer();
        const ArtifactCore::ImageF32x4_RGBA renderBuffer =
            useCrop ? makeTransparentCropCanvas(buffer, cropRect) : buffer;
        drawWithClonerEffect(this, baseTransform, [renderer, renderBuffer, size, this](const QMatrix4x4& transform, float weight) {
            renderer->drawSpriteTransformed(0.0f, 0.0f,
                                            static_cast<float>(size.width),
                                            static_cast<float>(size.height),
                                            transform, renderBuffer,
                                            this->opacity() * weight);
        });
        return;
    }

    QImage img = toQImage();
    if (img.isNull()) return;
    if (useCrop) {
        img = makeTransparentCropCanvas(img, cropRect);
    }

    drawWithClonerEffect(this, baseTransform, [renderer, img, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, img,
                                        this->opacity() * weight);
    });

    drawFractureOverlay(renderer, baseTransform, QSizeF(size.width, size.height), opacity());
}

QImage ArtifactImageLayer::toQImage() const
{
    if (!impl_->hasImage_) {
        return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Missing image"));
    }

    const bool isMainThread = (QThread::currentThread() == qApp->thread());

    // メインスレッドのみ: 完了済みプリフェッチをキャッシュに取り込む
    // (バックグラウンドスレッドは impl_ を書かず future から直接返す)
    if (isMainThread && !impl_->prefetchDone_ && impl_->prefetchFuture_.isFinished()) {
        QImage loaded = impl_->prefetchFuture_.result();
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(std::move(loaded));
            impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
            impl_->width_  = impl_->cache_->width();
            impl_->height_ = impl_->cache_->height();
        }
        impl_->prefetchDone_ = true;
    }

    // キャッシュが既にある場合はそのまま返す
    if (impl_->cache_) {
        return *impl_->cache_;
    }

    // プリフェッチがまだ完了していない場合はフォールバック（同期読み込み）
    if (!impl_->sourcePath_.isEmpty()) {
        if (!impl_->prefetchDone_) {
            if (!isMainThread) {
                // バックグラウンドスレッド: futureがある場合は待機して直接返す (impl_書き込み不要)
                if (impl_->prefetchFuture_.isRunning() || impl_->prefetchFuture_.isFinished()) {
                    impl_->prefetchFuture_.waitForFinished();
                    QImage img = impl_->prefetchFuture_.result();
                    if (!img.isNull()) return img;
                }
                // futureが無い / 結果がnull: バックグラウンドで同期ロード (impl_非書き込み)
                return loadImageViaOIIO(impl_->sourcePath_);
            }
            // メインスレッド: プリフェッチ実行中はブロックせず次フレームで再試行
            if (impl_->prefetchFuture_.isRunning()) {
                return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Loading image"));
            }
            // 非同期パスを通らなかった場合のフォールバック (メインスレッドのみ impl_書き込み)
            QSize size;
            QString errorString;
            QImage loaded = loadImageViaOIIO(impl_->sourcePath_, &size, &errorString);
            if (!loaded.isNull()) {
                impl_->cache_ = std::make_shared<QImage>(loaded);
                impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(loaded));
                if (size.isValid()) {
                    impl_->width_  = size.width();
                    impl_->height_ = size.height();
                }
            } else {
                qWarning() << "[ArtifactImageLayer::toQImage] Sync fallback load failed:"
                           << impl_->sourcePath_ << "error=" << errorString;
                ArtifactCore::FallbackTracker::instance()->record(
                    ArtifactCore::FallbackCategory::Image,
                    ArtifactCore::FallbackAction::Fallback,
                    impl_->sourcePath_, "placeholder",
                    QStringLiteral("Sync fallback load failed, using placeholder"));
                return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Missing image"));
            }
            impl_->prefetchDone_ = true;
        }
    }

    if (!impl_->cache_) {
        ArtifactCore::FallbackTracker::instance()->record(
            ArtifactCore::FallbackCategory::Image,
            ArtifactCore::FallbackAction::Fallback,
            impl_->sourcePath_, "placeholder",
            QStringLiteral("No cache available, using placeholder"));
        return makeMissingImagePlaceholder(QSize(256, 256), QStringLiteral("Missing image"));
    }
    const QImage& base = *impl_->cache_;
    const QRect cropRect = sourceCropToRect(impl_->sourceCrop_, QSize(base.width(), base.height()));
    if (cropRect.isValid() && cropRect.width() > 0 && cropRect.height() > 0) {
        return makeTransparentCropCanvas(base, cropRect);
    }
    return base;
}

const ArtifactCore::ImageF32x4_RGBA& ArtifactImageLayer::currentFrameBuffer() const
{
    static ArtifactCore::ImageF32x4_RGBA empty;
    if (impl_ && !impl_->cacheBuffer_ && impl_->cache_) {
        impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(*impl_->cache_));
    }
    if (impl_ && impl_->cacheBuffer_) {
        return *impl_->cacheBuffer_;
    }
    return empty;
}

bool ArtifactImageLayer::hasCurrentFrameBuffer() const
{
    return impl_ && ((impl_->cacheBuffer_ && !impl_->cacheBuffer_->isEmpty()) || impl_->cache_);
}

void ArtifactImageLayer::setFromQImage(const QImage& image)
{
    if (image.isNull()) {
        impl_->hasImage_ = false;
        impl_->cache_ = nullptr;
        impl_->cacheBuffer_.reset();
        return;
    }

    impl_->width_ = image.width();
    impl_->height_ = image.height();
    impl_->cache_ = std::make_shared<QImage>(image);
    impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(toFrameBuffer(image));
    impl_->hasImage_ = true;

    setSourceSize(Size_2D(image.width(), image.height()));
    impl_->sourceCrop_.clampToSource(QSizeF(image.width(), image.height()));
}

void ArtifactImageLayer::setFitToLayer(bool fit)
{
    impl_->fitToLayer_ = fit;
}

bool ArtifactImageLayer::fitToLayer() const
{
    return impl_->fitToLayer_;
}

QRectF ArtifactImageLayer::localBounds() const
{
    const auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
        return QRectF();
    }

    if (!impl_->fitToLayer_ && impl_->width_ > 0 && impl_->height_ > 0) {
        return QRectF(0.0, 0.0, static_cast<qreal>(impl_->width_), static_cast<qreal>(impl_->height_));
    }

    return QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
}

} // namespace Artifact
