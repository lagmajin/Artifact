module;

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QColor>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QSet>

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
import Serialization.Registry;
import Serialization.SchemaMigration;

namespace Artifact
{

namespace {
const bool registeredArtifactEffectPreset = [] {
    ArtifactCore::Serialization::registerSerializableType<ArtifactEffectPreset>();
    auto& migrations = ArtifactCore::Serialization::SchemaMigrationRegistry::instance();
    migrations.registerMigration(
        QStringLiteral("ArtifactEffectPreset"), 0, 1,
        [](const QJsonObject& legacy) { return legacy; });
    migrations.registerMigration(
        QStringLiteral("ArtifactEffectPreset"), 1, 2,
        [](const QJsonObject& legacy) {
            QJsonObject migrated = legacy;
            migrated[QStringLiteral("schema_version")] = 2;
            QJsonArray parameters;
            for (const auto& value : legacy.value(QStringLiteral("parameters")).toArray()) {
                QJsonObject parameter = value.toObject();
                const int type = parameter.value(QStringLiteral("type")).toInt();
                static const std::array<const char*, 3> legacyTypes{
                    "float", "color", "string"};
                if (type >= 0 && type < static_cast<int>(legacyTypes.size())) {
                    parameter[QStringLiteral("value_type")] =
                        QString::fromLatin1(legacyTypes[static_cast<std::size_t>(type)]);
                }
                parameters.append(parameter);
            }
            migrated[QStringLiteral("parameters")] = parameters;
            return migrated;
        });
    return true;
}();
}

namespace
{
constexpr qint64 kMaxEffectPresetFileBytes = 16LL * 1024LL * 1024LL;
constexpr qsizetype kMaxEffectPresetEntries = 100000;
}

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
        double doubleValue = 0.0;
        int integerValue = 0;
        bool booleanValue = false;
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

ArtifactEffectPreset::ArtifactEffectPreset(const ArtifactEffectPreset& other)
    : impl_(new Impl(*other.impl_))
{
}

ArtifactEffectPreset& ArtifactEffectPreset::operator=(const ArtifactEffectPreset& other)
{
    if (this != &other) {
        auto* replacement = new Impl(*other.impl_);
        delete impl_;
        impl_ = replacement;
    }
    return *this;
}

ArtifactEffectPreset::ArtifactEffectPreset(ArtifactEffectPreset&& other)
    : impl_(other.impl_)
{
    other.impl_ = new Impl();
}

ArtifactEffectPreset& ArtifactEffectPreset::operator=(ArtifactEffectPreset&& other)
{
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl();
    }
    return *this;
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
    Impl::ParameterData p;
    p.type = Parameter::Float;
    p.name = paramName;
    p.floatValue = value;
    impl_->parameters_.push_back(p);
}

void ArtifactEffectPreset::addParameter(const QString& paramName, double value)
{
    Impl::ParameterData p;
    p.type = Parameter::Double;
    p.name = paramName;
    p.doubleValue = value;
    impl_->parameters_.push_back(p);
}

void ArtifactEffectPreset::addParameter(const QString& paramName, int value)
{
    Impl::ParameterData p;
    p.type = Parameter::Integer;
    p.name = paramName;
    p.integerValue = value;
    impl_->parameters_.push_back(p);
}

void ArtifactEffectPreset::addParameter(const QString& paramName, bool value)
{
    Impl::ParameterData p;
    p.type = Parameter::Boolean;
    p.name = paramName;
    p.booleanValue = value;
    impl_->parameters_.push_back(p);
}

void ArtifactEffectPreset::addParameter(const QString& paramName, const QColor& color)
{
    Impl::ParameterData p;
    p.type = Parameter::Color;
    p.name = paramName;
    p.colorValue = color;
    impl_->parameters_.push_back(p);
}

void ArtifactEffectPreset::addParameter(const QString& paramName, const QString& value)
{
    Impl::ParameterData p;
    p.type = Parameter::String;
    p.name = paramName;
    p.stringValue = value;
    impl_->parameters_.push_back(p);
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

double ArtifactEffectPreset::getDoubleParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::Double) return p.doubleValue;
    }
    return 0.0;
}

int ArtifactEffectPreset::getIntegerParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::Integer) return p.integerValue;
    }
    return 0;
}

