module ;

export module Artifact.Layers.Model3D;



export namespace Artifact {

 class Artifact3DLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Artifact3DLayer();
  ~Artifact3DLayer();
  void loadFromFile();

 };





};