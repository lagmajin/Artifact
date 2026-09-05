module;
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QSizeF>
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>

module Artifact.Layers.Model3D;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Color.Float;
import Mesh;
import Time.Rational;
import MeshImporter;
import Utils.String.UniString;
import Material.Material;
import Core.Parallel;
import EnvironmentVariable.Expansion;

namespace Artifact {

namespace {
float finiteClamped(float value, float fallback, float minimum, float maximum) {
  return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

Artifact::Detail::float3 toFloat3(const QVector3D &v) {
  return {v.x(), v.y(), v.z()};
}

void centerMeshPositions(Mesh &mesh) {
  mesh.updateBounds();
  // Skin matrices are authored in the imported mesh space. Translating only
  // the positions would make later pose updates use a mismatched origin.
  // Keep skinned assets in source space; camera/model framing handles bounds.
  if (!mesh.skinBones().isEmpty()) {
    return;
  }
  const QVector3D minB = mesh.boundingBoxMin();
  const QVector3D maxB = mesh.boundingBoxMax();
  const QVector3D center = (minB + maxB) * 0.5f;
  auto positions = mesh.vertexAttributes().get<QVector3D>("position");
  if (!positions) {
    return;
  }
  auto &positionData = positions->data();
  ArtifactCore::Parallel::For(0, static_cast<int>(positionData.size()), static_cast<int>(positionData.size()), [&](int index) {
    positionData[index] -= center;
  });
  mesh.updateBounds();
  mesh.invalidateSkinningBase();
}

QString detectSiblingBaseColorTexture(const QString& modelPath)
{
  const QFileInfo modelInfo(modelPath);
  if (!modelInfo.exists()) {
    return {};
  }

  const QDir dir = modelInfo.dir();
  const QString base = modelInfo.completeBaseName();
  const QStringList candidates = {
      base + QStringLiteral("_basecolor.png"),
      base + QStringLiteral("_BaseColor.png"),
      base + QStringLiteral("_albedo.png"),
      base + QStringLiteral("_Albedo.png"),
      base + QStringLiteral("_diffuse.png"),
      base + QStringLiteral("_Diffuse.png"),
      base + QStringLiteral(".png"),
      base + QStringLiteral(".jpg"),
      base + QStringLiteral(".jpeg"),
      base + QStringLiteral(".tga"),
      base + QStringLiteral(".bmp"),
      base + QStringLiteral(".webp")
  };

  for (const auto& candidate : candidates) {
    if (dir.exists(candidate)) {
      return dir.absoluteFilePath(candidate);
    }
  }

  return {};
}
} // namespace

class Artifact3DLayer::Impl {
public:
  ModelRenderMode renderMode_ = ModelRenderMode::Solid;
  FixedGeometry3D fixedGeometry_ = FixedGeometry3D::Auto;
  float geometryWidth_ = 200.0f;
  float geometryHeight_ = 200.0f;
  float geometryDepth_ = 200.0f;
  int geometrySegments_ = 20;
  int geometryRings_ = 12;
  ArtifactCore::Material material_ = ArtifactCore::Material::makeDefault();
  Mesh mesh_; // The 3D mesh data
  QString sourcePath_;
  bool meshLoaded_ = false;
  bool affectedByLights_ = true;
  bool useTextureInSolid_ = false;
  bool wireOverlay_ = false;
  bool faceNormals_ = false;
  bool vertexNormals_ = false;
  int64_t lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  int skinAnimationClipIndex_ = 0;
  bool updatingSkinAnimation_ = false;
  bool skinAnimationEnabled_ = true;
  QHash<QString, float> blendShapeWeightOverrides_;
  float normalLength_ = 25.0f;
  float pointSize_ = 1.0f;
  QString lastRenderTraceOutcome_;
  Impl() {}
  ~Impl() {}
};

Artifact3DLayer::Artifact3DLayer() : impl_(new Impl()) {
  // Set the 3D flag directly as well (redundant but safe)
  setIs3D(true);
  // Load default mesh (cube)
  createFixedGeometryMesh(FixedGeometry3D::Cube);
  impl_->meshLoaded_ = true;
  updateSourceSizeFromMesh();
}

Artifact3DLayer::Artifact3DLayer(FixedGeometry3D geometry) : impl_(new Impl()) {
  setIs3D(true);
  setFixedGeometry(geometry);
}
Artifact3DLayer::~Artifact3DLayer() { delete impl_; }

void Artifact3DLayer::loadFromFile() {
  const QString reloadPath = impl_->sourcePath_;
  if (reloadPath.isEmpty()) {
    qWarning() << "[Artifact3DLayer] Cannot reload model without a source path";
    return;
  }
  // Keep reload behavior identical to explicit loading, including imported
  // material textures and source metadata.
  loadFromFile(reloadPath);
}

void Artifact3DLayer::loadFromFile(const QString &filePath) {
  // テンプレート ($VAR 等) はメンバ保持し、ファイル IO のみ展開結果を使う。
  const QString templatePath = filePath.trimmed();
  QString normalizedInput = templatePath;
  if (containsExpansionMarker(normalizedInput)) {
      ExpansionContext expansionContext;
      normalizedInput = expandTokens(normalizedInput, expansionContext);
  }
  if (normalizedInput.isEmpty()) {
    qWarning() << "[Artifact3DLayer] Ignoring empty source path reload";
    return;
  }
  impl_->blendShapeWeightOverrides_.clear();
  const QFileInfo inputInfo(normalizedInput);
  if (!inputInfo.exists() || !inputInfo.isFile()) {
    qWarning() << "[Artifact3DLayer] Ignoring missing model source:" << normalizedInput;
    return;
  }
  impl_->fixedGeometry_ = FixedGeometry3D::Auto;
  impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();

  ArtifactCore::MeshImporter importer;
  auto mesh = importer.importMeshFromFile(UniString(normalizedInput));

  if (mesh && mesh->vertexCount() > 0) {
    impl_->mesh_ = *mesh;
    if (!impl_->mesh_.skinBones().isEmpty()) {
      impl_->mesh_.applyDeformers(impl_->mesh_.skinPoseMatrices());
    }
    centerMeshPositions(impl_->mesh_);
    impl_->meshLoaded_ = true;
    updateSourceSizeFromMesh();
    const QString importedTexture = importer.lastBaseColorTexture();
    if (!importedTexture.isEmpty() &&
        impl_->material_.baseColorTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported base color texture:" << importedTexture;
      impl_->material_.setBaseColorTexture(
          ArtifactCore::UniString::fromQString(importedTexture));
    }
    const QString importedMetallicRoughnessTexture =
        importer.lastMetallicRoughnessTexture();
    if (!importedMetallicRoughnessTexture.isEmpty() &&
        impl_->material_.metallicRoughnessTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported metallic-roughness texture:"
               << importedMetallicRoughnessTexture;
      impl_->material_.setMetallicRoughnessTexture(
          ArtifactCore::UniString::fromQString(importedMetallicRoughnessTexture));
    }
    const QString importedNormalTexture = importer.lastNormalTexture();
    if (!importedNormalTexture.isEmpty() &&
        impl_->material_.normalTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported normal texture:" << importedNormalTexture;
      impl_->material_.setNormalTexture(
          ArtifactCore::UniString::fromQString(importedNormalTexture));
    }
    const QString importedEmissionTexture = importer.lastEmissionTexture();
    if (!importedEmissionTexture.isEmpty() &&
        impl_->material_.emissionTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported emission texture:"
               << importedEmissionTexture;
      impl_->material_.setEmissionTexture(
          ArtifactCore::UniString::fromQString(importedEmissionTexture));
    }
    const QString importedOcclusionTexture = importer.lastOcclusionTexture();
    if (!importedOcclusionTexture.isEmpty() &&
        impl_->material_.occlusionTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported occlusion texture:"
               << importedOcclusionTexture;
      impl_->material_.setOcclusionTexture(
          ArtifactCore::UniString::fromQString(importedOcclusionTexture));
    }
    const QString importedOpacityTexture = importer.lastOpacityTexture();
    if (!importedOpacityTexture.isEmpty() &&
        impl_->material_.opacityTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported opacity texture:"
               << importedOpacityTexture;
      impl_->material_.setOpacityTexture(
          ArtifactCore::UniString::fromQString(importedOpacityTexture));
    }
    if (impl_->material_.baseColorTexture().toQString().isEmpty()) {
      const QString detectedTexture = detectSiblingBaseColorTexture(normalizedInput);
      if (!detectedTexture.isEmpty()) {
        qDebug() << "[Artifact3DLayer] Auto-detected base color texture:" << detectedTexture;
        impl_->material_.setBaseColorTexture(ArtifactCore::UniString::fromQString(detectedTexture));
      }
    }
    impl_->renderMode_ = ModelRenderMode::Solid;
    const QFileInfo sourceInfo(normalizedInput);
    const QString normalizedSourcePath = sourceInfo.canonicalFilePath().isEmpty()
        ? templatePath
        : (sourceInfo.canonicalFilePath());
    // テンプレートを保持する (展開済み canonical パスではなく)
    impl_->sourcePath_ = containsExpansionMarker(templatePath) ? templatePath : normalizedSourcePath;
    setLayerName(sourceInfo.baseName());
    Q_EMIT changed();
    return;
  }

  // Fallback to cube on failure
  qWarning() << "Failed to load mesh from:" << normalizedInput
             << "- using default cube";
  impl_->fixedGeometry_ = FixedGeometry3D::Cube;
  impl_->sourcePath_.clear();
  createCubeMesh();
  impl_->meshLoaded_ = true;
  updateSourceSizeFromMesh();
  impl_->renderMode_ = ModelRenderMode::Solid;
  Q_EMIT changed();
}

void Artifact3DLayer::setFixedGeometry(FixedGeometry3D geometry)
{
  if (impl_->fixedGeometry_ == geometry && impl_->meshLoaded_) {
    return;
  }
  impl_->fixedGeometry_ = geometry;
  impl_->sourcePath_.clear();
  createFixedGeometryMesh(geometry);
  impl_->meshLoaded_ = true;
  updateSourceSizeFromMesh();
  impl_->renderMode_ = ModelRenderMode::Solid;
  Q_EMIT changed();
}

FixedGeometry3D Artifact3DLayer::fixedGeometry() const
{
  return impl_->fixedGeometry_;
}

QString Artifact3DLayer::sourcePath() const { return impl_->sourcePath_; }

UniString Artifact3DLayer::className() const { return QStringLiteral("Artifact3DLayer"); }

QJsonObject Artifact3DLayer::toJson() const {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Model3D);
  obj["sourcePath"] = impl_->sourcePath_;
  obj["animation.enabled"] = impl_->skinAnimationEnabled_;
  obj["animation.clipIndex"] = impl_->skinAnimationClipIndex_;
  QJsonArray blendShapeWeights;
  for (auto it = impl_->blendShapeWeightOverrides_.cbegin();
       it != impl_->blendShapeWeightOverrides_.cend(); ++it) {
    blendShapeWeights.append(QJsonObject{
        {QStringLiteral("name"), it.key()},
        {QStringLiteral("weight"), it.value()},
        {QStringLiteral("override"), true}});
  }
  obj["deformers.blendShapes"] = blendShapeWeights;
  obj["renderMode"] = static_cast<int>(impl_->renderMode_);
  obj["render.affectedByLights"] = impl_->affectedByLights_;
  obj["render.useTextureInSolid"] = impl_->useTextureInSolid_;
  obj["render.wireOverlay"] = impl_->wireOverlay_;
  obj["render.faceNormals"] = impl_->faceNormals_;
  obj["render.vertexNormals"] = impl_->vertexNormals_;
  obj["render.normalLength"] = impl_->normalLength_;
  obj["render.pointSize"] = impl_->pointSize_;
  obj["fixedGeometry"] = static_cast<int>(impl_->fixedGeometry_);
  obj["geometry.width"] = impl_->geometryWidth_;
  obj["geometry.height"] = impl_->geometryHeight_;
  obj["geometry.depth"] = impl_->geometryDepth_;
  obj["geometry.segments"] = impl_->geometrySegments_;
  obj["geometry.rings"] = impl_->geometryRings_;
  const QColor baseColor = impl_->material_.baseColor();
  obj["material.base.color"] = QJsonObject{
      {QStringLiteral("r"), baseColor.redF()},
      {QStringLiteral("g"), baseColor.greenF()},
      {QStringLiteral("b"), baseColor.blueF()},
      {QStringLiteral("a"), baseColor.alphaF()}};
  obj["material.metallic"] = impl_->material_.metallic();
  obj["material.roughness"] = impl_->material_.roughness();
  obj["material.specular"] = impl_->material_.specular();
  obj["material.ior"] = impl_->material_.ior();
  obj["material.transmission"] = impl_->material_.transmission();
  obj["material.clearcoat"] = impl_->material_.clearcoat();
  obj["material.clearcoatRoughness"] = impl_->material_.clearcoatRoughness();
  obj["material.sheen"] = impl_->material_.sheen();
  obj["material.opacity"] = impl_->material_.opacity();
  obj["material.alphaMode"] = static_cast<int>(impl_->material_.alphaMode());
  obj["material.alphaCutoff"] = impl_->material_.alphaCutoff();
  const QColor emissionColor = impl_->material_.emissionColor();
  obj["material.emission.color"] = QJsonObject{
      {QStringLiteral("r"), emissionColor.redF()},
      {QStringLiteral("g"), emissionColor.greenF()},
      {QStringLiteral("b"), emissionColor.blueF()},
      {QStringLiteral("a"), emissionColor.alphaF()}};
  obj["material.emissionStrength"] = impl_->material_.emissionStrength();
  obj["material.normalStrength"] = impl_->material_.normalStrength();
  obj["material.occlusionStrength"] = impl_->material_.occlusionStrength();
  obj["material.baseColorTexture"] = impl_->material_.baseColorTexture().toQString();
  obj["material.metallicRoughnessTexture"] =
      impl_->material_.metallicRoughnessTexture().toQString();
  obj["material.normalTexture"] = impl_->material_.normalTexture().toQString();
  obj["material.emissionTexture"] = impl_->material_.emissionTexture().toQString();
  obj["material.occlusionTexture"] = impl_->material_.occlusionTexture().toQString();
  obj["material.opacityTexture"] = impl_->material_.opacityTexture().toQString();
  const QString materialGraphJson = impl_->material_.materialGraphJson().toQString();
  if (!materialGraphJson.trimmed().isEmpty()) {
    obj["material.graph"] = materialGraphJson;
  }
  return obj;
}

void Artifact3DLayer::fromJsonProperties(const QJsonObject& obj)
{
  ArtifactAbstractLayer::fromJsonProperties(obj);

  impl_->geometryWidth_ = finiteClamped(static_cast<float>(obj.value("geometry.width").toDouble(impl_->geometryWidth_)), impl_->geometryWidth_, 0.01f, 100000.0f);
  impl_->geometryHeight_ = finiteClamped(static_cast<float>(obj.value("geometry.height").toDouble(impl_->geometryHeight_)), impl_->geometryHeight_, 0.01f, 100000.0f);
  impl_->geometryDepth_ = finiteClamped(static_cast<float>(obj.value("geometry.depth").toDouble(impl_->geometryDepth_)), impl_->geometryDepth_, 0.01f, 100000.0f);
  impl_->geometrySegments_ = std::clamp(obj.value("geometry.segments").toInt(impl_->geometrySegments_), 3, 128);
  impl_->geometryRings_ = std::clamp(obj.value("geometry.rings").toInt(impl_->geometryRings_), 2, 128);

  const QString sourcePath = obj.contains("model.sourcePath")
                                 ? obj.value("model.sourcePath").toString()
                                 : obj.value("sourcePath").toString();
  if (!sourcePath.isEmpty()) {
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.exists() && sourceInfo.isFile()) {
      loadFromFile(sourcePath);
    } else {
      // A missing source must not leave a previous model visible when an
      // existing layer instance is reused for project restoration.
      impl_->sourcePath_ = sourcePath;
      impl_->blendShapeWeightOverrides_.clear();
      impl_->mesh_ = Mesh();
      impl_->meshLoaded_ = false;
      updateSourceSizeFromMesh();
      impl_->lastRenderTraceOutcome_.clear();
    }
  }

