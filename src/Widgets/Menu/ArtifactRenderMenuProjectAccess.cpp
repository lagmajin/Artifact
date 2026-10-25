module;

#include <QString>

module Menu.Render;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

struct RenderMenuCompositionSnapshot {
    bool hasComposition = false;
    double frameRate = 0.0;
    QString compositionId;
    QString compositionName;
};

RenderMenuCompositionSnapshot renderMenuCompositionSnapshot()
{
    RenderMenuCompositionSnapshot snapshot;
    auto* projectService = ArtifactProjectService::instance();
    const auto composition = projectService
        ? projectService->currentComposition().lock()
        : ArtifactCompositionPtr{};
    if (!composition) {
        return snapshot;
    }
    snapshot.hasComposition = true;
    snapshot.frameRate = composition->frameRate().framerate();
    snapshot.compositionId = composition->id().toQString();
    snapshot.compositionName =
        composition->settings().compositionName().toQString();
    return snapshot;
}

} // namespace Artifact
