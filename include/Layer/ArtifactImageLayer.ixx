module;



export module Layers.Image;

import Artifact.Layers;

export namespace Artifact {



 class ArtifactImageLayer:public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactImageLayer();
  ~ArtifactImageLayer();
 };




}