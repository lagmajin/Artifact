module;

#include <memory>
#include <string>
#include <vector>

#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Layer.Factory;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import ArtifactCore.Plugin.Layer.Interface;
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
    // Delegate to PluginLoader for DLL discovery
    auto& registry = ArtifactPluginRegistry::instance();
    auto layerPlugins = registry.pluginsOfCategory(PluginCategory::Layer);
    for (const auto& desc : layerPlugins) {
        // Load DLL and get vtable — delegated to PluginLoader integration
    }
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
