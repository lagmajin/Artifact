module Artifact.Plugin.Loader;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;

#include "Plugin/ArtifactPluginABI.h"

namespace Artifact {

typedef int (*PFN_ArtifactPlugin_GetAPIVersion)();
typedef int (*PFN_ArtifactPlugin_GetPluginCount)();
typedef const ArtifactPluginDescriptor* (*PFN_ArtifactPlugin_GetPlugin)(int);

struct ArtifactPluginLoader::Impl {
    std::vector<LoadResult> results;

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

        QLibrary lib(path);
        if (!lib.load()) {
            result.success = false;
            result.errorMessage = lib.errorString().toStdString();
            return result;
        }

        auto fnVersion = reinterpret_cast<PFN_ArtifactPlugin_GetAPIVersion>(
            lib.resolve("ArtifactPlugin_GetAPIVersion"));
        auto fnCount = reinterpret_cast<PFN_ArtifactPlugin_GetPluginCount>(
            lib.resolve("ArtifactPlugin_GetPluginCount"));
        auto fnPlugin = reinterpret_cast<PFN_ArtifactPlugin_GetPlugin>(
            lib.resolve("ArtifactPlugin_GetPlugin"));

        if (!fnVersion || !fnCount || !fnPlugin) {
            lib.unload();
            result.success = false;
            result.errorMessage = "Missing required exports: ArtifactPlugin_GetAPIVersion, _GetPluginCount, _GetPlugin";
            return result;
        }

        const int apiVersion = fnVersion();
        if (apiVersion < 1 || apiVersion > ARTIFACT_PLUGIN_API_VERSION) {
            lib.unload();
            result.success = false;
            result.errorMessage = "Unsupported API version: " + std::to_string(apiVersion);
            return result;
        }

        const int count = fnCount();
        bool anySucceeded = false;

        for (int i = 0; i < count; ++i) {
            const auto* desc = fnPlugin(i);
            if (!desc || !desc->id || !desc->displayName) {
                continue;
            }

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

            result.pluginId = pd.id;
            result.success = true;
            anySucceeded = true;
        }

        if (!anySucceeded) {
            lib.unload();
            result.success = false;
            result.errorMessage = "No valid plugins found in DLL";
            return result;
        }

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

        result.success = false;
        result.errorMessage = "Subprocess plugin loading not yet implemented";
        return result;
    }

    void scanDirectory(const QString& dirPath, PluginLoadMode mode) {
        QDir dir(dirPath);
        if (!dir.exists()) return;

        const QStringList nameFilters = {
            "*.dll",
        };
        const auto entries = dir.entryInfoList(nameFilters, QDir::Files);
        for (const auto& info : entries) {
            const auto filePath = info.absoluteFilePath();
            LoadResult r = loadPlugin(filePath, mode);
            results.push_back(std::move(r));
        }
    }
};

ArtifactPluginLoader::ArtifactPluginLoader()
    : impl_(std::make_unique<Impl>()) {}

void ArtifactPluginLoader::discoverAndLoad(const QStringList& searchPaths,
                                           PluginLoadMode mode) {
    QStringList paths = searchPaths.isEmpty() ? impl_->defaultSearchPaths() : searchPaths;
    for (const auto& p : paths) {
        impl_->scanDirectory(p, mode);
    }
}

LoadResult ArtifactPluginLoader::loadPlugin(const QString& path, PluginLoadMode mode) {
    LoadResult r;
    r.pluginPath = path.toStdString();

    if (mode == PluginLoadMode::Subprocess) {
        r = impl_->loadSubprocessPlugin(path);
        impl_->results.push_back(r);
        return r;
    }

    r = impl_->loadDllPlugin(path);
    impl_->results.push_back(r);
    return r;
}

void ArtifactPluginLoader::unloadAll() {
    impl_->results.clear();
}

std::vector<LoadResult> ArtifactPluginLoader::loadResults() const {
    return impl_->results;
}

} // namespace Artifact
