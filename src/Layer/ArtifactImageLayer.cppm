module;

#include <QImage>
#include <QPainter>
#include <wobjectimpl.h>

module Artifact.Layer.Image;

import std;
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
    std::shared_ptr<QImage> cache_;
};

W_OBJECT_IMPL(ArtifactImageLayer)

ArtifactImageLayer::ArtifactImageLayer() : impl_(new Impl()) {
}

ArtifactImageLayer::~ArtifactImageLayer() {
    delete impl_;
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
        renderer->drawSprite(0.0f, 0.0f, (float)size.width, (float)size.height, img, opacity());
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
    QImage qimg = impl_->cache_->image().toQImage();
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

void ArtifactImageLayer::setImage(const QImage& image)
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

} // namespace Artifact
