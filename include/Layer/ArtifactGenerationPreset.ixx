module;
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

export module Artifact.Layer.GenerationPreset;

export namespace Artifact {

// Portable recipe shared by built-in and user-authored New > Presets entries.
struct ArtifactGenerationPreset {
  QString id;
  QString displayName;
  QJsonObject layer;
  QJsonArray masks;
  QJsonArray effects;
  QJsonArray animators;
  // Layer Recipe v2: ordered entries. Each entry owns `key`, `layer`,
  // `masks`, `effects`, `animators`, optional numeric `blend`, and optional
  // `parent` (another entry key). The v1 fields above remain for migration.
  QJsonArray layers;

  QJsonObject toJson() const {
    return {{QStringLiteral("schema"), QStringLiteral("artifact.layer-recipe.v2")},
            {QStringLiteral("id"), id}, {QStringLiteral("displayName"), displayName},
            {QStringLiteral("layer"), layer}, {QStringLiteral("masks"), masks},
            {QStringLiteral("effects"), effects}, {QStringLiteral("animators"), animators},
            {QStringLiteral("layers"), layers}};
  }
  static ArtifactGenerationPreset fromJson(const QJsonObject& object) {
    ArtifactGenerationPreset preset;
    const QString schema = object.value(QStringLiteral("schema")).toString();
    if (!schema.isEmpty() && schema != QStringLiteral("artifact.generation-preset.v1") &&
        schema != QStringLiteral("artifact.layer-recipe.v2")) {
      return preset;
    }
    preset.id = object.value(QStringLiteral("id")).toString();
    preset.displayName = object.value(QStringLiteral("displayName")).toString();
    preset.layer = object.value(QStringLiteral("layer")).toObject();
    preset.masks = object.value(QStringLiteral("masks")).toArray();
    preset.effects = object.value(QStringLiteral("effects")).toArray();
    preset.animators = object.value(QStringLiteral("animators")).toArray();
    preset.layers = object.value(QStringLiteral("layers")).toArray();
    return preset;
  }
  bool valid() const {
    return !id.trimmed().isEmpty() && !displayName.trimmed().isEmpty() &&
           layer.value(QStringLiteral("type")).isDouble() &&
           (layer.value(QStringLiteral("type")).toInt() > 0 || !layers.isEmpty());
  }
};

}
