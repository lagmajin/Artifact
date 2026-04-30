module;
#include <utility>

#include <QDebug>
#include <QPainter>
#include <QRectF>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QSvgRenderer>
#include <QFuture>
#include <QtConcurrent>
#include <wobjectimpl.h>
#include <Layer/ArtifactCloneEffectSupport.hpp>

module Artifact.Layer.Svg;

import std;
import Artifact.Layers.Abstract._2D;
import Thread.Helper;
import CvUtils;
import Property.Abstract;
import Property.Group;
import Artifact.Render.IRenderer;
import Size;

namespace Artifact {

namespace {
bool isSvgPath(const QString& path)
{
    return path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive);
}

QSize svgNaturalSize(const QString& path)
{
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return {};
    }

    const QSize defaultSize = renderer.defaultSize();
    if (defaultSize.isValid() && !defaultSize.isEmpty()) {
        return defaultSize;
    }

    const QRectF viewBox = renderer.viewBoxF();
    if (viewBox.isValid() && viewBox.width() > 0.0 && viewBox.height() > 0.0) {
        return viewBox.size().toSize();
    }

    return {};
}

QImage renderSvgToImage(const QString& path, const QSize& sizeHint)
{
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return QImage();
    }

    QSize renderSize = sizeHint;
    if (!renderSize.isValid() || renderSize.isEmpty()) {
        renderSize = renderer.defaultSize();
    }
    if (!renderSize.isValid() || renderSize.isEmpty()) {
        const QRectF viewBox = renderer.viewBoxF();
        if (viewBox.isValid() && viewBox.width() > 0.0 && viewBox.height() > 0.0) {
            renderSize = viewBox.size().toSize();
        }
    }
    if (!renderSize.isValid() || renderSize.isEmpty()) {
        renderSize = QSize(512, 512);
    }

    QImage img(renderSize, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    renderer.render(&painter);
    return img;
}
} // namespace

class ArtifactSvgLayer::Impl {
public:
    bool fitToLayer_ = true;
    bool loaded_ = false;
    int width_ = 0;
    int height_ = 0;
    QString sourcePath_;
    mutable std::shared_ptr<QImage> cache_;
    mutable std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> cacheBuffer_;
    mutable QFuture<QImage> prefetchFuture_;
    mutable bool prefetchDone_ = false;
};

W_OBJECT_IMPL(ArtifactSvgLayer)

ArtifactSvgLayer::ArtifactSvgLayer() : impl_(new Impl()) {}

ArtifactSvgLayer::~ArtifactSvgLayer()
{
    delete impl_;
}

bool ArtifactSvgLayer::loadFromPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || !isSvgPath(trimmed)) {
        qWarning() << "[ArtifactSvgLayer] Unsupported or empty SVG path:" << path;
        impl_->loaded_ = false;
        return false;
    }

    const QSize size = svgNaturalSize(trimmed);
    if (!size.isValid() || size.isEmpty()) {
        qWarning() << "[ArtifactSvgLayer] Failed to read SVG size:" << trimmed;
        impl_->loaded_ = false;
        return false;
    }

    impl_->sourcePath_ = trimmed;
    impl_->cache_.reset();
    impl_->cacheBuffer_.reset();
    impl_->prefetchDone_ = false;
    impl_->loaded_ = true;
    impl_->width_ = size.width();
    impl_->height_ = size.height();
    setSourceSize(Size_2D(size.width(), size.height()));

    impl_->prefetchFuture_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [trimmed, size]() -> QImage {
        ArtifactCore::ScopedThreadName threadName(
            QStringLiteral("SvgLayer/prefetch:%1").arg(QFileInfo(trimmed).fileName()));
        return renderSvgToImage(trimmed, size);
    });

    qDebug() << "[ArtifactSvgLayer] Prefetch started:" << trimmed
             << "sizeHint=" << size;
    Q_EMIT changed();
    return true;
}

QString ArtifactSvgLayer::sourcePath() const
{
    return impl_->sourcePath_;
}

bool ArtifactSvgLayer::isLoaded() const
{
    return impl_->loaded_;
}

void ArtifactSvgLayer::setFitToLayer(bool fit)
{
    impl_->fitToLayer_ = fit;
    Q_EMIT changed();
}

bool ArtifactSvgLayer::fitToLayer() const
{
    return impl_->fitToLayer_;
}

