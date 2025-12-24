module;

export module Artifact.Layer.Composition;

import std;
import Utils;
import Artifact.Layer.Abstract;



export namespace Artifact {

 //class ArtifactCompositionLayerPrivate;

 class ArtifactCompositionLayer:public ArtifactAbstractLayer {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactCompositionLayer();
  ~ArtifactCompositionLayer();
  CompositionID sourceCompositionId() const;
  void setCompositionId(const CompositionID& id);
 };






}
