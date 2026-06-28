module;
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QSizeF>
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QVector>
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

namespace Artifact {

namespace {
Artifact::Detail::float3 toFloat3(const QVector3D &v) {
  return {v.x(), v.y(), v.z()};
}

void centerMeshPositions(Mesh &mesh) {
  mesh.updateBounds();
  const QVector3D minB = mesh.boundingBoxMin();
  const QVector3D maxB = mesh.boundingBoxMax();
  const QVector3D center = (minB + maxB) * 0.5f;
  auto positions = mesh.vertexAttributes().get<QVector3D>("position");
  if (!positions) {
    return;
  }
  for (auto &pos : positions->data()) {
    pos -= center;
  }
  mesh.updateBounds();
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
  RenderMode renderMode_ = RenderMode::Wireframe;
  FixedGeometry3D fixedGeometry_ = FixedGeometry3D::Auto;
  ArtifactCore::Material material_ = ArtifactCore::Material::makeDefault();
  Mesh mesh_; // The 3D mesh data
  QString sourcePath_;
  bool meshLoaded_ = false;
  bool affectedByLights_ = true;
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
  impl_->sourcePath_.clear();
  impl_->fixedGeometry_ = FixedGeometry3D::Auto;
  // Try loading via MeshImporter (ufbx for FBX, tinyobj for OBJ)
  ArtifactCore::MeshImporter importer;
  auto mesh = importer.importMeshFromFile(UniString("")); // Will be set by user

  if (mesh && mesh->vertexCount() > 0) {
    impl_->mesh_ = *mesh;
    centerMeshPositions(impl_->mesh_);
    impl_->meshLoaded_ = true;
    updateSourceSizeFromMesh();
    return;
  }

  // Fallback: create a simple cube mesh programmatically
  if (!impl_->meshLoaded_) {
    createCubeMesh();
    impl_->meshLoaded_ = true;
  }
}

void Artifact3DLayer::loadFromFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    qWarning() << "[Artifact3DLayer] Ignoring empty source path reload";
    return;
  }
  impl_->fixedGeometry_ = FixedGeometry3D::Auto;

  ArtifactCore::MeshImporter importer;
  auto mesh = importer.importMeshFromFile(UniString(filePath));

  if (mesh && mesh->vertexCount() > 0) {
    impl_->mesh_ = *mesh;
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
      const QString detectedTexture = detectSiblingBaseColorTexture(filePath);
      if (!detectedTexture.isEmpty()) {
        qDebug() << "[Artifact3DLayer] Auto-detected base color texture:" << detectedTexture;
        impl_->material_.setBaseColorTexture(ArtifactCore::UniString::fromQString(detectedTexture));
      }
    }
    impl_->renderMode_ = RenderMode::Solid;
    impl_->sourcePath_ = filePath;
    setLayerName(QFileInfo(filePath).baseName());
    return;
  }

  // Fallback to cube on failure
  qWarning() << "Failed to load mesh from:" << filePath
             << "- using default cube";
  if (!impl_->meshLoaded_) {
    createCubeMesh();
    impl_->meshLoaded_ = true;
    updateSourceSizeFromMesh();
  }
}

void Artifact3DLayer::setFixedGeometry(FixedGeometry3D geometry)
{
  impl_->fixedGeometry_ = geometry;
  impl_->sourcePath_.clear();
  createFixedGeometryMesh(geometry);
  impl_->meshLoaded_ = true;
  updateSourceSizeFromMesh();
  impl_->renderMode_ = RenderMode::Wireframe;
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
  obj["renderMode"] = static_cast<int>(impl_->renderMode_);
  obj["fixedGeometry"] = static_cast<int>(impl_->fixedGeometry_);
  obj["material.baseColorTexture"] = impl_->material_.baseColorTexture().toQString();
  obj["material.metallicRoughnessTexture"] =
      impl_->material_.metallicRoughnessTexture().toQString();
  obj["material.normalTexture"] = impl_->material_.normalTexture().toQString();
  obj["material.emissionTexture"] = impl_->material_.emissionTexture().toQString();
  obj["material.occlusionTexture"] = impl_->material_.occlusionTexture().toQString();
  obj["material.opacityTexture"] = impl_->material_.opacityTexture().toQString();
  return obj;
}

