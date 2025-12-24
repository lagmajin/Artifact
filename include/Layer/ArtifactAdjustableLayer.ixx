module;


export module Artifact.Layer.AdjustableLayer;

import Artifact.Layer.Abstract;

export namespace Artifact
{

 class ArtifactAdjustableLayer:public ArtifactAbstractLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAdjustableLayer();
  ~ArtifactAdjustableLayer();
 };


}
