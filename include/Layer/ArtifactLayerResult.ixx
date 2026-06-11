module;
#include <utility>
#include <QString>
export module Artifact.Layer.Result;


import Artifact.Layer.Abstract;

export namespace Artifact {

 struct ArtifactLayerResult
 {
  LayerType type;
  bool success = false;
  ArtifactAbstractLayerPtr layer;

  ArtifactLayerResult() = default;
  ArtifactLayerResult(const ArtifactLayerResult&) = default;
  ArtifactLayerResult& operator=(const ArtifactLayerResult&) = default;
  ArtifactLayerResult(ArtifactLayerResult&&) noexcept = default;
  ArtifactLayerResult& operator=(ArtifactLayerResult&&) noexcept = default;
 	
 };


};
