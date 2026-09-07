module;
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <utility>

export module Artifact.Layer.GenerationPresetLibrary;
import Artifact.Layer.GenerationPreset;

export namespace Artifact {

inline QVector<ArtifactGenerationPreset> standardGenerationPresets() {
  auto make = [](QString id, QString name, int type, QJsonArray masks = {},
                 QJsonArray effects = {}, QJsonArray animators = {}) {
    ArtifactGenerationPreset preset;
    preset.id = std::move(id); preset.displayName = std::move(name);
    preset.layer = {{QStringLiteral("type"), type}};
    preset.masks = std::move(masks); preset.effects = std::move(effects);
    preset.animators = std::move(animators);
    return preset;
  };
  return {
    make(QStringLiteral("builtin.solid.circle-mask"), QStringLiteral("Solid + Circle Mask"), 3,
         QJsonArray{QJsonObject{{QStringLiteral("kind"), QStringLiteral("ellipse")}}}),
    make(QStringLiteral("builtin.adjustment.mask.blur"), QStringLiteral("Adjustment + Mask + Blur"), 4,
         QJsonArray{QJsonObject{{QStringLiteral("kind"), QStringLiteral("rectangle")}}},
         QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("blur")}}}),
    make(QStringLiteral("builtin.text.animator"), QStringLiteral("Text + Animator"), 5,
         {}, {}, QJsonArray{QJsonObject{{QStringLiteral("kind"), QStringLiteral("default")}}}),
    make(QStringLiteral("builtin.shape.glow"), QStringLiteral("Shape + Glow"), 6,
         {}, QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("glow")}}})
  };
}

inline QString generationPresetDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         QStringLiteral("/generation-presets");
}

// Validates the portable portion of a recipe. Runtime-only checks (such as
// whether an effect factory is available) remain at the application boundary.
inline QString validateGenerationPreset(const ArtifactGenerationPreset& preset) {
  if (!preset.valid()) return QStringLiteral("A preset needs an id, display name, and layer type.");
  for (const QJsonValue& value : preset.masks) {
    const QString kind = value.toObject().value(QStringLiteral("kind")).toString();
    if (kind != QStringLiteral("ellipse") && kind != QStringLiteral("rectangle"))
      return QStringLiteral("Unsupported mask kind: %1").arg(kind);
  }
  for (const QJsonValue& value : preset.effects) {
    if (value.toObject().value(QStringLiteral("type")).toString().trimmed().isEmpty())
      return QStringLiteral("An effect needs a type.");
  }
  for (const QJsonValue& value : preset.animators) {
    if (value.toObject().value(QStringLiteral("kind")).toString() != QStringLiteral("default"))
      return QStringLiteral("Unsupported animator kind.");
  }
  QSet<QString> keys;
  for (const QJsonValue& value : preset.layers) {
    const QJsonObject entry = value.toObject();
    const QString key = entry.value(QStringLiteral("key")).toString().trimmed();
    if (key.isEmpty() || keys.contains(key) ||
        !entry.value(QStringLiteral("layer")).toObject().value(QStringLiteral("type")).isDouble())
      return QStringLiteral("Each recipe layer needs a unique key and type.");
    keys.insert(key);
    if (entry.contains(QStringLiteral("blend")) && !entry.value(QStringLiteral("blend")).isDouble())
      return QStringLiteral("A layer blend must be numeric.");
  }
  for (const QJsonValue& value : preset.layers) {
    const QString parent = value.toObject().value(QStringLiteral("parent")).toString();
    if (!parent.isEmpty() && !keys.contains(parent))
      return QStringLiteral("A layer parent must reference a recipe layer key.");
  }
  return {};
}

inline QVector<ArtifactGenerationPreset> userGenerationPresets() {
  QVector<ArtifactGenerationPreset> presets;
  QSet<QString> ids;
  for (const ArtifactGenerationPreset& preset : standardGenerationPresets()) {
    ids.insert(preset.id);
  }
  const QDir directory(generationPresetDirectory());
  for (const QFileInfo& entry : directory.entryInfoList(
           {QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
    QFile file(entry.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) continue;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) continue;
    const ArtifactGenerationPreset preset =
        ArtifactGenerationPreset::fromJson(document.object());
    if (validateGenerationPreset(preset).isEmpty() && !ids.contains(preset.id)) {
      ids.insert(preset.id);
      presets.append(preset);
    }
  }
  return presets;
}

inline bool saveUserGenerationPreset(const ArtifactGenerationPreset& preset) {
  if (!validateGenerationPreset(preset).isEmpty()) return false;
  QDir directory;
  if (!directory.mkpath(generationPresetDirectory())) return false;
  QString fileStem = preset.id;
  fileStem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")),
                   QStringLiteral("_"));
  if (fileStem.isEmpty()) return false;
  QSaveFile file(generationPresetDirectory() + QLatin1Char('/') + fileStem +
                 QStringLiteral(".json"));
  if (!file.open(QIODevice::WriteOnly)) return false;
  file.write(QJsonDocument(preset.toJson()).toJson(QJsonDocument::Indented));
  return file.commit();
}

}
