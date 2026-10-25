module;

#include <QString>

module Artifact.Widget.Dialog.RenderOutputSetting;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

bool resolveRenderMatrixComposition(
    ArtifactCore::CompositionID& compositionId,
    QString& compositionName,
    bool& projectServiceAvailable)
{
    auto* service = ArtifactProjectService::instance();
    projectServiceAvailable = service != nullptr;
    if (!service) {
        return false;
    }
    const auto composition = service->currentComposition().lock();
    if (!composition) {
        return false;
    }
    compositionId = composition->id();
    compositionName = composition->settings().compositionName().toQString();
    return true;
}

} // namespace Artifact
