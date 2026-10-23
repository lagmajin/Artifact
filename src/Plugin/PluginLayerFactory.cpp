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
import Memory.SharedPtr;

namespace Artifact {
using namespace ArtifactCore;

struct PluginLayerFactory::Impl {
    std::vector<LayerPluginInfo> plugins;
    std::unique_ptr<ArtifactPluginLoader> loader = std::make_unique<ArtifactPluginLoader>();

    void registerFromDll(const ArtifactCore::String& pluginId,
                         ArtifactPluginInstance instance,
                         ArtifactLayerPluginVTable vtable,
                         LayerPluginAdapter::DestroyFunction destroy,
                         const ArtifactCore::String& version) {
        for (const auto& existing : plugins) {
            if (existing.id == pluginId) {
                if (destroy && instance) destroy(instance);
                return;
            }
        }
        auto adapter = ArtifactCore::makeShared<LayerPluginAdapter>(ArtifactCore::toStdString(pluginId), vtable, instance, destroy);
        if (adapter->initialize()) {
            LayerPluginInfo info;
            info.id = pluginId;
            info.version = version;
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
    // For each loaded layer plugin DLL, resolve the layer-specific ABI functions
    // and create an adapter via registerFromDll.
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->loader->setOnPluginLoaded([this](const QString& dllPath, void* libHandle,
                                     const ArtifactCore::PluginDescriptor& desc) {
        if (desc.category != PluginCategory::Layer) return;

        auto* lib = static_cast<QLibrary*>(libHandle);

        using CreateLayerFn = ArtifactPluginInstance (*)(const char*);
        using DestroyLayerFn = void (*)(ArtifactPluginInstance);
        using GetVTableFn = const ArtifactLayerPluginVTable* (*)(const char*);
        auto fnCreate = reinterpret_cast<CreateLayerFn>(
            lib->resolve("ArtifactPlugin_CreateLayer"));
        auto fnDestroy = reinterpret_cast<DestroyLayerFn>(
            lib->resolve("ArtifactPlugin_DestroyLayer"));
        auto fnVTable = reinterpret_cast<GetVTableFn>(
            lib->resolve("ArtifactPlugin_GetLayerVTable"));

        if (!fnCreate || !fnVTable) return;

        const std::string descriptorId = ArtifactCore::toStdString(desc.id);
        auto instance = fnCreate(descriptorId.c_str());
        auto vtable = fnVTable(descriptorId.c_str());
        if (instance && vtable) {
            registerFromDll(desc.id, instance, *vtable, fnDestroy, desc.version);
        } else if (instance && fnDestroy) {
            fnDestroy(instance);
        }
    });

    // Scan standard layer plugin directories
    QStringList paths = {
        QDir(QCoreApplication::applicationDirPath()).filePath("plugins/layers"),
    };
    impl_->loader->discoverAndLoad(paths, PluginLoadMode::DllInProcess);
}

void PluginLayerFactory::registerFromDll(const ArtifactCore::String& pluginId,
                                          ArtifactPluginInstance instance,
                                          ArtifactLayerPluginVTable vtable,
                                          LayerPluginAdapter::DestroyFunction destroy,
                                          const ArtifactCore::String& version) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->registerFromDll(pluginId, instance, vtable, destroy, version);
}

void PluginLayerFactory::unregisterAll() {
    if (!impl_) return;
    impl_->plugins.clear();
    impl_->loader->unloadAll();
}

std::vector<LayerPluginInfo> PluginLayerFactory::availablePlugins() const {
    if (!impl_) return {};
    return impl_->plugins;
}

ArtifactCore::SharedPtr<ArtifactCore::ILayerPlugin> PluginLayerFactory::pluginById(const ArtifactCore::String& id) const {
    if (!impl_) return nullptr;
    for (const auto& p : impl_->plugins) {
        if (p.id == id) return p.plugin;
    }
    return nullptr;
}

} // namespace Artifact
