module;

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QColor>
#include <QUuid>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

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
module Artifact.Effect.Preset;




import Artifact.Effect.Abstract;

namespace Artifact
{

// ==================== ArtifactEffectPreset::Impl ====================

class ArtifactEffectPreset::Impl
{
public:
    PresetID id_;
    QString name_;
    QString category_;
    QString description_;

    struct ParameterData
    {
        Parameter::Type type;
        QString name;
        float floatValue = 0.0f;
        QColor colorValue;
        QString stringValue;
    };
    QVector<ParameterData> parameters_;

    QByteArray thumbnail_;
};

// ==================== ArtifactEffectPreset ====================

ArtifactEffectPreset::ArtifactEffectPreset()
    : impl_(new Impl())
{
    impl_->id_ = QUuid::createUuid().toString();
}

ArtifactEffectPreset::ArtifactEffectPreset(const QString& name)
    : impl_(new Impl())
{
    impl_->id_ = QUuid::createUuid().toString();
    impl_->name_ = name;
}

ArtifactEffectPreset::~ArtifactEffectPreset()
{
    delete impl_;
}

ArtifactEffectPreset::PresetID ArtifactEffectPreset::id() const
{
    return impl_->id_;
}

void ArtifactEffectPreset::setId(const PresetID& id)
{
    impl_->id_ = id;
}

QString ArtifactEffectPreset::name() const
{
    return impl_->name_;
}

void ArtifactEffectPreset::setName(const QString& name)
{
    impl_->name_ = name;
}

QString ArtifactEffectPreset::category() const
{
    return impl_->category_;
}

void ArtifactEffectPreset::setCategory(const QString& category)
{
    impl_->category_ = category;
}

QString ArtifactEffectPreset::description() const
{
    return impl_->description_;
}

void ArtifactEffectPreset::setDescription(const QString& desc)
{
    impl_->description_ = desc;
}

void ArtifactEffectPreset::addParameter(const QString& paramName, float value)
{
    impl_->parameters_.push_back({Parameter::Float, paramName, value, QColor(), QString()});
}

void ArtifactEffectPreset::addParameter(const QString& paramName, const QColor& color)
{
    impl_->parameters_.push_back({Parameter::Color, paramName, 0.0f, color, QString()});
}

void ArtifactEffectPreset::addParameter(const QString& paramName, const QString& value)
{
    impl_->parameters_.push_back({Parameter::String, paramName, 0.0f, QColor(), value});
}

float ArtifactEffectPreset::getFloatParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::Float) {
            return p.floatValue;
        }
    }
    return 0.0f;
}

QColor ArtifactEffectPreset::getColorParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::Color) {
            return p.colorValue;
        }
    }
    return Qt::white;
}

QString ArtifactEffectPreset::getStringParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::String) {
            return p.stringValue;
        }
    }
    return QString();
}

QVector<ArtifactEffectPreset::Parameter> ArtifactEffectPreset::allParameters() const
{
    QVector<Parameter> result;
    for (const auto& p : impl_->parameters_) {
        Parameter outParam;
        outParam.type = p.type;
        outParam.name = p.name;
        if (p.type == Parameter::Float) {
            outParam.floatValue = p.floatValue;
        } else if (p.type == Parameter::Color) {
            outParam.colorValue = p.colorValue;
        } else {
            outParam.stringValue = p.stringValue;
        }
        result.push_back(outParam);
    }
    return result;
}

QJsonObject ArtifactEffectPreset::toJson() const
{
    QJsonObject obj;
    obj["id"] = impl_->id_;
    obj["name"] = impl_->name_;
    obj["category"] = impl_->category_;
    obj["description"] = impl_->description_;

    QJsonArray params;
    for (const auto& p : impl_->parameters_) {
        QJsonObject paramObj;
        paramObj["name"] = p.name;
        paramObj["type"] = static_cast<int>(p.type);

        switch (p.type) {
        case Parameter::Float:
            paramObj["value"] = static_cast<double>(p.floatValue);
            break;
        case Parameter::Color:
            paramObj["value"] = p.colorValue.name();
            break;
        case Parameter::String:
            paramObj["value"] = p.stringValue;
            break;
        }
        params.append(paramObj);
    }
    obj["parameters"] = params;

    return obj;
}

ArtifactEffectPreset ArtifactEffectPreset::fromJson(const QJsonObject& json)
{
    ArtifactEffectPreset preset;
    preset.setId(json["id"].toString());
    preset.setName(json["name"].toString());
    preset.setCategory(json["category"].toString());
    preset.setDescription(json["description"].toString());

    QJsonArray params = json["parameters"].toArray();
    for (const QJsonValue& v : params) {
        QJsonObject p = v.toObject();
        QString name = p["name"].toString();
        auto type = static_cast<Parameter::Type>(p["type"].toInt());

        switch (type) {
        case Parameter::Float:
            preset.addParameter(name, static_cast<float>(p["value"].toDouble()));
            break;
        case Parameter::Color:
            preset.addParameter(name, QColor(p["value"].toString()));
            break;
        case Parameter::String:
            preset.addParameter(name, p["value"].toString());
            break;
        }
    }

    return preset;
}

void ArtifactEffectPreset::applyTo(ArtifactAbstractEffect* effect) const
{
    if (!effect) return;

    for (const auto& p : impl_->parameters_) {
        switch (p.type) {
        case Parameter::Float:
            effect->setParameter(p.name, p.floatValue);
            break;
        case Parameter::Color:
            effect->setParameterColor(p.name, p.colorValue);
            break;
        case Parameter::String:
            // 文字列パラメータは文字列として設定
            break;
        }
    }
}

