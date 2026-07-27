module;
#include <memory>

export module Artifact.Layer.Composition.Factory;

import Artifact.Layer.Abstract;
import Memory.SharedPtr;

export namespace Artifact {

SharedPtr<ArtifactAbstractLayer> createArtifactCompositionLayer();

} // namespace Artifact
