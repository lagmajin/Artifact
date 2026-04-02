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
import Artifact.Layer.Svg;
import Artifact.Layer.Particle;
import Artifact.Layer.Shape;
import Artifact.Layers.SolidImage;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Text;
 import Artifact.Layer.Group;
 import Artifact.Layer.Clone;
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
  case LayerType::Solid: {
   auto solidLayer = std::make_shared<ArtifactSolidImageLayer>();
   if (auto* solidParams = dynamic_cast<ArtifactSolidLayerInitParams*>(&params)) {
    solidLayer->setSize(solidParams->width(), solidParams->height());
    solidLayer->setColor(solidParams->color());
   } else {
    solidLayer->setSize(1920, 1080);
   }
   ptr = solidLayer;
   break;
  }
  case LayerType::Image:
   ptr = std::make_shared<ArtifactImageLayer>();
   if (ptr) {
    // 画像パラメータからパスを取得して読み込み
    if (auto* imageParams = dynamic_cast<ArtifactImageInitParams*>(&params)) {
     const QString path = imageParams->imagePath();
     if (!path.isEmpty()) {
      auto* imageLayer = static_cast<ArtifactImageLayer*>(ptr.get());
      imageLayer->loadFromPath(path);
     }
    }
   }
   break;
  case LayerType::Adjustment:
   ptr = std::make_shared<ArtifactAdjustableLayer>();
   break;
  case LayerType::Text:
   ptr = std::make_shared<ArtifactTextLayer>();
   break;
  case LayerType::Shape: {
   if (auto* svgParams = dynamic_cast<ArtifactSvgInitParams*>(&params)) {
    auto svgLayer = std::make_shared<ArtifactSvgLayer>();
    const QString path = svgParams->svgPath();
    if (!path.isEmpty()) {
     svgLayer->loadFromPath(path);
    }
    ptr = svgLayer;
   } else {
    ptr = std::make_shared<ArtifactShapeLayer>();
   }
   break;
  }
  case LayerType::Particle:
   ptr = createParticleLayer(QStringLiteral("fire"));
   break;
  case LayerType::Audio: {
   auto audioLayer = std::make_shared<ArtifactAudioLayer>();
   if (auto* audioParams = dynamic_cast<ArtifactAudioInitParams*>(&params)) {
    const QString path = audioParams->audioPath();
    if (!path.isEmpty()) {
     audioLayer->loadFromPath(path);
    }
   }
   ptr = audioLayer;
   break;
  }
  case LayerType::Video:
   ptr = std::make_shared<ArtifactVideoLayer>();
   break;
  case LayerType::Precomp:
   //ptr = std::make_shared<ArtifactCompositionLayer>();
  case LayerType::Camera:
   ptr = std::make_shared<ArtifactCameraLayer>();
   break;
   case LayerType::Group:
    ptr = std::make_shared<ArtifactGroupLayer>();
    break;
   case LayerType::Clone:
    ptr = std::make_shared<ArtifactCloneLayer>();
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
      ArtifactLayerFactory factory;
      if (json.contains("svg.sourcePath") || json.contains("sourcePath") || json.contains("svg.fitToLayer")) {
          ArtifactSvgInitParams svgParams(name);
          if (json.contains("svg.sourcePath")) {
              svgParams.setSvgPath(json.value("svg.sourcePath").toString());
          } else if (json.contains("sourcePath")) {
              svgParams.setSvgPath(json.value("sourcePath").toString());
          }
          auto result = factory.createLayer(svgParams);
          if (result.success && result.layer) {
              result.layer->fromJsonProperties(json);
              return result.layer;
          }
          return nullptr;
      }
      ArtifactLayerInitParams paramsForFactory(name, type);
      auto result = factory.createLayer(paramsForFactory);
      if (result.success && result.layer) {
          result.layer->fromJsonProperties(json);
          return result.layer;
      }
      return nullptr;
  }
}
