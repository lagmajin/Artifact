module;
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QString>
#include <QVariant>
#include <QMap>
#include <algorithm>
#include <cmath>


export module Artifact.AI.MaterialAutomation;

import std;
import Core.AI.Describable;
import Material.Material;
import Artifact.Service.Project;
import Artifact.Layers.Model3D;

export namespace Artifact {

class MaterialAutomation : public ArtifactCore::IDescribable {
public:
  static void ensureRegistered() {
    static const bool registered = []() {
      ArtifactCore::DescriptionRegistry::instance().registerDescribable(
          QStringLiteral("MaterialAutomation"),
          []() -> const ArtifactCore::IDescribable * {
            return &MaterialAutomation::instance();
          });
      return true;
    }();
    (void)registered;
  }

  static MaterialAutomation &instance() {
    static MaterialAutomation automation;
    return automation;
  }

  QString className() const override {
    return QStringLiteral("MaterialAutomation");
  }

  ArtifactCore::LocalizedText briefDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "Provides 3D material creation, editing, and assignment operations.",
        "Provides 3D material creation, editing, and assignment operations.",
        {});
  }

  ArtifactCore::LocalizedText detailedDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "This tool enables AI to create, modify, and assign 3D materials to "
        "layers. "
        "Supports material property editing, preset application, and material "
        "assignment to 3D objects.",
        "This tool enables AI to create, modify, and assign 3D materials to "
        "layers. "
        "Supports material property editing, preset application, and material "
        "assignment to 3D objects.",
        {});
  }

  QList<ArtifactCore::MethodDescription> methodDescriptions() const override {
    using ArtifactCore::IDescribable;
    return {
        {"createMaterial",
         IDescribable::loc(
             "Create a new 3D material with specified properties.",
             "Create a new 3D material with specified properties.", {}),
         "QVariantMap",
         {QStringLiteral("QString"), QStringLiteral("QVariantMap")},
         {QStringLiteral("name"), QStringLiteral("properties")}},
        {"getMaterialProperties",
         IDescribable::loc("Get properties of a material by name.",
                           "Get properties of a material by name.", {}),
         "QVariantMap",
         {QStringLiteral("QString")},
         {QStringLiteral("name")}},
        {"updateMaterialProperty",
         IDescribable::loc("Update a specific property of a material.",
                           "Update a specific property of a material.", {}),
         "bool",
         {QStringLiteral("QString"), QStringLiteral("QString"),
          QStringLiteral("QVariant")},
         {QStringLiteral("name"), QStringLiteral("property"),
          QStringLiteral("value")}},
        {"applyMaterialPreset",
         IDescribable::loc("Apply a preset material to a layer.",
                           "Apply a preset material to a layer.", {}),
         "bool",
         {QStringLiteral("QString"), QStringLiteral("QString")},
         {QStringLiteral("preset"), QStringLiteral("layerId")}},
        {"listMaterialPresets",
         IDescribable::loc("List available material presets.",
                           "List available material presets.", {}),
         "QVariantList"},
        {"assignMaterialToLayer",
         IDescribable::loc("Assign a material to a 3D layer.",
                           "Assign a material to a 3D layer.", {}),
         "bool",
         {QStringLiteral("QString"), QStringLiteral("QString")},
         {QStringLiteral("materialName"), QStringLiteral("layerId")}},
    };
  }

  QVariant invokeMethod(QStringView methodName,
                        const QVariantList &args) override {
    if (methodName == "createMaterial") {
      return createMaterial(args);
    } else if (methodName == "getMaterialProperties") {
      return getMaterialProperties(args);
    } else if (methodName == "updateMaterialProperty") {
      return updateMaterialProperty(args);
    } else if (methodName == "applyMaterialPreset") {
      return applyMaterialPreset(args);
    } else if (methodName == "listMaterialPresets") {
      return listMaterialPresets(args);
    } else if (methodName == "assignMaterialToLayer") {
      return assignMaterialToLayer(args);
    }
    return QVariant();
  }

  QVariantMap materialProperties(const QString &name) const {
    return materials_.value(name.trimmed());
  }

  QString assignedMaterial(const QString &layerId) const {
    return layerAssignments_.value(layerId.trimmed());
  }

