module;
#include <utility>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

module Artifact.Layer.Factory;
import std;
import Memory.SharedPtr;

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
import Artifact.Layer.Paint;
import Artifact.Layers.Model3D;
import Artifact.Layer.Composition;
import Artifact.Layer.MaterialContainer;
import Artifact.Layer.Switch;
import Artifact.Layer.SandSim2D;
import Artifact.Layers.Noise;
import Artifact.Layer.ParametricComposition;
import Artifact.Layer.EnvironmentMap;
import Artifact.Layer.EnvironmentMapInitParams;
import Animation.Transform3D;
import Time.Rational;
//import Artifact.Layer.Video;

namespace Artifact {

ArtifactAbstractLayerPtr createArtifactLayerFromJson(const QJsonObject& json);

 class ArtifactLayerFactory::Impl {
 private:
 public:
  Impl();
  ~Impl();
 ArtifactAbstractLayerPtr createNewLayer(const ArtifactLayerInitParams& params) noexcept;
 ArtifactLayerResult createLayer(const ArtifactLayerInitParams& params) noexcept;
 };

 ArtifactLayerFactory::Impl::Impl()
 {

 }

 ArtifactLayerFactory::Impl::~Impl()
 {

 }

ArtifactAbstractLayerPtr ArtifactLayerFactory::Impl::createNewLayer(const ArtifactLayerInitParams& params) noexcept
{
  auto result = createLayer(params);
  ArtifactAbstractLayerPtr layer = result.layer;
  return layer;
}

