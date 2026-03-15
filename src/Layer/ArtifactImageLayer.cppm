module;
#include <QImage>
#include <QFileInfo>
#include <QVariant>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Layer.Image;

import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Property.Abstract;
import Property.Group;

namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactImageLayer::Impl
 {
 public:
  ImageF32x4RGBAWithCachePtr cache_;
 int width_ = 0;
  int height_ = 0;
  bool hasImage_ = false;
  bool fitToLayer_ = true;
  QString sourcePath_;

  Impl() = default;
  ~Impl() = default;
 };

 ArtifactImageLayer::ArtifactImageLayer()
  : impl_(new Impl())
 {
 }

 ArtifactImageLayer::~ArtifactImageLayer()
 {
  delete impl_;
 }

 bool ArtifactImageLayer::loadFromPath(const QString& path)
 {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QImage image(trimmed);
    if (image.isNull()) {
        return false;
    }

    impl_->sourcePath_ = QFileInfo(trimmed).absoluteFilePath();
    setFromQImage(image);
    return true;
 }

 QString ArtifactImageLayer::sourcePath() const
 {
    return impl_->sourcePath_;
 }

 void ArtifactImageLayer::setFromQImage(const QImage& image)
 {
    if (image.isNull()) return;

    QImage img = image.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat mat(img.height(), img.width(), CV_8UC4, const_cast<uchar*>(img.bits()), img.bytesPerLine());
    setFromCvMat(mat);
 }

 void ArtifactImageLayer::setFromCvMat(const cv::Mat& mat)
 {
    if (mat.empty()) return;

    cv::Mat converted;
    if (mat.type() == CV_8UC3) cv::cvtColor(mat, converted, cv::COLOR_BGR2RGBA);
    else if (mat.type() == CV_8UC4) converted = mat.clone();
    else if (mat.type() == CV_32FC4) converted = mat.clone();
    else {
        mat.convertTo(converted, CV_32F, 1.0/255.0);
        if (converted.channels() == 3) cv::cvtColor(converted, converted, cv::COLOR_BGR2RGBA);
    }

    cv::Mat fmat;
    if (converted.type() == CV_8UC4) converted.convertTo(fmat, CV_32F, 1.0/255.0);
    else fmat = converted;

    ArtifactCore::ImageF32x4_RGBA img;
    img.setFromCVMat(fmat);
    impl_->cache_ = std::make_shared<ArtifactCore::ImageF32x4RGBAWithCache>(img);
    impl_->width_ = img.width();
    impl_->height_ = img.height();
    impl_->hasImage_ = true;

    setSourceSize(Size_2D(impl_->width_, impl_->height_));
    Q_EMIT changed();
 }

 void ArtifactImageLayer::setFromCvMat()
 {
 }

 void ArtifactImageLayer::draw(ArtifactIRenderer* renderer)
 {
    if (!impl_->hasImage_) return;

    QImage img = toQImage();
    if (img.isNull()) return;

    auto size = sourceSize();
    if (!impl_->fitToLayer_) {
        size = Size_2D(impl_->width_, impl_->height_);
    }

    // Diligent렌더러에서 drawSprite를 호출
    // 다른 렌더러 타입의 경우 동적으로 처리되어야 함
    if (renderer != nullptr) {
        renderer->drawSprite(0.0f, 0.0f, (float)size.width, (float)size.height, img);
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

 std::vector<ArtifactCore::PropertyGroup> ArtifactImageLayer::getLayerPropertyGroups() const
 {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup imageGroup(QStringLiteral("Image"));

    auto makeProp = [](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        auto p = std::make_shared<ArtifactCore::AbstractProperty>();
        p->setName(name);
        p->setType(type);
        p->setValue(value);
        p->setDisplayPriority(priority);
        return p;
    };

    imageGroup.addProperty(makeProp(QStringLiteral("image.loaded"), ArtifactCore::PropertyType::Boolean, impl_->hasImage_, -120));
    imageGroup.addProperty(makeProp(QStringLiteral("image.sourcePath"), ArtifactCore::PropertyType::String, impl_->sourcePath_, -115));
    imageGroup.addProperty(makeProp(QStringLiteral("image.pixelWidth"), ArtifactCore::PropertyType::Integer, impl_->width_, -110));
    imageGroup.addProperty(makeProp(QStringLiteral("image.pixelHeight"), ArtifactCore::PropertyType::Integer, impl_->height_, -100));
    imageGroup.addProperty(makeProp(QStringLiteral("image.fitToLayer"), ArtifactCore::PropertyType::Boolean, impl_->fitToLayer_, -90));

    groups.push_back(imageGroup);
    return groups;
 }

 bool ArtifactImageLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
    if (propertyPath == QStringLiteral("image.sourcePath")) {
        const auto path = value.toString().trimmed();
        if (path.isEmpty()) {
            return false;
        }
        const bool loaded = loadFromPath(path);
        if (loaded) {
            Q_EMIT changed();
        }
        return loaded;
    }
    if (propertyPath == QStringLiteral("image.fitToLayer")) {
        impl_->fitToLayer_ = value.toBool();
        Q_EMIT changed();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
