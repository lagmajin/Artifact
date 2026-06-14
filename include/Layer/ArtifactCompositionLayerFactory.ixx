module;
#include <memory>

export module Artifact.Layer.Composition.Factory;

import Artifact.Layer.Abstract;

export namespace Artifact {

std::shared_ptr<ArtifactAbstractLayer> createArtifactCompositionLayer();

} // namespace Artifact
