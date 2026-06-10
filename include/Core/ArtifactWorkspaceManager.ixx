module;
#include <QString>
#include <QStringList>
#include <QMainWindow>

export module Artifact.Workspace.Manager;

import Widgets.ToolBar;

export namespace Artifact {

class ArtifactWorkspaceManager {
public:
  explicit ArtifactWorkspaceManager(QString workspaceRoot = QString());
  ~ArtifactWorkspaceManager();

  QString workspaceRoot() const;
  QString sessionFilePath() const;
  QString presetFilePath(const QString &presetName) const;
  QStringList presetNames() const;
  bool presetExists(const QString &presetName) const;

  bool saveSession(const QMainWindow *window) const;
  bool restoreSession(QMainWindow *window) const;
  bool savePreset(const QString &presetName, const QMainWindow *window) const;
  bool restorePreset(const QString &presetName, QMainWindow *window) const;
  bool deletePreset(const QString &presetName) const;
  bool renamePreset(const QString &oldName, const QString &newName) const;

private:
  QString workspaceRoot_;
};

} // namespace Artifact
