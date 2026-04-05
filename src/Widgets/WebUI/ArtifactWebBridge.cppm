module;
#include <wobjectimpl.h>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Widgets.WebBridge;




import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

    using namespace ArtifactCore;

    W_OBJECT_IMPL(ArtifactWebBridge)

    ArtifactWebBridge::ArtifactWebBridge(QObject* parent)
        : QObject(parent)
    {
        eventBusSubscriptions_.push_back(eventBus_.subscribe<LayerSelectionChangedEvent>(
            [this](const LayerSelectionChangedEvent& event) {
                emit layerSelectionChanged(event.layerId);
            }));
    }

    ArtifactWebBridge::~ArtifactWebBridge() = default;

    void ArtifactWebBridge::selectLayer(const QString& layerId)
    {
        qDebug() << "[WebBridge] selectLayer called from JS:" << layerId;
        if (auto* service = ArtifactProjectService::instance()) {
            LayerID lid;
            // TODO: construct LayerID from string
            service->selectLayer(lid);
        }
    }

    void ArtifactWebBridge::setEffectProperty(const QString& effectId, const QString& propertyName, const QString& jsonValue)
    {
        qDebug() << "[WebBridge] setEffectProperty:" << effectId << propertyName << jsonValue;

        // Parse JSON value
        QJsonDocument doc = QJsonDocument::fromJson(jsonValue.toUtf8());
        QVariant val;
        if (doc.isObject()) {
            val = doc.object().toVariantMap();
        } else {
            val = QVariant(jsonValue);
        }

        // TODO: Look up effect by ID from the current layer and call setPropertyValue()
        // For now, emit the signal back to notify any listeners
        emit propertyUpdated(effectId, propertyName, jsonValue);
    }

    QString ArtifactWebBridge::getProjectInfo()
    {
        QJsonObject info;
        info["appName"] = "Artifact";
        info["version"] = "0.1.0";
        info["pipeline"] = "Generator -> GeometryTransform -> MaterialRender -> Rasterizer -> LayerTransform";

        auto* service = ArtifactProjectService::instance();
        if (service) {
            info["hasProject"] = true;
            // TODO: add composition count, layer count, etc.
        } else {
            info["hasProject"] = false;
        }

        return QJsonDocument(info).toJson(QJsonDocument::Compact);
    }

    QString ArtifactWebBridge::getSelectedLayerProperties()
    {
        QJsonObject result;
        result["layerId"] = "";
        result["effects"] = QJsonArray();

        // TODO: Get current selected layer and serialize its effects/properties to JSON
        // This will be the main data source for the web-based inspector UI

        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

}