void Artifact3DLayer::fromJsonProperties(const QJsonObject& obj)
{
  ArtifactAbstractLayer::fromJsonProperties(obj);

  const QString sourcePath = obj.contains("model.sourcePath")
                                 ? obj.value("model.sourcePath").toString()
                                 : obj.value("sourcePath").toString();
  if (!sourcePath.isEmpty()) {
    loadFromFile(sourcePath);
  }

  if (obj.contains("fixedGeometry")) {
    setFixedGeometry(static_cast<FixedGeometry3D>(obj.value("fixedGeometry").toInt()));
  }

  if (obj.contains("renderMode")) {
    setRenderMode(static_cast<RenderMode>(obj.value("renderMode").toInt()));
  }

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
}

void Artifact3DLayer::createCubeMesh() {
  // Create a simple cube mesh
  const float halfSize = 0.5f;
  QVector<QVector3D> positions = {
      QVector3D(-halfSize, -halfSize, -halfSize), // 0
      QVector3D(halfSize, -halfSize, -halfSize),  // 1
      QVector3D(halfSize, halfSize, -halfSize),   // 2
      QVector3D(-halfSize, halfSize, -halfSize),  // 3
      QVector3D(-halfSize, -halfSize, halfSize),  // 4
      QVector3D(halfSize, -halfSize, halfSize),   // 5
      QVector3D(halfSize, halfSize, halfSize),    // 6
      QVector3D(-halfSize, halfSize, halfSize)    // 7
  };

  impl_->mesh_.setVertexCount(8);
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  positionAttr->data() = positions;
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  for (int i = 0; i < positions.size(); ++i) {
    const QVector3D normal = positions[i].normalized();
    (*normalAttr)[i] = normal;
    (*uvAttr)[i] = QVector2D((positions[i].x() + halfSize) / (2.0f * halfSize),
                             (positions[i].y() + halfSize) / (2.0f * halfSize));
  }

  // Add polygons (triangulated for simplicity)
  // Bottom face
  impl_->mesh_.addPolygon({0, 1, 2});
  impl_->mesh_.addPolygon({0, 2, 3});
  // Top face
  impl_->mesh_.addPolygon({4, 5, 6});
  impl_->mesh_.addPolygon({4, 6, 7});
  // Front face
  impl_->mesh_.addPolygon({0, 1, 5});
  impl_->mesh_.addPolygon({0, 5, 4});
  // Back face
  impl_->mesh_.addPolygon({3, 2, 6});
  impl_->mesh_.addPolygon({3, 6, 7});
  // Left face
  impl_->mesh_.addPolygon({0, 3, 7});
  impl_->mesh_.addPolygon({0, 7, 4});
  // Right face
  impl_->mesh_.addPolygon({1, 2, 6});
  impl_->mesh_.addPolygon({1, 6, 5});
}

void Artifact3DLayer::createPlaneMesh()
{
  const float halfSize = 0.5f;
  QVector<QVector3D> positions = {
      QVector3D(-halfSize, -halfSize, 0.0f),
      QVector3D(halfSize, -halfSize, 0.0f),
      QVector3D(halfSize, halfSize, 0.0f),
      QVector3D(-halfSize, halfSize, 0.0f)
  };

  impl_->mesh_.setVertexCount(4);
  auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  auto positionAttr = vertexAttrs.add<QVector3D>("position");
  positionAttr->data() = positions;
  auto normalAttr = vertexAttrs.add<QVector3D>("normal");
  auto uvAttr = vertexAttrs.add<QVector2D>("uv");
  for (int i = 0; i < positions.size(); ++i) {
    (*normalAttr)[i] = QVector3D(0.0f, 0.0f, 1.0f);
    (*uvAttr)[i] = QVector2D((positions[i].x() + halfSize) / (2.0f * halfSize),
                             (positions[i].y() + halfSize) / (2.0f * halfSize));
  }
  impl_->mesh_.addPolygon({0, 1, 2});
  impl_->mesh_.addPolygon({0, 2, 3});
}