 ArtifactLayerResult ArtifactLayerFactory::Impl::createLayer(const ArtifactLayerInitParams& params) noexcept
 {
  ArtifactLayerResult result;
  result.type = params.layerType();
  result.success = false;
  result.layer = nullptr;
  ArtifactAbstractLayerPtr ptr;

  switch (params.layerType()) {
  case LayerType::Null:
   ptr = ArtifactCore::makeShared<ArtifactNullLayer>();
   break;
  case LayerType::Solid: {
    auto solidLayer = ArtifactCore::makeShared<ArtifactSolidImageLayer>();
   if (auto* solidParams = dynamic_cast<const ArtifactSolidLayerInitParams*>(&params)) {
    solidLayer->setSize(solidParams->width(), solidParams->height());
    solidLayer->setColor(solidParams->color());
    solidLayer->setFillType(solidParams->fillType());
    solidLayer->setGradientStartColor(solidParams->gradientStartColor());
    solidLayer->setGradientEndColor(solidParams->gradientEndColor());
    solidLayer->setGradientAngleDegrees(solidParams->gradientAngleDegrees());
    solidLayer->setGradientReverse(solidParams->gradientReverse());
    solidLayer->setGradientCenterX(solidParams->gradientCenterX());
    solidLayer->setGradientCenterY(solidParams->gradientCenterY());
    solidLayer->setGradientScale(solidParams->gradientScale());
    solidLayer->setGradientOffset(solidParams->gradientOffset());

    // Match an AE solid's initial transform: its Anchor Point is centred,
    // while Position is offset by the same amount so the new layer keeps its
    // existing top-left placement in the composition.
    const float anchorX = static_cast<float>(solidParams->width()) * 0.5f;
    const float anchorY = static_cast<float>(solidParams->height()) * 0.5f;
    auto &transform = solidLayer->transform3D();
    transform.setInitialPosition(ArtifactCore::RationalTime(0, 1), anchorX,
                                 anchorY);
    // Keep random-access transform evaluation (used by motion paths) in sync
    // with the initial transform state.
    transform.setCurrentPosition(anchorX, anchorY);
    transform.setAnchor(ArtifactCore::RationalTime(0, 1), anchorX, anchorY);
    transform.setCurrentAnchor(anchorX, anchorY);
   } else {
    solidLayer->setSize(1920, 1080);
   }
   ptr = solidLayer;
   break;
  }
  case LayerType::Image:
   ptr = ArtifactCore::makeShared<ArtifactImageLayer>();
   if (ptr) {
    // 画像パラメータからパスを取得して読み込み
    if (auto* imageParams = dynamic_cast<const ArtifactImageInitParams*>(&params)) {
     auto* imageLayer = static_cast<ArtifactImageLayer*>(ptr.get());
     imageLayer->setPsdSubimageIndex(imageParams->psdSubimageIndex());
     imageLayer->setInputInterpretation(imageParams->inputColorSpace(),
                                        imageParams->inputTransferFunction());
     const QStringList sequencePaths = imageParams->sequencePaths();
     if (sequencePaths.size() > 1) {
      // 連番シーケンスは代表フレーム読み込み＋シーケンス関係の保持
      imageLayer->setImageSequence(sequencePaths, imageParams->sequenceFrameRate());
     } else {
      const QString path = imageParams->imagePath().isEmpty() &&
              sequencePaths.size() == 1
          ? sequencePaths.front()
          : imageParams->imagePath();
      if (!path.isEmpty()) {
       imageLayer->loadFromPath(path);
      }
     }
    }
   }
   break;
  case LayerType::Adjustment:
   ptr = ArtifactCore::makeShared<ArtifactAdjustableLayer>();
   break;
  case LayerType::Text:
   ptr = ArtifactCore::makeShared<ArtifactTextLayer>();
   break;
  case LayerType::Shape: {
   if (auto* svgParams = dynamic_cast<const ArtifactSvgInitParams*>(&params)) {
     auto svgLayer = ArtifactCore::makeShared<ArtifactSvgLayer>();
    const QString path = svgParams->svgPath();
    if (path.isEmpty() || !svgLayer->loadFromPath(path)) {
     qWarning() << "[ArtifactLayerFactory] Failed to create SVG layer from path:" << path;
     break;
    }
     ptr = svgLayer;
   } else {
     ptr = ArtifactCore::makeShared<ArtifactShapeLayer>();
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
    auto audioLayer = ArtifactCore::makeShared<ArtifactAudioLayer>();
   if (auto* audioParams = dynamic_cast<const ArtifactAudioInitParams*>(&params)) {
    const QString path = audioParams->audioPath();
    if (!path.isEmpty()) {
     audioLayer->loadFromPath(path);
    }
   }
    ptr = audioLayer;
   break;
  }
  case LayerType::Paint:
    ptr = ArtifactCore::makeShared<ArtifactPaintLayer>();
   break;
  case LayerType::Video:
    ptr = ArtifactCore::makeShared<ArtifactVideoLayer>();
   if (ptr) {
    if (auto* videoParams = dynamic_cast<const ArtifactVideoInitParams*>(&params)) {
     const QString path = videoParams->videoPath();
     if (!path.isEmpty()) {
      auto* videoLayer = static_cast<ArtifactVideoLayer*>(ptr.get());
      videoLayer->setSourceFile(path);
     }
    }
  }
   break;
  case LayerType::Precomp:
   ptr = ArtifactCore::makeShared<ArtifactCompositionLayer>();
   break;
  case LayerType::Camera:
   ptr = ArtifactCore::makeShared<ArtifactCameraLayer>();
   break;
  case LayerType::Light:
   ptr = ArtifactCore::makeShared<ArtifactLightLayer>();
   break;
  case LayerType::Group:
     ptr = ArtifactCore::makeShared<ArtifactGroupLayer>();
    break;
  case LayerType::MaterialContainer:
     ptr = ArtifactCore::makeShared<ArtifactMaterialContainerLayer>();
    break;
  case LayerType::Switch:
     ptr = ArtifactCore::makeShared<ArtifactSwitchLayer>();
    break;
   case LayerType::Clone:
     ptr = ArtifactCore::makeShared<ArtifactCloneLayer>();
    break;
  case LayerType::SDF:
    ptr = ArtifactCore::makeShared<ArtifactSDFLayer>();
   break;
  case LayerType::ParametricComposition:
    ptr = ArtifactCore::makeShared<ArtifactParametricCompositionLayer>();
   break;
  case LayerType::Construction:
    ptr = ArtifactCore::makeShared<ArtifactConstructionLayer>();
   break;
  case LayerType::CompositionBackground:
    ptr = ArtifactCore::makeShared<ArtifactCompositionBackgroundLayer>();
   break;
 case LayerType::Model3D: {
    auto modelLayer = ArtifactCore::makeShared<Artifact3DLayer>();
   if (auto* fixedParams =
           dynamic_cast<const ArtifactFixedGeometry3DLayerInitParams*>(&params)) {
    modelLayer->setFixedGeometry(fixedParams->geometry());
   } else if (auto* modelParams =
           dynamic_cast<const ArtifactModel3DLayerInitParams*>(&params)) {
    const QString path = modelParams->modelPath();
    if (!path.isEmpty()) {
     modelLayer->loadFromFile(path);
    }
   }
    ptr = modelLayer;
   break;
  }
  case LayerType::SandSim2D:
     ptr = ArtifactCore::makeShared<ArtifactSandSim2DLayer>();
    break;
  case LayerType::Noise:
     ptr = ArtifactCore::makeShared<ArtifactNoiseLayer>();
    break;
  case LayerType::EnvironmentMap:
     ptr = ArtifactCore::makeShared<ArtifactEnvironmentMapLayer>();
    if (auto* envParams = dynamic_cast<const ArtifactEnvironmentMapLayerInitParams*>(&params)) {
      auto* envLayer = static_cast<ArtifactEnvironmentMapLayer*>(ptr.get());
      envLayer->setHdriPath(envParams->hdriPath());
    }
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
  setArtifactLayerJsonFactory(&createArtifactLayerFromJson);
 }

 ArtifactLayerFactory::~ArtifactLayerFactory()
 {
  delete impl_;
 }
  ArtifactAbstractLayerPtr ArtifactLayerFactory::createNewLayer(const ArtifactLayerInitParams& params) noexcept
  {
   return impl_->createNewLayer(params);
  }

  ArtifactLayerResult ArtifactLayerFactory::createLayer(const ArtifactLayerInitParams& params) noexcept
  {
   return impl_->createLayer(params);
  }

  ArtifactAbstractLayerPtr ArtifactLayerFactory::createFromJson(const QJsonObject& json) noexcept
  {
      return createArtifactLayerFromJson(json);
  }

 ArtifactAbstractLayerPtr createArtifactLayerFromJson(const QJsonObject& json) {
      setArtifactLayerJsonFactory(&createArtifactLayerFromJson);
      const QString legacyLayerType = json.value("layerType").toString();
      const QString serializedType = json.value("type").toString();
      const bool hasRecognizedLegacyType =
          legacyLayerType == QStringLiteral("MaterialContainer") ||
          legacyLayerType == QStringLiteral("MaterialContainerLayer") ||
          legacyLayerType == QStringLiteral("Null") ||
          legacyLayerType == QStringLiteral("NullLayer") ||
          legacyLayerType == QStringLiteral("Solid") ||
          legacyLayerType == QStringLiteral("SolidLayer") ||
          legacyLayerType == QStringLiteral("Image") ||
          legacyLayerType == QStringLiteral("ImageLayer") ||
          legacyLayerType == QStringLiteral("Shape") ||
          legacyLayerType == QStringLiteral("ShapeLayer") ||
          legacyLayerType == QStringLiteral("Adjustment") ||
          legacyLayerType == QStringLiteral("AdjustmentLayer") ||
          legacyLayerType == QStringLiteral("Text") ||
          legacyLayerType == QStringLiteral("TextLayer") ||
          legacyLayerType == QStringLiteral("Audio") ||
          legacyLayerType == QStringLiteral("AudioLayer") ||
          legacyLayerType == QStringLiteral("Video") ||
          legacyLayerType == QStringLiteral("VideoLayer") ||
          legacyLayerType == QStringLiteral("Group") ||
          legacyLayerType == QStringLiteral("GroupLayer") ||
          legacyLayerType == QStringLiteral("Particle") ||
          legacyLayerType == QStringLiteral("ParticleLayer") ||
          legacyLayerType == QStringLiteral("Clone") ||
          legacyLayerType == QStringLiteral("CloneLayer") ||
          legacyLayerType == QStringLiteral("Precomp") ||
          legacyLayerType == QStringLiteral("PrecompLayer") ||
          legacyLayerType == QStringLiteral("CompositionLayer") ||
          legacyLayerType == QStringLiteral("Camera") ||
          legacyLayerType == QStringLiteral("CameraLayer") ||
          legacyLayerType == QStringLiteral("Light") ||
          legacyLayerType == QStringLiteral("LightLayer") ||
          legacyLayerType == QStringLiteral("SDF") ||
          legacyLayerType == QStringLiteral("SDFLayer") ||
          legacyLayerType == QStringLiteral("Model3D") ||
          legacyLayerType == QStringLiteral("Model3DLayer") ||
          legacyLayerType == QStringLiteral("Construction") ||
          legacyLayerType == QStringLiteral("ConstructionLayer") ||
          legacyLayerType == QStringLiteral("CompositionBackground") ||
          legacyLayerType == QStringLiteral("CompositionBackgroundLayer") ||
          legacyLayerType == QStringLiteral("ParametricComposition") ||
          legacyLayerType == QStringLiteral("ParametricCompositionLayer") ||
          legacyLayerType == QStringLiteral("SandSim2D") ||
          legacyLayerType == QStringLiteral("SandSim2DLayer") ||
          legacyLayerType == QStringLiteral("EnvironmentMap") ||
          legacyLayerType == QStringLiteral("EnvironmentMapLayer") ||
          legacyLayerType == QStringLiteral("Switch") ||
          legacyLayerType == QStringLiteral("SwitchLayer") ||
          legacyLayerType == QStringLiteral("Paint") ||
          legacyLayerType == QStringLiteral("PaintLayer") ||
          legacyLayerType == QStringLiteral("FormParticleLayer") ||
          legacyLayerType == QStringLiteral("FormParticle") ||
          legacyLayerType == QStringLiteral("Procedural3D") ||
          legacyLayerType == QStringLiteral("Procedural3DLayer") ||
          json.contains("formParticle") ||
          json.contains("procedural3D");
      if (!json.contains("type") &&
          !json.value("isConstruction").toBool(false) &&
          !json.value("isCompositionBackground").toBool(false) &&
          !hasRecognizedLegacyType) return nullptr;
      const bool hasNumericType = json.value("type").isDouble();
      LayerType type = json.contains("type")
          ? static_cast<LayerType>(json["type"].toInt())
          : LayerType::Construction;
      if (!hasNumericType && json.value("isConstruction").toBool(false)) {
          type = LayerType::Construction;
      } else if (!hasNumericType &&
                 json.value("isCompositionBackground").toBool(false)) {
          type = LayerType::CompositionBackground;
      } else if (hasNumericType) {
          // A numeric type is the canonical discriminator.  Do not let a
          // stale legacy layerType alias override it.
          type = static_cast<LayerType>(json["type"].toInt());
      } else if (serializedType == QStringLiteral("MaterialContainer") ||
                 serializedType == QStringLiteral("MaterialContainerLayer") ||
                 legacyLayerType == QStringLiteral("MaterialContainer") ||
                 legacyLayerType == QStringLiteral("MaterialContainerLayer")) {
          type = LayerType::MaterialContainer;
      } else if (serializedType == QStringLiteral("Null") ||
                 serializedType == QStringLiteral("NullLayer")) {
          type = LayerType::Null;
      } else if (serializedType == QStringLiteral("Solid") ||
                 serializedType == QStringLiteral("SolidLayer")) {
          type = LayerType::Solid;
      } else if (serializedType == QStringLiteral("Image") ||
                 serializedType == QStringLiteral("ImageLayer")) {
          type = LayerType::Image;
      } else if (serializedType == QStringLiteral("Shape") ||
                 serializedType == QStringLiteral("ShapeLayer")) {
          type = LayerType::Shape;
      } else if (serializedType == QStringLiteral("Text") ||
                 serializedType == QStringLiteral("TextLayer")) {
          type = LayerType::Text;
      } else if (serializedType == QStringLiteral("Audio") ||
                 serializedType == QStringLiteral("AudioLayer")) {
          type = LayerType::Audio;
      } else if (serializedType == QStringLiteral("Video") ||
                 serializedType == QStringLiteral("VideoLayer")) {
          type = LayerType::Video;
      } else if (serializedType == QStringLiteral("Group") ||
                 serializedType == QStringLiteral("GroupLayer")) {
          type = LayerType::Group;
      } else if (serializedType == QStringLiteral("Particle") ||
                 serializedType == QStringLiteral("ParticleLayer")) {
          type = LayerType::Particle;
      } else if (serializedType == QStringLiteral("Clone") ||
                 serializedType == QStringLiteral("CloneLayer")) {
          type = LayerType::Clone;
      } else if (serializedType == QStringLiteral("Precomp") ||
                 serializedType == QStringLiteral("PrecompLayer") ||
                 serializedType == QStringLiteral("CompositionLayer")) {
          type = LayerType::Precomp;
      } else if (serializedType == QStringLiteral("Camera") ||
                 serializedType == QStringLiteral("CameraLayer")) {
          type = LayerType::Camera;
      } else if (serializedType == QStringLiteral("Light") ||
                 serializedType == QStringLiteral("LightLayer")) {
          type = LayerType::Light;
      } else if (serializedType == QStringLiteral("Adjustment") ||
                 serializedType == QStringLiteral("AdjustmentLayer")) {
          type = LayerType::Adjustment;
      } else if (serializedType == QStringLiteral("SDF") ||
                 serializedType == QStringLiteral("SDFLayer")) {
          type = LayerType::SDF;
      } else if (serializedType == QStringLiteral("Model3D") ||
                 serializedType == QStringLiteral("Model3DLayer")) {
          type = LayerType::Model3D;
      } else if (serializedType == QStringLiteral("Construction") ||
                 serializedType == QStringLiteral("ConstructionLayer")) {
          type = LayerType::Construction;
      } else if (serializedType == QStringLiteral("CompositionBackground") ||
                 serializedType == QStringLiteral("CompositionBackgroundLayer")) {
          type = LayerType::CompositionBackground;
      } else if (serializedType == QStringLiteral("ParametricComposition") ||
                 serializedType == QStringLiteral("ParametricCompositionLayer")) {
          type = LayerType::ParametricComposition;
      } else if (serializedType == QStringLiteral("SandSim2D") ||
                 serializedType == QStringLiteral("SandSim2DLayer")) {
          type = LayerType::SandSim2D;
      } else if (serializedType == QStringLiteral("EnvironmentMap") ||
                 serializedType == QStringLiteral("EnvironmentMapLayer")) {
          type = LayerType::EnvironmentMap;
      } else if (serializedType == QStringLiteral("Switch") ||
                 serializedType == QStringLiteral("SwitchLayer")) {
          type = LayerType::Switch;
      } else if (serializedType == QStringLiteral("Paint") ||
                 serializedType == QStringLiteral("PaintLayer")) {
          type = LayerType::Paint;
      } else if (legacyLayerType == QStringLiteral("Null") ||
                 legacyLayerType == QStringLiteral("NullLayer")) {
          type = LayerType::Null;
      } else if (legacyLayerType == QStringLiteral("Solid") ||
                 legacyLayerType == QStringLiteral("SolidLayer")) {
          type = LayerType::Solid;
      } else if (legacyLayerType == QStringLiteral("Image") ||
                 legacyLayerType == QStringLiteral("ImageLayer")) {
          type = LayerType::Image;
      } else if (legacyLayerType == QStringLiteral("Shape") ||
                 legacyLayerType == QStringLiteral("ShapeLayer")) {
          type = LayerType::Shape;
      } else if (legacyLayerType == QStringLiteral("Adjustment") ||
                 legacyLayerType == QStringLiteral("AdjustmentLayer")) {
          type = LayerType::Adjustment;
      } else if (legacyLayerType == QStringLiteral("Text") ||
                 legacyLayerType == QStringLiteral("TextLayer")) {
          type = LayerType::Text;
      } else if (legacyLayerType == QStringLiteral("Audio") ||
                 legacyLayerType == QStringLiteral("AudioLayer")) {
          type = LayerType::Audio;
      } else if (legacyLayerType == QStringLiteral("Video") ||
                 legacyLayerType == QStringLiteral("VideoLayer")) {
          type = LayerType::Video;
      } else if (legacyLayerType == QStringLiteral("Group") ||
                 legacyLayerType == QStringLiteral("GroupLayer")) {
          type = LayerType::Group;
      } else if (legacyLayerType == QStringLiteral("Particle") ||
                 legacyLayerType == QStringLiteral("ParticleLayer")) {
          type = LayerType::Particle;
      } else if (legacyLayerType == QStringLiteral("Clone") ||
                 legacyLayerType == QStringLiteral("CloneLayer")) {
          type = LayerType::Clone;
      } else if (legacyLayerType == QStringLiteral("Precomp") ||
                 legacyLayerType == QStringLiteral("PrecompLayer") ||
                 legacyLayerType == QStringLiteral("CompositionLayer")) {
          type = LayerType::Precomp;
      } else if (legacyLayerType == QStringLiteral("Camera") ||
                 legacyLayerType == QStringLiteral("CameraLayer")) {
          type = LayerType::Camera;
      } else if (legacyLayerType == QStringLiteral("Light") ||
                 legacyLayerType == QStringLiteral("LightLayer")) {
          type = LayerType::Light;
      } else if (legacyLayerType == QStringLiteral("SDF") ||
                 legacyLayerType == QStringLiteral("SDFLayer")) {
          type = LayerType::SDF;
      } else if (legacyLayerType == QStringLiteral("Model3D") ||
                 legacyLayerType == QStringLiteral("Model3DLayer")) {
          type = LayerType::Model3D;
      } else if (legacyLayerType == QStringLiteral("Construction") ||
                 legacyLayerType == QStringLiteral("ConstructionLayer")) {
          type = LayerType::Construction;
      } else if (legacyLayerType == QStringLiteral("CompositionBackground") ||
                 legacyLayerType == QStringLiteral("CompositionBackgroundLayer")) {
          type = LayerType::CompositionBackground;
      } else if (legacyLayerType == QStringLiteral("ParametricComposition") ||
                 legacyLayerType == QStringLiteral("ParametricCompositionLayer")) {
          type = LayerType::ParametricComposition;
      } else if (legacyLayerType == QStringLiteral("SandSim2D") ||
                 legacyLayerType == QStringLiteral("SandSim2DLayer")) {
          type = LayerType::SandSim2D;
      } else if (legacyLayerType == QStringLiteral("EnvironmentMap") ||
                 legacyLayerType == QStringLiteral("EnvironmentMapLayer")) {
          type = LayerType::EnvironmentMap;
      } else if (legacyLayerType == QStringLiteral("Switch") ||
                 legacyLayerType == QStringLiteral("SwitchLayer")) {
          type = LayerType::Switch;
      } else if (legacyLayerType == QStringLiteral("Paint") ||
                 legacyLayerType == QStringLiteral("PaintLayer")) {
          type = LayerType::Paint;
      } else if (serializedType == QStringLiteral("FormParticleLayer") ||
                 serializedType == QStringLiteral("FormParticle") ||
                 json.value("layerType").toString() == QStringLiteral("FormParticleLayer") ||
                 json.value("layerType").toString() == QStringLiteral("FormParticle") ||
                 json.contains("formParticle")) {
          type = LayerType::FormParticle;
      } else if (serializedType == QStringLiteral("Procedural3DLayer") ||
                 serializedType == QStringLiteral("Procedural3D") ||
                 legacyLayerType == QStringLiteral("Procedural3D") ||
                 json.value("layerType").toString() == QStringLiteral("Procedural3DLayer") ||
                 json.value("isProcedural3DLayer").toBool(false) ||
                 json.contains("procedural3D")) {
          type = LayerType::Procedural3D;
      }
      QString name = json.value("name").toString();
      if (name.isEmpty()) {
          name = json.value("layerName").toString("Layer");
      }
  ArtifactLayerFactory factory;
      const auto restoreSerializedId = [&json](ArtifactAbstractLayerPtr layer) {
          const QString idValue = json.value("id").toString().trimmed();
          if (!layer || idValue.isEmpty()) {
              return layer;
          }
          const LayerID serializedId(idValue);
          if (!serializedId.isNil()) {
              layer->setId(serializedId);
          }
          return layer;
      };
      if (type == LayerType::Model3D &&
          (json.contains("sourcePath") || json.contains("modelPath") || json.contains("fixedGeometry"))) {
          if (json.contains("fixedGeometry")) {
              ArtifactFixedGeometry3DLayerInitParams fixedParams(
                  name, static_cast<FixedGeometry3D>(std::clamp(
                      json.value("fixedGeometry").toInt(), 0, 5)));
              auto result = factory.createLayer(fixedParams);
              if (result.success && result.layer) {
                  if (auto modelLayer = dynamic_cast<Artifact3DLayer*>(result.layer.get())) {
                      if (json.contains("renderMode")) {
                          modelLayer->setRenderMode(static_cast<RenderMode>(json.value("renderMode").toInt()));
                      }
                  }
                  result.layer->fromJsonProperties(json);
                  ArtifactAbstractLayerPtr layer = result.layer;
                  return restoreSerializedId(layer);
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
              return restoreSerializedId(layer);
          }
          return nullptr;
      }
      if (type == LayerType::Video || json.value("type").toString() == QStringLiteral("VideoLayer")) {
          return restoreSerializedId(ArtifactVideoLayer::fromJson(json));
      }
      if (json.contains("svg.sourcePath") ||
          (type == LayerType::Shape && json.contains("sourcePath"))) {
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
              return restoreSerializedId(layer);
          }
          return nullptr;
      }
      if (type == LayerType::Shape) {
          return restoreSerializedId(ArtifactShapeLayer::fromJson(json));
      }
      ArtifactLayerInitParams paramsForFactory(name, type);
      auto result = factory.createLayer(paramsForFactory);
      if (result.success && result.layer) {
          result.layer->fromJsonProperties(json);
          ArtifactAbstractLayerPtr layer = result.layer;
          return restoreSerializedId(layer);
      }
      return nullptr;
  }

}



