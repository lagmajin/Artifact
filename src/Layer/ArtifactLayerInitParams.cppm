module;


module Artifact.Layer.InitParams;

import Artifact.Layers.Abstract;


namespace Artifact {

 class ArtifactLayerInitParams::Impl {
 private:
  LayerType	layerType_;
 public:
  Impl();
  ~Impl();
  LayerType layerType() const;
 };

 ArtifactLayerInitParams::Impl::Impl()
 {

 }

 ArtifactLayerInitParams::Impl::~Impl()
 {

 }

 Artifact::LayerType ArtifactLayerInitParams::Impl::layerType() const
 {
  return layerType_;
 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(const QString& name, LayerType type)
 {

 }

 ArtifactLayerInitParams::~ArtifactLayerInitParams()
 {
  delete impl_;
 }

 ArtifactSolidLayerInitParams::~ArtifactSolidLayerInitParams()
 {

 }

};