void Artifact3DLayer::createFixedGeometryMesh(FixedGeometry3D geometry)
{
  impl_->mesh_ = Mesh();
  switch (geometry) {
  case FixedGeometry3D::Plane:
    createPlaneMesh();
    break;
  case FixedGeometry3D::Cube:
  case FixedGeometry3D::Auto:
  default:
    createCubeMesh();
    break;
  }
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

RenderMode Artifact3DLayer::renderMode() const { return impl_->renderMode_; }

void Artifact3DLayer::setRenderMode(RenderMode mode) {
  impl_->renderMode_ = mode;
}

void Artifact3DLayer::draw(ArtifactIRenderer *renderer) {
  if (!renderer || !isVisible() || !impl_->meshLoaded_) {
    return;
  }

  const auto &t3 = transform3D();
  const auto size = sourceSize();
  const RationalTime frameTime(currentFrame(), 30); // Assume 30fps for now
  const auto snapshot = t3.snapshotAt(frameTime);
  QMatrix4x4 modelMatrix;
  modelMatrix.setToIdentity();
  modelMatrix.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
  modelMatrix.rotate(snapshot.rotation, 0.0f, 0.0f, 1.0f);
  modelMatrix.scale(snapshot.scaleX, snapshot.scaleY, 1.0f);
  modelMatrix.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);

  // Get mesh data
  const auto &vertexAttrs = impl_->mesh_.vertexAttributes();
  const auto positions = vertexAttrs.get<QVector3D>("position");
  if (!positions || positions->data().isEmpty()) {
    return;
  }

  // Transform vertices
  QVector<QVector3D> transformedVertices;
  transformedVertices.reserve(positions->data().size());
  for (const auto &pos : positions->data()) {
    transformedVertices.append(modelMatrix.map(pos));
  }

  const FloatColor wireframeColor{1.0f, 1.0f, 1.0f, opacity()};
  const float thickness = 2.0f;

  if (impl_->renderMode_ == RenderMode::Solid) {
    const QString cacheKey = sourcePath().isEmpty() ? id().toString() : sourcePath();
    renderer->drawMesh(cacheKey, impl_->mesh_, impl_->material_, modelMatrix,
                       opacity());
  } else {
    // Wireframe mode: draw edges
    const FloatColor color = wireframeColor;
    for (int i = 0; i < impl_->mesh_.polygonCount(); ++i) {
      const auto vertexIndices = impl_->mesh_.getPolygonVertices(i);
      for (size_t j = 0; j < vertexIndices.size(); ++j) {
        const QVector3D &v0 = transformedVertices[vertexIndices[j]];
        const QVector3D &v1 =
            transformedVertices[vertexIndices[(j + 1) % vertexIndices.size()]];
        renderer->draw3DLine(toFloat3(v0), toFloat3(v1), color, thickness);
      }
    }
  }

  drawFractureOverlay(renderer, modelMatrix,
                      QSizeF(size.width, size.height), opacity());
}

