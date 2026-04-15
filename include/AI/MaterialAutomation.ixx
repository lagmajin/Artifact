module;
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QString>
#include <QVariant>


export module Artifact.AI.MaterialAutomation;

import std;
import Core.AI.Describable;
import Material.Material;

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

private:
  QVariant createMaterial(const QVariantList &args) {
    if (args.size() < 2)
      return QVariant();
    QString name = args[0].toString();
    QVariantMap properties = args[1].toMap();

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

    // For now, just return success - in full implementation would store
    // material
    QVariantMap result;
    result["success"] = true;
    result["materialName"] = name;
    return result;
  }

  QVariant getMaterialProperties(const QVariantList &args) {
    if (args.isEmpty())
      return QVariant();
    QString name = args[0].toString();

    // Placeholder - would lookup material by name
    QVariantMap properties;
    properties["diffuseColor"] = QColor(200, 200, 200);
    properties["specularColor"] = QColor(255, 255, 255);
    properties["roughness"] = 0.5f;
    return properties;
  }

  QVariant updateMaterialProperty(const QVariantList &args) {
    if (args.size() < 3)
      return false;
    QString name = args[0].toString();
    QString property = args[1].toString();
    QVariant value = args[2];

    // Placeholder - would update material property
    return true;
  }

  QVariant applyMaterialPreset(const QVariantList &args) {
    if (args.size() < 2)
      return false;
    QString preset = args[0].toString();
    QString layerId = args[1].toString();

    // Placeholder - would apply preset to layer
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
    QString materialName = args[0].toString();
    QString layerId = args[1].toString();

    // Placeholder - would assign material to layer
    return true;
  }
};

} // namespace Artifact
