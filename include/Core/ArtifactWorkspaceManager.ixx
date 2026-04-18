module;
#include <QString>
#include <QStringList>

export module Artifact.Workspace.Manager;

import Artifact.MainWindow;

export namespace Artifact {

class ArtifactWorkspaceManager {
public:
  explicit ArtifactWorkspaceManager(QString workspaceRoot = QString());
  ~ArtifactWorkspaceManager();

  QString workspaceRoot() const;
  QString sessionFilePath() const;
  QString presetFilePath(const QString &presetName) const;
  QStringList presetNames() const;

  bool saveSession(const ArtifactMainWindow *window) const;
  bool restoreSession(ArtifactMainWindow *window) const;
  bool savePreset(const QString &presetName, const ArtifactMainWindow *window) const;
  bool restorePreset(const QString &presetName, ArtifactMainWindow *window) const;

private:
  QString workspaceRoot_;
};

} // namespace Artifact
