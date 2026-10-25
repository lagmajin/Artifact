module;

#include <QString>

module Artifact.Menu.Effect;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

bool effectMenuEventMatchesComposition(const QString& compositionId)
{
    if (auto* service = ArtifactProjectService::instance()) {
        if (const auto composition = service->currentComposition().lock()) {
            return composition->id().toString() == compositionId;
        }
    }
    return true;
}

ArtifactAbstractLayerPtr currentEffectMenuLayer(
    const ArtifactCore::LayerID& layerId)
{
    auto* service = ArtifactProjectService::instance();
    const auto composition = service
        ? service->currentComposition().lock()
        : ArtifactCompositionPtr{};
    return composition ? composition->layerById(layerId)
                       : ArtifactAbstractLayerPtr{};
}

bool effectMenuHasCurrentComposition()
{
    auto* service = ArtifactProjectService::instance();
    return service && service->hasProject() &&
           static_cast<bool>(service->currentComposition().lock());
}

} // namespace Artifact