private:
  QVariant createMaterial(const QVariantList &args) {
    if (args.size() < 2)
      return QVariant();
    const QString name = args[0].toString().trimmed();
    if (name.isEmpty()) return QVariant();
    const QVariantMap properties = args[1].toMap();

    // Create material based on properties
    ArtifactCore::Material material = ArtifactCore::Material::makeDefault();
    material.setName(name.toStdString().c_str());

    if (properties.contains("diffuseColor")) {
      material.setBaseColor(properties["diffuseColor"].value<QColor>());
    }
    if (properties.contains("specularColor")) {
      material.setEmissionColor(properties["specularColor"].value<QColor>());
    }
    if (properties.contains("roughness")) {
      material.setRoughness(properties["roughness"].toFloat());
    }

    QVariantMap stored;
    stored["diffuseColor"] = properties.value("diffuseColor", QColor(200, 200, 200));
    stored["specularColor"] = properties.value("specularColor", QColor(255, 255, 255));
    stored["roughness"] = std::clamp(properties.value("roughness", 0.5).toFloat(), 0.0f, 1.0f);
    stored["metallic"] = std::clamp(properties.value("metallic", 0.0).toFloat(), 0.0f, 1.0f);
    materials_[name] = stored;
    for (auto assignment = layerAssignments_.cbegin();
         assignment != layerAssignments_.cend(); ++assignment) {
      if (assignment.value() == name) {
        applyMaterialToLayer(assignment.key(), name);
      }
    }
    QVariantMap result;
    result["success"] = true;
    result["materialName"] = name;
    return result;
  }

  QVariant getMaterialProperties(const QVariantList &args) {
    if (args.isEmpty())
      return QVariant();
    const QString name = args[0].toString().trimmed();
    return materials_.value(name);
  }

  QVariant updateMaterialProperty(const QVariantList &args) {
    if (args.size() < 3)
      return false;
    const QString name = args[0].toString().trimmed();
    const QString property = args[1].toString().trimmed();
    auto it = materials_.find(name);
    if (it == materials_.end()) return false;
    if (property != "diffuseColor" && property != "specularColor" &&
        property != "roughness" && property != "metallic") return false;
    if (property == "diffuseColor" || property == "specularColor") {
      if (!args[2].canConvert<QColor>()) return false;
      it.value()[property] = args[2].value<QColor>();
    } else {
      const float numeric = args[2].toFloat();
      if (!std::isfinite(numeric)) return false;
      it.value()[property] = std::clamp(numeric, 0.0f, 1.0f);
    }
    for (auto assignment = layerAssignments_.cbegin();
         assignment != layerAssignments_.cend(); ++assignment) {
      if (assignment.value() == name) {
        applyMaterialToLayer(assignment.key(), name);
      }
    }
    return true;
  }

  QVariant applyMaterialPreset(const QVariantList &args) {
    if (args.size() < 2)
      return false;
    const QString preset = args[0].toString().trimmed();
    const QString layerId = args[1].toString().trimmed();
    if (layerId.isEmpty() ||
        !listMaterialPresets({}).toList().contains(QVariant(preset)) ||
        !layerExistsInCurrentComposition(layerId)) return false;
    if (!materials_.contains(preset)) {
      QVariantMap presetProperties;
      presetProperties["diffuseColor"] = QColor(200, 200, 200);
      presetProperties["specularColor"] = QColor(255, 255, 255);
      presetProperties["roughness"] = 0.5f;
      presetProperties["metallic"] = 0.0f;
      if (preset == QStringLiteral("Plastic")) {
        presetProperties["roughness"] = 0.3f;
      } else if (preset == QStringLiteral("Metal")) {
        presetProperties["metallic"] = 1.0f;
        presetProperties["roughness"] = 0.2f;
      } else if (preset == QStringLiteral("Glass")) {
        presetProperties["roughness"] = 0.05f;
        presetProperties["specularColor"] = QColor(255, 255, 255, 220);
      }
      materials_[preset] = presetProperties;
    }
    if (!applyMaterialToLayer(layerId, preset)) return false;
    layerAssignments_[layerId] = preset;
    return true;
  }

  QVariant listMaterialPresets(const QVariantList &args) {
    Q_UNUSED(args)
    QVariantList presets;
    presets << "Default" << "Plastic" << "Metal" << "Glass";
    return presets;
  }

  QVariant assignMaterialToLayer(const QVariantList &args) {
    if (args.size() < 2)
      return false;
    const QString materialName = args[0].toString().trimmed();
    const QString layerId = args[1].toString().trimmed();
    if (materialName.isEmpty() || layerId.isEmpty() ||
        !materials_.contains(materialName) ||
        !layerExistsInCurrentComposition(layerId)) return false;
    if (!applyMaterialToLayer(layerId, materialName)) return false;
    layerAssignments_[layerId] = materialName;
    return true;
  }

  bool applyMaterialToLayer(const QString &layerId, const QString &materialName) {
    auto *service = ArtifactProjectService::instance();
    if (!service) return false;
    const auto composition = service->currentComposition().lock();
    if (!composition) return false;
    const auto layer = composition->layerById(ArtifactCore::LayerID(layerId));
    auto *model = layer ? dynamic_cast<Artifact3DLayer *>(layer.get()) : nullptr;
    if (!model) return false;
    const QVariantMap properties = materials_.value(materialName);
    bool changed = false;
    changed |= model->setLayerPropertyValue(
        QStringLiteral("material.base.color"), properties.value(QStringLiteral("diffuseColor")));
    changed |= model->setLayerPropertyValue(
        QStringLiteral("material.emission.color"), properties.value(QStringLiteral("specularColor")));
    changed |= model->setLayerPropertyValue(
        QStringLiteral("material.roughness"), properties.value(QStringLiteral("roughness")));
    changed |= model->setLayerPropertyValue(
        QStringLiteral("material.metallic"), properties.value(QStringLiteral("metallic")));
    if (changed) model->changed();
    return changed;
  }

  static bool layerExistsInCurrentComposition(const QString &layerId) {
    auto *service = ArtifactProjectService::instance();
    if (!service) return false;
    const auto composition = service->currentComposition().lock();
    return composition && composition->layerById(ArtifactCore::LayerID(layerId));
  }

  QMap<QString, QVariantMap> materials_;
  QMap<QString, QString> layerAssignments_;
};

} // namespace Artifact
