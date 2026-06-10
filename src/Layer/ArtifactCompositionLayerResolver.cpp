#include "../../include/Layer/ArtifactCompositionLayerResolver.ixx"

import Artifact.Composition.Abstract;
import Artifact.Service.Project;

namespace Artifact {

std::shared_ptr<ArtifactAbstractComposition> resolveArtifactCompositionLayerSource(const QString& compositionId) {
  auto *service = ArtifactProjectService::instance();
  if (!service) {
    return nullptr;
  }
  const auto result = service->findComposition(CompositionID(compositionId));
  return result.ptr.lock();
}

} // namespace Artifact
