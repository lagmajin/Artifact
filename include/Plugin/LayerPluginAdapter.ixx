module;

#include <memory>
#include <string>

export module Artifact.Plugin.Layer.Adapter;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Layer.Interface;
import ArtifactCore.Plugin.Registry;

#include "ArtifactPluginABI.h"

export namespace Artifact {

class LayerPluginAdapter : public ArtifactCore::ILayerPlugin {
public:
    LayerPluginAdapter(const std::string& pluginId,
                       ArtifactLayerPluginVTable vtable,
                       ArtifactPluginInstance instance);
    ~LayerPluginAdapter() override;

    std::string pluginId() const override;
    std::string displayName() const override;
    bool initialize() override;
    void shutdown() override;
    std::vector<ArtifactCore::PropertyGroup> extraPropertyGroups() override;
    void drawContent(void* layerPtr,
                     const ArtifactCore::DrawContext& ctx) override;
    void serializeExtra(void* layerPtr, QJsonObject& json) override;
    void deserializeExtra(void* layerPtr, const QJsonObject& json) override;

private:
    std::string pluginId_;
    ArtifactLayerPluginVTable vtable_;
    ArtifactPluginInstance instance_ = nullptr;
};

} // namespace Artifact
