module;
#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <optional>

export module Artifact.Project.CreationDefaults;

import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Utils.String.UniString;

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

struct CreationCompositionDefaults {
    ArtifactCompositionInitParams composition;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

struct CreationLayerDefaults {
    std::optional<ArtifactLayerInitParams> layer;
    QString sourcePath;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

struct CreationDefaultsBundle {
    CreationCompositionDefaults composition;
    CreationLayerDefaults shape;
    CreationLayerDefaults text;
    CreationLayerDefaults image;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

struct CreationDefaultsState {
    CreationDefaultsBundle globalDefaults;
    CreationDefaultsBundle projectDefaults;
    CreationDefaultsBundle lastUsed;
    QJsonObject presets;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);
};

class CreationDefaultResolver {
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

private:
    CreationDefaultsBundle globalDefaults_;
    CreationDefaultsBundle projectDefaults_;
    CreationDefaultsBundle lastUsedDefaults_;
    QString activePresetName_;
    CreationDefaultsBundle activePresetDefaults_;
};

}
