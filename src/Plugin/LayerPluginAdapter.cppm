module;
#include <cstdlib>
#include <QByteArray>
#include <QJsonDocument>
#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Layer.Adapter;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Layer.Interface;

namespace Artifact {

LayerPluginAdapter::LayerPluginAdapter(const std::string& pluginId,
                                       ArtifactLayerPluginVTable vtable,
                                       ArtifactPluginInstance instance)
    : pluginId_(pluginId), vtable_(vtable), instance_(instance) {}

LayerPluginAdapter::~LayerPluginAdapter() {
    if (instance_ && vtable_.shutdown) {
        vtable_.shutdown(instance_);
    }
}

std::string LayerPluginAdapter::pluginId() const { return pluginId_; }

std::string LayerPluginAdapter::displayName() const {
    if (vtable_.getDisplayName && instance_) {
        const char* name = vtable_.getDisplayName(instance_);
        return name ? name : pluginId_;
    }
    return pluginId_;
}

bool LayerPluginAdapter::initialize() {
    if (!instance_ || !vtable_.initialize) return false;
    return vtable_.initialize(instance_) != 0;
}

void LayerPluginAdapter::shutdown() {
    if (instance_ && vtable_.shutdown) {
        vtable_.shutdown(instance_);
    }
}

std::vector<ArtifactCore::PropertyGroup> LayerPluginAdapter::extraPropertyGroups() {
    std::vector<ArtifactCore::PropertyGroup> groups;
    if (!instance_ || !vtable_.getPropertyGroupCount || !vtable_.getPropertyGroupDef) {
        return groups;
    }
    const int count = vtable_.getPropertyGroupCount(instance_);
    for (int i = 0; i < count; ++i) {
        char* nameOut = nullptr;
        char* jsonSchemaOut = nullptr;
        if (vtable_.getPropertyGroupDef(instance_, i, &nameOut, &jsonSchemaOut) == 0) {
            if (nameOut) {
                ArtifactCore::PropertyGroup group(QString::fromUtf8(nameOut));
                // The JSON schema from the plugin describes what properties exist.
                // The PropertyGroup holds the container; property instances are
                // created/configured externally based on the schema.
                groups.push_back(std::move(group));
            }
            free(nameOut);
            free(jsonSchemaOut);
        }
    }
    return groups;
}

void LayerPluginAdapter::drawContent(void* layerPtr, const ArtifactCore::DrawContext& ctx) {
    if (instance_ && vtable_.drawContent) {
        vtable_.drawContent(instance_, layerPtr,
                            ctx.currentTime, ctx.frameNumber,
                            ctx.compositionWidth, ctx.compositionHeight);
    }
}

void LayerPluginAdapter::serializeExtra(void* layerPtr, QJsonObject& json) {
    if (instance_ && vtable_.serializeExtra) {
        char* jsonOut = nullptr;
        if (vtable_.serializeExtra(instance_, layerPtr, &jsonOut) == 0 && jsonOut) {
            json = QJsonDocument::fromJson(QByteArray(jsonOut)).object();
            free(jsonOut);
        }
    }
}

void LayerPluginAdapter::deserializeExtra(void* layerPtr, const QJsonObject& json) {
    if (instance_ && vtable_.deserializeExtra) {
        QByteArray bytes = QJsonDocument(json).toJson(QJsonDocument::Compact);
        vtable_.deserializeExtra(instance_, layerPtr, bytes.constData());
    }
}

} // namespace Artifact
