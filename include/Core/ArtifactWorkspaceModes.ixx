module;
#include <QList>
#include <QString>
#include <QStringList>

export module Artifact.Workspace.Modes;

export import Widgets.ToolBar;

export namespace Artifact {

struct WorkspaceModeInfo {
  WorkspaceMode mode;
  QString key;
  QString label;
  QString iconPath;
};

WorkspaceMode workspaceModeFromText(const QString &text);
QString workspaceModeText(WorkspaceMode mode);
QStringList workspaceModeLabels();
QList<WorkspaceModeInfo> workspaceModeInfos();
WorkspaceModeInfo workspaceModeInfo(WorkspaceMode mode);
WorkspaceModeInfo workspaceModeInfoForKey(const QString &key);
WorkspaceModeInfo workspaceModeInfoForLabel(const QString &label);
WorkspaceModeInfo workspaceModeInfoForText(const QString &text);

} // namespace Artifact
