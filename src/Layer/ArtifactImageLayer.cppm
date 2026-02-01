module;
#include <QImage>
#include <opencv2/opencv.hpp>
module Artifact.Layer.Image;

import std;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import FloatRGBA;


namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactImageLayer::Impl
 {
 private:
 public:
  ImageF32x4RGBAWithCachePtr cache_;
  int width_ = 0;
  int height_ = 0;
  bool hasImage_ = false;

 public:
  Impl();
  ~Impl();
 };

 ArtifactImageLayer::Impl::Impl()
 {

 }

 ArtifactImageLayer::Impl::~Impl()
 {

 }

 ArtifactImageLayer::ArtifactImageLayer()
 {

 }

 ArtifactImageLayer::~ArtifactImageLayer()
 {

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
 }

 void ArtifactImageLayer::draw()
 {
    // Ensure GPU texture updated from CPU image if needed
    if (!impl_->hasImage_) return;
    impl_->cache_->UpdateGpuTextureFromCpuData();
 }

 QImage ArtifactImageLayer::toQImage() const
 {
    if (!impl_->hasImage_) return QImage();
    // Get CPU image and convert to QImage
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

};