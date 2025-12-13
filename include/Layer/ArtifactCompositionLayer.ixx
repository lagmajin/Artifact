module;

export module Layers.Composition;

import Utils;
import Artifact.Layers;



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
 };






}