  if (obj.contains("animation.enabled")) {
    impl_->skinAnimationEnabled_ = obj.value("animation.enabled").toBool(
        impl_->skinAnimationEnabled_);
  }
  if (obj.contains("animation.clipIndex")) {
    const int requestedClip = obj.value("animation.clipIndex").toInt(0);
    const int clipCount = static_cast<int>(
        impl_->mesh_.skinAnimationClips().size());
    impl_->skinAnimationClipIndex_ = clipCount > 0
        ? std::clamp(requestedClip, 0, clipCount - 1)
        : std::max(0, requestedClip);
    impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  }

  if (obj.value("deformers.blendShapes").isArray()) {
    const QJsonArray savedWeights =
        obj.value("deformers.blendShapes").toArray();
    for (const QJsonValue& value : savedWeights) {
      if (!value.isObject()) continue;
      const QJsonObject entry = value.toObject();
      if (entry.contains("override") &&
          !entry.value("override").toBool(true)) {
        continue;
      }
      const QString name = entry.value("name").toString();
      if (name.isEmpty()) continue;
      const float weight = static_cast<float>(
          entry.value("weight").toDouble(0.0));
      if (!std::isfinite(weight)) continue;
      for (int index = 0; index < impl_->mesh_.blendShapes().size(); ++index) {
        if (impl_->mesh_.blendShapes()[index].name == name) {
          impl_->blendShapeWeightOverrides_.insert(name, weight);
          impl_->mesh_.setBlendShapeWeight(index, weight);
        }
      }
    }
    updateSourceSizeFromMesh();
  }

  if (obj.contains("fixedGeometry")) {
    const int geometry = obj.value("fixedGeometry").toInt(
        static_cast<int>(FixedGeometry3D::Auto));
    if (geometry >= static_cast<int>(FixedGeometry3D::Auto) &&
        geometry <= static_cast<int>(FixedGeometry3D::Pyramid)) {
      if (geometry == static_cast<int>(FixedGeometry3D::Auto) &&
          sourcePath.isEmpty()) {
        // Reusing a layer instance for source-less JSON must not retain the
        // previously loaded model. Auto has no model to restore here, so use
        // the same safe fallback as a failed model load.
        setFixedGeometry(FixedGeometry3D::Cube);
      } else if (geometry != static_cast<int>(FixedGeometry3D::Auto) ||
                 sourcePath.isEmpty()) {
        setFixedGeometry(static_cast<FixedGeometry3D>(geometry));
      }
    }
  }

  if (obj.contains("renderMode")) {
    const int mode = obj.value("renderMode").toInt(
        static_cast<int>(ModelRenderMode::Solid));
    if (mode >= static_cast<int>(ModelRenderMode::Wireframe) &&
        mode <= static_cast<int>(ModelRenderMode::Solid)) {
      setRenderMode(static_cast<ModelRenderMode>(mode));
    }
  }
  impl_->affectedByLights_ =
      obj.value("render.affectedByLights").toBool(impl_->affectedByLights_);
  impl_->useTextureInSolid_ =
      obj.value("render.useTextureInSolid").toBool(impl_->useTextureInSolid_);
  impl_->wireOverlay_ =
      obj.value("render.wireOverlay").toBool(impl_->wireOverlay_);
  impl_->faceNormals_ =
      obj.value("render.faceNormals").toBool(impl_->faceNormals_);
  impl_->vertexNormals_ =
      obj.value("render.vertexNormals").toBool(impl_->vertexNormals_);
  impl_->normalLength_ = finiteClamped(
      static_cast<float>(obj.value("render.normalLength").toDouble(
          impl_->normalLength_)), impl_->normalLength_, 0.01f, 10000.0f);
  impl_->pointSize_ = finiteClamped(
      static_cast<float>(obj.value("render.pointSize").toDouble(
          impl_->pointSize_)), impl_->pointSize_, 0.25f, 8.0f);

