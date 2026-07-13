module;
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QStandardPaths>
#include <QStringList>
#include <utility>

module Artifact.Workspace.Manager;

import Widgets.ToolBar;
import UI.Layout.State;

namespace Artifact {

static QString normalizedRootPath(const QString &workspaceRoot) {
  if (!workspaceRoot.isEmpty()) {
    return workspaceRoot;
  }
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return QDir(appData).filePath(QStringLiteral("Workspaces"));
}

static QString presetDirPath(const QString &workspaceRoot) {
  return QDir(normalizedRootPath(workspaceRoot)).filePath(QStringLiteral("Presets"));
}

static QString sessionPath(const QString &workspaceRoot) {
  return QDir(normalizedRootPath(workspaceRoot))
      .filePath(QStringLiteral("workspace_session.json"));
}

static QString presetPath(const QString &workspaceRoot, const QString &presetName) {
  const QString safeName =
      presetName.trimmed().isEmpty() ? QStringLiteral("Default")
                                     : presetName.trimmed();
  return QDir(presetDirPath(workspaceRoot)).filePath(safeName + QStringLiteral(".json"));
}

ArtifactWorkspaceManager::ArtifactWorkspaceManager(QString workspaceRoot)
    : workspaceRoot_(std::move(workspaceRoot)) {}

ArtifactWorkspaceManager::~ArtifactWorkspaceManager() = default;

QString ArtifactWorkspaceManager::workspaceRoot() const {
  return normalizedRootPath(workspaceRoot_);
}

QString ArtifactWorkspaceManager::sessionFilePath() const {
  return sessionPath(workspaceRoot_);
}

QString ArtifactWorkspaceManager::presetFilePath(const QString &presetName) const {
  return presetPath(workspaceRoot_, presetName);
}

bool ArtifactWorkspaceManager::presetExists(const QString &presetName) const {
  return QFileInfo::exists(presetPath(workspaceRoot_, presetName));
}

QStringList ArtifactWorkspaceManager::presetNames() const {
  QStringList names;
  QDir dir(presetDirPath(workspaceRoot_));
  if (!dir.exists()) {
    return names;
  }

  const QFileInfoList files =
      dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files,
                        QDir::Name);
  for (const QFileInfo &info : files) {
    names.push_back(info.completeBaseName());
  }
  return names;
}

static WorkspaceMode workspaceModeForWindow(const QWidget *window) {
  if (!window) {
    return WorkspaceMode::Default;
  }
  if (const auto *toolBar = window->findChild<ArtifactToolBar *>()) {
    return toolBar->workspaceMode();
  }
  return WorkspaceMode::Default;
}

static void setWorkspaceModeForWindow(QWidget *window, WorkspaceMode mode) {
  if (!window) {
    return;
  }
  if (auto *toolBar = window->findChild<ArtifactToolBar *>()) {
    toolBar->setWorkspaceMode(mode);
  }
}

static QJsonObject captureWindowState(const QWidget *window,
                                     bool includeWorkspaceMode) {
  QJsonObject json;
  if (!window) {
    return json;
  }

  ArtifactCore::UiLayoutState layout(QStringLiteral("ArtifactMainWindow"));
  layout.geometry = window->saveGeometry();
  // Qt ADS owns dock/tab/splitter/floating layout.  Do not persist
  // QMainWindow::saveState(), which can replay stale native layout state.
  layout.version = 1;

  json["layout"] = layout.toJson();
  if (includeWorkspaceMode) {
    json["workspaceMode"] = static_cast<int>(workspaceModeForWindow(window));
  }

  return json;
}

static bool applyWindowState(QWidget *window,
                             const QJsonObject &json,
                             bool applyWorkspaceMode) {
  if (!window) {
    return false;
  }

  const QJsonObject layoutJson = json.value(QStringLiteral("layout")).toObject();
  const ArtifactCore::UiLayoutState layout =
      ArtifactCore::UiLayoutState::fromJson(layoutJson);
  if (!layout.geometry.isEmpty()) {
    window->restoreGeometry(layout.geometry);
  }
  if (applyWorkspaceMode) {
    const WorkspaceMode mode = static_cast<WorkspaceMode>(
        json.value(QStringLiteral("workspaceMode")).toInt(
            static_cast<int>(WorkspaceMode::Default)));
    setWorkspaceModeForWindow(window, mode);
  }

  return true;
}

static bool writeJsonFile(const QString &path, const QJsonObject &json) {
  QDir dir = QFileInfo(path).dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
  return true;
}

static QJsonObject readJsonFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  return doc.isObject() ? doc.object() : QJsonObject{};
}

bool ArtifactWorkspaceManager::saveSession(const QWidget *window) const {
  if (!window) {
    return false;
  }
  return writeJsonFile(sessionPath(workspaceRoot_),
                       captureWindowState(window, false));
}

bool ArtifactWorkspaceManager::restoreSession(QWidget *window) const {
  if (!window) {
    return false;
  }
  const QJsonObject json = readJsonFile(sessionPath(workspaceRoot_));
  if (json.isEmpty()) {
    return false;
  }
  return applyWindowState(window, json, false);
}

bool ArtifactWorkspaceManager::savePreset(const QString &presetName,
                                          const QWidget *window) const {
  if (!window) {
    return false;
  }
  return writeJsonFile(presetPath(workspaceRoot_, presetName),
                       captureWindowState(window, true));
}

bool ArtifactWorkspaceManager::restorePreset(const QString &presetName,
                                             QWidget *window) const {
  if (!window) {
    return false;
  }
  const QJsonObject json = readJsonFile(presetPath(workspaceRoot_, presetName));
  if (json.isEmpty()) {
    return false;
  }
  return applyWindowState(window, json, true);
}

bool ArtifactWorkspaceManager::deletePreset(const QString &presetName) const {
  const QString path = presetPath(workspaceRoot_, presetName);
  if (!QFileInfo::exists(path)) {
    return false;
  }
  return QFile::remove(path);
}

bool ArtifactWorkspaceManager::renamePreset(const QString &oldName,
                                            const QString &newName) const {
  const QString oldPath = presetPath(workspaceRoot_, oldName);
  const QString newPath = presetPath(workspaceRoot_, newName);
  if (oldPath == newPath || !QFileInfo::exists(oldPath)) {
    return false;
  }
  if (QFileInfo::exists(newPath) && !QFile::remove(newPath)) {
    return false;
  }
  return QFile::rename(oldPath, newPath);
}

} // namespace Artifact
