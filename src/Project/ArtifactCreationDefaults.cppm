module;
#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>
#include <QFile>

module Artifact.Project.CreationDefaults;

import std;

namespace Artifact {
namespace {

bool isExplicit(const CreationCompositionDefaults& value)
{
    return value.composition.isValid();
}

bool isExplicit(const CreationLayerDefaults& value)
{
    return value.layer.has_value() || !value.sourcePath.isEmpty();
}

}

QJsonObject CreationCompositionDefaults::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("compositionName"), composition.compositionName().toQString());
    json.insert(QStringLiteral("width"), composition.width());
    json.insert(QStringLiteral("height"), composition.height());
    json.insert(QStringLiteral("frameRate"), composition.frameRate().framerate());
    const FloatColor bg = composition.backgroundColor();
    QJsonObject color;
    color.insert(QStringLiteral("r"), bg.red());
    color.insert(QStringLiteral("g"), bg.green());
    color.insert(QStringLiteral("b"), bg.blue());
    color.insert(QStringLiteral("a"), bg.alpha());
    json.insert(QStringLiteral("backgroundColor"), color);
    return json;
}

bool CreationCompositionDefaults::fromJson(const QJsonObject& json)
{
    composition = ArtifactCompositionInitParams();
    if (json.isEmpty()) {
        return true;
    }
    if (json.contains(QStringLiteral("compositionName"))) {
        composition.setCompositionName(UniString(json.value(QStringLiteral("compositionName")).toString()));
    }
    if (json.contains(QStringLiteral("width")) && json.contains(QStringLiteral("height"))) {
        composition.setResolution(json.value(QStringLiteral("width")).toInt(), json.value(QStringLiteral("height")).toInt());
    }
    if (json.contains(QStringLiteral("frameRate"))) {
        composition.setFrameRate(json.value(QStringLiteral("frameRate")).toDouble());
    }
    if (json.contains(QStringLiteral("backgroundColor"))) {
        const auto colorObj = json.value(QStringLiteral("backgroundColor")).toObject();
        composition.setBackgroundColor(FloatColor{
            static_cast<float>(colorObj.value(QStringLiteral("r")).toDouble(0.0)),
            static_cast<float>(colorObj.value(QStringLiteral("g")).toDouble(0.0)),
            static_cast<float>(colorObj.value(QStringLiteral("b")).toDouble(0.0)),
            static_cast<float>(colorObj.value(QStringLiteral("a")).toDouble(1.0))
        });
    }
    return true;
}

QJsonObject CreationLayerDefaults::toJson() const
{
    QJsonObject json;
    if (layer.has_value()) {
        json.insert(QStringLiteral("layerType"), static_cast<int>(layer->layerType()));
        json.insert(QStringLiteral("name"), layer->name().toQString());
    }
    if (!sourcePath.isEmpty()) {
        json.insert(QStringLiteral("sourcePath"), sourcePath);
    }
    return json;
}

bool CreationLayerDefaults::fromJson(const QJsonObject& json)
{
    const auto layerType = static_cast<LayerType>(json.value(QStringLiteral("layerType")).toInt(static_cast<int>(LayerType::Null)));
    const QString name = json.value(QStringLiteral("name")).toString();
    layer = ArtifactLayerInitParams(name, layerType);
    sourcePath = json.value(QStringLiteral("sourcePath")).toString();
    return true;
}

QJsonObject CreationDefaultsBundle::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("composition"), composition.toJson());
    json.insert(QStringLiteral("shape"), shape.toJson());
    json.insert(QStringLiteral("text"), text.toJson());
    json.insert(QStringLiteral("image"), image.toJson());
    return json;
}

bool CreationDefaultsBundle::fromJson(const QJsonObject& json)
{
    composition.fromJson(json.value(QStringLiteral("composition")).toObject());
    shape.fromJson(json.value(QStringLiteral("shape")).toObject());
    text.fromJson(json.value(QStringLiteral("text")).toObject());
    image.fromJson(json.value(QStringLiteral("image")).toObject());
    return true;
}

QJsonObject CreationDefaultsState::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("globalDefaults"), globalDefaults.toJson());
    json.insert(QStringLiteral("projectDefaults"), projectDefaults.toJson());
    json.insert(QStringLiteral("lastUsed"), lastUsed.toJson());
    json.insert(QStringLiteral("presets"), presets);
    return json;
}