  const QJsonObject baseColor = obj.value("material.base.color").toObject();
  if (!baseColor.isEmpty()) {
    const float r = finiteClamped(static_cast<float>(baseColor.value("r").toDouble(1.0)), 1.0f, 0.0f, 1.0f);
    const float g = finiteClamped(static_cast<float>(baseColor.value("g").toDouble(1.0)), 1.0f, 0.0f, 1.0f);
    const float b = finiteClamped(static_cast<float>(baseColor.value("b").toDouble(1.0)), 1.0f, 0.0f, 1.0f);
    const float a = finiteClamped(static_cast<float>(baseColor.value("a").toDouble(1.0)), 1.0f, 0.0f, 1.0f);
    impl_->material_.setBaseColor(QColor::fromRgbF(r, g, b, a));
  }
  impl_->material_.setMetallic(
      finiteClamped(static_cast<float>(obj.value("material.metallic").toDouble(impl_->material_.metallic())),
                    impl_->material_.metallic(), 0.0f, 1.0f));
  impl_->material_.setRoughness(
      finiteClamped(static_cast<float>(obj.value("material.roughness").toDouble(impl_->material_.roughness())),
                    impl_->material_.roughness(), 0.0f, 1.0f));
  impl_->material_.setSpecular(
      finiteClamped(static_cast<float>(obj.value("material.specular").toDouble(impl_->material_.specular())),
                    impl_->material_.specular(), 0.0f, 1.0f));
  impl_->material_.setIOR(
      finiteClamped(static_cast<float>(obj.value("material.ior").toDouble(impl_->material_.ior())),
                    impl_->material_.ior(), 1.0f, 3.0f));
  impl_->material_.setTransmission(
      finiteClamped(static_cast<float>(obj.value("material.transmission").toDouble(impl_->material_.transmission())),
                    impl_->material_.transmission(), 0.0f, 1.0f));
  impl_->material_.setClearcoat(
      finiteClamped(static_cast<float>(obj.value("material.clearcoat").toDouble(impl_->material_.clearcoat())),
                    impl_->material_.clearcoat(), 0.0f, 1.0f));
  impl_->material_.setClearcoatRoughness(
      finiteClamped(static_cast<float>(obj.value("material.clearcoatRoughness").toDouble(impl_->material_.clearcoatRoughness())),
                    impl_->material_.clearcoatRoughness(), 0.0f, 1.0f));
  impl_->material_.setSheen(
      finiteClamped(static_cast<float>(obj.value("material.sheen").toDouble(impl_->material_.sheen())),
                    impl_->material_.sheen(), 0.0f, 1.0f));
  impl_->material_.setOpacity(
      finiteClamped(static_cast<float>(obj.value("material.opacity").toDouble(impl_->material_.opacity())),
                    impl_->material_.opacity(), 0.0f, 1.0f));
  if (obj.contains("material.alphaMode")) {
    const int alphaMode = std::clamp(
        obj.value("material.alphaMode").toInt(
            static_cast<int>(impl_->material_.alphaMode())),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Opaque),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Blended));
    impl_->material_.setAlphaMode(
        static_cast<ArtifactCore::MaterialAlphaMode>(alphaMode));
  }
  impl_->material_.setAlphaCutoff(finiteClamped(
      static_cast<float>(obj.value("material.alphaCutoff").toDouble(
          impl_->material_.alphaCutoff())),
      impl_->material_.alphaCutoff(), 0.0f, 1.0f));
  const QJsonObject emissionColor = obj.value("material.emission.color").toObject();
  if (!emissionColor.isEmpty()) {
    impl_->material_.setEmissionColor(QColor::fromRgbF(
        finiteClamped(static_cast<float>(emissionColor.value("r").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(emissionColor.value("g").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(emissionColor.value("b").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(emissionColor.value("a").toDouble(1.0)), 1.0f, 0.0f, 1.0f)));
  }
  impl_->material_.setEmissionStrength(
      finiteClamped(static_cast<float>(obj.value("material.emissionStrength").toDouble(impl_->material_.emissionStrength())),
                    impl_->material_.emissionStrength(), 0.0f, 100000.0f));
  impl_->material_.setNormalStrength(
      finiteClamped(static_cast<float>(obj.value("material.normalStrength").toDouble(impl_->material_.normalStrength())),
                    impl_->material_.normalStrength(), 0.0f, 10.0f));
  impl_->material_.setOcclusionStrength(
      finiteClamped(static_cast<float>(obj.value("material.occlusionStrength").toDouble(impl_->material_.occlusionStrength())),
                    impl_->material_.occlusionStrength(), 0.0f, 1.0f));

  const QString baseColorTexture = obj.contains("material.baseColorTexture")
                                       ? obj.value("material.baseColorTexture").toString()
                                       : QString();
  impl_->material_.setBaseColorTexture(ArtifactCore::UniString::fromQString(baseColorTexture));

  const QString metallicRoughnessTexture =
      obj.value("material.metallicRoughnessTexture").toString();
  impl_->material_.setMetallicRoughnessTexture(
      ArtifactCore::UniString::fromQString(metallicRoughnessTexture));

  const QString normalTexture = obj.value("material.normalTexture").toString();
  impl_->material_.setNormalTexture(ArtifactCore::UniString::fromQString(normalTexture));

  const QString emissionTexture = obj.value("material.emissionTexture").toString();
  impl_->material_.setEmissionTexture(ArtifactCore::UniString::fromQString(emissionTexture));

  const QString occlusionTexture = obj.value("material.occlusionTexture").toString();
  impl_->material_.setOcclusionTexture(ArtifactCore::UniString::fromQString(occlusionTexture));

  const QString opacityTexture = obj.value("material.opacityTexture").toString();
  impl_->material_.setOpacityTexture(ArtifactCore::UniString::fromQString(opacityTexture));

  // Material graph JSON is stored verbatim; the renderer compiles it on
  // first draw. Invalid payloads are ignored at apply time, never here.
  impl_->material_.setMaterialGraphJson(ArtifactCore::UniString::fromQString(
      obj.value("material.graph").toString()));
}

void Artifact3DLayer::createCubeMesh() {
  const float halfWidth = impl_->geometryWidth_ * 0.5f;
  const float halfHeight = impl_->geometryHeight_ * 0.5f;
  const float halfDepth = impl_->geometryDepth_ * 0.5f;
  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;
  positions.reserve(24);
  normals.reserve(24);
  uvs.reserve(24);

  const auto addFace = [&](const QVector3D &normal,
                           const QVector3D &bottomLeft,
                           const QVector3D &bottomRight,
                           const QVector3D &topRight,
                           const QVector3D &topLeft) {
    const int first = positions.size();
    positions << bottomLeft << bottomRight << topRight << topLeft;
    normals << normal << normal << normal << normal;
    uvs << QVector2D(0.0f, 0.0f) << QVector2D(1.0f, 0.0f)
        << QVector2D(1.0f, 1.0f) << QVector2D(0.0f, 1.0f);
    // Preserve the authored box face as a quad. RenderData triangulates this
    // later for the GPU, while edit overlays retain the quad boundary.
    impl_->mesh_.addPolygon({first, first + 1, first + 2, first + 3});
  };

  addFace(QVector3D(0.0f, 0.0f, 1.0f),
          QVector3D(-halfWidth, -halfHeight, halfDepth),
          QVector3D(halfWidth, -halfHeight, halfDepth),
          QVector3D(halfWidth, halfHeight, halfDepth),
          QVector3D(-halfWidth, halfHeight, halfDepth));
  addFace(QVector3D(0.0f, 0.0f, -1.0f),
          QVector3D(halfWidth, -halfHeight, -halfDepth),
          QVector3D(-halfWidth, -halfHeight, -halfDepth),
          QVector3D(-halfWidth, halfHeight, -halfDepth),
          QVector3D(halfWidth, halfHeight, -halfDepth));
  addFace(QVector3D(1.0f, 0.0f, 0.0f),
          QVector3D(halfWidth, -halfHeight, halfDepth),
          QVector3D(halfWidth, -halfHeight, -halfDepth),
          QVector3D(halfWidth, halfHeight, -halfDepth),
          QVector3D(halfWidth, halfHeight, halfDepth));
  addFace(QVector3D(-1.0f, 0.0f, 0.0f),
          QVector3D(-halfWidth, -halfHeight, -halfDepth),
          QVector3D(-halfWidth, -halfHeight, halfDepth),
          QVector3D(-halfWidth, halfHeight, halfDepth),
          QVector3D(-halfWidth, halfHeight, -halfDepth));
  addFace(QVector3D(0.0f, 1.0f, 0.0f),
          QVector3D(-halfWidth, halfHeight, halfDepth),
          QVector3D(halfWidth, halfHeight, halfDepth),
          QVector3D(halfWidth, halfHeight, -halfDepth),
          QVector3D(-halfWidth, halfHeight, -halfDepth));
  addFace(QVector3D(0.0f, -1.0f, 0.0f),
          QVector3D(-halfWidth, -halfHeight, -halfDepth),
          QVector3D(halfWidth, -halfHeight, -halfDepth),
          QVector3D(halfWidth, -halfHeight, halfDepth),
          QVector3D(-halfWidth, -halfHeight, halfDepth));

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  positionAttr->data() = positions;
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  normalAttr->data() = normals;
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  uvAttr->data() = uvs;
}

void Artifact3DLayer::createPlaneMesh()
{
  const float halfWidth = impl_->geometryWidth_ * 0.5f;
  const float halfHeight = impl_->geometryHeight_ * 0.5f;
  QVector<QVector3D> positions = {
      QVector3D(-halfWidth, -halfHeight, 0.0f),
      QVector3D(halfWidth, -halfHeight, 0.0f),
      QVector3D(halfWidth, halfHeight, 0.0f),
      QVector3D(-halfWidth, halfHeight, 0.0f)
  };

  impl_->mesh_.setVertexCount(8);
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  for (int i = 0; i < positions.size(); ++i) {
    (*positionAttr)[i] = positions[i];
    (*positionAttr)[i + 4] = positions[i];
    (*normalAttr)[i] = QVector3D(0.0f, 0.0f, 1.0f);
    (*normalAttr)[i + 4] = QVector3D(0.0f, 0.0f, -1.0f);
    const QVector2D uv(
        impl_->geometryWidth_ > 0.0f ? (positions[i].x() + halfWidth) / impl_->geometryWidth_ : 0.5f,
        impl_->geometryHeight_ > 0.0f ? (positions[i].y() + halfHeight) / impl_->geometryHeight_ : 0.5f);
    (*uvAttr)[i] = uv;
    (*uvAttr)[i + 4] = uv;
  }
  // Keep each side as one source quad; GPU render data is triangulated only
  // at submission time, so the selection overlay does not show a fake diagonal.
  impl_->mesh_.addPolygon({0, 1, 2, 3});
  impl_->mesh_.addPolygon({4, 7, 6, 5});
}

void Artifact3DLayer::createSphereMesh()
{
  const int kSegments = std::clamp(impl_->geometrySegments_, 3, 128);
  const int kRings = std::clamp(impl_->geometryRings_, 2, 128);
  const float radiusX = impl_->geometryWidth_ * 0.5f;
  const float radiusY = impl_->geometryHeight_ * 0.5f;
  const float radiusZ = impl_->geometryDepth_ * 0.5f;

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;
  positions.reserve((kRings + 1) * (kSegments + 1));
  normals.reserve((kRings + 1) * (kSegments + 1));
  uvs.reserve((kRings + 1) * (kSegments + 1));

  for (int ring = 0; ring <= kRings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(kRings);
    const float phi = static_cast<float>(M_PI) * v;
    const float y = std::cos(phi);
    const float r = std::sin(phi);
    for (int segment = 0; segment <= kSegments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
      const float theta = static_cast<float>(M_PI * 2.0) * u;
      const float x = std::cos(theta) * r;
      const float z = std::sin(theta) * r;
      const QVector3D position(x * radiusX, y * radiusY, z * radiusZ);
      positions.push_back(position);
      normals.push_back(QVector3D(x, y, z).normalized());
      uvs.push_back(QVector2D(u, 1.0f - v));
    }
  }

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;

  const int stride = kSegments + 1;
  for (int ring = 0; ring < kRings; ++ring) {
    for (int segment = 0; segment < kSegments; ++segment) {
      const int a = ring * stride + segment;
      const int b = a + 1;
      const int c = a + stride;
      const int d = c + 1;
      // The pole fans are genuinely triangular. Everywhere else preserve the
      // latitude/longitude cell as an editable quad; RenderData triangulates
      // it only at GPU submission time.
      if (ring == 0 || ring + 1 == kRings) {
        impl_->mesh_.addPolygon({a, c, b});
        impl_->mesh_.addPolygon({b, c, d});
      } else {
        impl_->mesh_.addPolygon({a, c, d, b});
      }
    }
  }
}

void Artifact3DLayer::createCylinderMesh()
{
  const int kSegments = std::clamp(impl_->geometrySegments_, 3, 128);
  const float kRadiusX = impl_->geometryWidth_ * 0.5f;
  const float kRadiusZ = impl_->geometryDepth_ * 0.5f;
  const float kHalfHeight = impl_->geometryHeight_ * 0.5f;

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;

  auto appendVertex = [&](const QVector3D& position, const QVector3D& normal, const QVector2D& uv) {
    positions.push_back(position);
    normals.push_back(normal);
    uvs.push_back(uv);
    return positions.size() - 1;
  };

  QVector<int> bottomRing;
  QVector<int> topRing;
  bottomRing.reserve(kSegments);
  topRing.reserve(kSegments);

  for (int segment = 0; segment < kSegments; ++segment) {
    const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kRadiusX;
    const float z = std::sin(theta) * kRadiusZ;
    const QVector3D sideNormal = QVector3D(
        kRadiusZ > 0.0f ? x / kRadiusZ : x,
        0.0f,
        kRadiusX > 0.0f ? z / kRadiusX : z).normalized();
    bottomRing.push_back(appendVertex(QVector3D(x, -kHalfHeight, z), sideNormal, QVector2D(u, 1.0f)));
    topRing.push_back(appendVertex(QVector3D(x, kHalfHeight, z), sideNormal, QVector2D(u, 0.0f)));
  }

  for (int segment = 0; segment < kSegments; ++segment) {
    const int next = (segment + 1) % kSegments;
    const int b0 = bottomRing[segment];
    const int b1 = bottomRing[next];
    const int t0 = topRing[segment];
    const int t1 = topRing[next];
    // Keep the authored side panel as a quad for editing/selection. The GPU
    // render path performs its own fan triangulation at submission time.
    impl_->mesh_.addPolygon({b0, t0, t1, b1});
  }

  const int bottomCenter = appendVertex(QVector3D(0.0f, -kHalfHeight, 0.0f),
                                        QVector3D(0.0f, -1.0f, 0.0f),
                                        QVector2D(0.5f, 0.5f));
  const int topCenter = appendVertex(QVector3D(0.0f, kHalfHeight, 0.0f),
                                     QVector3D(0.0f, 1.0f, 0.0f),
                                     QVector2D(0.5f, 0.5f));

  QVector<int> bottomCap;
  QVector<int> topCap;
  bottomCap.reserve(kSegments);
  topCap.reserve(kSegments);
  for (int segment = 0; segment < kSegments; ++segment) {
    const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kRadiusX;
    const float z = std::sin(theta) * kRadiusZ;
    bottomCap.push_back(appendVertex(QVector3D(x, -kHalfHeight, z),
                                     QVector3D(0.0f, -1.0f, 0.0f),
                                     QVector2D(kRadiusX > 0.0f ? x / impl_->geometryWidth_ + 0.5f : 0.5f,
                                               kRadiusZ > 0.0f ? z / impl_->geometryDepth_ + 0.5f : 0.5f)));
    topCap.push_back(appendVertex(QVector3D(x, kHalfHeight, z),
                                  QVector3D(0.0f, 1.0f, 0.0f),
                                  QVector2D(kRadiusX > 0.0f ? x / impl_->geometryWidth_ + 0.5f : 0.5f,
                                            kRadiusZ > 0.0f ? z / impl_->geometryDepth_ + 0.5f : 0.5f)));
  }

  for (int segment = 0; segment < kSegments; ++segment) {
    const int next = (segment + 1) % kSegments;
    impl_->mesh_.addPolygon({bottomCenter, bottomCap[next], bottomCap[segment]});
    impl_->mesh_.addPolygon({topCenter, topCap[segment], topCap[next]});
  }

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;
}

void Artifact3DLayer::createConeMesh()
{
  const int kSegments = std::clamp(impl_->geometrySegments_, 3, 128);
  const float kRadiusX = impl_->geometryWidth_ * 0.5f;
  const float kRadiusZ = impl_->geometryDepth_ * 0.5f;
  const float kHalfHeight = impl_->geometryHeight_ * 0.5f;

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;

  auto appendVertex = [&](const QVector3D& position, const QVector3D& normal, const QVector2D& uv) {
    positions.push_back(position);
    normals.push_back(normal);
    uvs.push_back(uv);
    return positions.size() - 1;
  };

  const QVector3D tip(0.0f, kHalfHeight, 0.0f);
  QVector<int> baseRing;
  baseRing.reserve(kSegments);

  for (int segment = 0; segment < kSegments; ++segment) {
    const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kRadiusX;
    const float z = std::sin(theta) * kRadiusZ;
    const QVector3D tangent(-z, 0.0f, x);
    const QVector3D slope = tip - QVector3D(x, -kHalfHeight, z);
    const QVector3D sideNormal = QVector3D::crossProduct(tangent, slope).normalized();
    baseRing.push_back(appendVertex(QVector3D(x, -kHalfHeight, z), sideNormal, QVector2D(u, 1.0f)));
  }

  for (int segment = 0; segment < kSegments; ++segment) {
    const int next = (segment + 1) % kSegments;
    const float midU = (static_cast<float>(segment) + 0.5f) / static_cast<float>(kSegments);
    const QVector3D tipNormal = (positions[baseRing[segment]] + positions[baseRing[next]]) * 0.5f;
    const int tipIndex = appendVertex(tip, QVector3D(tipNormal.x(), impl_->geometryHeight_, tipNormal.z()).normalized(),
                                      QVector2D(midU, 0.0f));
    impl_->mesh_.addPolygon({baseRing[segment], tipIndex, baseRing[next]});
  }

  const int bottomCenter = appendVertex(QVector3D(0.0f, -kHalfHeight, 0.0f),
                                        QVector3D(0.0f, -1.0f, 0.0f),
                                        QVector2D(0.5f, 0.5f));
  QVector<int> bottomCap;
  bottomCap.reserve(kSegments);
  for (int segment = 0; segment < kSegments; ++segment) {
    const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kRadiusX;
    const float z = std::sin(theta) * kRadiusZ;
    bottomCap.push_back(appendVertex(QVector3D(x, -kHalfHeight, z),
                                     QVector3D(0.0f, -1.0f, 0.0f),
                                     QVector2D(kRadiusX > 0.0f ? x / impl_->geometryWidth_ + 0.5f : 0.5f,
                                               kRadiusZ > 0.0f ? z / impl_->geometryDepth_ + 0.5f : 0.5f)));
  }

  for (int segment = 0; segment < kSegments; ++segment) {
    const int next = (segment + 1) % kSegments;
    impl_->mesh_.addPolygon({bottomCenter, bottomCap[next], bottomCap[segment]});
  }

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;
}

void Artifact3DLayer::createFixedGeometryMesh(FixedGeometry3D geometry)
{
  impl_->mesh_ = Mesh();
  switch (geometry) {
  case FixedGeometry3D::Plane:
    createPlaneMesh();
    break;
  case FixedGeometry3D::Cube:
    createCubeMesh();
    break;
  case FixedGeometry3D::Sphere:
    createSphereMesh();
    break;
  case FixedGeometry3D::Cylinder:
    createCylinderMesh();
    break;
  case FixedGeometry3D::Cone:
    createConeMesh();
    break;
  case FixedGeometry3D::Torus:
    createTorusMesh();
    break;
  case FixedGeometry3D::Capsule:
    createCapsuleMesh();
    break;
  case FixedGeometry3D::Pyramid:
    createPyramidMesh();
    break;
  case FixedGeometry3D::Auto:
  default:
    createCubeMesh();
    break;
  }
}

void Artifact3DLayer::createTorusMesh()
{
  const int kSegments = std::clamp(impl_->geometrySegments_, 3, 128);
  const int kRings = std::clamp(impl_->geometryRings_, 2, 128);
  const float majorRadiusX = std::max(0.01f, impl_->geometryWidth_ * 0.5f);
  const float majorRadiusY = std::max(0.01f, impl_->geometryHeight_ * 0.5f);
  const float tubeRadius = std::max(0.01f, impl_->geometryDepth_ * 0.25f);

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;
  positions.reserve((kRings + 1) * (kSegments + 1));
  normals.reserve((kRings + 1) * (kSegments + 1));
  uvs.reserve((kRings + 1) * (kSegments + 1));

  for (int ring = 0; ring <= kRings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(kRings);
    const float phi = static_cast<float>(M_PI * 2.0) * v;
    const float cosPhi = std::cos(phi);
    const float sinPhi = std::sin(phi);
    for (int segment = 0; segment <= kSegments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
      const float theta = static_cast<float>(M_PI * 2.0) * u;
      const float cosTheta = std::cos(theta);
      const float sinTheta = std::sin(theta);

      const float centerX = cosPhi * majorRadiusX;
      const float centerY = sinPhi * majorRadiusY;
      const float positionX = (majorRadiusX + tubeRadius * cosTheta) * cosPhi;
      const float positionY = (majorRadiusY + tubeRadius * cosTheta) * sinPhi;
      const float positionZ = tubeRadius * sinTheta;
      positions.push_back(QVector3D(positionX, positionY, positionZ));

      const QVector3D outward(centerX, centerY, 0.0f);
      const QVector3D up(0.0f, 0.0f, 1.0f);
      const QVector3D tubeNormal = cosTheta * outward.normalized()
          + sinTheta * up;
      normals.push_back(tubeNormal.normalized());
      uvs.push_back(QVector2D(u, v));
    }
  }

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;

  const int stride = kSegments + 1;
  for (int ring = 0; ring < kRings; ++ring) {
    for (int segment = 0; segment < kSegments; ++segment) {
      const int a = ring * stride + segment;
      const int b = a + 1;
      const int c = a + stride;
      const int d = c + 1;
      impl_->mesh_.addPolygon({a, c, d, b});
    }
  }
}

void Artifact3DLayer::createCapsuleMesh()
{
  const int kSegments = std::clamp(impl_->geometrySegments_, 3, 128);
  const float kRadiusX = std::max(0.01f, impl_->geometryWidth_ * 0.5f);
  const float kRadiusZ = std::max(0.01f, impl_->geometryDepth_ * 0.5f);
  const float kTotalHeight = std::max(2.01f, impl_->geometryHeight_);
  const float kCylinderHalfHeight =
      std::max(0.0f, (kTotalHeight * 0.5f) - kRadiusX);
  const float kMaxRing = std::max(2, std::min(32, kSegments));

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;

  auto appendVertex = [&](const QVector3D& position,
                          const QVector3D& normal,
                          const QVector2D& uv) {
    positions.push_back(position);
    normals.push_back(normal);
    uvs.push_back(uv);
    return positions.size() - 1;
  };

  QVector<int> bottomRing;
  QVector<int> topRing;
  bottomRing.reserve(kSegments);
  topRing.reserve(kSegments);

  for (int segment = 0; segment < kSegments; ++segment) {
    const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kRadiusX;
    const float z = std::sin(theta) * kRadiusZ;
    const QVector3D sideNormal = QVector3D(
        kRadiusZ > 0.0f ? x / kRadiusZ : x,
        0.0f,
        kRadiusX > 0.0f ? z / kRadiusX : z).normalized();
    bottomRing.push_back(
        appendVertex(QVector3D(x, -kCylinderHalfHeight, z), sideNormal,
                     QVector2D(u, 1.0f)));
    topRing.push_back(
        appendVertex(QVector3D(x, kCylinderHalfHeight, z), sideNormal,
                     QVector2D(u, 0.0f)));
  }

  for (int segment = 0; segment < kSegments; ++segment) {
    const int next = (segment + 1) % kSegments;
    const int b0 = bottomRing[segment];
    const int b1 = bottomRing[next];
    const int t0 = topRing[segment];
    const int t1 = topRing[next];
    impl_->mesh_.addPolygon({b0, t0, t1, b1});
  }

  const auto buildHemisphere =
      [&](float sign, const QVector<int>& baseRing) {
        QVector<QVector<int>> rings;
        rings.reserve(kMaxRing + 1);
        rings.push_back(baseRing);
        for (int ring = 1; ring <= kMaxRing; ++ring) {
          const float t = static_cast<float>(ring) / static_cast<float>(kMaxRing);
          const float phi = (M_PI * 0.5f) * t;
          const float yOffset = sign * kRadiusX * std::cos(phi);
          const float ringRadius = std::sin(phi);
          QVector<int> ringIndices;
          ringIndices.reserve(kSegments);
          for (int segment = 0; segment < kSegments; ++segment) {
            const float u = static_cast<float>(segment) /
                static_cast<float>(kSegments);
            const float theta = static_cast<float>(M_PI * 2.0) * u;
            const float x = std::cos(theta) * kRadiusX * ringRadius;
            const float z = std::sin(theta) * kRadiusZ * ringRadius;
            const QVector3D normal = QVector3D(
                kRadiusZ > 0.0f ? x / kRadiusZ : x,
                sign * std::cos(phi),
                kRadiusX > 0.0f ? z / kRadiusX : z).normalized();
            ringIndices.push_back(appendVertex(
                QVector3D(x, yOffset, z), normal, QVector2D(u, 0.5f - 0.5f * sign * t)));
          }
          rings.push_back(ringIndices);
        }
        const int poleIndex = appendVertex(
            QVector3D(0.0f, sign * kCylinderHalfHeight + sign * kRadiusX,
                      0.0f),
            QVector3D(0.0f, sign, 0.0f),
            QVector2D(0.5f, sign > 0.0f ? 0.0f : 1.0f));
        for (int ring = 0; ring < kMaxRing; ++ring) {
          const auto& current = rings[ring];
          const auto& next = rings[ring + 1];
          for (int segment = 0; segment < kSegments; ++segment) {
            const int nextSegment = (segment + 1) % kSegments;
            const int a = current[segment];
            const int b = current[nextSegment];
            const int c = next[segment];
            const int d = next[nextSegment];
            if (ring + 1 == kMaxRing) {
              impl_->mesh_.addPolygon({a, c, poleIndex});
              impl_->mesh_.addPolygon({c, d, poleIndex});
            } else {
              impl_->mesh_.addPolygon({a, c, d, b});
            }
          }
        }
      };

  buildHemisphere(-1.0f, bottomRing);
  buildHemisphere(1.0f, topRing);

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;
}

void Artifact3DLayer::createPyramidMesh()
{
  const int kSides = std::clamp(impl_->geometrySegments_, 3, 128);
  const float kHalfWidth = std::max(0.01f, impl_->geometryWidth_ * 0.5f);
  const float kHalfDepth = std::max(0.01f, impl_->geometryDepth_ * 0.5f);
  const float kHalfHeight = std::max(0.01f, impl_->geometryHeight_ * 0.5f);

  QVector<QVector3D> positions;
  QVector<QVector3D> normals;
  QVector<QVector2D> uvs;

  auto appendVertex = [&](const QVector3D& position,
                          const QVector3D& normal,
                          const QVector2D& uv) {
    positions.push_back(position);
    normals.push_back(normal);
    uvs.push_back(uv);
    return positions.size() - 1;
  };

  const QVector3D apex(0.0f, kHalfHeight, 0.0f);
  QVector<int> baseRing;
  baseRing.reserve(kSides);

  for (int side = 0; side < kSides; ++side) {
    const float u = static_cast<float>(side) / static_cast<float>(kSides);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kHalfWidth;
    const float z = std::sin(theta) * kHalfDepth;
    baseRing.push_back(appendVertex(QVector3D(x, -kHalfHeight, z),
                                    QVector3D(0.0f, -1.0f, 0.0f),
                                    QVector2D(u, 1.0f)));
  }

  for (int side = 0; side < kSides; ++side) {
    const int next = (side + 1) % kSides;
    const float midU = (static_cast<float>(side) + 0.5f) /
        static_cast<float>(kSides);
    const QVector3D midBase = (positions[baseRing[side]]
        + positions[baseRing[next]]) * 0.5f;
    const QVector3D slope = apex - midBase;
    const QVector3D tangent = positions[baseRing[next]]
        - positions[baseRing[side]];
    QVector3D sideNormal = QVector3D::crossProduct(tangent, slope);
    if (sideNormal.lengthSquared() <= 1.0e-10f) {
      sideNormal = QVector3D(midBase.x(), 0.0f, midBase.z()).normalized();
    } else {
      sideNormal.normalize();
    }
    const int apexIndex = appendVertex(apex, sideNormal, QVector2D(midU, 0.0f));
    impl_->mesh_.addPolygon({baseRing[side], apexIndex, baseRing[next]});
  }

  QVector<int> baseCap;
  baseCap.reserve(kSides);
  for (int side = 0; side < kSides; ++side) {
    const float u = static_cast<float>(side) / static_cast<float>(kSides);
    const float theta = static_cast<float>(M_PI * 2.0) * u;
    const float x = std::cos(theta) * kHalfWidth;
    const float z = std::sin(theta) * kHalfDepth;
    baseCap.push_back(appendVertex(
        QVector3D(x, -kHalfHeight, z), QVector3D(0.0f, -1.0f, 0.0f),
        QVector2D(kHalfWidth > 0.0f ? x / (kHalfWidth * 2.0f) + 0.5f : 0.5f,
                  kHalfDepth > 0.0f ? z / (kHalfDepth * 2.0f) + 0.5f : 0.5f)));
  }
  const int baseCenter = appendVertex(
      QVector3D(0.0f, -kHalfHeight, 0.0f), QVector3D(0.0f, -1.0f, 0.0f),
      QVector2D(0.5f, 0.5f));
  for (int side = 0; side < kSides; ++side) {
    const int next = (side + 1) % kSides;
    impl_->mesh_.addPolygon({baseCenter, baseCap[next], baseCap[side]});
  }

  impl_->mesh_.setVertexCount(positions.size());
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  positionAttr->data() = positions;
  normalAttr->data() = normals;
  uvAttr->data() = uvs;
}

void Artifact3DLayer::updateSourceSizeFromMesh() {
  impl_->mesh_.updateBounds();
  const QVector3D minB = impl_->mesh_.boundingBoxMin();
  const QVector3D maxB = impl_->mesh_.boundingBoxMax();
  const int width =
      std::max(1, static_cast<int>(std::ceil(std::abs(maxB.x() - minB.x()))));
  const int height =
      std::max(1, static_cast<int>(std::ceil(std::abs(maxB.y() - minB.y()))));
  setSourceSize(Size_2D(width, height));
}

ModelRenderMode Artifact3DLayer::renderMode() const { return impl_->renderMode_; }

void Artifact3DLayer::loadFromFileAtTime(const QString& filePath,
                                         const double time,
                                         const int clipIndex)
{
  const QString normalizedInput = filePath.trimmed();
  if (normalizedInput.isEmpty()) return;

  ArtifactCore::MeshImporter importer;
  auto mesh = importer.importMeshFromFileAtTime(
      UniString(normalizedInput), time, clipIndex);
  if (!mesh || mesh->vertexCount() <= 0) {
    qWarning() << "[Artifact3DLayer] Timed model evaluation failed:"
               << normalizedInput << importer.lastError();
    return;
  }
  impl_->fixedGeometry_ = FixedGeometry3D::Auto;
  const int clipCount = static_cast<int>(mesh->skinAnimationClips().size());
  impl_->skinAnimationClipIndex_ = clipCount > 0
      ? std::clamp(clipIndex, 0, clipCount - 1)
      : std::max(0, clipIndex);
  impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  impl_->mesh_ = *mesh;
  if (!impl_->mesh_.skinBones().isEmpty()) {
    impl_->mesh_.applyDeformers(impl_->mesh_.skinPoseMatrices());
  }
  for (int shapeIndex = 0;
       shapeIndex < impl_->mesh_.blendShapes().size(); ++shapeIndex) {
    const auto overrideIt = impl_->blendShapeWeightOverrides_.constFind(
        impl_->mesh_.blendShapes()[shapeIndex].name);
    if (overrideIt != impl_->blendShapeWeightOverrides_.constEnd()) {
      impl_->mesh_.setBlendShapeWeight(shapeIndex, overrideIt.value());
    }
  }
  centerMeshPositions(impl_->mesh_);
  impl_->meshLoaded_ = true;
  impl_->renderMode_ = ModelRenderMode::Solid;
  updateSourceSizeFromMesh();
  const QFileInfo sourceInfo(normalizedInput);
  const QString normalizedSourcePath = sourceInfo.canonicalFilePath().isEmpty()
      ? sourceInfo.absoluteFilePath()
      : sourceInfo.canonicalFilePath();
  impl_->sourcePath_ = normalizedSourcePath;
  setLayerName(sourceInfo.baseName());
  Q_EMIT changed();
}

void Artifact3DLayer::setAnimationTime(const double time, const int clipIndex)
{
  if (impl_->sourcePath_.isEmpty() || !impl_->meshLoaded_) {
    return;
  }
  loadFromFileAtTime(impl_->sourcePath_, time, clipIndex);
}

void Artifact3DLayer::setSkinAnimationEnabled(const bool enabled)
{
  impl_->skinAnimationEnabled_ = enabled;
  impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  if (!enabled && impl_->meshLoaded_ && !impl_->sourcePath_.isEmpty()) {
    if (const auto* clip = impl_->mesh_.skinAnimationClip(
            impl_->skinAnimationClipIndex_)) {
      loadFromFileAtTime(impl_->sourcePath_, clip->timeBegin,
                         impl_->skinAnimationClipIndex_);
    }
  }
  Q_EMIT changed();
}

bool Artifact3DLayer::skinAnimationEnabled() const
{
  return impl_->skinAnimationEnabled_;
}

void Artifact3DLayer::setSkinAnimationClipIndex(const int clipIndex)
{
  const int clipCount = static_cast<int>(
      impl_->mesh_.skinAnimationClips().size());
  impl_->skinAnimationClipIndex_ = clipCount > 0
      ? std::clamp(clipIndex, 0, clipCount - 1)
      : 0;
  impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  if (impl_->meshLoaded_ && !impl_->sourcePath_.isEmpty()) {
    if (const auto* clip = impl_->mesh_.skinAnimationClip(
            impl_->skinAnimationClipIndex_)) {
      loadFromFileAtTime(impl_->sourcePath_, clip->timeBegin,
                         impl_->skinAnimationClipIndex_);
    }
  }
  Q_EMIT changed();
}

int Artifact3DLayer::skinAnimationClipIndex() const
{
  return impl_->skinAnimationClipIndex_;
}

int Artifact3DLayer::skinAnimationClipCount() const
{
  return static_cast<int>(impl_->mesh_.skinAnimationClips().size());
}

QString Artifact3DLayer::skinAnimationClipName(const int clipIndex) const
{
  const auto* clip = impl_->mesh_.skinAnimationClip(clipIndex);
  return clip ? clip->name : QString();
}

int Artifact3DLayer::blendShapeCount() const
{
  return static_cast<int>(impl_->mesh_.blendShapes().size());
}

QString Artifact3DLayer::blendShapeName(const int shapeIndex) const
{
  const auto& shapes = impl_->mesh_.blendShapes();
  return shapeIndex >= 0 && shapeIndex < shapes.size()
      ? shapes[shapeIndex].name : QString();
}

float Artifact3DLayer::blendShapeWeight(const int shapeIndex) const
{
  return impl_->mesh_.blendShapeWeight(shapeIndex);
}

void Artifact3DLayer::setBlendShapeWeight(const int shapeIndex,
                                          const float weight)
{
  if (shapeIndex < 0 || shapeIndex >= impl_->mesh_.blendShapes().size() ||
      !std::isfinite(weight)) {
    return;
  }
  impl_->blendShapeWeightOverrides_.insert(
      impl_->mesh_.blendShapes()[shapeIndex].name, weight);
  impl_->mesh_.setBlendShapeWeight(shapeIndex, weight);
  updateSourceSizeFromMesh();
  Q_EMIT changed();
}

void Artifact3DLayer::clearBlendShapeWeightOverride(const int shapeIndex)
{
  if (shapeIndex < 0 || shapeIndex >= impl_->mesh_.blendShapes().size()) {
    return;
  }
  impl_->blendShapeWeightOverrides_.remove(
      impl_->mesh_.blendShapes()[shapeIndex].name);
  if (!impl_->sourcePath_.isEmpty()) {
    const double fps = std::isfinite(compositionFrameRate()) &&
                               compositionFrameRate() > 0.0
        ? compositionFrameRate()
        : 30.0;
    loadFromFileAtTime(impl_->sourcePath_,
                       static_cast<double>(currentFrame()) / fps,
                       impl_->skinAnimationClipIndex_);
    return;
  }
  Q_EMIT changed();
}

void Artifact3DLayer::setRenderMode(ModelRenderMode mode) {
  const int raw = static_cast<int>(mode);
  if (raw < static_cast<int>(ModelRenderMode::Wireframe) ||
      raw > static_cast<int>(ModelRenderMode::Solid) ||
      impl_->renderMode_ == mode) {
    return;
  }
  impl_->renderMode_ = mode;
  Q_EMIT changed();
}

const ArtifactCore::Mesh& Artifact3DLayer::mesh() const
{
  return impl_->mesh_;
}

void Artifact3DLayer::setSkinPoseMatrices(
    const QVector<QMatrix4x4>& boneMatrices)
{
  if (boneMatrices.isEmpty() || impl_->mesh_.skinBones().isEmpty()) {
    return;
  }
  impl_->mesh_.applyDeformers(boneMatrices);
  impl_->mesh_.updateBounds();
  updateSourceSizeFromMesh();
  Q_EMIT changed();
}

void Artifact3DLayer::draw(ArtifactIRenderer *renderer) {
  const int64_t frame = currentFrame();
  static const bool traceEnabled =
      !qEnvironmentVariableIsSet("ARTIFACT_DISABLE_3D_RENDER_TRACE");
  const auto traceResult = [&](const QString &outcome,
                               const QString &detail = {}) {
    if (!traceEnabled || impl_->lastRenderTraceOutcome_ == outcome) {
      return;
    }
    impl_->lastRenderTraceOutcome_ = outcome;
    qInfo().noquote()
        << QStringLiteral(
               "[Artifact3DLayer][RenderTrace] frame=%1 layer=\"%2\" id=%3 "
               "outcome=%4 mode=%5 vertices=%6 polygons=%7 opacity=%8 %9")
               .arg(frame)
               .arg(layerName())
               .arg(id().toString())
               .arg(outcome)
               .arg(impl_->renderMode_ == ModelRenderMode::Solid
                        ? QStringLiteral("solid")
                        : QStringLiteral("wireframe"))
               .arg(impl_->mesh_.vertexCount())
               .arg(impl_->mesh_.polygonCount())
               .arg(opacity(), 0, 'f', 3)
               .arg(detail);
  };

  if (!renderer) {
    traceResult(QStringLiteral("skip:no-renderer"));
    return;
  }
  if (!isVisible()) {
    traceResult(QStringLiteral("skip:not-visible"));
    return;
  }
  if (!impl_->meshLoaded_) {
    traceResult(QStringLiteral("skip:mesh-not-loaded"));
    return;
  }

  const auto &t3 = transform3D();
  const auto size = sourceSize();
  const double compositionFps = std::isfinite(compositionFrameRate()) &&
                                compositionFrameRate() > 0.0
      ? compositionFrameRate()
      : 30.0;
  if (!impl_->sourcePath_.isEmpty() &&
      !impl_->mesh_.skinAnimationClips().isEmpty() &&
      impl_->skinAnimationEnabled_ &&
      impl_->lastSkinAnimationFrame_ != frame &&
      !impl_->updatingSkinAnimation_) {
    const int clipCount = static_cast<int>(
        impl_->mesh_.skinAnimationClips().size());
    impl_->skinAnimationClipIndex_ = std::clamp(
        impl_->skinAnimationClipIndex_, 0, clipCount - 1);
    impl_->updatingSkinAnimation_ = true;
    const auto* clip = impl_->mesh_.skinAnimationClip(
        impl_->skinAnimationClipIndex_);
    if (clip && std::isfinite(clip->timeBegin) &&
        std::isfinite(clip->timeEnd) && clip->timeEnd > clip->timeBegin) {
      const double duration = clip->timeEnd - clip->timeBegin;
      const double requestedTime = static_cast<double>(frame) / compositionFps;
      const double relativeTime = std::isfinite(requestedTime)
          ? std::fmod(std::max(0.0, requestedTime - clip->timeBegin),
                      duration)
          : 0.0;
      setAnimationTime(clip->timeBegin + relativeTime,
                       impl_->skinAnimationClipIndex_);
    }
    impl_->updatingSkinAnimation_ = false;
    impl_->lastSkinAnimationFrame_ = frame;
  }
  const RationalTime frameTime(currentFrame(), compositionFps);
  const auto snapshot = t3.snapshotAt(frameTime);
  const RationalTime previousFrameTime(
      std::max<int64_t>(0, currentFrame() - 1), compositionFps);
  const auto previousSnapshot = t3.snapshotAt(previousFrameTime);
  QMatrix4x4 modelMatrix;
  modelMatrix.setToIdentity();
  modelMatrix.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  modelMatrix.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  modelMatrix.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  modelMatrix.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);
  QMatrix4x4 previousModelMatrix;
  previousModelMatrix.setToIdentity();
  previousModelMatrix.translate(previousSnapshot.positionX, previousSnapshot.positionY, previousSnapshot.positionZ);
  previousModelMatrix.rotate(previousSnapshot.rotationX, 1.0f, 0.0f, 0.0f);
  previousModelMatrix.rotate(previousSnapshot.rotationY, 0.0f, 1.0f, 0.0f);
  previousModelMatrix.rotate(previousSnapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  previousModelMatrix.scale(previousSnapshot.scaleX, previousSnapshot.scaleY,
                           previousSnapshot.scaleZ);
  previousModelMatrix.translate(-previousSnapshot.anchorX, -previousSnapshot.anchorY, -previousSnapshot.anchorZ);

  // Get mesh data
  const auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  const auto positions = vertexAttrs.get<QVector3D>("position");
  if (!positions || positions->data().isEmpty()) {
    traceResult(QStringLiteral("skip:no-position-data"));
    return;
  }

  // Transform vertices
  QVector<QVector3D> transformedVertices;
  transformedVertices.resize(positions->data().size());
  ArtifactCore::Parallel::For(0, static_cast<int>(positions->data().size()), static_cast<int>(positions->data().size()), [&](int index) {
    transformedVertices[index] = modelMatrix.map(positions->data()[index]);
  });

  const FloatColor wireframeColor{1.0f, 1.0f, 1.0f, opacity()};
  const float thickness = 2.0f;
  const auto drawEdges = [&](const FloatColor &color, float lineThickness) {
    for (int i = 0; i < impl_->mesh_.polygonCount(); ++i) {
      const auto vertexIndices = impl_->mesh_.getPolygonVertices(i);
      for (size_t j = 0; j < vertexIndices.size(); ++j) {
        const QVector3D &v0 = transformedVertices[vertexIndices[j]];
        const QVector3D &v1 =
            transformedVertices[vertexIndices[(j + 1) % vertexIndices.size()]];
        renderer->draw3DLine(toFloat3(v0), toFloat3(v1), color, lineThickness);
      }
    }
  };
  // Minimal point-cloud fallback: vertex-only PLY has no polygons, so the
  // indexed mesh path submits nothing. Draw a capped set of 3-axis crosses
  // in world space through the same draw3DLine path as wireframe.
  const auto drawPoints = [&]() {
    const int pointCount = static_cast<int>(transformedVertices.size());
    if (pointCount <= 0) {
      return 0;
    }
    const auto colorAttr = vertexAttrs.get<QVector4D>("color");
    const bool hasColor = colorAttr && colorAttr->data().size() == pointCount;
    const QVector3D diagVec = impl_->mesh_.boundingBoxMax() - impl_->mesh_.boundingBoxMin();
    const float avgScale = (snapshot.scaleX + snapshot.scaleY + snapshot.scaleZ) / 3.0f;
    const float crossHalf = std::clamp(diagVec.length() * avgScale * 0.004f, 0.75f, 14.0f) *
        std::clamp(impl_->pointSize_, 0.25f, 8.0f);
    const int kMaxPointDraws = 32768;
    const int stride = std::max(1, (pointCount + kMaxPointDraws - 1) / kMaxPointDraws);
    int drawn = 0;
    const float baseAlpha = static_cast<float>(opacity());
    for (int i = 0; i < pointCount; i += stride) {
      const QVector3D &p = transformedVertices[i];
      FloatColor c{1.0f, 1.0f, 1.0f, baseAlpha};
      if (hasColor) {
        const QVector4D &vc = colorAttr->data()[i];
        c = FloatColor{vc.x(), vc.y(), vc.z(), baseAlpha * vc.w()};
      }
      const QVector3D dx(crossHalf, 0.0f, 0.0f);
      const QVector3D dy(0.0f, crossHalf, 0.0f);
      const QVector3D dz(0.0f, 0.0f, crossHalf);
      renderer->draw3DLine(toFloat3(p - dx), toFloat3(p + dx), c, 1.5f);
      renderer->draw3DLine(toFloat3(p - dy), toFloat3(p + dy), c, 1.5f);
      renderer->draw3DLine(toFloat3(p - dz), toFloat3(p + dz), c, 1.5f);
      ++drawn;
    }
    return drawn;
  };
  const bool isPointCloud = impl_->mesh_.polygonCount() == 0 && !transformedVertices.isEmpty();
  const auto drawFaceNormals = [&]() {
    const FloatColor normalColor{1.0f, 0.35f, 0.08f, opacity() * 0.9f};
    const auto normalMatrix = modelMatrix.normalMatrix();
    for (int polygon = 0; polygon < impl_->mesh_.polygonCount(); ++polygon) {
      const auto vertexIndices = impl_->mesh_.getPolygonVertices(polygon);
      if (vertexIndices.size() < 3) {
        continue;
      }

      QVector3D center;
      QVector3D localNormal;
      float edgeLengthSum = 0.0f;
      for (size_t i = 0; i < vertexIndices.size(); ++i) {
        const QVector3D &current = positions->data()[vertexIndices[i]];
        const QVector3D &next =
            positions->data()[vertexIndices[(i + 1) % vertexIndices.size()]];
        center += current;
        edgeLengthSum += (next - current).length();
        localNormal += QVector3D(
            (current.y() - next.y()) * (current.z() + next.z()),
            (current.z() - next.z()) * (current.x() + next.x()),
            (current.x() - next.x()) * (current.y() + next.y()));
      }
      if (localNormal.lengthSquared() <= 1.0e-10f) {
        continue;
      }
      center /= static_cast<float>(vertexIndices.size());
      const QVector3D normal = QVector3D(
          normalMatrix(0, 0) * localNormal.x() +
              normalMatrix(0, 1) * localNormal.y() +
              normalMatrix(0, 2) * localNormal.z(),
          normalMatrix(1, 0) * localNormal.x() +
              normalMatrix(1, 1) * localNormal.y() +
              normalMatrix(1, 2) * localNormal.z(),
          normalMatrix(2, 0) * localNormal.x() +
          normalMatrix(2, 1) * localNormal.y() +
              normalMatrix(2, 2) * localNormal.z())
          .normalized();
      const float averageEdgeLength =
          edgeLengthSum / static_cast<float>(vertexIndices.size());
      const float faceScale = impl_->normalLength_ > 0.0f
                                  ? impl_->normalLength_
                                  : std::max(1.0f, averageEdgeLength * 0.25f);
      const QVector3D worldCenter = modelMatrix.map(center);
      const QVector3D worldEnd = worldCenter + normal * faceScale;
      renderer->draw3DLine(toFloat3(worldCenter), toFloat3(worldEnd),
                           normalColor, 1.5f);
    }
  };
  const auto drawVertexNormals = [&]() {
    const auto normals = vertexAttrs.get<QVector3D>("normal");
    if (!normals || normals->data().size() != positions->data().size()) {
      return;
    }
    const FloatColor normalColor{1.0f, 0.72f, 0.08f, opacity() * 0.82f};
    const auto normalMatrix = modelMatrix.normalMatrix();
    for (int index = 0; index < positions->data().size(); ++index) {
      const QVector3D localNormal = normals->data()[index];
      if (localNormal.lengthSquared() <= 1.0e-10f) {
        continue;
      }
      const QVector3D worldStart = transformedVertices[index];
      const QVector3D worldNormal = QVector3D(
          normalMatrix(0, 0) * localNormal.x() +
              normalMatrix(0, 1) * localNormal.y() +
              normalMatrix(0, 2) * localNormal.z(),
          normalMatrix(1, 0) * localNormal.x() +
              normalMatrix(1, 1) * localNormal.y() +
              normalMatrix(1, 2) * localNormal.z(),
          normalMatrix(2, 0) * localNormal.x() +
          normalMatrix(2, 1) * localNormal.y() +
              normalMatrix(2, 2) * localNormal.z())
          .normalized();
      const QVector3D worldEnd = worldStart + worldNormal * impl_->normalLength_;
      renderer->draw3DLine(toFloat3(worldStart), toFloat3(worldEnd),
                           normalColor, 1.25f);
    }
  };

  if (impl_->renderMode_ == ModelRenderMode::Solid) {
    if (isPointCloud) {
      const int drawnPoints = drawPoints();
      traceResult(
          QStringLiteral("points-submitted"),
          QStringLiteral("points=%1 drawn=%2")
              .arg(transformedVertices.size())
              .arg(drawnPoints));
    } else {
    QString cacheKey = QStringLiteral("%1|layer=%2")
        .arg(sourcePath().isEmpty() ? id().toString() : sourcePath(),
             id().toString());
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      cacheKey += QStringLiteral("|fixed=%1|w=%2|h=%3|d=%4|seg=%5|ring=%6")
          .arg(static_cast<int>(impl_->fixedGeometry_))
          .arg(impl_->geometryWidth_, 0, 'g', 9)
          .arg(impl_->geometryHeight_, 0, 'g', 9)
          .arg(impl_->geometryDepth_, 0, 'g', 9)
          .arg(impl_->geometrySegments_)
          .arg(impl_->geometryRings_);
    }
    const int solidShadingMode = impl_->useTextureInSolid_ ? 3 : 8;
    renderer->drawMesh(cacheKey, impl_->mesh_, impl_->material_, modelMatrix,
                       opacity(), solidShadingMode, &previousModelMatrix);
    traceResult(
        QStringLiteral("mesh-submitted"),
        QStringLiteral("position=(%1,%2,%3) scale=(%4,%5) shading=%6")
            .arg(snapshot.positionX, 0, 'f', 2)
            .arg(snapshot.positionY, 0, 'f', 2)
            .arg(snapshot.positionZ, 0, 'f', 2)
            .arg(snapshot.scaleX, 0, 'f', 3)
            .arg(snapshot.scaleY, 0, 'f', 3)
            .arg(solidShadingMode));
    if (impl_->wireOverlay_) {
      drawEdges(FloatColor{0.04f, 0.05f, 0.06f, opacity() * 0.72f}, 1.0f);
      if (impl_->faceNormals_) {
        drawFaceNormals();
      }
      if (impl_->vertexNormals_) {
        drawVertexNormals();
      }
    }
    }
  } else {
    if (isPointCloud) {
      const int drawnPoints = drawPoints();
      traceResult(
          QStringLiteral("points-submitted"),
          QStringLiteral("points=%1 drawn=%2")
              .arg(transformedVertices.size())
              .arg(drawnPoints));
    } else {
      drawEdges(wireframeColor, thickness);
      traceResult(QStringLiteral("wireframe-submitted"));
    }
  }

  drawFractureOverlay(renderer, modelMatrix,
                      QSizeF(size.width, size.height), opacity());
}

void Artifact3DLayer::drawLOD(ArtifactIRenderer *renderer, DetailLevel lod) {
  // For now, same as regular draw
  draw(renderer);
}

void Artifact3DLayer::drawSelectionOutline(ArtifactIRenderer *renderer) const {
  if (!renderer || !impl_->meshLoaded_) {
    return;
  }
  const auto positions = impl_->mesh_.vertexAttributes().get<QVector3D>("position");
  if (!positions || positions->data().isEmpty()) {
    return;
  }
  const double fps = compositionFrameRate();
  const RationalTime frameTime(
      currentFrame(), std::isfinite(fps) && fps > 0.0 ? fps : 30.0);
  const auto snapshot = transform3D().snapshotAt(frameTime);
  QMatrix4x4 modelMatrix;
  modelMatrix.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  modelMatrix.rotate(snapshot.rotationX, 1.0f, 0.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationY, 0.0f, 1.0f, 0.0f);
  modelMatrix.rotate(snapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  modelMatrix.scale(snapshot.scaleX, snapshot.scaleY, snapshot.scaleZ);
  modelMatrix.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);
  const FloatColor outlineColor{0.30f, 0.86f, 1.0f, 0.98f};
  if (impl_->mesh_.polygonCount() == 0) {
    // Point cloud: no edges to trace, outline the world-space bounds box.
    const QVector3D minB = impl_->mesh_.boundingBoxMin();
    const QVector3D maxB = impl_->mesh_.boundingBoxMax();
    const QVector3D corners[] = {
        {minB.x(), minB.y(), minB.z()}, {maxB.x(), minB.y(), minB.z()},
        {maxB.x(), maxB.y(), minB.z()}, {minB.x(), maxB.y(), minB.z()},
        {minB.x(), minB.y(), maxB.z()}, {maxB.x(), minB.y(), maxB.z()},
        {maxB.x(), maxB.y(), maxB.z()}, {minB.x(), maxB.y(), maxB.z()},
    };
    const int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &edge : edges) {
      const QVector3D worldA = modelMatrix.map(corners[edge[0]]);
      const QVector3D worldB = modelMatrix.map(corners[edge[1]]);
      renderer->draw3DLine({worldA.x(), worldA.y(), worldA.z()},
                           {worldB.x(), worldB.y(), worldB.z()},
                           outlineColor, 1.8f);
    }
    return;
  }
  for (int polygon = 0; polygon < impl_->mesh_.polygonCount(); ++polygon) {
    const auto indices = impl_->mesh_.getPolygonVertices(polygon);
    for (size_t i = 0; i < indices.size(); ++i) {
      const auto &a = positions->data()[indices[i]];
      const auto &b = positions->data()[indices[(i + 1) % indices.size()]];
      const QVector3D worldA = modelMatrix.map(a);
      const QVector3D worldB = modelMatrix.map(b);
      renderer->draw3DLine({worldA.x(), worldA.y(), worldA.z()},
                            {worldB.x(), worldB.y(), worldB.z()},
                            outlineColor, 1.8f);
    }
  }
}

