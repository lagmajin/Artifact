module;

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QStringList>

#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Layer.Factory;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import ArtifactCore.Plugin.Layer.Interface;
import Artifact.Plugin.Loader;   // ArtifactPluginLoader
import Artifact.Plugin.Layer.Adapter;

namespace Artifact {
using namespace ArtifactCore;

struct PluginLayerFactory::Impl {
    std::vector<LayerPluginInfo> plugins;

    void registerFromDll(const std::string& pluginId,
                         ArtifactPluginInstance instance,
                         ArtifactLayerPluginVTable vtable) {
        auto adapter = std::make_shared<LayerPluginAdapter>(pluginId, vtable, instance);
        if (adapter->initialize()) {
            LayerPluginInfo info;
            info.id = pluginId;
            info.displayName = adapter->displayName();
            info.plugin = adapter;
            plugins.push_back(std::move(info));
        }
    }
};

PluginLayerFactory& PluginLayerFactory::instance() {
    static PluginLayerFactory factory;
    return factory;
}

void PluginLayerFactory::scanAndRegister() {
    ArtifactPluginLoader loader;

    // For each loaded layer plugin DLL, resolve the layer-specific ABI functions
    // and create an adapter via registerFromDll.
    loader.setOnPluginLoaded([this](const QString& dllPath, void* libHandle,
                                     const ArtifactCore::PluginDescriptor& desc) {
        if (desc.category != PluginCategory::Layer) return;

        auto* lib = static_cast<QLibrary*>(libHandle);

        using CreateLayerFn = ArtifactPluginInstance (*)(const char*);
        using GetVTableFn = const ArtifactLayerPluginVTable* (*)(const char*);
        auto fnCreate = reinterpret_cast<CreateLayerFn>(
            lib->resolve("ArtifactPlugin_CreateLayer"));
        auto fnVTable = reinterpret_cast<GetVTableFn>(
            lib->resolve("ArtifactPlugin_GetLayerVTable"));

        if (!fnCreate || !fnVTable) return;

        auto instance = fnCreate(desc.id.c_str());
        auto vtable = fnVTable(desc.id.c_str());
        if (instance && vtable) {
            registerFromDll(desc.id, instance, *vtable);
        }
    });

    // Scan standard layer plugin directories
    QStringList paths = {
        QDir(QCoreApplication::applicationDirPath()).filePath("plugins/layers"),
    };
    loader.discoverAndLoad(paths, PluginLoadMode::DllInProcess);
}

void PluginLayerFactory::registerFromDll(const std::string& pluginId,
                                          ArtifactPluginInstance instance,
                                          ArtifactLayerPluginVTable vtable) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->registerFromDll(pluginId, instance, vtable);
}

void PluginLayerFactory::unregisterAll() {
    if (impl_) impl_->plugins.clear();
}

std::vector<LayerPluginInfo> PluginLayerFactory::availablePlugins() const {
    if (!impl_) return {};
    return impl_->plugins;
}

std::shared_ptr<ArtifactCore::ILayerPlugin> PluginLayerFactory::pluginById(const std::string& id) const {
    if (!impl_) return nullptr;
    for (const auto& p : impl_->plugins) {
        if (p.id == id) return p.plugin;
    }
    return nullptr;
}

} // namespace Artifact
