module;
#include <cstdlib>
#include <unordered_set>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include "Plugin/ArtifactPluginABI.h"

module Artifact.Plugin.Layer.Adapter;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Layer.Interface;

namespace Artifact {

LayerPluginAdapter::LayerPluginAdapter(const std::string& pluginId,
                                       ArtifactLayerPluginVTable vtable,
                                       ArtifactPluginInstance instance,
                                       DestroyFunction destroy)
    : pluginId_(pluginId), vtable_(vtable), instance_(instance), destroy_(destroy) {}

LayerPluginAdapter::~LayerPluginAdapter() {
    if (instance_ && !shutdownCalled_ && vtable_.shutdown) {
        vtable_.shutdown(instance_);
    }
    if (instance_ && destroy_) {
        destroy_(instance_);
    }
    instance_ = nullptr;
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
    if (!instance_ || shutdownCalled_ || !vtable_.initialize) return false;
    if (initialized_) return true;
    initialized_ = vtable_.initialize(instance_) != 0;
    return initialized_;
}

void LayerPluginAdapter::shutdown() {
    if (instance_ && !shutdownCalled_ && vtable_.shutdown) {
        vtable_.shutdown(instance_);
    }
    shutdownCalled_ = true;
}

std::vector<ArtifactCore::PropertyGroup> LayerPluginAdapter::extraPropertyGroups() {
    std::vector<ArtifactCore::PropertyGroup> groups;
    if (!instance_ || !vtable_.getPropertyGroupCount || !vtable_.getPropertyGroupDef) {
        return groups;
    }
    const int count = vtable_.getPropertyGroupCount(instance_);
    constexpr int kMaxPropertyGroups = 1024;
    if (count < 0 || count > kMaxPropertyGroups) {
        return groups;
    }
    std::unordered_set<std::string> groupNames;
    for (int i = 0; i < count; ++i) {
        char* nameOut = nullptr;
        char* jsonSchemaOut = nullptr;
        if (vtable_.getPropertyGroupDef(instance_, i, &nameOut, &jsonSchemaOut) == 0) {
            if (nameOut) {
                const QString groupName = QString::fromUtf8(nameOut).trimmed();
                if (!groupName.isEmpty() &&
                    groupNames.insert(groupName.toStdString()).second) {
                    ArtifactCore::PropertyGroup group(groupName);
                // The JSON schema from the plugin describes what properties exist.
                // The PropertyGroup holds the container; property instances are
                // created/configured externally based on the schema.
                    groups.push_back(std::move(group));
                }
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
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(
                QByteArray(jsonOut), &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                json = document.object();
            }
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