QImage ArtifactSvgLayer::toQImage() const
{
    if (!impl_->loaded_) {
        return QImage();
    }

    if (!impl_->prefetchDone_ && impl_->prefetchFuture_.isFinished()) {
        QImage loaded = impl_->prefetchFuture_.result();
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(loaded);
            impl_->cacheBuffer_.reset();
            if (impl_->width_ <= 0 || impl_->height_ <= 0) {
                impl_->width_ = loaded.width();
                impl_->height_ = loaded.height();
            }
        }
        impl_->prefetchDone_ = true;
    }

    if (!impl_->cache_ && !impl_->sourcePath_.isEmpty()) {
        if (!impl_->prefetchDone_ && impl_->prefetchFuture_.isRunning()) {
            return QImage();
        }
        const QSize size = impl_->width_ > 0 && impl_->height_ > 0
                               ? QSize(impl_->width_, impl_->height_)
                               : svgNaturalSize(impl_->sourcePath_);
        QImage loaded = renderSvgToImage(impl_->sourcePath_, size);
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(loaded);
            impl_->cacheBuffer_.reset();
            if (impl_->width_ <= 0 || impl_->height_ <= 0) {
                impl_->width_ = loaded.width();
                impl_->height_ = loaded.height();
            }
        }
        impl_->prefetchDone_ = true;
    }

    if (!impl_->cache_) {
        return QImage();
    }
    return *impl_->cache_;
}

const ArtifactCore::ImageF32x4_RGBA &ArtifactSvgLayer::currentFrameBuffer() const
{
    if (!impl_->loaded_) {
        static ArtifactCore::ImageF32x4_RGBA empty;
        return empty;
    }

    if (!impl_->cacheBuffer_ && impl_->cache_) {
        impl_->cacheBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>();
        const QImage rgba = (impl_->cache_->format() == QImage::Format_RGBA8888)
                                ? *impl_->cache_
                                : impl_->cache_->convertToFormat(QImage::Format_RGBA8888);
        if (!rgba.isNull()) {
            std::vector<std::uint8_t> packed;
            packed.reserve(static_cast<size_t>(rgba.width()) * static_cast<size_t>(rgba.height()) * 4u);
            for (int y = 0; y < rgba.height(); ++y) {
                const auto* row = rgba.constScanLine(y);
                packed.insert(packed.end(), row, row + static_cast<size_t>(rgba.width()) * 4u);
            }
            impl_->cacheBuffer_->setFromRGBA8(packed.data(), rgba.width(), rgba.height());
        }
    }

    if (impl_->cacheBuffer_) {
        return *impl_->cacheBuffer_;
    }

    static ArtifactCore::ImageF32x4_RGBA empty;
    return empty;
}

bool ArtifactSvgLayer::hasCurrentFrameBuffer() const
{
    return impl_->loaded_ && impl_->cache_ && !currentFrameBuffer().isEmpty();
}

QJsonObject ArtifactSvgLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstract2DLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Shape);
    obj["svg.sourcePath"] = impl_->sourcePath_;
    obj["svg.fitToLayer"] = impl_->fitToLayer_;
    return obj;
}

void ArtifactSvgLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
    if (obj.contains("svg.sourcePath")) {
        loadFromPath(obj.value("svg.sourcePath").toString());
    } else if (obj.contains("sourcePath")) {
        loadFromPath(obj.value("sourcePath").toString());
    }
    if (obj.contains("svg.fitToLayer")) {
        setFitToLayer(obj.value("svg.fitToLayer").toBool());
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactSvgLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup svgGroup(QStringLiteral("SVG"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type,
                           const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    svgGroup.addProperty(makeProp(QStringLiteral("svg.sourcePath"),
                                  ArtifactCore::PropertyType::String,
                                  impl_->sourcePath_, -150));
    svgGroup.addProperty(makeProp(QStringLiteral("svg.fitToLayer"),
                                  ArtifactCore::PropertyType::Boolean,
                                  impl_->fitToLayer_, -140));
    groups.push_back(svgGroup);
    return groups;
}

bool ArtifactSvgLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("svg.sourcePath") || propertyPath == QStringLiteral("sourcePath")) {
        return loadFromPath(value.toString());
    }
    if (propertyPath == QStringLiteral("svg.fitToLayer")) {
        setFitToLayer(value.toBool());
        return true;
    }
    return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactSvgLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer || !impl_->loaded_) {
        return;
    }

    const QImage img = toQImage();
    if (img.isNull()) {
        return;
    }

    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }

    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    drawWithClonerEffect(this, baseTransform, [renderer, img, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, img, this->opacity() * weight);
    });
}

QRectF ArtifactSvgLayer::localBounds() const
{
    if (!impl_->fitToLayer_ && impl_->width_ > 0 && impl_->height_ > 0) {
        return QRectF(0.0, 0.0, static_cast<qreal>(impl_->width_), static_cast<qreal>(impl_->height_));
    }

    const auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
        return QRectF();
    }
    return QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
}

} // namespace Artifact
