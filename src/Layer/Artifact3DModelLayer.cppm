module;
#include <QFileDialog>
#include <Graphics/InstanceData.h>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
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
import Size;
import MeshImporter;
import Utils.String.UniString;
import Material.Material;
import Asset.Database;
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
  ArtifactCore::Parallel::ForSize(
      0, positionData.size(), positionData.size(), [&](size_t index) {
    positionData[static_cast<qsizetype>(index)] -= center;
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
  // Per-source-mesh data for stages that concatenate several meshes (USD).
  // Empty on the single-mesh import path; mesh_/material_ stay authoritative.
  std::vector<ArtifactCore::Material> meshMaterials_;
  std::vector<QMatrix4x4> meshLocalTransforms_;
  bool meshUsesSingleInstanceForMaterialSlots_ = false;
  QString sourcePath_;
  QUuid sourceAssetId_;
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
  // 0 = Loop (wrap with fmod), 1 = Hold (clamp at the last clip frame),
  // 2 = PingPong (stateless triangle wave over 2x duration).
  int animationPlaybackMode_ = 0;
  float animationSpeed_ = 1.0f;
  QHash<QString, float> blendShapeWeightOverrides_;
  float normalLength_ = 25.0f;
  float pointSize_ = 1.0f;
  QString lastRenderTraceOutcome_;
  ArtifactCore::MeshImporter* animationImporter_ = nullptr;
  // Scratch buffer for world-space overlay vertices. Retained between frames so
  // the overlay paths reuse capacity instead of reallocating per draw.
  QVector<QVector3D> overlayVertices_;
  int64_t overlayVertexCount_ = 0;
  Impl() {}
  ~Impl() { delete animationImporter_; }
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

  if (!impl_->animationImporter_) {
    impl_->animationImporter_ = new ArtifactCore::MeshImporter();
  }
  ArtifactCore::MeshImporter* importer = impl_->animationImporter_;
  impl_->meshUsesSingleInstanceForMaterialSlots_ = false;
  const QString canonicalInput = inputInfo.canonicalFilePath().isEmpty()
      ? inputInfo.absoluteFilePath()
      : inputInfo.canonicalFilePath();
  auto mesh = importer->importMeshFromFile(UniString(canonicalInput));

  if (mesh && mesh->vertexCount() > 0) {
    impl_->mesh_ = *mesh;
    if (!impl_->mesh_.skinBones().isEmpty()) {
      impl_->mesh_.applyDeformers(impl_->mesh_.skinPoseMatrices());
    }
    // A multi-mesh source (a USD stage with several prims) carries per-mesh
    // world transforms that the renderer applies as instances.  Centering the
    // concatenated buffer would shift every primitive away from the transform
    // that positions it, so the recentre step is skipped for that case.
    const int sourceMeshCount = importer->sourceMeshCount();
    if (sourceMeshCount > 1) {
      impl_->meshMaterials_.clear();
      impl_->meshLocalTransforms_.clear();
      impl_->meshMaterials_.reserve(static_cast<std::size_t>(sourceMeshCount));
      impl_->meshLocalTransforms_.reserve(
          static_cast<std::size_t>(sourceMeshCount));
      const auto& sourceTransforms = importer->sourceMeshTransforms();
      const auto metallicFactors = importer->sourceMeshMetallicFactors();
      const auto roughnessFactors = importer->sourceMeshRoughnessFactors();
      const auto& baseColorTextures = importer->sourceMeshBaseColorTextures();
      const auto& metallicRoughnessTextures =
          importer->sourceMeshMetallicRoughnessTextures();
      const auto& normalTextures = importer->sourceMeshNormalTextures();
      const auto& emissionTextures = importer->sourceMeshEmissionTextures();
      const auto& occlusionTextures = importer->sourceMeshOcclusionTextures();
      const auto& opacityTextures = importer->sourceMeshOpacityTextures();
      const auto& clearcoatRoughnessTextures =
          importer->sourceMeshClearcoatRoughnessTextures();
      for (int sourceIndex = 0; sourceIndex < sourceMeshCount; ++sourceIndex) {
        const auto slotIndex = static_cast<std::size_t>(sourceIndex);
        ArtifactCore::Material sourceMaterial = impl_->material_;
        const auto assignTexture = [&sourceMaterial](
                                       const std::vector<UniString>& paths,
                                       std::size_t index,
                                       void (ArtifactCore::Material::*setter)(
                                           const UniString&)) {
          if (index < paths.size() && !paths[index].toQString().isEmpty()) {
            (sourceMaterial.*setter)(paths[index]);
          }
        };
        assignTexture(baseColorTextures, slotIndex,
                      &ArtifactCore::Material::setBaseColorTexture);
        assignTexture(metallicRoughnessTextures, slotIndex,
                      &ArtifactCore::Material::setMetallicRoughnessTexture);
        assignTexture(normalTextures, slotIndex,
                      &ArtifactCore::Material::setNormalTexture);
        assignTexture(emissionTextures, slotIndex,
                      &ArtifactCore::Material::setEmissionTexture);
        assignTexture(occlusionTextures, slotIndex,
                      &ArtifactCore::Material::setOcclusionTexture);
        assignTexture(opacityTextures, slotIndex,
                      &ArtifactCore::Material::setOpacityTexture);
        assignTexture(clearcoatRoughnessTextures, slotIndex,
                      &ArtifactCore::Material::setClearcoatRoughnessTexture);
        if (importer->hasSourceMeshMetallicFactor(sourceIndex)) {
          sourceMaterial.setMetallic(importer->sourceMeshMetallicFactors()[slotIndex]);
        }
        if (importer->hasSourceMeshRoughnessFactor(sourceIndex)) {
          sourceMaterial.setRoughness(importer->sourceMeshRoughnessFactors()[slotIndex]);
        }
        impl_->meshMaterials_.push_back(sourceMaterial);
        QMatrix4x4 localTransform;
        if (slotIndex < sourceTransforms.size()) {
          const auto& sourceTransform = sourceTransforms[slotIndex];
          for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
              localTransform(row, col) = sourceTransform.transform[row * 4 + col];
            }
          }
        }
        impl_->meshLocalTransforms_.push_back(localTransform);
      }
      qDebug() << "[Artifact3DLayer] Imported multi-mesh source with"
               << sourceMeshCount << "primitives";
    } else {
      impl_->meshMaterials_.clear();
      impl_->meshLocalTransforms_.clear();
      centerMeshPositions(impl_->mesh_);
    }
    const auto makeImportedMaterial = [&](const auto& imported) {
      ArtifactCore::Material sourceMaterial = impl_->material_;
      const auto applyImportedUv = [&](const auto& transform,
          auto scaleSetter, auto rotationSetter, auto offsetUSetter,
          auto offsetVSetter, auto uvSetSetter) {
        if (!transform.hasTransform && transform.uvSet == 0) return;
        if (transform.hasTransform && transform.scaleU > 0.0f &&
            std::abs(transform.scaleU - transform.scaleV) <= 1.0e-4f) {
          (sourceMaterial.*scaleSetter)(transform.scaleU);
        }
        if (transform.hasTransform) {
          (sourceMaterial.*rotationSetter)(transform.rotationDegrees);
          (sourceMaterial.*offsetUSetter)(transform.offsetU);
          (sourceMaterial.*offsetVSetter)(transform.offsetV);
        }
        (sourceMaterial.*uvSetSetter)(transform.uvSet);
      };
      if (!imported.name.toQString().isEmpty()) {
        sourceMaterial.setName(imported.name);
      }
      if (imported.hasBaseColor) {
        sourceMaterial.setBaseColor(QColor::fromRgbF(
            imported.baseColor[0], imported.baseColor[1],
            imported.baseColor[2], imported.baseColor[3]));
      }
      if (imported.baseColor[3] < 0.999f) {
        sourceMaterial.setAlphaMode(ArtifactCore::MaterialAlphaMode::Blended);
      }
      if (imported.hasOpacity) {
        sourceMaterial.setOpacity(imported.opacity);
        if (imported.opacity < 0.999f) {
          sourceMaterial.setAlphaMode(
              ArtifactCore::MaterialAlphaMode::Blended);
        }
      }
      if (!imported.baseColorTexture.toQString().isEmpty()) {
        sourceMaterial.setBaseColorTexture(imported.baseColorTexture);
        applyImportedUv(imported.baseColorUv,
            &ArtifactCore::Material::setBaseColorUvScale,
            &ArtifactCore::Material::setBaseColorUvRotationDegrees,
            &ArtifactCore::Material::setBaseColorUvOffsetU,
            &ArtifactCore::Material::setBaseColorUvOffsetV,
            &ArtifactCore::Material::setBaseColorUvSet);
      }
      if (!imported.metallicRoughnessTexture.toQString().isEmpty()) {
        sourceMaterial.setMetallicRoughnessTexture(
            imported.metallicRoughnessTexture);
        applyImportedUv(imported.metallicRoughnessUv,
            &ArtifactCore::Material::setMetallicRoughnessUvScale,
            &ArtifactCore::Material::setMetallicRoughnessUvRotationDegrees,
            &ArtifactCore::Material::setMetallicRoughnessUvOffsetU,
            &ArtifactCore::Material::setMetallicRoughnessUvOffsetV,
            &ArtifactCore::Material::setMetallicRoughnessUvSet);
      }
      if (!imported.metallicTexture.toQString().isEmpty()) {
        sourceMaterial.setMetallicTexture(imported.metallicTexture);
        ArtifactCore::MaterialTextureUvTransform transform;
        transform.scaleU = imported.metallicUv.scaleU;
        transform.scaleV = imported.metallicUv.scaleV;
        transform.rotationDegrees = imported.metallicUv.rotationDegrees;
        transform.offsetU = imported.metallicUv.offsetU;
        transform.offsetV = imported.metallicUv.offsetV;
        transform.uvSet = imported.metallicUv.uvSet;
        sourceMaterial.setMetallicTextureUvTransform(transform);
      }
      if (!imported.roughnessTexture.toQString().isEmpty()) {
        sourceMaterial.setRoughnessTexture(imported.roughnessTexture);
        ArtifactCore::MaterialTextureUvTransform transform;
        transform.scaleU = imported.roughnessUv.scaleU;
        transform.scaleV = imported.roughnessUv.scaleV;
        transform.rotationDegrees = imported.roughnessUv.rotationDegrees;
        transform.offsetU = imported.roughnessUv.offsetU;
        transform.offsetV = imported.roughnessUv.offsetV;
        transform.uvSet = imported.roughnessUv.uvSet;
        sourceMaterial.setRoughnessTextureUvTransform(transform);
      }
      if (!imported.normalTexture.toQString().isEmpty()) {
        sourceMaterial.setNormalTexture(imported.normalTexture);
        applyImportedUv(imported.normalUv,
            &ArtifactCore::Material::setNormalUvScale,
            &ArtifactCore::Material::setNormalUvRotationDegrees,
            &ArtifactCore::Material::setNormalUvOffsetU,
            &ArtifactCore::Material::setNormalUvOffsetV,
            &ArtifactCore::Material::setNormalUvSet);
      }
      if (!imported.clearcoatTexture.toQString().isEmpty()) {
        sourceMaterial.setClearcoatTexture(imported.clearcoatTexture);
        applyImportedUv(imported.clearcoatUv,
            &ArtifactCore::Material::setClearcoatTextureUvScale,
            &ArtifactCore::Material::setClearcoatTextureUvRotationDegrees,
            &ArtifactCore::Material::setClearcoatTextureUvOffsetU,
            &ArtifactCore::Material::setClearcoatTextureUvOffsetV,
            &ArtifactCore::Material::setClearcoatTextureUvSet);
      }
      if (!imported.clearcoatRoughnessTexture.toQString().isEmpty()) {
        sourceMaterial.setClearcoatRoughnessTexture(
            imported.clearcoatRoughnessTexture);
        applyImportedUv(imported.clearcoatRoughnessUv,
            &ArtifactCore::Material::setClearcoatRoughnessTextureUvScale,
            &ArtifactCore::Material::setClearcoatRoughnessTextureUvRotationDegrees,
            &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetU,
            &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetV,
            &ArtifactCore::Material::setClearcoatRoughnessTextureUvSet);
      }
      if (!imported.clearcoatNormalTexture.toQString().isEmpty()) {
        sourceMaterial.setClearcoatNormalTexture(
            imported.clearcoatNormalTexture);
        applyImportedUv(imported.clearcoatNormalUv,
            &ArtifactCore::Material::setClearcoatNormalUvScale,
            &ArtifactCore::Material::setClearcoatNormalUvRotationDegrees,
            &ArtifactCore::Material::setClearcoatNormalUvOffsetU,
            &ArtifactCore::Material::setClearcoatNormalUvOffsetV,
            &ArtifactCore::Material::setClearcoatNormalUvSet);
      }
      if (!imported.emissionTexture.toQString().isEmpty()) {
        sourceMaterial.setEmissionTexture(imported.emissionTexture);
        applyImportedUv(imported.emissionUv,
            &ArtifactCore::Material::setEmissionTextureUvScale,
            &ArtifactCore::Material::setEmissionTextureUvRotationDegrees,
            &ArtifactCore::Material::setEmissionTextureUvOffsetU,
            &ArtifactCore::Material::setEmissionTextureUvOffsetV,
            &ArtifactCore::Material::setEmissionTextureUvSet);
      }
      if (!imported.occlusionTexture.toQString().isEmpty()) {
        sourceMaterial.setOcclusionTexture(imported.occlusionTexture);
        applyImportedUv(imported.occlusionUv,
            &ArtifactCore::Material::setOcclusionTextureUvScale,
            &ArtifactCore::Material::setOcclusionTextureUvRotationDegrees,
            &ArtifactCore::Material::setOcclusionTextureUvOffsetU,
            &ArtifactCore::Material::setOcclusionTextureUvOffsetV,
            &ArtifactCore::Material::setOcclusionTextureUvSet);
      }
      if (!imported.opacityTexture.toQString().isEmpty()) {
        sourceMaterial.setOpacityTexture(imported.opacityTexture);
        applyImportedUv(imported.opacityUv,
            &ArtifactCore::Material::setOpacityTextureUvScale,
            &ArtifactCore::Material::setOpacityTextureUvRotationDegrees,
            &ArtifactCore::Material::setOpacityTextureUvOffsetU,
            &ArtifactCore::Material::setOpacityTextureUvOffsetV,
            &ArtifactCore::Material::setOpacityTextureUvSet);
      }
      if (!imported.sheenColorTexture.toQString().isEmpty()) {
        sourceMaterial.setSheenColorTexture(imported.sheenColorTexture);
        ArtifactCore::MaterialTextureUvTransform transform;
        transform.scaleU = imported.sheenColorUv.scaleU;
        transform.scaleV = imported.sheenColorUv.scaleV;
        transform.rotationDegrees = imported.sheenColorUv.rotationDegrees;
        transform.offsetU = imported.sheenColorUv.offsetU;
        transform.offsetV = imported.sheenColorUv.offsetV;
        transform.uvSet = imported.sheenColorUv.uvSet;
        sourceMaterial.setSheenColorTextureUvTransform(transform);
      }
      if (!imported.sheenRoughnessTexture.toQString().isEmpty()) {
        sourceMaterial.setSheenRoughnessTexture(
            imported.sheenRoughnessTexture);
        ArtifactCore::MaterialTextureUvTransform transform;
        transform.scaleU = imported.sheenRoughnessUv.scaleU;
        transform.scaleV = imported.sheenRoughnessUv.scaleV;
        transform.rotationDegrees = imported.sheenRoughnessUv.rotationDegrees;
        transform.offsetU = imported.sheenRoughnessUv.offsetU;
        transform.offsetV = imported.sheenRoughnessUv.offsetV;
        transform.uvSet = imported.sheenRoughnessUv.uvSet;
        sourceMaterial.setSheenRoughnessTextureUvTransform(transform);
      }
      if (imported.hasMetallic) {
        sourceMaterial.setMetallic(imported.metallic);
      }
      if (imported.hasRoughness) {
        sourceMaterial.setRoughness(imported.roughness);
      }
      if (imported.hasDiffuseRoughness) {
        sourceMaterial.setDiffuseRoughness(imported.diffuseRoughness);
      }
      if (imported.hasClearcoat) {
        sourceMaterial.setClearcoat(imported.clearcoat);
      }
      if (imported.hasClearcoatRoughness) {
        sourceMaterial.setClearcoatRoughness(imported.clearcoatRoughness);
      }
      if (imported.hasClearcoatIor) {
        sourceMaterial.setClearcoatIOR(imported.clearcoatIor);
      }
      if (imported.hasClearcoatTint) {
        sourceMaterial.setClearcoatTint(QColor::fromRgbF(
            imported.clearcoatTint[0], imported.clearcoatTint[1],
            imported.clearcoatTint[2]));
      }
      if (imported.hasThinFilmThickness || imported.hasThinFilmIor) {
        sourceMaterial.setThinFilm(
            imported.hasThinFilmThickness
                ? imported.thinFilmThickness
                : sourceMaterial.thinFilmThickness(),
            imported.hasThinFilmIor
                ? imported.thinFilmIor
                : sourceMaterial.thinFilmIOR());
      }
      if (imported.hasTransmission) {
        sourceMaterial.setTransmission(imported.transmission);
      }
      if (imported.hasIor) {
        sourceMaterial.setIOR(imported.ior);
      }
      if (imported.hasSheen) {
        sourceMaterial.setSheen(imported.sheen);
      }
      if (imported.hasSheenRoughness) {
        sourceMaterial.setSheenRoughness(imported.sheenRoughness);
      }
      if (imported.hasSpecular) {
        sourceMaterial.setSpecular(imported.specular);
      }
      if (imported.hasSpecularTint) {
        sourceMaterial.setSpecularTint(QColor::fromRgbF(
            imported.specularTint[0], imported.specularTint[1],
            imported.specularTint[2]));
      }
      if (imported.hasSheenTint) {
        sourceMaterial.setSheenTint(QColor::fromRgbF(
            imported.sheenTint[0], imported.sheenTint[1],
            imported.sheenTint[2]));
      }
      if (imported.hasEmissionColor) {
        sourceMaterial.setEmissionColor(QColor::fromRgbF(
            imported.emissionColor[0], imported.emissionColor[1],
            imported.emissionColor[2]));
      }
      if (imported.hasEmissionStrength) {
        sourceMaterial.setEmissionStrength(imported.emissionStrength);
      }
      if (imported.hasSubsurface) {
        sourceMaterial.setSubsurface(imported.subsurface);
      }
      if (imported.hasSubsurfaceColor) {
        sourceMaterial.setSubsurfaceColor(QColor::fromRgbF(
            imported.subsurfaceColor[0], imported.subsurfaceColor[1],
            imported.subsurfaceColor[2]));
      }
      if (imported.hasSubsurfaceRadius) {
        sourceMaterial.setSubsurfaceRadius(QVector3D(
            imported.subsurfaceRadius[0], imported.subsurfaceRadius[1],
            imported.subsurfaceRadius[2]));
      }
      if (imported.hasSubsurfaceAnisotropy) {
        sourceMaterial.setSubsurfaceAnisotropy(
            imported.subsurfaceAnisotropy);
      }
      if (imported.hasAnisotropy || imported.hasAnisotropyRotation) {
        sourceMaterial.setAnisotropy(
            imported.hasAnisotropy ? imported.anisotropy
                                   : sourceMaterial.anisotropy(),
            imported.hasAnisotropyRotation
                ? imported.anisotropyRotationDegrees
                : sourceMaterial.anisotropyRotationDegrees());
      }
      if (!imported.anisotropyTexture.toQString().isEmpty()) {
        sourceMaterial.setAnisotropyTexture(imported.anisotropyTexture);
        ArtifactCore::MaterialTextureUvTransform transform;
        transform.scaleU = imported.anisotropyUv.scaleU;
        transform.scaleV = imported.anisotropyUv.scaleV;
        transform.rotationDegrees = imported.anisotropyUv.rotationDegrees;
        transform.offsetU = imported.anisotropyUv.offsetU;
        transform.offsetV = imported.anisotropyUv.offsetV;
        transform.uvSet = imported.anisotropyUv.uvSet;
        sourceMaterial.setAnisotropyTextureUvTransform(transform);
      }
      return sourceMaterial;
    };
    const auto& importedMaterials = importer->importedPbrMaterials();
    const bool importedMaterialSlots =
        (importer->lastBackend() == ArtifactCore::MeshImporter::Backend::Ufbx ||
         importer->lastBackend() == ArtifactCore::MeshImporter::Backend::UfbxGltf ||
         importer->lastBackend() == ArtifactCore::MeshImporter::Backend::TinyObj) &&
        !importedMaterials.empty() && impl_->mesh_.materialSlotCount() > 0;
    if (importedMaterialSlots && importedMaterials.size() > 1) {
      impl_->meshMaterials_.clear();
      impl_->meshMaterials_.reserve(importedMaterials.size());
      for (const auto& imported : importedMaterials) {
        impl_->meshMaterials_.push_back(makeImportedMaterial(imported));
      }
      impl_->meshLocalTransforms_.clear();
      impl_->meshUsesSingleInstanceForMaterialSlots_ = true;
    } else if (importer->lastBackend() ==
                   ArtifactCore::MeshImporter::Backend::TinyObj &&
               importedMaterials.size() == 1 &&
               impl_->mesh_.materialSlotCount() > 0) {
      impl_->material_ = makeImportedMaterial(importedMaterials.front());
    }
    impl_->meshLoaded_ = true;
    updateSourceSizeFromMesh();
    const QString importedTexture = importer->lastBaseColorTexture();
    if (!importedTexture.isEmpty() &&
        impl_->material_.baseColorTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported base color texture:" << importedTexture;
      impl_->material_.setBaseColorTexture(
          ArtifactCore::UniString::fromQString(importedTexture));
    }
    const QString importedMetallicRoughnessTexture =
        importer->lastMetallicRoughnessTexture();
    if (!importedMetallicRoughnessTexture.isEmpty() &&
        impl_->material_.metallicRoughnessTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported metallic-roughness texture:"
               << importedMetallicRoughnessTexture;
      impl_->material_.setMetallicRoughnessTexture(
          ArtifactCore::UniString::fromQString(importedMetallicRoughnessTexture));
    }
    const QString importedNormalTexture = importer->lastNormalTexture();
    if (!importedNormalTexture.isEmpty() &&
        impl_->material_.normalTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported normal texture:" << importedNormalTexture;
      impl_->material_.setNormalTexture(
          ArtifactCore::UniString::fromQString(importedNormalTexture));
    }
    const QString importedClearcoatRoughnessTexture =
        importer->lastClearcoatRoughnessTexture();
    if (!importedClearcoatRoughnessTexture.isEmpty() &&
        impl_->material_.clearcoatRoughnessTexture().toQString().isEmpty()) {
      impl_->material_.setClearcoatRoughnessTexture(
          ArtifactCore::UniString::fromQString(
              importedClearcoatRoughnessTexture));
    }
    const QString importedEmissionTexture = importer->lastEmissionTexture();
    if (!importedEmissionTexture.isEmpty() &&
        impl_->material_.emissionTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported emission texture:"
               << importedEmissionTexture;
      impl_->material_.setEmissionTexture(
          ArtifactCore::UniString::fromQString(importedEmissionTexture));
    }
    const QString importedOcclusionTexture = importer->lastOcclusionTexture();
    if (!importedOcclusionTexture.isEmpty() &&
        impl_->material_.occlusionTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported occlusion texture:"
               << importedOcclusionTexture;
      impl_->material_.setOcclusionTexture(
          ArtifactCore::UniString::fromQString(importedOcclusionTexture));
    }
    const QString importedOpacityTexture = importer->lastOpacityTexture();
    if (!importedOpacityTexture.isEmpty() &&
        impl_->material_.opacityTexture().toQString().isEmpty()) {
      qDebug() << "[Artifact3DLayer] Imported opacity texture:"
               << importedOpacityTexture;
      impl_->material_.setOpacityTexture(
          ArtifactCore::UniString::fromQString(importedOpacityTexture));
    }
    // PBR scalar factors: apply only when the file specifies them, the ufbx
    // backend produced them, and the material is still at its default (never
    // user-edited), mirroring the texture isEmpty guards above.
    const bool factorsFromUfbx =
        importer->lastBackend() == ArtifactCore::MeshImporter::Backend::Ufbx ||
        importer->lastBackend() == ArtifactCore::MeshImporter::Backend::UfbxGltf;
    if (factorsFromUfbx && importer->hasLastMetallicFactor() &&
        impl_->material_.metallic() == 0.0f) {
      impl_->material_.setMetallic(importer->lastMetallicFactor());
    }
    if (factorsFromUfbx && importer->hasLastRoughnessFactor() &&
        impl_->material_.roughness() == 0.5f) {
      impl_->material_.setRoughness(importer->lastRoughnessFactor());
    }
    if (factorsFromUfbx && importer->hasLastClearcoatFactor() &&
        impl_->material_.clearcoat() == 0.0f) {
      impl_->material_.setClearcoat(importer->lastClearcoatFactor());
    }
    if (factorsFromUfbx && importer->hasLastClearcoatRoughnessFactor() &&
        impl_->material_.clearcoatRoughness() == 0.03f) {
      impl_->material_.setClearcoatRoughness(
          importer->lastClearcoatRoughnessFactor());
    }
    if (factorsFromUfbx && importer->hasLastTransmissionFactor() &&
        impl_->material_.transmission() == 0.0f) {
      impl_->material_.setTransmission(importer->lastTransmissionFactor());
    }
    if (factorsFromUfbx && importer->hasLastIorFactor() &&
        impl_->material_.ior() == 1.5f) {
      impl_->material_.setIOR(importer->lastIorFactor());
    }
    if (factorsFromUfbx && importer->hasLastSheenFactor() &&
        impl_->material_.sheen() == 0.0f) {
      impl_->material_.setSheen(importer->lastSheenFactor());
    }
    if (factorsFromUfbx && importer->hasLastSheenRoughnessFactor() &&
        impl_->material_.sheenRoughness() == 0.5f) {
      impl_->material_.setSheenRoughness(
          importer->lastSheenRoughnessFactor());
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
    impl_->sourceAssetId_ = ArtifactCore::AssetDatabase::instance().registerAsset(
        normalizedSourcePath, ArtifactCore::AssetType::Model,
        impl_->sourceAssetId_);
    setLayerName(sourceInfo.baseName());
    Q_EMIT changed();
    return;
  }

  // Fallback to cube on failure
  qWarning() << "Failed to load mesh from:" << normalizedInput
             << "- using default cube";
  impl_->fixedGeometry_ = FixedGeometry3D::Cube;
  impl_->sourcePath_.clear();
  impl_->sourceAssetId_ = QUuid();
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
  impl_->sourceAssetId_ = QUuid();
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

QUuid Artifact3DLayer::sourceAssetId() const { return impl_->sourceAssetId_; }

UniString Artifact3DLayer::className() const { return QStringLiteral("Artifact3DLayer"); }

QJsonObject Artifact3DLayer::toJson() const {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
  const auto textureTransformJson = [](const auto& transform) {
    return QJsonObject{
        {QStringLiteral("scaleU"), transform.scaleU},
        {QStringLiteral("scaleV"), transform.scaleV},
        {QStringLiteral("rotation"), transform.rotationDegrees},
        {QStringLiteral("offsetU"), transform.offsetU},
        {QStringLiteral("offsetV"), transform.offsetV},
        {QStringLiteral("uvSet"), transform.uvSet}};
  };
  obj["type"] = static_cast<int>(LayerType::Model3D);
  obj["sourcePath"] = impl_->sourcePath_;
  obj["model.sourceAssetId"] = impl_->sourceAssetId_.toString(QUuid::WithoutBraces);
  obj["animation.enabled"] = impl_->skinAnimationEnabled_;
  obj["animation.clipIndex"] = impl_->skinAnimationClipIndex_;
  obj["animation.playbackMode"] = impl_->animationPlaybackMode_;
  obj["animation.speed"] = impl_->animationSpeed_;
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
  obj["material.diffuseRoughness"] =
      impl_->material_.diffuseRoughness();
  obj["material.specular"] = impl_->material_.specular();
  const QColor specularTint = impl_->material_.specularTint();
  obj["material.specularTint"] = QJsonObject{
      {QStringLiteral("r"), specularTint.redF()},
      {QStringLiteral("g"), specularTint.greenF()},
      {QStringLiteral("b"), specularTint.blueF()},
      {QStringLiteral("a"), specularTint.alphaF()}};
  obj["material.thinFilmThickness"] = impl_->material_.thinFilmThickness();
  obj["material.thinFilmIOR"] = impl_->material_.thinFilmIOR();
  obj["material.ior"] = impl_->material_.ior();
  obj["material.transmission"] = impl_->material_.transmission();
  obj["material.thinWall"] = impl_->material_.thinWall();
  obj["material.clearcoat"] = impl_->material_.clearcoat();
  obj["material.clearcoatRoughness"] = impl_->material_.clearcoatRoughness();
  obj["material.clearcoatIOR"] = impl_->material_.clearcoatIOR();
  const QColor clearcoatTint = impl_->material_.clearcoatTint();
  obj["material.clearcoatTint"] = QJsonObject{
      {QStringLiteral("r"), clearcoatTint.redF()},
      {QStringLiteral("g"), clearcoatTint.greenF()},
      {QStringLiteral("b"), clearcoatTint.blueF()},
      {QStringLiteral("a"), clearcoatTint.alphaF()}};
  obj["material.clearcoatNormalStrength"] =
      impl_->material_.clearcoatNormalStrength();
  obj["material.sheen"] = impl_->material_.sheen();
  obj["material.subsurface"] = impl_->material_.subsurface();
  obj["material.subsurfaceIOR"] = impl_->material_.subsurfaceIOR();
  obj["material.subsurfaceAnisotropy"] =
      impl_->material_.subsurfaceAnisotropy();
  const QColor subsurfaceColor = impl_->material_.subsurfaceColor();
  obj["material.subsurfaceColor"] = QJsonObject{
      {QStringLiteral("r"), subsurfaceColor.redF()},
      {QStringLiteral("g"), subsurfaceColor.greenF()},
      {QStringLiteral("b"), subsurfaceColor.blueF()},
      {QStringLiteral("a"), subsurfaceColor.alphaF()}};
  const QVector3D subsurfaceRadius = impl_->material_.subsurfaceRadius();
  obj["material.subsurfaceRadius"] = QJsonObject{
      {QStringLiteral("r"), subsurfaceRadius.x()},
      {QStringLiteral("g"), subsurfaceRadius.y()},
      {QStringLiteral("b"), subsurfaceRadius.z()}};
  obj["material.anisotropy"] = impl_->material_.anisotropy();
  obj["material.anisotropyRotationDegrees"] =
      impl_->material_.anisotropyRotationDegrees();
  obj["material.anisotropyTexture"] =
      impl_->material_.anisotropyTexture().toQString();
  obj["material.anisotropyTextureUv"] = textureTransformJson(
      impl_->material_.anisotropyTextureUvTransform());
  obj["material.sheenRoughness"] = impl_->material_.sheenRoughness();
  const QColor sheenTint = impl_->material_.sheenTint();
  obj["material.sheenTint"] = QJsonObject{
      {QStringLiteral("r"), sheenTint.redF()},
      {QStringLiteral("g"), sheenTint.greenF()},
      {QStringLiteral("b"), sheenTint.blueF()},
      {QStringLiteral("a"), sheenTint.alphaF()}};
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
  obj["material.normalUvRotation"] =
      impl_->material_.normalUvRotationDegrees();
  obj["material.normalUvScale"] = impl_->material_.normalUvScale();
  obj["material.normalUvOffsetU"] = impl_->material_.normalUvOffsetU();
  obj["material.normalUvOffsetV"] = impl_->material_.normalUvOffsetV();
  obj["material.normalUvSet"] = impl_->material_.normalUvSet();
  obj["material.occlusionStrength"] = impl_->material_.occlusionStrength();
  obj["material.occlusionTextureUvScale"] =
      impl_->material_.occlusionTextureUvScale();
  obj["material.occlusionTextureUvOffsetU"] =
      impl_->material_.occlusionTextureUvOffsetU();
  obj["material.occlusionTextureUvOffsetV"] =
      impl_->material_.occlusionTextureUvOffsetV();
  obj["material.occlusionTextureUvRotation"] =
      impl_->material_.occlusionTextureUvRotationDegrees();
  obj["material.occlusionTextureUvSet"] =
      impl_->material_.occlusionTextureUvSet();
  obj["material.uv.offsetU"] = impl_->material_.uvOffsetU();
  obj["material.uv.offsetV"] = impl_->material_.uvOffsetV();
  obj["material.uv.scaleU"] = impl_->material_.uvScaleU();
  obj["material.uv.scaleV"] = impl_->material_.uvScaleV();
  obj["material.uv.rotation"] = impl_->material_.uvRotationDegrees();
  obj["material.envIntensity"] = impl_->material_.environmentIntensity();
  obj["material.baseColorTexture"] = impl_->material_.baseColorTexture().toQString();
  obj["material.baseColorUvScale"] = impl_->material_.baseColorUvScale();
  obj["material.baseColorUvRotation"] =
      impl_->material_.baseColorUvRotationDegrees();
  obj["material.baseColorUvOffsetU"] =
      impl_->material_.baseColorUvOffsetU();
  obj["material.baseColorUvOffsetV"] =
      impl_->material_.baseColorUvOffsetV();
  obj["material.baseColorUvSet"] = impl_->material_.baseColorUvSet();
  obj["material.metallicRoughnessTexture"] =
      impl_->material_.metallicRoughnessTexture().toQString();
  obj["material.metallicTexture"] =
      impl_->material_.metallicTexture().toQString();
  obj["material.roughnessTexture"] =
      impl_->material_.roughnessTexture().toQString();
  obj["material.metallicTextureUv"] = textureTransformJson(
      impl_->material_.metallicTextureUvTransform());
  obj["material.roughnessTextureUv"] = textureTransformJson(
      impl_->material_.roughnessTextureUvTransform());
  obj["material.metallicRoughnessUvScale"] =
      impl_->material_.metallicRoughnessUvScale();
  obj["material.metallicRoughnessUvOffsetU"] =
      impl_->material_.metallicRoughnessUvOffsetU();
  obj["material.metallicRoughnessUvOffsetV"] =
      impl_->material_.metallicRoughnessUvOffsetV();
  obj["material.metallicRoughnessUvRotation"] =
      impl_->material_.metallicRoughnessUvRotationDegrees();
  obj["material.metallicRoughnessUvSet"] =
      impl_->material_.metallicRoughnessUvSet();
  obj["material.normalTexture"] = impl_->material_.normalTexture().toQString();
  obj["material.clearcoatNormalTexture"] =
      impl_->material_.clearcoatNormalTexture().toQString();
  obj["material.clearcoatNormalUvRotation"] =
      impl_->material_.clearcoatNormalUvRotationDegrees();
  obj["material.clearcoatNormalUvScale"] =
      impl_->material_.clearcoatNormalUvScale();
  obj["material.clearcoatNormalUvOffsetU"] =
      impl_->material_.clearcoatNormalUvOffsetU();
  obj["material.clearcoatNormalUvOffsetV"] =
      impl_->material_.clearcoatNormalUvOffsetV();
  obj["material.clearcoatNormalUvSet"] =
      impl_->material_.clearcoatNormalUvSet();
  obj["material.clearcoatTexture"] =
      impl_->material_.clearcoatTexture().toQString();
  obj["material.clearcoatTextureUvScale"] =
      impl_->material_.clearcoatTextureUvScale();
  obj["material.clearcoatTextureUvRotation"] =
      impl_->material_.clearcoatTextureUvRotationDegrees();
  obj["material.clearcoatTextureUvOffsetU"] =
      impl_->material_.clearcoatTextureUvOffsetU();
  obj["material.clearcoatTextureUvOffsetV"] =
      impl_->material_.clearcoatTextureUvOffsetV();
  obj["material.clearcoatTextureUvSet"] =
      impl_->material_.clearcoatTextureUvSet();
  obj["material.clearcoatRoughnessTexture"] =
      impl_->material_.clearcoatRoughnessTexture().toQString();
  obj["material.clearcoatRoughnessTextureUvScale"] =
      impl_->material_.clearcoatRoughnessTextureUvScale();
  obj["material.clearcoatRoughnessTextureUvRotation"] =
      impl_->material_.clearcoatRoughnessTextureUvRotationDegrees();
  obj["material.clearcoatRoughnessTextureUvOffsetU"] =
      impl_->material_.clearcoatRoughnessTextureUvOffsetU();
  obj["material.clearcoatRoughnessTextureUvOffsetV"] =
      impl_->material_.clearcoatRoughnessTextureUvOffsetV();
  obj["material.clearcoatRoughnessTextureUvSet"] =
      impl_->material_.clearcoatRoughnessTextureUvSet();
  obj["material.sheenColorTexture"] =
      impl_->material_.sheenColorTexture().toQString();
  obj["material.sheenColorTextureUv"] = textureTransformJson(
      impl_->material_.sheenColorTextureUvTransform());
  obj["material.sheenRoughnessTexture"] =
      impl_->material_.sheenRoughnessTexture().toQString();
  obj["material.sheenRoughnessTextureUv"] = textureTransformJson(
      impl_->material_.sheenRoughnessTextureUvTransform());
  obj["material.emissionTexture"] = impl_->material_.emissionTexture().toQString();
  obj["material.emissionTextureUvScale"] =
      impl_->material_.emissionTextureUvScale();
  obj["material.emissionTextureUvOffsetU"] =
      impl_->material_.emissionTextureUvOffsetU();
  obj["material.emissionTextureUvOffsetV"] =
      impl_->material_.emissionTextureUvOffsetV();
  obj["material.emissionTextureUvRotation"] =
      impl_->material_.emissionTextureUvRotationDegrees();
  obj["material.emissionTextureUvSet"] =
      impl_->material_.emissionTextureUvSet();
  obj["material.occlusionTexture"] = impl_->material_.occlusionTexture().toQString();
  obj["material.opacityTexture"] = impl_->material_.opacityTexture().toQString();
  obj["material.opacityTextureUvScale"] =
      impl_->material_.opacityTextureUvScale();
  obj["material.opacityTextureUvOffsetU"] =
      impl_->material_.opacityTextureUvOffsetU();
  obj["material.opacityTextureUvOffsetV"] =
      impl_->material_.opacityTextureUvOffsetV();
  obj["material.opacityTextureUvRotation"] =
      impl_->material_.opacityTextureUvRotationDegrees();
  obj["material.opacityTextureUvSet"] =
      impl_->material_.opacityTextureUvSet();
  if (!impl_->meshMaterials_.empty()) {
    QJsonArray materialSlots;
    for (const auto& material : impl_->meshMaterials_) {
      const QColor base = material.baseColor();
      const QColor specular = material.specularTint();
      const QColor sheen = material.sheenTint();
      const QColor emission = material.emissionColor();
      const auto textureUv = [](float scale, float rotation, float offsetU,
                                float offsetV, int uvSet) {
        return QJsonObject{{QStringLiteral("scale"), scale},
                           {QStringLiteral("rotation"), rotation},
                           {QStringLiteral("offsetU"), offsetU},
                           {QStringLiteral("offsetV"), offsetV},
                           {QStringLiteral("uvSet"), uvSet}};
      };
      const auto textureUvSeparate = [](const auto& transform) {
        return QJsonObject{
            {QStringLiteral("scaleU"), transform.scaleU},
            {QStringLiteral("scaleV"), transform.scaleV},
            {QStringLiteral("rotation"), transform.rotationDegrees},
            {QStringLiteral("offsetU"), transform.offsetU},
            {QStringLiteral("offsetV"), transform.offsetV},
            {QStringLiteral("uvSet"), transform.uvSet}};
      };
      materialSlots.append(QJsonObject{
          {QStringLiteral("name"), material.name().toQString()},
          {QStringLiteral("baseColor"), QJsonArray{base.redF(), base.greenF(),
                                                   base.blueF(), base.alphaF()}},
          {QStringLiteral("metallic"), material.metallic()},
          {QStringLiteral("roughness"), material.roughness()},
          {QStringLiteral("diffuseRoughness"), material.diffuseRoughness()},
          {QStringLiteral("specular"), material.specular()},
          {QStringLiteral("specularTint"), QJsonArray{specular.redF(),
              specular.greenF(), specular.blueF()}},
          {QStringLiteral("ior"), material.ior()},
          {QStringLiteral("transmission"), material.transmission()},
          {QStringLiteral("thinWall"), material.thinWall()},
          {QStringLiteral("thinFilmThickness"), material.thinFilmThickness()},
          {QStringLiteral("thinFilmIOR"), material.thinFilmIOR()},
          {QStringLiteral("clearcoat"), material.clearcoat()},
          {QStringLiteral("clearcoatRoughness"), material.clearcoatRoughness()},
          {QStringLiteral("clearcoatIOR"), material.clearcoatIOR()},
          {QStringLiteral("clearcoatTint"), QJsonArray{
              material.clearcoatTint().redF(),
              material.clearcoatTint().greenF(),
              material.clearcoatTint().blueF()}},
          {QStringLiteral("clearcoatNormalStrength"),
              material.clearcoatNormalStrength()},
          {QStringLiteral("sheen"), material.sheen()},
          {QStringLiteral("sheenRoughness"), material.sheenRoughness()},
          {QStringLiteral("sheenTint"), QJsonArray{sheen.redF(), sheen.greenF(),
                                                   sheen.blueF()}},
          {QStringLiteral("subsurface"), material.subsurface()},
          {QStringLiteral("subsurfaceIOR"), material.subsurfaceIOR()},
          {QStringLiteral("subsurfaceAnisotropy"),
              material.subsurfaceAnisotropy()},
          {QStringLiteral("subsurfaceColor"), QJsonArray{
              material.subsurfaceColor().redF(),
              material.subsurfaceColor().greenF(),
              material.subsurfaceColor().blueF()}},
          {QStringLiteral("subsurfaceRadius"), QJsonArray{
              material.subsurfaceRadius().x(),
              material.subsurfaceRadius().y(),
              material.subsurfaceRadius().z()}},
          {QStringLiteral("anisotropy"), material.anisotropy()},
          {QStringLiteral("anisotropyRotation"),
              material.anisotropyRotationDegrees()},
          {QStringLiteral("anisotropyTexture"),
              material.anisotropyTexture().toQString()},
          {QStringLiteral("anisotropyTextureUv"),
              textureUvSeparate(material.anisotropyTextureUvTransform())},
          {QStringLiteral("opacity"), material.opacity()},
          {QStringLiteral("alphaMode"), static_cast<int>(material.alphaMode())},
          {QStringLiteral("alphaCutoff"), material.alphaCutoff()},
          {QStringLiteral("emissionColor"), QJsonArray{emission.redF(),
              emission.greenF(), emission.blueF()}},
          {QStringLiteral("emissionStrength"), material.emissionStrength()},
          {QStringLiteral("normalStrength"), material.normalStrength()},
          {QStringLiteral("occlusionStrength"), material.occlusionStrength()},
          {QStringLiteral("environmentIntensity"),
              material.environmentIntensity()},
          {QStringLiteral("uvOffsetU"), material.uvOffsetU()},
          {QStringLiteral("uvOffsetV"), material.uvOffsetV()},
          {QStringLiteral("uvScaleU"), material.uvScaleU()},
          {QStringLiteral("uvScaleV"), material.uvScaleV()},
          {QStringLiteral("uvRotation"), material.uvRotationDegrees()},
          {QStringLiteral("baseColorTexture"),
              material.baseColorTexture().toQString()},
          {QStringLiteral("metallicRoughnessTexture"),
              material.metallicRoughnessTexture().toQString()},
          {QStringLiteral("metallicTexture"),
              material.metallicTexture().toQString()},
          {QStringLiteral("roughnessTexture"),
              material.roughnessTexture().toQString()},
          {QStringLiteral("metallicTextureUv"),
              textureUvSeparate(material.metallicTextureUvTransform())},
          {QStringLiteral("roughnessTextureUv"),
              textureUvSeparate(material.roughnessTextureUvTransform())},
          {QStringLiteral("normalTexture"), material.normalTexture().toQString()},
          {QStringLiteral("clearcoatNormalTexture"),
              material.clearcoatNormalTexture().toQString()},
          {QStringLiteral("clearcoatTexture"),
              material.clearcoatTexture().toQString()},
          {QStringLiteral("clearcoatRoughnessTexture"),
              material.clearcoatRoughnessTexture().toQString()},
          {QStringLiteral("sheenColorTexture"),
              material.sheenColorTexture().toQString()},
          {QStringLiteral("sheenColorUv"),
              textureUvSeparate(material.sheenColorTextureUvTransform())},
          {QStringLiteral("sheenRoughnessTexture"),
              material.sheenRoughnessTexture().toQString()},
          {QStringLiteral("sheenRoughnessUv"),
              textureUvSeparate(material.sheenRoughnessTextureUvTransform())},
          {QStringLiteral("anisotropyTexture"),
              material.anisotropyTexture().toQString()},
          {QStringLiteral("anisotropyTextureUv"),
              textureUvSeparate(material.anisotropyTextureUvTransform())},
          {QStringLiteral("emissionTexture"),
              material.emissionTexture().toQString()},
          {QStringLiteral("occlusionTexture"),
              material.occlusionTexture().toQString()},
          {QStringLiteral("opacityTexture"),
              material.opacityTexture().toQString()},
          {QStringLiteral("baseColorUv"), textureUv(
              material.baseColorUvScale(),
              material.baseColorUvRotationDegrees(),
              material.baseColorUvOffsetU(), material.baseColorUvOffsetV(),
              material.baseColorUvSet())},
          {QStringLiteral("metallicRoughnessUv"), textureUv(
              material.metallicRoughnessUvScale(),
              material.metallicRoughnessUvRotationDegrees(),
              material.metallicRoughnessUvOffsetU(),
              material.metallicRoughnessUvOffsetV(),
              material.metallicRoughnessUvSet())},
          {QStringLiteral("normalUv"), textureUv(
              material.normalUvScale(), material.normalUvRotationDegrees(),
              material.normalUvOffsetU(), material.normalUvOffsetV(),
              material.normalUvSet())},
          {QStringLiteral("clearcoatNormalUv"), textureUv(
              material.clearcoatNormalUvScale(),
              material.clearcoatNormalUvRotationDegrees(),
              material.clearcoatNormalUvOffsetU(),
              material.clearcoatNormalUvOffsetV(),
              material.clearcoatNormalUvSet())},
          {QStringLiteral("clearcoatTextureUv"), textureUv(
              material.clearcoatTextureUvScale(),
              material.clearcoatTextureUvRotationDegrees(),
              material.clearcoatTextureUvOffsetU(),
              material.clearcoatTextureUvOffsetV(),
              material.clearcoatTextureUvSet())},
          {QStringLiteral("clearcoatRoughnessTextureUv"), textureUv(
              material.clearcoatRoughnessTextureUvScale(),
              material.clearcoatRoughnessTextureUvRotationDegrees(),
              material.clearcoatRoughnessTextureUvOffsetU(),
              material.clearcoatRoughnessTextureUvOffsetV(),
              material.clearcoatRoughnessTextureUvSet())},
          {QStringLiteral("emissionUv"), textureUv(
              material.emissionTextureUvScale(),
              material.emissionTextureUvRotationDegrees(),
              material.emissionTextureUvOffsetU(),
              material.emissionTextureUvOffsetV(),
              material.emissionTextureUvSet())},
          {QStringLiteral("occlusionUv"), textureUv(
              material.occlusionTextureUvScale(),
              material.occlusionTextureUvRotationDegrees(),
              material.occlusionTextureUvOffsetU(),
              material.occlusionTextureUvOffsetV(),
              material.occlusionTextureUvSet())},
          {QStringLiteral("opacityUv"), textureUv(
              material.opacityTextureUvScale(),
              material.opacityTextureUvRotationDegrees(),
              material.opacityTextureUvOffsetU(),
              material.opacityTextureUvOffsetV(),
              material.opacityTextureUvSet())}});
    }
    obj["material.slots"] = materialSlots;
  }
  const QString materialGraphJson = impl_->material_.materialGraphJson().toQString();
  if (!materialGraphJson.trimmed().isEmpty()) {
    obj["material.graph"] = materialGraphJson;
  }
  return obj;
}

void Artifact3DLayer::fromJsonProperties(const QJsonObject& obj)
{
  ArtifactAbstractLayer::fromJsonProperties(obj);
  impl_->sourceAssetId_ = QUuid::fromString(
      obj.value(QStringLiteral("model.sourceAssetId")).toString());

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
  if (obj.contains("animation.playbackMode")) {
    impl_->animationPlaybackMode_ = std::clamp(
        obj.value("animation.playbackMode").toInt(0), 0, 2);
    impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  }
  if (obj.contains("animation.speed")) {
    const float speed = static_cast<float>(
        obj.value("animation.speed").toDouble(impl_->animationSpeed_));
    impl_->animationSpeed_ =
        std::isfinite(speed) ? std::clamp(speed, 0.0f, 8.0f) : 1.0f;
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
  impl_->material_.setDiffuseRoughness(finiteClamped(
      static_cast<float>(obj.value("material.diffuseRoughness").toDouble(
          impl_->material_.diffuseRoughness())),
      impl_->material_.diffuseRoughness(), 0.0f, 1.0f));
  impl_->material_.setSpecular(
      finiteClamped(static_cast<float>(obj.value("material.specular").toDouble(impl_->material_.specular())),
                    impl_->material_.specular(), 0.0f, 1.0f));
  const QJsonObject specularTint = obj.value("material.specularTint").toObject();
  if (!specularTint.isEmpty()) {
    impl_->material_.setSpecularTint(QColor::fromRgbF(
        finiteClamped(static_cast<float>(specularTint.value("r").toDouble(1.0)),
                      1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(specularTint.value("g").toDouble(1.0)),
                      1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(specularTint.value("b").toDouble(1.0)),
                      1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(specularTint.value("a").toDouble(1.0)),
                      1.0f, 0.0f, 1.0f)));
  }
  impl_->material_.setThinFilm(
      finiteClamped(static_cast<float>(obj.value(
          "material.thinFilmThickness").toDouble(
              impl_->material_.thinFilmThickness())),
          impl_->material_.thinFilmThickness(), 0.0f, 1000.0f),
      finiteClamped(static_cast<float>(obj.value(
          "material.thinFilmIOR").toDouble(impl_->material_.thinFilmIOR())),
          impl_->material_.thinFilmIOR(), 1.0f, 2.5f));
  impl_->material_.setIOR(
      finiteClamped(static_cast<float>(obj.value("material.ior").toDouble(impl_->material_.ior())),
                    impl_->material_.ior(), 1.0f, 3.0f));
  impl_->material_.setTransmission(
      finiteClamped(static_cast<float>(obj.value("material.transmission").toDouble(impl_->material_.transmission())),
                    impl_->material_.transmission(), 0.0f, 1.0f));
  impl_->material_.setThinWall(
      obj.value(QStringLiteral("material.thinWall")).toBool(
          impl_->material_.thinWall()));
  impl_->material_.setClearcoat(
      finiteClamped(static_cast<float>(obj.value("material.clearcoat").toDouble(impl_->material_.clearcoat())),
                    impl_->material_.clearcoat(), 0.0f, 1.0f));
  impl_->material_.setClearcoatRoughness(
      finiteClamped(static_cast<float>(obj.value("material.clearcoatRoughness").toDouble(impl_->material_.clearcoatRoughness())),
                    impl_->material_.clearcoatRoughness(), 0.0f, 1.0f));
  impl_->material_.setClearcoatIOR(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatIOR").toDouble(
          impl_->material_.clearcoatIOR())),
      impl_->material_.clearcoatIOR(), 1.0f, 2.0f));
  const QJsonObject clearcoatTint =
      obj.value("material.clearcoatTint").toObject();
  if (!clearcoatTint.isEmpty()) {
    impl_->material_.setClearcoatTint(QColor::fromRgbF(
        finiteClamped(static_cast<float>(clearcoatTint.value(
            QStringLiteral("r")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(clearcoatTint.value(
            QStringLiteral("g")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(clearcoatTint.value(
            QStringLiteral("b")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(clearcoatTint.value(
            QStringLiteral("a")).toDouble(1.0)), 1.0f, 0.0f, 1.0f)));
  }
  impl_->material_.setClearcoatNormalStrength(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatNormalStrength").toDouble(
          impl_->material_.clearcoatNormalStrength())),
      impl_->material_.clearcoatNormalStrength(), 0.0f, 1.0f));
  impl_->material_.setSheen(
      finiteClamped(static_cast<float>(obj.value("material.sheen").toDouble(impl_->material_.sheen())),
                    impl_->material_.sheen(), 0.0f, 1.0f));
  impl_->material_.setSubsurface(finiteClamped(
      static_cast<float>(obj.value("material.subsurface").toDouble(
          impl_->material_.subsurface())),
      impl_->material_.subsurface(), 0.0f, 1.0f));
  impl_->material_.setSubsurfaceIOR(finiteClamped(
      static_cast<float>(obj.value("material.subsurfaceIOR").toDouble(
          impl_->material_.subsurfaceIOR())),
      impl_->material_.subsurfaceIOR(), 1.0f, 2.0f));
  impl_->material_.setSubsurfaceAnisotropy(finiteClamped(
      static_cast<float>(obj.value("material.subsurfaceAnisotropy").toDouble(
          impl_->material_.subsurfaceAnisotropy())),
      impl_->material_.subsurfaceAnisotropy(), -0.9f, 0.9f));
  const QJsonObject subsurfaceColor =
      obj.value("material.subsurfaceColor").toObject();
  if (!subsurfaceColor.isEmpty()) {
    impl_->material_.setSubsurfaceColor(QColor::fromRgbF(
        finiteClamped(static_cast<float>(subsurfaceColor.value(
            QStringLiteral("r")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(subsurfaceColor.value(
            QStringLiteral("g")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(subsurfaceColor.value(
            QStringLiteral("b")).toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(subsurfaceColor.value(
            QStringLiteral("a")).toDouble(1.0)), 1.0f, 0.0f, 1.0f)));
  }
  const QJsonObject subsurfaceRadius =
      obj.value("material.subsurfaceRadius").toObject();
  if (!subsurfaceRadius.isEmpty()) {
    impl_->material_.setSubsurfaceRadius(QVector3D(
        finiteClamped(static_cast<float>(subsurfaceRadius.value("r").toDouble(1.0)),
                      1.0f, 0.0f, 10.0f),
        finiteClamped(static_cast<float>(subsurfaceRadius.value("g").toDouble(0.2)),
                      0.2f, 0.0f, 10.0f),
        finiteClamped(static_cast<float>(subsurfaceRadius.value("b").toDouble(0.1)),
                      0.1f, 0.0f, 10.0f)));
  }
  impl_->material_.setAnisotropy(
      finiteClamped(
          static_cast<float>(obj.value("material.anisotropy").toDouble(
              impl_->material_.anisotropy())),
          impl_->material_.anisotropy(), -1.0f, 1.0f),
      finiteClamped(static_cast<float>(obj.value(
          "material.anisotropyRotationDegrees").toDouble(
              impl_->material_.anisotropyRotationDegrees())),
          impl_->material_.anisotropyRotationDegrees(), -180.0f, 180.0f));
  if (obj.contains("material.anisotropyTexture")) {
    impl_->material_.setAnisotropyTexture(ArtifactCore::UniString::fromQString(
        obj.value("material.anisotropyTexture").toString()));
  }
  if (obj.contains("material.anisotropyTextureUv")) {
    const QJsonObject uv = obj.value("material.anisotropyTextureUv").toObject();
    auto transform = impl_->material_.anisotropyTextureUvTransform();
    transform.scaleU = finiteClamped(static_cast<float>(uv.value("scaleU").toDouble(transform.scaleU)), transform.scaleU, 0.0001f, 10000.0f);
    transform.scaleV = finiteClamped(static_cast<float>(uv.value("scaleV").toDouble(transform.scaleV)), transform.scaleV, 0.0001f, 10000.0f);
    transform.rotationDegrees = finiteClamped(static_cast<float>(uv.value("rotation").toDouble(transform.rotationDegrees)), transform.rotationDegrees, -360.0f, 360.0f);
    transform.offsetU = finiteClamped(static_cast<float>(uv.value("offsetU").toDouble(transform.offsetU)), transform.offsetU, -1000000.0f, 1000000.0f);
    transform.offsetV = finiteClamped(static_cast<float>(uv.value("offsetV").toDouble(transform.offsetV)), transform.offsetV, -1000000.0f, 1000000.0f);
    transform.uvSet = std::clamp(uv.value("uvSet").toInt(transform.uvSet), 0, 1);
    impl_->material_.setAnisotropyTextureUvTransform(transform);
  }
  impl_->material_.setSheenRoughness(finiteClamped(
      static_cast<float>(obj.value("material.sheenRoughness").toDouble(
          impl_->material_.sheenRoughness())),
      impl_->material_.sheenRoughness(), 0.0f, 1.0f));
  const QJsonObject sheenTint = obj.value("material.sheenTint").toObject();
  if (!sheenTint.isEmpty()) {
    impl_->material_.setSheenTint(QColor::fromRgbF(
        finiteClamped(static_cast<float>(sheenTint.value("r").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(sheenTint.value("g").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(sheenTint.value("b").toDouble(1.0)), 1.0f, 0.0f, 1.0f),
        finiteClamped(static_cast<float>(sheenTint.value("a").toDouble(1.0)), 1.0f, 0.0f, 1.0f)));
  }
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
  impl_->material_.setNormalUvRotationDegrees(
      finiteClamped(static_cast<float>(obj.value("material.normalUvRotation").toDouble(
          impl_->material_.normalUvRotationDegrees())),
          impl_->material_.normalUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setNormalUvScale(finiteClamped(
      static_cast<float>(obj.value("material.normalUvScale").toDouble(
          impl_->material_.normalUvScale())),
      impl_->material_.normalUvScale(), 0.01f, 10.0f));
  impl_->material_.setNormalUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.normalUvOffsetU").toDouble(
          impl_->material_.normalUvOffsetU())),
      impl_->material_.normalUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setNormalUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.normalUvOffsetV").toDouble(
          impl_->material_.normalUvOffsetV())),
      impl_->material_.normalUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setNormalUvSet(
      obj.value("material.normalUvSet").toInt(0));
  impl_->material_.setOcclusionStrength(
      finiteClamped(static_cast<float>(obj.value("material.occlusionStrength").toDouble(impl_->material_.occlusionStrength())),
                    impl_->material_.occlusionStrength(), 0.0f, 1.0f));
  impl_->material_.setOcclusionTextureUvScale(finiteClamped(
      static_cast<float>(obj.value("material.occlusionTextureUvScale")
          .toDouble(impl_->material_.occlusionTextureUvScale())),
      impl_->material_.occlusionTextureUvScale(), 0.01f, 10.0f));
  impl_->material_.setOcclusionTextureUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.occlusionTextureUvOffsetU")
          .toDouble(impl_->material_.occlusionTextureUvOffsetU())),
      impl_->material_.occlusionTextureUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setOcclusionTextureUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.occlusionTextureUvOffsetV")
          .toDouble(impl_->material_.occlusionTextureUvOffsetV())),
      impl_->material_.occlusionTextureUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setOcclusionTextureUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.occlusionTextureUvRotation")
          .toDouble(impl_->material_.occlusionTextureUvRotationDegrees())),
      impl_->material_.occlusionTextureUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setOcclusionTextureUvSet(
      obj.value("material.occlusionTextureUvSet").toInt(0));
  impl_->material_.setUvOffsetU(
      finiteClamped(static_cast<float>(obj.value("material.uv.offsetU").toDouble(impl_->material_.uvOffsetU())),
                    impl_->material_.uvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setUvOffsetV(
      finiteClamped(static_cast<float>(obj.value("material.uv.offsetV").toDouble(impl_->material_.uvOffsetV())),
                    impl_->material_.uvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setUvScaleU(
      finiteClamped(static_cast<float>(obj.value("material.uv.scaleU").toDouble(impl_->material_.uvScaleU())),
                    impl_->material_.uvScaleU(), 0.01f, 10.0f));
  impl_->material_.setUvScaleV(
      finiteClamped(static_cast<float>(obj.value("material.uv.scaleV").toDouble(impl_->material_.uvScaleV())),
                    impl_->material_.uvScaleV(), 0.01f, 10.0f));
  impl_->material_.setUvRotationDegrees(
      finiteClamped(static_cast<float>(obj.value("material.uv.rotation").toDouble(impl_->material_.uvRotationDegrees())),
                    impl_->material_.uvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setEnvironmentIntensity(
      finiteClamped(static_cast<float>(obj.value("material.envIntensity").toDouble(impl_->material_.environmentIntensity())),
                    impl_->material_.environmentIntensity(), 0.0f, 4.0f));

  const QString baseColorTexture = obj.contains("material.baseColorTexture")
                                       ? obj.value("material.baseColorTexture").toString()
                                       : QString();
  impl_->material_.setBaseColorTexture(ArtifactCore::UniString::fromQString(baseColorTexture));
  impl_->material_.setBaseColorUvScale(finiteClamped(
      static_cast<float>(obj.value("material.baseColorUvScale").toDouble(
          impl_->material_.baseColorUvScale())),
      impl_->material_.baseColorUvScale(), 0.01f, 10.0f));
  impl_->material_.setBaseColorUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.baseColorUvRotation")
          .toDouble(impl_->material_.baseColorUvRotationDegrees())),
      impl_->material_.baseColorUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setBaseColorUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.baseColorUvOffsetU")
          .toDouble(impl_->material_.baseColorUvOffsetU())),
      impl_->material_.baseColorUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setBaseColorUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.baseColorUvOffsetV")
          .toDouble(impl_->material_.baseColorUvOffsetV())),
      impl_->material_.baseColorUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setBaseColorUvSet(
      obj.value("material.baseColorUvSet").toInt(0));

  const QString metallicRoughnessTexture =
      obj.value("material.metallicRoughnessTexture").toString();
  impl_->material_.setMetallicRoughnessTexture(
      ArtifactCore::UniString::fromQString(metallicRoughnessTexture));
  impl_->material_.setMetallicTexture(ArtifactCore::UniString::fromQString(
      obj.value("material.metallicTexture").toString()));
  impl_->material_.setRoughnessTexture(ArtifactCore::UniString::fromQString(
      obj.value("material.roughnessTexture").toString()));
  const auto restoreIndependentTextureUv = [&](const char* key, auto setter,
                                                auto getter) {
    QJsonObject uv = obj.value(QString::fromLatin1(key)).toObject();
    if (uv.isEmpty()) return;
    auto transform = (impl_->material_.*getter)();
    const auto readUv = [&](const char* field, float current,
                            float minimum, float maximum) {
      return finiteClamped(static_cast<float>(uv.value(
          QString::fromLatin1(field)).toDouble(current)), current,
          minimum, maximum);
    };
    transform.scaleU = readUv("scaleU", transform.scaleU, 0.01f, 10.0f);
    transform.scaleV = readUv("scaleV", transform.scaleV, 0.01f, 10.0f);
    transform.rotationDegrees = readUv(
        "rotation", transform.rotationDegrees, -360.0f, 360.0f);
    transform.offsetU = readUv("offsetU", transform.offsetU, -10.0f, 10.0f);
    transform.offsetV = readUv("offsetV", transform.offsetV, -10.0f, 10.0f);
    transform.uvSet = std::clamp(
        uv.value(QStringLiteral("uvSet")).toInt(transform.uvSet), 0, 1);
    (impl_->material_.*setter)(transform);
  };
  restoreIndependentTextureUv("material.metallicTextureUv",
      &ArtifactCore::Material::setMetallicTextureUvTransform,
      &ArtifactCore::Material::metallicTextureUvTransform);
  restoreIndependentTextureUv("material.roughnessTextureUv",
      &ArtifactCore::Material::setRoughnessTextureUvTransform,
      &ArtifactCore::Material::roughnessTextureUvTransform);
  restoreIndependentTextureUv("material.anisotropyTextureUv",
      &ArtifactCore::Material::setAnisotropyTextureUvTransform,
      &ArtifactCore::Material::anisotropyTextureUvTransform);
  impl_->material_.setMetallicRoughnessUvScale(finiteClamped(
      static_cast<float>(obj.value("material.metallicRoughnessUvScale")
          .toDouble(impl_->material_.metallicRoughnessUvScale())),
      impl_->material_.metallicRoughnessUvScale(), 0.01f, 10.0f));
  impl_->material_.setMetallicRoughnessUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.metallicRoughnessUvOffsetU")
          .toDouble(impl_->material_.metallicRoughnessUvOffsetU())),
      impl_->material_.metallicRoughnessUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setMetallicRoughnessUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.metallicRoughnessUvOffsetV")
          .toDouble(impl_->material_.metallicRoughnessUvOffsetV())),
      impl_->material_.metallicRoughnessUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setMetallicRoughnessUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.metallicRoughnessUvRotation")
          .toDouble(impl_->material_.metallicRoughnessUvRotationDegrees())),
      impl_->material_.metallicRoughnessUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setMetallicRoughnessUvSet(
      obj.value("material.metallicRoughnessUvSet").toInt(0));

  const QString normalTexture = obj.value("material.normalTexture").toString();
  impl_->material_.setNormalTexture(ArtifactCore::UniString::fromQString(normalTexture));
  impl_->material_.setClearcoatNormalTexture(
      ArtifactCore::UniString::fromQString(
          obj.value("material.clearcoatNormalTexture").toString()));
  impl_->material_.setClearcoatNormalUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatNormalUvRotation")
          .toDouble(impl_->material_.clearcoatNormalUvRotationDegrees())),
      impl_->material_.clearcoatNormalUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setClearcoatNormalUvScale(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatNormalUvScale")
          .toDouble(impl_->material_.clearcoatNormalUvScale())),
      impl_->material_.clearcoatNormalUvScale(), 0.01f, 10.0f));
  impl_->material_.setClearcoatNormalUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatNormalUvOffsetU")
          .toDouble(impl_->material_.clearcoatNormalUvOffsetU())),
      impl_->material_.clearcoatNormalUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setClearcoatNormalUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatNormalUvOffsetV")
          .toDouble(impl_->material_.clearcoatNormalUvOffsetV())),
      impl_->material_.clearcoatNormalUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setClearcoatNormalUvSet(
      obj.value("material.clearcoatNormalUvSet").toInt(0));
  impl_->material_.setClearcoatTexture(ArtifactCore::UniString::fromQString(
      obj.value("material.clearcoatTexture").toString()));
  impl_->material_.setClearcoatTextureUvScale(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatTextureUvScale")
          .toDouble(impl_->material_.clearcoatTextureUvScale())),
      impl_->material_.clearcoatTextureUvScale(), 0.01f, 10.0f));
  impl_->material_.setClearcoatTextureUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatTextureUvRotation")
          .toDouble(impl_->material_.clearcoatTextureUvRotationDegrees())),
      impl_->material_.clearcoatTextureUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setClearcoatTextureUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatTextureUvOffsetU")
          .toDouble(impl_->material_.clearcoatTextureUvOffsetU())),
      impl_->material_.clearcoatTextureUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setClearcoatTextureUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.clearcoatTextureUvOffsetV")
          .toDouble(impl_->material_.clearcoatTextureUvOffsetV())),
      impl_->material_.clearcoatTextureUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setClearcoatTextureUvSet(
      obj.value("material.clearcoatTextureUvSet").toInt(0));
  impl_->material_.setSheenColorTexture(ArtifactCore::UniString::fromQString(
      obj.value("material.sheenColorTexture").toString()));
  if (obj.contains("material.sheenColorTextureUv")) {
    const QJsonObject uv = obj.value("material.sheenColorTextureUv").toObject();
    auto transform = impl_->material_.sheenColorTextureUvTransform();
    transform.scaleU = finiteClamped(static_cast<float>(uv.value("scaleU").toDouble(transform.scaleU)), transform.scaleU, 0.0001f, 10000.0f);
    transform.scaleV = finiteClamped(static_cast<float>(uv.value("scaleV").toDouble(transform.scaleV)), transform.scaleV, 0.0001f, 10000.0f);
    transform.rotationDegrees = finiteClamped(static_cast<float>(uv.value("rotation").toDouble(transform.rotationDegrees)), transform.rotationDegrees, -360.0f, 360.0f);
    transform.offsetU = finiteClamped(static_cast<float>(uv.value("offsetU").toDouble(transform.offsetU)), transform.offsetU, -1000000.0f, 1000000.0f);
    transform.offsetV = finiteClamped(static_cast<float>(uv.value("offsetV").toDouble(transform.offsetV)), transform.offsetV, -1000000.0f, 1000000.0f);
    transform.uvSet = std::clamp(uv.value("uvSet").toInt(transform.uvSet), 0, 1);
    impl_->material_.setSheenColorTextureUvTransform(transform);
  }
  if (obj.contains("material.sheenRoughnessTexture")) {
    impl_->material_.setSheenRoughnessTexture(
        ArtifactCore::UniString::fromQString(
            obj.value("material.sheenRoughnessTexture").toString()));
  }
  if (obj.contains("material.sheenRoughnessTextureUv")) {
    const QJsonObject uv = obj.value("material.sheenRoughnessTextureUv").toObject();
    auto transform = impl_->material_.sheenRoughnessTextureUvTransform();
    transform.scaleU = static_cast<float>(uv.value("scaleU").toDouble(transform.scaleU));
    transform.scaleV = static_cast<float>(uv.value("scaleV").toDouble(transform.scaleV));
    transform.rotationDegrees = static_cast<float>(uv.value("rotation").toDouble(transform.rotationDegrees));
    transform.offsetU = static_cast<float>(uv.value("offsetU").toDouble(transform.offsetU));
    transform.offsetV = static_cast<float>(uv.value("offsetV").toDouble(transform.offsetV));
    transform.uvSet = std::clamp(uv.value("uvSet").toInt(transform.uvSet), 0, 1);
    impl_->material_.setSheenRoughnessTextureUvTransform(transform);
  }

  const QString emissionTexture = obj.value("material.emissionTexture").toString();
  impl_->material_.setEmissionTexture(ArtifactCore::UniString::fromQString(emissionTexture));
  impl_->material_.setEmissionTextureUvScale(finiteClamped(
      static_cast<float>(obj.value("material.emissionTextureUvScale")
          .toDouble(impl_->material_.emissionTextureUvScale())),
      impl_->material_.emissionTextureUvScale(), 0.01f, 10.0f));
  impl_->material_.setEmissionTextureUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.emissionTextureUvOffsetU")
          .toDouble(impl_->material_.emissionTextureUvOffsetU())),
      impl_->material_.emissionTextureUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setEmissionTextureUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.emissionTextureUvOffsetV")
          .toDouble(impl_->material_.emissionTextureUvOffsetV())),
      impl_->material_.emissionTextureUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setEmissionTextureUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.emissionTextureUvRotation")
          .toDouble(impl_->material_.emissionTextureUvRotationDegrees())),
      impl_->material_.emissionTextureUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setEmissionTextureUvSet(
      obj.value("material.emissionTextureUvSet").toInt(0));

  const QString occlusionTexture = obj.value("material.occlusionTexture").toString();
  impl_->material_.setOcclusionTexture(ArtifactCore::UniString::fromQString(occlusionTexture));

  const QString opacityTexture = obj.value("material.opacityTexture").toString();
  impl_->material_.setOpacityTexture(ArtifactCore::UniString::fromQString(opacityTexture));
  impl_->material_.setOpacityTextureUvScale(finiteClamped(
      static_cast<float>(obj.value("material.opacityTextureUvScale")
          .toDouble(impl_->material_.opacityTextureUvScale())),
      impl_->material_.opacityTextureUvScale(), 0.01f, 10.0f));
  impl_->material_.setOpacityTextureUvOffsetU(finiteClamped(
      static_cast<float>(obj.value("material.opacityTextureUvOffsetU")
          .toDouble(impl_->material_.opacityTextureUvOffsetU())),
      impl_->material_.opacityTextureUvOffsetU(), -10.0f, 10.0f));
  impl_->material_.setOpacityTextureUvOffsetV(finiteClamped(
      static_cast<float>(obj.value("material.opacityTextureUvOffsetV")
          .toDouble(impl_->material_.opacityTextureUvOffsetV())),
      impl_->material_.opacityTextureUvOffsetV(), -10.0f, 10.0f));
  impl_->material_.setOpacityTextureUvRotationDegrees(finiteClamped(
      static_cast<float>(obj.value("material.opacityTextureUvRotation")
          .toDouble(impl_->material_.opacityTextureUvRotationDegrees())),
      impl_->material_.opacityTextureUvRotationDegrees(), -360.0f, 360.0f));
  impl_->material_.setOpacityTextureUvSet(
      obj.value("material.opacityTextureUvSet").toInt(0));

  // Material graph JSON is stored verbatim; the renderer compiles it on
  // first draw. Invalid payloads are ignored at apply time, never here.
  impl_->material_.setMaterialGraphJson(ArtifactCore::UniString::fromQString(
      obj.value("material.graph").toString()));

  const QJsonArray savedMaterialSlots =
      obj.value(QStringLiteral("material.slots")).toArray();
  QVector<quint8> restoredSlots(
      static_cast<qsizetype>(impl_->meshMaterials_.size()), 0);
  const auto readColor = [](const QJsonValue& value, const QColor& fallback,
                            bool includeAlpha) {
    const QJsonArray components = value.toArray();
    if (components.size() < (includeAlpha ? 4 : 3)) return fallback;
    const auto component = [&](int index, float defaultValue) {
      const double number = components[index].toDouble(defaultValue);
      return std::isfinite(number)
          ? static_cast<float>(std::clamp(number, 0.0, 1.0)) : defaultValue;
    };
    return QColor::fromRgbF(component(0, fallback.redF()),
                            component(1, fallback.greenF()),
                            component(2, fallback.blueF()),
                            includeAlpha ? component(3, fallback.alphaF())
                                         : fallback.alphaF());
  };
  for (qsizetype savedIndex = 0; savedIndex < savedMaterialSlots.size();
       ++savedIndex) {
    const QJsonValue slotValue = savedMaterialSlots[savedIndex];
    if (!slotValue.isObject()) continue;
    const QJsonObject slot = slotValue.toObject();
    const QString savedName = slot.value(QStringLiteral("name")).toString();
    qsizetype materialIndex = -1;
    if (!savedName.isEmpty()) {
      for (qsizetype candidate = 0;
           candidate < static_cast<qsizetype>(impl_->meshMaterials_.size());
           ++candidate) {
        if (!restoredSlots[candidate] &&
            impl_->meshMaterials_[static_cast<std::size_t>(candidate)]
                    .name().toQString() == savedName) {
          materialIndex = candidate;
          break;
        }
      }
    }
    if (materialIndex < 0 &&
        savedIndex < static_cast<qsizetype>(impl_->meshMaterials_.size()) &&
        !restoredSlots[savedIndex]) {
      materialIndex = savedIndex;
    }
    if (materialIndex < 0) continue;
    restoredSlots[materialIndex] = 1;
    auto& material = impl_->meshMaterials_[static_cast<std::size_t>(materialIndex)];
    material.setBaseColor(readColor(slot.value("baseColor"),
                                    material.baseColor(), true));
    const auto readScalar = [&](const char* key, float fallback,
                                float minimum, float maximum) {
      const double number = slot.value(QString::fromLatin1(key)).toDouble(fallback);
      return std::isfinite(number)
          ? static_cast<float>(std::clamp(number,
                static_cast<double>(minimum), static_cast<double>(maximum)))
          : fallback;
    };
    const auto readScalarFromObject = [](const QJsonObject& object,
        const char* key, float fallback, float minimum, float maximum) {
      const double number = object.value(QString::fromLatin1(key))
                                .toDouble(fallback);
      return std::isfinite(number)
          ? static_cast<float>(std::clamp(number,
                static_cast<double>(minimum), static_cast<double>(maximum)))
          : fallback;
    };
    material.setMetallic(readScalar("metallic", material.metallic(), 0.0f, 1.0f));
    material.setRoughness(readScalar("roughness", material.roughness(), 0.0f, 1.0f));
    material.setDiffuseRoughness(readScalar("diffuseRoughness",
        material.diffuseRoughness(), 0.0f, 1.0f));
    material.setSpecular(readScalar("specular", material.specular(), 0.0f, 1.0f));
    material.setSpecularTint(readColor(slot.value("specularTint"),
                                       material.specularTint(), false));
    material.setIOR(readScalar("ior", material.ior(), 1.0f, 3.0f));
    material.setTransmission(readScalar("transmission", material.transmission(),
                                        0.0f, 1.0f));
    material.setThinWall(slot.value(QStringLiteral("thinWall")).toBool(
        material.thinWall()));
    material.setThinFilm(readScalar("thinFilmThickness",
        material.thinFilmThickness(), 0.0f, 1000.0f),
        readScalar("thinFilmIOR", material.thinFilmIOR(), 1.0f, 2.5f));
    material.setClearcoat(readScalar("clearcoat", material.clearcoat(),
                                     0.0f, 1.0f));
    material.setClearcoatRoughness(readScalar("clearcoatRoughness",
        material.clearcoatRoughness(), 0.0f, 1.0f));
    material.setClearcoatIOR(readScalar("clearcoatIOR",
        material.clearcoatIOR(), 1.0f, 2.0f));
    material.setClearcoatTint(readColor(slot.value("clearcoatTint"),
                                        material.clearcoatTint(), false));
    material.setClearcoatNormalStrength(readScalar("clearcoatNormalStrength",
        material.clearcoatNormalStrength(), 0.0f, 1.0f));
    material.setSheen(readScalar("sheen", material.sheen(), 0.0f, 1.0f));
    material.setSheenRoughness(readScalar("sheenRoughness",
        material.sheenRoughness(), 0.0f, 1.0f));
    material.setSheenTint(readColor(slot.value("sheenTint"),
                                    material.sheenTint(), false));
    material.setSubsurface(readScalar("subsurface", material.subsurface(),
                                      0.0f, 1.0f));
    material.setSubsurfaceIOR(readScalar("subsurfaceIOR",
        material.subsurfaceIOR(), 1.0f, 2.0f));
    material.setSubsurfaceAnisotropy(readScalar("subsurfaceAnisotropy",
        material.subsurfaceAnisotropy(), -0.9f, 0.9f));
    material.setSubsurfaceColor(readColor(slot.value("subsurfaceColor"),
                                          material.subsurfaceColor(), false));
    const QJsonArray savedSubsurfaceRadius =
        slot.value(QStringLiteral("subsurfaceRadius")).toArray();
    if (savedSubsurfaceRadius.size() >= 3) {
      const QVector3D currentRadius = material.subsurfaceRadius();
      const auto radiusComponent = [&](int component, float fallback) {
        const double number = savedSubsurfaceRadius[component].toDouble(fallback);
        return std::isfinite(number)
            ? static_cast<float>(std::clamp(number, 0.0, 10.0)) : fallback;
      };
      material.setSubsurfaceRadius(QVector3D(
          radiusComponent(0, currentRadius.x()),
          radiusComponent(1, currentRadius.y()),
          radiusComponent(2, currentRadius.z())));
    }
    material.setAnisotropy(
        readScalar("anisotropy", material.anisotropy(), -1.0f, 1.0f),
        readScalar("anisotropyRotation", material.anisotropyRotationDegrees(),
                   -180.0f, 180.0f));
    if (slot.contains(QStringLiteral("anisotropyTexture"))) {
      material.setAnisotropyTexture(ArtifactCore::UniString::fromQString(
          slot.value(QStringLiteral("anisotropyTexture")).toString()));
    }
    material.setOpacity(readScalar("opacity", material.opacity(), 0.0f, 1.0f));
    const int alphaMode = std::clamp(slot.value("alphaMode").toInt(
        static_cast<int>(material.alphaMode())),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Opaque),
        static_cast<int>(ArtifactCore::MaterialAlphaMode::Blended));
    material.setAlphaMode(static_cast<ArtifactCore::MaterialAlphaMode>(alphaMode));
    material.setAlphaCutoff(readScalar("alphaCutoff", material.alphaCutoff(),
                                       0.0f, 1.0f));
    material.setEmissionColor(readColor(slot.value("emissionColor"),
                                        material.emissionColor(), false));
    material.setEmissionStrength(readScalar("emissionStrength",
        material.emissionStrength(), 0.0f, 100000.0f));
    material.setNormalStrength(readScalar("normalStrength",
        material.normalStrength(), 0.0f, 10.0f));
    material.setOcclusionStrength(readScalar("occlusionStrength",
        material.occlusionStrength(), 0.0f, 1.0f));
    material.setEnvironmentIntensity(readScalar("environmentIntensity",
        material.environmentIntensity(), 0.0f, 4.0f));
    material.setUvOffsetU(readScalar("uvOffsetU", material.uvOffsetU(),
                                     -10.0f, 10.0f));
    material.setUvOffsetV(readScalar("uvOffsetV", material.uvOffsetV(),
                                     -10.0f, 10.0f));
    material.setUvScaleU(readScalar("uvScaleU", material.uvScaleU(),
                                    0.01f, 10.0f));
    material.setUvScaleV(readScalar("uvScaleV", material.uvScaleV(),
                                    0.01f, 10.0f));
    material.setUvRotationDegrees(readScalar("uvRotation",
        material.uvRotationDegrees(), -360.0f, 360.0f));
    const auto restoreUv = [&](const char* key, auto scaleSetter,
        auto rotationSetter, auto offsetUSetter, auto offsetVSetter,
        auto uvSetSetter, float currentScale, float currentRotation,
        float currentOffsetU, float currentOffsetV, int currentUvSet) {
      const QJsonObject uv = slot.value(QString::fromLatin1(key)).toObject();
      if (uv.isEmpty()) return;
      (material.*scaleSetter)(readScalarFromObject(uv, "scale", currentScale,
                                                   0.01f, 10.0f));
      (material.*rotationSetter)(readScalarFromObject(uv, "rotation",
          currentRotation, -360.0f, 360.0f));
      (material.*offsetUSetter)(readScalarFromObject(uv, "offsetU",
          currentOffsetU, -10.0f, 10.0f));
      (material.*offsetVSetter)(readScalarFromObject(uv, "offsetV",
          currentOffsetV, -10.0f, 10.0f));
      (material.*uvSetSetter)(std::clamp(uv.value("uvSet").toInt(currentUvSet),
                                         0, 1));
    };
    const auto restoreIndependentUv = [&](const char* key, auto setter,
                                          auto getter) {
      const QJsonObject uv = slot.value(QString::fromLatin1(key)).toObject();
      if (uv.isEmpty()) return;
      auto transform = (material.*getter)();
      transform.scaleU = readScalarFromObject(
          uv, "scaleU", transform.scaleU, 0.01f, 10.0f);
      transform.scaleV = readScalarFromObject(
          uv, "scaleV", transform.scaleV, 0.01f, 10.0f);
      transform.rotationDegrees = readScalarFromObject(
          uv, "rotation", transform.rotationDegrees, -360.0f, 360.0f);
      transform.offsetU = readScalarFromObject(
          uv, "offsetU", transform.offsetU, -10.0f, 10.0f);
      transform.offsetV = readScalarFromObject(
          uv, "offsetV", transform.offsetV, -10.0f, 10.0f);
      transform.uvSet = std::clamp(
          uv.value(QStringLiteral("uvSet")).toInt(transform.uvSet), 0, 1);
      (material.*setter)(transform);
    };
    restoreUv("baseColorUv", &ArtifactCore::Material::setBaseColorUvScale,
        &ArtifactCore::Material::setBaseColorUvRotationDegrees,
        &ArtifactCore::Material::setBaseColorUvOffsetU,
        &ArtifactCore::Material::setBaseColorUvOffsetV,
        &ArtifactCore::Material::setBaseColorUvSet,
        material.baseColorUvScale(), material.baseColorUvRotationDegrees(),
        material.baseColorUvOffsetU(), material.baseColorUvOffsetV(),
        material.baseColorUvSet());
    restoreUv("metallicRoughnessUv",
        &ArtifactCore::Material::setMetallicRoughnessUvScale,
        &ArtifactCore::Material::setMetallicRoughnessUvRotationDegrees,
        &ArtifactCore::Material::setMetallicRoughnessUvOffsetU,
        &ArtifactCore::Material::setMetallicRoughnessUvOffsetV,
        &ArtifactCore::Material::setMetallicRoughnessUvSet,
        material.metallicRoughnessUvScale(),
        material.metallicRoughnessUvRotationDegrees(),
        material.metallicRoughnessUvOffsetU(),
        material.metallicRoughnessUvOffsetV(),
        material.metallicRoughnessUvSet());
    restoreIndependentUv("metallicTextureUv",
        &ArtifactCore::Material::setMetallicTextureUvTransform,
        &ArtifactCore::Material::metallicTextureUvTransform);
    restoreIndependentUv("roughnessTextureUv",
        &ArtifactCore::Material::setRoughnessTextureUvTransform,
        &ArtifactCore::Material::roughnessTextureUvTransform);
    restoreUv("normalUv", &ArtifactCore::Material::setNormalUvScale,
        &ArtifactCore::Material::setNormalUvRotationDegrees,
        &ArtifactCore::Material::setNormalUvOffsetU,
        &ArtifactCore::Material::setNormalUvOffsetV,
        &ArtifactCore::Material::setNormalUvSet,
        material.normalUvScale(), material.normalUvRotationDegrees(),
        material.normalUvOffsetU(), material.normalUvOffsetV(),
        material.normalUvSet());
    restoreUv("clearcoatNormalUv",
        &ArtifactCore::Material::setClearcoatNormalUvScale,
        &ArtifactCore::Material::setClearcoatNormalUvRotationDegrees,
        &ArtifactCore::Material::setClearcoatNormalUvOffsetU,
        &ArtifactCore::Material::setClearcoatNormalUvOffsetV,
        &ArtifactCore::Material::setClearcoatNormalUvSet,
        material.clearcoatNormalUvScale(),
        material.clearcoatNormalUvRotationDegrees(),
        material.clearcoatNormalUvOffsetU(),
        material.clearcoatNormalUvOffsetV(),
        material.clearcoatNormalUvSet());
    restoreUv("clearcoatTextureUv",
        &ArtifactCore::Material::setClearcoatTextureUvScale,
        &ArtifactCore::Material::setClearcoatTextureUvRotationDegrees,
        &ArtifactCore::Material::setClearcoatTextureUvOffsetU,
        &ArtifactCore::Material::setClearcoatTextureUvOffsetV,
        &ArtifactCore::Material::setClearcoatTextureUvSet,
        material.clearcoatTextureUvScale(),
        material.clearcoatTextureUvRotationDegrees(),
        material.clearcoatTextureUvOffsetU(),
        material.clearcoatTextureUvOffsetV(),
        material.clearcoatTextureUvSet());
    restoreUv("clearcoatRoughnessTextureUv",
        &ArtifactCore::Material::setClearcoatRoughnessTextureUvScale,
        &ArtifactCore::Material::setClearcoatRoughnessTextureUvRotationDegrees,
        &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetU,
        &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetV,
        &ArtifactCore::Material::setClearcoatRoughnessTextureUvSet,
        material.clearcoatRoughnessTextureUvScale(),
        material.clearcoatRoughnessTextureUvRotationDegrees(),
        material.clearcoatRoughnessTextureUvOffsetU(),
        material.clearcoatRoughnessTextureUvOffsetV(),
        material.clearcoatRoughnessTextureUvSet());
    restoreUv("emissionUv",
        &ArtifactCore::Material::setEmissionTextureUvScale,
        &ArtifactCore::Material::setEmissionTextureUvRotationDegrees,
        &ArtifactCore::Material::setEmissionTextureUvOffsetU,
        &ArtifactCore::Material::setEmissionTextureUvOffsetV,
        &ArtifactCore::Material::setEmissionTextureUvSet,
        material.emissionTextureUvScale(),
        material.emissionTextureUvRotationDegrees(),
        material.emissionTextureUvOffsetU(),
        material.emissionTextureUvOffsetV(),
        material.emissionTextureUvSet());
    restoreUv("occlusionUv",
        &ArtifactCore::Material::setOcclusionTextureUvScale,
        &ArtifactCore::Material::setOcclusionTextureUvRotationDegrees,
        &ArtifactCore::Material::setOcclusionTextureUvOffsetU,
        &ArtifactCore::Material::setOcclusionTextureUvOffsetV,
        &ArtifactCore::Material::setOcclusionTextureUvSet,
        material.occlusionTextureUvScale(),
        material.occlusionTextureUvRotationDegrees(),
        material.occlusionTextureUvOffsetU(),
        material.occlusionTextureUvOffsetV(),
        material.occlusionTextureUvSet());
    restoreUv("opacityUv", &ArtifactCore::Material::setOpacityTextureUvScale,
        &ArtifactCore::Material::setOpacityTextureUvRotationDegrees,
        &ArtifactCore::Material::setOpacityTextureUvOffsetU,
        &ArtifactCore::Material::setOpacityTextureUvOffsetV,
        &ArtifactCore::Material::setOpacityTextureUvSet,
        material.opacityTextureUvScale(),
        material.opacityTextureUvRotationDegrees(),
        material.opacityTextureUvOffsetU(),
        material.opacityTextureUvOffsetV(),
        material.opacityTextureUvSet());
    restoreIndependentUv("sheenColorUv",
        &ArtifactCore::Material::setSheenColorTextureUvTransform,
        &ArtifactCore::Material::sheenColorTextureUvTransform);
    restoreIndependentUv("sheenRoughnessUv",
        &ArtifactCore::Material::setSheenRoughnessTextureUvTransform,
        &ArtifactCore::Material::sheenRoughnessTextureUvTransform);
    restoreIndependentUv("anisotropyTextureUv",
        &ArtifactCore::Material::setAnisotropyTextureUvTransform,
        &ArtifactCore::Material::anisotropyTextureUvTransform);
    const auto restoreTexture = [&](const char* key,
                                    void (ArtifactCore::Material::*setter)(
                                        const ArtifactCore::UniString&)) {
      const QString path = slot.value(QString::fromLatin1(key)).toString();
      if (slot.contains(QString::fromLatin1(key))) {
        (material.*setter)(ArtifactCore::UniString::fromQString(path));
      }
    };
    restoreTexture("baseColorTexture", &ArtifactCore::Material::setBaseColorTexture);
    restoreTexture("metallicRoughnessTexture",
        &ArtifactCore::Material::setMetallicRoughnessTexture);
    restoreTexture("metallicTexture",
        &ArtifactCore::Material::setMetallicTexture);
    restoreTexture("roughnessTexture",
        &ArtifactCore::Material::setRoughnessTexture);
    restoreTexture("normalTexture", &ArtifactCore::Material::setNormalTexture);
    restoreTexture("clearcoatNormalTexture",
        &ArtifactCore::Material::setClearcoatNormalTexture);
    restoreTexture("clearcoatTexture",
        &ArtifactCore::Material::setClearcoatTexture);
    restoreTexture("clearcoatRoughnessTexture",
        &ArtifactCore::Material::setClearcoatRoughnessTexture);
    restoreTexture("emissionTexture", &ArtifactCore::Material::setEmissionTexture);
    restoreTexture("occlusionTexture", &ArtifactCore::Material::setOcclusionTexture);
    restoreTexture("opacityTexture", &ArtifactCore::Material::setOpacityTexture);
    restoreTexture("sheenColorTexture",
        &ArtifactCore::Material::setSheenColorTexture);
    restoreTexture("sheenRoughnessTexture",
        &ArtifactCore::Material::setSheenRoughnessTexture);
    restoreTexture("anisotropyTexture",
        &ArtifactCore::Material::setAnisotropyTexture);
  }
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

