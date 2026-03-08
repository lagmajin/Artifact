module;
#include <QImage>
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

    renderer->drawSprite(0.0f, 0.0f, (float)size.width, (float)size.height, img);
 }

 QImage ArtifactImageLayer::toQImage() const
 {
    if (!impl_->hasImage_ || !impl_->cache_) return QImage();

    auto& cpu = impl_->cache_->image();
    cv::Mat mat = cpu.toCVMat();
    cv::Mat bgr;
    if (mat.type() == CV_32FC4) {
        cv::Mat tmp;
        mat.convertTo(tmp, CV_8UC4, 255.0);
        cv::cvtColor(tmp, bgr, cv::COLOR_RGBA2BGRA);
        QImage img((uchar*)bgr.data, bgr.cols, bgr.rows, bgr.step, QImage::Format_RGBA8888);
        return img.copy();
    }
    return QImage();
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
    imageGroup.addProperty(makeProp(QStringLiteral("image.pixelWidth"), ArtifactCore::PropertyType::Integer, impl_->width_, -110));
    imageGroup.addProperty(makeProp(QStringLiteral("image.pixelHeight"), ArtifactCore::PropertyType::Integer, impl_->height_, -100));
    imageGroup.addProperty(makeProp(QStringLiteral("image.fitToLayer"), ArtifactCore::PropertyType::Boolean, impl_->fitToLayer_, -90));

    groups.push_back(imageGroup);
    return groups;
 }

 bool ArtifactImageLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
    if (propertyPath == QStringLiteral("image.fitToLayer")) {
        impl_->fitToLayer_ = value.toBool();
        Q_EMIT changed();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

};
