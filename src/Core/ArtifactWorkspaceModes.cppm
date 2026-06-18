module;
#include <QString>
#include <QList>
#include <QStringList>
#include <utility>

export module Artifact.Workspace.Modes;

import Widgets.ToolBar;

namespace Artifact {

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

module Artifact.Workspace.Modes;

namespace Artifact {

static constexpr auto kWorkspaceModeCount = 10;

WorkspaceMode workspaceModeFromText(const QString &text) {
  const QString normalized = text.trimmed().toLower();
  if (normalized == QStringLiteral("animation")) {
    return WorkspaceMode::Animation;
  }
  if (normalized == QStringLiteral("import")) {
    return WorkspaceMode::Import;
  }
  if (normalized == QStringLiteral("layout")) {
    return WorkspaceMode::Layout;
  }
  if (normalized == QStringLiteral("vfx")) {
    return WorkspaceMode::VFX;
  }
  if (normalized == QStringLiteral("compositing")) {
    return WorkspaceMode::Compositing;
  }
  if (normalized == QStringLiteral("text")) {
    return WorkspaceMode::Text;
  }
  if (normalized == QStringLiteral("export")) {
    return WorkspaceMode::Export;
  }
  if (normalized == QStringLiteral("debug")) {
    return WorkspaceMode::Debug;
  }
  if (normalized == QStringLiteral("audio")) {
    return WorkspaceMode::Audio;
  }
  return WorkspaceMode::Default;
}

QString workspaceModeText(WorkspaceMode mode) {
  switch (mode) {
  case WorkspaceMode::Default:
    return QStringLiteral("Default");
  case WorkspaceMode::Import:
    return QStringLiteral("Import");
  case WorkspaceMode::Layout:
    return QStringLiteral("Layout");
  case WorkspaceMode::Animation:
    return QStringLiteral("Animation");
  case WorkspaceMode::VFX:
    return QStringLiteral("VFX");
  case WorkspaceMode::Compositing:
    return QStringLiteral("Compositing");
  case WorkspaceMode::Text:
    return QStringLiteral("Text");
  case WorkspaceMode::Export:
    return QStringLiteral("Export");
  case WorkspaceMode::Debug:
    return QStringLiteral("Debug");
  case WorkspaceMode::Audio:
    return QStringLiteral("Audio");
  }
  return QStringLiteral("Default");
}

QStringList workspaceModeLabels() {
  QStringList labels;
  for (const auto &info : workspaceModeInfos()) {
    labels.push_back(info.label);
  }
  return labels;
}

WorkspaceModeInfo workspaceModeInfo(WorkspaceMode mode) {
  for (const auto &info : workspaceModeInfos()) {
    if (info.mode == mode) {
      return info;
    }
  }
  return workspaceModeInfos().front();
}

WorkspaceModeInfo workspaceModeInfoForKey(const QString &key) {
  const QString normalized = key.trimmed().toLower();
  for (const auto &info : workspaceModeInfos()) {
    if (info.key == normalized) {
      return info;
    }
  }
  return workspaceModeInfos().front();
}

WorkspaceModeInfo workspaceModeInfoForLabel(const QString &label) {
  const QString normalized = label.trimmed();
  for (const auto &info : workspaceModeInfos()) {
    if (info.label.compare(normalized, Qt::CaseInsensitive) == 0) {
      return info;
    }
  }
  return workspaceModeInfos().front();
}

WorkspaceModeInfo workspaceModeInfoForText(const QString &text) {
  const QString normalized = text.trimmed().toLower();
  for (const auto &info : workspaceModeInfos()) {
    if (info.key == normalized || info.label.compare(text.trimmed(), Qt::CaseInsensitive) == 0) {
      return info;
    }
  }
  return workspaceModeInfos().front();
}

QList<WorkspaceModeInfo> workspaceModeInfos() {
  return {
      {WorkspaceMode::Default, QStringLiteral("default"),
       QStringLiteral("Default"), QStringLiteral("Studio/viewmenu_workspace_default.svg")},
      {WorkspaceMode::Import, QStringLiteral("import"),
       QStringLiteral("Import"), QStringLiteral("Studio/viewmenu_workspace_import.svg")},
      {WorkspaceMode::Layout, QStringLiteral("layout"),
       QStringLiteral("Layout"), QStringLiteral("Studio/viewmenu_workspace_layout.svg")},
      {WorkspaceMode::Animation, QStringLiteral("animation"),
       QStringLiteral("Animation"), QStringLiteral("Studio/viewmenu_workspace_animation.svg")},
      {WorkspaceMode::VFX, QStringLiteral("vfx"),
       QStringLiteral("VFX"), QStringLiteral("Studio/viewmenu_workspace_vfx.svg")},
      {WorkspaceMode::Compositing, QStringLiteral("compositing"),
       QStringLiteral("Compositing"), QStringLiteral("Studio/viewmenu_workspace_compositing.svg")},
      {WorkspaceMode::Text, QStringLiteral("text"),
       QStringLiteral("Text"), QStringLiteral("Studio/viewmenu_workspace_text.svg")},
      {WorkspaceMode::Export, QStringLiteral("export"),
       QStringLiteral("Export"), QStringLiteral("Studio/viewmenu_workspace_export.svg")},
      {WorkspaceMode::Debug, QStringLiteral("debug"),
       QStringLiteral("Debug"), QStringLiteral("Studio/viewmenu_workspace_debug.svg")},
      {WorkspaceMode::Audio, QStringLiteral("audio"),
       QStringLiteral("Audio"), QStringLiteral("Studio/viewmenu_workspace_audio.svg")},
  };
}

} // namespace Artifact