void Artifact3DLayer::updateSourceSizeFromMesh(const bool updateBounds) {
  if (updateBounds) {
    impl_->mesh_.updateBounds();
  }
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
  const QString templatePath = filePath.trimmed();
  QString normalizedInput = templatePath;
  if (containsExpansionMarker(normalizedInput)) {
    ExpansionContext expansionContext;
    normalizedInput = expandTokens(normalizedInput, expansionContext);
  }
  if (normalizedInput.isEmpty()) return;
  if (!impl_->animationImporter_ || !impl_->meshLoaded_) {
    return;
  }

  const auto updateWholeMeshAtTime = [&]() {
    auto evaluatedMesh = impl_->animationImporter_->importMeshFromFileAtTime(
        UniString(normalizedInput), time, clipIndex);
    if (!evaluatedMesh || evaluatedMesh->vertexCount() <= 0) {
      return false;
    }
    impl_->mesh_ = *evaluatedMesh;
    for (int shapeIndex = 0;
         shapeIndex < impl_->mesh_.blendShapes().size(); ++shapeIndex) {
      const auto overrideIt = impl_->blendShapeWeightOverrides_.constFind(
          impl_->mesh_.blendShapes()[shapeIndex].name);
      if (overrideIt != impl_->blendShapeWeightOverrides_.constEnd()) {
        impl_->mesh_.setBlendShapeWeight(shapeIndex, overrideIt.value());
      }
    }
    centerMeshPositions(impl_->mesh_);
    updateSourceSizeFromMesh(false);
    return true;
  };

  // Node-only and blend-shape animation still use the existing timed import
  // path. The retained ufbx source scene avoids reparsing the file, while the
  // skin-only path below updates the existing mesh in place.
  if (impl_->mesh_.skinBones().isEmpty() ||
      !impl_->mesh_.blendShapes().isEmpty()) {
    if (!updateWholeMeshAtTime()) {
      qWarning() << "[Artifact3DLayer] Timed model evaluation failed:"
                 << normalizedInput << impl_->animationImporter_->lastError();
      return;
    }
    Q_EMIT changed();
    return;
  }

  if (!impl_->animationImporter_->updateSkinPose(
          UniString(normalizedInput), time, clipIndex, impl_->mesh_)) {
    if (updateWholeMeshAtTime()) {
      Q_EMIT changed();
      return;
    }
    qWarning() << "[Artifact3DLayer] Timed model evaluation failed:"
               << normalizedInput << impl_->animationImporter_->lastError();
    return;
  }
  const int clipCount = static_cast<int>(impl_->mesh_.skinAnimationClips().size());
  impl_->skinAnimationClipIndex_ = clipCount > 0
      ? std::clamp(clipIndex, 0, clipCount - 1)
      : std::max(0, clipIndex);
  impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
  updateSourceSizeFromMesh(false);
  Q_EMIT changed();
}

