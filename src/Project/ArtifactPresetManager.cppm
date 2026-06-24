module;

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

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
module Artifact.Project.PresetManager;




import Artifact.Effect.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Property.Abstract;
import Utils.String.UniString;
import Artifact.Color.Palette;

namespace Artifact {

QJsonObject ArtifactPresetManager::effectToPresetJson(const ArtifactAbstractEffectPtr& effect) {
    if (!effect) return {};

    QJsonObject root;
    root["effect_id"] = effect->effectID().toQString();
    root["display_name"] = effect->displayName().toQString();
    root["mask_enabled"] = effect->maskEnabled();
    root["mask_layer_id"] = effect->maskLayerId();
    root["mask_name"] = effect->maskName();
    root["mask_inverted"] = effect->maskInverted();
    root["mask_opacity"] = effect->maskOpacity();

    QJsonArray propsArray;
    auto props = effect->getProperties();
    for (const auto& p : props) {
        QJsonObject propObj;
        propObj["name"] = p.getName();
        propObj["type"] = static_cast<int>(p.getType());
        propObj["value"] = QJsonValue::fromVariant(p.getValue());
        propsArray.append(propObj);
    }
    root["properties"] = propsArray;
    return root;
}

bool ArtifactPresetManager::applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json) {
    if (!effect || json.isEmpty()) return false;

    if (json.contains("mask_enabled")) {
        effect->setMaskEnabled(json["mask_enabled"].toBool());
    }
    if (json.contains("mask_layer_id")) {
        effect->setMaskLayerId(json["mask_layer_id"].toString());
    }
    if (json.contains("mask_name")) {
        effect->setMaskName(json["mask_name"].toString());
    }
    if (json.contains("mask_inverted")) {
        effect->setMaskInverted(json["mask_inverted"].toBool());
    }
    if (json.contains("mask_opacity")) {
        effect->setMaskOpacity(static_cast<float>(json["mask_opacity"].toDouble()));
    }

    // Check effect ID match? Or just apply what we can.
    QJsonArray propsArray = json["properties"].toArray();
    for (int i = 0; i < propsArray.size(); ++i) {
        QJsonObject propObj = propsArray[i].toObject();
        QString name = propObj["name"].toString();
        QVariant value = propObj["value"].toVariant();
        
        effect->setPropertyValue(UniString(name.toStdString()), value);
    }
    return true;
}

