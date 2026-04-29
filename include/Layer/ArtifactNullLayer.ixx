module;
#include <utility>

export module Artifact.Layer.Null;

import Artifact.Layers;
import Artifact.Layers.Abstract._2D;

export namespace Artifact {



 class ArtifactNullLayerSettings
 {

 };

 class ArtifactNullLayer:public ArtifactAbstract2DLayer
 {
 private:
  class Impl;
  Impl* impl_;
  ArtifactNullLayer(const ArtifactNullLayer&) = delete;
  ArtifactNullLayer& operator=(const ArtifactNullLayer&) = delete;
 public:
  ArtifactNullLayer();
  ~ArtifactNullLayer();

   void draw(ArtifactIRenderer* renderer) override;

   std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;

    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;

    QImage toQImage() const;

    QJsonObject toJson() const override;

    static std::shared_ptr<ArtifactNullLayer> fromJson(const QJsonObject& obj);

    bool isAdjustmentLayer() const override;


   bool isNullLayer() const override;

 };





};
