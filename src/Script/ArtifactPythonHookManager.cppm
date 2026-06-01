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
#include <QJsonDocument>
module Artifact.Script.Hooks;

import Artifact.Script.Hooks;
import Script.Python.Engine;
import Core.FastSettingsStore;
import Artifact.AI.WorkspaceAutomation;

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

// ============================================================================
// Python Workspace Automation API Registration
// ============================================================================

void ArtifactPythonHookManager::registerWorkspaceAutomationPythonAPI()
{
    if (!ArtifactCore::PythonEngine::instance().isInitialized()) {
        return;
    }

    // Create artifact.workspace module with composition/layer/render methods
    ArtifactCore::PythonEngine::instance().execute(R"PYCODE(
import sys

class _WorkspaceModule:
    """Artifact workspace automation - Python API bridge."""
    pass

sys.modules['artifact.workspace'] = _WorkspaceModule()
)PYCODE");

    // Register C++ callbacks for key WorkspaceAutomation methods
    registerWorkspaceMethod("workspaceSnapshot", []() {
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod("workspaceSnapshot", {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("projectSnapshot", []() {
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod("projectSnapshot", {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("currentCompositionSnapshot", []() {
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod("currentCompositionSnapshot", {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("listCompositions", []() {
        const QVariantList comps = WorkspaceAutomation::instance().invokeMethod("listCompositions", {}).toList();
        return QString::fromUtf8(QJsonDocument::fromVariant(comps).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("listCurrentCompositionLayers", []() {
        const QVariantList layers = WorkspaceAutomation::instance().invokeMethod("listCurrentCompositionLayers", {}).toList();
        return QString::fromUtf8(QJsonDocument::fromVariant(layers).toJson(QJsonDocument::Compact));
    });

    // Layer manipulation methods (with arguments)
    registerWorkspaceMethod("selectLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("selectLayer", {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerPosition", [](const std::vector<std::string>& args) -> QString {
        double x = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 0.0;
        double y = args.size() > 2 ? QString::fromStdString(args[2]).toDouble() : 0.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerPosition", {layerId, x, y});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerScale", [](const std::vector<std::string>& args) -> QString {
        double sx = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 1.0;
        double sy = args.size() > 2 ? QString::fromStdString(args[2]).toDouble() : 1.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerScale", {layerId, sx, sy});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerRotation", [](const std::vector<std::string>& args) -> QString {
        double rot = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 0.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerRotation", {layerId, rot});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerOpacity", [](const std::vector<std::string>& args) -> QString {
        double opacity = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 100.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerOpacity", {layerId, opacity});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("addTextLayer", [](const std::vector<std::string>& args) -> QString {
        QString name = args.empty() ? QStringLiteral("Text") : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("addTextLayerToCurrentComposition", {name});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("addSolidLayer", [](const std::vector<std::string>& args) -> QString {
        QString name = args.empty() ? QStringLiteral("Solid") : QString::fromStdString(args[0]);
        int w = args.size() > 1 ? QString::fromStdString(args[1]).toInt() : 1920;
        int h = args.size() > 2 ? QString::fromStdString(args[2]).toInt() : 1080;
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("addSolidLayerToCurrentComposition", {name, w, h});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("renameLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString newName = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("renameLayerInCurrentComposition", {layerId, newName});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("removeLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("removeLayerFromCurrentComposition", {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Effect API
    registerWorkspaceMethod("addEffect", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectType = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("addLayerEffect", {layerId, effectType});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("removeEffect", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("removeLayerEffect", {layerId, effectId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setEffectParam", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        QString paramName = args.size() < 3 ? QString() : QString::fromStdString(args[2]);
        QVariant value = args.size() < 4 ? QVariant() : QVariant(QString::fromStdString(args[3]));
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerEffectParameter", {layerId, effectId, paramName, value});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Keyframe API
    registerWorkspaceMethod("setKeyframe", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString propPath = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        int frame = args.size() < 3 ? 0 : QString::fromStdString(args[2]).toInt();
        double value = args.size() < 4 ? 0.0 : QString::fromStdString(args[3]).toDouble();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setKeyframe", {layerId, propPath, frame, value});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("getKeyframes", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString propPath = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("getKeyframes", {layerId, propPath});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });

    // Playback API
    registerWorkspaceMethod("playbackStart", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("playbackStart", {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackPause", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("playbackPause", {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackStop", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("playbackStop", {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackSetFrame", [](const std::vector<std::string>& args) -> QString {
        int frame = args.empty() ? 0 : QString::fromStdString(args[0]).toInt();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("playbackSetCurrentFrame", {frame});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackGetFrame", []() -> QString {
        return QString::number(WorkspaceAutomation::instance().invokeMethod("playbackGetCurrentFrame", {}).toInt());
    });

    // Render Queue API
    registerWorkspaceMethod("renderQueueAddCurrent", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("addRenderQueueForCurrentComposition", {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("renderQueueStart", [](const std::vector<std::string>& args) -> QString {
        int index = args.empty() ? 0 : QString::fromStdString(args[0]).toInt();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("startRenderQueueAt", {index});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("exportComp", [](const std::vector<std::string>& args) -> QString {
        QString compId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString outputPath = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        QString fmt = args.size() < 3 ? QStringLiteral("mp4") : QString::fromStdString(args[2]);
        QString codec = args.size() < 4 ? QStringLiteral("h264") : QString::fromStdString(args[3]);
        int w = args.size() < 5 ? 1920 : QString::fromStdString(args[4]).toInt();
        int h = args.size() < 6 ? 1080 : QString::fromStdString(args[5]).toInt();
        double fps = args.size() < 7 ? 60.0 : QString::fromStdString(args[6]).toDouble();
        int bitrate = args.size() < 8 ? 5000 : QString::fromStdString(args[7]).toInt();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("exportComposition", 
            {compId, outputPath, fmt, codec, w, h, fps, bitrate});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });

    // Template API
    registerWorkspaceMethod("defineTemplateSlot", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString slotName = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        QString defaultValue = args.size() < 3 ? QString() : QString::fromStdString(args[2]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("defineTemplateSlot", {layerId, slotName, defaultValue});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("listTemplateSlots", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("listTemplateSlots", {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("applyTemplateVariation", [](const std::vector<std::string>& args) -> QString {
        QString variationJson = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("applyTemplateVariation", {variationJson});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Mask/Blend/Parent API
    registerWorkspaceMethod("setLayerParent", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString parentId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerParentInCurrentComposition", {layerId, parentId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("clearLayerParent", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("clearLayerParentInCurrentComposition", {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerBlendMode", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString mode = args.size() < 2 ? QStringLiteral("normal") : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("setLayerBlendModeInCurrentComposition", {layerId, mode});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Effect listing API
    registerWorkspaceMethod("listAvailableEffects", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod("listAvailableEffects", {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
};

void ArtifactPythonHookManager::registerWorkspaceMethod(const std::string& name, std::function<QString(const std::vector<std::string>&)> func)
{
    ArtifactCore::PythonEngine::instance().registerFunction(
        std::string("workspace_") + name,
        [func](const std::vector<std::string>& args) -> std::string {
            return func(args).toStdString();
        });

    ArtifactCore::PythonEngine::instance().execute(
        QString("import artifact.workspace\n"
                "artifact.workspace.%1 = workspace_%1\n")
            .arg(QString::fromStdString(name)).toStdString());
}

}
