module;
#include <utility>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

module Artifact.Layer.Factory;
import std;

import Utils.String.UniString;
import Artifact.Layer.Abstract;
import Artifact.Layer.Result;
import Artifact.Layer.Null;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Particle;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Procedural3D;
import Artifact.Layer.Shape;
import Artifact.Layers.SolidImage;
import Artifact.Layer.AdjustableLayer;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Layer.Text;
import Artifact.Layer.Group;
import Artifact.Layer.Clone;
import Artifact.Layer.SDF;
import Artifact.Layer.Construction;
import Artifact.Layer.CompositionBackground;
import Artifact.Layers.Model3D;
import Artifact.Layer.Composition;
import Artifact.Layer.MaterialContainer;
import Artifact.Layer.SandSim2D;
//import Artifact.Layer.Video;

namespace Artifact {

std::shared_ptr<ArtifactAbstractLayer> createArtifactLayerFromJson(const QJsonObject& json);

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
  ArtifactAbstractLayerPtr layer = result.layer;
  return layer;
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
   ptr = ArtifactAbstractLayerPtr(new ArtifactNullLayer());
   break;
  case LayerType::Solid: {
   auto* solidLayer = new ArtifactSolidImageLayer();
   if (auto* solidParams = dynamic_cast<ArtifactSolidLayerInitParams*>(&params)) {
    solidLayer->setSize(solidParams->width(), solidParams->height());
    solidLayer->setColor(solidParams->color());
    solidLayer->setFillType(solidParams->fillType());
    solidLayer->setGradientStartColor(solidParams->gradientStartColor());
    solidLayer->setGradientEndColor(solidParams->gradientEndColor());
    solidLayer->setGradientAngleDegrees(solidParams->gradientAngleDegrees());
   } else {
    solidLayer->setSize(1920, 1080);
   }
   ptr = ArtifactAbstractLayerPtr(solidLayer);
   break;
  }
  case LayerType::Image:
   ptr = ArtifactAbstractLayerPtr(new ArtifactImageLayer());
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
   ptr = ArtifactAbstractLayerPtr(new ArtifactAdjustableLayer());
   break;
  case LayerType::Text:
   ptr = ArtifactAbstractLayerPtr(new ArtifactTextLayer());
   break;
  case LayerType::Shape: {
   if (auto* svgParams = dynamic_cast<ArtifactSvgInitParams*>(&params)) {
    auto* svgLayer = new ArtifactSvgLayer();
    const QString path = svgParams->svgPath();
    if (path.isEmpty() || !svgLayer->loadFromPath(path)) {
     qWarning() << "[ArtifactLayerFactory] Failed to create SVG layer from path:" << path;
     break;
    }
    ptr = ArtifactAbstractLayerPtr(svgLayer);
   } else {
    ptr = ArtifactAbstractLayerPtr(new ArtifactShapeLayer());
   }
   break;
  }
  case LayerType::Particle:
   ptr = createParticleLayer(QStringLiteral("fire"));
   break;
  case LayerType::FormParticle:
   ptr = createFormParticleLayer(QStringLiteral("dotGrid"));
   break;
  case LayerType::Procedural3D:
   ptr = createTerrainLayer();
   break;
  case LayerType::Audio: {
   auto* audioLayer = new ArtifactAudioLayer();
   if (auto* audioParams = dynamic_cast<ArtifactAudioInitParams*>(&params)) {
    const QString path = audioParams->audioPath();
    if (!path.isEmpty()) {
     audioLayer->loadFromPath(path);
    }
   }
   ptr = ArtifactAbstractLayerPtr(audioLayer);
   break;
  }
  case LayerType::Video:
   ptr = ArtifactAbstractLayerPtr(new ArtifactVideoLayer());
   if (ptr) {
    if (auto* videoParams = dynamic_cast<ArtifactVideoInitParams*>(&params)) {
     const QString path = videoParams->videoPath();
     if (!path.isEmpty()) {
      auto* videoLayer = static_cast<ArtifactVideoLayer*>(ptr.get());
      videoLayer->setSourceFile(path);
     }
    }
  }
   break;
  case LayerType::Precomp:
   ptr = std::make_shared<ArtifactCompositionLayer>();
   break;
  case LayerType::Camera:
   ptr = ArtifactAbstractLayerPtr(new ArtifactCameraLayer());
   break;
  case LayerType::Light:
   ptr = ArtifactAbstractLayerPtr(new ArtifactLightLayer());
   break;
  case LayerType::Group:
    ptr = ArtifactAbstractLayerPtr(new ArtifactGroupLayer());
    break;
  case LayerType::MaterialContainer:
    ptr = ArtifactAbstractLayerPtr(new ArtifactMaterialContainerLayer());
    break;
   case LayerType::Clone:
    ptr = ArtifactAbstractLayerPtr(new ArtifactCloneLayer());
    break;
  case LayerType::SDF:
   ptr = ArtifactAbstractLayerPtr(new ArtifactSDFLayer());
   break;
  case LayerType::Construction:
   ptr = ArtifactAbstractLayerPtr(new ArtifactConstructionLayer());
   break;
  case LayerType::CompositionBackground:
   ptr = ArtifactAbstractLayerPtr(new ArtifactCompositionBackgroundLayer());
   break;
 case LayerType::Model3D: {
   auto* modelLayer = new Artifact3DLayer();
   if (auto* fixedParams =
           dynamic_cast<ArtifactFixedGeometry3DLayerInitParams*>(&params)) {
    modelLayer->setFixedGeometry(fixedParams->geometry());
   } else if (auto* modelParams =
           dynamic_cast<ArtifactModel3DLayerInitParams*>(&params)) {
    const QString path = modelParams->modelPath();
    if (!path.isEmpty()) {
     modelLayer->loadFromFile(path);
    }
   }
   ptr = ArtifactAbstractLayerPtr(modelLayer);
   break;
  }
  case LayerType::SandSim2D:
    ptr = ArtifactAbstractLayerPtr(new ArtifactSandSim2DLayer());
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
      return createArtifactLayerFromJson(json);
  }

 std::shared_ptr<ArtifactAbstractLayer> createArtifactLayerFromJson(const QJsonObject& json) {
      if (!json.contains("type") &&
          !json.value("isConstruction").toBool(false) &&
          !json.value("isCompositionBackground").toBool(false)) return nullptr;
      LayerType type = json.contains("type")
          ? static_cast<LayerType>(json["type"].toInt())
          : LayerType::Construction;
      if (json.value("isConstruction").toBool(false)) {
          type = LayerType::Construction;
      } else if (json.value("isCompositionBackground").toBool(false)) {
          type = LayerType::CompositionBackground;
      } else if (json.value("type").toString() == QStringLiteral("MaterialContainer") ||
                 json.value("layerType").toString() == QStringLiteral("MaterialContainer")) {
          type = LayerType::MaterialContainer;
      } else if (json.value("type").toString() == QStringLiteral("FormParticleLayer") ||
                 json.value("layerType").toString() == QStringLiteral("FormParticleLayer") ||
                 json.value("layerType").toString() == QStringLiteral("FormParticle") ||
                 json.contains("formParticle")) {
          type = LayerType::FormParticle;
      } else if (json.value("type").toString() == QStringLiteral("Procedural3DLayer") ||
                 json.value("layerType").toString() == QStringLiteral("Procedural3DLayer") ||
                 json.value("isProcedural3DLayer").toBool(false) ||
                 json.contains("procedural3D")) {
          type = LayerType::Procedural3D;
      }
      QString name = json.value("name").toString("Layer");
  ArtifactLayerFactory factory;
      if (type == LayerType::Model3D &&
          (json.contains("sourcePath") || json.contains("modelPath") || json.contains("fixedGeometry"))) {
          if (json.contains("fixedGeometry")) {
              ArtifactFixedGeometry3DLayerInitParams fixedParams(
                  name, static_cast<FixedGeometry3D>(json.value("fixedGeometry").toInt()));
              auto result = factory.createLayer(fixedParams);
              if (result.success && result.layer) {
                  if (auto modelLayer = dynamic_cast<Artifact3DLayer*>(result.layer.get())) {
                      if (json.contains("renderMode")) {
                          modelLayer->setRenderMode(static_cast<RenderMode>(json.value("renderMode").toInt()));
                      }
                  }
                  result.layer->fromJsonProperties(json);
                  ArtifactAbstractLayerPtr layer = result.layer;
                  return layer;
              }
              return nullptr;
          }
          ArtifactModel3DLayerInitParams modelParams(name);
          if (json.contains("sourcePath")) {
              modelParams.setModelPath(json.value("sourcePath").toString());
          } else if (json.contains("modelPath")) {
              modelParams.setModelPath(json.value("modelPath").toString());
          }
          auto result = factory.createLayer(modelParams);
          if (result.success && result.layer) {
              if (auto modelLayer = dynamic_cast<Artifact3DLayer*>(result.layer.get())) {
                  if (json.contains("renderMode")) {
                      modelLayer->setRenderMode(static_cast<RenderMode>(json.value("renderMode").toInt()));
                  }
              }
              result.layer->fromJsonProperties(json);
              ArtifactAbstractLayerPtr layer = result.layer;
              return layer;
          }
          return nullptr;
      }
      if ((type == LayerType::Video || json.value("type").toString() == QStringLiteral("VideoLayer")) &&
          (json.contains("video.sourcePath") || json.contains("sourcePath"))) {
          ArtifactVideoInitParams videoParams(name);
          if (json.contains("video.sourcePath")) {
              videoParams.setVideoPath(json.value("video.sourcePath").toString());
          } else {
              videoParams.setVideoPath(json.value("sourcePath").toString());
          }
          auto result = factory.createLayer(videoParams);
          if (result.success && result.layer) {
              result.layer->fromJsonProperties(json);
              ArtifactAbstractLayerPtr layer = result.layer;
              return layer;
          }
          return nullptr;
      }
      if (json.contains("svg.sourcePath") || json.contains("sourcePath")) {
          ArtifactSvgInitParams svgParams(name);
          if (json.contains("svg.sourcePath")) {
              svgParams.setSvgPath(json.value("svg.sourcePath").toString());
          } else if (json.contains("sourcePath")) {
              svgParams.setSvgPath(json.value("sourcePath").toString());
          }
          auto result = factory.createLayer(svgParams);
          if (result.success && result.layer) {
              result.layer->fromJsonProperties(json);
              ArtifactAbstractLayerPtr layer = result.layer;
              return layer;
          }
          return nullptr;
      }
      ArtifactLayerInitParams paramsForFactory(name, type);
      auto result = factory.createLayer(paramsForFactory);
      if (result.success && result.layer) {
          result.layer->fromJsonProperties(json);
          ArtifactAbstractLayerPtr layer = result.layer;
          return layer;
      }
      return nullptr;
  }

}
