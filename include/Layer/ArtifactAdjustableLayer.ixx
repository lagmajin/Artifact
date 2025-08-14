module;


export module Artifact.Layers.AdjustableLayer;


export namespace Artifact
{

 class ArtifactAdjustableLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactAdjustableLayer();
  ~ArtifactAdjustableLayer();
 };


}