bool ArtifactEffectPreset::getBooleanParameter(const QString& paramName) const
{
    for (const auto& p : impl_->parameters_) {
        if (p.name == paramName && p.type == Parameter::Boolean) return p.booleanValue;
    }
    return false;
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
        } else if (p.type == Parameter::Double) {
            outParam.doubleValue = p.doubleValue;
        } else if (p.type == Parameter::Integer) {
            outParam.integerValue = p.integerValue;
        } else if (p.type == Parameter::Boolean) {
            outParam.booleanValue = p.booleanValue;
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
    obj["schema_version"] = schemaVersion();

    QJsonArray params;
    for (const auto& p : impl_->parameters_) {
        QJsonObject paramObj;
        paramObj["name"] = p.name;
        paramObj["type"] = static_cast<int>(p.type);

        switch (p.type) {
        case Parameter::Float:
            paramObj["value_type"] = QStringLiteral("float");
            paramObj["value"] = static_cast<double>(p.floatValue);
            break;
        case Parameter::Color:
            paramObj["value_type"] = QStringLiteral("color");
            paramObj["value"] = p.colorValue.name(QColor::HexArgb);
            break;
        case Parameter::String:
            paramObj["value_type"] = QStringLiteral("string");
            paramObj["value"] = p.stringValue;
            break;
        case Parameter::Integer:
            paramObj["value_type"] = QStringLiteral("integer");
            paramObj["value"] = p.integerValue;
            break;
        case Parameter::Boolean:
            paramObj["value_type"] = QStringLiteral("boolean");
            paramObj["value"] = p.booleanValue;
            break;
        case Parameter::Double:
            paramObj["value_type"] = QStringLiteral("double");
            paramObj["value"] = p.doubleValue;
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
        const QString valueType = p.value(QStringLiteral("value_type")).toString();
        if (valueType == QStringLiteral("integer")) type = Parameter::Integer;
        else if (valueType == QStringLiteral("boolean")) type = Parameter::Boolean;
        else if (valueType == QStringLiteral("double")) type = Parameter::Double;
        else if (valueType == QStringLiteral("color")) type = Parameter::Color;
        else if (valueType == QStringLiteral("string")) type = Parameter::String;
        else if (valueType == QStringLiteral("float")) type = Parameter::Float;

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
        case Parameter::Integer:
            preset.addParameter(name, p["value"].toInt());
            break;
        case Parameter::Boolean:
            preset.addParameter(name, p["value"].toBool());
            break;
        case Parameter::Double:
            preset.addParameter(name, p["value"].toDouble());
            break;
        }
    }

    return preset;
}

bool ArtifactEffectPreset::deserialize(const QJsonObject& json)
{
    const ArtifactEffectPreset parsed = ArtifactEffectPreset::fromJson(json);
    setId(parsed.id());
    setName(parsed.name());
    setCategory(parsed.category());
    setDescription(parsed.description());
    setThumbnail(parsed.thumbnail());
    impl_->parameters_.clear();
    for (const auto& parameter : parsed.allParameters()) {
        switch (parameter.type) {
        case Parameter::Float:
            addParameter(parameter.name, parameter.floatValue);
            break;
        case Parameter::Color:
            addParameter(parameter.name, parameter.colorValue);
            break;
        case Parameter::String:
            addParameter(parameter.name, parameter.stringValue);
            break;
        case Parameter::Integer:
            addParameter(parameter.name, parameter.integerValue);
            break;
        case Parameter::Boolean:
            addParameter(parameter.name, parameter.booleanValue);
            break;
        case Parameter::Double:
            addParameter(parameter.name, parameter.doubleValue);
            break;
        }
    }
    return !id().isEmpty();
}

void ArtifactEffectPreset::applyTo(ArtifactAbstractEffect* effect) const
{
    if (!effect) return;

    for (const auto& p : impl_->parameters_) {
        switch (p.type) {
        case Parameter::Float:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.floatValue));
            break;
        case Parameter::Color:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.colorValue));
            break;
        case Parameter::String:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.stringValue));
            break;
        case Parameter::Integer:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.integerValue));
            break;
        case Parameter::Boolean:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.booleanValue));
            break;
        case Parameter::Double:
            effect->setPropertyValue(ArtifactCore::UniString(p.name), QVariant(p.doubleValue));
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
    std::map<ArtifactEffectPreset::PresetID, std::unique_ptr<ArtifactEffectPreset>> presets_;
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
    ArtifactEffectPreset::PresetID id = preset->id();
    ArtifactEffectPreset* ptr = preset.get();
    impl_->presets_[id] = std::move(preset);
    return ptr;
}

void ArtifactEffectPresetCollection::deletePreset(const ArtifactEffectPreset::PresetID& id)
{
    impl_->presets_.erase(id);
}

