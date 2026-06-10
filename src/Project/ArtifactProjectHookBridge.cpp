#include "../../include/Project/ArtifactProjectHookBridge.ixx"

#include <QStringList>
import Artifact.Script.Hooks;

namespace Artifact {

void runArtifactProjectHook(const QString& hookName, const QString& path) {
  ArtifactPythonHookManager::runHook(hookName, QStringList() << path);
}

} // namespace Artifact