void Artifact3DLayer::drawLOD(ArtifactIRenderer *renderer, DetailLevel lod) {
  // For now, same as regular draw
  draw(renderer);
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
  PropertyGroup renderGroup(QStringLiteral("3D Render"));

  auto renderModeProp = persistentLayerProperty(
      QStringLiteral("render.mode"), PropertyType::Integer,
      static_cast<int>(renderMode()), -50);
  renderModeProp->setDisplayLabel(QStringLiteral("Render Mode"));
  renderModeProp->setTooltip(QStringLiteral("0=Wireframe, 1=Solid"));
  renderGroup.addProperty(renderModeProp);

  auto sourcePathProp = persistentLayerProperty(
      QStringLiteral("model.sourcePath"), PropertyType::String,
      sourcePath(), -55);
  sourcePathProp->setDisplayLabel(QStringLiteral("Source Path"));
  sourcePathProp->setTooltip(QStringLiteral("3D model source file path"));
  renderGroup.addProperty(sourcePathProp);

  auto affectedByLightsProp = persistentLayerProperty(
      QStringLiteral("render.affectedByLights"), PropertyType::Boolean,
      affectedByLights(), -54);
  affectedByLightsProp->setDisplayLabel(QStringLiteral("Affected By Lights"));
  affectedByLightsProp->setTooltip(QStringLiteral("Disable to ignore all scene lights for this 3D layer"));
  renderGroup.addProperty(affectedByLightsProp);

  PropertyGroup materialGroup(QStringLiteral("Material"));

  auto baseColorProp = persistentLayerProperty(
      QStringLiteral("material.base.color"), PropertyType::Color,
      impl_->material_.baseColor(), -40);
  baseColorProp->setDisplayLabel(QStringLiteral("Base Color"));
  materialGroup.addProperty(baseColorProp);

  auto baseColorTextureProp = persistentLayerProperty(
      QStringLiteral("material.baseColorTexture"), PropertyType::String,
      impl_->material_.baseColorTexture().toQString(), -41);
  baseColorTextureProp->setDisplayLabel(QStringLiteral("Base Color Texture"));
  baseColorTextureProp->setTooltip(QStringLiteral("Texture path for the base color"));
  materialGroup.addProperty(baseColorTextureProp);

  auto metallicRoughnessTextureProp = persistentLayerProperty(
      QStringLiteral("material.metallicRoughnessTexture"), PropertyType::String,
      impl_->material_.metallicRoughnessTexture().toQString(), -40);
  metallicRoughnessTextureProp->setDisplayLabel(QStringLiteral("Metallic Roughness Texture"));
  materialGroup.addProperty(metallicRoughnessTextureProp);

  auto normalTextureProp = persistentLayerProperty(
      QStringLiteral("material.normalTexture"), PropertyType::String,
      impl_->material_.normalTexture().toQString(), -39);
  normalTextureProp->setDisplayLabel(QStringLiteral("Normal Texture"));
  materialGroup.addProperty(normalTextureProp);

  auto emissionTextureProp = persistentLayerProperty(
      QStringLiteral("material.emissionTexture"), PropertyType::String,
      impl_->material_.emissionTexture().toQString(), -38);
  emissionTextureProp->setDisplayLabel(QStringLiteral("Emission Texture"));
  materialGroup.addProperty(emissionTextureProp);

  auto occlusionTextureProp = persistentLayerProperty(
      QStringLiteral("material.occlusionTexture"), PropertyType::String,
      impl_->material_.occlusionTexture().toQString(), -37);
  occlusionTextureProp->setDisplayLabel(QStringLiteral("Occlusion Texture"));
  materialGroup.addProperty(occlusionTextureProp);

  auto opacityTextureProp = persistentLayerProperty(
      QStringLiteral("material.opacityTexture"), PropertyType::String,
      impl_->material_.opacityTexture().toQString(), -36);
  opacityTextureProp->setDisplayLabel(QStringLiteral("Opacity Texture"));
  materialGroup.addProperty(opacityTextureProp);

  auto emissionColorProp = persistentLayerProperty(
      QStringLiteral("material.emission.color"), PropertyType::Color,
      impl_->material_.emissionColor(), -35);
  emissionColorProp->setDisplayLabel(QStringLiteral("Emission Color"));
  materialGroup.addProperty(emissionColorProp);

  auto metallicProp = persistentLayerProperty(
      QStringLiteral("material.metallic"), PropertyType::Float,
      impl_->material_.metallic(), -34);
  metallicProp->setDisplayLabel(QStringLiteral("Metallic"));
  materialGroup.addProperty(metallicProp);

  auto roughnessProp = persistentLayerProperty(
      QStringLiteral("material.roughness"), PropertyType::Float,
      impl_->material_.roughness(), -33);
  roughnessProp->setDisplayLabel(QStringLiteral("Roughness"));
  materialGroup.addProperty(roughnessProp);

  auto emissionStrengthProp = persistentLayerProperty(
      QStringLiteral("material.emissionStrength"), PropertyType::Float,
      impl_->material_.emissionStrength(), -32);
  emissionStrengthProp->setDisplayLabel(QStringLiteral("Emission Strength"));
  emissionStrengthProp->setTooltip(QStringLiteral("Emission intensity multiplier"));
  materialGroup.addProperty(emissionStrengthProp);

  auto opacityProp = persistentLayerProperty(
      QStringLiteral("material.opacity"), PropertyType::Float,
      impl_->material_.opacity(), -31);
  opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
  opacityProp->setTooltip(QStringLiteral("Material opacity (0=transparent, 1=opaque)"));
  materialGroup.addProperty(opacityProp);

  auto normalStrengthProp = persistentLayerProperty(
      QStringLiteral("material.normalStrength"), PropertyType::Float,
      impl_->material_.normalStrength(), -30);
  normalStrengthProp->setDisplayLabel(QStringLiteral("Normal Strength"));
  normalStrengthProp->setTooltip(QStringLiteral("Normal map intensity"));
  materialGroup.addProperty(normalStrengthProp);

  auto occlusionStrengthProp = persistentLayerProperty(
      QStringLiteral("material.occlusionStrength"), PropertyType::Float,
      impl_->material_.occlusionStrength(), -29);
  occlusionStrengthProp->setDisplayLabel(QStringLiteral("Occlusion Strength"));
  occlusionStrengthProp->setTooltip(QStringLiteral("Ambient occlusion intensity"));
  materialGroup.addProperty(occlusionStrengthProp);

  // MaterialX summary
  if (impl_->material_.materialXDocument().length() > 0) {
    auto materialXProp = persistentLayerProperty(
        QStringLiteral("material.materialx.summary"), PropertyType::String,
        QStringLiteral("MaterialX document present"), -37);
    materialXProp->setDisplayLabel(QStringLiteral("MaterialX"));
    materialGroup.addProperty(materialXProp);
  }

  groups.push_back(renderGroup);
  groups.push_back(materialGroup);
  return groups;
}

