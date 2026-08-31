module;

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
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
ArtifactCreationPreset makeCreationPreset(const char* id, const char* name,
    const char* description, ArtifactCreationPresetKind kind,
    ArtifactCreationLayerKind layer, ArtifactCreationMaskKind mask,
    int width = 1920, int height = 1080, float inset = 0.0f)
{
    ArtifactCreationPreset preset;
    preset.id = QString::fromUtf8(id);
    preset.displayName = QString::fromUtf8(name);
    preset.description = QString::fromUtf8(description);
    preset.kind = kind;
    preset.layerKind = layer;
    preset.maskKind = mask;
    preset.width = width;
    preset.height = height;
    preset.maskInset = inset;
    return preset;
}

ArtifactCreationPreset makeCompositionPreset()
{
    auto preset = makeCreationPreset("composition.default", "空のコンポジション",
        "標準サイズの空コンポジション", ArtifactCreationPresetKind::Composition,
        ArtifactCreationLayerKind::Solid, ArtifactCreationMaskKind::None);
    preset.backgroundColor = QStringLiteral("#00000000");
    preset.frameRate = 30.0;
    preset.durationFrames = 300;
    return preset;
}

ArtifactCreationPreset makeCircleCompositionPreset()
{
    auto preset = makeCreationPreset("composition.circle-card", "円形カード",
        "背景と円形マスク平面を含むコンポジション",
        ArtifactCreationPresetKind::Composition, ArtifactCreationLayerKind::Solid,
        ArtifactCreationMaskKind::None, 1080, 1080);
    preset.backgroundColor = QStringLiteral("#20232AFF");
    preset.frameRate = 30.0;
    preset.durationFrames = 150;
    preset.layers.push_back(makeCreationPreset("layer.background", "背景",
        "コンポジション背景", ArtifactCreationPresetKind::Layer,
        ArtifactCreationLayerKind::Solid, ArtifactCreationMaskKind::None,
        1080, 1080));
    preset.layers.push_back(makeCreationPreset("layer.image.circle", "円形画像",
        "円形マスク付き平面", ArtifactCreationPresetKind::Layer,
        ArtifactCreationLayerKind::Image, ArtifactCreationMaskKind::Circle,
        1080, 1080, 0.08f));
    return preset;
}
}

std::vector<ArtifactCreationPreset> ArtifactPresetManager::standardCreationPresets()
{
    return {
        makeCompositionPreset(),
        makeCircleCompositionPreset(),
        makeCreationPreset("layer.solid", "平面", "マスクなしの矩形平面", ArtifactCreationPresetKind::Layer, ArtifactCreationLayerKind::Solid, ArtifactCreationMaskKind::None),
        makeCreationPreset("layer.solid.circle", "円形マスク平面", "円形マスクを持つ平面", ArtifactCreationPresetKind::Layer, ArtifactCreationLayerKind::Solid, ArtifactCreationMaskKind::Circle, 1920, 1080, 0.08f),
        makeCreationPreset("layer.solid.rounded", "角丸マスク平面", "角丸矩形マスクを持つ平面", ArtifactCreationPresetKind::Layer, ArtifactCreationLayerKind::Solid, ArtifactCreationMaskKind::RoundedRectangle, 1920, 1080, 0.08f),
        makeCreationPreset("layer.shape", "シェイプ", "シェイプ編集用のレイヤー", ArtifactCreationPresetKind::Layer, ArtifactCreationLayerKind::Shape, ArtifactCreationMaskKind::None),
        makeCreationPreset("layer.text", "テキスト", "テキスト編集用のレイヤー", ArtifactCreationPresetKind::Layer, ArtifactCreationLayerKind::Text, ArtifactCreationMaskKind::None)
    };
}

std::optional<ArtifactCreationPreset> ArtifactPresetManager::creationPreset(const QString& id)
{
    const QString normalized = id.trimmed();
    for (const auto& preset : standardCreationPresets()) {
        if (preset.id == normalized) return preset;
    }
    return std::nullopt;
}

