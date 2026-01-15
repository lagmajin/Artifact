module;
#include <QImage>

#include <opencv2/opencv.hpp>

export module Artifact.Layer.Image;

import Artifact.Layers;

import Image;

export namespace Artifact {



 class ArtifactImageLayer:public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactImageLayer();
  ~ArtifactImageLayer();
  void toQImage() const;
  void setFromQImage(const QImage& image);
  void setFromCvMat(const cv::Mat& mat);
  void setFromCvMat();

  void draw() override;

 };




}