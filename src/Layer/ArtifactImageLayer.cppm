module;
#include <utility>

#include <QDebug>
#include <QImage>
#include <QMatrix4x4>
#include <QImageReader>
#include <QPainter>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <wobjectimpl.h>
#include <Layer/ArtifactCloneEffectSupport.hpp>

module Artifact.Layer.Image;

import std;
import CvUtils;
import Artifact.Render.IRenderer;
import Image.ImageF32x4_RGBA;
import Size;

namespace Artifact {
class ArtifactImageLayer::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    bool hasImage_ = false;
    bool fitToLayer_ = true;
    int width_ = 0;
    int height_ = 0;
    QString sourcePath_;
    mutable std::shared_ptr<QImage> cache_;
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
            impl_->width_ = impl_->cache_->width();
            impl_->height_ = impl_->cache_->height();
            setSourceSize(Size_2D(impl_->width_, impl_->height_));
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
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << path
                   << "format=" << reader.format()
                   << "error=" << reader.errorString()
                   << "supported=" << QImageReader::supportedImageFormats();
        return false;
    }

    const QSize size = reader.size();
    impl_->sourcePath_ = path;
    impl_->cache_.reset();
    impl_->prefetchDone_ = false;
    impl_->hasImage_ = true;
    if (size.isValid()) {
        impl_->width_ = size.width();
        impl_->height_ = size.height();
        setSourceSize(Size_2D(size.width(), size.height()));
    }

    // [Fix 1] バックグラウンドで画像を先読みし、初回 draw() 呼び出し時の
    // メインスレッドブロックを排除する
    impl_->prefetchFuture_ = QtConcurrent::run([path]() -> QImage {
        QImageReader r(path);
        r.setAutoTransform(true);
        QImage img = r.read();
        if (!img.isNull()) {
            // GPU 転送用に RGBA8888 に変換も先側で実行しておく
            return img.convertToFormat(QImage::Format_RGBA8888);
        }
        return img;
    });
    impl_->prefetchWatcher_.setFuture(impl_->prefetchFuture_);

    qDebug() << "[ArtifactImageLayer] Prefetch started:" << path
             << "sizeHint=" << size;
    return true;
}

QString ArtifactImageLayer::sourcePath() const
{
    return impl_->sourcePath_;
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
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactImageLayer::setFromCvMat(const cv::Mat& mat)
{
    if (mat.empty()) {
        impl_->hasImage_ = false;
        impl_->cache_.reset();
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

    QImage img = toQImage();
    if (img.isNull()) return;

    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }

    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    drawWithClonerEffect(this, baseTransform, [renderer, &img, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, img,
                                        this->opacity() * weight);
    });
}

QImage ArtifactImageLayer::toQImage() const
{
    if (!impl_->hasImage_) {
        return QImage();
    }

    // [Fix 1] バックグラウンドプリフェッチが完了していればキャッシュに取り込む
    if (!impl_->prefetchDone_ && impl_->prefetchFuture_.isFinished()) {
        QImage loaded = impl_->prefetchFuture_.result();
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(std::move(loaded));
            impl_->width_  = impl_->cache_->width();
            impl_->height_ = impl_->cache_->height();
        }
        impl_->prefetchDone_ = true;
    }

    // プリフェッチがまだ完了していない場合はフォールバック（同期読み込み）
    if (!impl_->cache_ && !impl_->sourcePath_.isEmpty()) {
        if (!impl_->prefetchDone_ && impl_->prefetchFuture_.isRunning()) {
            // まだ実行中: ブロックかけずに空画像を返し、次のフレームで再試行
            return QImage();
        }
        // 非同期パスを通らなかった場合のフォールバック
        QImageReader reader(impl_->sourcePath_);
        reader.setAutoTransform(true);
        QImage loaded = reader.read();
        if (!loaded.isNull()) {
            impl_->cache_ = std::make_shared<QImage>(loaded.convertToFormat(QImage::Format_RGBA8888));
            if (impl_->width_ <= 0 || impl_->height_ <= 0) {
                impl_->width_  = loaded.width();
                impl_->height_ = loaded.height();
            }
        } else {
            qWarning() << "[ArtifactImageLayer::toQImage] Sync fallback load failed:"
                       << impl_->sourcePath_ << "error=" << reader.errorString();
            return QImage();
        }
        impl_->prefetchDone_ = true;
    }

    if (!impl_->cache_) {
        return QImage();
    }
    return *impl_->cache_;
}

void ArtifactImageLayer::setFromQImage(const QImage& image)
{
    if (image.isNull()) {
        impl_->hasImage_ = false;
        impl_->cache_ = nullptr;
        return;
    }

    impl_->width_ = image.width();
    impl_->height_ = image.height();
    impl_->cache_ = std::make_shared<QImage>(image);
    impl_->hasImage_ = true;

    setSourceSize(Size_2D(image.width(), image.height()));
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
