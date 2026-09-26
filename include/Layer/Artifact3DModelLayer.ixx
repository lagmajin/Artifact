module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <vector>
#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QRectF>
#include <QMatrix4x4>
#include <QVector>

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;
import Material.Material;
import Mesh;
import AssetType;

export namespace Artifact {

 enum class ModelRenderMode {
   Wireframe,
   Solid
 };

  enum class FixedGeometry3D {
    Auto = 0,
    Plane,
    Cube,
    Sphere,
    Cylinder,
    Cone,
    Torus,
    Capsule,
    Pyramid
  };

 class Artifact3DLayer : public ArtifactAbstractLayer
 {
 private:
   class Impl;
   Impl* impl_;
    void createCubeMesh();
    void createPlaneMesh();
    void createSphereMesh();
    void createCylinderMesh();
    void createConeMesh();
    void createTorusMesh();
    void createCapsuleMesh();
    void createPyramidMesh();
    void createFixedGeometryMesh(FixedGeometry3D geometry);
    void updateSourceSizeFromMesh(bool updateBounds = true);

 public:
    Artifact3DLayer();
    explicit Artifact3DLayer(FixedGeometry3D geometry);
    ~Artifact3DLayer();
    void loadFromFile();
    void loadFromFile(const QString& filePath);
    void loadFromFileAtTime(const QString& filePath, double time,
                            int clipIndex = 0);
    void setAnimationTime(double time, int clipIndex = 0);
    void setSkinAnimationEnabled(bool enabled);
    bool skinAnimationEnabled() const;
    void setSkinAnimationClipIndex(int clipIndex);
    int skinAnimationClipIndex() const;
    int skinAnimationClipCount() const;
    QString skinAnimationClipName(int clipIndex) const;
    int blendShapeCount() const;
    QString blendShapeName(int shapeIndex) const;
    float blendShapeWeight(int shapeIndex) const;
    void setBlendShapeWeight(int shapeIndex, float weight);
    void clearBlendShapeWeightOverride(int shapeIndex);
    void setFixedGeometry(FixedGeometry3D geometry);
    FixedGeometry3D fixedGeometry() const;

   // Render mode
    ModelRenderMode renderMode() const;
    void setRenderMode(ModelRenderMode mode);
    const ArtifactCore::Mesh& mesh() const;
    const ArtifactCore::Material& material() const;
    // Multi-mesh accessors.  A USD stage may contribute several source meshes;
    // they are concatenated into `mesh()` while these expose the per-mesh
    // material and the hierarchy transform resolved for each of them.  Both
    // containers are empty for the ordinary single-mesh import path, and
    // mesh()/material() keep returning the first entry in that case.
    const std::vector<ArtifactCore::Material>& meshMaterials() const;
    const std::vector<QMatrix4x4>& meshLocalTransforms() const;
    // Writes the source USD composition back out as-is (identity export), so
    // hierarchy, material bindings, skeletons and blend shapes survive.  Only
    // layers whose source is a USD file can be exported; other formats report
    // the reason through the return value.  Pass an empty path to overwrite the
    // source file, or a destination ending in .usda / .usdc / .usdz to write a
    // copy in that format.
    bool exportSourceUsd(const QString& outputPath = QString()) const;
    [[nodiscard]] bool hasExportableUsdSource() const;
    void setSkinPoseMatrices(const QVector<QMatrix4x4>& boneMatrices);

    // ArtifactIRenderer interface
    void draw(ArtifactIRenderer* renderer) override;
    void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;
    // Draw the selected-model wire outline over the shaded result.
    void drawSelectionOutline(ArtifactIRenderer* renderer) const;
    QRectF localBounds() const override;
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    QString sourcePath() const;
    QUuid sourceAssetId() const;
    UniString className() const override;

    // Properties
    bool affectedByLights() const;
    void setAffectedByLights(bool enabled);
    bool hasTransparentMaterial() const;
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
    QString materialSignature() const;
    // Session material-graph JSON (ShaderNode round-trip). Stored verbatim;
    // the renderer compiles it on next draw. No undo integration yet.
    void setMaterialGraphJson(const QString& json);
    QString materialGraphJson() const;
    void clearMaterialGraph();
 };

}
