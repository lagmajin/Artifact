module;

export module Artifact.Layers.Factory;

import std;

import Artifact.Layers.Abstract;


export namespace Artifact {
 
 
 class LayerFactory {
 private:
  class Impl;
  Impl* impl_;
 public:
  LayerFactory();
  ~LayerFactory();
  ArtifactAbstractLayerPtr createNewLayer(LayerType type) noexcept;
 };



};