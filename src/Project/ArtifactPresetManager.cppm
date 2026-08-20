module;

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QByteArray>
#include <QColor>

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
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Artifact.Color.Palette;

namespace Artifact {

namespace {
constexpr qint64 kMaxPresetFileBytes = 16LL * 1024LL * 1024LL;
}

namespace {

QJsonObject imageToJson(const ArtifactCore::SharedPtr<ImageF32x4_RGBA>& image)
{
    QJsonObject obj;
    if (!image || image->width() <= 0 || image->height() <= 0) {
        return obj;
    }

    obj["width"] = image->width();
    obj["height"] = image->height();
    const std::size_t byteCount = image->totalPixels() * 4u * sizeof(float);
    if (byteCount == 0 || !image->rgba32fData()) {
        return obj;
    }

    const QByteArray bytes(
        reinterpret_cast<const char*>(image->rgba32fData()),
        static_cast<qsizetype>(byteCount));
    obj["pixels_b64"] = QString::fromLatin1(bytes.toBase64());
    return obj;
}

ArtifactCore::SharedPtr<ImageF32x4_RGBA> imageFromJson(const QJsonObject& obj)
{
    const int width = obj.value(QStringLiteral("width")).toInt(0);
    const int height = obj.value(QStringLiteral("height")).toInt(0);
    if (width <= 0 || height <= 0) {
        return {};
    }

    auto image = ArtifactCore::makeShared<ImageF32x4_RGBA>();
    image->resize(width, height);
    image->fillAlpha(0.0f);

    const QString pixelsB64 = obj.value(QStringLiteral("pixels_b64")).toString();
    if (!pixelsB64.isEmpty()) {
        const QByteArray bytes = QByteArray::fromBase64(pixelsB64.toLatin1());
        const std::size_t requiredBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u * sizeof(float);
        if (bytes.size() >= static_cast<qsizetype>(requiredBytes)) {
            image->setFromRGBA32F(reinterpret_cast<const float*>(bytes.constData()), width, height);
        }
    }
    return image;
}

} // namespace

QJsonObject ArtifactPresetManager::effectToPresetJson(const ArtifactAbstractEffectPtr& effect) {
    if (!effect) return {};

    QJsonObject root;
    root["schema_version"] = 2;
    root["effect_id"] = effect->effectID().toQString();
    root["display_name"] = effect->displayName().toQString();
    root["effect_enabled"] = effect->isEnabled();
    root["effect_mix"] = effect->mix();
    root["allow_overscan"] = effect->allowOverscan();
    root["mask_enabled"] = effect->maskEnabled();
    root["mask_layer_id"] = effect->maskLayerId();
    root["mask_name"] = effect->maskName();
    root["mask_inverted"] = effect->maskInverted();
    root["mask_opacity"] = effect->maskOpacity();
    if (effect->maskImage()) {
        root["mask_image"] = imageToJson(effect->maskImage());
    }

    QJsonArray effectMaskArray;
    for (int i = 0; i < effect->effectMaskImageCount(); ++i) {
        effectMaskArray.append(imageToJson(effect->effectMaskImage(i)));
    }
    root["effect_mask_images"] = effectMaskArray;

    QJsonArray propsArray;
    auto props = effect->getProperties();
    for (const auto& p : props) {
        QJsonObject propObj;
        propObj["name"] = p.getName();
        propObj["parameter_id"] = p.getName();
        propObj["type"] = static_cast<int>(p.getType());
        switch (p.getType()) {
        case ArtifactCore::PropertyType::Float:
            propObj["value_type"] = QStringLiteral("double");
            break;
        case ArtifactCore::PropertyType::Integer:
            propObj["value_type"] = QStringLiteral("integer");
            break;
        case ArtifactCore::PropertyType::Boolean:
            propObj["value_type"] = QStringLiteral("boolean");
            break;
        case ArtifactCore::PropertyType::Color:
            propObj["value_type"] = QStringLiteral("color");
            break;
        case ArtifactCore::PropertyType::String:
            propObj["value_type"] = QStringLiteral("string");
            break;
        case ArtifactCore::PropertyType::ObjectReference:
            propObj["value_type"] = QStringLiteral("object_reference");
            break;
        case ArtifactCore::PropertyType::Point2D:
            propObj["value_type"] = QStringLiteral("point2d");
            break;
        }
        if (p.getType() == ArtifactCore::PropertyType::Color) {
            propObj["value"] = p.getValue().value<QColor>().name(QColor::HexArgb);
        } else {
            propObj["value"] = QJsonValue::fromVariant(p.getValue());
        }
        propsArray.append(propObj);
    }
    root["properties"] = propsArray;
    return root;
}

bool ArtifactPresetManager::applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json) {
    if (!effect || json.isEmpty()) return false;
    const int schemaVersion = json.value(QStringLiteral("schema_version")).toInt(1);
    if (schemaVersion < 1 || schemaVersion > 2) return false;

    if (json.contains("mask_enabled")) {
        effect->setMaskEnabled(json["mask_enabled"].toBool());
    }
    if (json.contains("effect_enabled")) {
        effect->setEnabled(json["effect_enabled"].toBool(true));
    }
    if (json.contains("effect_mix")) {
        effect->setMix(static_cast<float>(json["effect_mix"].toDouble(1.0)));
    }
    if (json.contains("allow_overscan")) {
        effect->setAllowOverscan(json["allow_overscan"].toBool(false));
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
    if (json.contains("mask_image")) {
        if (json["mask_image"].isObject()) {
            effect->setMaskImage(imageFromJson(json["mask_image"].toObject()));
        } else {
            effect->setMaskImage({});
        }
    }

    if (json.contains("effect_mask_images")) {
        effect->clearEffectMaskImages();
        const QJsonArray effectMaskArray = json["effect_mask_images"].toArray();
        for (const auto& value : effectMaskArray) {
            if (value.isObject()) {
                auto maskImage = imageFromJson(value.toObject());
                if (maskImage) {
                    effect->addEffectMaskImage(maskImage);
                }
            }
        }
    }

    // Check effect ID match? Or just apply what we can.
    QJsonArray propsArray = json["properties"].toArray();
    for (int i = 0; i < propsArray.size(); ++i) {
        QJsonObject propObj = propsArray[i].toObject();
        QString name = propObj.value(QStringLiteral("parameter_id")).toString(
            propObj.value(QStringLiteral("name")).toString());
        if (name.isEmpty()) continue;
        const QJsonValue jsonValue = propObj.value(QStringLiteral("value"));
        const QString valueType = propObj.value(QStringLiteral("value_type")).toString();
        QVariant value;
        if (valueType == QStringLiteral("integer")) value = jsonValue.toInt();
        else if (valueType == QStringLiteral("boolean")) value = jsonValue.toBool();
        else if (valueType == QStringLiteral("double")) value = jsonValue.toDouble();
        else if (valueType == QStringLiteral("string")) value = jsonValue.toString();
        else if (valueType == QStringLiteral("color")) value = QColor(jsonValue.toString());
        else value = jsonValue.toVariant();
        
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
    snapshot.mode = static_cast<MaskMode>(std::clamp(
        obj.value("mode").toInt(static_cast<int>(MaskMode::Add)), 0, 3));
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
    path.setMode(static_cast<MaskMode>(std::clamp(
        obj.value("mode").toInt(static_cast<int>(MaskMode::Add)), 0, 3)));
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
    const QString normalizedPath = filePath.trimmed();
    if (json.isEmpty() || normalizedPath.isEmpty()) {
        return false;
    }

    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QJsonDocument doc(json);
    const QByteArray payload = doc.toJson();
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool ArtifactPresetManager::loadMaskPreset(LayerMask& mask, const QString& filePath) {
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return false;
    }
    QFile file(normalizedPath);
    if (file.size() <= 0 || file.size() > kMaxPresetFileBytes ||
        !file.open(QIODevice::ReadOnly)) {
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
    const QString normalizedPath = filePath.trimmed();
    if (json.isEmpty() || normalizedPath.isEmpty()) return false;

    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(json);
    const QByteArray payload = doc.toJson();
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool ArtifactPresetManager::loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) {
    const QString normalizedPath = filePath.trimmed();
    if (!effect || normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return false;
    }
    QFile file(normalizedPath);
    if (file.size() <= 0 || file.size() > kMaxPresetFileBytes ||
        !file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return false;

    return applyPresetJsonToEffect(effect, doc.object());
}

bool ArtifactPresetManager::saveColorPaletteMapping(const ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath) {
    const QString normalizedPath = filePath.trimmed();
    return !normalizedPath.isEmpty() && manager.saveToFile(normalizedPath);
}

bool ArtifactPresetManager::loadColorPaletteMapping(ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath) {
    const QString normalizedPath = filePath.trimmed();
    return !normalizedPath.isEmpty() && QFileInfo::exists(normalizedPath) &&
           manager.loadFromFile(normalizedPath);
}

} // namespace Artifact
