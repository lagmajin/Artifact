module;

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_set>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QCoreApplication>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>

#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Loader;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import Artifact.Plugin.Sandbox;

namespace Artifact {
using namespace ArtifactCore;

typedef int (*PFN_ArtifactPlugin_GetAPIVersion)();
typedef int (*PFN_ArtifactPlugin_GetPluginCount)();
typedef const ArtifactPluginDescriptor* (*PFN_ArtifactPlugin_GetPlugin)(int);

struct ArtifactPluginLoader::Impl {
    std::vector<LoadResult> results;
    std::vector<std::unique_ptr<QLibrary>> loadedLibs;
    std::vector<std::unique_ptr<ArtifactPluginSandbox>> sandboxes;
    std::vector<std::string> loadedPluginIds;
    std::function<PluginLoadedCallback> onPluginLoaded;

    QStringList defaultSearchPaths() {
        return {
            QDir(QCoreApplication::applicationDirPath()).filePath("plugins/layers"),
            QDir(QCoreApplication::applicationDirPath()).filePath("plugins/tools"),
            QDir(QCoreApplication::applicationDirPath()).filePath("plugins/effects"),
            QDir(QCoreApplication::applicationDirPath()).filePath("plugins"),
        };
    }

    LoadResult loadDllPlugin(const QString& path) {
        LoadResult result;
        result.pluginPath = path.toStdString();
        result.loadedMode = PluginLoadMode::DllInProcess;

        auto lib = std::make_unique<QLibrary>(path);
        if (!lib->load()) {
            result.success = false;
            result.errorMessage = lib->errorString().toStdString();
            return result;
        }

        auto fnVersion = reinterpret_cast<PFN_ArtifactPlugin_GetAPIVersion>(
            lib->resolve("ArtifactPlugin_GetAPIVersion"));
        auto fnCount = reinterpret_cast<PFN_ArtifactPlugin_GetPluginCount>(
            lib->resolve("ArtifactPlugin_GetPluginCount"));
        auto fnPlugin = reinterpret_cast<PFN_ArtifactPlugin_GetPlugin>(
            lib->resolve("ArtifactPlugin_GetPlugin"));

        if (!fnVersion || !fnCount || !fnPlugin) {
            result.success = false;
            result.errorMessage = "Missing required exports: ArtifactPlugin_GetAPIVersion, _GetPluginCount, _GetPlugin";
            return result;
        }

        const int apiVersion = fnVersion();
        if (apiVersion < 1 || apiVersion > ARTIFACT_PLUGIN_API_VERSION) {
            result.success = false;
            result.errorMessage = "Unsupported API version: " + std::to_string(apiVersion);
            return result;
        }

        const int count = fnCount();
        constexpr int kMaxPluginsPerLibrary = 1024;
        if (count < 0 || count > kMaxPluginsPerLibrary) {
            result.success = false;
            result.errorMessage = "Invalid plugin count returned by DLL";
            return result;
        }
        bool anySucceeded = false;
        std::unordered_set<std::string> seenPluginIds;
        void* libRaw = lib.get();

        for (int i = 0; i < count; ++i) {
            const auto* desc = fnPlugin(i);
            if (!desc || !desc->id || !desc->displayName) {
                continue;
            }
            if (desc->apiVersion <= 0 ||
                desc->apiVersion > ARTIFACT_PLUGIN_API_VERSION ||
                desc->category < static_cast<int>(PluginCategory::Effect) ||
                desc->category > static_cast<int>(PluginCategory::ImportExport)) {
                continue;
            }
            const std::string pluginId = desc->id;
            if (pluginId.empty()) continue;
            if (!seenPluginIds.insert(pluginId).second) continue;

            PluginDescriptor pd;
            pd.id = desc->id;
            pd.displayName = desc->displayName;
            pd.version = desc->version ? desc->version : "0.0.0";
            pd.author = desc->author ? desc->author : "";
            pd.description = desc->description ? desc->description : "";
            pd.category = static_cast<PluginCategory>(desc->category);
            pd.apiVersion = desc->apiVersion;
            pd.pluginPath = path.toStdString();
            pd.state = PluginState::Validated;

            auto& registry = ArtifactPluginRegistry::instance();
            registry.registerPlugin(pd);
            const std::string loadedPluginId = toStdString(pd.id);
            if (std::find(loadedPluginIds.cbegin(), loadedPluginIds.cend(), loadedPluginId) ==
                loadedPluginIds.cend()) {
                loadedPluginIds.push_back(loadedPluginId);
            }

            // Fire category-specific callback so consumers (e.g. PluginLayerFactory)
            // can resolve additional ABI functions while the library is still loaded.
            if (onPluginLoaded) {
                onPluginLoaded(QDir::toNativeSeparators(path), libRaw, pd);
            }

            result.pluginId = pd.id;
            result.success = true;
            anySucceeded = true;
        }

        if (!anySucceeded) {
            result.success = false;
            result.errorMessage = "No valid plugins found in DLL";
            return result;
        }

        // Keep the library loaded for consumers that resolved ABI via callback.
        loadedLibs.push_back(std::move(lib));

        if (!result.success) {
            result.success = true;
            result.errorMessage.clear();
        }

        return result;
    }