namespace {

QJsonObject maskVertexToJson(const MaskVertex& vertex)
{
    QJsonObject obj;
    obj["px"] = vertex.position.x();
    obj["py"] = vertex.position.y();
    obj["ix"] = vertex.inTangent.x();
    obj["iy"] = vertex.inTangent.y();
    obj["ox"] = vertex.outTangent.x();
    obj["oy"] = vertex.outTangent.y();
    return obj;
}

MaskVertex maskVertexFromJson(const QJsonObject& obj)
{
    MaskVertex vertex;
    vertex.position = QPointF(obj.value("px").toDouble(), obj.value("py").toDouble());
    vertex.inTangent = QPointF(obj.value("ix").toDouble(), obj.value("iy").toDouble());
    vertex.outTangent = QPointF(obj.value("ox").toDouble(), obj.value("oy").toDouble());
    return vertex;
}

QJsonObject maskPathSnapshotToJson(const MaskPathKeyframeSnapshot& snapshot)
{
    QJsonObject obj;
    obj["frame"] = static_cast<qint64>(snapshot.frame);
    obj["closed"] = snapshot.closed;
    obj["opacity"] = static_cast<double>(snapshot.opacity);
    obj["feather"] = static_cast<double>(snapshot.feather);
    obj["featherHorizontal"] = static_cast<double>(snapshot.featherHorizontal);
    obj["featherVertical"] = static_cast<double>(snapshot.featherVertical);
    obj["featherInner"] = static_cast<double>(snapshot.featherInner);
    obj["featherOuter"] = static_cast<double>(snapshot.featherOuter);
    obj["expansion"] = static_cast<double>(snapshot.expansion);
    obj["inverted"] = snapshot.inverted;
    obj["mode"] = static_cast<int>(snapshot.mode);
    obj["name"] = snapshot.name.toQString();
    QJsonArray vertsArray;
    for (const auto& vertex : snapshot.vertices) {
        vertsArray.append(maskVertexToJson(vertex));
    }
    obj["vertices"] = vertsArray;
    return obj;
}

MaskPathKeyframeSnapshot maskPathSnapshotFromJson(const QJsonObject& obj)
{
    MaskPathKeyframeSnapshot snapshot;
    snapshot.frame = obj.value("frame").toVariant().toLongLong();
    snapshot.closed = obj.value("closed").toBool(true);
    snapshot.opacity = static_cast<float>(obj.value("opacity").toDouble(1.0));
    snapshot.feather = static_cast<float>(obj.value("feather").toDouble(0.0));
    snapshot.featherHorizontal = static_cast<float>(obj.value("featherHorizontal").toDouble(0.0));
    snapshot.featherVertical = static_cast<float>(obj.value("featherVertical").toDouble(0.0));
    snapshot.featherInner = static_cast<float>(obj.value("featherInner").toDouble(0.0));
    snapshot.featherOuter = static_cast<float>(obj.value("featherOuter").toDouble(0.0));
    snapshot.expansion = static_cast<float>(obj.value("expansion").toDouble(0.0));
    snapshot.inverted = obj.value("inverted").toBool(false);
    snapshot.mode = static_cast<MaskMode>(obj.value("mode").toInt(static_cast<int>(MaskMode::Add)));
    snapshot.name = UniString(obj.value("name").toString().toStdString());

    const auto vertsArray = obj.value("vertices").toArray();
    snapshot.vertices.reserve(vertsArray.size());
    for (const auto& vertVal : vertsArray) {
        if (vertVal.isObject()) {
            snapshot.vertices.push_back(maskVertexFromJson(vertVal.toObject()));
        }
    }
    return snapshot;
}

QJsonObject maskPathToJson(const MaskPath& path)
{
    QJsonObject obj;
    QJsonArray vertsArray;
    for (int vi = 0; vi < path.vertexCount(); ++vi) {
        vertsArray.append(maskVertexToJson(path.vertex(vi)));
    }
    obj["vertices"] = vertsArray;
    obj["closed"] = path.isClosed();
    obj["opacity"] = static_cast<double>(path.opacity());
    obj["feather"] = static_cast<double>(path.feather());
    obj["featherHorizontal"] = static_cast<double>(path.featherHorizontal());
    obj["featherVertical"] = static_cast<double>(path.featherVertical());
    obj["featherInner"] = static_cast<double>(path.featherInner());
    obj["featherOuter"] = static_cast<double>(path.featherOuter());
    obj["expansion"] = static_cast<double>(path.expansion());
    obj["inverted"] = path.isInverted();
    obj["mode"] = static_cast<int>(path.mode());
    obj["name"] = path.name().toQString();

    if (path.hasAnimationKeyframes()) {
        QJsonArray kfArray;
        for (const auto& kf : path.animationKeyframes()) {
            kfArray.append(maskPathSnapshotToJson(kf));
        }
        obj["animationKeyframes"] = kfArray;
    }
    return obj;
}

MaskPath maskPathFromJson(const QJsonObject& obj)
{
    MaskPath path;
    const auto vertsArray = obj.value("vertices").toArray();
    for (const auto& vertVal : vertsArray) {
        if (vertVal.isObject()) {
            path.addVertex(maskVertexFromJson(vertVal.toObject()));
        }
    }
    path.setClosed(obj.value("closed").toBool(true));
    path.setOpacity(static_cast<float>(obj.value("opacity").toDouble(1.0)));
    path.setFeather(static_cast<float>(obj.value("feather").toDouble(0.0)));
    path.setFeatherHorizontal(static_cast<float>(obj.value("featherHorizontal").toDouble(0.0)));
    path.setFeatherVertical(static_cast<float>(obj.value("featherVertical").toDouble(0.0)));
    path.setFeatherInner(static_cast<float>(obj.value("featherInner").toDouble(0.0)));
    path.setFeatherOuter(static_cast<float>(obj.value("featherOuter").toDouble(0.0)));
    path.setExpansion(static_cast<float>(obj.value("expansion").toDouble(0.0)));
    path.setInverted(obj.value("inverted").toBool(false));
    path.setMode(static_cast<MaskMode>(obj.value("mode").toInt(static_cast<int>(MaskMode::Add))));
    path.setName(UniString(obj.value("name").toString().toStdString()));

    const auto kfArray = obj.value("animationKeyframes").toArray();
    for (const auto& kfVal : kfArray) {
        if (kfVal.isObject()) {
            const auto snapshot = maskPathSnapshotFromJson(kfVal.toObject());
            path.setAnimationKeyframe(snapshot.frame, snapshot);
        }
    }
    return path;
}

} // namespace

QJsonObject ArtifactPresetManager::maskToPresetJson(const LayerMask& mask) {
    QJsonObject root;
    root["enabled"] = mask.isEnabled();

    QJsonArray pathsArray;
    for (int i = 0; i < mask.maskPathCount(); ++i) {
        pathsArray.append(maskPathToJson(mask.maskPath(i)));
    }
    root["paths"] = pathsArray;
    return root;
}

bool ArtifactPresetManager::applyPresetJsonToMask(LayerMask& mask, const QJsonObject& json) {
    if (json.isEmpty()) {
        return false;
    }

    mask.clearMaskPaths();
    mask.setEnabled(json.value("enabled").toBool(true));

    const auto pathsArray = json.value("paths").toArray();
    for (const auto& pathVal : pathsArray) {
        if (!pathVal.isObject()) {
            continue;
        }
        mask.addMaskPath(maskPathFromJson(pathVal.toObject()));
    }
    return true;
}

bool ArtifactPresetManager::saveMaskPreset(const LayerMask& mask, const QString& filePath) {
    const QJsonObject json = maskToPresetJson(mask);
    if (json.isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool ArtifactPresetManager::loadMaskPreset(LayerMask& mask, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    return applyPresetJsonToMask(mask, doc.object());
}

bool ArtifactPresetManager::saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath) {
    QJsonObject json = effectToPresetJson(effect);
    if (json.isEmpty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool ArtifactPresetManager::loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return false;

    return applyPresetJsonToEffect(effect, doc.object());
}

bool ArtifactPresetManager::saveColorPaletteMapping(const ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath) {
    return manager.saveToFile(filePath);
}

bool ArtifactPresetManager::loadColorPaletteMapping(ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath) {
    return manager.loadFromFile(filePath);
}

} // namespace Artifact