QRectF Artifact3DLayer::localBounds() const
{
  const auto size = sourceSize();
  if (size.width <= 0 || size.height <= 0) {
    return QRectF();
  }

  const qreal halfW = static_cast<qreal>(size.width) * 0.5;
  const qreal halfH = static_cast<qreal>(size.height) * 0.5;
  return QRectF(-halfW, -halfH,
                static_cast<qreal>(size.width),
                static_cast<qreal>(size.height));
}

std::vector<ArtifactCore::PropertyGroup>
Artifact3DLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

  using namespace ArtifactCore;
  PropertyGroup geometryGroup(QStringLiteral("Geometry"));

  auto geometryTypeProp = persistentLayerProperty(
      QStringLiteral("geometry.type"), PropertyType::Integer,
      static_cast<int>(fixedGeometry()), -60);
  geometryTypeProp->setDisplayLabel(QStringLiteral("Primitive Type"));
  geometryTypeProp->setTooltip(
      fixedGeometry() == FixedGeometry3D::Auto
          ? QStringLiteral("0=Imported Model, 1=Plane, 2=Box, 3=Sphere, 4=Cylinder, 5=Cone, 6=Torus, 7=Capsule, 8=Pyramid")
          : QStringLiteral("1=Plane, 2=Box, 3=Sphere, 4=Cylinder, 5=Cone, 6=Torus, 7=Capsule, 8=Pyramid"));
  geometryGroup.addProperty(geometryTypeProp);

  const FixedGeometry3D geometry = fixedGeometry();
  if (geometry != FixedGeometry3D::Auto) {
    auto geometryWidthProp = persistentLayerProperty(
        QStringLiteral("geometry.width"), PropertyType::Float,
        impl_->geometryWidth_, -59);
    geometryWidthProp->setDisplayLabel(QStringLiteral("Width"));
    geometryWidthProp->setSoftRange(1.0, 2000.0);
    geometryGroup.addProperty(geometryWidthProp);

    auto geometryHeightProp = persistentLayerProperty(
        QStringLiteral("geometry.height"), PropertyType::Float,
        impl_->geometryHeight_, -58);
    geometryHeightProp->setDisplayLabel(QStringLiteral("Height"));
    geometryHeightProp->setSoftRange(1.0, 2000.0);
    geometryGroup.addProperty(geometryHeightProp);

    if (geometry != FixedGeometry3D::Plane) {
      auto geometryDepthProp = persistentLayerProperty(
          QStringLiteral("geometry.depth"), PropertyType::Float,
          impl_->geometryDepth_, -57);
      geometryDepthProp->setDisplayLabel(QStringLiteral("Depth"));
      geometryDepthProp->setSoftRange(1.0, 2000.0);
      geometryGroup.addProperty(geometryDepthProp);
    }

    if (geometry == FixedGeometry3D::Sphere ||
        geometry == FixedGeometry3D::Cylinder ||
        geometry == FixedGeometry3D::Cone ||
        geometry == FixedGeometry3D::Torus ||
        geometry == FixedGeometry3D::Capsule ||
        geometry == FixedGeometry3D::Pyramid) {
      auto geometrySegmentsProp = persistentLayerProperty(
          QStringLiteral("geometry.segments"), PropertyType::Integer,
          impl_->geometrySegments_, -56);
      geometrySegmentsProp->setDisplayLabel(QStringLiteral("Segments"));
      geometrySegmentsProp->setSoftRange(3, 64);
      geometryGroup.addProperty(geometrySegmentsProp);
    }

    if (geometry == FixedGeometry3D::Sphere ||
        geometry == FixedGeometry3D::Torus) {
      auto geometryRingsProp = persistentLayerProperty(
          QStringLiteral("geometry.rings"), PropertyType::Integer,
          impl_->geometryRings_, -55);
      geometryRingsProp->setDisplayLabel(QStringLiteral("Rings"));
      geometryRingsProp->setSoftRange(2, 64);
      geometryGroup.addProperty(geometryRingsProp);
    }
  }

  PropertyGroup renderGroup(QStringLiteral("3D Render"));

  auto renderModeProp = persistentLayerProperty(
      QStringLiteral("render.mode"), PropertyType::Integer,
      static_cast<int>(renderMode()), -50);
  renderModeProp->setDisplayLabel(QStringLiteral("Render Mode"));
  renderModeProp->setTooltip(QStringLiteral("0=Wireframe, 1=Solid"));
  renderGroup.addProperty(renderModeProp);

  if (fixedGeometry() == FixedGeometry3D::Auto) {
    auto sourcePathProp = persistentLayerProperty(
        QStringLiteral("model.sourcePath"), PropertyType::String,
        sourcePath(), -55);
    sourcePathProp->setDisplayLabel(QStringLiteral("Source Path"));
    sourcePathProp->setTooltip(QStringLiteral("3D model source file path"));
    renderGroup.addProperty(sourcePathProp);

    auto skinAnimationEnabledProp = persistentLayerProperty(
        QStringLiteral("animation.enabled"), PropertyType::Boolean,
        impl_->skinAnimationEnabled_, -54);
    skinAnimationEnabledProp->setDisplayLabel(QStringLiteral("Skin Animation"));
    skinAnimationEnabledProp->setTooltip(
        QStringLiteral("Evaluate imported FBX/glTF skin animation during composition playback"));
    renderGroup.addProperty(skinAnimationEnabledProp);

    auto skinAnimationClipProp = persistentLayerProperty(
        QStringLiteral("animation.clipIndex"), PropertyType::Integer,
        impl_->skinAnimationClipIndex_, -53);
    skinAnimationClipProp->setDisplayLabel(QStringLiteral("Animation Clip"));
    skinAnimationClipProp->setTooltip(
        QStringLiteral("Imported animation clip index"));
    skinAnimationClipProp->setHardRange(0, 9999);
    renderGroup.addProperty(skinAnimationClipProp);

    if (!impl_->mesh_.blendShapes().isEmpty()) {
      PropertyGroup morphGroup(QStringLiteral("Morphs"));
      for (int shapeIndex = 0;
           shapeIndex < impl_->mesh_.blendShapes().size(); ++shapeIndex) {
        const auto& shape = impl_->mesh_.blendShapes()[shapeIndex];
        const QString propertyPath = QStringLiteral(
            "deformers.blendShapes.%1.weight").arg(shapeIndex);
        auto weightProp = persistentLayerProperty(
            propertyPath, PropertyType::Float, shape.weight,
            -52 - shapeIndex);
        weightProp->setDisplayLabel(
            shape.name.isEmpty()
                ? QStringLiteral("Shape %1").arg(shapeIndex + 1)
                : shape.name);
        weightProp->setTooltip(QStringLiteral("Blend Shape weight"));
        weightProp->setSoftRange(0.0, 1.0);
        morphGroup.addProperty(weightProp);
      }
      groups.push_back(morphGroup);
    }
  }

  auto affectedByLightsProp = persistentLayerProperty(
      QStringLiteral("render.affectedByLights"), PropertyType::Boolean,
      affectedByLights(), -54);
  affectedByLightsProp->setDisplayLabel(QStringLiteral("Affected By Lights"));
  affectedByLightsProp->setTooltip(QStringLiteral("Disable to ignore all scene lights for this 3D layer"));
  renderGroup.addProperty(affectedByLightsProp);

  auto useTextureInSolidProp = persistentLayerProperty(
      QStringLiteral("render.useTextureInSolid"), PropertyType::Boolean,
      impl_->useTextureInSolid_, -53);
  useTextureInSolidProp->setDisplayLabel(QStringLiteral("Use Texture in Solid"));
  useTextureInSolidProp->setTooltip(
      QStringLiteral("Use the base-color texture instead of Blender-style material color"));
  renderGroup.addProperty(useTextureInSolidProp);

  auto wireOverlayProp = persistentLayerProperty(
      QStringLiteral("render.wireOverlay"), PropertyType::Boolean,
      impl_->wireOverlay_, -52);
  wireOverlayProp->setDisplayLabel(QStringLiteral("Wire Overlay"));
  wireOverlayProp->setTooltip(QStringLiteral("Draw mesh edges over the solid viewport"));
  renderGroup.addProperty(wireOverlayProp);

  auto faceNormalsProp = persistentLayerProperty(
      QStringLiteral("render.faceNormals"), PropertyType::Boolean,
      impl_->faceNormals_, -51);
  faceNormalsProp->setDisplayLabel(QStringLiteral("Face Normal"));
  faceNormalsProp->setTooltip(
      QStringLiteral("Draw one normal line from the center of each face"));
  renderGroup.addProperty(faceNormalsProp);

  auto vertexNormalsProp = persistentLayerProperty(
      QStringLiteral("render.vertexNormals"), PropertyType::Boolean,
      impl_->vertexNormals_, -50);
  vertexNormalsProp->setDisplayLabel(QStringLiteral("Vertex Normal"));
  vertexNormalsProp->setTooltip(
      QStringLiteral("Draw one normal line from each vertex"));
  renderGroup.addProperty(vertexNormalsProp);

  auto normalLengthProp = persistentLayerProperty(
      QStringLiteral("render.normalLength"), PropertyType::Float,
      impl_->normalLength_, -49);
  normalLengthProp->setDisplayLabel(QStringLiteral("Normal Length"));
  normalLengthProp->setTooltip(QStringLiteral("Length of displayed normal lines"));
  normalLengthProp->setHardRange(0.01, 10000.0);
  normalLengthProp->setSoftRange(1.0, 200.0);
  renderGroup.addProperty(normalLengthProp);

  auto pointSizeProp = persistentLayerProperty(
      QStringLiteral("render.pointSize"), PropertyType::Float,
      impl_->pointSize_, -48);
  pointSizeProp->setDisplayLabel(QStringLiteral("Point Size"));
  pointSizeProp->setTooltip(QStringLiteral("Point cross size multiplier (point clouds only)"));
  pointSizeProp->setHardRange(0.25, 8.0);
  pointSizeProp->setSoftRange(0.5, 4.0);
  renderGroup.addProperty(pointSizeProp);

  PropertyGroup materialGroup(QStringLiteral("Material"));

  auto baseColorProp = persistentLayerProperty(
      QStringLiteral("material.base.color"), PropertyType::Color,
      impl_->material_.baseColor(), -50);
  baseColorProp->setDisplayLabel(QStringLiteral("Base Color"));
  materialGroup.addProperty(baseColorProp);

  auto metallicProp = persistentLayerProperty(
      QStringLiteral("material.metallic"), PropertyType::Float,
      impl_->material_.metallic(), -49);
  metallicProp->setDisplayLabel(QStringLiteral("Metallic"));
  metallicProp->setHardRange(0.0, 1.0);
  metallicProp->setSoftRange(0.0, 1.0);
  materialGroup.addProperty(metallicProp);

  auto roughnessProp = persistentLayerProperty(
      QStringLiteral("material.roughness"), PropertyType::Float,
      impl_->material_.roughness(), -48);
  roughnessProp->setDisplayLabel(QStringLiteral("Roughness"));
  roughnessProp->setHardRange(0.0, 1.0);
  roughnessProp->setSoftRange(0.0, 1.0);
  materialGroup.addProperty(roughnessProp);

  auto specularProp = persistentLayerProperty(
      QStringLiteral("material.specular"), PropertyType::Float,
      impl_->material_.specular(), -46);
  specularProp->setDisplayLabel(QStringLiteral("Specular"));
  specularProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(specularProp);

  auto iorProp = persistentLayerProperty(
      QStringLiteral("material.ior"), PropertyType::Float,
      impl_->material_.ior(), -45);
  iorProp->setDisplayLabel(QStringLiteral("IOR"));
  iorProp->setHardRange(1.0, 3.0);
  materialGroup.addProperty(iorProp);

  auto transmissionProp = persistentLayerProperty(
      QStringLiteral("material.transmission"), PropertyType::Float,
      impl_->material_.transmission(), -44);
  transmissionProp->setDisplayLabel(QStringLiteral("Transmission"));
  transmissionProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(transmissionProp);

  auto clearcoatProp = persistentLayerProperty(
      QStringLiteral("material.clearcoat"), PropertyType::Float,
      impl_->material_.clearcoat(), -43);
  clearcoatProp->setDisplayLabel(QStringLiteral("Clearcoat"));
  clearcoatProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(clearcoatProp);

  auto clearcoatRoughnessProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatRoughness"), PropertyType::Float,
      impl_->material_.clearcoatRoughness(), -42);
  clearcoatRoughnessProp->setDisplayLabel(QStringLiteral("Clearcoat Roughness"));
  clearcoatRoughnessProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(clearcoatRoughnessProp);

  auto sheenProp = persistentLayerProperty(
      QStringLiteral("material.sheen"), PropertyType::Float,
      impl_->material_.sheen(), -41);
  sheenProp->setDisplayLabel(QStringLiteral("Sheen"));
  sheenProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(sheenProp);

  auto opacityProp = persistentLayerProperty(
      QStringLiteral("material.opacity"), PropertyType::Float,
      impl_->material_.opacity(), -47);
  opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
  opacityProp->setTooltip(QStringLiteral("Material opacity (0=transparent, 1=opaque)"));
  opacityProp->setHardRange(0.0, 1.0);
  opacityProp->setSoftRange(0.0, 1.0);
  materialGroup.addProperty(opacityProp);

  auto alphaModeProp = persistentLayerProperty(
      QStringLiteral("material.alphaMode"), PropertyType::Integer,
      static_cast<int>(impl_->material_.alphaMode()), -46);
  alphaModeProp->setDisplayLabel(QStringLiteral("Alpha Mode"));
  alphaModeProp->setTooltip(QStringLiteral("0=Opaque, 1=Masked, 2=Blended"));
  alphaModeProp->setHardRange(0, 2);
  materialGroup.addProperty(alphaModeProp);

  auto alphaCutoffProp = persistentLayerProperty(
      QStringLiteral("material.alphaCutoff"), PropertyType::Float,
      impl_->material_.alphaCutoff(), -45);
  alphaCutoffProp->setDisplayLabel(QStringLiteral("Alpha Cutoff"));
  alphaCutoffProp->setHardRange(0.0, 1.0);
  alphaCutoffProp->setSoftRange(0.0, 1.0);
  materialGroup.addProperty(alphaCutoffProp);

  auto baseColorTextureProp = persistentLayerProperty(
      QStringLiteral("material.baseColorTexture"), PropertyType::String,
      impl_->material_.baseColorTexture().toQString(), -40);
  baseColorTextureProp->setDisplayLabel(QStringLiteral("Base Color Texture"));
  baseColorTextureProp->setTooltip(QStringLiteral("Texture path for the base color"));
  materialGroup.addProperty(baseColorTextureProp);

  auto metallicRoughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessTexture"), PropertyType::String,
      impl_->material_.metallicRoughnessTexture().toQString(), -39);
  metallicRoughnessTextureProp->setDisplayLabel(QStringLiteral("Metallic Roughness Texture"));
  materialGroup.addProperty(metallicRoughnessTextureProp);

  auto normalTextureProp = persistentLayerProperty(
      QStringLiteral("material.normalTexture"), PropertyType::String,
      impl_->material_.normalTexture().toQString(), -38);
  normalTextureProp->setDisplayLabel(QStringLiteral("Normal Texture"));
  materialGroup.addProperty(normalTextureProp);

  auto emissionTextureProp = persistentLayerProperty(
      QStringLiteral("material.emissionTexture"), PropertyType::String,
      impl_->material_.emissionTexture().toQString(), -37);
  emissionTextureProp->setDisplayLabel(QStringLiteral("Emission Texture"));
  materialGroup.addProperty(emissionTextureProp);

  auto occlusionTextureProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTexture"), PropertyType::String,
      impl_->material_.occlusionTexture().toQString(), -36);
  occlusionTextureProp->setDisplayLabel(QStringLiteral("Occlusion Texture"));
  materialGroup.addProperty(occlusionTextureProp);

  auto opacityTextureProp = persistentLayerProperty(
      QStringLiteral("material.opacityTexture"), PropertyType::String,
      impl_->material_.opacityTexture().toQString(), -35);
  opacityTextureProp->setDisplayLabel(QStringLiteral("Opacity Texture"));
  materialGroup.addProperty(opacityTextureProp);

  auto emissionColorProp = persistentLayerProperty(
      QStringLiteral("material.emission.color"), PropertyType::Color,
      impl_->material_.emissionColor(), -34);
  emissionColorProp->setDisplayLabel(QStringLiteral("Emission Color"));
  materialGroup.addProperty(emissionColorProp);

  auto emissionStrengthProp = persistentLayerProperty(
      QStringLiteral("material.emissionStrength"), PropertyType::Float,
      impl_->material_.emissionStrength(), -33);
  emissionStrengthProp->setDisplayLabel(QStringLiteral("Emission Strength"));
  emissionStrengthProp->setTooltip(QStringLiteral("Emission intensity multiplier"));
  materialGroup.addProperty(emissionStrengthProp);

  auto normalStrengthProp = persistentLayerProperty(
      QStringLiteral("material.normalStrength"), PropertyType::Float,
      impl_->material_.normalStrength(), -30);
  normalStrengthProp->setDisplayLabel(QStringLiteral("Normal Strength"));
  normalStrengthProp->setTooltip(QStringLiteral("Normal map intensity"));
  normalStrengthProp->setHardRange(0.0, 10.0);
  materialGroup.addProperty(normalStrengthProp);

  auto occlusionStrengthProp = persistentLayerProperty(
      QStringLiteral("material.occlusionStrength"), PropertyType::Float,
      impl_->material_.occlusionStrength(), -29);
  occlusionStrengthProp->setDisplayLabel(QStringLiteral("Occlusion Strength"));
  occlusionStrengthProp->setTooltip(QStringLiteral("Ambient occlusion intensity"));
  occlusionStrengthProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(occlusionStrengthProp);

  // MaterialX summary
  if (impl_->material_.materialXDocument().length() > 0) {
    auto materialXProp = persistentLayerProperty(
        QStringLiteral("material.materialx.summary"), PropertyType::String,
        QStringLiteral("MaterialX document present"), -37);
    materialXProp->setDisplayLabel(QStringLiteral("MaterialX"));
    materialGroup.addProperty(materialXProp);
  }

  groups.push_back(geometryGroup);
  groups.push_back(renderGroup);
  groups.push_back(materialGroup);
  return groups;
}

