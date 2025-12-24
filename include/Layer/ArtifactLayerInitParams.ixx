module;
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Layer.InitParams;

import std;

import Utils.String.Like;
import Utils.String.UniString;
import Artifact.Layer.Abstract;

export namespace Artifact {


 class ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerInitParams(const QString& name, LayerType type);
  ArtifactLayerInitParams(const UniString& name, LayerType type);
  virtual ~ArtifactLayerInitParams();
  LayerType layerType() const;
  UniString name() const;
 };



 class ArtifactSolidLayerInitParams : public ArtifactLayerInitParams
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactSolidLayerInitParams(const QString& name);
  ~ArtifactSolidLayerInitParams();

 };

 class ArtifactNullLayerInitParams : public ArtifactLayerInitParams
 {
 private:
 public:
  ArtifactNullLayerInitParams(const QString& name);
  ~ArtifactNullLayerInitParams();

 };

 class ArtifactImageInitParams :public ArtifactLayerInitParams
 {
 private:


 public:
  ArtifactImageInitParams(const QString& name);
  ~ArtifactImageInitParams();
 };


 class ArtifactCameraLayerInitParams :public ArtifactLayerInitParams
 {
 	private:
 	
 public:
  ~ArtifactCameraLayerInitParams();
 };

};

W_REGISTER_ARGTYPE(Artifact::ArtifactSolidLayerInitParams)