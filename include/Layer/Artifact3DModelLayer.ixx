module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QJsonObject>
#include <QRectF>
#include <QMatrix4x4>
#include <QVector>

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;
import Mesh;

export namespace Artifact {

 enum class RenderMode {
   Wireframe,
   Solid
 };

 enum class FixedGeometry3D {
   Auto = 0,
   Plane,
   Cube,
   Sphere,
   Cylinder,
   Cone
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
   void createFixedGeometryMesh(FixedGeometry3D geometry);
   void updateSourceSizeFromMesh();

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
    RenderMode renderMode() const;
    void setRenderMode(RenderMode mode);
    const ArtifactCore::Mesh& mesh() const;
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
    UniString className() const override;

    // Properties
    bool affectedByLights() const;
    void setAffectedByLights(bool enabled);
    bool hasTransparentMaterial() const;
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
    QString materialSignature() const;
 };

}
