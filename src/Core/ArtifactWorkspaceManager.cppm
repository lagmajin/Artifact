module;
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QStandardPaths>
#include <QStringList>
#include <utility>

module Artifact.Workspace.Manager;

import Artifact.MainWindow;
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

static QJsonObject captureWindowState(const ArtifactMainWindow *window,
                                     bool includeWorkspaceMode) {
  QJsonObject json;
  if (!window) {
    return json;
  }

  ArtifactCore::UiLayoutState layout(QStringLiteral("ArtifactMainWindow"));
  layout.geometry = window->saveGeometry();
  layout.state = window->saveState();
  layout.version = 1;

  json["layout"] = layout.toJson();
  if (includeWorkspaceMode) {
    json["workspaceMode"] = static_cast<int>(window->workspaceMode());
  }

  QJsonArray docks;
  for (const QString &title : window->dockTitles()) {
    QJsonObject dock;
    dock["title"] = title;
    dock["visible"] = window->isDockVisible(title);
    docks.append(dock);
  }
  json["docks"] = docks;
  return json;
}

static bool applyWindowState(ArtifactMainWindow *window,
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
  if (!layout.state.isEmpty()) {
    window->restoreState(layout.state);
  }

  if (applyWorkspaceMode) {
    const WorkspaceMode mode = static_cast<WorkspaceMode>(
        json.value(QStringLiteral("workspaceMode")).toInt(
            static_cast<int>(WorkspaceMode::Default)));
    window->setWorkspaceMode(mode);
  }

  const QJsonArray docks = json.value(QStringLiteral("docks")).toArray();
  for (const QJsonValue &value : docks) {
    const QJsonObject dock = value.toObject();
    const QString title = dock.value(QStringLiteral("title")).toString();
    if (!title.isEmpty()) {
      window->setDockVisible(title, dock.value(QStringLiteral("visible")).toBool());
    }
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

bool ArtifactWorkspaceManager::saveSession(const ArtifactMainWindow *window) const {
  if (!window) {
    return false;
  }
  return writeJsonFile(sessionPath(workspaceRoot_),
                       captureWindowState(window, false));
}

bool ArtifactWorkspaceManager::restoreSession(ArtifactMainWindow *window) const {
  if (!window) {
    return false;
  }
  const QJsonObject json = readJsonFile(sessionPath(workspaceRoot_));
  if (json.isEmpty()) {
    return false;
  }
  const bool ok = applyWindowState(window, json, false);
  if (ok) {
    window->setDockVisible(QStringLiteral("Composition Viewer"), true);
    window->activateDock(QStringLiteral("Composition Viewer"));
  }
  return ok;
}

bool ArtifactWorkspaceManager::savePreset(const QString &presetName,
                                          const ArtifactMainWindow *window) const {
  if (!window) {
    return false;
  }
  return writeJsonFile(presetPath(workspaceRoot_, presetName),
                       captureWindowState(window, true));
}

bool ArtifactWorkspaceManager::restorePreset(const QString &presetName,
                                             ArtifactMainWindow *window) const {
  if (!window) {
    return false;
  }
  const QJsonObject json = readJsonFile(presetPath(workspaceRoot_, presetName));
  if (json.isEmpty()) {
    return false;
  }
  return applyWindowState(window, json, true);
}

} // namespace Artifact
