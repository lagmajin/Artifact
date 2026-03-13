module;
#include <QJsonObject>
#include <QList>
#include <QObject>

module Artifact.Layer.Factory;
import std;

import Utils.String.UniString;
import Artifact.Layer.Result;
import Artifact.Layer.Null;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Layers.SolidImage;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layer.Video;
import Artifact.Layer.Camera;
import Artifact.Layer.Text;
//import Artifact.Layer.Video;

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
   ptr = std::make_shared<ArtifactSolidImageLayer>();
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
  case LayerType::Audio: {
   auto videoLayer = std::make_shared<ArtifactVideoLayer>();
   videoLayer->setHasVideo(false);
   ptr = videoLayer;
   break;
  }
  case LayerType::Video:
   ptr = std::make_shared<ArtifactVideoLayer>();
   break;
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
  if (ptr) {
   ptr->setLayerName(params.name().toQString());
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

  ArtifactAbstractLayerPtr ArtifactLayerFactory::createFromJson(const QJsonObject& json) noexcept
  {
      if (!json.contains("type")) return nullptr;
      LayerType type = static_cast<LayerType>(json["type"].toInt());
      QString name = json.value("name").toString("Layer");
      
      ArtifactLayerInitParams params(name, type);
      
      ArtifactLayerFactory factory;
      auto result = factory.createLayer(params);
      if (result.success && result.layer) {
          result.layer->fromJsonProperties(json);
          return result.layer;
      }
      return nullptr;
  }
}
