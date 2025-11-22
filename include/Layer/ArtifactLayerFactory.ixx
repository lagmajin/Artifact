module;

export module Artifact.Layers.Factory;

import std;
//import Artifact.Layers;

import Artifact.Layers.Abstract;


export namespace Artifact {
 
 
 class LayerFactory {
 private:
  class Impl;
  Impl* impl_;
 public:
  LayerFactory();
  ~LayerFactory();
  ArtifactAbstractLayer createNewLayer(LayerType type) noexcept;
 };



};