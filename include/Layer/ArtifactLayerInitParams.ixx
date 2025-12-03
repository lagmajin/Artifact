module ;
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Layer.InitParams;

import std;

import Utils.String.Like;
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
  ArtifactSolidLayerInitParams(const QString& name);
  ~ArtifactSolidLayerInitParams();

 };

};

W_REGISTER_ARGTYPE(Artifact::ArtifactSolidLayerInitParams)