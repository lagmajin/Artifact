module;
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Script.Hooks;

import Artifact.Script.Hooks;
import Script.Python.Engine;
import Core.FastSettingsStore;

namespace Artifact {
namespace {
ArtifactCore::FastSettingsStore& hookSettingsStore()
{
 static ArtifactCore::FastSettingsStore store;
 static bool initialized = false;
 if (!initialized) {
  const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(appDataDir);
  if (!dir.exists()) {
   dir.mkpath(QStringLiteral("."));
  }
  store.open(dir.filePath(QStringLiteral("python_hooks.cbor")));
  store.setAutoSyncThreshold(4);
  initialized = true;
 }
 return store;
}
}

QStringList ArtifactPythonHookManager::knownHooks()
{
 return QStringList{
  QStringLiteral("on_startup"),
  QStringLiteral("before_project_save"),
  QStringLiteral("after_project_export"),
  QStringLiteral("on_project_save_failed"),
  QStringLiteral("project_opened"),
  QStringLiteral("project_changed"),
  QStringLiteral("composition_created"),
  QStringLiteral("layer_added"),
  QStringLiteral("layer_removed")
 };
}

QString ArtifactPythonHookManager::hookScriptPath(const QString& hookName)
{
 if (hookName.trimmed().isEmpty()) return QString();
 const QString appDir = QCoreApplication::applicationDirPath();
 const QStringList candidates = {
  QDir(appDir).filePath(QStringLiteral("scripts/hooks/%1.py").arg(hookName)),
  QDir(QDir::currentPath()).filePath(QStringLiteral("scripts/hooks/%1.py").arg(hookName))
 };
 for (const QString& c : candidates) {
  if (QFileInfo::exists(c)) return QFileInfo(c).absoluteFilePath();
 }
 return QFileInfo(candidates.first()).absoluteFilePath();
}

bool ArtifactPythonHookManager::hookScriptExists(const QString& hookName)
{
 const QString path = hookScriptPath(hookName);
 return !path.isEmpty() && QFileInfo::exists(path);
}

bool ArtifactPythonHookManager::isHookEnabled(const QString& hookName)
{
 auto& store = hookSettingsStore();
 const QString key = QStringLiteral("PythonHooks/Enabled/%1").arg(hookName);
 if (store.contains(key)) {
  return store.value(key, true).toBool();
 }

 // Backward compatibility: migrate once from QSettings.
 QSettings legacy(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
 legacy.beginGroup(QStringLiteral("PythonHooks/Enabled"));
 const bool enabled = legacy.value(hookName, true).toBool();
 legacy.endGroup();
 store.setValue(key, enabled);
 store.sync();
 return enabled;
}

void ArtifactPythonHookManager::setHookEnabled(const QString& hookName, bool enabled)
{
 auto& store = hookSettingsStore();
 const QString key = QStringLiteral("PythonHooks/Enabled/%1").arg(hookName);
 store.setValue(key, enabled);
 store.sync();
}

bool ArtifactPythonHookManager::runHook(const QString& hookName, const QStringList& args)
{
 if (!isHookEnabled(hookName)) return false;
 const QString scriptPath = hookScriptPath(hookName);
 if (scriptPath.isEmpty() || !QFileInfo::exists(scriptPath)) return false;

 auto& py = ArtifactCore::PythonEngine::instance();
 if (!py.isInitialized()) return false;

 py.setGlobalString("artifact_hook_name", hookName.toStdString());
 py.setGlobalString("artifact_hook_file", scriptPath.toStdString());
 py.setGlobalString("artifact_hook_args", args.join('|').toStdString());
 return py.executeFile(scriptPath.toStdString());
}

}
