module;
#include <QList>
#include <QString>
#include <QStringList>

export module Artifact.Workspace.Modes;

import Widgets.ToolBar;

export namespace Artifact {

export struct WorkspaceModeInfo {
  WorkspaceMode mode;
  QString key;
  QString label;
  QString iconPath;
};

export WorkspaceMode workspaceModeFromText(const QString &text);
export QString workspaceModeText(WorkspaceMode mode);
export QStringList workspaceModeLabels();
export QList<WorkspaceModeInfo> workspaceModeInfos();
export WorkspaceModeInfo workspaceModeInfo(WorkspaceMode mode);
export WorkspaceModeInfo workspaceModeInfoForKey(const QString &key);
export WorkspaceModeInfo workspaceModeInfoForLabel(const QString &label);
export WorkspaceModeInfo workspaceModeInfoForText(const QString &text);

} // namespace Artifact
