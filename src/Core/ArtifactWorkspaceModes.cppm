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
};

export WorkspaceMode workspaceModeFromText(const QString &text);
export QString workspaceModeText(WorkspaceMode mode);
export QStringList workspaceModeLabels();
export QList<WorkspaceModeInfo> workspaceModeInfos();

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

QList<WorkspaceModeInfo> workspaceModeInfos() {
  return {
      {WorkspaceMode::Default, QStringLiteral("default"),
       QStringLiteral("Default")},
      {WorkspaceMode::Import, QStringLiteral("import"),
       QStringLiteral("Import")},
      {WorkspaceMode::Layout, QStringLiteral("layout"),
       QStringLiteral("Layout")},
      {WorkspaceMode::Animation, QStringLiteral("animation"),
       QStringLiteral("Animation")},
      {WorkspaceMode::VFX, QStringLiteral("vfx"),
       QStringLiteral("VFX")},
      {WorkspaceMode::Compositing, QStringLiteral("compositing"),
       QStringLiteral("Compositing")},
      {WorkspaceMode::Text, QStringLiteral("text"),
       QStringLiteral("Text")},
      {WorkspaceMode::Export, QStringLiteral("export"),
       QStringLiteral("Export")},
      {WorkspaceMode::Debug, QStringLiteral("debug"),
       QStringLiteral("Debug")},
      {WorkspaceMode::Audio, QStringLiteral("audio"),
       QStringLiteral("Audio")},
  };
}

} // namespace Artifact
