module;
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>

#include <iostream>
#include <cstddef>
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
#include <QVariantMap>
module Artifact.Script.Hooks;

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
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("workspaceSnapshot"), {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("agentPreflight", []() {
        const QVariantMap preflight = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("agentPreflight"), {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(preflight).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("workspaceDiagnostics", []() {
        const QVariantMap diagnostics = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("workspaceDiagnostics"), {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(diagnostics).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("agentContract", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("agentContract"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("commandVocabulary", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("commandVocabulary"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("selectionSnapshot", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("selectionSnapshot"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("get_selected_layers", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("get_selected_layers"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("renderQueueSnapshot", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("renderQueueSnapshot"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("get_render_queue_summary", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("get_render_queue_summary"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("projectSnapshot", []() {
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("projectSnapshot"), {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("currentCompositionSnapshot", []() {
        const QVariantMap snap = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("currentCompositionSnapshot"), {}).toMap();
        return QString::fromUtf8(QJsonDocument::fromVariant(snap).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("get_project_overview", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("get_project_overview"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("get_active_composition", []() {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("get_active_composition"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("currentCompositionThumbnailAtFrame", [](const std::vector<std::string>& args) -> QString {
        int frame = args.size() > 0 ? QString::fromStdString(args[0]).toInt() : 0;
        int width = args.size() > 1 ? QString::fromStdString(args[1]).toInt() : 256;
        int height = args.size() > 2 ? QString::fromStdString(args[2]).toInt() : 144;
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(
            QStringLiteral("currentCompositionThumbnailAtFrame"), {frame, width, height});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("listCompositions", []() {
        const QVariantList comps = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("listCompositions"), {}).toList();
        return QString::fromUtf8(QJsonDocument::fromVariant(comps).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("listCurrentCompositionLayers", []() {
        const QVariantList layers = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("listCurrentCompositionLayers"), {}).toList();
        return QString::fromUtf8(QJsonDocument::fromVariant(layers).toJson(QJsonDocument::Compact));
    });

    // Read-only discovery and verification methods. Keep these on the same
    // JSON bridge as the existing workspace snapshots so Python hooks can
    // inspect state without reimplementing C++-side lookup rules.
    const auto jsonWorkspaceResult = [](QStringView method,
                                        const QVariantList& args) -> QString {
        const QVariant result =
            WorkspaceAutomation::instance().invokeMethod(method, args);
        return QString::fromUtf8(
            QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    };
    const auto workspaceStringArg = [](const std::vector<std::string>& args,
                                       const std::size_t index) -> QString {
        return index < args.size() ? QString::fromStdString(args[index]) : QString{};
    };
    const auto workspaceIntArg = [](const std::vector<std::string>& args,
                                    const std::size_t index,
                                    const int fallback = 0) -> int {
        return index < args.size()
                   ? QString::fromStdString(args[index]).toInt()
                   : fallback;
    };
    const auto workspaceJsonMapArg =
        [](const std::vector<std::string>& args,
           const std::size_t index) -> QVariantMap {
        if (index >= args.size()) {
            return {};
        }
        const QJsonDocument document = QJsonDocument::fromJson(
            QString::fromStdString(args[index]).toUtf8());
        return document.isObject() ? document.object().toVariantMap()
                                   : QVariantMap{};
    };

    registerWorkspaceMethod("safeWriteAuditLogSnapshot",
                            [jsonWorkspaceResult]() {
                                return jsonWorkspaceResult(
                                    QStringLiteral("safeWriteAuditLogSnapshot"), {});
                            });
    registerWorkspaceMethod("getViewportSettings", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("getViewportSettings"), {});
    });
    registerWorkspaceMethod("describeViewportSettings", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("describeViewportSettings"), {});
    });
    registerWorkspaceMethod("listProjectItems", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("listProjectItems"), {});
    });
    registerWorkspaceMethod("listRenderQueueJobs", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("listRenderQueueJobs"), {});
    });
    registerWorkspaceMethod("getEffectRegistryMetadata", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("getEffectRegistryMetadata"), {});
    });
    registerWorkspaceMethod("getPlaybackAudioDiagnostics", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("getPlaybackAudioDiagnostics"), {});
    });
    registerWorkspaceMethod("getSupportedExportFormats", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("getSupportedExportFormats"), {});
    });
    registerWorkspaceMethod("playbackGetState", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetState"), {});
    });
    registerWorkspaceMethod("playbackGetCurrentFrame", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetCurrentFrame"), {});
    });
    registerWorkspaceMethod("playbackGetDuration", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetDuration"), {});
    });
    registerWorkspaceMethod("playbackGetFrameRange", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetFrameRange"), {});
    });
    registerWorkspaceMethod("playbackGetFrameRate", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetFrameRate"), {});
    });
    registerWorkspaceMethod("playbackGetSpeed", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetSpeed"), {});
    });
    registerWorkspaceMethod("playbackGetLooping", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("playbackGetLooping"), {});
    });
    registerWorkspaceMethod("getLayerPosition",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerPosition"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerScale",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerScale"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerRotation",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerRotation"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerOpacity",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerOpacity"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerEffects",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerEffects"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerEffectParameters",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerEffectParameters"),
                                    {workspaceStringArg(args, 0),
                                     workspaceStringArg(args, 1)});
                            });
    registerWorkspaceMethod("getLayerEffectParameterKeyframes",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerEffectParameterKeyframes"),
                                    {workspaceStringArg(args, 0),
                                     workspaceStringArg(args, 1),
                                     workspaceStringArg(args, 2)});
                            });
    registerWorkspaceMethod("getLayerKeyframeSummary",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerKeyframeSummary"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getLayerNote",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getLayerNote"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getCompositionNote",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getCompositionNote"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getAudioDeClickRanges",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getAudioDeClickRanges"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getAudioDeClickSettings",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getAudioDeClickSettings"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getAudioLayerTrim",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getAudioLayerTrim"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getAudioLayerPlaybackRate",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getAudioLayerPlaybackRate"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("getDefaultCodecForFormat",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("getDefaultCodecForFormat"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("findProjectItemById",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("findProjectItemById"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("projectItemPathById",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("projectItemPathById"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("renderQueueJobByIndex",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("renderQueueJobByIndex"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("renderQueueJobById",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("renderQueueJobById"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("renderQueueJobStatusAt",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("renderQueueJobStatusAt"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("renderQueueJobProgressAt",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("renderQueueJobProgressAt"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("renderQueueJobErrorMessageAt",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("renderQueueJobErrorMessageAt"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("getRenderQueueJobSelectiveSettingsAt",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral(
                                        "getRenderQueueJobSelectiveSettingsAt"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("listLayerEffectPresets",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("listLayerEffectPresets"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("recentLayerEffectPresets",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("recentLayerEffectPresets"),
                                    {workspaceIntArg(args, 0, 20)});
                            });
    registerWorkspaceMethod("validateViewportSettings",
                            [jsonWorkspaceResult, workspaceJsonMapArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("validateViewportSettings"),
                                    {workspaceJsonMapArg(args, 0)});
                            });
    registerWorkspaceMethod("validateCommand",
                            [jsonWorkspaceResult, workspaceJsonMapArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("validateCommand"),
                                    {workspaceJsonMapArg(args, 0)});
                            });
    registerWorkspaceMethod("dryRunRemoveAllAssets", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(QStringLiteral("dryRunRemoveAllAssets"), {});
    });
    registerWorkspaceMethod("dryRunRemoveAllRenderQueues", [jsonWorkspaceResult]() {
        return jsonWorkspaceResult(
            QStringLiteral("dryRunRemoveAllRenderQueues"), {});
    });
    registerWorkspaceMethod("dryRunRemoveComposition",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("dryRunRemoveComposition"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("dryRunRemoveLayerFromCurrentComposition",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral(
                                        "dryRunRemoveLayerFromCurrentComposition"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("dryRunRemoveProjectItemById",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("dryRunRemoveProjectItemById"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("dryRunRemoveRenderQueueAt",
                            [jsonWorkspaceResult, workspaceIntArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral("dryRunRemoveRenderQueueAt"),
                                    {workspaceIntArg(args, 0)});
                            });
    registerWorkspaceMethod("compositionRemovalConfirmationMessage",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral(
                                        "compositionRemovalConfirmationMessage"),
                                    {workspaceStringArg(args, 0)});
                            });
    registerWorkspaceMethod("projectItemRemovalConfirmationMessage",
                            [jsonWorkspaceResult, workspaceStringArg](
                                const std::vector<std::string>& args) {
                                return jsonWorkspaceResult(
                                    QStringLiteral(
                                        "projectItemRemovalConfirmationMessage"),
                                    {workspaceStringArg(args, 0)});
                            });

    // Layer manipulation methods (with arguments)
    registerWorkspaceMethod("selectLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("selectLayer"), {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerPosition", [](const std::vector<std::string>& args) -> QString {
        double x = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 0.0;
        double y = args.size() > 2 ? QString::fromStdString(args[2]).toDouble() : 0.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerPosition"), {layerId, x, y});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerScale", [](const std::vector<std::string>& args) -> QString {
        double sx = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 1.0;
        double sy = args.size() > 2 ? QString::fromStdString(args[2]).toDouble() : 1.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerScale"), {layerId, sx, sy});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerRotation", [](const std::vector<std::string>& args) -> QString {
        double rot = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 0.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerRotation"), {layerId, rot});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerOpacity", [](const std::vector<std::string>& args) -> QString {
        double opacity = args.size() > 1 ? QString::fromStdString(args[1]).toDouble() : 100.0;
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerOpacity"), {layerId, opacity});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("addTextLayer", [](const std::vector<std::string>& args) -> QString {
        QString name = args.empty() ? QStringLiteral("Text") : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("addTextLayerToCurrentComposition"), {name});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("addSolidLayer", [](const std::vector<std::string>& args) -> QString {
        QString name = args.empty() ? QStringLiteral("Solid") : QString::fromStdString(args[0]);
        int w = args.size() > 1 ? QString::fromStdString(args[1]).toInt() : 1920;
        int h = args.size() > 2 ? QString::fromStdString(args[2]).toInt() : 1080;
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("addSolidLayerToCurrentComposition"), {name, w, h});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    const auto createNoiseLayerPython = [](const std::vector<std::string>& args) -> QString {
        const QString compositionId = args.empty()
                                          ? QStringLiteral("current")
                                          : QString::fromStdString(args[0]);
        const QString name = args.size() > 1
                                 ? QString::fromStdString(args[1])
                                 : QStringLiteral("Noise Layer");
        const int width = args.size() > 2
                              ? QString::fromStdString(args[2]).toInt()
                              : 0;
        const int height = args.size() > 3
                               ? QString::fromStdString(args[3]).toInt()
                               : 0;
        const int seed = args.size() > 4
                             ? QString::fromStdString(args[4]).toInt()
                             : 42;
        const QString kind = args.size() > 5
                                 ? QString::fromStdString(args[5])
                                 : QStringLiteral("perlin");
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(
            QStringLiteral("createNoiseLayer"),
            {compositionId, name, width, height, seed, kind});
        return QString::fromUtf8(
            QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    };
    registerWorkspaceMethod("createNoiseLayer", createNoiseLayerPython);
    registerWorkspaceMethod("addNoiseLayer", createNoiseLayerPython);
    registerWorkspaceMethod("renameLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString newName = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("renameLayerInCurrentComposition"), {layerId, newName});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("removeLayer", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("removeLayerFromCurrentComposition"), {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Effect API
    registerWorkspaceMethod("addEffect", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectType = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("addLayerEffect"), {layerId, effectType});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("removeEffect", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("removeLayerEffect"), {layerId, effectId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setEffectParam", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString effectId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        QString paramName = args.size() < 3 ? QString() : QString::fromStdString(args[2]);
        QVariant value = args.size() < 4 ? QVariant() : QVariant(QString::fromStdString(args[3]));
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerEffectParameter"), {layerId, effectId, paramName, value});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Keyframe API
    registerWorkspaceMethod("setKeyframe", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString propPath = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        int frame = args.size() < 3 ? 0 : QString::fromStdString(args[2]).toInt();
        double value = args.size() < 4 ? 0.0 : QString::fromStdString(args[3]).toDouble();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setKeyframe"), {layerId, propPath, frame, value});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("getKeyframes", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString propPath = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("getKeyframes"), {layerId, propPath});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });

    // Playback API
    registerWorkspaceMethod("playbackStart", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("playbackStart"), {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackPause", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("playbackPause"), {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackStop", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("playbackStop"), {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackSetFrame", [](const std::vector<std::string>& args) -> QString {
        int frame = args.empty() ? 0 : QString::fromStdString(args[0]).toInt();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("playbackSetCurrentFrame"), {frame});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("playbackGetFrame", []() -> QString {
        return QString::number(WorkspaceAutomation::instance().invokeMethod(QStringLiteral("playbackGetCurrentFrame"), {}).toInt());
    });

    // Render Queue API
    registerWorkspaceMethod("renderQueueAddCurrent", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("addRenderQueueForCurrentComposition"), {});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("renderQueueStart", [](const std::vector<std::string>& args) -> QString {
        int index = args.empty() ? 0 : QString::fromStdString(args[0]).toInt();
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("startRenderQueueAt"), {index});
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
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("exportComposition"), 
            {compId, outputPath, fmt, codec, w, h, fps, bitrate});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });

    // Template API
    registerWorkspaceMethod("defineTemplateSlot", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString slotName = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        QString defaultValue = args.size() < 3 ? QString() : QString::fromStdString(args[2]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("defineTemplateSlot"), {layerId, slotName, defaultValue});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("listTemplateSlots", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("listTemplateSlots"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
    registerWorkspaceMethod("applyTemplateVariation", [](const std::vector<std::string>& args) -> QString {
        QString variationJson = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("applyTemplateVariation"), {variationJson});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Mask/Blend/Parent API
    registerWorkspaceMethod("setLayerParent", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString parentId = args.size() < 2 ? QString() : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerParentInCurrentComposition"), {layerId, parentId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("clearLayerParent", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.empty() ? QString() : QString::fromStdString(args[0]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("clearLayerParentInCurrentComposition"), {layerId});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });
    registerWorkspaceMethod("setLayerBlendMode", [](const std::vector<std::string>& args) -> QString {
        QString layerId = args.size() < 1 ? QString() : QString::fromStdString(args[0]);
        QString mode = args.size() < 2 ? QStringLiteral("normal") : QString::fromStdString(args[1]);
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("setLayerBlendModeInCurrentComposition"), {layerId, mode});
        return result.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    });

    // Effect listing API
    registerWorkspaceMethod("listAvailableEffects", []() -> QString {
        const QVariant result = WorkspaceAutomation::instance().invokeMethod(QStringLiteral("listAvailableEffects"), {});
        return QString::fromUtf8(QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact));
    });
};

void ArtifactPythonHookManager::registerWorkspaceMethod(const std::string& name, std::function<QString()> func)
{
    ArtifactCore::PythonEngine::instance().registerFunction(
        std::string("workspace_") + name,
        [func](const std::vector<std::string>&) -> std::string {
            return func().toStdString();
        });

    ArtifactCore::PythonEngine::instance().execute(
        QString("import artifact.workspace\n"
                "artifact.workspace.%1 = workspace_%1\n")
            .arg(QString::fromStdString(name)).toStdString());
}

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