QJsonObject ArtifactPresetManager::creationPresetToJson(const ArtifactCreationPreset& preset)
{
    QJsonObject json;
    json["schema_version"] = 1;
    json["id"] = preset.id;
    json["display_name"] = preset.displayName;
    json["description"] = preset.description;
    json["kind"] = static_cast<int>(preset.kind);
    json["layer_kind"] = static_cast<int>(preset.layerKind);
    json["mask_kind"] = static_cast<int>(preset.maskKind);
    json["width"] = preset.width;
    json["height"] = preset.height;
    json["mask_inset"] = preset.maskInset;
    json["background_color"] = preset.backgroundColor;
    json["frame_rate"] = preset.frameRate;
    json["duration_frames"] = preset.durationFrames;
    QJsonArray layers;
    for (const auto& layer : preset.layers) {
        layers.append(creationPresetToJson(layer));
    }
    json["layers"] = layers;
    return json;
}

std::optional<ArtifactCreationPreset> ArtifactPresetManager::creationPresetFromJson(const QJsonObject& json)
{
    if (json.isEmpty() || json.value("id").toString().trimmed().isEmpty()) return std::nullopt;
    ArtifactCreationPreset preset;
    preset.id = json.value("id").toString().trimmed();
    preset.displayName = json.value("display_name").toString(preset.id);
    preset.description = json.value("description").toString();
    preset.kind = static_cast<ArtifactCreationPresetKind>(std::clamp(json.value("kind").toInt(1), 0, 1));
    preset.layerKind = static_cast<ArtifactCreationLayerKind>(std::clamp(json.value("layer_kind").toInt(0), 0, 3));
    preset.maskKind = static_cast<ArtifactCreationMaskKind>(std::clamp(json.value("mask_kind").toInt(0), 0, 4));
    preset.width = std::clamp(json.value("width").toInt(1920), 1, 16384);
    preset.height = std::clamp(json.value("height").toInt(1080), 1, 16384);
    preset.maskInset = std::clamp(static_cast<float>(json.value("mask_inset").toDouble(0.0)), 0.0f, 1.0f);
    preset.backgroundColor = json.value("background_color").toString(QStringLiteral("#00000000"));
    preset.frameRate = std::clamp(json.value("frame_rate").toDouble(30.0), 1.0, 240.0);
    preset.durationFrames = std::clamp(json.value("duration_frames").toInt(300), 1, 1000000);
    const QJsonArray layers = json.value("layers").toArray();
    for (const auto& value : layers) {
        if (value.isObject()) {
            if (auto layer = creationPresetFromJson(value.toObject())) {
                preset.layers.push_back(*layer);
            }
        }
    }
    return preset;
}

bool ArtifactPresetManager::isValidCreationPreset(const ArtifactCreationPreset& preset)
{
    if (preset.id.trimmed().isEmpty() || preset.displayName.trimmed().isEmpty() ||
        preset.width <= 0 || preset.height <= 0 ||
        preset.width > 16384 || preset.height > 16384 ||
        !std::isfinite(preset.frameRate) || preset.frameRate < 1.0 ||
        preset.frameRate > 240.0 || preset.durationFrames <= 0 ||
        preset.durationFrames > 1000000 || !std::isfinite(preset.maskInset) ||
        preset.maskInset < 0.0f || preset.maskInset > 1.0f) {
        return false;
    }
    if (preset.kind == ArtifactCreationPresetKind::Layer && !preset.layers.empty()) {
        return false;
    }
    for (const auto& layer : preset.layers) {
        if (!isValidCreationPreset(layer) ||
            layer.kind != ArtifactCreationPresetKind::Layer) {
            return false;
        }
    }
    return true;
}

