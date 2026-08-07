module;

#include <QStringList>

export module Artifact.Application.InteractiveShell;

export namespace Artifact {

struct InteractiveShellResult {
  int exitCode = 0;
  bool quitRequested = false;
};

[[nodiscard]] InteractiveShellResult runInteractiveShell(
    const QStringList& projectPaths,
    const QString& scriptPath = {});

} // namespace Artifact
