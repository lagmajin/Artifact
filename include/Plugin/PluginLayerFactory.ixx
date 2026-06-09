module;

#include <memory>
#include <string>
#include <vector>

export module Artifact.Plugin.Layer.Factory;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import ArtifactCore.Plugin.Layer.Interface;
import Artifact.Plugin.Layer.Adapter;

#include "ArtifactPluginABI.h"

export namespace Artifact {

struct LayerPluginInfo {
    std::string id;
    std::string displayName;
    std::string version;
    std::shared_ptr<ArtifactCore::ILayerPlugin> plugin;
};

class PluginLayerFactory {
public:
    static PluginLayerFactory& instance();

    void scanAndRegister();
    void registerFromDll(const std::string& pluginId,
                         ArtifactPluginInstance instance,
                         ArtifactLayerPluginVTable vtable);
    void unregisterAll();

    std::vector<LayerPluginInfo> availablePlugins() const;
    std::shared_ptr<ArtifactCore::ILayerPlugin> pluginById(const std::string& id) const;

private:
    PluginLayerFactory() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
