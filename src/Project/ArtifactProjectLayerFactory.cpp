module;

module Artifact.Project;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;

namespace Artifact {

ArtifactLayerResult createProjectLayer(ArtifactLayerInitParams& params) {
  ArtifactLayerFactory factory;
  return factory.createLayer(params);
}

} // namespace Artifact
