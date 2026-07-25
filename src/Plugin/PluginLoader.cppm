module;

#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QCoreApplication>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Loader;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;

namespace Artifact {
using namespace ArtifactCore;

typedef int (*PFN_ArtifactPlugin_GetAPIVersion)();
typedef int (*PFN_ArtifactPlugin_GetPluginCount)();
typedef const ArtifactPluginDescriptor* (*PFN_ArtifactPlugin_GetPlugin)(int);

struct ArtifactPluginLoader::Impl {
    std::vector<LoadResult> results;
    std::vector<std::unique_ptr<QLibrary>> loadedLibs;
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
        bool anySucceeded = false;
        void* libRaw = lib.get();

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

        result.success = false;
        result.errorMessage =
            "Subprocess loading requires a plugin runner executable. "
            "See docs/PLUGIN_SUBPROCESS_PROTOCOL.md for the JSON IPC spec. "
            "PluginSandbox is ready to manage the subprocess lifecycle.";
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
            LoadResult r = (mode == PluginLoadMode::Subprocess)
                ? loadSubprocessPlugin(filePath)
                : loadDllPlugin(filePath);
            results.push_back(std::move(r));
        }
    }
};

ArtifactPluginLoader::ArtifactPluginLoader()
    : impl_(std::make_unique<Impl>()) {}

ArtifactPluginLoader::~ArtifactPluginLoader() = default;

void ArtifactPluginLoader::discoverAndLoad(const QStringList& searchPaths,
                                           PluginLoadMode mode) {
    QStringList paths = searchPaths.isEmpty() ? impl_->defaultSearchPaths() : searchPaths;
    for (const auto& p : paths) {
        impl_->scanDirectory(p, mode);
    }
}

void ArtifactPluginLoader::setOnPluginLoaded(std::function<PluginLoadedCallback> callback) {
    impl_->onPluginLoaded = std::move(callback);
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
    impl_->loadedLibs.clear();
}

std::vector<LoadResult> ArtifactPluginLoader::loadResults() const {
    return impl_->results;
}

} // namespace Artifact
