module;

#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <wobjectimpl.h>

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
    std::shared_ptr<QImage> cache_;
};

W_OBJECT_IMPL(ArtifactImageLayer)

ArtifactImageLayer::ArtifactImageLayer() : impl_(new Impl()) {
}

ArtifactImageLayer::~ArtifactImageLayer() {
    delete impl_;
}

bool ArtifactImageLayer::loadFromPath(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "[ArtifactImageLayer] Failed to load image from:" << path
                   << "format=" << reader.format()
                   << "error=" << reader.errorString()
                   << "supported=" << QImageReader::supportedImageFormats();
        return false;
    }

    impl_->sourcePath_ = path;
    qDebug() << "[ArtifactImageLayer] Loaded image:" << path
             << "format=" << reader.format()
             << "size=" << image.size()
             << "depth=" << image.depth()
             << "bytesPerLine=" << image.bytesPerLine();
    setFromQImage(image);
    return true;
}

QString ArtifactImageLayer::sourcePath() const
{
    return impl_->sourcePath_;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactImageLayer::getLayerPropertyGroups() const
{
    std::vector<ArtifactCore::PropertyGroup> groups;
    
    ArtifactCore::PropertyGroup group("Image Layer");
    // TODO: プロパティの追加
    
    groups.push_back(group);
    
    // Base class groups (Transform, etc.)
    auto baseGroups = ArtifactAbstractLayer::getLayerPropertyGroups();
    groups.insert(groups.end(), baseGroups.begin(), baseGroups.end());
    
    return groups;
}

bool ArtifactImageLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "sourcePath") {
        return loadFromPath(value.toString());
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

    // Diligent レンデラーで drawSprite を呼び出し
    // 他のレンデラータイプの場合は動的に処理されるべき
    if (renderer != nullptr) {
        renderer->drawSprite(0.0f, 0.0f, (float)size.width, (float)size.height, img, this->opacity());
    }
}

QImage ArtifactImageLayer::toQImage() const
{
    if (!impl_->hasImage_ || !impl_->cache_) {
        qDebug() << "[ArtifactImageLayer::toQImage] No cache: hasImage=" << impl_->hasImage_
                 << "cache=" << (impl_->cache_ ? "valid" : "null");
        return QImage();
    }

    // キャッシュから QImage を生成
    QImage qimg = *impl_->cache_;
    if (!qimg.isNull()) {
        return qimg;
    }

    // フォールバック：キャッシュが破損している場合
    qDebug() << "[ArtifactImageLayer::toQImage] Cache returned null, using fallback:"
             << "size=" << impl_->width_ << "x" << impl_->height_;
    // 空の画像を返す代わりに、エラー画像を生成
    QImage errorImg(impl_->width_, impl_->height_, QImage::Format_ARGB32_Premultiplied);
    errorImg.fill(QColor(100, 50, 50));
    QPainter p(&errorImg);
    p.setPen(QColor(255, 100, 100));
    p.drawText(errorImg.rect(), Qt::AlignCenter, QStringLiteral("Cache Error"));
    return errorImg;
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
