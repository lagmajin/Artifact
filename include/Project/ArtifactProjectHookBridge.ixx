module;
#include <QString>

export module Artifact.Project.HookBridge;

export namespace Artifact {

void runArtifactProjectHook(const QString& hookName, const QString& path);

} // namespace Artifact
