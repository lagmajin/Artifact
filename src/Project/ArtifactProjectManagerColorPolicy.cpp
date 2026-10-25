module;

module Artifact.Project.Manager;

import Artifact.Color.OCIOManager;

namespace Artifact {

void prepareColorPolicyForNewProject()
{
    if (auto* ocio = ArtifactOCIOManager::instance()) {
        ocio->setGeneratedColorPolicy(
            GeneratedColorPolicy::ConvertToWorkingSpace);
    }
}

} // namespace Artifact
