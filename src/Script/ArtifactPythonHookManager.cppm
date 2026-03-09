module;
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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

namespace Artifact {

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
 QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
 settings.beginGroup(QStringLiteral("PythonHooks/Enabled"));
 const bool enabled = settings.value(hookName, true).toBool();
 settings.endGroup();
 return enabled;
}

void ArtifactPythonHookManager::setHookEnabled(const QString& hookName, bool enabled)
{
 QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
 settings.beginGroup(QStringLiteral("PythonHooks/Enabled"));
 settings.setValue(hookName, enabled);
 settings.endGroup();
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