std::vector<ArtifactCreationPreset> ArtifactPresetManager::layerCreationPlan(
    const ArtifactCreationPreset& preset)
{
    if (!isValidCreationPreset(preset)) return {};
    if (preset.kind == ArtifactCreationPresetKind::Layer) return {preset};
    return preset.layers;
}

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

    const QString serializedEffectId =
        json.value(QStringLiteral("effect_id")).toString().trimmed();
    const QString targetEffectId = effect->effectID().toQString().trimmed();
    if (!serializedEffectId.isEmpty() && !targetEffectId.isEmpty() &&
        serializedEffectId != targetEffectId) {
        qWarning() << "[ArtifactPresetManager] effect preset target mismatch"
                   << "preset=" << serializedEffectId
                   << "target=" << targetEffectId;
        return false;
    }

    // Validate known property payloads before mutating any part of the effect.
    // Unknown properties remain forward-compatible, but malformed values for
    // properties owned by this effect must not produce a partial preset load.
    const QJsonValue propertiesValue = json.value(QStringLiteral("properties"));
    if (!propertiesValue.isUndefined() && !propertiesValue.isArray()) {
        return false;
    }
    const QJsonArray propsArray = propertiesValue.toArray();
    for (const auto& rawProperty : propsArray) {
        if (!rawProperty.isObject()) {
            return false;
        }
        const QJsonObject propertyObject = rawProperty.toObject();
        const QString name = propertyObject.value(QStringLiteral("parameter_id")).toString(
            propertyObject.value(QStringLiteral("name")).toString()).trimmed();
        if (name.isEmpty()) {
            return false;
        }
        if (!effect->editableProperty(name)) {
            continue;
        }
        const QJsonValue jsonValue = propertyObject.value(QStringLiteral("value"));
        if (jsonValue.isUndefined()) {
            return false;
        }
        const QString valueType = propertyObject.value(QStringLiteral("value_type")).toString();
        if (valueType == QStringLiteral("integer")) {
            if (!jsonValue.isDouble() || jsonValue.toDouble() != std::floor(jsonValue.toDouble())) {
                return false;
            }
        } else if (valueType == QStringLiteral("double")) {
            if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble())) {
                return false;
            }
        } else if (valueType == QStringLiteral("boolean")) {
            if (!jsonValue.isBool()) {
                return false;
            }
        } else if (valueType == QStringLiteral("string")) {
            if (!jsonValue.isString()) {
                return false;
            }
        } else if (valueType == QStringLiteral("color")) {
            if (!jsonValue.isString() || !QColor(jsonValue.toString()).isValid()) {
                return false;
            }
        }
    }

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

    const QJsonValue pathsValue = json.value(QStringLiteral("paths"));
    if (!pathsValue.isUndefined() && !pathsValue.isArray()) {
        return false;
    }
    const auto pathsArray = pathsValue.toArray();
    const auto finiteNumber = [](const QJsonValue& value) {
        return !value.isUndefined() && value.isDouble() &&
               std::isfinite(value.toDouble());
    };
    for (const auto& pathVal : pathsArray) {
        if (!pathVal.isObject()) {
            return false;
        }
        const QJsonObject pathObject = pathVal.toObject();
        const QJsonValue verticesValue = pathObject.value(QStringLiteral("vertices"));
        if (!verticesValue.isUndefined() && !verticesValue.isArray()) {
            return false;
        }
        for (const auto& vertexVal : verticesValue.toArray()) {
            if (!vertexVal.isObject()) {
                return false;
            }
            const QJsonObject vertexObject = vertexVal.toObject();
            for (const auto& key : {QStringLiteral("px"), QStringLiteral("py"),
                                    QStringLiteral("ix"), QStringLiteral("iy"),
                                    QStringLiteral("ox"), QStringLiteral("oy")}) {
                if (vertexObject.contains(key) &&
                    !finiteNumber(vertexObject.value(key))) {
                    return false;
                }
            }
        }
        const QJsonValue keyframesValue =
            pathObject.value(QStringLiteral("animationKeyframes"));
        if (!keyframesValue.isUndefined() && !keyframesValue.isArray()) {
            return false;
        }
        for (const auto& keyframeVal : keyframesValue.toArray()) {
            if (!keyframeVal.isObject()) {
                return false;
            }
        }
    }

    mask.clearMaskPaths();
    mask.setEnabled(json.value("enabled").toBool(true));

    for (const auto& pathVal : pathsArray) {
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
