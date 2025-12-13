module;

export module Artifact.Layer.Factory;

import std;

import Artifact.Layers.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layer.Result;

export namespace Artifact {
 
 
 class ArtifactLayerFactory {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerFactory();
  ~ArtifactLayerFactory();
  ArtifactAbstractLayerPtr createNewLayer(ArtifactLayerInitParams params) noexcept;
  ArtifactLayerResult createLayer(ArtifactLayerInitParams& params) noexcept;
 };



};