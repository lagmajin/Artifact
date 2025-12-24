module;
#include <QString>

export module Artifact.Layer.Result;

import Artifact.Layer.Abstract;

export namespace Artifact {

 struct ArtifactLayerResult
 {
  LayerType type;
  bool success = false;
  ArtifactAbstractLayerPtr layer;
 	
 };


};