    LoadResult loadSubprocessPlugin(const QString& path) {
        LoadResult result;
        result.pluginPath = path.toStdString();
        result.loadedMode = PluginLoadMode::Subprocess;
        const QFileInfo pluginInfo(path);
        if (!pluginInfo.exists() || !pluginInfo.isFile()) {
            result.errorMessage = "Plugin path is not a file";
            return result;
        }

        const QStringList runnerCandidates = {
            QDir(QCoreApplication::applicationDirPath()).filePath("ArtifactPluginRunner.exe"),
            QDir(QCoreApplication::applicationDirPath()).filePath("plugins/ArtifactPluginRunner.exe")
        };
        QString runnerPath;
        for (const auto& candidate : runnerCandidates) {
            if (QFileInfo::exists(candidate) && QFileInfo(candidate).isFile()) {
                runnerPath = candidate;
                break;
            }
        }
        if (runnerPath.isEmpty()) {
            result.errorMessage = "Plugin runner executable was not found";
            return result;
        }

        const std::string sandboxId = QFileInfo(path).completeBaseName().toStdString();
        auto sandbox = std::make_unique<ArtifactPluginSandbox>(sandboxId, runnerPath, path);
        if (!sandbox->start()) {
            result.errorMessage = sandbox->lastError();
            return result;
        }
        result.success = true;
        result.subprocessId = sandboxId;
        sandboxes.push_back(std::move(sandbox));
        return result;
    }

    void scanDirectory(const QString& dirPath, PluginLoadMode mode,
                       QSet<QString>* seenPaths = nullptr) {
        QDir dir(dirPath);
        if (!dir.exists()) return;

        const QStringList nameFilters = {
            "*.dll",
        };
        const auto entries = dir.entryInfoList(nameFilters, QDir::Files);
        for (const auto& info : entries) {
            const auto filePath = info.absoluteFilePath();
            const QString identity = QFileInfo(filePath).canonicalFilePath().isEmpty()
                ? QDir::cleanPath(filePath)
                : QFileInfo(filePath).canonicalFilePath();
#ifdef Q_OS_WIN
            const QString normalizedIdentity = identity.toCaseFolded();
#else
            const QString& normalizedIdentity = identity;
#endif
            if (seenPaths && seenPaths->contains(normalizedIdentity)) continue;
            if (seenPaths) seenPaths->insert(normalizedIdentity);
            LoadResult r;
            if (mode == PluginLoadMode::Subprocess) {
                r = loadSubprocessPlugin(filePath);
            } else {
                r = loadDllPlugin(filePath);
                if (mode == PluginLoadMode::Auto && !r.success) {
                    const LoadResult fallback = loadSubprocessPlugin(filePath);
                    if (fallback.success) {
                        r = fallback;
                    } else {
                        r.errorMessage += "; Subprocess fallback: " + fallback.errorMessage;
                    }
                }
            }
            results.push_back(std::move(r));
        }
    }
};

ArtifactPluginLoader::ArtifactPluginLoader()
    : impl_(std::make_unique<Impl>()) {}

ArtifactPluginLoader::~ArtifactPluginLoader() {
    unloadAll();
}

void ArtifactPluginLoader::discoverAndLoad(const QStringList& searchPaths,
                                           PluginLoadMode mode) {
    QStringList paths = searchPaths.isEmpty() ? impl_->defaultSearchPaths() : searchPaths;
    QSet<QString> seenPaths;
    for (const auto& p : paths) {
        impl_->scanDirectory(p, mode, &seenPaths);
    }
}

void ArtifactPluginLoader::setOnPluginLoaded(std::function<PluginLoadedCallback> callback) {
    impl_->onPluginLoaded = std::move(callback);
}

LoadResult ArtifactPluginLoader::loadPlugin(const QString& path, PluginLoadMode mode) {
    LoadResult r;
    r.pluginPath = path.toStdString();
    const QFileInfo pluginInfo(path);
    if (path.trimmed().isEmpty() || !pluginInfo.exists() || !pluginInfo.isFile()) {
        r.errorMessage = "Plugin path is not a file";
        impl_->results.push_back(r);
        return r;
    }

    if (mode == PluginLoadMode::Subprocess) {
        r = impl_->loadSubprocessPlugin(path);
        impl_->results.push_back(r);
        return r;
    }

    r = impl_->loadDllPlugin(path);
    if (mode == PluginLoadMode::Auto && !r.success) {
        const LoadResult fallback = impl_->loadSubprocessPlugin(path);
        if (fallback.success) {
            r = fallback;
        } else {
            r.errorMessage += "; Subprocess fallback: " + fallback.errorMessage;
        }
    }
    impl_->results.push_back(r);
    return r;
}

void ArtifactPluginLoader::unloadAll() {
    auto& registry = ArtifactCore::ArtifactPluginRegistry::instance();
    for (const auto& pluginId : impl_->loadedPluginIds) {
        registry.unregisterPlugin(pluginId);
    }
    impl_->loadedPluginIds.clear();
    for (auto& sandbox : impl_->sandboxes) {
        if (sandbox) sandbox->stop();
    }
    impl_->sandboxes.clear();
    impl_->results.clear();
    impl_->loadedLibs.clear();
}

std::vector<LoadResult> ArtifactPluginLoader::loadResults() const {
    return impl_->results;
}

} // namespace Artifact
