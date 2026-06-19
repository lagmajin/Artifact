module;
#include <utility>

export module Artifact.Layer.Result;

import Artifact.Layer.Abstract;
import Utils.Result;

export namespace Artifact {

struct ArtifactLayerResult {
  LayerType type{};
  bool success = false;
  Status status{};
  ArtifactAbstractLayerPtr layer{};

  ArtifactLayerResult() = default;
  ArtifactLayerResult(const ArtifactLayerResult&) = default;
  ArtifactLayerResult& operator=(const ArtifactLayerResult&) = default;
  ArtifactLayerResult(ArtifactLayerResult&&) noexcept = default;
  ArtifactLayerResult& operator=(ArtifactLayerResult&&) noexcept = default;
};

}
