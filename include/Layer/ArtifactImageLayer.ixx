module;
#include <utility>
#include <cstdint>
#include <QImage>
#include <QJsonObject>
#include <QRectF>
#include <QStringList>
#include <QVariant>
#include <QUuid>
#include <wobjectimpl.h>
export module Artifact.Layer.Image;


import Artifact.Layers.Abstract._2D;

import Image;
import Image.ImageF32x4_RGBA;
import Image.DepthMap;
import Geometry.DepthMeshGenerator;
import Core.AI.ImageSegmenter;

export namespace Artifact {

 class ArtifactImageLayer:public ArtifactAbstract2DLayer {
 W_OBJECT(ArtifactImageLayer)
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactImageLayer();
  ~ArtifactImageLayer();
  QImage toQImage() const;
  QImage getThumbnail(int width = 128, int height = 128) const override;
  const ArtifactCore::ImageF32x4_RGBA& currentFrameBuffer() const;
  bool hasCurrentFrameBuffer() const;
  bool loadFromPath(const QString& path);
  // PSD/OIIO subimage index.  -1 keeps the default flattened image (subimage 0).
  void setPsdSubimageIndex(int index);
  int psdSubimageIndex() const;
  bool setImageSequence(const QStringList& framePaths, double frameRate);
  QStringList sequenceFramePaths() const;
  bool isImageSequence() const;
  double sequenceFrameRate() const;
  QString sourcePath() const;
  void setInputInterpretation(const QString& colorSpace, const QString& transferFunction);
  QString inputColorSpace() const;
  QString inputTransferFunction() const;
  QUuid sourceAssetId() const;
  std::uint64_t sourceVersion() const;
  bool canShareSourceGpuTexture() const;
  bool sourceCropEnabled() const;
  QString sourceCropSignature() const;
  bool localizeSourceIdentity();
  bool relinkSourceIdentityToShared();
  bool isSourceIdentityLocalized() const;
  QJsonObject toJson() const override;
  void fromJsonProperties(const QJsonObject& obj) override;
  void setFromQImage(const QImage& image);
  void setFromImageBuffer(const ArtifactCore::ImageF32x4_RGBA& image);
  void setDepthMap(const ArtifactCore::DepthMap& depthMap);
  bool setDepthMapPath(const QString& path);
  QString depthMapPath() const;
  void clearDepthMap();
  bool hasDepthMap() const;
  const ArtifactCore::Mesh& depthMesh() const;
  void setDepthMeshOptions(const ArtifactCore::DepthMeshOptions& options);
  ArtifactCore::DepthMeshOptions depthMeshOptions() const;
  bool applySegmentationMask(const ArtifactCore::DepthMap& mask,
                             float opacity = 1.0f);
  void setFromCvMat(const cv::Mat& mat);
  void setFromCvMat();
  void setFitToLayer(bool fit);
  bool fitToLayer() const;
  QRectF localBounds() const override;
  std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
  bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

  void draw(ArtifactIRenderer* renderer) override;
 };

}
