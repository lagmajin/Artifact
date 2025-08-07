module;

export module Artifact.Layers.Null;

import Artifact.Layers;

namespace Artifact {



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
 };





};