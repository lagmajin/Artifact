module ;
#include <QString>
export module Artifact.Layer.InitParams;

import std;

import Artifact.Layers.Abstract;

export namespace Artifact {


 class ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerInitParams(const QString& name, LayerType type);
  ~ArtifactLayerInitParams();

 };

 class ArtifactSolidLayerInitParams : public ArtifactLayerInitParams
 {
 private:

 public:
  ArtifactSolidLayerInitParams();
  ~ArtifactSolidLayerInitParams();

 };

}