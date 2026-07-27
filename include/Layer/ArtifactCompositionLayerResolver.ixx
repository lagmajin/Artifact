module;
#include <memory>
#include <QString>

export module Artifact.Layer.Composition.Resolver;

import Artifact.Composition.Abstract;
import Memory.SharedPtr;

export namespace Artifact {

SharedPtr<ArtifactAbstractComposition> resolveArtifactCompositionLayerSource(const QString& compositionId);

} // namespace Artifact
