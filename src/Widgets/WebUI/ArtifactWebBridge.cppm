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
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Property.Group;
import Property.Abstract;
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
            const LayerID lid(layerId);
            service->selectLayer(lid);
        }
    }

    void ArtifactWebBridge::setEffectProperty(const QString& effectId, const QString& propertyName, const QString& jsonValue)
    {
        qDebug() << "[WebBridge] setEffectProperty:" << effectId << propertyName << jsonValue;

        QJsonDocument doc = QJsonDocument::fromJson(jsonValue.toUtf8());
        QVariant val;
        if (doc.isObject()) {
            val = doc.object().toVariantMap();
        } else {
            val = QVariant(jsonValue);
        }

        if (auto* service = ArtifactProjectService::instance()) {
            const auto comp = service->currentComposition().lock();
            if (comp) {
                for (const auto& layer : comp->allLayerRef()) {
                    if (!layer) {
                        continue;
                    }
                    for (const auto& effect : layer->getEffects()) {
                        if (!effect) {
                            continue;
                        }
                        const QString currentEffectId = QString(effect->effectID());
                        const QString currentEffectName = QString(effect->displayName());
                        if (currentEffectId == effectId || currentEffectName == effectId) {
                            effect->setPropertyValue(ArtifactCore::UniString(propertyName.toStdString()), val);
                            emit propertyUpdated(effectId, propertyName, jsonValue);
                            return;
                        }
                    }
                }
            }
        }

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
            const auto comp = service->currentComposition().lock();
            info["hasComposition"] = static_cast<bool>(comp);
            info["compositionCount"] = comp ? 1 : 0;
            info["layerCount"] = comp ? static_cast<int>(comp->allLayerRef().size()) : 0;
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
        result["properties"] = QJsonArray();

        auto* service = ArtifactProjectService::instance();
        if (!service) {
            return QJsonDocument(result).toJson(QJsonDocument::Compact);
        }

        const auto comp = service->currentComposition().lock();
        if (!comp) {
            return QJsonDocument(result).toJson(QJsonDocument::Compact);
        }

        auto *selection = ArtifactApplicationManager::instance()
                               ? ArtifactApplicationManager::instance()->layerSelectionManager()
                               : nullptr;
        const ArtifactAbstractLayerPtr layer =
            selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
        if (!layer) {
            return QJsonDocument(result).toJson(QJsonDocument::Compact);
        }

        result["layerId"] = layer->id().toString();
        result["layerName"] = layer->layerName();
        result["layerType"] = QString(layer->className());
        result["effectCount"] = layer->effectCount();

        QJsonArray effects;
        for (const auto& effect : layer->getEffects()) {
            if (!effect) {
                continue;
            }
            QJsonObject effectJson;
            effectJson["effectId"] = QString(effect->effectID());
            effectJson["displayName"] = QString(effect->displayName());
            effectJson["enabled"] = effect->isEnabled();
            QJsonArray properties;
            for (const auto& property : effect->getProperties()) {
                QJsonObject prop;
                prop["name"] = property.getName();
                prop["type"] = QString::fromUtf8(ArtifactCore::propertyTypeToString(property.getType()).toUtf8());
                prop["value"] = QJsonValue::fromVariant(property.getValue());
                properties.push_back(prop);
            }
            effectJson["properties"] = properties;
            effects.push_back(effectJson);
        }
        result["effects"] = effects;

        QJsonArray properties;
        for (const auto& group : layer->getLayerPropertyGroups()) {
            QJsonObject groupJson;
            groupJson["name"] = group.name();
            QJsonArray entries;
            for (const auto& property : group.allProperties()) {
                if (!property) {
                    continue;
                }
                QJsonObject prop;
                prop["name"] = property->getName();
                prop["type"] = QString::fromUtf8(ArtifactCore::propertyTypeToString(property->getType()).toUtf8());
                prop["value"] = QJsonValue::fromVariant(property->getValue());
                prop["displayPriority"] = property->displayPriority();
                entries.push_back(prop);
            }
            groupJson["properties"] = entries;
            properties.push_back(groupJson);
        }
        result["properties"] = properties;

        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

}
