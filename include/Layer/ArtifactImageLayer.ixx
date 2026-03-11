module;
#include <QImage>
#include <QVariant>

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
  QImage toQImage() const;
  bool loadFromPath(const QString& path);
  QString sourcePath() const;
  void setFromQImage(const QImage& image);
  void setFromCvMat(const cv::Mat& mat);
  void setFromCvMat();
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;
 };

}
