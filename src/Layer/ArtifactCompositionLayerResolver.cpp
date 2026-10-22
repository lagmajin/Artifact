module;
#include <memory>
#include <QString>

module Artifact.Layer.Composition.Resolver;

import Artifact.Composition.Abstract;
import Artifact.Service.Project;
import Memory.SharedPtr;

namespace Artifact {

SharedPtr<ArtifactAbstractComposition> resolveArtifactCompositionLayerSource(const QString& compositionId) {
  auto *service = ArtifactProjectService::instance();
  if (!service) {
    return nullptr;
  }
  const auto result = service->findComposition(CompositionID(compositionId));
  return result.ptr.lock();
}

} // namespace Artifact