ArtifactEffectPreset* ArtifactEffectPresetCollection::getPreset(const ArtifactEffectPreset::PresetID& id)
{
    auto it = impl_->presets_.find(id);
    return (it != impl_->presets_.end()) ? it->second.get() : nullptr;
}

const ArtifactEffectPreset* ArtifactEffectPresetCollection::getPreset(const ArtifactEffectPreset::PresetID& id) const
{
    auto it = impl_->presets_.find(id);
    return (it != impl_->presets_.end()) ? it->second.get() : nullptr;
}

QVector<ArtifactEffectPreset*> ArtifactEffectPresetCollection::getPresetsByCategory(const QString& category) const
{
    QVector<ArtifactEffectPreset*> result;
    for (const auto& [id, preset] : impl_->presets_) {
        Q_UNUSED(id);
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
        Q_UNUSED(id);
        cats.insert(preset->category());
    }
    return cats.values();
}

QVector<ArtifactEffectPreset*> ArtifactEffectPresetCollection::allPresets()
{
    QVector<ArtifactEffectPreset*> result;
    for (auto& [id, preset] : impl_->presets_) {
        Q_UNUSED(id);
        result.append(preset.get());
    }
    return result;
}

QVector<const ArtifactEffectPreset*> ArtifactEffectPresetCollection::allPresets() const
{
    QVector<const ArtifactEffectPreset*> result;
    for (const auto& [id, preset] : impl_->presets_) {
        Q_UNUSED(id);
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
        Q_UNUSED(id);
        arr.append(preset->toJson());
    }

    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) return false;
    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray payload = QJsonDocument(arr).toJson();
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool ArtifactEffectPresetCollection::loadFromFile(const QString& filePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return false;
    }
    QFile file(normalizedPath);
    if (file.size() <= 0 || file.size() > kMaxEffectPresetFileBytes ||
        !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        qsizetype loadedCount = 0;
        for (const QJsonValue& v : arr) {
            if (loadedCount++ >= kMaxEffectPresetEntries) {
                break;
            }
            if (!v.isObject()) {
                continue;
            }
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

    // Layer-style presets. Keep property names aligned with the rasterizer
    // effects so the preset browser can apply them without a special-case
    // adapter.
    auto* bevelPreset = createPreset("Bevel");
    bevelPreset->setCategory(PresetCategories::Stylize);
    bevelPreset->setDescription("Adds an adjustable edge bevel to the layer");
    bevelPreset->addParameter("Strength", 1.0f);
    bevelPreset->addParameter("Softness", 2.0f);
    bevelPreset->addParameter("Edge Mode", false);

    auto* innerShadowPreset = createPreset("Inner Shadow");
    innerShadowPreset->setCategory(PresetCategories::Stylize);
    innerShadowPreset->setDescription("Adds a soft shadow inside the layer alpha");
    innerShadowPreset->addParameter("Shadow Color", QColor(0, 0, 0, 255));
    innerShadowPreset->addParameter("Distance", 5.0f);
    innerShadowPreset->addParameter("Angle", 45.0f);
    innerShadowPreset->addParameter("Softness", 5.0f);
    innerShadowPreset->addParameter("Opacity", 50.0f);

    auto* strokePreset = createPreset("Stroke");
    strokePreset->setCategory(PresetCategories::Stylize);
    strokePreset->setDescription("Adds an outline around the layer alpha");
    strokePreset->addParameter("Stroke Color", QColor(255, 255, 255, 255));
    strokePreset->addParameter("Width", 4.0f);
    strokePreset->addParameter("Opacity", 100.0f);

    auto* satinPreset = createPreset("Satin");
    satinPreset->setCategory(PresetCategories::Stylize);
    satinPreset->setDescription("Adds an inner satin shading effect");
    satinPreset->addParameter("Satin Color", QColor(200, 200, 200, 180));
    satinPreset->addParameter("Distance", 4.0f);
    satinPreset->addParameter("Angle", 45.0f);
    satinPreset->addParameter("Softness", 3.0f);
    satinPreset->addParameter("Opacity", 50.0f);
    satinPreset->addParameter("Invert", false);

    // デフォルトプリセット例：Glow
    auto* glowPreset = createPreset("Glow");
    glowPreset->setCategory(PresetCategories::Stylize);
    glowPreset->setDescription("Adds a glow effect");
    glowPreset->addParameter("glowIntensity", 1.0f);
    glowPreset->addParameter("glowRadius", 10.0f);
    glowPreset->addParameter("glowColor", QColor(255, 255, 200, 255));
}

} // namespace Artifact
