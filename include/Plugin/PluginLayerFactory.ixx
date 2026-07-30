module;

#include <memory>
#include <string>
#include <vector>
#include "Plugin/ArtifactPluginABI.h"

export module Artifact.Plugin.Layer.Factory;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;
import ArtifactCore.Plugin.Layer.Interface;
import Artifact.Plugin.Layer.Adapter;
import Memory.SharedPtr;
import Core.ArtifactString;

export namespace Artifact {

struct LayerPluginInfo {
    ArtifactCore::String id;
    ArtifactCore::String displayName;
    ArtifactCore::String version;
    ArtifactCore::SharedPtr<ArtifactCore::ILayerPlugin> plugin;
};

class PluginLayerFactory {
public:
    static PluginLayerFactory& instance();

    void scanAndRegister();
    void registerFromDll(const ArtifactCore::String& pluginId,
                         ArtifactPluginInstance instance,
                         ArtifactLayerPluginVTable vtable);
    void unregisterAll();

    std::vector<LayerPluginInfo> availablePlugins() const;
    ArtifactCore::SharedPtr<ArtifactCore::ILayerPlugin> pluginById(const ArtifactCore::String& id) const;

private:
    PluginLayerFactory() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
