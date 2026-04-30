module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QJsonObject>

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;

export namespace Artifact {

 enum class RenderMode {
   Wireframe,
   Solid
 };

 class Artifact3DLayer : public ArtifactAbstractLayer
 {
 private:
   class Impl;
   Impl* impl_;
   void createCubeMesh();
   void updateSourceSizeFromMesh();

 public:
    Artifact3DLayer();
    ~Artifact3DLayer();
    void loadFromFile();
    void loadFromFile(const QString& filePath);

   // Render mode
   RenderMode renderMode() const;
   void setRenderMode(RenderMode mode);

    // ArtifactIRenderer interface
    void draw(ArtifactIRenderer* renderer) override;
    void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;
    QJsonObject toJson() const override;
    QString sourcePath() const;
    UniString className() const override;

    // Properties
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
 };

}