QByteArray ArtifactEffectPreset::thumbnail() const
{
    return impl_->thumbnail_;
}

void ArtifactEffectPreset::setThumbnail(const QByteArray& data)
{
    impl_->thumbnail_ = data;
}

// ==================== ArtifactEffectPresetCollection::Impl ====================

class ArtifactEffectPresetCollection::Impl
{
public:
    QHash<PresetID, std::unique_ptr<ArtifactEffectPreset>> presets_;
};

ArtifactEffectPresetCollection::ArtifactEffectPresetCollection()
    : impl_(new Impl())
{
}

ArtifactEffectPresetCollection::~ArtifactEffectPresetCollection()
{
    delete impl_;
}

ArtifactEffectPreset* ArtifactEffectPresetCollection::createPreset(const QString& name)
{
    auto preset = std::make_unique<ArtifactEffectPreset>(name);
    PresetID id = preset->id();
    ArtifactEffectPreset* ptr = preset.get();
    impl_->presets_[id] = std::move(preset);
    return ptr;
}

void ArtifactEffectPresetCollection::deletePreset(const PresetID& id)
{
    impl_->presets_.remove(id);
}

ArtifactEffectPreset* ArtifactEffectPresetCollection::getPreset(const PresetID& id)
{
    auto it = impl_->presets_.find(id);
    return (it != impl_->presets_.end()) ? it->get() : nullptr;
}

const ArtifactEffectPreset* ArtifactEffectPresetCollection::getPreset(const PresetID& id) const
{
    auto it = impl_->presets_.find(id);
    return (it != impl_->presets_.end()) ? it->get() : nullptr;
}

QVector<ArtifactEffectPreset*> ArtifactEffectPresetCollection::getPresetsByCategory(const QString& category) const
{
    QVector<ArtifactEffectPreset*> result;
    for (const auto& [id, preset] : impl_->presets_) {
        if (preset->category() == category) {
            result.append(preset.get());
        }
    }
    return result;
}

QStringList ArtifactEffectPresetCollection::allCategories() const
{
    QSet<QString> cats;
    for (const auto& [id, preset] : impl_->presets_) {
        cats.insert(preset->category());
    }
    return cats.toList();
}

QVector<ArtifactEffectPreset*> ArtifactEffectPresetCollection::allPresets()
{
    QVector<ArtifactEffectPreset*> result;
    for (auto& [id, preset] : impl_->presets_) {
        result.append(preset.get());
    }
    return result;
}

QVector<const ArtifactEffectPreset*> ArtifactEffectPresetCollection::allPresets() const
{
    QVector<const ArtifactEffectPreset*> result;
    for (const auto& [id, preset] : impl_->presets_) {
        result.append(preset.get());
    }
    return result;
}

int ArtifactEffectPresetCollection::presetCount() const
{
    return static_cast<int>(impl_->presets_.size());
}

bool ArtifactEffectPresetCollection::isEmpty() const
{
    return impl_->presets_.empty();
}

bool ArtifactEffectPresetCollection::saveToFile(const QString& filePath) const
{
    QJsonArray arr;
    for (const auto& [id, preset] : impl_->presets_) {
        arr.append(preset->toJson());
    }

    QJsonDocument doc(arr);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        return true;
    }
    return false;
}

bool ArtifactEffectPresetCollection::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue& v : arr) {
            auto preset = ArtifactEffectPreset::fromJson(v.toObject());
            auto id = preset.id();
            impl_->presets_[id] = std::make_unique<ArtifactEffectPreset>(preset);
        }
        return true;
    }
    return false;
}

void ArtifactEffectPresetCollection::loadDefaultPresets()
{
    // デフォルトプリセット例：Gaussian Blur
    auto* blurPreset = createPreset("Gaussian Blur");
    blurPreset->setCategory(PresetCategories::Blur);
    blurPreset->setDescription("Smooths edges using a Gaussian blur");
    blurPreset->addParameter("blurAmount", 10.0f);
    blurPreset->addParameter("blurDimension", 1.0f);  // 0=Horizontal, 1=Both

    // デフォルトプリセット例：Brightness/Contrast
    auto* bcPreset = createPreset("Brightness & Contrast");
    bcPreset->setCategory(PresetCategories::Color);
    bcPreset->setDescription("Adjusts the brightness and contrast");
    bcPreset->addParameter("brightness", 0.0f);
    bcPreset->addParameter("contrast", 0.0f);
    bcPreset->addParameter("useLegacy", false);

    // デフォルトプリセット例：Drop Shadow
    auto* shadowPreset = createPreset("Drop Shadow");
    shadowPreset->setCategory(PresetCategories::Stylize);
    shadowPreset->setDescription("Adds a drop shadow behind the layer");
    shadowPreset->addParameter("shadowColor", QColor(0, 0, 0, 255));
    shadowPreset->addParameter("shadowOpacity", 50.0f);
    shadowPreset->addParameter("shadowAngle", 45.0f);
    shadowPreset->addParameter("shadowDistance", 5.0f);
    shadowPreset->addParameter("shadowBlur", 5.0f);

    // デフォルトプリセット例：Glow
    auto* glowPreset = createPreset("Glow");
    glowPreset->setCategory(PresetCategories::Stylize);
    glowPreset->setDescription("Adds a glow effect");
    glowPreset->addParameter("glowIntensity", 1.0f);
    glowPreset->addParameter("glowRadius", 10.0f);
    glowPreset->addParameter("glowColor", QColor(255, 255, 200, 255));
}

} // namespace Artifact
