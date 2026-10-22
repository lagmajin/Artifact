module;
#include <memory>

module Artifact.Layer.Composition.Factory;

import Artifact.Layer.Composition;
import Memory.SharedPtr;

namespace Artifact {

SharedPtr<ArtifactAbstractLayer> createArtifactCompositionLayer() {
  return makeShared<ArtifactCompositionLayer>();
}

} // namespace Artifact