bool Artifact3DLayer::setLayerPropertyValue(const QString &propertyPath,
                                            const QVariant &value) {
  if (propertyPath == QStringLiteral("geometry.type")) {
    int geometryInt = value.toInt();
    if (geometryInt >= static_cast<int>(FixedGeometry3D::Auto) &&
        geometryInt <= static_cast<int>(FixedGeometry3D::Pyramid)) {
      setFixedGeometry(static_cast<FixedGeometry3D>(geometryInt));
      return true;
    }
  } else if (propertyPath == QStringLiteral("geometry.width")) {
    impl_->geometryWidth_ = finiteClamped(value.toFloat(), impl_->geometryWidth_, 0.01f, 100000.0f);
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      createFixedGeometryMesh(impl_->fixedGeometry_);
      updateSourceSizeFromMesh();
    }
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("geometry.height")) {
    impl_->geometryHeight_ = finiteClamped(value.toFloat(), impl_->geometryHeight_, 0.01f, 100000.0f);
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      createFixedGeometryMesh(impl_->fixedGeometry_);
      updateSourceSizeFromMesh();
    }
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("geometry.depth")) {
    impl_->geometryDepth_ = finiteClamped(value.toFloat(), impl_->geometryDepth_, 0.01f, 100000.0f);
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      createFixedGeometryMesh(impl_->fixedGeometry_);
      updateSourceSizeFromMesh();
    }
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("geometry.segments")) {
    impl_->geometrySegments_ = std::clamp(value.toInt(), 3, 128);
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      createFixedGeometryMesh(impl_->fixedGeometry_);
      updateSourceSizeFromMesh();
    }
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("geometry.rings")) {
    impl_->geometryRings_ = std::clamp(value.toInt(), 2, 128);
    if (impl_->fixedGeometry_ != FixedGeometry3D::Auto) {
      createFixedGeometryMesh(impl_->fixedGeometry_);
      updateSourceSizeFromMesh();
    }
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.mode")) {
    int modeInt = value.toInt();
    if (modeInt >= static_cast<int>(ModelRenderMode::Wireframe) &&
        modeInt <= static_cast<int>(ModelRenderMode::Solid)) {
      setRenderMode(static_cast<ModelRenderMode>(modeInt));
      return true;
    }
  } else if (propertyPath == QStringLiteral("model.sourcePath") ||
             propertyPath == QStringLiteral("sourcePath")) {
    loadFromFile(value.toString());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("animation.enabled")) {
    setSkinAnimationEnabled(value.toBool());
    return true;
  } else if (propertyPath == QStringLiteral("animation.clipIndex")) {
    setSkinAnimationClipIndex(value.toInt());
    return true;
  } else if (propertyPath.startsWith(
                 QStringLiteral("deformers.blendShapes.")) &&
             propertyPath.endsWith(QStringLiteral(".weight"))) {
    const QString indexText = propertyPath.mid(
        QStringLiteral("deformers.blendShapes.").size(),
        propertyPath.size() - QStringLiteral("deformers.blendShapes.").size() -
            QStringLiteral(".weight").size());
    bool indexOk = false;
    const int shapeIndex = indexText.toInt(&indexOk);
    if (indexOk && shapeIndex >= 0 &&
        shapeIndex < impl_->mesh_.blendShapes().size()) {
      const float weight = value.toFloat();
      if (std::isfinite(weight)) {
        setBlendShapeWeight(shapeIndex, weight);
        return true;
      }
    }
  } else if (propertyPath == QStringLiteral("render.affectedByLights")) {
    setAffectedByLights(value.toBool());
    return true;
  } else if (propertyPath == QStringLiteral("render.useTextureInSolid")) {
    impl_->useTextureInSolid_ = value.toBool();
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.wireOverlay")) {
    impl_->wireOverlay_ = value.toBool();
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.faceNormals")) {
    impl_->faceNormals_ = value.toBool();
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.vertexNormals")) {
    impl_->vertexNormals_ = value.toBool();
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.normalLength")) {
    impl_->normalLength_ = finiteClamped(
        value.toFloat(), impl_->normalLength_, 0.01f, 10000.0f);
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.pointSize")) {
    impl_->pointSize_ = finiteClamped(
        value.toFloat(), impl_->pointSize_, 0.25f, 8.0f);
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.base.color")) {
    impl_->material_.setBaseColor(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.baseColorTexture")) {
    impl_->material_.setBaseColorTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.metallicRoughnessTexture")) {
    impl_->material_.setMetallicRoughnessTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalTexture")) {
    impl_->material_.setNormalTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emissionTexture")) {
    impl_->material_.setEmissionTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.occlusionTexture")) {
    impl_->material_.setOcclusionTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.opacityTexture")) {
    impl_->material_.setOpacityTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emission.color")) {
    impl_->material_.setEmissionColor(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.metallic")) {
    impl_->material_.setMetallic(finiteClamped(value.toFloat(), impl_->material_.metallic(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.roughness")) {
    impl_->material_.setRoughness(finiteClamped(value.toFloat(), impl_->material_.roughness(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.specular")) {
    impl_->material_.setSpecular(finiteClamped(value.toFloat(), impl_->material_.specular(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.ior")) {
    impl_->material_.setIOR(finiteClamped(value.toFloat(), impl_->material_.ior(), 1.0f, 3.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.transmission")) {
    impl_->material_.setTransmission(finiteClamped(value.toFloat(), impl_->material_.transmission(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.clearcoat")) {
    impl_->material_.setClearcoat(finiteClamped(value.toFloat(), impl_->material_.clearcoat(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.clearcoatRoughness")) {
    impl_->material_.setClearcoatRoughness(finiteClamped(value.toFloat(), impl_->material_.clearcoatRoughness(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.sheen")) {
    impl_->material_.setSheen(finiteClamped(value.toFloat(), impl_->material_.sheen(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emissionStrength")) {
    impl_->material_.setEmissionStrength(finiteClamped(value.toFloat(), impl_->material_.emissionStrength(), 0.0f, 100000.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.opacity")) {
    impl_->material_.setOpacity(finiteClamped(value.toFloat(), impl_->material_.opacity(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.alphaMode")) {
    const int alphaMode = std::clamp(
        value.toInt(),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Opaque),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Blended));
    impl_->material_.setAlphaMode(
        static_cast<ArtifactCore::MaterialAlphaMode>(alphaMode));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.alphaCutoff")) {
    impl_->material_.setAlphaCutoff(finiteClamped(
        value.toFloat(), impl_->material_.alphaCutoff(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalStrength")) {
    impl_->material_.setNormalStrength(finiteClamped(value.toFloat(), impl_->material_.normalStrength(), 0.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.occlusionStrength")) {
    impl_->material_.setOcclusionStrength(finiteClamped(value.toFloat(), impl_->material_.occlusionStrength(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

bool Artifact3DLayer::affectedByLights() const { return impl_->affectedByLights_; }

bool Artifact3DLayer::hasTransparentMaterial() const {
  return impl_->material_.opacity() < 0.9999f ||
         impl_->material_.baseColor().alphaF() < 0.9999f ||
         impl_->material_.hasOpacityTexture();
}

QString Artifact3DLayer::materialSignature() const
{
  const QColor baseColor = impl_->material_.baseColor();
  const QColor emissionColor = impl_->material_.emissionColor();
  return QStringLiteral(
             "src=%1|baseTex=%2|mrTex=%3|normalTex=%4|emissionTex=%5|occTex=%6|opacityTex=%7|"
             "base=%8,%9,%10,%11|emission=%12,%13,%14,%15|metallic=%16|roughness=%17|"
             "specular=%18|ior=%19|transmission=%20|clearcoat=%21|clearcoatRoughness=%22|"
             "alphaMode=%23|alphaCutoff=%24|emissionStrength=%25|opacity=%26|"
             "normalStrength=%27|occlusionStrength=%28|sheen=%29|solidTexture=%30|"
             "wireOverlay=%31|graph=%32")
      .arg(impl_->sourcePath_)
      .arg(impl_->material_.baseColorTexture().toQString())
      .arg(impl_->material_.metallicRoughnessTexture().toQString())
      .arg(impl_->material_.normalTexture().toQString())
      .arg(impl_->material_.emissionTexture().toQString())
      .arg(impl_->material_.occlusionTexture().toQString())
      .arg(impl_->material_.opacityTexture().toQString())
      .arg(baseColor.red())
      .arg(baseColor.green())
      .arg(baseColor.blue())
      .arg(baseColor.alpha())
      .arg(emissionColor.red())
      .arg(emissionColor.green())
      .arg(emissionColor.blue())
      .arg(emissionColor.alpha())
      .arg(impl_->material_.metallic(), 0, 'f', 6)
      .arg(impl_->material_.roughness(), 0, 'f', 6)
      .arg(impl_->material_.specular(), 0, 'f', 6)
      .arg(impl_->material_.ior(), 0, 'f', 6)
      .arg(impl_->material_.transmission(), 0, 'f', 6)
      .arg(impl_->material_.clearcoat(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatRoughness(), 0, 'f', 6)
      .arg(static_cast<int>(impl_->material_.alphaMode()))
      .arg(impl_->material_.alphaCutoff(), 0, 'f', 6)
      .arg(impl_->material_.emissionStrength(), 0, 'f', 6)
      .arg(impl_->material_.opacity(), 0, 'f', 6)
      .arg(impl_->material_.normalStrength(), 0, 'f', 6)
      .arg(impl_->material_.occlusionStrength(), 0, 'f', 6)
      .arg(impl_->material_.sheen(), 0, 'f', 6)
      .arg(impl_->useTextureInSolid_ ? 1 : 0)
      .arg(impl_->wireOverlay_ ? 1 : 0)
      .arg(impl_->material_.materialGraphJson().toQString());
}

void Artifact3DLayer::setMaterialGraphJson(const QString& json)
{
  if (impl_->material_.materialGraphJson().toQString() == json) {
    return;
  }
  impl_->material_.setMaterialGraphJson(ArtifactCore::UniString::fromQString(json));
  setDirty();
  Q_EMIT changed();
}

QString Artifact3DLayer::materialGraphJson() const
{
  return impl_->material_.materialGraphJson().toQString();
}

void Artifact3DLayer::clearMaterialGraph()
{
  setMaterialGraphJson(QString());
}

void Artifact3DLayer::setAffectedByLights(bool enabled)
{
  if (impl_->affectedByLights_ == enabled) {
    return;
  }
  impl_->affectedByLights_ = enabled;
  Q_EMIT changed();
}

} // namespace Artifact
