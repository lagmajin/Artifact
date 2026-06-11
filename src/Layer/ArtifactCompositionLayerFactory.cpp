#include "../../include/Layer/ArtifactCompositionLayerFactory.ixx"

import Artifact.Layer.Composition;

namespace Artifact {

std::shared_ptr<ArtifactAbstractLayer> createArtifactCompositionLayer() {
  return std::make_shared<ArtifactCompositionLayer>();
}

} // namespace Artifact
