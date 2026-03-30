module ;

export module Artifact.Layers.Model3D;

import Artifact.Layer.Abstract;

export namespace Artifact {

 class Artifact3DLayer : public ArtifactAbstractLayer
 {
 private:
   class Impl;
   Impl* impl_;
 public:
   Artifact3DLayer();
   ~Artifact3DLayer();
   void loadFromFile() override;
 
   // ArtifactIRenderer interface
   void draw(ArtifactIRenderer* renderer) override;
   void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) override;
 };
 
 
 
 
 
 };





};