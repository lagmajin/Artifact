﻿module;
#include <QImage>

#include <opencv2/opencv.hpp>

export module Layers.Image;

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
  void setFromQImage(const QImage& image);
  void setFromCvMat();
 };




}