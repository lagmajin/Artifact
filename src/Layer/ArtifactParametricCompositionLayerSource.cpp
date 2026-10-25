module;

module Artifact.Layer.ParametricComposition;

import Artifact.Composition.Abstract;
import Artifact.Service.Project;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

SharedPtr<ArtifactAbstractComposition> resolveParametricSourceComposition(
    const CompositionID& compositionId)
{
    auto* service = ArtifactProjectService::instance();
    if (!service || compositionId.isNil()) {
        return nullptr;
    }
    const auto result = service->findComposition(compositionId);
    return result.ptr.lock();
}

} // namespace Artifact