void Artifact3DLayer::setAnimationTime(const double time, const int clipIndex)
{
  if (impl_->sourcePath_.isEmpty() || !impl_->meshLoaded_) {
    return;
  }
  loadFromFileAtTime(impl_->sourcePath_, time, clipIndex);
}

void Artifact3DLayer::evaluateSkinAnimation()
{
  if (!impl_->meshLoaded_ || impl_->updatingSkinAnimation_) {
    return;
  }
  const int64_t frame = currentFrame();
  if (impl_->sourcePath_.isEmpty() ||
      impl_->mesh_.skinAnimationClips().isEmpty() ||
      !impl_->skinAnimationEnabled_ ||
      impl_->lastSkinAnimationFrame_ == frame) {
    return;
  }
  const int clipCount = static_cast<int>(
      impl_->mesh_.skinAnimationClips().size());
  impl_->skinAnimationClipIndex_ = std::clamp(
      impl_->skinAnimationClipIndex_, 0, clipCount - 1);
  impl_->updatingSkinAnimation_ = true;
  const auto* clip = impl_->mesh_.skinAnimationClip(
      impl_->skinAnimationClipIndex_);
  if (clip && std::isfinite(clip->timeBegin) &&
      std::isfinite(clip->timeEnd) && clip->timeEnd > clip->timeBegin) {
    const double compositionFps = std::isfinite(compositionFrameRate()) &&
                                  compositionFrameRate() > 0.0
          ? compositionFrameRate()
          : 30.0;
    const double duration = clip->timeEnd - clip->timeBegin;
    const double requestedTime = static_cast<double>(frame) / compositionFps;
    const double speed = std::isfinite(impl_->animationSpeed_)
        ? std::clamp(impl_->animationSpeed_, 0.0f, 8.0f) : 1.0f;
    const double local = std::isfinite(requestedTime)
        ? std::max(0.0, requestedTime - clip->timeBegin) *
          static_cast<double>(speed)
        : 0.0;
    double relativeTime = std::fmod(local, duration);
    if (impl_->animationPlaybackMode_ == 1) {
      relativeTime = std::min(local, duration);
    } else if (impl_->animationPlaybackMode_ == 2) {
      const double span = duration * 2.0;
      const double phase = span > 0.0 ? std::fmod(local, span) : 0.0;
      relativeTime = phase <= duration ? phase : span - phase;
    }
    setAnimationTime(clip->timeBegin + relativeTime,
                     impl_->skinAnimationClipIndex_);
  }
  impl_->updatingSkinAnimation_ = false;
  impl_->lastSkinAnimationFrame_ = frame;
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

const ArtifactCore::Material& Artifact3DLayer::material() const
{
  return impl_->material_;
}

const std::vector<ArtifactCore::Material>& Artifact3DLayer::meshMaterials() const
{
  return impl_->meshMaterials_;
}

const std::vector<QMatrix4x4>& Artifact3DLayer::meshLocalTransforms() const
{
  return impl_->meshLocalTransforms_;
}

bool Artifact3DLayer::hasExportableUsdSource() const
{
  return impl_->animationImporter_ &&
         impl_->animationImporter_->hasLoadedUsdStage();
}

bool Artifact3DLayer::exportSourceUsd(const QString& outputPath) const
{
  if (!impl_->animationImporter_ ||
      !impl_->animationImporter_->hasLoadedUsdStage()) {
    qWarning() << "[Artifact3DLayer] USD export skipped: source is not a USD file"
               << impl_->sourcePath_;
    return false;
  }
  // An empty destination overwrites the file the stage was opened from; any
  // other path is written as a copy and the retained stage keeps its own name.
  const QString target = outputPath.isEmpty()
                             ? impl_->animationImporter_->loadedUsdPath()
                             : outputPath;
  if (!impl_->animationImporter_->exportLoadedUsdStage(
          ArtifactCore::UniString::fromQString(target))) {
    qWarning() << "[Artifact3DLayer] USD export failed:"
               << impl_->animationImporter_->lastError();
    return false;
  }
  qInfo() << "[Artifact3DLayer] Exported USD source to" << target;
  return true;
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
  evaluateSkinAnimation();
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

  const bool isPointCloud = impl_->mesh_.polygonCount() == 0 && !positions->data().isEmpty();
  const bool needsWireOverlay =
      impl_->renderMode_ == ModelRenderMode::Wireframe || impl_->wireOverlay_;
  // The solid path hands the mesh to the GPU renderer as-is; world-space overlay
  // vertices are only needed for the wire/point/normal overlays. Populate the
  // scratch buffer on demand so the common solid-without-overlay case does no
  // per-vertex transform work or allocation at all.
  QVector<QVector3D> &transformedVertices = impl_->overlayVertices_;
  const qsizetype vertexCount = positions->data().size();
  if (needsWireOverlay || isPointCloud) {
    if (impl_->overlayVertexCount_ != vertexCount) {
      transformedVertices.resize(vertexCount);
      impl_->overlayVertexCount_ = vertexCount;
    }
    ArtifactCore::Parallel::ForSize(
        0, static_cast<size_t>(vertexCount), static_cast<size_t>(vertexCount),
        [&](size_t index) {
      const qsizetype vertexIndex = static_cast<qsizetype>(index);
      transformedVertices[vertexIndex] =
          modelMatrix.map(positions->data()[vertexIndex]);
    });
  }

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
    for (qsizetype index = 0; index < positions->data().size(); ++index) {
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
    if (impl_->meshMaterials_.size() > 1) {
      // Multi-primitive source (a USD stage with several UsdGeomMesh prims):
      // the geometry is already concatenated into mesh_ and split by material
      // slots, so one instance per source mesh reproduces the hierarchy while
      // the renderer issues one draw per material span.
      const std::size_t meshCount =
          impl_->meshUsesSingleInstanceForMaterialSlots_
              ? 1u
              : std::min(impl_->meshLocalTransforms_.size(),
                         impl_->meshMaterials_.size());
      if (meshCount > 1 ||
          (meshCount == 1 &&
           impl_->meshUsesSingleInstanceForMaterialSlots_)) {
        std::vector<ArtifactCore::InstanceData> instances;
        instances.reserve(meshCount);
        const float layerAlpha = std::clamp(opacity(), 0.0f, 1.0f);
        for (std::size_t meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
          const QMatrix4x4 combined =
              impl_->meshUsesSingleInstanceForMaterialSlots_
                  ? modelMatrix
                  : modelMatrix * impl_->meshLocalTransforms_[meshIndex];
          ArtifactCore::InstanceData instance{};
          const float* modelData = combined.constData();
          for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
              instance.transform[row * 4 + col] = modelData[col * 4 + row];
              instance.previousTransform[row * 4 + col] =
                  instance.transform[row * 4 + col];
            }
          }
          instance.color[0] = impl_->material_.baseColor().redF();
          instance.color[1] = impl_->material_.baseColor().greenF();
          instance.color[2] = impl_->material_.baseColor().blueF();
          instance.color[3] = layerAlpha;
          instance.weight = 1.0f;
          instance.timeOffset = static_cast<float>(solidShadingMode);
          instances.push_back(instance);
        }
        renderer->drawMeshMulti(cacheKey, impl_->mesh_, impl_->meshMaterials_,
                                instances, opacity(), solidShadingMode);
        traceResult(
            QStringLiteral("mesh-multi-submitted"),
            QStringLiteral("meshes=%1 shading=%2")
                .arg(static_cast<qulonglong>(meshCount))
                .arg(solidShadingMode));
        if (impl_->wireOverlay_) {
          drawEdges(FloatColor{0.04f, 0.05f, 0.06f, opacity() * 0.72f}, 1.0f);
        }
        // Normals are an independent overlay: they must stay visible even when
        // the wire overlay is off, otherwise "Face Normal" alone renders nothing.
        if (impl_->faceNormals_) {
          drawFaceNormals();
        }
        if (impl_->vertexNormals_) {
          drawVertexNormals();
        }
        return;
      }
    }
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
    }
    // Normals are an independent overlay: they must stay visible even when
    // the wire overlay is off, otherwise "Face Normal" alone renders nothing.
    if (impl_->faceNormals_) {
      drawFaceNormals();
    }
    if (impl_->vertexNormals_) {
      drawVertexNormals();
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

    auto animationPlaybackModeProp = persistentLayerProperty(
        QStringLiteral("animation.playbackMode"), PropertyType::Integer,
        impl_->animationPlaybackMode_, -52);
    animationPlaybackModeProp->setDisplayLabel(QStringLiteral("Playback Mode"));
    animationPlaybackModeProp->setTooltip(QStringLiteral("0 = Loop, 1 = Hold last frame, 2 = PingPong"));
    animationPlaybackModeProp->setHardRange(0, 2);
    renderGroup.addProperty(animationPlaybackModeProp);

    auto animationSpeedProp = persistentLayerProperty(
        QStringLiteral("animation.speed"), PropertyType::Float,
        impl_->animationSpeed_, -51);
    animationSpeedProp->setDisplayLabel(QStringLiteral("Playback Speed"));
    animationSpeedProp->setTooltip(QStringLiteral("Clip playback speed multiplier (0 = frozen)"));
    animationSpeedProp->setHardRange(0.0, 8.0);
    animationSpeedProp->setSoftRange(0.0, 2.0);
    renderGroup.addProperty(animationSpeedProp);

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
  const auto addIndependentTextureUvProperties = [&] (
      const QString& prefix, const QString& label,
      const ArtifactCore::MaterialTextureUvTransform& transform,
      int priority) {
    const auto addFloat = [&](const QString& field, const QString& fieldLabel,
                              float value, double minimum, double maximum,
                              int order) {
      auto property = persistentLayerProperty(
          prefix + field, PropertyType::Float, value, order);
      property->setDisplayLabel(fieldLabel);
      property->setHardRange(minimum, maximum);
      materialGroup.addProperty(property);
    };
    addFloat(QStringLiteral("scaleU"), label + QStringLiteral(" UV Scale U"),
             transform.scaleU, 0.01, 10.0, priority);
    addFloat(QStringLiteral("scaleV"), label + QStringLiteral(" UV Scale V"),
             transform.scaleV, 0.01, 10.0, priority + 1);
    addFloat(QStringLiteral("rotation"), label + QStringLiteral(" UV Rotation"),
             transform.rotationDegrees, -360.0, 360.0, priority + 2);
    addFloat(QStringLiteral("offsetU"), label + QStringLiteral(" UV Offset U"),
             transform.offsetU, -10.0, 10.0, priority + 3);
    addFloat(QStringLiteral("offsetV"), label + QStringLiteral(" UV Offset V"),
             transform.offsetV, -10.0, 10.0, priority + 4);
    auto uvSet = persistentLayerProperty(
        prefix + QStringLiteral("uvSet"), PropertyType::Integer,
        transform.uvSet, priority + 5);
    uvSet->setDisplayLabel(label + QStringLiteral(" UV Set"));
    uvSet->setHardRange(0, 1);
    materialGroup.addProperty(uvSet);
  };

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

  auto diffuseRoughnessProp = persistentLayerProperty(
      QStringLiteral("material.diffuseRoughness"), PropertyType::Float,
      impl_->material_.diffuseRoughness(), -47);
  diffuseRoughnessProp->setDisplayLabel(QStringLiteral("Diffuse Roughness"));
  diffuseRoughnessProp->setTooltip(QStringLiteral(
      "Roughens the diffuse response independently from specular roughness."));
  diffuseRoughnessProp->setHardRange(0.0, 1.0);
  diffuseRoughnessProp->setSoftRange(0.0, 1.0);
  materialGroup.addProperty(diffuseRoughnessProp);

  auto specularProp = persistentLayerProperty(
      QStringLiteral("material.specular"), PropertyType::Float,
      impl_->material_.specular(), -46);
  specularProp->setDisplayLabel(QStringLiteral("Specular"));
  specularProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(specularProp);
  auto specularTintProp = persistentLayerProperty(
      QStringLiteral("material.specularTint"), PropertyType::Color,
      impl_->material_.specularTint(), -45);
  specularTintProp->setDisplayLabel(QStringLiteral("Specular Tint"));
  materialGroup.addProperty(specularTintProp);
  auto thinFilmThicknessProp = persistentLayerProperty(
      QStringLiteral("material.thinFilmThickness"), PropertyType::Float,
      impl_->material_.thinFilmThickness(), -44);
  thinFilmThicknessProp->setDisplayLabel(QStringLiteral("Thin Film Thickness"));
  thinFilmThicknessProp->setTooltip(QStringLiteral(
      "Interference film thickness in nanometers; zero disables the effect"));
  thinFilmThicknessProp->setHardRange(0.0, 1000.0);
  materialGroup.addProperty(thinFilmThicknessProp);
  auto thinFilmIorProp = persistentLayerProperty(
      QStringLiteral("material.thinFilmIOR"), PropertyType::Float,
      impl_->material_.thinFilmIOR(), -43);
  thinFilmIorProp->setDisplayLabel(QStringLiteral("Thin Film IOR"));
  thinFilmIorProp->setHardRange(1.0, 2.5);
  materialGroup.addProperty(thinFilmIorProp);

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
  auto thinWallProp = persistentLayerProperty(
      QStringLiteral("material.thinWall"), PropertyType::Boolean,
      impl_->material_.thinWall(), -43);
  thinWallProp->setDisplayLabel(QStringLiteral("Thin Wall"));
  thinWallProp->setTooltip(QStringLiteral(
      "Treat transmission as a zero-thickness sheet with no refraction bend"));
  materialGroup.addProperty(thinWallProp);

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

  auto clearcoatIorProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatIOR"), PropertyType::Float,
      impl_->material_.clearcoatIOR(), -41);
  clearcoatIorProp->setDisplayLabel(QStringLiteral("Clearcoat IOR"));
  clearcoatIorProp->setHardRange(1.0, 2.0);
  materialGroup.addProperty(clearcoatIorProp);

  auto clearcoatTintProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTint"), PropertyType::Color,
      impl_->material_.clearcoatTint(), -40);
  clearcoatTintProp->setDisplayLabel(QStringLiteral("Clearcoat Tint"));
  materialGroup.addProperty(clearcoatTintProp);

  auto clearcoatNormalStrengthProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalStrength"), PropertyType::Float,
      impl_->material_.clearcoatNormalStrength(), -39);
  clearcoatNormalStrengthProp->setDisplayLabel(
      QStringLiteral("Coat Normal Strength"));
  clearcoatNormalStrengthProp->setTooltip(QStringLiteral(
      "Controls how strongly the base normal map affects the clearcoat layer"));
  clearcoatNormalStrengthProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(clearcoatNormalStrengthProp);

  auto sheenProp = persistentLayerProperty(
      QStringLiteral("material.sheen"), PropertyType::Float,
      impl_->material_.sheen(), -39);
  sheenProp->setDisplayLabel(QStringLiteral("Sheen"));
  sheenProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(sheenProp);

  auto sheenRoughnessProp = persistentLayerProperty(
      QStringLiteral("material.sheenRoughness"), PropertyType::Float,
      impl_->material_.sheenRoughness(), -38);
  sheenRoughnessProp->setDisplayLabel(QStringLiteral("Sheen Roughness"));
  sheenRoughnessProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(sheenRoughnessProp);

  auto sheenTintProp = persistentLayerProperty(
      QStringLiteral("material.sheenTint"), PropertyType::Color,
      impl_->material_.sheenTint(), -37);
  sheenTintProp->setDisplayLabel(QStringLiteral("Sheen Tint"));
  materialGroup.addProperty(sheenTintProp);
  auto sheenColorTextureProp = persistentLayerProperty(
      QStringLiteral("material.sheenColorTexture"), PropertyType::String,
      impl_->material_.sheenColorTexture().toQString(), -38);
  sheenColorTextureProp->setDisplayLabel(QStringLiteral("Sheen Color Texture"));
  materialGroup.addProperty(sheenColorTextureProp);
  const auto sheenColorUv = impl_->material_.sheenColorTextureUvTransform();
  const auto addSheenUv = [&](const QString& suffix, const QString& label,
                              double value, double minimum, double maximum,
                              int priority) {
    auto property = persistentLayerProperty(
        QStringLiteral("material.sheenColorTextureUv.") + suffix,
        PropertyType::Float, value, priority);
    property->setDisplayLabel(label);
    property->setHardRange(minimum, maximum);
    materialGroup.addProperty(property);
  };
  addSheenUv(QStringLiteral("scaleU"), QStringLiteral("Sheen Texture U Scale"),
      sheenColorUv.scaleU, 0.0001, 10000.0, -37);
  addSheenUv(QStringLiteral("scaleV"), QStringLiteral("Sheen Texture V Scale"),
      sheenColorUv.scaleV, 0.0001, 10000.0, -36);
  addSheenUv(QStringLiteral("rotation"), QStringLiteral("Sheen Texture Rotation"),
      sheenColorUv.rotationDegrees, -360.0, 360.0, -35);
  addSheenUv(QStringLiteral("offsetU"), QStringLiteral("Sheen Texture U Offset"),
      sheenColorUv.offsetU, -1000000.0, 1000000.0, -34);
  addSheenUv(QStringLiteral("offsetV"), QStringLiteral("Sheen Texture V Offset"),
      sheenColorUv.offsetV, -1000000.0, 1000000.0, -33);
  auto sheenColorUvSetProp = persistentLayerProperty(
      QStringLiteral("material.sheenColorTextureUv.uvSet"),
      PropertyType::Integer, sheenColorUv.uvSet, -32);
  sheenColorUvSetProp->setDisplayLabel(QStringLiteral("Sheen Texture UV Set"));
  sheenColorUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(sheenColorUvSetProp);

  auto sheenRoughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.sheenRoughnessTexture"), PropertyType::String,
      impl_->material_.sheenRoughnessTexture().toQString(), -31);
  sheenRoughnessTextureProp->setDisplayLabel(
      QStringLiteral("Sheen Roughness Texture"));
  materialGroup.addProperty(sheenRoughnessTextureProp);
  const auto sheenRoughnessUv =
      impl_->material_.sheenRoughnessTextureUvTransform();
  const auto addSheenRoughnessUv = [&](const QString& suffix,
                                       const QString& label, double value,
                                       double minimum, double maximum,
                                       int priority) {
    auto property = persistentLayerProperty(
        QStringLiteral("material.sheenRoughnessTextureUv.") + suffix,
        PropertyType::Float, value, priority);
    property->setDisplayLabel(label);
    property->setHardRange(minimum, maximum);
    materialGroup.addProperty(property);
  };
  addSheenRoughnessUv(QStringLiteral("scaleU"),
      QStringLiteral("Sheen Roughness U Scale"), sheenRoughnessUv.scaleU,
      0.0001, 10000.0, -30);
  addSheenRoughnessUv(QStringLiteral("scaleV"),
      QStringLiteral("Sheen Roughness V Scale"), sheenRoughnessUv.scaleV,
      0.0001, 10000.0, -29);
  addSheenRoughnessUv(QStringLiteral("rotation"),
      QStringLiteral("Sheen Roughness Texture Rotation"),
      sheenRoughnessUv.rotationDegrees, -360.0, 360.0, -28);
  addSheenRoughnessUv(QStringLiteral("offsetU"),
      QStringLiteral("Sheen Roughness Texture U Offset"),
      sheenRoughnessUv.offsetU, -1000000.0, 1000000.0, -27);
  addSheenRoughnessUv(QStringLiteral("offsetV"),
      QStringLiteral("Sheen Roughness Texture V Offset"),
      sheenRoughnessUv.offsetV, -1000000.0, 1000000.0, -26);
  auto sheenRoughnessUvSetProp = persistentLayerProperty(
      QStringLiteral("material.sheenRoughnessTextureUv.uvSet"),
      PropertyType::Integer, sheenRoughnessUv.uvSet, -25);
  sheenRoughnessUvSetProp->setDisplayLabel(
      QStringLiteral("Sheen Roughness Texture UV Set"));
  sheenRoughnessUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(sheenRoughnessUvSetProp);

  auto subsurfaceProp = persistentLayerProperty(
      QStringLiteral("material.subsurface"), PropertyType::Float,
      impl_->material_.subsurface(), -34);
  subsurfaceProp->setDisplayLabel(QStringLiteral("Subsurface Weight"));
  subsurfaceProp->setHardRange(0.0, 1.0);
  materialGroup.addProperty(subsurfaceProp);
  auto subsurfaceIorProp = persistentLayerProperty(
      QStringLiteral("material.subsurfaceIOR"), PropertyType::Float,
      impl_->material_.subsurfaceIOR(), -32);
  subsurfaceIorProp->setDisplayLabel(QStringLiteral("Subsurface IOR"));
  subsurfaceIorProp->setTooltip(QStringLiteral(
      "IOR for the subsurface component; used by its Fresnel energy approximation"));
  subsurfaceIorProp->setHardRange(1.0, 2.0);
  materialGroup.addProperty(subsurfaceIorProp);
  auto subsurfaceAnisotropyProp = persistentLayerProperty(
      QStringLiteral("material.subsurfaceAnisotropy"), PropertyType::Float,
      impl_->material_.subsurfaceAnisotropy(), -31);
  subsurfaceAnisotropyProp->setDisplayLabel(
      QStringLiteral("Subsurface Anisotropy"));
  subsurfaceAnisotropyProp->setTooltip(QStringLiteral(
      "Directional phase approximation for direct light; does not model volume scattering"));
  subsurfaceAnisotropyProp->setHardRange(-0.9, 0.9);
  materialGroup.addProperty(subsurfaceAnisotropyProp);

  auto subsurfaceColorProp = persistentLayerProperty(
      QStringLiteral("material.subsurfaceColor"), PropertyType::Color,
      impl_->material_.subsurfaceColor(), -33);
  subsurfaceColorProp->setDisplayLabel(QStringLiteral("Subsurface Color"));
  materialGroup.addProperty(subsurfaceColorProp);

  const QVector3D subsurfaceRadius = impl_->material_.subsurfaceRadius();
  auto subsurfaceRadiusR = persistentLayerProperty(
      QStringLiteral("material.subsurfaceRadius.r"), PropertyType::Float,
      subsurfaceRadius.x(), -32);
  subsurfaceRadiusR->setDisplayLabel(QStringLiteral("Subsurface Radius R"));
  subsurfaceRadiusR->setTooltip(QStringLiteral(
      "Relative scattering radius multiplier; the fast approximation does not trace volume thickness"));
  subsurfaceRadiusR->setHardRange(0.0, 10.0);
  materialGroup.addProperty(subsurfaceRadiusR);
  auto subsurfaceRadiusG = persistentLayerProperty(
      QStringLiteral("material.subsurfaceRadius.g"), PropertyType::Float,
      subsurfaceRadius.y(), -31);
  subsurfaceRadiusG->setDisplayLabel(QStringLiteral("Subsurface Radius G"));
  subsurfaceRadiusG->setHardRange(0.0, 10.0);
  materialGroup.addProperty(subsurfaceRadiusG);
  auto subsurfaceRadiusB = persistentLayerProperty(
      QStringLiteral("material.subsurfaceRadius.b"), PropertyType::Float,
      subsurfaceRadius.z(), -30);
  subsurfaceRadiusB->setDisplayLabel(QStringLiteral("Subsurface Radius B"));
  subsurfaceRadiusB->setHardRange(0.0, 10.0);
  materialGroup.addProperty(subsurfaceRadiusB);

  auto anisotropyProp = persistentLayerProperty(
      QStringLiteral("material.anisotropy"), PropertyType::Float,
      impl_->material_.anisotropy(), -36);
  anisotropyProp->setDisplayLabel(QStringLiteral("Anisotropy"));
  anisotropyProp->setHardRange(-1.0, 1.0);
  materialGroup.addProperty(anisotropyProp);

  auto anisotropyRotationProp = persistentLayerProperty(
      QStringLiteral("material.anisotropyRotationDegrees"),
      PropertyType::Float, impl_->material_.anisotropyRotationDegrees(), -35);
  anisotropyRotationProp->setDisplayLabel(
      QStringLiteral("Anisotropy Rotation"));
  anisotropyRotationProp->setHardRange(-180.0, 180.0);
  materialGroup.addProperty(anisotropyRotationProp);
  auto anisotropyTextureProp = persistentLayerProperty(
      QStringLiteral("material.anisotropyTexture"), PropertyType::String,
      impl_->material_.anisotropyTexture().toQString(), -34);
  anisotropyTextureProp->setDisplayLabel(QStringLiteral("Anisotropy Texture"));
  materialGroup.addProperty(anisotropyTextureProp);
  const auto anisotropyUv = impl_->material_.anisotropyTextureUvTransform();
  const auto addAnisotropyUv = [&](const QString& suffix,
      const QString& label, double value, double minimum, double maximum,
      int priority) {
    auto property = persistentLayerProperty(
        QStringLiteral("material.anisotropyTextureUv.") + suffix,
        PropertyType::Float, value, priority);
    property->setDisplayLabel(label);
    property->setHardRange(minimum, maximum);
    materialGroup.addProperty(property);
  };
  addAnisotropyUv(QStringLiteral("scaleU"), QStringLiteral("Anisotropy Texture U Scale"), anisotropyUv.scaleU, 0.0001, 10000.0, -33);
  addAnisotropyUv(QStringLiteral("scaleV"), QStringLiteral("Anisotropy Texture V Scale"), anisotropyUv.scaleV, 0.0001, 10000.0, -32);
  addAnisotropyUv(QStringLiteral("rotation"), QStringLiteral("Anisotropy Texture Rotation"), anisotropyUv.rotationDegrees, -360.0, 360.0, -31);
  addAnisotropyUv(QStringLiteral("offsetU"), QStringLiteral("Anisotropy Texture U Offset"), anisotropyUv.offsetU, -1000000.0, 1000000.0, -30);
  addAnisotropyUv(QStringLiteral("offsetV"), QStringLiteral("Anisotropy Texture V Offset"), anisotropyUv.offsetV, -1000000.0, 1000000.0, -29);
  auto anisotropyUvSetProp = persistentLayerProperty(
      QStringLiteral("material.anisotropyTextureUv.uvSet"),
      PropertyType::Integer, anisotropyUv.uvSet, -28);
  anisotropyUvSetProp->setDisplayLabel(QStringLiteral("Anisotropy Texture UV Set"));
  anisotropyUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(anisotropyUvSetProp);

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

  auto baseColorUvSetProp = persistentLayerProperty(
      QStringLiteral("material.baseColorUvSet"), PropertyType::Integer,
      impl_->material_.baseColorUvSet(), -38);
  baseColorUvSetProp->setDisplayLabel(QStringLiteral("Base Color UV Set"));
  baseColorUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(baseColorUvSetProp);

  auto baseColorUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.baseColorUvScale"), PropertyType::Float,
      impl_->material_.baseColorUvScale(), -37);
  baseColorUvScaleProp->setDisplayLabel(
      QStringLiteral("Base Color Texture UV Scale"));
  baseColorUvScaleProp->setTooltip(QStringLiteral(
      "Scales base color texture coordinates around the UV center"));
  baseColorUvScaleProp->setHardRange(0.01, 10.0);
  baseColorUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(baseColorUvScaleProp);

  auto baseColorUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.baseColorUvRotation"), PropertyType::Float,
      impl_->material_.baseColorUvRotationDegrees(), -34);
  baseColorUvRotationProp->setDisplayLabel(
      QStringLiteral("Base Color Texture UV Rotation"));
  baseColorUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(baseColorUvRotationProp);

  auto baseColorUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.baseColorUvOffsetU"), PropertyType::Float,
      impl_->material_.baseColorUvOffsetU(), -36);
  baseColorUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Base Color Texture UV Offset U"));
  baseColorUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(baseColorUvOffsetUProp);

  auto baseColorUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.baseColorUvOffsetV"), PropertyType::Float,
      impl_->material_.baseColorUvOffsetV(), -35);
  baseColorUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Base Color Texture UV Offset V"));
  baseColorUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(baseColorUvOffsetVProp);

  auto metallicRoughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessTexture"), PropertyType::String,
      impl_->material_.metallicRoughnessTexture().toQString(), -39);
  metallicRoughnessTextureProp->setDisplayLabel(QStringLiteral("Metallic Roughness Texture"));
  materialGroup.addProperty(metallicRoughnessTextureProp);

  auto metallicTextureProp = persistentLayerProperty(
      QStringLiteral("material.metallicTexture"), PropertyType::String,
      impl_->material_.metallicTexture().toQString(), -38);
  metallicTextureProp->setDisplayLabel(QStringLiteral("Metallic Texture"));
  materialGroup.addProperty(metallicTextureProp);

  auto roughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.roughnessTexture"), PropertyType::String,
      impl_->material_.roughnessTexture().toQString(), -37);
  roughnessTextureProp->setDisplayLabel(QStringLiteral("Roughness Texture"));
  materialGroup.addProperty(roughnessTextureProp);
  addIndependentTextureUvProperties(
      QStringLiteral("material.metallicTextureUv."),
      QStringLiteral("Metallic Texture"),
      impl_->material_.metallicTextureUvTransform(), -30);
  addIndependentTextureUvProperties(
      QStringLiteral("material.roughnessTextureUv."),
      QStringLiteral("Roughness Texture"),
      impl_->material_.roughnessTextureUvTransform(), -24);

  auto metallicRoughnessUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessUvScale"), PropertyType::Float,
      impl_->material_.metallicRoughnessUvScale(), -36);
  metallicRoughnessUvScaleProp->setDisplayLabel(
      QStringLiteral("Metallic Roughness Texture UV Scale"));
  metallicRoughnessUvScaleProp->setTooltip(QStringLiteral(
      "Scales metallic roughness texture coordinates around the UV center"));
  metallicRoughnessUvScaleProp->setHardRange(0.01, 10.0);
  metallicRoughnessUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(metallicRoughnessUvScaleProp);

  auto metallicRoughnessUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessUvOffsetU"),
      PropertyType::Float, impl_->material_.metallicRoughnessUvOffsetU(), -35);
  metallicRoughnessUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Metallic Roughness Texture UV Offset U"));
  metallicRoughnessUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(metallicRoughnessUvOffsetUProp);

  auto metallicRoughnessUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessUvOffsetV"),
      PropertyType::Float, impl_->material_.metallicRoughnessUvOffsetV(), -34);
  metallicRoughnessUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Metallic Roughness Texture UV Offset V"));
  metallicRoughnessUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(metallicRoughnessUvOffsetVProp);

  auto metallicRoughnessUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessUvRotation"),
      PropertyType::Float,
      impl_->material_.metallicRoughnessUvRotationDegrees(), -33);
  metallicRoughnessUvRotationProp->setDisplayLabel(
      QStringLiteral("Metallic Roughness Texture UV Rotation"));
  metallicRoughnessUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(metallicRoughnessUvRotationProp);

  auto metallicRoughnessUvSetProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessUvSet"),
      PropertyType::Integer, impl_->material_.metallicRoughnessUvSet(), -34);
  metallicRoughnessUvSetProp->setDisplayLabel(
      QStringLiteral("Metallic Roughness UV Set"));
  metallicRoughnessUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(metallicRoughnessUvSetProp);

  auto normalTextureProp = persistentLayerProperty(
      QStringLiteral("material.normalTexture"), PropertyType::String,
      impl_->material_.normalTexture().toQString(), -38);
  normalTextureProp->setDisplayLabel(QStringLiteral("Normal Texture"));
  materialGroup.addProperty(normalTextureProp);

  auto normalUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.normalUvRotation"), PropertyType::Float,
      impl_->material_.normalUvRotationDegrees(), -37);
  normalUvRotationProp->setDisplayLabel(QStringLiteral("Normal UV Rotation"));
  normalUvRotationProp->setTooltip(QStringLiteral(
      "Additional counter-clockwise rotation for normal map coordinates"));
  normalUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(normalUvRotationProp);

  auto normalUvSetProp = persistentLayerProperty(
      QStringLiteral("material.normalUvSet"), PropertyType::Integer,
      impl_->material_.normalUvSet(), -35);
  normalUvSetProp->setDisplayLabel(QStringLiteral("Normal UV Set"));
  normalUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(normalUvSetProp);

  auto normalUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.normalUvScale"), PropertyType::Float,
      impl_->material_.normalUvScale(), -36);
  normalUvScaleProp->setDisplayLabel(QStringLiteral("Normal UV Scale"));
  normalUvScaleProp->setTooltip(QStringLiteral(
      "Uniform tiling for the normal texture"));
  normalUvScaleProp->setHardRange(0.01, 10.0);
  normalUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(normalUvScaleProp);

  auto normalUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.normalUvOffsetU"), PropertyType::Float,
      impl_->material_.normalUvOffsetU(), -33);
  normalUvOffsetUProp->setDisplayLabel(QStringLiteral("Normal UV Offset U"));
  normalUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(normalUvOffsetUProp);

  auto normalUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.normalUvOffsetV"), PropertyType::Float,
      impl_->material_.normalUvOffsetV(), -32);
  normalUvOffsetVProp->setDisplayLabel(QStringLiteral("Normal UV Offset V"));
  normalUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(normalUvOffsetVProp);

  auto clearcoatNormalTextureProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalTexture"), PropertyType::String,
      impl_->material_.clearcoatNormalTexture().toQString(), -38);
  clearcoatNormalTextureProp->setDisplayLabel(
      QStringLiteral("Coat Normal Texture"));
  materialGroup.addProperty(clearcoatNormalTextureProp);

  auto clearcoatTextureProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTexture"), PropertyType::String,
      impl_->material_.clearcoatTexture().toQString(), -39);
  clearcoatTextureProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture"));
  materialGroup.addProperty(clearcoatTextureProp);
  auto clearcoatRoughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatRoughnessTexture"), PropertyType::String,
      impl_->material_.clearcoatRoughnessTexture().toQString(), -40);
  clearcoatRoughnessTextureProp->setDisplayLabel(
      QStringLiteral("Clearcoat Roughness Texture"));
  materialGroup.addProperty(clearcoatRoughnessTextureProp);
  auto clearcoatTextureScaleProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTextureUvScale"), PropertyType::Float,
      impl_->material_.clearcoatTextureUvScale(), -38);
  clearcoatTextureScaleProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture UV Scale"));
  clearcoatTextureScaleProp->setHardRange(0.01, 10.0);
  materialGroup.addProperty(clearcoatTextureScaleProp);
  auto clearcoatTextureRotationProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTextureUvRotation"),
      PropertyType::Float,
      impl_->material_.clearcoatTextureUvRotationDegrees(), -37);
  clearcoatTextureRotationProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture UV Rotation"));
  clearcoatTextureRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(clearcoatTextureRotationProp);
  auto clearcoatTextureOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTextureUvOffsetU"),
      PropertyType::Float,
      impl_->material_.clearcoatTextureUvOffsetU(), -36);
  clearcoatTextureOffsetUProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture UV Offset U"));
  clearcoatTextureOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(clearcoatTextureOffsetUProp);
  auto clearcoatTextureOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTextureUvOffsetV"),
      PropertyType::Float,
      impl_->material_.clearcoatTextureUvOffsetV(), -35);
  clearcoatTextureOffsetVProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture UV Offset V"));
  clearcoatTextureOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(clearcoatTextureOffsetVProp);
  auto clearcoatTextureUvSetProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatTextureUvSet"),
      PropertyType::Integer,
      impl_->material_.clearcoatTextureUvSet(), -34);
  clearcoatTextureUvSetProp->setDisplayLabel(
      QStringLiteral("Clearcoat Texture UV Set"));
  clearcoatTextureUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(clearcoatTextureUvSetProp);

  auto clearcoatNormalUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalUvRotation"),
      PropertyType::Float,
      impl_->material_.clearcoatNormalUvRotationDegrees(), -35);
  clearcoatNormalUvRotationProp->setDisplayLabel(
      QStringLiteral("Coat Normal UV Rotation"));
  clearcoatNormalUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(clearcoatNormalUvRotationProp);

  auto clearcoatNormalUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalUvScale"), PropertyType::Float,
      impl_->material_.clearcoatNormalUvScale(), -34);
  clearcoatNormalUvScaleProp->setDisplayLabel(
      QStringLiteral("Coat Normal UV Scale"));
  clearcoatNormalUvScaleProp->setHardRange(0.01, 10.0);
  clearcoatNormalUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(clearcoatNormalUvScaleProp);

  auto clearcoatNormalUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalUvOffsetU"),
      PropertyType::Float, impl_->material_.clearcoatNormalUvOffsetU(), -33);
  clearcoatNormalUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Coat Normal UV Offset U"));
  clearcoatNormalUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(clearcoatNormalUvOffsetUProp);

  auto clearcoatNormalUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalUvOffsetV"),
      PropertyType::Float, impl_->material_.clearcoatNormalUvOffsetV(), -32);
  clearcoatNormalUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Coat Normal UV Offset V"));
  clearcoatNormalUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(clearcoatNormalUvOffsetVProp);

  auto clearcoatNormalUvSetProp = persistentLayerProperty(
      QStringLiteral("material.clearcoatNormalUvSet"),
      PropertyType::Integer, impl_->material_.clearcoatNormalUvSet(), -31);
  clearcoatNormalUvSetProp->setDisplayLabel(
      QStringLiteral("Coat Normal UV Set"));
  clearcoatNormalUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(clearcoatNormalUvSetProp);

  auto emissionTextureProp = persistentLayerProperty(
      QStringLiteral("material.emissionTexture"), PropertyType::String,
      impl_->material_.emissionTexture().toQString(), -37);
  emissionTextureProp->setDisplayLabel(QStringLiteral("Emission Texture"));
  materialGroup.addProperty(emissionTextureProp);

  auto emissionTextureUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.emissionTextureUvScale"), PropertyType::Float,
      impl_->material_.emissionTextureUvScale(), -29);
  emissionTextureUvScaleProp->setDisplayLabel(
      QStringLiteral("Emission Texture UV Scale"));
  emissionTextureUvScaleProp->setTooltip(QStringLiteral(
      "Scales emission texture coordinates around the UV center"));
  emissionTextureUvScaleProp->setHardRange(0.01, 10.0);
  emissionTextureUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(emissionTextureUvScaleProp);

  auto emissionTextureUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.emissionTextureUvOffsetU"),
      PropertyType::Float, impl_->material_.emissionTextureUvOffsetU(), -27);
  emissionTextureUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Emission Texture UV Offset U"));
  emissionTextureUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(emissionTextureUvOffsetUProp);

  auto emissionTextureUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.emissionTextureUvOffsetV"),
      PropertyType::Float, impl_->material_.emissionTextureUvOffsetV(), -26);
  emissionTextureUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Emission Texture UV Offset V"));
  emissionTextureUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(emissionTextureUvOffsetVProp);

  auto emissionTextureUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.emissionTextureUvRotation"),
      PropertyType::Float,
      impl_->material_.emissionTextureUvRotationDegrees(), -25);
  emissionTextureUvRotationProp->setDisplayLabel(
      QStringLiteral("Emission Texture UV Rotation"));
  emissionTextureUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(emissionTextureUvRotationProp);

  auto emissionTextureUvSetProp = persistentLayerProperty(
      QStringLiteral("material.emissionTextureUvSet"), PropertyType::Integer,
      impl_->material_.emissionTextureUvSet(), -24);
  emissionTextureUvSetProp->setDisplayLabel(
      QStringLiteral("Emission Texture UV Set"));
  emissionTextureUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(emissionTextureUvSetProp);

  auto occlusionTextureProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTexture"), PropertyType::String,
      impl_->material_.occlusionTexture().toQString(), -36);
  occlusionTextureProp->setDisplayLabel(QStringLiteral("Occlusion Texture"));
  materialGroup.addProperty(occlusionTextureProp);

  auto occlusionTextureUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTextureUvScale"), PropertyType::Float,
      impl_->material_.occlusionTextureUvScale(), -30);
  occlusionTextureUvScaleProp->setDisplayLabel(
      QStringLiteral("Occlusion Texture UV Scale"));
  occlusionTextureUvScaleProp->setTooltip(QStringLiteral(
      "Scales occlusion texture coordinates around the UV center"));
  occlusionTextureUvScaleProp->setHardRange(0.01, 10.0);
  occlusionTextureUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(occlusionTextureUvScaleProp);

  auto occlusionTextureUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTextureUvOffsetU"),
      PropertyType::Float, impl_->material_.occlusionTextureUvOffsetU(), -24);
  occlusionTextureUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Occlusion Texture UV Offset U"));
  occlusionTextureUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(occlusionTextureUvOffsetUProp);

  auto occlusionTextureUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTextureUvOffsetV"),
      PropertyType::Float, impl_->material_.occlusionTextureUvOffsetV(), -23);
  occlusionTextureUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Occlusion Texture UV Offset V"));
  occlusionTextureUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(occlusionTextureUvOffsetVProp);

  auto occlusionTextureUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTextureUvRotation"),
      PropertyType::Float,
      impl_->material_.occlusionTextureUvRotationDegrees(), -22);
  occlusionTextureUvRotationProp->setDisplayLabel(
      QStringLiteral("Occlusion Texture UV Rotation"));
  occlusionTextureUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(occlusionTextureUvRotationProp);

  auto occlusionTextureUvSetProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTextureUvSet"), PropertyType::Integer,
      impl_->material_.occlusionTextureUvSet(), -21);
  occlusionTextureUvSetProp->setDisplayLabel(
      QStringLiteral("Occlusion Texture UV Set"));
  occlusionTextureUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(occlusionTextureUvSetProp);

  auto opacityTextureProp = persistentLayerProperty(
      QStringLiteral("material.opacityTexture"), PropertyType::String,
      impl_->material_.opacityTexture().toQString(), -35);
  opacityTextureProp->setDisplayLabel(QStringLiteral("Opacity Texture"));
  materialGroup.addProperty(opacityTextureProp);
  auto opacityTextureUvScaleProp = persistentLayerProperty(
      QStringLiteral("material.opacityTextureUvScale"), PropertyType::Float,
      impl_->material_.opacityTextureUvScale(), -28);
  opacityTextureUvScaleProp->setDisplayLabel(
      QStringLiteral("Opacity Texture UV Scale"));
  opacityTextureUvScaleProp->setTooltip(QStringLiteral(
      "Scales the opacity map UVs around the center independently."));
  opacityTextureUvScaleProp->setHardRange(0.01, 10.0);
  opacityTextureUvScaleProp->setSoftRange(0.1, 4.0);
  materialGroup.addProperty(opacityTextureUvScaleProp);

  auto opacityTextureUvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.opacityTextureUvOffsetU"),
      PropertyType::Float, impl_->material_.opacityTextureUvOffsetU(), -22);
  opacityTextureUvOffsetUProp->setDisplayLabel(
      QStringLiteral("Opacity Texture UV Offset U"));
  opacityTextureUvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(opacityTextureUvOffsetUProp);

  auto opacityTextureUvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.opacityTextureUvOffsetV"),
      PropertyType::Float, impl_->material_.opacityTextureUvOffsetV(), -21);
  opacityTextureUvOffsetVProp->setDisplayLabel(
      QStringLiteral("Opacity Texture UV Offset V"));
  opacityTextureUvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(opacityTextureUvOffsetVProp);

  auto opacityTextureUvRotationProp = persistentLayerProperty(
      QStringLiteral("material.opacityTextureUvRotation"),
      PropertyType::Float,
      impl_->material_.opacityTextureUvRotationDegrees(), -20);
  opacityTextureUvRotationProp->setDisplayLabel(
      QStringLiteral("Opacity Texture UV Rotation"));
  opacityTextureUvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(opacityTextureUvRotationProp);

  auto opacityTextureUvSetProp = persistentLayerProperty(
      QStringLiteral("material.opacityTextureUvSet"), PropertyType::Integer,
      impl_->material_.opacityTextureUvSet(), -19);
  opacityTextureUvSetProp->setDisplayLabel(
      QStringLiteral("Opacity Texture UV Set"));
  opacityTextureUvSetProp->setHardRange(0, 1);
  materialGroup.addProperty(opacityTextureUvSetProp);

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

  auto uvOffsetUProp = persistentLayerProperty(
      QStringLiteral("material.uv.offsetU"), PropertyType::Float,
      impl_->material_.uvOffsetU(), -28);
  uvOffsetUProp->setDisplayLabel(QStringLiteral("UV Offset U"));
  uvOffsetUProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(uvOffsetUProp);

  auto uvOffsetVProp = persistentLayerProperty(
      QStringLiteral("material.uv.offsetV"), PropertyType::Float,
      impl_->material_.uvOffsetV(), -27);
  uvOffsetVProp->setDisplayLabel(QStringLiteral("UV Offset V"));
  uvOffsetVProp->setHardRange(-10.0, 10.0);
  materialGroup.addProperty(uvOffsetVProp);

  auto uvScaleUProp = persistentLayerProperty(
      QStringLiteral("material.uv.scaleU"), PropertyType::Float,
      impl_->material_.uvScaleU(), -26);
  uvScaleUProp->setDisplayLabel(QStringLiteral("UV Scale U"));
  uvScaleUProp->setHardRange(0.01, 10.0);
  materialGroup.addProperty(uvScaleUProp);

  auto uvScaleVProp = persistentLayerProperty(
      QStringLiteral("material.uv.scaleV"), PropertyType::Float,
      impl_->material_.uvScaleV(), -25);
  uvScaleVProp->setDisplayLabel(QStringLiteral("UV Scale V"));
  uvScaleVProp->setHardRange(0.01, 10.0);
  materialGroup.addProperty(uvScaleVProp);

  auto uvRotationProp = persistentLayerProperty(
      QStringLiteral("material.uv.rotation"), PropertyType::Float,
      impl_->material_.uvRotationDegrees(), -24);
  uvRotationProp->setDisplayLabel(QStringLiteral("UV Rotation"));
  uvRotationProp->setTooltip(QStringLiteral("Degrees, counter-clockwise around UV center"));
  uvRotationProp->setHardRange(-360.0, 360.0);
  materialGroup.addProperty(uvRotationProp);

  auto envIntensityProp = persistentLayerProperty(
      QStringLiteral("material.envIntensity"), PropertyType::Float,
      impl_->material_.environmentIntensity(), -23);
  envIntensityProp->setDisplayLabel(QStringLiteral("Environment Intensity"));
  envIntensityProp->setTooltip(QStringLiteral("Per-material IBL scale for indirect light (0 = no environment response)"));
  envIntensityProp->setHardRange(0.0, 4.0);
  envIntensityProp->setSoftRange(0.0, 2.0);
  materialGroup.addProperty(envIntensityProp);

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
  if (impl_->meshUsesSingleInstanceForMaterialSlots_ &&
      impl_->meshMaterials_.size() > 1) {
    for (std::size_t slotIndex = 0;
         slotIndex < impl_->meshMaterials_.size(); ++slotIndex) {
      const auto& material = impl_->meshMaterials_[slotIndex];
      const QString slotName = material.name().toQString().trimmed();
      PropertyGroup slotGroup(
          slotName.isEmpty()
              ? QStringLiteral("Material Slot %1").arg(
                    static_cast<int>(slotIndex + 1))
              : QStringLiteral("Material: %1").arg(slotName));
      const QString prefix = QStringLiteral("material.slots.%1.")
                                 .arg(slotIndex);
      const auto addColor = [&](const QString& field, const QString& label,
                                const QColor& color, int priority) {
        auto property = persistentLayerProperty(
            prefix + field, PropertyType::Color, color, priority);
        property->setDisplayLabel(label);
        slotGroup.addProperty(property);
      };
      const auto addScalar = [&](const QString& field, const QString& label,
          float scalar, double minimum, double maximum, int priority) {
        auto property = persistentLayerProperty(
            prefix + field, PropertyType::Float, scalar, priority);
        property->setDisplayLabel(label);
        property->setHardRange(minimum, maximum);
        slotGroup.addProperty(property);
      };
      const auto addTexturePath = [&](const QString& field,
          const QString& label, const ArtifactCore::UniString& path,
          int priority) {
        auto property = persistentLayerProperty(
            prefix + field, PropertyType::String, path.toQString(), priority);
        property->setDisplayLabel(label);
        slotGroup.addProperty(property);
      };
      const auto addTextureUv = [&](const QString& field,
          const QString& label, float scale, float rotation, float offsetU,
          float offsetV, int uvSet, int priority) {
        const QString uvPrefix = field + QStringLiteral("Uv.");
        addScalar(uvPrefix + QStringLiteral("scale"),
                  label + QStringLiteral(" UV Scale"), scale, 0.01, 10.0,
                  priority);
        addScalar(uvPrefix + QStringLiteral("rotation"),
                  label + QStringLiteral(" UV Rotation"), rotation,
                  -360.0, 360.0, priority + 1);
        addScalar(uvPrefix + QStringLiteral("offsetU"),
                  label + QStringLiteral(" UV Offset U"), offsetU,
                  -10.0, 10.0, priority + 2);
        addScalar(uvPrefix + QStringLiteral("offsetV"),
                  label + QStringLiteral(" UV Offset V"), offsetV,
                  -10.0, 10.0, priority + 3);
        auto uvSetProperty = persistentLayerProperty(
            prefix + uvPrefix + QStringLiteral("uvSet"),
            PropertyType::Integer, uvSet, priority + 4);
        uvSetProperty->setDisplayLabel(label + QStringLiteral(" UV Set"));
        uvSetProperty->setHardRange(0, 1);
        slotGroup.addProperty(uvSetProperty);
      };
      const auto addIndependentTextureUv = [&](const QString& field,
          const QString& label,
          const ArtifactCore::MaterialTextureUvTransform& transform,
          int priority) {
        const QString uvPrefix = field + QStringLiteral("Uv.");
        addScalar(uvPrefix + QStringLiteral("scaleU"),
            label + QStringLiteral(" UV Scale U"), transform.scaleU,
            0.01, 10.0, priority);
        addScalar(uvPrefix + QStringLiteral("scaleV"),
            label + QStringLiteral(" UV Scale V"), transform.scaleV,
            0.01, 10.0, priority + 1);
        addScalar(uvPrefix + QStringLiteral("rotation"),
            label + QStringLiteral(" UV Rotation"),
            transform.rotationDegrees, -360.0, 360.0, priority + 2);
        addScalar(uvPrefix + QStringLiteral("offsetU"),
            label + QStringLiteral(" UV Offset U"), transform.offsetU,
            -10.0, 10.0, priority + 3);
        addScalar(uvPrefix + QStringLiteral("offsetV"),
            label + QStringLiteral(" UV Offset V"), transform.offsetV,
            -10.0, 10.0, priority + 4);
        auto uvSetProperty = persistentLayerProperty(
            prefix + uvPrefix + QStringLiteral("uvSet"),
            PropertyType::Integer, transform.uvSet, priority + 5);
        uvSetProperty->setDisplayLabel(label + QStringLiteral(" UV Set"));
        uvSetProperty->setHardRange(0, 1);
        slotGroup.addProperty(uvSetProperty);
      };
      addColor(QStringLiteral("baseColor"), QStringLiteral("Base Color"),
               material.baseColor(), -80);
      addScalar(QStringLiteral("metallic"), QStringLiteral("Metallic"),
                material.metallic(), 0.0, 1.0, -79);
      addScalar(QStringLiteral("roughness"), QStringLiteral("Roughness"),
                material.roughness(), 0.0, 1.0, -78);
      addScalar(QStringLiteral("diffuseRoughness"),
                QStringLiteral("Diffuse Roughness"),
                material.diffuseRoughness(), 0.0, 1.0, -77);
      addScalar(QStringLiteral("specular"), QStringLiteral("Specular"),
                material.specular(), 0.0, 1.0, -76);
      addColor(QStringLiteral("specularTint"),
               QStringLiteral("Specular Tint"), material.specularTint(), -75);
      addScalar(QStringLiteral("thinFilmThickness"),
                QStringLiteral("Thin Film Thickness"),
                material.thinFilmThickness(), 0.0, 1000.0, -74);
      addScalar(QStringLiteral("thinFilmIOR"), QStringLiteral("Thin Film IOR"),
                material.thinFilmIOR(), 1.0, 2.5, -73);
      addScalar(QStringLiteral("ior"), QStringLiteral("IOR"), material.ior(),
                1.0, 3.0, -72);
      addScalar(QStringLiteral("transmission"), QStringLiteral("Transmission"),
                material.transmission(), 0.0, 1.0, -71);
      auto thinWallProperty = persistentLayerProperty(
          prefix + QStringLiteral("thinWall"), PropertyType::Boolean,
          material.thinWall(), -70);
      thinWallProperty->setDisplayLabel(QStringLiteral("Thin Wall"));
      thinWallProperty->setTooltip(QStringLiteral(
          "Treat transmission as a zero-thickness sheet with no refraction bend"));
      slotGroup.addProperty(thinWallProperty);
      addScalar(QStringLiteral("clearcoat"), QStringLiteral("Clearcoat"),
                material.clearcoat(), 0.0, 1.0, -70);
      addScalar(QStringLiteral("clearcoatRoughness"),
                QStringLiteral("Clearcoat Roughness"),
                material.clearcoatRoughness(), 0.0, 1.0, -69);
      addTexturePath(QStringLiteral("clearcoatRoughnessTexture"),
          QStringLiteral("Clearcoat Roughness Texture"),
          material.clearcoatRoughnessTexture(), -68);
      addTextureUv(QStringLiteral("clearcoatRoughnessTexture"),
          QStringLiteral("Clearcoat Roughness"),
          material.clearcoatRoughnessTextureUvScale(),
          material.clearcoatRoughnessTextureUvRotationDegrees(),
          material.clearcoatRoughnessTextureUvOffsetU(),
          material.clearcoatRoughnessTextureUvOffsetV(),
          material.clearcoatRoughnessTextureUvSet(), -67);
      addScalar(QStringLiteral("clearcoatIOR"), QStringLiteral("Clearcoat IOR"),
                material.clearcoatIOR(), 1.0, 2.0, -68);
      addColor(QStringLiteral("clearcoatTint"),
               QStringLiteral("Clearcoat Tint"), material.clearcoatTint(), -67);
      addScalar(QStringLiteral("clearcoatNormalStrength"),
                QStringLiteral("Coat Normal Strength"),
                material.clearcoatNormalStrength(), 0.0, 1.0, -66);
      addScalar(QStringLiteral("sheen"), QStringLiteral("Sheen"),
                material.sheen(), 0.0, 1.0, -65);
      addScalar(QStringLiteral("sheenRoughness"),
                QStringLiteral("Sheen Roughness"),
                material.sheenRoughness(), 0.0, 1.0, -64);
      addColor(QStringLiteral("sheenTint"), QStringLiteral("Sheen Tint"),
               material.sheenTint(), -63);
      addScalar(QStringLiteral("subsurface"), QStringLiteral("Subsurface Weight"),
                material.subsurface(), 0.0, 1.0, -62);
      addScalar(QStringLiteral("subsurfaceIOR"), QStringLiteral("Subsurface IOR"),
                material.subsurfaceIOR(), 1.0, 2.0, -61);
      addScalar(QStringLiteral("subsurfaceAnisotropy"),
                QStringLiteral("Subsurface Anisotropy"),
                material.subsurfaceAnisotropy(), -0.9, 0.9, -60);
      addColor(QStringLiteral("subsurfaceColor"),
               QStringLiteral("Subsurface Color"), material.subsurfaceColor(), -59);
      const QVector3D subsurfaceRadius = material.subsurfaceRadius();
      addScalar(QStringLiteral("subsurfaceRadius.r"),
                QStringLiteral("Subsurface Radius R"),
                subsurfaceRadius.x(), 0.0, 10.0, -58);
      addScalar(QStringLiteral("subsurfaceRadius.g"),
                QStringLiteral("Subsurface Radius G"),
                subsurfaceRadius.y(), 0.0, 10.0, -57);
      addScalar(QStringLiteral("subsurfaceRadius.b"),
                QStringLiteral("Subsurface Radius B"),
                subsurfaceRadius.z(), 0.0, 10.0, -56);
      addScalar(QStringLiteral("anisotropy"), QStringLiteral("Anisotropy"),
                material.anisotropy(), -1.0, 1.0, -55);
      addScalar(QStringLiteral("anisotropyRotation"),
                QStringLiteral("Anisotropy Rotation"),
                material.anisotropyRotationDegrees(), -180.0, 180.0, -54);
      addTexturePath(QStringLiteral("anisotropyTexture"),
          QStringLiteral("Anisotropy Texture"), material.anisotropyTexture(), -52);
      addIndependentTextureUv(QStringLiteral("anisotropyTexture"),
          QStringLiteral("Anisotropy Texture"),
          material.anisotropyTextureUvTransform(), -51);
      addColor(QStringLiteral("emissionColor"), QStringLiteral("Emission Color"),
               material.emissionColor(), -53);
      addScalar(QStringLiteral("emissionStrength"),
                QStringLiteral("Emission Strength"),
                material.emissionStrength(), 0.0, 100000.0, -52);
      addScalar(QStringLiteral("opacity"), QStringLiteral("Opacity"),
                material.opacity(), 0.0, 1.0, -51);
      auto alphaModeProperty = persistentLayerProperty(
          prefix + QStringLiteral("alphaMode"), PropertyType::Integer,
          static_cast<int>(material.alphaMode()), -50);
      alphaModeProperty->setDisplayLabel(QStringLiteral("Alpha Mode"));
      alphaModeProperty->setHardRange(0, 2);
      slotGroup.addProperty(alphaModeProperty);
      addScalar(QStringLiteral("alphaCutoff"), QStringLiteral("Alpha Cutoff"),
                material.alphaCutoff(), 0.0, 1.0, -49);
      addScalar(QStringLiteral("normalStrength"),
                QStringLiteral("Normal Strength"),
                material.normalStrength(), 0.0, 10.0, -48);
      addScalar(QStringLiteral("occlusionStrength"),
                QStringLiteral("Occlusion Strength"),
                material.occlusionStrength(), 0.0, 1.0, -47);
      addScalar(QStringLiteral("environmentIntensity"),
                QStringLiteral("Environment Intensity"),
                material.environmentIntensity(), 0.0, 4.0, -46);
      addScalar(QStringLiteral("uvOffsetU"), QStringLiteral("UV Offset U"),
                material.uvOffsetU(), -10.0, 10.0, -45);
      addScalar(QStringLiteral("uvOffsetV"), QStringLiteral("UV Offset V"),
                material.uvOffsetV(), -10.0, 10.0, -44);
      addScalar(QStringLiteral("uvScaleU"), QStringLiteral("UV Scale U"),
                material.uvScaleU(), 0.01, 10.0, -43);
      addScalar(QStringLiteral("uvScaleV"), QStringLiteral("UV Scale V"),
                material.uvScaleV(), 0.01, 10.0, -42);
      addScalar(QStringLiteral("uvRotation"), QStringLiteral("UV Rotation"),
                material.uvRotationDegrees(), -360.0, 360.0, -41);
      addTexturePath(QStringLiteral("baseColorTexture"),
          QStringLiteral("Base Color Texture"), material.baseColorTexture(),
          -40);
      addTextureUv(QStringLiteral("baseColor"), QStringLiteral("Base Color"),
          material.baseColorUvScale(), material.baseColorUvRotationDegrees(),
          material.baseColorUvOffsetU(), material.baseColorUvOffsetV(),
          material.baseColorUvSet(), -39);
      addTexturePath(QStringLiteral("metallicRoughnessTexture"),
          QStringLiteral("Metallic-Roughness Texture"),
          material.metallicRoughnessTexture(), -34);
      addTexturePath(QStringLiteral("metallicTexture"),
          QStringLiteral("Metallic Texture"), material.metallicTexture(), -32);
      addTexturePath(QStringLiteral("roughnessTexture"),
          QStringLiteral("Roughness Texture"), material.roughnessTexture(), -31);
      addIndependentTextureUv(QStringLiteral("metallicTexture"),
          QStringLiteral("Metallic Texture"),
          material.metallicTextureUvTransform(), -30);
      addIndependentTextureUv(QStringLiteral("roughnessTexture"),
          QStringLiteral("Roughness Texture"),
          material.roughnessTextureUvTransform(), -24);
      addTextureUv(QStringLiteral("metallicRoughness"),
          QStringLiteral("Metallic-Roughness"),
          material.metallicRoughnessUvScale(),
          material.metallicRoughnessUvRotationDegrees(),
          material.metallicRoughnessUvOffsetU(),
          material.metallicRoughnessUvOffsetV(),
          material.metallicRoughnessUvSet(), -33);
      addTexturePath(QStringLiteral("normalTexture"),
          QStringLiteral("Normal Texture"), material.normalTexture(), -28);
      addTextureUv(QStringLiteral("normal"), QStringLiteral("Normal"),
          material.normalUvScale(), material.normalUvRotationDegrees(),
          material.normalUvOffsetU(), material.normalUvOffsetV(),
          material.normalUvSet(), -27);
      addTexturePath(QStringLiteral("clearcoatNormalTexture"),
          QStringLiteral("Coat Normal Texture"),
          material.clearcoatNormalTexture(), -22);
      addTexturePath(QStringLiteral("clearcoatTexture"),
          QStringLiteral("Clearcoat Texture"),
          material.clearcoatTexture(), -23);
      addTexturePath(QStringLiteral("sheenColorTexture"),
          QStringLiteral("Sheen Color Texture"),
          material.sheenColorTexture(), -22);
      addIndependentTextureUv(QStringLiteral("sheenColorTexture"),
          QStringLiteral("Sheen Color"),
          material.sheenColorTextureUvTransform(), -21);
      addTexturePath(QStringLiteral("sheenRoughnessTexture"),
          QStringLiteral("Sheen Roughness Texture"),
          material.sheenRoughnessTexture(), -20);
      addIndependentTextureUv(QStringLiteral("sheenRoughnessTexture"),
          QStringLiteral("Sheen Roughness"),
          material.sheenRoughnessTextureUvTransform(), -19);
      addTexturePath(QStringLiteral("anisotropyTexture"),
          QStringLiteral("Anisotropy Texture"), material.anisotropyTexture(), -18);
      addIndependentTextureUv(QStringLiteral("anisotropyTexture"),
          QStringLiteral("Anisotropy Texture"),
          material.anisotropyTextureUvTransform(), -17);
      addTextureUv(QStringLiteral("clearcoatTexture"),
          QStringLiteral("Clearcoat"),
          material.clearcoatTextureUvScale(),
          material.clearcoatTextureUvRotationDegrees(),
          material.clearcoatTextureUvOffsetU(),
          material.clearcoatTextureUvOffsetV(),
          material.clearcoatTextureUvSet(), -24);
      addTextureUv(QStringLiteral("clearcoatNormal"),
          QStringLiteral("Coat Normal"), material.clearcoatNormalUvScale(),
          material.clearcoatNormalUvRotationDegrees(),
          material.clearcoatNormalUvOffsetU(),
          material.clearcoatNormalUvOffsetV(),
          material.clearcoatNormalUvSet(), -21);
      addTexturePath(QStringLiteral("emissionTexture"),
          QStringLiteral("Emission Texture"), material.emissionTexture(),
          -16);
      addTextureUv(QStringLiteral("emission"), QStringLiteral("Emission"),
          material.emissionTextureUvScale(),
          material.emissionTextureUvRotationDegrees(),
          material.emissionTextureUvOffsetU(),
          material.emissionTextureUvOffsetV(),
          material.emissionTextureUvSet(), -15);
      addTexturePath(QStringLiteral("occlusionTexture"),
          QStringLiteral("Occlusion Texture"), material.occlusionTexture(),
          -10);
      addTextureUv(QStringLiteral("occlusion"), QStringLiteral("Occlusion"),
          material.occlusionTextureUvScale(),
          material.occlusionTextureUvRotationDegrees(),
          material.occlusionTextureUvOffsetU(),
          material.occlusionTextureUvOffsetV(),
          material.occlusionTextureUvSet(), -9);
      addTexturePath(QStringLiteral("opacityTexture"),
          QStringLiteral("Opacity Texture"), material.opacityTexture(), -4);
      addTextureUv(QStringLiteral("opacity"), QStringLiteral("Opacity"),
          material.opacityTextureUvScale(),
          material.opacityTextureUvRotationDegrees(),
          material.opacityTextureUvOffsetU(),
          material.opacityTextureUvOffsetV(),
          material.opacityTextureUvSet(), -3);
      groups.push_back(std::move(slotGroup));
    }
  }
  return groups;
}

