module;

export module Artifact.Layer.Null;

import Artifact.Layers;

export namespace Artifact {



 class ArtifactNullLayerSettings
 {

 };

 class ArtifactNullLayer:public ArtifactAbstractLayer
 {
 private:
  class Impl;
  Impl* impl_;
  ArtifactNullLayer(const ArtifactNullLayer&) = delete;
  ArtifactNullLayer& operator=(const ArtifactNullLayer&) = delete;
 public:
  ArtifactNullLayer();
  ~ArtifactNullLayer();

  void draw() override;


  bool isAdjustmentLayer() const override;


  bool isNullLayer() const override;

 };





};