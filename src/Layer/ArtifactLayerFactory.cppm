module;

module Artifact.Layer.Factory;

import std;
import Utils.String.UniString;
import Artifact.Layer.Result;
import Artifact.Layer.Null;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layer.Media;
import Artifact.Layer.Camera;
import Artifact.Layer.Text;
//import Artifact.lay

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
	auto result = createLayer(params);
	return result.layer;
 }

 ArtifactLayerResult ArtifactLayerFactory::Impl::createLayer(ArtifactLayerInitParams& params) noexcept
 {
  ArtifactLayerResult result;
  result.type = params.layerType();
  result.success = false;
  result.layer = nullptr;
  ArtifactAbstractLayerPtr ptr;

  switch (params.layerType()) {
  case LayerType::Null:
   ptr = std::make_shared<ArtifactNullLayer>();
   break;
  case LayerType::Solid:
   ptr = std::make_shared<ArtifactSolid2DLayer>();
   break;
  case LayerType::Image:
   ptr = std::make_shared<ArtifactImageLayer>();
   break;
  case LayerType::Adjustment:
   ptr = std::make_shared<ArtifactAdjustableLayer>();
   break;
  case LayerType::Text:
   ptr = std::make_shared<ArtifactTextLayer>();
   break;
  case LayerType::Media:
   ptr = std::make_shared<ArtifactMediaLayer>();
   break;
  case LayerType::Audio: {
   auto mediaLayer = std::make_shared<ArtifactMediaLayer>();
   mediaLayer->setHasVideo(false);
   ptr = mediaLayer;
   break;
  }
  case LayerType::Video: {
   auto mediaLayer = std::make_shared<ArtifactMediaLayer>();
   mediaLayer->setHasAudio(false);
   ptr = mediaLayer;
   break;
  }
  case LayerType::Precomp:
   //ptr = std::make_shared<ArtifactCompositionLayer>();
  case LayerType::Camera:
   //ptr = std::make_shared<ArtifactCameraLayer>();
   break;
   break;
  default:
   break;
  }

  result.layer = ptr;
  result.success = static_cast<bool>(ptr);
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


