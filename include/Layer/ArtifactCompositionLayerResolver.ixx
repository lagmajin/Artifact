module;
#include <memory>
#include <QString>

export module Artifact.Layer.Composition.Resolver;

import Artifact.Composition.Abstract;

export namespace Artifact {

std::shared_ptr<ArtifactAbstractComposition> resolveArtifactCompositionLayerSource(const QString& compositionId);

} // namespace Artifact
