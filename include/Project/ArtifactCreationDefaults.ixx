module;
#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

export module Artifact.Project.CreationDefaults;

import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Utils.String.UniString;
import Utils.Optional;
import Serialization.ISerializable;

export namespace Artifact {

enum class CreationTargetKind {
    Composition,
    Shape,
    Text,
    Image
};

enum class CreationDefaultScope {
    Global,
    Project,
    Preset,
    LastUsed
};

struct CreationCompositionDefaults : public ArtifactCore::Serialization::ISerializable {
    ArtifactCompositionInitParams composition;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override { return fromJson(json); }
    QString typeName() const override { return QStringLiteral("CreationCompositionDefaults"); }
    int schemaVersion() const override { return 1; }
};

struct CreationLayerDefaults : public ArtifactCore::Serialization::ISerializable {
    ArtifactCore::Optional<ArtifactLayerInitParams> layer;
    QString sourcePath;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override { return fromJson(json); }
    QString typeName() const override { return QStringLiteral("CreationLayerDefaults"); }
    int schemaVersion() const override { return 1; }
};

struct CreationDefaultsBundle : public ArtifactCore::Serialization::ISerializable {
    CreationCompositionDefaults composition;
    CreationLayerDefaults shape;
    CreationLayerDefaults text;
    CreationLayerDefaults image;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override { return fromJson(json); }
    QString typeName() const override { return QStringLiteral("CreationDefaultsBundle"); }
    int schemaVersion() const override { return 1; }
};

struct CreationDefaultsState : public ArtifactCore::Serialization::ISerializable {
    CreationDefaultsBundle globalDefaults;
    CreationDefaultsBundle projectDefaults;
    CreationDefaultsBundle lastUsed;
    QJsonObject presets;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override { return fromJson(json); }
    QString typeName() const override { return QStringLiteral("CreationDefaultsState"); }
    int schemaVersion() const override { return 1; }
};

class CreationDefaultResolver : public ArtifactCore::Serialization::ISerializable {
public:
    CreationDefaultResolver() = default;

    void setGlobalDefaults(const CreationDefaultsBundle& defaults);
    void setProjectDefaults(const CreationDefaultsBundle& defaults);
    void setLastUsedDefaults(const CreationDefaultsBundle& defaults);
    void setPreset(const QString& presetName, const CreationDefaultsBundle& defaults);
    void clearPreset();

    CreationCompositionDefaults resolveComposition(
        const CreationCompositionDefaults& explicitPreset = CreationCompositionDefaults()) const;
    CreationLayerDefaults resolveShape(const CreationLayerDefaults& explicitPreset = CreationLayerDefaults()) const;
    CreationLayerDefaults resolveText(const CreationLayerDefaults& explicitPreset = CreationLayerDefaults()) const;
    CreationLayerDefaults resolveImage(const CreationLayerDefaults& explicitPreset = CreationLayerDefaults()) const;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    QJsonObject serialize() const override { return toJson(); }
    bool deserialize(const QJsonObject& json) override { return fromJson(json); }
    QString typeName() const override { return QStringLiteral("CreationDefaultResolver"); }
    int schemaVersion() const override { return 1; }

private:
    CreationDefaultsBundle globalDefaults_;
    CreationDefaultsBundle projectDefaults_;
    CreationDefaultsBundle lastUsedDefaults_;
    QString activePresetName_;
    CreationDefaultsBundle activePresetDefaults_;
};

}
