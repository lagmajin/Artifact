module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QJsonObject>
#include <QRectF>

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;

export namespace Artifact {

 enum class RenderMode {
   Wireframe,
   Solid
 };

 enum class FixedGeometry3D {
   Auto = 0,
   Plane,
   Cube
 };

 class Artifact3DLayer : public ArtifactAbstractLayer
 {
 private:
   class Impl;
   Impl* impl_;
   void createCubeMesh();
   void createPlaneMesh();
   void createFixedGeometryMesh(FixedGeometry3D geometry);
   void updateSourceSizeFromMesh();

 public:
    Artifact3DLayer();
    explicit Artifact3DLayer(FixedGeometry3D geometry);
    ~Artifact3DLayer();
    void loadFromFile();
    void loadFromFile(const QString& filePath);
    void setFixedGeometry(FixedGeometry3D geometry);
    FixedGeometry3D fixedGeometry() const;

   // Render mode
   RenderMode renderMode() const;
   void setRenderMode(RenderMode mode);

    // ArtifactIRenderer interface
    void draw(ArtifactIRenderer* renderer) override;
    void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;
    QRectF localBounds() const override;
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& obj) override;
    QString sourcePath() const;
    UniString className() const override;

    // Properties
    bool affectedByLights() const;
    void setAffectedByLights(bool enabled);
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
 };

}
