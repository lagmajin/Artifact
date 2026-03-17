module;
#include <QImage>
#include <QVariant>

#include <opencv2/opencv.hpp>

#include <wobjectimpl.h>

export module Artifact.Layer.Image;

import Artifact.Layers;

import Image;

export namespace Artifact {

 class ArtifactImageLayer:public ArtifactAbstractLayer {
 W_OBJECT(ArtifactImageLayer)
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactImageLayer();
  ~ArtifactImageLayer();
  QImage toQImage() const;
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  void setFromQImage(const QImage& image);
  void setFromCvMat(const cv::Mat& mat);
  void setFromCvMat();
  void setFitToLayer(bool fit);
  bool fitToLayer() const;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;
 };

}