bool CreationDefaultsState::fromJson(const QJsonObject& json)
{
    globalDefaults.fromJson(json.value(QStringLiteral("globalDefaults")).toObject());
    projectDefaults.fromJson(json.value(QStringLiteral("projectDefaults")).toObject());
    lastUsed.fromJson(json.value(QStringLiteral("lastUsed")).toObject());
    presets = json.value(QStringLiteral("presets")).toObject();
    return true;
}

void CreationDefaultResolver::setGlobalDefaults(const CreationDefaultsBundle& defaults) { globalDefaults_ = defaults; }
void CreationDefaultResolver::setProjectDefaults(const CreationDefaultsBundle& defaults) { projectDefaults_ = defaults; }
void CreationDefaultResolver::setLastUsedDefaults(const CreationDefaultsBundle& defaults) { lastUsedDefaults_ = defaults; }
void CreationDefaultResolver::setPreset(const QString& presetName, const CreationDefaultsBundle& defaults)
{
    activePresetName_ = presetName;
    activePresetDefaults_ = defaults;
}
void CreationDefaultResolver::clearPreset()
{
    activePresetName_.clear();
    activePresetDefaults_ = CreationDefaultsBundle();
}

CreationCompositionDefaults CreationDefaultResolver::resolveComposition(
    const CreationCompositionDefaults& explicitPreset) const
{
    if (!activePresetName_.isEmpty()) {
        return activePresetDefaults_.composition;
    }
    if (isExplicit(explicitPreset)) {
        return explicitPreset;
    }
    if (isExplicit(lastUsedDefaults_.composition)) {
        return lastUsedDefaults_.composition;
    }
    if (isExplicit(projectDefaults_.composition)) {
        return projectDefaults_.composition;
    }
    return globalDefaults_.composition;
}

CreationLayerDefaults CreationDefaultResolver::resolveShape(const CreationLayerDefaults& explicitPreset) const
{
    if (!activePresetName_.isEmpty()) {
        return activePresetDefaults_.shape;
    }
    if (isExplicit(explicitPreset)) {
        return explicitPreset;
    }
    if (isExplicit(lastUsedDefaults_.shape)) {
        return lastUsedDefaults_.shape;
    }
    if (isExplicit(projectDefaults_.shape)) {
        return projectDefaults_.shape;
    }
    return globalDefaults_.shape;
}

CreationLayerDefaults CreationDefaultResolver::resolveText(const CreationLayerDefaults& explicitPreset) const
{
    if (!activePresetName_.isEmpty()) {
        return activePresetDefaults_.text;
    }
    if (isExplicit(explicitPreset)) {
        return explicitPreset;
    }
    if (isExplicit(lastUsedDefaults_.text)) {
        return lastUsedDefaults_.text;
    }
    if (isExplicit(projectDefaults_.text)) {
        return projectDefaults_.text;
    }
    return globalDefaults_.text;
}

CreationLayerDefaults CreationDefaultResolver::resolveImage(const CreationLayerDefaults& explicitPreset) const
{
    if (!activePresetName_.isEmpty()) {
        return activePresetDefaults_.image;
    }
    if (isExplicit(explicitPreset)) {
        return explicitPreset;
    }
    if (isExplicit(lastUsedDefaults_.image)) {
        return lastUsedDefaults_.image;
    }
    if (isExplicit(projectDefaults_.image)) {
        return projectDefaults_.image;
    }
    return globalDefaults_.image;
}

QJsonObject CreationDefaultResolver::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("globalDefaults"), globalDefaults_.toJson());
    json.insert(QStringLiteral("projectDefaults"), projectDefaults_.toJson());
    json.insert(QStringLiteral("lastUsedDefaults"), lastUsedDefaults_.toJson());
    json.insert(QStringLiteral("activePresetName"), activePresetName_);
    json.insert(QStringLiteral("activePresetDefaults"), activePresetDefaults_.toJson());
    return json;
}

bool CreationDefaultResolver::fromJson(const QJsonObject& json)
{
    globalDefaults_.fromJson(json.value(QStringLiteral("globalDefaults")).toObject());
    projectDefaults_.fromJson(json.value(QStringLiteral("projectDefaults")).toObject());
    lastUsedDefaults_.fromJson(json.value(QStringLiteral("lastUsedDefaults")).toObject());
    activePresetName_ = json.value(QStringLiteral("activePresetName")).toString();
    activePresetDefaults_.fromJson(json.value(QStringLiteral("activePresetDefaults")).toObject());
    return true;
}

}
