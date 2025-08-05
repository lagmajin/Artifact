module;

export module Artifact.Layers.Null;

import Artifact.Layers;

namespace Artifact {

 class ArtifactNullLayer:public ArtifactAbstractLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactNullLayer();
  ~ArtifactNullLayer();
 };





};