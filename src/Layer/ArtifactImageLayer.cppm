module;
#include <QImage>
#include <opencv2/opencv.hpp>
module Artifact.Layer.Image;

import std;
import Image.ImageF32x4RGBAWithCache;


namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactImageLayer::Impl
 {
 private:
  
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

 }

 void ArtifactImageLayer::setFromCvMat(const cv::Mat& mat)
 {

 }

 void ArtifactImageLayer::draw()
 {
  throw std::logic_error("The method or operation is not implemented.");
 }

 QImage ArtifactImageLayer::toQImage() const
 {
  return QImage();
 }

};