bool Artifact3DLayer::setLayerPropertyValue(const QString &propertyPath,
                                            const QVariant &value) {
  const QString materialSlotPrefix = QStringLiteral("material.slots.");
  if (propertyPath.startsWith(materialSlotPrefix)) {
    const QString slotAndField = propertyPath.mid(materialSlotPrefix.size());
    const qsizetype separator = slotAndField.indexOf(QLatin1Char('.'));
    if (separator <= 0) return false;
    bool indexValid = false;
    const int slotIndex = slotAndField.left(separator).toInt(&indexValid);
    if (!indexValid || slotIndex < 0 ||
        static_cast<std::size_t>(slotIndex) >= impl_->meshMaterials_.size()) {
      return false;
    }
    auto& material = impl_->meshMaterials_[static_cast<std::size_t>(slotIndex)];
    const QString field = slotAndField.mid(separator + 1);
    const auto setTexturePath = [&](const QString& textureField,
        void (ArtifactCore::Material::*setter)(const ArtifactCore::UniString&)) {
      if (field != textureField) return false;
      (material.*setter)(ArtifactCore::UniString::fromQString(value.toString()));
      return true;
    };
    if (setTexturePath(QStringLiteral("baseColorTexture"),
            &ArtifactCore::Material::setBaseColorTexture) ||
        setTexturePath(QStringLiteral("metallicRoughnessTexture"),
            &ArtifactCore::Material::setMetallicRoughnessTexture) ||
        setTexturePath(QStringLiteral("metallicTexture"),
            &ArtifactCore::Material::setMetallicTexture) ||
        setTexturePath(QStringLiteral("roughnessTexture"),
            &ArtifactCore::Material::setRoughnessTexture) ||
        setTexturePath(QStringLiteral("normalTexture"),
            &ArtifactCore::Material::setNormalTexture) ||
        setTexturePath(QStringLiteral("clearcoatNormalTexture"),
            &ArtifactCore::Material::setClearcoatNormalTexture) ||
        setTexturePath(QStringLiteral("clearcoatTexture"),
            &ArtifactCore::Material::setClearcoatTexture) ||
        setTexturePath(QStringLiteral("clearcoatRoughnessTexture"),
            &ArtifactCore::Material::setClearcoatRoughnessTexture) ||
        setTexturePath(QStringLiteral("sheenColorTexture"),
            &ArtifactCore::Material::setSheenColorTexture) ||
        setTexturePath(QStringLiteral("sheenRoughnessTexture"),
            &ArtifactCore::Material::setSheenRoughnessTexture) ||
        setTexturePath(QStringLiteral("anisotropyTexture"),
            &ArtifactCore::Material::setAnisotropyTexture) ||
        setTexturePath(QStringLiteral("emissionTexture"),
            &ArtifactCore::Material::setEmissionTexture) ||
        setTexturePath(QStringLiteral("occlusionTexture"),
            &ArtifactCore::Material::setOcclusionTexture) ||
        setTexturePath(QStringLiteral("opacityTexture"),
            &ArtifactCore::Material::setOpacityTexture)) {
      // Texture paths are authored values and are validated by the renderer
      // when the material is bound.
    } else if (field == QStringLiteral("baseColor")) {
      material.setBaseColor(value.value<QColor>());
    } else if (field == QStringLiteral("specularTint")) {
      material.setSpecularTint(value.value<QColor>());
    } else if (field == QStringLiteral("sheenTint")) {
      material.setSheenTint(value.value<QColor>());
    } else if (field == QStringLiteral("clearcoatTint")) {
      material.setClearcoatTint(value.value<QColor>());
    } else if (field == QStringLiteral("subsurfaceColor")) {
      material.setSubsurfaceColor(value.value<QColor>());
    } else if (field == QStringLiteral("emissionColor")) {
      material.setEmissionColor(value.value<QColor>());
    } else {
      const auto scalar = [&]() {
        const float incoming = value.toFloat();
        return std::isfinite(incoming) ? incoming : 0.0f;
      };
      const float incoming = scalar();
      bool uvPropertyHandled = false;
      const auto updateTextureUv = [&](const QString& uvPrefix,
          auto scaleSetter, auto rotationSetter, auto offsetUSetter,
          auto offsetVSetter, auto uvSetSetter) {
        if (!field.startsWith(uvPrefix)) return;
        const QString uvField = field.mid(uvPrefix.size());
        if (uvField == QStringLiteral("scale"))
          (material.*scaleSetter)(std::clamp(incoming, 0.01f, 10.0f));
        else if (uvField == QStringLiteral("rotation"))
          (material.*rotationSetter)(std::clamp(incoming, -360.0f, 360.0f));
        else if (uvField == QStringLiteral("offsetU"))
          (material.*offsetUSetter)(std::clamp(incoming, -10.0f, 10.0f));
        else if (uvField == QStringLiteral("offsetV"))
          (material.*offsetVSetter)(std::clamp(incoming, -10.0f, 10.0f));
        else if (uvField == QStringLiteral("uvSet"))
          (material.*uvSetSetter)(std::clamp(value.toInt(), 0, 1));
        else return;
        uvPropertyHandled = true;
      };
      updateTextureUv(QStringLiteral("baseColorUv."),
          &ArtifactCore::Material::setBaseColorUvScale,
          &ArtifactCore::Material::setBaseColorUvRotationDegrees,
          &ArtifactCore::Material::setBaseColorUvOffsetU,
          &ArtifactCore::Material::setBaseColorUvOffsetV,
          &ArtifactCore::Material::setBaseColorUvSet);
      updateTextureUv(QStringLiteral("metallicRoughnessUv."),
          &ArtifactCore::Material::setMetallicRoughnessUvScale,
          &ArtifactCore::Material::setMetallicRoughnessUvRotationDegrees,
          &ArtifactCore::Material::setMetallicRoughnessUvOffsetU,
          &ArtifactCore::Material::setMetallicRoughnessUvOffsetV,
          &ArtifactCore::Material::setMetallicRoughnessUvSet);
      updateTextureUv(QStringLiteral("normalUv."),
          &ArtifactCore::Material::setNormalUvScale,
          &ArtifactCore::Material::setNormalUvRotationDegrees,
          &ArtifactCore::Material::setNormalUvOffsetU,
          &ArtifactCore::Material::setNormalUvOffsetV,
          &ArtifactCore::Material::setNormalUvSet);
      updateTextureUv(QStringLiteral("clearcoatNormalUv."),
          &ArtifactCore::Material::setClearcoatNormalUvScale,
          &ArtifactCore::Material::setClearcoatNormalUvRotationDegrees,
          &ArtifactCore::Material::setClearcoatNormalUvOffsetU,
          &ArtifactCore::Material::setClearcoatNormalUvOffsetV,
          &ArtifactCore::Material::setClearcoatNormalUvSet);
      updateTextureUv(QStringLiteral("clearcoatTextureUv."),
          &ArtifactCore::Material::setClearcoatTextureUvScale,
          &ArtifactCore::Material::setClearcoatTextureUvRotationDegrees,
          &ArtifactCore::Material::setClearcoatTextureUvOffsetU,
          &ArtifactCore::Material::setClearcoatTextureUvOffsetV,
          &ArtifactCore::Material::setClearcoatTextureUvSet);
      updateTextureUv(QStringLiteral("clearcoatRoughnessTextureUv."),
          &ArtifactCore::Material::setClearcoatRoughnessTextureUvScale,
          &ArtifactCore::Material::setClearcoatRoughnessTextureUvRotationDegrees,
          &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetU,
          &ArtifactCore::Material::setClearcoatRoughnessTextureUvOffsetV,
          &ArtifactCore::Material::setClearcoatRoughnessTextureUvSet);
      updateTextureUv(QStringLiteral("emissionUv."),
          &ArtifactCore::Material::setEmissionTextureUvScale,
          &ArtifactCore::Material::setEmissionTextureUvRotationDegrees,
          &ArtifactCore::Material::setEmissionTextureUvOffsetU,
          &ArtifactCore::Material::setEmissionTextureUvOffsetV,
          &ArtifactCore::Material::setEmissionTextureUvSet);
      updateTextureUv(QStringLiteral("occlusionUv."),
          &ArtifactCore::Material::setOcclusionTextureUvScale,
          &ArtifactCore::Material::setOcclusionTextureUvRotationDegrees,
          &ArtifactCore::Material::setOcclusionTextureUvOffsetU,
          &ArtifactCore::Material::setOcclusionTextureUvOffsetV,
          &ArtifactCore::Material::setOcclusionTextureUvSet);
      updateTextureUv(QStringLiteral("opacityUv."),
          &ArtifactCore::Material::setOpacityTextureUvScale,
          &ArtifactCore::Material::setOpacityTextureUvRotationDegrees,
          &ArtifactCore::Material::setOpacityTextureUvOffsetU,
          &ArtifactCore::Material::setOpacityTextureUvOffsetV,
          &ArtifactCore::Material::setOpacityTextureUvSet);
      const auto updateIndependentTextureUv = [&](const QString& uvPrefix,
          auto setter, auto getter) {
        if (!field.startsWith(uvPrefix)) return;
        const QString uvField = field.mid(uvPrefix.size());
        auto transform = (material.*getter)();
        if (uvField == QStringLiteral("scaleU"))
          transform.scaleU = std::clamp(incoming, 0.01f, 10.0f);
        else if (uvField == QStringLiteral("scaleV"))
          transform.scaleV = std::clamp(incoming, 0.01f, 10.0f);
        else if (uvField == QStringLiteral("rotation"))
          transform.rotationDegrees = std::clamp(incoming, -360.0f, 360.0f);
        else if (uvField == QStringLiteral("offsetU"))
          transform.offsetU = std::clamp(incoming, -10.0f, 10.0f);
        else if (uvField == QStringLiteral("offsetV"))
          transform.offsetV = std::clamp(incoming, -10.0f, 10.0f);
        else if (uvField == QStringLiteral("uvSet"))
          transform.uvSet = std::clamp(value.toInt(), 0, 1);
        else return;
        (material.*setter)(transform);
        uvPropertyHandled = true;
      };
      updateIndependentTextureUv(QStringLiteral("metallicTextureUv."),
          &ArtifactCore::Material::setMetallicTextureUvTransform,
          &ArtifactCore::Material::metallicTextureUvTransform);
      updateIndependentTextureUv(QStringLiteral("roughnessTextureUv."),
          &ArtifactCore::Material::setRoughnessTextureUvTransform,
          &ArtifactCore::Material::roughnessTextureUvTransform);
      updateIndependentTextureUv(QStringLiteral("sheenColorTextureUv."),
          &ArtifactCore::Material::setSheenColorTextureUvTransform,
          &ArtifactCore::Material::sheenColorTextureUvTransform);
      updateIndependentTextureUv(QStringLiteral("sheenRoughnessTextureUv."),
          &ArtifactCore::Material::setSheenRoughnessTextureUvTransform,
          &ArtifactCore::Material::sheenRoughnessTextureUvTransform);
      updateIndependentTextureUv(QStringLiteral("anisotropyTextureUv."),
          &ArtifactCore::Material::setAnisotropyTextureUvTransform,
          &ArtifactCore::Material::anisotropyTextureUvTransform);
      if (uvPropertyHandled) {
        Q_EMIT changed();
        return true;
      }
      if (field == QStringLiteral("metallic")) {
        material.setMetallic(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("roughness")) {
        material.setRoughness(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("specular")) {
        material.setSpecular(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("diffuseRoughness")) {
        material.setDiffuseRoughness(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("thinFilmThickness")) {
        material.setThinFilm(std::clamp(incoming, 0.0f, 1000.0f),
                             material.thinFilmIOR());
      } else if (field == QStringLiteral("thinFilmIOR")) {
        material.setThinFilm(material.thinFilmThickness(),
                             std::clamp(incoming, 1.0f, 2.5f));
      } else if (field == QStringLiteral("ior")) {
        material.setIOR(std::clamp(incoming, 1.0f, 3.0f));
      } else if (field == QStringLiteral("transmission")) {
        material.setTransmission(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("thinWall")) {
        material.setThinWall(value.toBool());
      } else if (field == QStringLiteral("clearcoat")) {
        material.setClearcoat(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("clearcoatRoughness")) {
        material.setClearcoatRoughness(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("clearcoatIOR")) {
        material.setClearcoatIOR(std::clamp(incoming, 1.0f, 2.0f));
      } else if (field == QStringLiteral("clearcoatNormalStrength")) {
        material.setClearcoatNormalStrength(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("sheen")) {
        material.setSheen(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("sheenRoughness")) {
        material.setSheenRoughness(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("subsurface")) {
        material.setSubsurface(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("subsurfaceIOR")) {
        material.setSubsurfaceIOR(std::clamp(incoming, 1.0f, 2.0f));
      } else if (field == QStringLiteral("subsurfaceAnisotropy")) {
        material.setSubsurfaceAnisotropy(std::clamp(incoming, -0.9f, 0.9f));
      } else if (field.startsWith(QStringLiteral("subsurfaceRadius."))) {
        QVector3D radius = material.subsurfaceRadius();
        const float component = std::clamp(incoming, 0.0f, 10.0f);
        if (field.endsWith(QLatin1Char('r'))) radius.setX(component);
        else if (field.endsWith(QLatin1Char('g'))) radius.setY(component);
        else if (field.endsWith(QLatin1Char('b'))) radius.setZ(component);
        else return false;
        material.setSubsurfaceRadius(radius);
      } else if (field == QStringLiteral("anisotropy")) {
        material.setAnisotropy(std::clamp(incoming, -1.0f, 1.0f),
                               material.anisotropyRotationDegrees());
      } else if (field == QStringLiteral("anisotropyRotation")) {
        material.setAnisotropy(material.anisotropy(),
                               std::clamp(incoming, -180.0f, 180.0f));
      } else if (field == QStringLiteral("emissionStrength")) {
        material.setEmissionStrength(std::clamp(incoming, 0.0f, 100000.0f));
      } else if (field == QStringLiteral("opacity")) {
        material.setOpacity(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("alphaMode")) {
        const int mode = std::clamp(value.toInt(),
            static_cast<int>(ArtifactCore::MaterialAlphaMode::Opaque),
            static_cast<int>(ArtifactCore::MaterialAlphaMode::Blended));
        material.setAlphaMode(
            static_cast<ArtifactCore::MaterialAlphaMode>(mode));
      } else if (field == QStringLiteral("alphaCutoff")) {
        material.setAlphaCutoff(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("normalStrength")) {
        material.setNormalStrength(std::clamp(incoming, 0.0f, 10.0f));
      } else if (field == QStringLiteral("occlusionStrength")) {
        material.setOcclusionStrength(std::clamp(incoming, 0.0f, 1.0f));
      } else if (field == QStringLiteral("environmentIntensity")) {
        material.setEnvironmentIntensity(std::clamp(incoming, 0.0f, 4.0f));
      } else if (field == QStringLiteral("uvOffsetU")) {
        material.setUvOffsetU(std::clamp(incoming, -10.0f, 10.0f));
      } else if (field == QStringLiteral("uvOffsetV")) {
        material.setUvOffsetV(std::clamp(incoming, -10.0f, 10.0f));
      } else if (field == QStringLiteral("uvScaleU")) {
        material.setUvScaleU(std::clamp(incoming, 0.01f, 10.0f));
      } else if (field == QStringLiteral("uvScaleV")) {
        material.setUvScaleV(std::clamp(incoming, 0.01f, 10.0f));
      } else if (field == QStringLiteral("uvRotation")) {
        material.setUvRotationDegrees(std::clamp(incoming, -360.0f, 360.0f));
      } else {
        return false;
      }
    }
    Q_EMIT changed();
    return true;
  }
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
  } else if (propertyPath == QStringLiteral("animation.playbackMode")) {
    impl_->animationPlaybackMode_ = std::clamp(value.toInt(), 0, 2);
    impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("animation.speed")) {
    const float speed = value.toFloat();
    impl_->animationSpeed_ =
        std::isfinite(speed) ? std::clamp(speed, 0.0f, 8.0f) : 1.0f;
    impl_->lastSkinAnimationFrame_ = std::numeric_limits<int64_t>::min();
    Q_EMIT changed();
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
  } else if (propertyPath.startsWith(
                 QStringLiteral("material.metallicTextureUv.")) ||
             propertyPath.startsWith(
                 QStringLiteral("material.roughnessTextureUv."))) {
    const bool metallicMap = propertyPath.startsWith(
        QStringLiteral("material.metallicTextureUv."));
    const auto setter = metallicMap
        ? &ArtifactCore::Material::setMetallicTextureUvTransform
        : &ArtifactCore::Material::setRoughnessTextureUvTransform;
    const auto getter = metallicMap
        ? &ArtifactCore::Material::metallicTextureUvTransform
        : &ArtifactCore::Material::roughnessTextureUvTransform;
    auto transform = (impl_->material_.*getter)();
    const QString field = propertyPath.section(QLatin1Char('.'), -1);
    const float incoming = value.toFloat();
    if (field == QStringLiteral("scaleU"))
      transform.scaleU = finiteClamped(incoming, transform.scaleU, 0.01f, 10.0f);
    else if (field == QStringLiteral("scaleV"))
      transform.scaleV = finiteClamped(incoming, transform.scaleV, 0.01f, 10.0f);
    else if (field == QStringLiteral("rotation"))
      transform.rotationDegrees = finiteClamped(
          incoming, transform.rotationDegrees, -360.0f, 360.0f);
    else if (field == QStringLiteral("offsetU"))
      transform.offsetU = finiteClamped(incoming, transform.offsetU, -10.0f, 10.0f);
    else if (field == QStringLiteral("offsetV"))
      transform.offsetV = finiteClamped(incoming, transform.offsetV, -10.0f, 10.0f);
    else if (field == QStringLiteral("uvSet"))
      transform.uvSet = std::clamp(value.toInt(), 0, 1);
    else
      return false;
    (impl_->material_.*setter)(transform);
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
  } else if (propertyPath == QStringLiteral("material.baseColorUvScale")) {
    impl_->material_.setBaseColorUvScale(finiteClamped(
        value.toFloat(), impl_->material_.baseColorUvScale(), 0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.baseColorUvSet")) {
    impl_->material_.setBaseColorUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.baseColorUvRotation")) {
    impl_->material_.setBaseColorUvRotationDegrees(finiteClamped(
        value.toFloat(), impl_->material_.baseColorUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.baseColorUvOffsetU")) {
    impl_->material_.setBaseColorUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.baseColorUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.baseColorUvOffsetV")) {
    impl_->material_.setBaseColorUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.baseColorUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.metallicRoughnessTexture")) {
    impl_->material_.setMetallicRoughnessTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.metallicTexture")) {
    impl_->material_.setMetallicTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.roughnessTexture")) {
    impl_->material_.setRoughnessTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.metallicRoughnessUvScale")) {
    impl_->material_.setMetallicRoughnessUvScale(finiteClamped(
        value.toFloat(), impl_->material_.metallicRoughnessUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.metallicRoughnessUvOffsetU")) {
    impl_->material_.setMetallicRoughnessUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.metallicRoughnessUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.metallicRoughnessUvOffsetV")) {
    impl_->material_.setMetallicRoughnessUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.metallicRoughnessUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.metallicRoughnessUvRotation")) {
    impl_->material_.setMetallicRoughnessUvRotationDegrees(finiteClamped(
        value.toFloat(),
        impl_->material_.metallicRoughnessUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.metallicRoughnessUvSet")) {
    impl_->material_.setMetallicRoughnessUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalTexture")) {
    impl_->material_.setNormalTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalUvRotation")) {
    impl_->material_.setNormalUvRotationDegrees(finiteClamped(
        value.toFloat(), impl_->material_.normalUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalUvSet")) {
    impl_->material_.setNormalUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalUvScale")) {
    impl_->material_.setNormalUvScale(finiteClamped(
        value.toFloat(), impl_->material_.normalUvScale(), 0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalUvOffsetU")) {
    impl_->material_.setNormalUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.normalUvOffsetU(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalUvOffsetV")) {
    impl_->material_.setNormalUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.normalUvOffsetV(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalTexture")) {
    impl_->material_.setClearcoatNormalTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTexture")) {
    impl_->material_.setClearcoatTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.sheenColorTexture")) {
    impl_->material_.setSheenColorTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.sheenRoughnessTexture")) {
    impl_->material_.setSheenRoughnessTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath.startsWith(
                 QStringLiteral("material.sheenRoughnessTextureUv."))) {
    auto transform = impl_->material_.sheenRoughnessTextureUvTransform();
    const QString field = propertyPath.mid(
        QStringLiteral("material.sheenRoughnessTextureUv.").size());
    if (field == QStringLiteral("scaleU"))
      transform.scaleU = finiteClamped(value.toFloat(), transform.scaleU,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("scaleV"))
      transform.scaleV = finiteClamped(value.toFloat(), transform.scaleV,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("rotation"))
      transform.rotationDegrees = finiteClamped(value.toFloat(),
          transform.rotationDegrees, -360.0f, 360.0f);
    else if (field == QStringLiteral("offsetU"))
      transform.offsetU = finiteClamped(value.toFloat(), transform.offsetU,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("offsetV"))
      transform.offsetV = finiteClamped(value.toFloat(), transform.offsetV,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("uvSet"))
      transform.uvSet = value.toInt() == 1 ? 1 : 0;
    else
      return false;
    impl_->material_.setSheenRoughnessTextureUvTransform(transform);
    Q_EMIT changed();
    return true;
  } else if (propertyPath.startsWith(
                 QStringLiteral("material.sheenColorTextureUv."))) {
    auto transform = impl_->material_.sheenColorTextureUvTransform();
    const QString field = propertyPath.mid(
        QStringLiteral("material.sheenColorTextureUv.").size());
    if (field == QStringLiteral("scaleU"))
      transform.scaleU = finiteClamped(value.toFloat(), transform.scaleU,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("scaleV"))
      transform.scaleV = finiteClamped(value.toFloat(), transform.scaleV,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("rotation"))
      transform.rotationDegrees = finiteClamped(value.toFloat(),
          transform.rotationDegrees, -360.0f, 360.0f);
    else if (field == QStringLiteral("offsetU"))
      transform.offsetU = finiteClamped(value.toFloat(), transform.offsetU,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("offsetV"))
      transform.offsetV = finiteClamped(value.toFloat(), transform.offsetV,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("uvSet"))
      transform.uvSet = value.toInt() == 1 ? 1 : 0;
    else
      return false;
    impl_->material_.setSheenColorTextureUvTransform(transform);
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatRoughnessTexture")) {
    impl_->material_.setClearcoatRoughnessTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatRoughnessTextureUvScale")) {
    impl_->material_.setClearcoatRoughnessTextureUvScale(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatRoughnessTextureUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral(
                 "material.clearcoatRoughnessTextureUvRotation")) {
    impl_->material_.setClearcoatRoughnessTextureUvRotationDegrees(finiteClamped(
        value.toFloat(),
        impl_->material_.clearcoatRoughnessTextureUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral(
                 "material.clearcoatRoughnessTextureUvOffsetU")) {
    impl_->material_.setClearcoatRoughnessTextureUvOffsetU(finiteClamped(
        value.toFloat(),
        impl_->material_.clearcoatRoughnessTextureUvOffsetU(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral(
                 "material.clearcoatRoughnessTextureUvOffsetV")) {
    impl_->material_.setClearcoatRoughnessTextureUvOffsetV(finiteClamped(
        value.toFloat(),
        impl_->material_.clearcoatRoughnessTextureUvOffsetV(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral(
                 "material.clearcoatRoughnessTextureUvSet")) {
    impl_->material_.setClearcoatRoughnessTextureUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTextureUvScale")) {
    impl_->material_.setClearcoatTextureUvScale(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatTextureUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTextureUvRotation")) {
    impl_->material_.setClearcoatTextureUvRotationDegrees(finiteClamped(
        value.toFloat(),
        impl_->material_.clearcoatTextureUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTextureUvOffsetU")) {
    impl_->material_.setClearcoatTextureUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatTextureUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTextureUvOffsetV")) {
    impl_->material_.setClearcoatTextureUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatTextureUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatTextureUvSet")) {
    impl_->material_.setClearcoatTextureUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalUvRotation")) {
    impl_->material_.setClearcoatNormalUvRotationDegrees(finiteClamped(
        value.toFloat(),
        impl_->material_.clearcoatNormalUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalUvScale")) {
    impl_->material_.setClearcoatNormalUvScale(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatNormalUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalUvOffsetU")) {
    impl_->material_.setClearcoatNormalUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatNormalUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalUvOffsetV")) {
    impl_->material_.setClearcoatNormalUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatNormalUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalUvSet")) {
    impl_->material_.setClearcoatNormalUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emissionTexture")) {
    impl_->material_.setEmissionTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.emissionTextureUvScale")) {
    impl_->material_.setEmissionTextureUvScale(finiteClamped(
        value.toFloat(), impl_->material_.emissionTextureUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.emissionTextureUvOffsetU")) {
    impl_->material_.setEmissionTextureUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.emissionTextureUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.emissionTextureUvOffsetV")) {
    impl_->material_.setEmissionTextureUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.emissionTextureUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.emissionTextureUvRotation")) {
    impl_->material_.setEmissionTextureUvRotationDegrees(finiteClamped(
        value.toFloat(),
        impl_->material_.emissionTextureUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.emissionTextureUvSet")) {
    impl_->material_.setEmissionTextureUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.occlusionTexture")) {
    impl_->material_.setOcclusionTexture(ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.occlusionTextureUvScale")) {
    impl_->material_.setOcclusionTextureUvScale(finiteClamped(
        value.toFloat(), impl_->material_.occlusionTextureUvScale(),
        0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.occlusionTextureUvOffsetU")) {
    impl_->material_.setOcclusionTextureUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.occlusionTextureUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.occlusionTextureUvOffsetV")) {
    impl_->material_.setOcclusionTextureUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.occlusionTextureUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.occlusionTextureUvRotation")) {
    impl_->material_.setOcclusionTextureUvRotationDegrees(finiteClamped(
        value.toFloat(), impl_->material_.occlusionTextureUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.occlusionTextureUvSet")) {
    impl_->material_.setOcclusionTextureUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.opacityTexture")) {
    impl_->material_.setOpacityTexture(ArtifactCore::UniString::fromQString(value.toString()));
  } else if (propertyPath == QStringLiteral("material.opacityTextureUvScale")) {
    impl_->material_.setOpacityTextureUvScale(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.opacityTextureUvOffsetU")) {
    impl_->material_.setOpacityTextureUvOffsetU(finiteClamped(
        value.toFloat(), impl_->material_.opacityTextureUvOffsetU(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.opacityTextureUvOffsetV")) {
    impl_->material_.setOpacityTextureUvOffsetV(finiteClamped(
        value.toFloat(), impl_->material_.opacityTextureUvOffsetV(),
        -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.opacityTextureUvRotation")) {
    impl_->material_.setOpacityTextureUvRotationDegrees(finiteClamped(
        value.toFloat(), impl_->material_.opacityTextureUvRotationDegrees(),
        -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.opacityTextureUvSet")) {
    impl_->material_.setOpacityTextureUvSet(value.toInt());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emission.color")) {
    impl_->material_.setEmissionColor(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.sheenTint")) {
    impl_->material_.setSheenTint(value.value<QColor>());
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
  } else if (propertyPath == QStringLiteral("material.diffuseRoughness")) {
    impl_->material_.setDiffuseRoughness(finiteClamped(
        value.toFloat(), impl_->material_.diffuseRoughness(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.specular")) {
    impl_->material_.setSpecular(finiteClamped(value.toFloat(), impl_->material_.specular(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.specularTint")) {
    impl_->material_.setSpecularTint(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.thinFilmThickness")) {
    impl_->material_.setThinFilm(
        finiteClamped(value.toFloat(), impl_->material_.thinFilmThickness(),
                      0.0f, 1000.0f),
        impl_->material_.thinFilmIOR());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.thinFilmIOR")) {
    impl_->material_.setThinFilm(
        impl_->material_.thinFilmThickness(),
        finiteClamped(value.toFloat(), impl_->material_.thinFilmIOR(),
                      1.0f, 2.5f));
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
  } else if (propertyPath == QStringLiteral("material.thinWall")) {
    impl_->material_.setThinWall(value.toBool());
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
  } else if (propertyPath == QStringLiteral("material.clearcoatIOR")) {
    impl_->material_.setClearcoatIOR(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatIOR(), 1.0f, 2.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.clearcoatTint")) {
    impl_->material_.setClearcoatTint(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.clearcoatNormalStrength")) {
    impl_->material_.setClearcoatNormalStrength(finiteClamped(
        value.toFloat(), impl_->material_.clearcoatNormalStrength(),
        0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.sheen")) {
    impl_->material_.setSheen(finiteClamped(value.toFloat(), impl_->material_.sheen(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.subsurface")) {
    impl_->material_.setSubsurface(finiteClamped(
        value.toFloat(), impl_->material_.subsurface(), 0.0f, 1.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.subsurfaceIOR")) {
    impl_->material_.setSubsurfaceIOR(finiteClamped(
        value.toFloat(), impl_->material_.subsurfaceIOR(), 1.0f, 2.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.subsurfaceAnisotropy")) {
    impl_->material_.setSubsurfaceAnisotropy(finiteClamped(
        value.toFloat(), impl_->material_.subsurfaceAnisotropy(),
        -0.9f, 0.9f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.subsurfaceColor")) {
    impl_->material_.setSubsurfaceColor(value.value<QColor>());
    Q_EMIT changed();
    return true;
  } else if (propertyPath.startsWith(QStringLiteral("material.subsurfaceRadius."))) {
    QVector3D radius = impl_->material_.subsurfaceRadius();
    const float component = finiteClamped(value.toFloat(), 1.0f, 0.0f, 10.0f);
    if (propertyPath.endsWith(QLatin1Char('r'))) radius.setX(component);
    else if (propertyPath.endsWith(QLatin1Char('g'))) radius.setY(component);
    else if (propertyPath.endsWith(QLatin1Char('b'))) radius.setZ(component);
    impl_->material_.setSubsurfaceRadius(radius);
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.anisotropy")) {
    impl_->material_.setAnisotropy(
        finiteClamped(value.toFloat(), impl_->material_.anisotropy(),
                      -1.0f, 1.0f),
        impl_->material_.anisotropyRotationDegrees());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.anisotropyTexture")) {
    impl_->material_.setAnisotropyTexture(
        ArtifactCore::UniString::fromQString(value.toString()));
    Q_EMIT changed();
    return true;
  } else if (propertyPath.startsWith(
                 QStringLiteral("material.anisotropyTextureUv."))) {
    auto transform = impl_->material_.anisotropyTextureUvTransform();
    const QString field = propertyPath.mid(
        QStringLiteral("material.anisotropyTextureUv.").size());
    if (field == QStringLiteral("scaleU"))
      transform.scaleU = finiteClamped(value.toFloat(), transform.scaleU,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("scaleV"))
      transform.scaleV = finiteClamped(value.toFloat(), transform.scaleV,
                                       0.0001f, 10000.0f);
    else if (field == QStringLiteral("rotation"))
      transform.rotationDegrees = finiteClamped(value.toFloat(),
          transform.rotationDegrees, -360.0f, 360.0f);
    else if (field == QStringLiteral("offsetU"))
      transform.offsetU = finiteClamped(value.toFloat(), transform.offsetU,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("offsetV"))
      transform.offsetV = finiteClamped(value.toFloat(), transform.offsetV,
                                        -1000000.0f, 1000000.0f);
    else if (field == QStringLiteral("uvSet"))
      transform.uvSet = std::clamp(value.toInt(), 0, 1);
    else
      return false;
    impl_->material_.setAnisotropyTextureUvTransform(transform);
    Q_EMIT changed();
    return true;
  } else if (propertyPath ==
             QStringLiteral("material.anisotropyRotationDegrees")) {
    impl_->material_.setAnisotropy(
        impl_->material_.anisotropy(),
        finiteClamped(value.toFloat(),
                      impl_->material_.anisotropyRotationDegrees(),
                      -180.0f, 180.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.sheenRoughness")) {
    impl_->material_.setSheenRoughness(finiteClamped(
        value.toFloat(), impl_->material_.sheenRoughness(), 0.0f, 1.0f));
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
  } else if (propertyPath == QStringLiteral("material.uv.offsetU")) {
    impl_->material_.setUvOffsetU(finiteClamped(value.toFloat(), impl_->material_.uvOffsetU(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.uv.offsetV")) {
    impl_->material_.setUvOffsetV(finiteClamped(value.toFloat(), impl_->material_.uvOffsetV(), -10.0f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.uv.scaleU")) {
    impl_->material_.setUvScaleU(finiteClamped(value.toFloat(), impl_->material_.uvScaleU(), 0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.uv.scaleV")) {
    impl_->material_.setUvScaleV(finiteClamped(value.toFloat(), impl_->material_.uvScaleV(), 0.01f, 10.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.uv.rotation")) {
    impl_->material_.setUvRotationDegrees(finiteClamped(value.toFloat(), impl_->material_.uvRotationDegrees(), -360.0f, 360.0f));
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.envIntensity")) {
    impl_->material_.setEnvironmentIntensity(finiteClamped(value.toFloat(), impl_->material_.environmentIntensity(), 0.0f, 4.0f));
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
  const QColor specularTint = impl_->material_.specularTint();
  const QColor sheenTint = impl_->material_.sheenTint();
  const QColor subsurfaceColor = impl_->material_.subsurfaceColor();
  const QVector3D subsurfaceRadius = impl_->material_.subsurfaceRadius();
  const QColor clearcoatTint = impl_->material_.clearcoatTint();
  QString signature = QStringLiteral(
             "src=%1|baseTex=%2|mrTex=%3|normalTex=%4|emissionTex=%5|occTex=%6|opacityTex=%7|"
             "base=%8,%9,%10,%11|emission=%12,%13,%14,%15|metallic=%16|roughness=%17|"
             "specular=%18|ior=%19|transmission=%20|clearcoat=%21|clearcoatRoughness=%22|"
             "alphaMode=%23|alphaCutoff=%24|emissionStrength=%25|opacity=%26|"
             "normalStrength=%27|occlusionStrength=%28|sheen=%29|solidTexture=%30|"
             "wireOverlay=%31|graph=%32|uv=%33,%34,%35,%36,%37|env=%38|"
             "sheenRoughness=%39|sheenTint=%40,%41,%42,%43|"
             "anisotropy=%44|anisotropyRotation=%45|subsurface=%46|"
             "subsurfaceColor=%47,%48,%49|clearcoatIOR=%50|"
             "clearcoatTint=%51,%52,%53|clearcoatNormalStrength=%54|"
             "subsurfaceRadius=%55,%56,%57|specularTint=%58,%59,%60|"
             "thinFilm=%61,%62|subsurfaceIOR=%63|subsurfaceAnisotropy=%64|"
             "clearcoatNormalTexture=%65|diffuseRoughness=%66|normalUvRotation=%67|"
             "normalUvScale=%68|coatNormalUv=%69,%70|baseColorUvScale=%71|"
             "metallicRoughnessUvScale=%72|normalUvOffset=%73,%74|"
             "occlusionTextureUvScale=%75|emissionTextureUvScale=%76|"
             "opacityTextureUvScale=%77|coatNormalUvOffset=%78,%79|"
             "metallicRoughnessUvOffset=%80,%81|emissionTextureUvOffset=%82,%83|"
             "occlusionTextureUvOffset=%84,%85|baseColorUvOffset=%86,%87|"
             "opacityTextureUvOffset=%88,%89|baseColorUvRotation=%90|"
             "emissionTextureUvRotation=%91|metallicRoughnessUvRotation=%92|"
             "opacityTextureUvRotation=%93|occlusionTextureUvRotation=%94|"
             "normalUvSet=%95|baseColorUvSet=%96|mapUvSets=%97")
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
      .arg(impl_->material_.materialGraphJson().toQString())
      .arg(impl_->material_.uvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.uvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.uvScaleU(), 0, 'f', 6)
      .arg(impl_->material_.uvScaleV(), 0, 'f', 6)
      .arg(impl_->material_.uvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.environmentIntensity(), 0, 'f', 6)
      .arg(impl_->material_.sheenRoughness(), 0, 'f', 6)
      .arg(sheenTint.redF(), 0, 'f', 6)
      .arg(sheenTint.greenF(), 0, 'f', 6)
      .arg(sheenTint.blueF(), 0, 'f', 6)
      .arg(impl_->material_.anisotropy(), 0, 'f', 6)
      .arg(impl_->material_.anisotropyRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.subsurface(), 0, 'f', 6)
      .arg(subsurfaceColor.redF(), 0, 'f', 6)
      .arg(subsurfaceColor.greenF(), 0, 'f', 6)
      .arg(subsurfaceColor.blueF(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatIOR(), 0, 'f', 6)
      .arg(clearcoatTint.redF(), 0, 'f', 6)
      .arg(clearcoatTint.greenF(), 0, 'f', 6)
      .arg(clearcoatTint.blueF(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalStrength(), 0, 'f', 6)
      .arg(subsurfaceRadius.x(), 0, 'f', 6)
      .arg(subsurfaceRadius.y(), 0, 'f', 6)
      .arg(subsurfaceRadius.z(), 0, 'f', 6)
      .arg(specularTint.redF(), 0, 'f', 6)
      .arg(specularTint.greenF(), 0, 'f', 6)
      .arg(specularTint.blueF(), 0, 'f', 6)
      .arg(impl_->material_.thinFilmThickness(), 0, 'f', 6)
      .arg(impl_->material_.thinFilmIOR(), 0, 'f', 6)
      .arg(impl_->material_.subsurfaceIOR(), 0, 'f', 6)
      .arg(impl_->material_.subsurfaceAnisotropy(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalTexture().toQString())
      .arg(impl_->material_.diffuseRoughness(), 0, 'f', 6)
      .arg(impl_->material_.normalUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.normalUvScale(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalUvScale(), 0, 'f', 6)
      .arg(impl_->material_.baseColorUvScale(), 0, 'f', 6)
      .arg(impl_->material_.metallicRoughnessUvScale(), 0, 'f', 6)
      .arg(impl_->material_.normalUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.normalUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.occlusionTextureUvScale(), 0, 'f', 6)
      .arg(impl_->material_.emissionTextureUvScale(), 0, 'f', 6)
      .arg(impl_->material_.opacityTextureUvScale(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatNormalUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.metallicRoughnessUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.metallicRoughnessUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.emissionTextureUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.emissionTextureUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.occlusionTextureUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.occlusionTextureUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.baseColorUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.baseColorUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.opacityTextureUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.opacityTextureUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.baseColorUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.emissionTextureUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.metallicRoughnessUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.opacityTextureUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.occlusionTextureUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.normalUvSet())
      .arg(impl_->material_.baseColorUvSet())
      .arg(QStringLiteral("%1,%2,%3,%4,%5")
               .arg(impl_->material_.metallicRoughnessUvSet())
               .arg(impl_->material_.occlusionTextureUvSet())
               .arg(impl_->material_.emissionTextureUvSet())
               .arg(impl_->material_.opacityTextureUvSet())
               .arg(impl_->material_.clearcoatNormalUvSet()));
  signature += QStringLiteral("|thinWall=%1|metallicTexture=%2|roughnessTexture=%3")
      .arg(impl_->material_.thinWall() ? 1 : 0)
      .arg(impl_->material_.metallicTexture().toQString())
      .arg(impl_->material_.roughnessTexture().toQString());
  signature += QStringLiteral("|clearcoatTexture=%1|clearcoatTextureUv=%2,%3,%4,%5,%6")
      .arg(impl_->material_.clearcoatTexture().toQString())
      .arg(impl_->material_.clearcoatTextureUvScale(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatTextureUvRotationDegrees(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatTextureUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatTextureUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatTextureUvSet());
  signature += QStringLiteral(
      "|clearcoatRoughnessTexture=%1|clearcoatRoughnessTextureUv=%2,%3,%4,%5,%6")
      .arg(impl_->material_.clearcoatRoughnessTexture().toQString())
      .arg(impl_->material_.clearcoatRoughnessTextureUvScale(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatRoughnessTextureUvRotationDegrees(),
           0, 'f', 6)
      .arg(impl_->material_.clearcoatRoughnessTextureUvOffsetU(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatRoughnessTextureUvOffsetV(), 0, 'f', 6)
      .arg(impl_->material_.clearcoatRoughnessTextureUvSet());
  const auto sheenColorTextureUv =
      impl_->material_.sheenColorTextureUvTransform();
  signature += QStringLiteral(
      "|sheenColorTexture=%1|sheenColorTextureUv=%2,%3,%4,%5,%6,%7")
      .arg(impl_->material_.sheenColorTexture().toQString())
      .arg(sheenColorTextureUv.scaleU, 0, 'f', 6)
      .arg(sheenColorTextureUv.scaleV, 0, 'f', 6)
      .arg(sheenColorTextureUv.rotationDegrees, 0, 'f', 6)
      .arg(sheenColorTextureUv.offsetU, 0, 'f', 6)
      .arg(sheenColorTextureUv.offsetV, 0, 'f', 6)
      .arg(sheenColorTextureUv.uvSet);
  const auto sheenRoughnessTextureUv =
      impl_->material_.sheenRoughnessTextureUvTransform();
  signature += QStringLiteral(
      "|sheenRoughnessTexture=%1|sheenRoughnessTextureUv=%2,%3,%4,%5,%6,%7")
      .arg(impl_->material_.sheenRoughnessTexture().toQString())
      .arg(sheenRoughnessTextureUv.scaleU, 0, 'f', 6)
      .arg(sheenRoughnessTextureUv.scaleV, 0, 'f', 6)
      .arg(sheenRoughnessTextureUv.rotationDegrees, 0, 'f', 6)
      .arg(sheenRoughnessTextureUv.offsetU, 0, 'f', 6)
      .arg(sheenRoughnessTextureUv.offsetV, 0, 'f', 6)
      .arg(sheenRoughnessTextureUv.uvSet);
  const auto anisotropyTextureUv =
      impl_->material_.anisotropyTextureUvTransform();
  signature += QStringLiteral(
      "|anisotropyTexture=%1|anisotropyTextureUv=%2,%3,%4,%5,%6,%7")
      .arg(impl_->material_.anisotropyTexture().toQString())
      .arg(anisotropyTextureUv.scaleU, 0, 'f', 6)
      .arg(anisotropyTextureUv.scaleV, 0, 'f', 6)
      .arg(anisotropyTextureUv.rotationDegrees, 0, 'f', 6)
      .arg(anisotropyTextureUv.offsetU, 0, 'f', 6)
      .arg(anisotropyTextureUv.offsetV, 0, 'f', 6)
      .arg(anisotropyTextureUv.uvSet);
  const auto metallicTextureUv = impl_->material_.metallicTextureUvTransform();
  const auto roughnessTextureUv = impl_->material_.roughnessTextureUvTransform();
  signature += QStringLiteral("|metallicMapUv=%1,%2,%3,%4,%5,%6|"
                              "roughnessMapUv=%7,%8,%9,%10,%11,%12")
      .arg(metallicTextureUv.scaleU, 0, 'f', 6)
      .arg(metallicTextureUv.scaleV, 0, 'f', 6)
      .arg(metallicTextureUv.rotationDegrees, 0, 'f', 6)
      .arg(metallicTextureUv.offsetU, 0, 'f', 6)
      .arg(metallicTextureUv.offsetV, 0, 'f', 6)
      .arg(metallicTextureUv.uvSet)
      .arg(roughnessTextureUv.scaleU, 0, 'f', 6)
      .arg(roughnessTextureUv.scaleV, 0, 'f', 6)
      .arg(roughnessTextureUv.rotationDegrees, 0, 'f', 6)
      .arg(roughnessTextureUv.offsetU, 0, 'f', 6)
      .arg(roughnessTextureUv.offsetV, 0, 'f', 6)
      .arg(roughnessTextureUv.uvSet);
  for (std::size_t index = 0; index < impl_->meshMaterials_.size(); ++index) {
    const auto& material = impl_->meshMaterials_[index];
    const auto metallicUv = material.metallicTextureUvTransform();
    const auto roughnessUv = material.roughnessTextureUvTransform();
    signature += QStringLiteral("|slot%1Maps=%2,%3,%4,%5")
        .arg(static_cast<qulonglong>(index))
        .arg(material.metallicRoughnessTexture().toQString())
        .arg(material.metallicTexture().toQString())
        .arg(material.roughnessTexture().toQString())
        .arg(material.thinWall() ? 1 : 0);
    signature += QStringLiteral("|slot%1ClearcoatMap=%2,%3,%4,%5,%6,%7")
        .arg(static_cast<qulonglong>(index))
        .arg(material.clearcoatTexture().toQString())
        .arg(material.clearcoatTextureUvScale(), 0, 'f', 6)
        .arg(material.clearcoatTextureUvRotationDegrees(), 0, 'f', 6)
        .arg(material.clearcoatTextureUvOffsetU(), 0, 'f', 6)
        .arg(material.clearcoatTextureUvOffsetV(), 0, 'f', 6)
        .arg(material.clearcoatTextureUvSet());
    const auto slotSheenUv = material.sheenColorTextureUvTransform();
    signature += QStringLiteral("|slot%1SheenColorMap=%2,%3,%4,%5,%6,%7,%8")
        .arg(static_cast<qulonglong>(index))
        .arg(material.sheenColorTexture().toQString())
        .arg(slotSheenUv.scaleU, 0, 'f', 6)
        .arg(slotSheenUv.scaleV, 0, 'f', 6)
        .arg(slotSheenUv.rotationDegrees, 0, 'f', 6)
        .arg(slotSheenUv.offsetU, 0, 'f', 6)
        .arg(slotSheenUv.offsetV, 0, 'f', 6)
        .arg(slotSheenUv.uvSet);
    const auto slotSheenRoughnessUv =
        material.sheenRoughnessTextureUvTransform();
    signature += QStringLiteral(
        "|slot%1SheenRoughnessMap=%2,%3,%4,%5,%6,%7,%8")
        .arg(static_cast<qulonglong>(index))
        .arg(material.sheenRoughnessTexture().toQString())
        .arg(slotSheenRoughnessUv.scaleU, 0, 'f', 6)
        .arg(slotSheenRoughnessUv.scaleV, 0, 'f', 6)
        .arg(slotSheenRoughnessUv.rotationDegrees, 0, 'f', 6)
        .arg(slotSheenRoughnessUv.offsetU, 0, 'f', 6)
        .arg(slotSheenRoughnessUv.offsetV, 0, 'f', 6)
        .arg(slotSheenRoughnessUv.uvSet);
    const auto slotAnisotropyUv = material.anisotropyTextureUvTransform();
    signature += QStringLiteral(
        "|slot%1AnisotropyMap=%2,%3,%4,%5,%6,%7,%8")
        .arg(static_cast<qulonglong>(index))
        .arg(material.anisotropyTexture().toQString())
        .arg(slotAnisotropyUv.scaleU, 0, 'f', 6)
        .arg(slotAnisotropyUv.scaleV, 0, 'f', 6)
        .arg(slotAnisotropyUv.rotationDegrees, 0, 'f', 6)
        .arg(slotAnisotropyUv.offsetU, 0, 'f', 6)
        .arg(slotAnisotropyUv.offsetV, 0, 'f', 6)
        .arg(slotAnisotropyUv.uvSet);
    signature += QStringLiteral("|slot%1PbrUv=%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
        .arg(static_cast<qulonglong>(index))
        .arg(metallicUv.scaleU, 0, 'f', 6)
        .arg(metallicUv.scaleV, 0, 'f', 6)
        .arg(metallicUv.rotationDegrees, 0, 'f', 6)
        .arg(metallicUv.offsetU, 0, 'f', 6)
        .arg(metallicUv.offsetV, 0, 'f', 6)
        .arg(metallicUv.uvSet)
        .arg(roughnessUv.scaleU, 0, 'f', 6)
        .arg(roughnessUv.scaleV, 0, 'f', 6)
        .arg(roughnessUv.rotationDegrees, 0, 'f', 6)
        .arg(roughnessUv.offsetU, 0, 'f', 6)
        .arg(roughnessUv.offsetV, 0, 'f', 6)
        .arg(roughnessUv.uvSet);
  }
  return signature;
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
