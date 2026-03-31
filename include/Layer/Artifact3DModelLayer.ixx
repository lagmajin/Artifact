module ;

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;
import Mesh.Mesh;

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
 public:
   Artifact3DLayer();
   ~Artifact3DLayer();
   void loadFromFile() override;

   // Render mode
   RenderMode renderMode() const;
   void setRenderMode(RenderMode mode);

   // ArtifactIRenderer interface
   void draw(ArtifactIRenderer* renderer) override;
   void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;

   // Properties
   std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
   bool setLayerPropertyValue(const QString &propertyPath, const QVariant &value) override;
 };
 
 
 
 
 
 };





};