module;


module Artifact.Layer.Factory;

import Artifact.Layer.Result;
import Artifact.Layer.Null;
import Artifact.Layer.Audio;
import Artifact.Layer.Shape;

import Artifact.Layer.Solid2D;
import Artifact.Layer.Image;

//import Artifact.Layers.Adjustable;


namespace Artifact {

 class ArtifactLayerFactory::Impl {
 private:
 public:
  Impl();
  ~Impl();
  ArtifactAbstractLayerPtr createNewLayer(ArtifactLayerInitParams params) noexcept;
  ArtifactLayerResult createLayer(ArtifactLayerInitParams& params) noexcept;
 };

 ArtifactLayerFactory::Impl::Impl()
 {

 }

 ArtifactLayerFactory::Impl::~Impl()
 {

 }

 ArtifactAbstractLayerPtr ArtifactLayerFactory::Impl::createNewLayer(ArtifactLayerInitParams params) noexcept
 {
	 
 	

  return nullptr;
 }

 ArtifactLayerResult ArtifactLayerFactory::Impl::createLayer(ArtifactLayerInitParams& params) noexcept
 {
  ArtifactLayerResult result;
  result.success = false;
  result.layer = nullptr;
 //	result.
  ArtifactAbstractLayerPtr ptr;
 	
  switch (params.layerType()) {
  case LayerType::Unknown:
   result.success = false;
   break;
  case LayerType::None:
   break;
  case LayerType::Null:
   ptr = std::make_shared<ArtifactNullLayer>();
  	
  	
   result.success = true;
   break;
  case LayerType::Solid:
   ptr = std::make_shared<ArtifactSolid2DLayer>();
  	
   break;
  case LayerType::Image:
   ptr = std::make_shared<ArtifactImageLayer>();
   break;
  case LayerType::Adjustment:
   break;
  case LayerType::Text:
   break;
  case LayerType::Shape:
   break;
  case LayerType::Precomp:
   break;
  case LayerType::Audio:
   break;
  case LayerType::Video:
   break;
  default:
   break;
  }
 	
 	
  return result;
 }

 ArtifactLayerFactory::ArtifactLayerFactory() :impl_(new Impl())
 {

 }

 ArtifactLayerFactory::~ArtifactLayerFactory()
 {
  delete impl_;
 }

 ArtifactAbstractLayerPtr ArtifactLayerFactory::createNewLayer(ArtifactLayerInitParams params) noexcept
 {
  return impl_->createNewLayer(params);
 }

 ArtifactLayerResult ArtifactLayerFactory::createLayer(ArtifactLayerInitParams& params) noexcept
 {
  return impl_->createLayer(params);
 }




}