bool Artifact3DLayer::setLayerPropertyValue(const QString &propertyPath,
                                            const QVariant &value) {
  if (propertyPath == QStringLiteral("render.mode")) {
    int modeInt = value.toInt();
    if (modeInt >= static_cast<int>(RenderMode::Wireframe) &&
        modeInt <= static_cast<int>(RenderMode::Solid)) {
      setRenderMode(static_cast<RenderMode>(modeInt));
      Q_EMIT changed();
      return true;
    }
  } else if (propertyPath == QStringLiteral("model.sourcePath") ||
             propertyPath == QStringLiteral("sourcePath")) {
    loadFromFile(value.toString());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("render.affectedByLights")) {
    setAffectedByLights(value.toBool());
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
    impl_->material_.setMetallic(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.roughness")) {
    impl_->material_.setRoughness(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.emissionStrength")) {
    impl_->material_.setEmissionStrength(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.opacity")) {
    impl_->material_.setOpacity(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.normalStrength")) {
    impl_->material_.setNormalStrength(value.toFloat());
    Q_EMIT changed();
    return true;
  } else if (propertyPath == QStringLiteral("material.occlusionStrength")) {
    impl_->material_.setOcclusionStrength(value.toFloat());
    Q_EMIT changed();
    return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

bool Artifact3DLayer::affectedByLights() const { return impl_->affectedByLights_; }

void Artifact3DLayer::setAffectedByLights(bool enabled)
{
  impl_->affectedByLights_ = enabled;
  changed();
}

} // namespace Artifact
