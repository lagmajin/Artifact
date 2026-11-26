module;

#include <DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#define NOMINMAX
#define QT_NO_KEYWORDS

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QIODevice>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QPointF>
#include <QRgb>
#include <QRect>
#include <QRectF>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QUuid>
#include <QVariant>
#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>
#include <opencv2/core.hpp>

#include <Layer/ArtifactSolidGradientUtil.hpp>

module Artifact.Widgets.CompositionRenderController:LayerDrawing;

import Artifact.Widgets.CompositionRenderController;
import :RasterEffects;
import Animation.Transform3D;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Composition;
import Artifact.Layer.Image;
import Artifact.Layer.Light;
import Artifact.Layer.MpmSand2D;
import Artifact.Layer.ParametricComposition;
import Artifact.Layer.Shape;
import Artifact.Layer.Solid2D;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Layers.Model3D;
import Artifact.Layers.SolidImage;
import Artifact.Render.CompositionMatteSurface;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.Context;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.IRenderer;
import Artifact.Render.PointwiseEffectFusion;
import Artifact.Tool.Manager;
import Artifact.Tool.PuppetTool;
import Color.Float;
import Container.NamedVector;
import Core.Light;
import Frame.Position;
import Graphics.LayerBlendPipeline;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Image.UploadConversion;
import Layer.Blend;
import Material.Material;
import Math.Vec;
import Memory.SharedPtr;
import Mesh;
import Property.Abstract;
import Time.Rational;
import Utils.Id;
import Video.VideoFrame;

namespace Artifact {

bool layerHasRasterizerEffectsOrMasks(ArtifactAbstractLayer* targetLayer) {
  if (!targetLayer) {
    return false;
  }
  if (targetLayer->hasMasks()) {
    return true;
  }
  return targetLayer->hasEnabledRasterizerEffect();
}

bool layerHasEnabledMatteReferences(ArtifactAbstractLayer* targetLayer) {
  return targetLayer && targetLayer->hasEnabledExternalMatteReference();
}

int layerActiveMatteReferenceCount(ArtifactAbstractLayer* targetLayer) {
  return targetLayer ? targetLayer->enabledExternalMatteReferenceCount() : 0;
}

bool solid3DCardColor(ArtifactAbstractLayer* layer, FloatColor& color) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      layerHasEnabledMatteReferences(layer)) {
    return false;
  }

  if (auto* solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layer)) {
    if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
      return false;
    }
    color = solid2D->color();
    return true;
  }
  if (auto* solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layer)) {
    if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
      return false;
    }
    color = solidImage->color();
    return true;
  }
  return false;
}

bool isOpaqueSolid3DCard(ArtifactAbstractLayer* layer) {
  FloatColor color;
  return solid3DCardColor(layer, color) && color.a() >= 0.9999f;
}

struct DirectShape3DCard {
  std::vector<Detail::float2> fillPoints;
  std::vector<Detail::float2> strokePoints;
  FloatColor fillColor;
  FloatColor strokeColor;
  float strokeWidth = 0.0f;
  bool strokeClosed = false;
};

bool directShape3DCard(ArtifactAbstractLayer* layer,
                       DirectShape3DCard& card) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      layerHasEnabledMatteReferences(layer)) {
    return false;
  }
  auto* shapeLayer = dynamic_cast<ArtifactShapeLayer*>(layer);
  if (!shapeLayer) {
    return false;
  }
  const auto appendPoints = [](const std::vector<QPointF>& source,
                               std::vector<Detail::float2>& destination) {
    destination.clear();
    destination.reserve(source.size());
    for (const auto& point : source) {
      destination.push_back({static_cast<float>(point.x()),
                             static_cast<float>(point.y())});
    }
  };
  appendPoints(shapeLayer->direct3DCardFillPoints(), card.fillPoints);
  appendPoints(shapeLayer->direct3DCardStrokePoints(), card.strokePoints);
  card.fillColor = shapeLayer->direct3DCardFillColor();
  card.strokeColor = shapeLayer->direct3DCardStrokeColor();
  card.strokeWidth = shapeLayer->strokeWidth();
  card.strokeClosed = shapeLayer->direct3DCardStrokeClosed();
  return card.fillPoints.size() >= 3 || card.strokePoints.size() >= 2;
}

std::vector<Detail::float2> makeDirectShapeStrokeQuad(
    const Detail::float2& start, const Detail::float2& end, float width) {
  const float dx = end.x - start.x;
  const float dy = end.y - start.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.0001f || width <= 0.0f) {
    return {};
  }
  const float halfWidth = width * 0.5f;
  const float nx = -dy / length * halfWidth;
  const float ny = dx / length * halfWidth;
  return {{start.x + nx, start.y + ny}, {end.x + nx, end.y + ny},
          {end.x - nx, end.y - ny}, {start.x - nx, start.y - ny}};
}

const ArtifactCore::ImageF32x4_RGBA* textured3DCardBuffer(
    ArtifactAbstractLayer* layer, QString* sourceKind = nullptr) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      layerHasEnabledMatteReferences(layer)) {
    return nullptr;
  }
  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer);
      imageLayer && !imageLayer->sourceCropEnabled() &&
          imageLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("image");
    return &imageLayer->currentFrameBuffer();
  }
  if (auto* svgLayer = dynamic_cast<ArtifactSvgLayer*>(layer);
      svgLayer && svgLayer->isLoaded() && svgLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("svg");
    return &svgLayer->currentFrameBuffer();
  }
  if (auto* textLayer = dynamic_cast<ArtifactTextLayer*>(layer);
      textLayer && textLayer->hasCurrentFrameBuffer()) {
    if (sourceKind) *sourceKind = QStringLiteral("text");
    return &textLayer->currentFrameBuffer();
  }
  return nullptr;
}

ArtifactVideoLayer* readyVideo3DCardLayer(ArtifactAbstractLayer* layer,
                                          int64_t timelineFrame) {
  if (!layer || !layer->is3D() || layer->isAdjustmentLayer() ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
      layerHasRasterizerEffectsOrMasks(layer) || layer->hasModifiers() ||
      layerHasEnabledMatteReferences(layer)) {
    return nullptr;
  }
  auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer);
  return videoLayer && videoLayer->isLoaded() &&
                 videoLayer->isFrameCached(timelineFrame)
             ? videoLayer
             : nullptr;
}

QColor toQColor(const FloatColor &color) {

  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());

}

ArtifactCore::Light makeSceneLightFromLayer(const ArtifactLightLayer* layer,

                                            const RationalTime& time)

{

  ArtifactCore::Light light;

  if (!layer) {

    return light;

  }



  switch (layer->lightType()) {

  case LightType::Point:

    light.setType(ArtifactCore::LightType::Point);

    break;

  case LightType::Spot:

    light.setType(ArtifactCore::LightType::Spot);

    break;

  case LightType::Parallel:

    light.setType(ArtifactCore::LightType::Directional);

    break;

  case LightType::Ambient:

    light.setType(ArtifactCore::LightType::Ambient);

    break;

  case LightType::Area:

    light.setType(ArtifactCore::LightType::Area);

    break;

  }



  const auto color = layer->color();

  const float intensity =
      std::max(0.0f, layer->intensity().value / 100.0f);

  light.setColor(ArtifactCore::float3<float>{color.r(), color.g(), color.b()});

  light.setIntensity(intensity);



  const QMatrix4x4 globalMat = layer->getGlobalTransform4x4();

  const QVector3D position = globalMat.map(QVector3D(0.0f, 0.0f, 0.0f));

  QVector3D direction = globalMat.mapVector(QVector3D(0.0f, 0.0f, 1.0f));

  if (direction.lengthSquared() <= 0.000001f) {

    direction = QVector3D(0.0f, 0.0f, 1.0f);

  } else {

    direction.normalize();

  }



  light.setPosition(
      ArtifactCore::Coordinates::worldPoint3FromQVector3D(position));

  light.setDirection(
      ArtifactCore::Coordinates::worldVectorFromQVector3D(direction));



  if (layer->lightType() == LightType::Point) {

    light.setRange(layer->range());

  }

  if (layer->lightType() == LightType::Spot) {

    light.setRange(layer->coneLength());

    const auto outerHalfAngle = layer->coneAngle() * 0.5f;
    const ArtifactCore::Units::Degrees innerHalfAngle{
        std::max(0.0f,
                 (layer->coneAngle() - layer->coneFeather()).value) * 0.5f};

    light.setCutoff(innerHalfAngle, outerHalfAngle);
    light.setGoboTexturePath(layer->goboTexturePath().toStdString());
    light.setGoboIntensity(layer->goboIntensity());
    light.setGoboRotation(layer->goboRotation());
    light.setGoboInvert(layer->goboInvert());

  }

  if (layer->lightType() == LightType::Area) {

    // Area falloff is owned entirely by the shader's areaScale/distance^2
    // term (MeshRenderer PSMain, lightType == 4). Feeding setRange here as
    // well applied a second inverse-square on top of it, so a far-away point
    // went black twice as fast and the authored Range stopped meaning
    // anything. Range now only bounds the gizmo; brightness is area-driven.
    //
    // The shader computes a Disk's luminous area from AreaSize.x alone, so a
    // Disk must be authored square. Passing the same extent on both axes
    // keeps the reported area correct for any width/height the user dialled
    // in, instead of silently using the narrower side.
    const ArtifactCore::Units::Pixels areaWidth = layer->areaWidth();
    const ArtifactCore::Units::Pixels areaHeight = layer->areaHeight();
    if (layer->areaShape() == AreaLightShape::Disk) {
      const float diskExtent = std::max(areaWidth.value, areaHeight.value);
      light.setAreaSize(ArtifactCore::Units::Pixels{diskExtent},
                        ArtifactCore::Units::Pixels{diskExtent});
    } else {
      light.setAreaSize(areaWidth, areaHeight);
    }
    light.setAreaShape(static_cast<int>(layer->areaShape()));

  }


  // Shadow contract bridge: the layer owns the UI values (shadowRadius UI
  // default 10 -> softness 1.0). Core::Light carries them renderer-neutral;
  // MeshRenderer clamps softness to 0..2 for its 3x3 PCF path.
  light.setCastsShadows(layer->castsShadows());
  light.setShadowSoftness(layer->shadowRadius().value / 10.0f);


  return light;

}



struct SceneLightEntry {

  ArtifactCore::Light light;

  const ArtifactLightLayer* source = nullptr;

};

// Mirrors MeshRenderer::Impl::MaxSceneLights (ArtifactCore). Kept as a literal
// because the constant is private to that class; the shader hardcodes the same
// 8 in `min(SceneLightingMeta.x, 8u)`.
constexpr size_t kMaxForwardSceneLights = 8;

// Dedupes the overflow warning to one line per distinct dropped count so a
// long timeline does not flood the log, while still surfacing the change when
// the user adds or removes lights.
void reportSceneLightOverflow(size_t lightCount, size_t maxLights) {

  static thread_local size_t lastReported = 0;

  if (lastReported == lightCount) {

    return;

  }

  lastReported = lightCount;

  qWarning().noquote() << QStringLiteral(

      "[SceneLighting] %1 light layers are active but the forward path supports "

      "%2. The extra %3 are ignored; the layers last in order are the ones "

      "dropped.")

                               .arg(lightCount)

                               .arg(maxLights)

                               .arg(lightCount - maxLights);

}



QSet<QString> parseLightLinkIdSet(const QString& text)

{

  QSet<QString> ids;

  for (const auto& token : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {

    const QString id = token.trimmed();

    if (!id.isEmpty()) {

      ids.insert(id);

    }

  }

  return ids;

}



bool lightAppliesToLayer(const ArtifactLightLayer* light,

                         const ArtifactAbstractLayer* targetLayer)

{

  if (!light || !targetLayer) {

    return false;

  }



  const QString targetId = targetLayer->id().toString();

  switch (light->lightLinkMode()) {

  case LightLinkMode::All:

    return true;

  case LightLinkMode::IncludeOnly:

    return parseLightLinkIdSet(light->linkedLayerIdsText()).contains(targetId);

  case LightLinkMode::ExcludeList:

    return !parseLightLinkIdSet(light->excludedLayerIdsText()).contains(targetId);

  }

  return true;

}



std::vector<ArtifactCore::Light> filterSceneLightsForLayer(

    const ArtifactAbstractLayer* targetLayer,

    const std::vector<SceneLightEntry>& sceneLights)

{

  std::vector<ArtifactCore::Light> filtered;

  if (!targetLayer) {

    return filtered;

  }



  const auto* modelLayer = dynamic_cast<const Artifact3DLayer*>(targetLayer);

  if (modelLayer && !modelLayer->affectedByLights()) {

    return filtered;

  }



  filtered.reserve(sceneLights.size());

  for (const auto& entry : sceneLights) {

    if (!entry.source || !entry.source->isVisible()) {

      continue;

    }

    if (lightAppliesToLayer(entry.source, targetLayer)) {

      filtered.push_back(entry.light);

    }

  }

  return filtered;

}



QString buildLayerSurfaceCacheKey(ArtifactAbstractLayer *layer,

                                  const QImage &surface, int64_t frameNumber,

                                  quint64 surfaceGeneration) {

  if (!layer) {

    return QString();

  }



  QString key = layer->id().toString();

  key +=

      QStringLiteral("|size=%1x%2").arg(surface.width()).arg(surface.height());

  const auto *composition =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  const int64_t layerFrame = layer->currentFrame();
  const int64_t compositionFrame = composition
      ? composition->framePosition().framePosition()
      : layerFrame;
  key += QStringLiteral("|colorPipeline=%1")
             .arg(composition
                      ? composition->colorPipelineVersion()
                      : ArtifactAbstractComposition::CanonicalColorPipelineVersion);

  key += QStringLiteral("|generation=%1").arg(surfaceGeneration);
  key += QStringLiteral("|maskRevision=%1").arg(layer->maskRevision());
  if (layer->hasCachedAnimatedPropertiesWithPrefix(QStringLiteral("mask."))) {
    key += QStringLiteral("|maskFrame=%1:%2:%3")
               .arg(frameNumber)
               .arg(layerFrame)
               .arg(compositionFrame);
  }
  key += QStringLiteral("|effectRevision=%1").arg(layer->effectRevision());
  // Source-time-aware cache key: timeline frame alone collides when a remap
  // or stop-motion sample resolves to a different/held source frame.
  if (layer->hasSourceTimeMapping()) {
   if (frameNumber != std::numeric_limits<int64_t>::min()) {
    const double mappedSource = layer->getSourceFrameAtCompFrame(frameNumber);
    key += QStringLiteral("|sourceTime=%1:%2:%3:%4:%5")
               .arg(mappedSource, 0, 'g', 17)
               .arg(static_cast<int>(layer->timeRemapFrameBlendMode()))
               .arg(layer->timeRemapFrameBlendAmount(), 0, 'g', 9)
               .arg(layer->isStopMotionSamplingEnabled() ? 1 : 0)
               .arg(layer->stopMotionSamplingFrameRate(), 0, 'g', 17);
   } else {
    key += QStringLiteral("|sourceTime=noframe:%1:%2:%3:%4")
               .arg(static_cast<int>(layer->timeRemapFrameBlendMode()))
               .arg(layer->timeRemapFrameBlendAmount(), 0, 'g', 9)
               .arg(layer->isStopMotionSamplingEnabled() ? 1 : 0)
               .arg(layer->stopMotionSamplingFrameRate(), 0, 'g', 17);
   }
  }
  // Do not use QImage::cacheKey() as a generic layer identity here. Solid and
  // gradient layers create a fresh temporary QImage on every draw, even when
  // their content is unchanged, which would force a surface/GPU-cache miss on
  // every frame. Image/SVG/sequence identities are encoded by their typed
  // cache-key branches below (source version and, for animated effects,
  // frame number).
  // Layer opacity is applied at draw time (see baseOpacity below), after the
  // processed surface is recovered from cache. Keep it out of this identity so
  // opacity-only edits can reuse the rasterized surface and only re-composite.

  if (layer->hasAnimatedEffectProperties() ||
      (layer->effectEnvelope().enabled && layer->effectCount() > 0)) {
    key += QStringLiteral("|effectFrame=%1:%2:%3")
               .arg(frameNumber)
               .arg(layerFrame)
               .arg(compositionFrame);
  }



  const auto rgbaKey = [](float r, float g, float b, float a) {
    return QStringLiteral("%1,%2,%3,%4")
        .arg(r, 0, 'g', 9)
        .arg(g, 0, 'g', 9)
        .arg(b, 0, 'g', 9)
        .arg(a, 0, 'g', 9);
  };
  const auto hasAnimatedGradient = [](ArtifactAbstractLayer *target) {
    return target && target->hasCachedAnimatedPropertiesWithPrefix(
                         QStringLiteral("solid.gradient"));
  };

  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {
    const QRectF bounds = solid2D->localBounds();
    key += QStringLiteral(
               "|solid2D|color=%1|fill=%2|g0=%3|g1=%4|ang=%5|rev=%6|cx=%7|cy=%8|scale=%9|off=%10|bounds=%11x%12")
               .arg(rgbaKey(solid2D->color().r(), solid2D->color().g(),
                            solid2D->color().b(), solid2D->color().a()))
               .arg(static_cast<int>(solid2D->fillType()))
               .arg(rgbaKey(solid2D->gradientStartColor().r(),
                            solid2D->gradientStartColor().g(),
                            solid2D->gradientStartColor().b(),
                            solid2D->gradientStartColor().a()))
               .arg(rgbaKey(solid2D->gradientEndColor().r(),
                            solid2D->gradientEndColor().g(),
                            solid2D->gradientEndColor().b(),
                            solid2D->gradientEndColor().a()))
               .arg(solid2D->gradientAngleDegrees(), 0, 'g', 9)
               .arg(solid2D->gradientReverse() ? 1 : 0)
               .arg(solid2D->gradientCenterX(), 0, 'g', 9)
               .arg(solid2D->gradientCenterY(), 0, 'g', 9)
               .arg(solid2D->gradientScale(), 0, 'g', 9)
               .arg(solid2D->gradientOffset(), 0, 'g', 9)
               .arg(bounds.width(), 0, 'g', 17)
               .arg(bounds.height(), 0, 'g', 17);
    if (hasAnimatedGradient(solid2D)) {
      key += QStringLiteral("|gradientFrame=%1:%2:%3")
                 .arg(frameNumber)
                 .arg(layerFrame)
                 .arg(compositionFrame);
    }
    return key;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral(
               "|solidImage|color=%1|fill=%2|g0=%3|g1=%4|ang=%5|rev=%6|cx=%7|cy=%8|scale=%9|off=%10|bounds=%11x%12")
               .arg(rgbaKey(solidImage->color().r(), solidImage->color().g(),
                            solidImage->color().b(), solidImage->color().a()))
               .arg(static_cast<int>(solidImage->fillType()))
               .arg(rgbaKey(solidImage->gradientStartColor().r(),
                            solidImage->gradientStartColor().g(),
                            solidImage->gradientStartColor().b(),
                            solidImage->gradientStartColor().a()))
               .arg(rgbaKey(solidImage->gradientEndColor().r(),
                            solidImage->gradientEndColor().g(),
                            solidImage->gradientEndColor().b(),
                            solidImage->gradientEndColor().a()))
               .arg(solidImage->gradientAngleDegrees(), 0, 'g', 9)
               .arg(solidImage->gradientReverse() ? 1 : 0)
               .arg(solidImage->gradientCenterX(), 0, 'g', 9)
               .arg(solidImage->gradientCenterY(), 0, 'g', 9)
               .arg(solidImage->gradientScale(), 0, 'g', 9)
               .arg(solidImage->gradientOffset(), 0, 'g', 9)
               .arg(bounds.width(), 0, 'g', 17)
               .arg(bounds.height(), 0, 'g', 17);
    if (hasAnimatedGradient(solidImage)) {
      key += QStringLiteral("|gradientFrame=%1:%2:%3")
                 .arg(frameNumber)
                 .arg(layerFrame)
                 .arg(compositionFrame);
    }
    return key;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {

    key += QStringLiteral(
               "|image|src=%1|rev=%2|fit=%3|size=%4x%5|cs=%6|tf=%7|crop=%8|seq=%9|content=%10")

               .arg(imageLayer->sourcePath())

               .arg(imageLayer->sourceVersion())

               .arg(imageLayer->fitToLayer() ? 1 : 0)

               .arg(surface.width())

               .arg(surface.height())

               .arg(imageLayer->inputColorSpace())

               .arg(imageLayer->inputTransferFunction())
               .arg(imageLayer->sourceCropRevision())
               .arg(imageLayer->isImageSequence()
                        ? imageLayer->sequenceCachedFrameIndex()
                        : -1)
               .arg(imageLayer->isImageSequence()
                        ? imageLayer->sequenceCachedFrameContentKey()
                        : 0);

    const bool animatedSourceCrop =
        imageLayer->hasCachedAnimatedPropertiesWithPrefix(
            QStringLiteral("sourceCrop."));
    if (animatedSourceCrop) {
      key += QStringLiteral("|cropFrame=%1:%2:%3")
                 .arg(frameNumber)
                 .arg(layerFrame)
                 .arg(compositionFrame);
    }

    return key;

  }



  if (auto *shapeLayer = dynamic_cast<ArtifactShapeLayer *>(layer)) {
    const bool animated = shapeLayer->hasPathKeyframes() ||
        shapeLayer->hasCachedAnimatedPropertiesWithPrefix(
            QStringLiteral("shape."));
    key += QStringLiteral("|shape|w=%1|h=%2|type=%3")
               .arg(shapeLayer->shapeWidth())
               .arg(shapeLayer->shapeHeight())
               .arg(static_cast<int>(shapeLayer->shapeType()));
    if (animated) {
      key += QStringLiteral("|frame=%1:%2:%3")
                 .arg(frameNumber)
                 .arg(layerFrame)
                 .arg(compositionFrame);
    }
    key += QStringLiteral("|shapeRevision=%1")
               .arg(shapeLayer->contentRevision());
    return key;
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {

    key += QStringLiteral("|svg|src=%1|rev=%2|fit=%3|size=%4x%5")

               .arg(svgLayer->sourcePath())

               .arg(svgLayer->sourceVersion())

               .arg(svgLayer->fitToLayer() ? 1 : 0)

               .arg(surface.width())

               .arg(surface.height());

    return key;

  }



  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {

    static const bool profileVideoLayerRendering =
        qEnvironmentVariableIsSet("ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD") &&
        qEnvironmentVariable("ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD") !=
            QStringLiteral("0");

    key += QStringLiteral("|video|src=%1|rev=%2|frame=%3|proxy=%4|size=%5x%6")

               .arg(videoLayer->sourcePath())

               .arg(videoLayer->sourceVersion())

               .arg(QStringLiteral("%1:%2:%3")
                        .arg(frameNumber)
                        .arg(layerFrame)
                        .arg(compositionFrame))

               .arg(static_cast<int>(videoLayer->proxyQuality()))

               .arg(surface.width())

               .arg(surface.height());

    return key;

  }



  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    key += QStringLiteral("|text|rev=%1")
               .arg(textLayer->contentRevision());
    // Source text, ordinary text properties, and animator stacks can alter
    // the resolved glyph surface without an authoring mutation.
    if (textLayer->hasSourceTextKeyframes() ||
        textLayer->hasAnimatedTextProperties() ||
        textLayer->animatorCount() > 0) {
      key += QStringLiteral("|textFrame=%1:%2:%3")
                 .arg(frameNumber)
                 .arg(layerFrame)
                 .arg(compositionFrame);
    }

    return key;

  }



  // Precomposition surfaces depend on the referenced composition's revision,
  // sampled child frame, and instance overrides. Until all of those inputs
  // have a stable cache identity, leave unsupported layer types uncached.
  return QString();

}

// Renderer-owned, bounded GPU results for spatially constant pointwise inputs.
// No global device state: a backend/context replacement invalidates all entries.
class SolidPointwisePreviewCache final {
  struct Entry {
    QString owner;
    QString signature;
    Diligent::RefCntAutoPtr<Diligent::ITexture> input;
    Diligent::RefCntAutoPtr<Diligent::ITexture> output;
    QRgb sourceRgba = 0;
    std::uint64_t effectRevision = 0;
    quint64 used = 0;
  };
  ArtifactCore::NamedVector<Entry> entries_;
  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
  std::unique_ptr<ArtifactCore::LayerBlendPipeline> pipeline_;
  quint64 requests_ = 0, hits_ = 0, dispatches_ = 0, uploads_ = 0;
  const bool profilingEnabled_ =
      QSettings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"))
          .value(QStringLiteral("Diagnostics/EffectProfiling"),
                 qEnvironmentVariableIntValue("ARTIFACT_EFFECT_PROFILE") != 0)
          .toBool();

public:
  void clear(ArtifactIRenderer* renderer = nullptr) {
    if (renderer) renderer->flush();
    entries_.clear();
    pipeline_.reset();
    context_.Release();
    device_.Release();
  }

  Diligent::ITextureView* resolve(ArtifactAbstractLayer* layer,
                                 ArtifactIRenderer* renderer,
                                 const QRectF& bounds) {
    if (!layer || !renderer || layer->is3D() || layer->isAdjustmentLayer() ||
        layer->hasMasks() || layer->hasModifiers() ||
        layerHasEnabledMatteReferences(layer)) return nullptr;
    ArtifactCore::FloatColor color;
    if (auto* solid = dynamic_cast<ArtifactSolid2DLayer*>(layer)) {
      if (solid->fillType() != ArtifactSolidFillType::Solid) return nullptr;
      color = solid->color();
    } else if (auto* solid = dynamic_cast<ArtifactSolidImageLayer*>(layer)) {
      if (solid->fillType() != ArtifactSolidFillType::Solid) return nullptr;
      color = solid->color();
    } else {
      return nullptr;
    }
    // Legacy surfaces evaluate RGB in their premultiplied storage domain.
    // Only opaque sources are equivalent to the straight pointwise contract.
    if (color.a() != 1.0f) return nullptr;
    const QColor sourceColor = toQColor(color);
    const std::array<float, 4> pixel{
        sourceColor.blue() / 255.0f, sourceColor.green() / 255.0f,
        sourceColor.red() / 255.0f, 1.0f};
    // Match the existing controller's ARGB32 -> CV_32FC4 storage order.
    // Channel-order normalization is a separate compatibility change.
    const auto device = renderer->device();
    const auto context = renderer->immediateContext();
    if (!device || !context) return nullptr;
    if (device_ != device || context_ != context) {
      renderer->flush();
      clear();
      device_ = device;
      context_ = context;
    }

    ++requests_;
    const QString owner = layer->id().toString();
    Entry *entry = nullptr;
    for (auto &candidate : entries_) {
      if (candidate.owner == owner) {
        entry = &candidate;
        break;
      }
    }
    const QRgb sourceRgba = sourceColor.rgba();
    const std::uint64_t effectRevision = layer->effectRevision();
    const auto layerEffectEnvelope = layer->effectEnvelope();
    const bool envelopeAffectsEffects =
        layerEffectEnvelope.enabled && layer->effectCount() > 0;
    if (!layer->hasAnimatedEffectProperties() && !envelopeAffectsEffects &&
        entry && entry->output &&
        entry->sourceRgba == sourceRgba &&
        entry->effectRevision == effectRevision) {
      ++hits_;
      entry->used = requests_;
      report();
      return entry->output->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }

    ArtifactCore::PointwiseEffectStack stack;
    QString signature = QString::number(sourceRgba);
    if (envelopeAffectsEffects) {
      signature += QStringLiteral("|effectStrength=%1")
                       .arg(layerEffectEnvelope.sample(layer->currentFrame())
                                .effectStrength,
                            0, 'g', 9);
    }
    std::uint32_t slot = 0;
    const auto effects = layer->getEffects();
    // Reject the entire stack before evaluating or submitting any prefix.
    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled()) continue;
      if (!ArtifactCore::dynamicPointerCast<ExposureEffect>(effect) ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer ||
          effect->computeMode() == ComputeMode::CPU) return nullptr;
    }
    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled()) continue;
      effect->setContext(makeControllerEffectContext(layer,
          QRectF(0, 0, bounds.width(), bounds.height())));
      if (effect->mix() != 1.0f || effect->hasEffectRegion() ||
          effect->maskEnabled() || effect->effectMaskImageCount() != 0 ||
          effect->allowOverscan() || slot + 5 > stack.kParameterSlotCount)
        return nullptr;
      const auto exposure = ArtifactCore::dynamicPointerCast<ExposureEffect>(effect);
      const float ev = exposure->exposure();
      const float offset = exposure->offset();
      const float gamma = exposure->gammaCorrection();
      if (!std::isfinite(ev) || !std::isfinite(offset) || !std::isfinite(gamma))
        return nullptr;
      signature += QStringLiteral("|%1,%2,%3").arg(ev, 0, 'g', 9)
          .arg(offset, 0, 'g', 9).arg(gamma, 0, 'g', 9);
      stack.addNode(ArtifactCore::PointwiseNodeKind::Exposure, slot);
      stack.setParameter(slot++, ev);
      stack.addNode(ArtifactCore::PointwiseNodeKind::Offset, slot);
      stack.setParameter(slot++, offset);
      // Match ExposureEffect's GPU identity-gamma threshold exactly.
      if (std::abs(1.0f / std::max(0.0001f, gamma) - 1.0f) > 0.0001f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Gamma, slot);
        stack.setParameter(slot++, gamma);
      }
      stack.addNode(ArtifactCore::PointwiseNodeKind::Clamp, slot);
      stack.setParameter(slot++, 0.0f);
      stack.setParameter(slot++, 1.0f);
    }
    if (stack.nodes().empty()) return nullptr;
    // Match convertImageForUpload(Rgba32LinearStraight) at the legacy
    // unknown-transfer boundary, after all effects have clamped to [0,1].
    stack.addNode(ArtifactCore::PointwiseNodeKind::SrgbToLinear);
    if (entry && entry->signature == signature && entry->output) {
      ++hits_;
      entry->used = requests_;
      entry->effectRevision = effectRevision;
      report();
      return entry->output->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }
    // Submit queued sprites before reusing/evicting a texture they reference.
    // Same-context resource transitions order subsequent compute and sampling;
    // no CPU readback or queue-idle wait is needed.
    renderer->flush();
    if (!pipeline_) {
      pipeline_ = renderer->createLayerBlendPipeline();
      if (!pipeline_ || !pipeline_->initialize()) {
        pipeline_.reset();
        return nullptr;
      }
    }
    if (!entry) {
      if (entries_.size() < 64) {
        entries_.push_back(Entry{});
        entry = entries_.last();
      } else {
        entry = &*std::min_element(entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) { return a.used < b.used; });
      }
      entry->owner = owner;
    }
    entry->signature.clear();
    entry->used = requests_;
    Diligent::TextureDesc desc;
    desc.Name = "Solid pointwise preview input";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = desc.Height = 1;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    Diligent::TextureSubResData data;
    data.pData = pixel.data();
    data.Stride = sizeof(pixel);
    Diligent::TextureData initial;
    initial.pSubResources = &data;
    initial.NumSubresources = 1;
    if (!entry->input || entry->sourceRgba != sourceRgba) {
      entry->input.Release();
      device->CreateTexture(desc, &initial, &entry->input);
      if (!entry->input) return nullptr;
      entry->sourceRgba = sourceRgba;
      ++uploads_;
    }
    if (!entry->output) {
      desc.Name = "Solid pointwise preview result";
      desc.Usage = Diligent::USAGE_DEFAULT;
      desc.BindFlags = Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS;
      device->CreateTexture(desc, nullptr, &entry->output);
    }
    if (!entry->output) return nullptr;
    const auto segments = stack.segments(ArtifactCore::PointwiseAlphaMode::Straight);
    if (segments.size() != 1 || !stack.validate().valid) return nullptr;
    const auto plan = ArtifactCore::PointwiseEffectFusion::makeComputePlan(
        "diligent", "rgba32f", stack.nodes(), segments.front(), 1, 1);
    if (!pipeline_->updatePointwiseParameters(context, stack) ||
        !pipeline_->applyPointwise(context,
            entry->input->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE),
            entry->output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS),
            pipeline_->createPointwiseParameterBuffer(), plan)) return nullptr;
    entry->signature = signature;
    entry->effectRevision = effectRevision;
    ++dispatches_;
    report();
    return entry->output->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  }

private:
  void report() const {
    if (requests_ != 1 && requests_ % 120 != 0) return;
    if (!profilingEnabled_) return;
    qInfo() << "[SolidPointwisePreview] requests=" << requests_ << "hits=" << hits_
            << "dispatches=" << dispatches_ << "uploadBytes=" << uploads_ * 16
            << "readbackBytes=0 idleWaits=0 entries=" << entries_.size();
  }
};

static Diligent::ITextureView* resolveProjectorTexture(
    ArtifactAbstractLayer* sourceLayer, int64_t frame,
    GPUTextureCacheManager* cacheManager);
static QMatrix4x4 resolveProjectorViewMatrix(
    ArtifactAbstractLayer* sourceLayer);
static QMatrix4x4 resolveProjectorProjMatrix(
    ArtifactAbstractLayer* sourceLayer);

struct ModelLayerFrustumCullingDecision {
  bool tested = false;
  bool culled = false;
};

// Conservative view-frustum rejection for model layers. Side planes plus the
// behind-camera case are enough for viewport culling and avoid depending on
// whether the projection uses [-1,1] or [0,1] depth, or on reversed-Z.
// Skinned and blend-shape meshes are classified after the caller has advanced
// the pose, so the mesh bounds already describe the vertices that will draw.
enum class BoxFrustumClass { Intersects, Outside, Unknown };

static BoxFrustumClass classifyBoxFrustum(const QMatrix4x4& mvp,
                                          const QVector3D& minB,
                                          const QVector3D& maxB) {
  const std::array<QVector3D, 8> corners{{
      {minB.x(), minB.y(), minB.z()},
      {maxB.x(), minB.y(), minB.z()},
      {minB.x(), maxB.y(), minB.z()},
      {maxB.x(), maxB.y(), minB.z()},
      {minB.x(), minB.y(), maxB.z()},
      {maxB.x(), minB.y(), maxB.z()},
      {minB.x(), maxB.y(), maxB.z()},
      {maxB.x(), maxB.y(), maxB.z()}}};

  bool allOutsideLeft = true;
  bool allOutsideRight = true;
  bool allOutsideTop = true;
  bool allOutsideBottom = true;
  bool allBehindCamera = true;
  for (const QVector3D& corner : corners) {
    const QVector4D clip = mvp * QVector4D(corner, 1.0f);
    const float w = clip.w();
    if (!std::isfinite(w) || !std::isfinite(clip.x()) ||
        !std::isfinite(clip.y())) {
      return BoxFrustumClass::Unknown;
    }
    allBehindCamera &= w <= 0.0f;
    if (w <= 0.0f) {
      allOutsideLeft = false;
      allOutsideRight = false;
      allOutsideTop = false;
      allOutsideBottom = false;
      continue;
    }
    allOutsideLeft &= clip.x() < -w;
    allOutsideRight &= clip.x() > w;
    allOutsideTop &= clip.y() > w;
    allOutsideBottom &= clip.y() < -w;
  }
  if (allBehindCamera || allOutsideLeft || allOutsideRight ||
      allOutsideTop || allOutsideBottom) {
    return BoxFrustumClass::Outside;
  }
  return BoxFrustumClass::Intersects;
}

static ModelLayerFrustumCullingDecision classifyModelLayerFrustum(
    const Artifact3DLayer& layer, const QMatrix4x4& view,
    const QMatrix4x4& proj) {
  ModelLayerFrustumCullingDecision decision;
  const auto& mesh = layer.mesh();
  if (mesh.vertexCount() <= 0) {
    return decision;
  }
  const QVector3D minB = mesh.boundingBoxMin();
  const QVector3D maxB = mesh.boundingBoxMax();
  if (!std::isfinite(minB.x()) || !std::isfinite(minB.y()) ||
      !std::isfinite(minB.z()) || !std::isfinite(maxB.x()) ||
      !std::isfinite(maxB.y()) || !std::isfinite(maxB.z())) {
    return decision;
  }
  if (minB.x() > maxB.x() || minB.y() > maxB.y() || minB.z() > maxB.z()) {
    return decision;
  }

  // Artifact3DLayer::draw() applies only this layer's own transform (the
  // parent chain is not composed into the model matrix) and samples it at the
  // layer's current frame, not the global composition frame. Rebuild the same
  // matrix so culling matches animated/time-remapped layers.
  const double compositionFps = std::isfinite(layer.compositionFrameRate()) &&
                                        layer.compositionFrameRate() > 0.0
                                    ? layer.compositionFrameRate()
                                    : 30.0;
  const RationalTime modelTime(layer.currentFrame(), compositionFps);
  const auto transformSnapshot = layer.transform3D().snapshotAt(modelTime);
  QMatrix4x4 modelMatrix;
  modelMatrix.setToIdentity();
  modelMatrix.translate(transformSnapshot.positionX,
                        transformSnapshot.positionY,
                        transformSnapshot.positionZ);
  modelMatrix.rotate(transformSnapshot.rotationX, 1.0f, 0.0f, 0.0f);
  modelMatrix.rotate(transformSnapshot.rotationY, 0.0f, 1.0f, 0.0f);
  modelMatrix.rotate(transformSnapshot.rotationZ, 0.0f, 0.0f, 1.0f);
  modelMatrix.scale(transformSnapshot.scaleX, transformSnapshot.scaleY,
                    transformSnapshot.scaleZ);
  modelMatrix.translate(-transformSnapshot.anchorX, -transformSnapshot.anchorY,
                        -transformSnapshot.anchorZ);
  const QMatrix4x4 baseMvp = proj * view * modelMatrix;
  // The combined mesh bounds cover the single-instance paths (including the
  // single-instance-per-material-slot import case).
  BoxFrustumClass baseClass = classifyBoxFrustum(baseMvp, minB, maxB);
  if (baseClass == BoxFrustumClass::Unknown) {
    return decision;
  }
  bool anyInstanceIntersects = baseClass == BoxFrustumClass::Intersects;
  if (!anyInstanceIntersects) {
    // Multi-primitive sources concatenate their geometry into mesh() and place
    // each material span with a hierarchy-local transform. Test the combined
    // bounds under every transform that the draw path can apply; keeping the
    // layer if any box intersects stays conservative for both instance modes.
    for (const QMatrix4x4& local : layer.meshLocalTransforms()) {
      const BoxFrustumClass instanceClass = classifyBoxFrustum(
          baseMvp * local, minB, maxB);
      if (instanceClass != BoxFrustumClass::Outside) {
        anyInstanceIntersects = true;
        break;
      }
    }
  }

  decision.tested = true;
  decision.culled = !anyInstanceIntersects;
  return decision;
}

void drawLayerForCompositionView(

    ArtifactAbstractLayer *layer, ArtifactIRenderer *renderer,

    float opacityOverride = -1.0f, QString *videoDebugOut = nullptr,

    QHash<QString, LayerSurfaceCacheEntry> *surfaceCache = nullptr,

    bool *surfaceCacheDiagnosticsEnabled = nullptr,

    QHash<QString, quint64> *surfaceCacheHitCounts = nullptr,

    QHash<QString, quint64> *surfaceCacheMissCounts = nullptr,

    QHash<QString, quint64> *surfaceCacheBuildCounts = nullptr,

    GPUTextureCacheManager *gpuTextureCacheManager = nullptr,

    int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),

    bool useGpuPath = false, const DetailLevel lod = DetailLevel::High,

    const QMatrix4x4 *cameraView = nullptr,

    const QMatrix4x4 *cameraProj = nullptr,
    const QMatrix4x4 *previousCameraView = nullptr,
    const QMatrix4x4 *previousCameraProj = nullptr,
    const std::function<QImage(const ArtifactCore::Id &)> *matteSourceResolver = nullptr,

    const std::vector<SceneLightEntry> *sceneLights = nullptr,

    bool interactiveDraft = false, bool applyViewportCameraTo2D = false,
    quint64 surfaceGeneration = 1,
    const std::function<Diligent::ITextureView*(ArtifactAbstractLayer*,
                                                int64_t, const QSize&)>*
        precompGpuResolver = nullptr,
    bool deferRasterizerEffectsToGpu = false,
    SolidPointwisePreviewCache* solidPointwiseCache = nullptr,
    CompositionRenderController::ViewportCullingStats* cullingStats = nullptr) {

  if (!layer || !renderer) {

    qCDebug(compositionViewLog)

        << "[CompositionView] drawLayerForCompositionView: invalid "

           "layer/renderer";

    return;

  }



  if (const auto parent = layer->parentLayer();

      parent && parent->isGroupLayer()) {

    return;

  }



  // Light layers contribute scene lighting, not beauty pixels. Their draw()
  // emits editor controls and belongs to the viewport overlay pass.
  if (dynamic_cast<ArtifactLightLayer*>(layer)) return;

  const QRectF localRect = layer->localBounds();

  if (!localRect.isValid() || localRect.width() <= 0.0 ||

      localRect.height() <= 0.0) {

    qCDebug(compositionViewLog)

        << "[CompositionView] skip layer: invalid local bounds"

        << "id=" << layer->id().toString() << "rect=" << localRect;

    return;

  }



  const QTransform globalTransform = layer->getGlobalTransform();

  const QMatrix4x4 globalTransform4x4 = layer->getGlobalTransform4x4();

  if (useGpuPath && solidPointwiseCache && !applyViewportCameraTo2D &&
      (!sceneLights || sceneLights->empty())) {
    if (auto* srv = solidPointwiseCache->resolve(layer, renderer, localRect)) {
      const float opacity = opacityOverride >= 0.0f ? opacityOverride : layer->opacity();
      renderer->drawSpriteTransformed(
          static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
          static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
          globalTransform4x4, srv, opacity);
      return;
    }
  }


  // Keep a queued 3D overlay bound to the camera that produced it. The scope
  // centralizes the required set -> draw -> optional flush -> reset order.
  struct Scoped3DLayerCamera final {
    ArtifactIRenderer *renderer = nullptr;
    bool active = false;
    bool flushOnExit = false;

    Scoped3DLayerCamera(ArtifactIRenderer *target,
                        const QMatrix4x4 *view,
                        const QMatrix4x4 *projection,
                        bool flush = false)
        : renderer(target),
          active(target && view && projection),
          flushOnExit(flush) {
      if (active) {
        renderer->set3DCameraMatrices(*view, *projection);
      }
    }

    Scoped3DLayerCamera(const Scoped3DLayerCamera &) = delete;
    Scoped3DLayerCamera &operator=(const Scoped3DLayerCamera &) = delete;

    void setPrevious(const QMatrix4x4 *view, const QMatrix4x4 *projection) {
      if (active && view && projection) {
        renderer->setPrevious3DCameraMatrices(*view, *projection);
      }
    }

    ~Scoped3DLayerCamera() {
      if (!renderer) {
        return;
      }
      if (flushOnExit) {
        renderer->flushGizmo3D();
      }
      if (active) {
        renderer->reset3DCameraMatrices();
      }
    }
  };

  FloatColor cardColor;

  if (solid3DCardColor(layer, cardColor)) {

    const float cardOpacity =

        opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

    const bool writeDepth = cardOpacity * cardColor.a() >= 0.9999f;

    Scoped3DLayerCamera layerCamera(renderer, cameraView, cameraProj);
    renderer->draw3DCard(localRect, globalTransform4x4, cardColor,
                         cardOpacity, writeDepth);

    return;

  }

  DirectShape3DCard shapeCard;

  if (directShape3DCard(layer, shapeCard)) {

    const float cardOpacity =

        opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

    const bool fillWriteDepth =

        cardOpacity * shapeCard.fillColor.a() >= 0.9999f;

    const bool strokeWriteDepth =

        cardOpacity * shapeCard.strokeColor.a() >= 0.9999f;

    Scoped3DLayerCamera layerCamera(renderer, cameraView, cameraProj);

    if (shapeCard.fillPoints.size() >= 3) {

      renderer->draw3DShape(shapeCard.fillPoints, globalTransform4x4,

                            shapeCard.fillColor, cardOpacity,

                            fillWriteDepth);

    }

    if (shapeCard.strokePoints.size() >= 2 &&

        shapeCard.strokeWidth > 0.0f) {

      const size_t strokeSegmentCount =

          shapeCard.strokeClosed ? shapeCard.strokePoints.size()

                                 : shapeCard.strokePoints.size() - 1;

      for (size_t segment = 0; segment < strokeSegmentCount; ++segment) {

        const auto& start = shapeCard.strokePoints[segment];

        const auto& end = shapeCard.strokePoints[

            (segment + 1) % shapeCard.strokePoints.size()];

        const auto quad = makeDirectShapeStrokeQuad(

            start, end, shapeCard.strokeWidth);

        if (quad.size() == 4) {

          renderer->draw3DShape(quad, globalTransform4x4,

                                shapeCard.strokeColor, cardOpacity,

                                strokeWriteDepth);

        }

      }

    }

    return;

  }

  QString texturedCardKind;

  if (const auto* cardBuffer =

          textured3DCardBuffer(layer, &texturedCardKind);

      cardBuffer && gpuTextureCacheManager) {

    QString textureOwner = layer->id().toString();

    QString textureKey = QStringLiteral("3d-card:%1:g%2:f%3")

                             .arg(texturedCardKind)

                             .arg(surfaceGeneration)

                             .arg(cacheFrameNumber);

    if (auto* imageCard = dynamic_cast<ArtifactImageLayer*>(layer);

        imageCard && imageCard->canShareSourceGpuTexture() &&

        !imageCard->sourceAssetId().isNull() &&

        imageCard->sourceVersion() > 0) {

      textureOwner = QStringLiteral("asset:%1").arg(

          imageCard->sourceAssetId().toString(QUuid::WithoutBraces));

      textureKey = QStringLiteral("image-f32:v%1|cs=%2|tf=%3|3d-card")

                       .arg(imageCard->sourceVersion())

                       .arg(imageCard->inputColorSpace())

                       .arg(imageCard->inputTransferFunction());

    }

    const auto handle = gpuTextureCacheManager->acquireOrCreate(

        textureOwner, textureKey, *cardBuffer);

    const auto binding = gpuTextureCacheManager->bindingRecord(handle);

    if (binding.isValid()) {

      const float cardOpacity =

          opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

      Scoped3DLayerCamera layerCamera(renderer, cameraView, cameraProj);
      if (layer->projectionEnabled() && !layer->projectionSourceLayerId().isEmpty()) {
        if (auto* composition = static_cast<ArtifactAbstractComposition*>(
                layer->composition())) {
          const auto sourceLayer = composition->layerById(
              ArtifactCore::LayerID(layer->projectionSourceLayerId()));
          if (sourceLayer) {
            if (auto* projectorSRV = resolveProjectorTexture(
                    sourceLayer.get(), cacheFrameNumber, gpuTextureCacheManager)) {
              renderer->setProjectorSource(projectorSRV);
              renderer->setProjectorMatrices(
                  resolveProjectorViewMatrix(sourceLayer.get()),
                  resolveProjectorProjMatrix(sourceLayer.get()));
              renderer->setProjectorEnabled(true);
            }
          }
        }
      }
      renderer->draw3DTexturedCard(localRect, globalTransform4x4,
                                   binding.srv, cardOpacity);
      if (layer->projectionEnabled()) {
        renderer->resetProjector();
      }
      return;

    }

  }

  if (auto* videoCard = readyVideo3DCardLayer(layer, cacheFrameNumber);

      videoCard && gpuTextureCacheManager) {

    const auto frameBuffer =

        videoCard->cachedFrameImageBuffer(cacheFrameNumber);

    if (!frameBuffer.isEmpty()) {

      const QString textureOwner = layer->id().toString();

      const QString textureKey = QStringLiteral("3d-card:video:s%1:g%2")

                                     .arg(videoCard->currentSourceFrameValue())

                                     .arg(surfaceGeneration);

      const auto handle = gpuTextureCacheManager->acquireOrCreate(

          textureOwner, textureKey, frameBuffer);

      const auto binding = gpuTextureCacheManager->bindingRecord(handle);

      if (binding.isValid()) {

        const float cardOpacity =

            opacityOverride >= 0.0f ? opacityOverride : layer->opacity();

        videoCard->markFrameRenderQueued(cacheFrameNumber);

        // This is an existing video-card path; use the same local camera
        // lifetime as the other immediate 3D cards without changing decoding.
        Scoped3DLayerCamera layerCamera(renderer, cameraView, cameraProj);
        renderer->draw3DTexturedCard(localRect, globalTransform4x4,
                                     binding.srv, cardOpacity);

        return;

      }

    }

  }



  if (auto *depthImage = dynamic_cast<ArtifactImageLayer *>(layer);
      depthImage && depthImage->hasDepthMap() &&
      depthImage->hasCurrentFrameBuffer() && gpuTextureCacheManager) {
    const auto &imageBuffer = depthImage->currentFrameBuffer();
    const QString textureKey = QStringLiteral("depth-image:f%1:g%2")
                                   .arg(cacheFrameNumber)
                                   .arg(surfaceGeneration);
    const auto handle = gpuTextureCacheManager->acquireOrCreate(
        layer->id().toString(), textureKey, imageBuffer);
    const auto binding = gpuTextureCacheManager->bindingRecord(handle);
    if (binding.isValid()) {
      ArtifactCore::Material material = ArtifactCore::Material::makeDefault();
      Scoped3DLayerCamera layerCamera(renderer, cameraView, cameraProj);
      renderer->drawMesh(QStringLiteral("depth-image:%1").arg(layer->id().toString()),
                         depthImage->depthMesh(), material,
                         globalTransform4x4,
                         opacityOverride >= 0.0f ? opacityOverride : layer->opacity(),
                         3, nullptr, binding.srv);
      return;
    }
  }

  // Handle 3D layers separately

  if (layer->is3D()) {

    std::vector<ArtifactCore::Light> filteredLights;

    auto *modelLayer = dynamic_cast<Artifact3DLayer *>(layer);

    // drawMesh() also registers/re-activates the mesh in the ray tracing scene.
    // Skipping a culled layer would leave stale or missing BLAS/TLAS instances,
    // so keep the full draw path whenever ray tracing owns that state.
    const bool rayTracingSceneTracking =
        renderer && renderer->rayTracingManager() &&
        renderer->rayTracingManager()->isSupported();

    if (modelLayer && cameraView && cameraProj && !rayTracingSceneTracking) {
      // draw() would advance the skin pose; do it first so the bounds used for
      // culling match the vertices that would be drawn.
      modelLayer->evaluateSkinAnimation();
      const auto culling =
          classifyModelLayerFrustum(*modelLayer, *cameraView, *cameraProj);
      if (culling.tested) {
        if (cullingStats) {
          ++cullingStats->tested3DLayers;
          if (culling.culled) {
            ++cullingStats->culled3DLayers;
          } else {
            ++cullingStats->drawn3DLayers;
          }
        }
        if (culling.culled) {
          return;
        }
      }
    }

    if (modelLayer && sceneLights) {

      filteredLights = filterSceneLightsForLayer(modelLayer, *sceneLights);

      renderer->setSceneLights(filteredLights);

    }

    const bool isLightLayer =
        dynamic_cast<ArtifactLightLayer *>(layer) != nullptr;
    {
      Scoped3DLayerCamera layerCamera(
          renderer, cameraView, cameraProj, isLightLayer);
      layerCamera.setPrevious(previousCameraView, previousCameraProj);
      layer->draw(renderer);
    }

    if (modelLayer && sceneLights) {

      std::vector<ArtifactCore::Light> restoreLights;

      restoreLights.reserve(sceneLights->size());

      for (const auto &entry : *sceneLights) {

        restoreLights.push_back(entry.light);

      }

      renderer->setSceneLights(restoreLights);

    }

    return;

  }

  struct External2DMatrixScope {
    ArtifactIRenderer *renderer = nullptr;
    bool active = false;

    ~External2DMatrixScope() {
      if (renderer && active) {
        renderer->setUseExternalMatrices(false);
      }
    }
  } external2DMatrixScope{
      renderer, applyViewportCameraTo2D && cameraView && cameraProj};

  if (external2DMatrixScope.active) {
    renderer->setViewMatrix(*cameraView);
    renderer->setProjectionMatrix(*cameraProj);
    renderer->setUseExternalMatrices(true);
  }

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF &rect,

                                 bool allowSurfaceCache) {

    if (surface.isNull()) {

      return false;

    }



    const float overscanPixels = layerOverscanPixels(layer);
    const QRectF outputRect = overscanPixels > 0.0f
        ? rect.adjusted(-overscanPixels, -overscanPixels, overscanPixels, overscanPixels)
        : rect;

    QHash<ArtifactCore::Id, QImage> resolvedMatteSources;

    QString matteSourceSignature;

    if (matteSourceResolver && layerHasEnabledMatteReferences(layer)) {

      for (const auto &ref : layer->matteReferences()) {

        if (!ref.enabled || ref.sourceLayerId.isNil() ||
            ref.sourceLayerId == layer->id()) {

          continue;

        }

        QImage source = (*matteSourceResolver)(ref.sourceLayerId);

        if (source.isNull()) {

          matteSourceSignature +=

              QStringLiteral("|matte=%1:null").arg(ref.sourceLayerId.toString());

          continue;

        }

        matteSourceSignature +=

            QStringLiteral("|matte=%1:%2:%3x%4:type=%5:blend=%6:fit=%7:opacity=%8:invert=%9")

                .arg(ref.sourceLayerId.toString())

                .arg(static_cast<qulonglong>(source.cacheKey()))

                .arg(source.width())

                .arg(source.height())
                .arg(static_cast<int>(ref.type))
                .arg(static_cast<int>(ref.blendMode))
                .arg(static_cast<int>(ref.fitMode))
                .arg(ref.opacity, 0, 'g', 9)
                .arg(ref.invert ? 1 : 0);

        resolvedMatteSources.insert(ref.sourceLayerId, std::move(source));

      }

    }



    const QString ownerId = layer->id().toString();

    const float effectResolutionScale = interactiveDraft
        ? 0.25f
        : lod == DetailLevel::Low
              ? 0.25f
              : lod == DetailLevel::Medium ? 0.5f : 1.0f;
    QString cacheSignature = buildLayerSurfaceCacheKey(
        layer, surface, cacheFrameNumber, surfaceGeneration);
    if (!cacheSignature.isEmpty()) {
      cacheSignature += QStringLiteral("|interactiveDraft=%1")
                            .arg(interactiveDraft ? 1 : 0);

      cacheSignature += QStringLiteral("|effectScale=%1")
                            .arg(effectResolutionScale, 0, 'f', 2);

      cacheSignature += QStringLiteral("|gpuPointwise=%1")
                            .arg(deferRasterizerEffectsToGpu ? 1 : 0);

      cacheSignature += QStringLiteral("|overscan=%1")
                            .arg(overscanPixels, 0, 'f', 3);

      cacheSignature += matteSourceSignature;
    }

    QString gpuOwnerId = ownerId;
    QString gpuCacheSignature = cacheSignature;
    if (!allowSurfaceCache && matteSourceSignature.isEmpty()) {
      if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer);
          imageLayer && imageLayer->canShareSourceGpuTexture()) {
        const auto version = imageLayer->sourceVersion();
        if (version > 0) {
          gpuOwnerId = QStringLiteral("asset:%1").arg(
              imageLayer->sourceAssetId().toString(QUuid::WithoutBraces));
          gpuCacheSignature =
              QStringLiteral("image-f32:v%1|cs=%2|tf=%3")
                  .arg(version)
                  .arg(imageLayer->inputColorSpace())
                  .arg(imageLayer->inputTransferFunction());
        }
      }
    }

    auto applyResolvedMattes = [&](QImage& targetSurface) {
      if (targetSurface.isNull() || resolvedMatteSources.isEmpty()) {
        return;
      }

      const std::function<QImage(const ArtifactCore::Id&)> cachedResolver =
          [&resolvedMatteSources](const ArtifactCore::Id& id) {
            return resolvedMatteSources.value(id);
          };

      applyLayerMatteToSurface(layer, targetSurface, cachedResolver);
    };

    auto applyResolvedMattesF32 = [&](ArtifactCore::ImageF32x4_RGBA& targetSurface) {
      if (targetSurface.isEmpty() || resolvedMatteSources.isEmpty()) {
        return;
      }

      const std::function<QImage(const ArtifactCore::Id&)> cachedResolver =
          [&resolvedMatteSources](const ArtifactCore::Id& id) {
            return resolvedMatteSources.value(id);
          };
      applyLayerMatteToSurface(layer, targetSurface, cachedResolver);
    };

    const QString cacheSlotKey =

        ownerId + (interactiveDraft ? QStringLiteral("|draft")

                                    : QStringLiteral("|full"));

    LayerSurfaceCacheEntry *cacheEntry = nullptr;
    ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> directProcessedBuffer;



    const bool useSurfaceCache = surfaceCache && !cacheSignature.isEmpty();



    if (useSurfaceCache) {

      auto cacheIt = surfaceCache->find(cacheSlotKey);

      if (cacheIt != surfaceCache->end() && cacheIt->ownerId == ownerId &&

          cacheIt->cacheSignature == cacheSignature &&

          (!cacheIt->processedSurface.isNull() || cacheIt->processedBuffer)) {

        if (surfaceCacheDiagnosticsEnabled && *surfaceCacheDiagnosticsEnabled &&
            surfaceCacheHitCounts) {
          ++(*surfaceCacheHitCounts)[cacheSlotKey];
        }
        cacheEntry = &(*cacheIt);
        directProcessedBuffer = cacheEntry->processedBuffer;

        if (!cacheIt->processedSurface.isNull()) {

          surface = cacheIt->processedSurface;

        }

      } else {

        if (surfaceCacheDiagnosticsEnabled && *surfaceCacheDiagnosticsEnabled &&
            surfaceCacheMissCounts) {
          ++(*surfaceCacheMissCounts)[cacheSlotKey];
        }
        ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;

        if (allowSurfaceCache) {

          ArtifactCore::ImageF32x4_RGBA processed;

          // Downsampling already controls preview cost. Skipping the rasterizer
          // here makes Blur and Gaussian Blur disappear in Draft/interactive views.
          if (buildRasterizedSurfaceBuffer(
                  layer, surface, &processed, interactiveDraft,
                  effectResolutionScale, deferRasterizerEffectsToGpu)) {

            processedBuffer =
                ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(processed);
            directProcessedBuffer = processedBuffer;

          }

        }

        if (processedBuffer && !resolvedMatteSources.isEmpty()) {
          applyResolvedMattesF32(*processedBuffer);
          directProcessedBuffer = processedBuffer;
        } else {
          applyResolvedMattes(surface);
        }



        LayerSurfaceCacheEntry entry;

        entry.ownerId = ownerId;

        entry.cacheSignature = cacheSignature;

        entry.processedBuffer = processedBuffer;

        entry.processedSurface = processedBuffer ? QImage{} : surface;

        entry.frameNumber = cacheFrameNumber;

        if (gpuTextureCacheManager &&

            layerUsesGpuTextureCacheForCompositionView(layer)) {

          if (entry.processedBuffer) {

            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(

                gpuOwnerId, gpuCacheSignature, *entry.processedBuffer);

          } else {

            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(

                gpuOwnerId, gpuCacheSignature, surface);

          }

        }

        (*surfaceCache)[cacheSlotKey] = entry;
        if (surfaceCacheDiagnosticsEnabled && *surfaceCacheDiagnosticsEnabled &&
            surfaceCacheBuildCounts) {
          ++(*surfaceCacheBuildCounts)[cacheSlotKey];
        }

        cacheEntry = &(*surfaceCache)[cacheSlotKey];
        directProcessedBuffer = cacheEntry->processedBuffer;

      }

    } else if (allowSurfaceCache) {

      ArtifactCore::ImageF32x4_RGBA processed;

      if (buildRasterizedSurfaceBuffer(
              layer, surface, &processed, interactiveDraft,
              effectResolutionScale, deferRasterizerEffectsToGpu) &&

          !processed.isEmpty()) {
        directProcessedBuffer =
            ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>(processed);

      }

      if (directProcessedBuffer && !resolvedMatteSources.isEmpty()) {
        applyResolvedMattesF32(*directProcessedBuffer);
      } else if (!directProcessedBuffer) {
        applyResolvedMattes(surface);
      }

    }



    const float baseOpacity =

        (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

    drawWithClonerEffect(

        layer, globalTransform4x4,

        [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {

          const float finalOpacity = baseOpacity * instanceWeight;



          if (gpuTextureCacheManager && cacheEntry &&

              layerUsesGpuTextureCacheForCompositionView(layer)) {

            if (!gpuTextureCacheManager->isValid(

                    cacheEntry->gpuTextureHandle)) {

              const QImage &uploadSurface =

                  cacheEntry->processedSurface.isNull()

                      ? surface

                      : cacheEntry->processedSurface;

              if (cacheEntry->processedBuffer) {

                cacheEntry->gpuTextureHandle =

                    gpuTextureCacheManager->acquireOrCreate(

                        gpuOwnerId, gpuCacheSignature,

                        *cacheEntry->processedBuffer);

              } else {

                cacheEntry->gpuTextureHandle =

                    gpuTextureCacheManager->acquireOrCreate(

                        gpuOwnerId, gpuCacheSignature, uploadSurface);

              }

            }

            const auto binding = gpuTextureCacheManager->bindingRecord(

                cacheEntry->gpuTextureHandle);

            if (binding.isValid()) {

              renderer->drawSpriteTransformed(

                  static_cast<float>(outputRect.x()), static_cast<float>(outputRect.y()),

                  static_cast<float>(outputRect.width()),

                  static_cast<float>(outputRect.height()), instanceTransform, binding.srv,

                  finalOpacity);

              return;

            }

            directProcessedBuffer = cacheEntry->processedBuffer;

          }



          if (directProcessedBuffer && !directProcessedBuffer->isEmpty()) {
            renderer->drawSpriteTransformed(
                static_cast<float>(outputRect.x()), static_cast<float>(outputRect.y()),
                static_cast<float>(outputRect.width()),
                static_cast<float>(outputRect.height()), instanceTransform,
                *directProcessedBuffer, finalOpacity);
          } else {
            renderer->drawSpriteTransformed(
                static_cast<float>(outputRect.x()), static_cast<float>(outputRect.y()),
                static_cast<float>(outputRect.width()),
                static_cast<float>(outputRect.height()), instanceTransform, surface,
                finalOpacity);
          }

        });

    return true;

  };



  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {

    const auto color = solid2D->color();

    if (layerHasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(

          std::max(1, static_cast<int>(std::ceil(localRect.width()))),

          std::max(1, static_cast<int>(std::ceil(localRect.height()))));

      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
        surface = ArtifactSolidGradientUtil::makeSolidGradientImage(
            surfaceSize,
            QColor::fromRgbF(solid2D->gradientStartColor().r(), solid2D->gradientStartColor().g(),
                             solid2D->gradientStartColor().b(), solid2D->gradientStartColor().a()),
            QColor::fromRgbF(solid2D->gradientEndColor().r(), solid2D->gradientEndColor().g(),
                             solid2D->gradientEndColor().b(), solid2D->gradientEndColor().a()),
            static_cast<int>(solid2D->fillType()), solid2D->gradientAngleDegrees(),
            solid2D->gradientReverse(), solid2D->gradientCenterX(), solid2D->gradientCenterY(),
            solid2D->gradientScale(), solid2D->gradientOffset());
      } else {
        surface.fill(toQColor(color));
      }

      applySurfaceAndDraw(surface, localRect, true);

    } else {

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      if (solid2D->fillType() != ArtifactSolidFillType::Solid) {
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawGradientRectTransformed(
                  static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
                  instanceTransform, solid2D->gradientStartColor(), solid2D->gradientEndColor(),
                  static_cast<int>(solid2D->fillType()), solid2D->gradientAngleDegrees(),
                  solid2D->gradientReverse(), solid2D->gradientCenterX(), solid2D->gradientCenterY(),
                  solid2D->gradientScale(), solid2D->gradientOffset(), baseOpacity * instanceWeight);
            });
        return;
      }

      drawWithClonerEffect(

          layer, globalTransform4x4,

          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {

            renderer->drawSolidRectTransformed(

                static_cast<float>(localRect.x()),

                static_cast<float>(localRect.y()),

                static_cast<float>(localRect.width()),

                static_cast<float>(localRect.height()), instanceTransform,

                color, baseOpacity * instanceWeight);

          });

    }

    return;

  }



  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {

    const auto color = solidImage->color();

    if (layerHasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(

          std::max(1, static_cast<int>(std::ceil(localRect.width()))),

          std::max(1, static_cast<int>(std::ceil(localRect.height()))));

      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
        surface = solidImage->toQImage();
      } else {
        surface.fill(toQColor(color));
      }

      applySurfaceAndDraw(surface, localRect, true);

    } else {

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      if (solidImage->fillType() != ArtifactSolidFillType::Solid) {
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
              renderer->drawGradientRectTransformed(
                  static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
                  instanceTransform, solidImage->gradientStartColor(), solidImage->gradientEndColor(),
                  static_cast<int>(solidImage->fillType()), solidImage->gradientAngleDegrees(),
                  solidImage->gradientReverse(), solidImage->gradientCenterX(), solidImage->gradientCenterY(),
                  solidImage->gradientScale(), solidImage->gradientOffset(), baseOpacity * instanceWeight);
            });
        return;
      }

      drawWithClonerEffect(

          layer, globalTransform4x4,

          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {

            renderer->drawSolidRectTransformed(

                static_cast<float>(localRect.x()),

                static_cast<float>(localRect.y()),

                static_cast<float>(localRect.width()),

                static_cast<float>(localRect.height()), instanceTransform,

                color, baseOpacity * instanceWeight);

          });

    }

    return;

  }



  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {

    // A supported rasterizer stack is applied after this layer has been drawn
    // into the reusable GPU layer target. Keep the non-destructive deformer in
    // that path too; CPU-only effects and masks still use the surface fallback.
    const bool canDeformOnGpu = !layer->hasMasks() &&
        (!layerHasRasterizerEffectsOrMasks(layer) ||
         deferRasterizerEffectsToGpu);
    if (canDeformOnGpu &&
        imageLayer->hasCurrentFrameBuffer()) {

      const ArtifactCore::ImageF32x4_RGBA &buffer =

          imageLayer->currentFrameBuffer();

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      auto* application = ArtifactApplicationManager::instance();
      auto* puppetTool = application ? application->puppetTool() : nullptr;
      imageLayer->refreshAnimatedSourceCrop();
      const SourceCropDrawLayout cropLayout =
          imageLayer->sourceCropDrawLayout();
      const QRect sourceRect = cropLayout.sourcePixelRect.intersected(
          QRect(0, 0, buffer.width(), buffer.height()));
      const QRectF drawRect = cropLayout.outputLocalRect.isValid() &&
              cropLayout.outputLocalRect.width() > 0.0 &&
              cropLayout.outputLocalRect.height() > 0.0
          ? cropLayout.outputLocalRect : localRect;
      const QRectF uvRect(
          static_cast<qreal>(sourceRect.x()) / buffer.width(),
          static_cast<qreal>(sourceRect.y()) / buffer.height(),
          static_cast<qreal>(sourceRect.width()) / buffer.width(),
          static_cast<qreal>(sourceRect.height()) / buffer.height());
      const bool hasStableSourceIdentity =
          !imageLayer->hasTemporarySourceOverride();
      Diligent::ITextureView* sourceTexture = renderer->textureForImage(
          buffer,
          hasStableSourceIdentity ? imageLayer->sourceAssetId() : QUuid{},
          hasStableSourceIdentity ? imageLayer->sourceVersion() : 0,
          hasStableSourceIdentity && imageLayer->isImageSequence()
              ? imageLayer->sequenceCachedFrameContentKey() : 0);

      static const QString clonerTestDiagnosticPath = [] {
        const QString outputPath = qEnvironmentVariable(
            "ARTIFACT_UI_TEST_CLONER_RENDER_OUTPUT");
        return outputPath.isEmpty()
            ? QString{}
            : outputPath + QStringLiteral(".route.txt");
      }();
      static bool clonerTestDiagnosticWritten = false;
      bool gpuGridHandled = false;
      int gpuGridDrawCount = 0;
      int gpuGridFallbackCount = 0;
      QStringList clonerWeightTrace;
      if (sourceTexture && !imageLayer->sourceCropEnabled() &&
          cropLayout.localTransform.isIdentity()) {
        struct ClonerSpriteDrawContext {
          ArtifactIRenderer* renderer = nullptr;
          ArtifactImageLayer* imageLayer = nullptr;
          ArtifactPuppetTool* puppetTool = nullptr;
          Diligent::ITextureView* texture = nullptr;
          QRectF rect;
          QRectF uvRect;
          QMatrix4x4 baseTransform;
          float opacity = 1.0f;
          int* gpuGridDrawCount = nullptr;
          int* gpuGridFallbackCount = nullptr;
          QStringList* weightTrace = nullptr;
        };
        ClonerSpriteDrawContext drawContext{
            renderer, imageLayer, puppetTool, sourceTexture, drawRect, uvRect,
            globalTransform4x4 * cropLayout.localTransform, baseOpacity,
            &gpuGridDrawCount, &gpuGridFallbackCount, &clonerWeightTrace};
        const auto drawClonerSprite = [](
            void* opaqueContext, const QMatrix4x4& instanceTransform,
            float instanceWeight,
            const ClonerGridPlacementRange& placementRange,
            std::uint32_t placementIndex) {
          auto* context = static_cast<ClonerSpriteDrawContext*>(opaqueContext);
          if (!context || !context->renderer || !context->imageLayer ||
              !context->texture) {
            return;
          }
          const auto& rect = context->rect;
          if (context->weightTrace && context->weightTrace->size() < 32) {
            context->weightTrace->append(QString::number(instanceWeight, 'f', 3));
          }
          if (context->puppetTool &&
              context->puppetTool->renderDeformedLayer(
                  context->renderer, context->imageLayer, instanceTransform,
                  context->opacity * instanceWeight)) {
            return;
          }
          if (placementRange.buffer &&
              context->renderer->drawSpriteTransformedClonerGrid(
                  static_cast<float>(rect.x()), static_cast<float>(rect.y()),
                  static_cast<float>(rect.width()),
                  static_cast<float>(rect.height()), context->baseTransform,
                  instanceTransform, context->texture,
                  context->opacity * instanceWeight, placementRange,
                  placementIndex)) {
            if (context->gpuGridDrawCount) {
              ++*context->gpuGridDrawCount;
            }
            return;
          }
          if (context->gpuGridFallbackCount) {
            ++*context->gpuGridFallbackCount;
          }
          context->renderer->drawSpriteTransformed(
              static_cast<float>(rect.x()), static_cast<float>(rect.y()),
              static_cast<float>(rect.width()),
              static_cast<float>(rect.height()), instanceTransform,
              context->texture, context->opacity * instanceWeight,
              context->uvRect);
        };
        gpuGridHandled = drawGpuClonerGridInstances(
            layer, renderer, globalTransform4x4 * cropLayout.localTransform,
            drawClonerSprite, &drawContext);
      }
      if (!clonerTestDiagnosticPath.isEmpty() && !clonerWeightTrace.isEmpty()) {
        QSaveFile weightFile(clonerTestDiagnosticPath +
                             QStringLiteral(".weights.txt"));
        if (weightFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
          weightFile.write(clonerWeightTrace.join(QLatin1Char(',' )).toUtf8());
          weightFile.write("\n");
          weightFile.commit();
        }
      }
      if (gpuGridHandled) {
        if (!clonerTestDiagnosticWritten &&
            !clonerTestDiagnosticPath.isEmpty()) {
          clonerTestDiagnosticWritten = true;
          QSaveFile diagnostic(clonerTestDiagnosticPath);
          if (diagnostic.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QString report =
                QStringLiteral("texture=%1 gpuReady=%2 placementHandled=%3 "
                               "gpuDraws=%4 cpuFallbackDraws=%5 crop=%6 "
                               "puppet=%7\n")
                    .arg(sourceTexture ? 1 : 0)
                    .arg(renderer->clonerGridPlacementGpuReady() ? 1 : 0)
                    .arg(gpuGridHandled ? 1 : 0)
                    .arg(gpuGridDrawCount)
                    .arg(gpuGridFallbackCount)
                    .arg(imageLayer->sourceCropEnabled() ? 1 : 0)
                    .arg(puppetTool ? 1 : 0);
            diagnostic.write(report.toUtf8());
            diagnostic.commit();
          }
        }
        return;
      }

      drawWithClonerEffect(

          layer, globalTransform4x4,

          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            ++gpuGridFallbackCount;
            if (!clonerTestDiagnosticPath.isEmpty() &&
                clonerWeightTrace.size() < 32) {
              clonerWeightTrace.append(QString::number(instanceWeight, 'f', 3));
            }
            if (puppetTool && puppetTool->renderDeformedLayer(
                    renderer, imageLayer, instanceTransform,
                    baseOpacity * instanceWeight)) {
              return;
            }

            const QMatrix4x4 cropTransform =
                instanceTransform * cropLayout.localTransform;
            if (sourceTexture) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(drawRect.x()),
                  static_cast<float>(drawRect.y()),
                  static_cast<float>(drawRect.width()),
                  static_cast<float>(drawRect.height()), cropTransform,
                  sourceTexture, baseOpacity * instanceWeight, uvRect);
              return;
            }
            renderer->drawSpriteTransformed(
                static_cast<float>(drawRect.x()),
                static_cast<float>(drawRect.y()),
                static_cast<float>(drawRect.width()),
                static_cast<float>(drawRect.height()),
                cropTransform, buffer, baseOpacity * instanceWeight, uvRect);

          });
      if (!clonerTestDiagnosticPath.isEmpty() && !clonerWeightTrace.isEmpty()) {
        QSaveFile weightFile(clonerTestDiagnosticPath +
                             QStringLiteral(".weights.txt"));
        if (weightFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
          weightFile.write(clonerWeightTrace.join(QLatin1Char(',')).toUtf8());
          weightFile.write("\n");
          weightFile.commit();
        }
      }
      if (!clonerTestDiagnosticWritten &&
          !clonerTestDiagnosticPath.isEmpty()) {
        clonerTestDiagnosticWritten = true;
        QSaveFile diagnostic(clonerTestDiagnosticPath);
        if (diagnostic.open(QIODevice::WriteOnly | QIODevice::Text)) {
          const QString report =
              QStringLiteral("texture=%1 gpuReady=%2 placementHandled=%3 "
                             "gpuDraws=%4 cpuFallbackDraws=%5 crop=%6 "
                             "puppet=%7\n")
                  .arg(sourceTexture ? 1 : 0)
                  .arg(renderer->clonerGridPlacementGpuReady() ? 1 : 0)
                  .arg(gpuGridHandled ? 1 : 0)
                  .arg(gpuGridDrawCount)
                  .arg(gpuGridFallbackCount)
                  .arg(imageLayer->sourceCropEnabled() ? 1 : 0)
                  .arg(puppetTool ? 1 : 0);
          diagnostic.write(report.toUtf8());
          diagnostic.commit();
        }
      }

      return;

    }



    // Rasterizer/mask processing consumes the cropped source surface. Resolve
    // crop keyframes before toQImage() so this path matches the direct draw
    // path and its cache identity below.
    imageLayer->refreshAnimatedSourceCrop();
    const QImage img = imageLayer->toQImage();

    if (!img.isNull()) {

      applySurfaceAndDraw(img, localRect, layerHasRasterizerEffectsOrMasks(layer));
      return;

    }

  }

  if (auto *shapeLayer = dynamic_cast<ArtifactShapeLayer *>(layer)) {
    auto* application = ArtifactApplicationManager::instance();
    auto* puppetTool = application ? application->puppetTool() : nullptr;
    const auto prepareDeformer = [](void* context,
                                    ArtifactAbstractLayer* target) {
      return static_cast<ArtifactPuppetTool*>(context)->prepareLayerDeformation(target);
    };
    const auto mapDeformerPoint = [](void* context,
                                     ArtifactAbstractLayer* target,
                                     const QPointF& point) {
      return static_cast<ArtifactPuppetTool*>(context)->mapDeformationPoint(
          target, point);
    };
    // The composition GPU path already renders this layer into the reusable
    // per-layer RTV. When the complete rasterizer stack has a GPU node
    // representation, keep the vector draw on that target and let
    // prepareGpuLayerForBlend() run the pointwise or spatial passes. Falling through
    // to toQImage() here would rasterize with QPainter, convert on CPU, and
    // upload the same frame again.
    if (deferRasterizerEffectsToGpu && !layer->hasMasks()) {
      shapeLayer->draw(renderer, puppetTool, mapDeformerPoint,
                       prepareDeformer);
      return;
    }
    if (layerHasRasterizerEffectsOrMasks(layer)) {
      const QImage shapeImg = shapeLayer->toQImage();
      if (!shapeImg.isNull()) {
        applySurfaceAndDraw(shapeImg, localRect, true);
        return;
      }
    } else {
      shapeLayer->draw(renderer, puppetTool, mapDeformerPoint,
                       prepareDeformer);
      return;
    }
  }



  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {

    if (svgLayer->isLoaded()) {

      if (!layerHasRasterizerEffectsOrMasks(layer) &&
          svgLayer->hasCurrentFrameBuffer()) {

        const ArtifactCore::ImageF32x4_RGBA &buffer =

            svgLayer->currentFrameBuffer();

        Diligent::ITextureView* sourceTexture = renderer->textureForImage(
            buffer, QUuid(layer->id().toString()), svgLayer->sourceVersion(), 0);

        const float baseOpacity =

            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

        drawWithClonerEffect(

            layer, globalTransform4x4,

            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {

              if (sourceTexture) {
                renderer->drawSpriteTransformed(
                    static_cast<float>(localRect.x()),
                    static_cast<float>(localRect.y()),
                    static_cast<float>(localRect.width()),
                    static_cast<float>(localRect.height()), instanceTransform,
                    sourceTexture, baseOpacity * instanceWeight);
                return;
              }
              renderer->drawSpriteTransformed(

                  static_cast<float>(localRect.x()),

                  static_cast<float>(localRect.y()),

                  static_cast<float>(localRect.width()),

                  static_cast<float>(localRect.height()), instanceTransform,

                  buffer, baseOpacity * instanceWeight);

            });

      } else {

        const QImage svgImage = svgLayer->toQImage();

        if (!svgImage.isNull()) {

          applySurfaceAndDraw(svgImage, localRect,

                              layerHasRasterizerEffectsOrMasks(layer));
        } else {

          svgLayer->draw(renderer);

        }

      }

      return;

    }

  }



  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {

    static const bool profileVideoLayerRendering = [] {
      return qEnvironmentVariableIsSet(
                 "ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD") &&
             qEnvironmentVariable("ARTIFACT_VIDEO_PREVIEW_PROFILE_UPLOAD") !=
                 QStringLiteral("0");
    }();
    static int videoLayerProfileSamples = 0;
    const bool profileThisVideoLayer =
        profileVideoLayerRendering && videoLayerProfileSamples++ < 24;

    const bool hasRasterizer = layerHasRasterizerEffectsOrMasks(layer);
    const bool hasBuffer = videoLayer->hasCurrentFrameBuffer();

    const bool currentFrameReady =

        videoLayer->isFrameUploadCached(layer->currentFrame());

    const bool loaded = videoLayer->isLoaded();

    const bool active =

        layer->isActiveAt(FramePosition(static_cast<int>(layer->currentFrame())));

    if (profileThisVideoLayer) {
      std::fprintf(stderr,
                   "[VideoPreviewLayerProfile] enter frame=%lld ready=%d "
                   "rasterizer=%d hasBuffer=%d loaded=%d\n",
                   static_cast<long long>(layer->currentFrame()),
                   currentFrameReady ? 1 : 0, hasRasterizer ? 1 : 0,
                   hasBuffer ? 1 : 0, loaded ? 1 : 0);
      std::fflush(stderr);
      qWarning().noquote()
          << "[VideoPreviewLayerProfile] enter frame=" << layer->currentFrame()
          << "frameReady=" << currentFrameReady << "rasterizer="
          << hasRasterizer << "hasBuffer=" << hasBuffer << "loaded=" << loaded;
    }

    const FramePosition ip = layer->inPoint();

    const FramePosition op = layer->outPoint();

    const qint64 directGpuFrameNumber =
        cacheFrameNumber != std::numeric_limits<int64_t>::min()
            ? cacheFrameNumber : layer->currentFrame();
    void* sharedD3D12Device =
        (!hasRasterizer && gpuTextureCacheManager)
            ? gpuTextureCacheManager->nativeD3D12Device() : nullptr;
    const bool useDirectD3D12Video =
        sharedD3D12Device &&
        videoLayer->streamInfo().codecName.compare(
            QStringLiteral("h264"), Qt::CaseInsensitive) == 0;
    if (useDirectD3D12Video) {
      QString gpuOwnerId = layer->id().toString();
      const QString gpuCacheSignature = QStringLiteral("video-gpu:v%1:f%2")
                                            .arg(videoLayer->sourceVersion())
                                            .arg(directGpuFrameNumber);
      GPUTextureCacheHandle gpuHandle = gpuTextureCacheManager->findExisting(
          gpuOwnerId, gpuCacheSignature, Diligent::TEX_FORMAT_RGBA16_FLOAT);
      GPUTextureBindingRecord gpuBinding =
          gpuTextureCacheManager->bindingRecord(gpuHandle);
      if (!gpuBinding.isValid()) {
        const ArtifactCore::GpuVideoFrame gpuFrame =
            videoLayer->decodeFrameToGpuFrame(directGpuFrameNumber,
                                               sharedD3D12Device);
        if (gpuFrame.isValid()) {
          gpuHandle = gpuTextureCacheManager->acquireOrCreate(
              gpuOwnerId, gpuCacheSignature, gpuFrame);
          gpuBinding = gpuTextureCacheManager->bindingRecord(gpuHandle);
        }
      }
      if (gpuBinding.isValid()) {
        if (profileThisVideoLayer) {
          qWarning().noquote()
              << "[VideoPreviewLayerProfile] branch=d3d12-gpu frame="
              << directGpuFrameNumber;
        }
        if (videoDebugOut) {
          *videoDebugOut = QStringLiteral(
              "[Video] branch=d3d12-gpu frame=%1 size=%2x%3")
                               .arg(directGpuFrameNumber)
                               .arg(videoLayer->streamInfo().width)
                               .arg(videoLayer->streamInfo().height);
        }
        videoLayer->markFrameRenderQueued(directGpuFrameNumber);
        const float baseOpacity =
            opacityOverride >= 0.0f ? opacityOverride : layer->opacity();
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(localRect.x()),
                  static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()),
                  static_cast<float>(localRect.height()), instanceTransform,
                  gpuBinding.srv, baseOpacity * instanceWeight);
            });
        return;
      }
    }

    if (!hasRasterizer && currentFrameReady) {

      QElapsedTimer videoBufferTimer;
      if (profileThisVideoLayer) videoBufferTimer.start();

      const auto sharedBuffer =
          videoLayer->cachedFrameImageBufferShared(layer->currentFrame());
      const auto preparedUpload =
          videoLayer->cachedFrameUploadBufferShared(layer->currentFrame());
      ArtifactCore::ImageF32x4_RGBA bufferFallback;
      if (!sharedBuffer) {
        bufferFallback =
            videoLayer->cachedFrameImageBuffer(layer->currentFrame());
      }
      const ArtifactCore::ImageF32x4_RGBA& buffer =
          sharedBuffer ? *sharedBuffer : bufferFallback;

      const qint64 videoBufferReadMs = profileThisVideoLayer
          ? videoBufferTimer.elapsed()
          : 0;

      if (buffer.isEmpty()) {

        qCDebug(compositionViewLog)

            << "[VideoLayerT] cached frame marker without frame payload"

            << videoLayer->decodeState()

            << "timelineFrame=" << layer->currentFrame();

      } else {

        const float baseOpacity =

            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

        if (videoDebugOut) {

          *videoDebugOut = QStringLiteral(

                               "[Video] branch=buffer loaded=%1 hasBuffer=%2 "

                               "rasterizer=%3 active=%4 frameReady=%5 "

                               "range=[%6,%7] curFrame=%8")

                               .arg(loaded)

                               .arg(hasBuffer)

                               .arg(hasRasterizer)

                               .arg(active)

                               .arg(currentFrameReady)

                               .arg(ip.framePosition())

                               .arg(op.framePosition())

                               .arg(layer->currentFrame());

          *videoDebugOut += QStringLiteral(" ") + videoLayer->decodeState();

        }

        videoLayer->markFrameRenderQueued(layer->currentFrame());
        QElapsedTimer videoDrawTimer;
        if (profileThisVideoLayer) videoDrawTimer.start();
        // Resolve/upload before the Cloner producer queues its dispatch packet.
        // An overwrite barrier may submit the owner queue and reset dispatch
        // packet indices; doing it here keeps the current dispatch and sprites
        // together in one ordered batch.
        Diligent::RefCntAutoPtr<Diligent::ITextureView> videoTexture(
            renderer->textureForVideoFrame(
                buffer, QUuid(layer->id().toString()),
                static_cast<qint64>(layer->currentFrame()),
                preparedUpload.get()));
        if (!videoTexture) {
            videoTexture = renderer->textureForImage(buffer);
        }
        drawWithClonerEffect(

            layer, globalTransform4x4,

            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
                renderer->drawSpriteTransformed(
                    static_cast<float>(localRect.x()),
                    static_cast<float>(localRect.y()),
                    static_cast<float>(localRect.width()),
                    static_cast<float>(localRect.height()), instanceTransform,
                    videoTexture.RawPtr(), baseOpacity * instanceWeight);

            });

        if (profileThisVideoLayer) {
          std::fprintf(
              stderr,
              "[VideoPreviewLayerProfile] branch=cached frame=%lld "
              "cacheReadMs=%lld drawSubmitMs=%lld size=%dx%d\n",
              static_cast<long long>(layer->currentFrame()),
              static_cast<long long>(videoBufferReadMs),
              static_cast<long long>(videoDrawTimer.elapsed()), buffer.width(),
              buffer.height());
          std::fflush(stderr);
          qWarning().noquote()
              << "[VideoPreviewLayerProfile] branch=cached frame="
              << layer->currentFrame() << "cacheReadMs=" << videoBufferReadMs
              << "drawSubmitMs=" << videoDrawTimer.elapsed()
              << "size=" << buffer.width() << 'x' << buffer.height();
        }

        return;

      }

    }



    ArtifactCore::ImageF32x4_RGBA frameBuffer;
    ArtifactCore::SharedPtr<const ArtifactCore::ImageF32x4_RGBA>
        cachedFallbackFrame;
    ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer>
        cachedFallbackUpload;
    qint64 fallbackFrameContentKey = layer->currentFrame();
    bool hasFallbackFrame = false;
    const auto isCurrentVideoSize = [videoLayer](
        const ArtifactCore::ImageF32x4_RGBA& candidate) {
      const auto& stream = videoLayer->streamInfo();
      return !candidate.isEmpty() &&
             (stream.width <= 0 || stream.height <= 0 ||
              (candidate.width() == stream.width &&
               candidate.height() == stream.height));
    };

    QElapsedTimer videoFallbackTimer;
    if (profileThisVideoLayer) videoFallbackTimer.start();

    QImage frame;

    bool usedSyncFallback = false;

    bool usedBufferFallback = false;

    QString reason;

    if (loaded) {

      cachedFallbackFrame =
          videoLayer->cachedFrameImageBufferShared(layer->currentFrame());

      if (cachedFallbackFrame &&
          !isCurrentVideoSize(*cachedFallbackFrame)) {
        cachedFallbackFrame.reset();
      }

      hasFallbackFrame = static_cast<bool>(cachedFallbackFrame);

      usedBufferFallback = hasFallbackFrame;

      if (!currentFrameReady) {
        for (int offset = 1; offset <= 4; ++offset) {
          const qint64 candidateFrame =
              static_cast<qint64>(layer->currentFrame()) - offset;
          if (candidateFrame < 0) {
            break;
          }
          auto candidateImage =
              videoLayer->cachedFrameImageBufferShared(candidateFrame);
          auto candidateUpload =
              videoLayer->cachedFrameUploadBufferShared(candidateFrame);
          if (!candidateImage || !candidateUpload ||
              !isCurrentVideoSize(*candidateImage) ||
              candidateUpload->width != candidateImage->width() ||
              candidateUpload->height != candidateImage->height()) {
            continue;
          }
          cachedFallbackFrame = std::move(candidateImage);
          cachedFallbackUpload = std::move(candidateUpload);
          fallbackFrameContentKey = candidateFrame;
          hasFallbackFrame = true;
          usedBufferFallback = true;
          break;
        }
      }

    } else {

      reason = QStringLiteral("notLoaded");

    }

    if (!hasFallbackFrame && loaded && hasBuffer) {

      // Rendering must never wait for a decoder. This starts or observes the

      // asynchronous request and retains the last good frame until the exact

      // frame reaches the RAM cache.

      frameBuffer = videoLayer->currentFrameImageBuffer();

      hasFallbackFrame = isCurrentVideoSize(frameBuffer);

      usedBufferFallback = hasFallbackFrame;

    }

    const ArtifactCore::ImageF32x4_RGBA& fallbackFrame =
        cachedFallbackFrame ? *cachedFallbackFrame : frameBuffer;

    const qint64 videoFallbackReadMs = profileThisVideoLayer
        ? videoFallbackTimer.elapsed()
        : 0;

    if (videoDebugOut) {

      if (reason.isEmpty()) {

        if (!loaded) {

          reason = QStringLiteral("notLoaded");

        } else if (usedSyncFallback) {

          reason = QStringLiteral("syncDecode");

        } else if (usedBufferFallback) {

          reason = currentFrameReady ? QStringLiteral("bufferFallback")

                                     : QStringLiteral("repeatLastGood");

        } else if (!hasFallbackFrame) {

          reason = hasBuffer ? QStringLiteral("decodeNull")

                             : QStringLiteral("noBuffer");

        } else {

          reason = QStringLiteral("ok");

        }

      }

      *videoDebugOut = QStringLiteral(

                          "[Video] branch=decode loaded=%1 hasBuffer=%2 "

                          "rasterizer=%3 active=%4 frameReady=%5 "

                          "syncFallback=%6 bufferFallback=%7 reason=%8 "

                          "range=[%9,%10] curFrame=%11")

                          .arg(loaded)

                          .arg(hasBuffer)

                          .arg(hasRasterizer)

                          .arg(active)

                          .arg(currentFrameReady)

                          .arg(usedSyncFallback)

                          .arg(usedBufferFallback)

                          .arg(reason)

                          .arg(ip.framePosition())

                          .arg(op.framePosition())

                          .arg(layer->currentFrame());

      *videoDebugOut += QStringLiteral(" ") + videoLayer->decodeState();

    }

    if (hasFallbackFrame && !hasRasterizer &&

        !layerHasEnabledMatteReferences(layer)) {

      const float baseOpacity =

          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());

      videoLayer->markFrameRenderQueued(

          layer->currentFrame(), !currentFrameReady);

      Diligent::ITextureView* fallbackVideoTexture = nullptr;
      if (cachedFallbackUpload) {
        fallbackVideoTexture = renderer->textureForVideoFrame(
            fallbackFrame, QUuid(layer->id().toString()),
            fallbackFrameContentKey, cachedFallbackUpload.get());
      }

      QElapsedTimer videoFallbackDrawTimer;
      if (profileThisVideoLayer) videoFallbackDrawTimer.start();

      drawWithClonerEffect(

          layer, globalTransform4x4,

          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {

            if (fallbackVideoTexture) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(localRect.x()),
                  static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()),
                  static_cast<float>(localRect.height()), instanceTransform,
                  fallbackVideoTexture, baseOpacity * instanceWeight);
            } else {
              renderer->drawSpriteTransformed(
                  static_cast<float>(localRect.x()),
                  static_cast<float>(localRect.y()),
                  static_cast<float>(localRect.width()),
                  static_cast<float>(localRect.height()), instanceTransform,
                  fallbackFrame, baseOpacity * instanceWeight);
            }

          });

      if (profileThisVideoLayer) {
        std::fprintf(
            stderr,
            "[VideoPreviewLayerProfile] branch=fallback frame=%lld ready=%d "
            "rasterizer=%d cacheReadMs=%lld drawSubmitMs=%lld size=%dx%d\n",
            static_cast<long long>(layer->currentFrame()),
            currentFrameReady ? 1 : 0, hasRasterizer ? 1 : 0,
            static_cast<long long>(videoFallbackReadMs),
            static_cast<long long>(videoFallbackDrawTimer.elapsed()),
            fallbackFrame.width(), fallbackFrame.height());
        std::fflush(stderr);
        qWarning().noquote()
            << "[VideoPreviewLayerProfile] branch=fallback frame="
            << layer->currentFrame() << "frameReady=" << currentFrameReady
            << "rasterizer=" << hasRasterizer
            << "cacheReadMs=" << videoFallbackReadMs
            << "drawSubmitMs=" << videoFallbackDrawTimer.elapsed()
            << "size=" << fallbackFrame.width() << 'x' << fallbackFrame.height();
      }

      return;

    }

    if (hasFallbackFrame) {

      frame = fallbackFrame.toQImage();

    }

    if (!frame.isNull()) {

      videoLayer->markFrameRenderQueued(

          layer->currentFrame(), !currentFrameReady);

      applySurfaceAndDraw(frame, localRect, true);

      return;

    }



    qCDebug(compositionViewLog)

        << "[VideoLayerT] drawLayerForCompositionView: no drawable frame"

        << videoLayer->decodeState()

        << "timelineFrame=" << cacheFrameNumber

        << "usedSyncFallback=" << usedSyncFallback

        << "hasCurrentFrameBuffer=" << videoLayer->hasCurrentFrameBuffer();

  }



  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    // Prefer the layer's glyph-atlas path when the controller does not need to
    // flatten the layer for a raster-only operation. The former surface-first
    // branch called hasCurrentFrameBuffer() unconditionally, which forced
    // QPainter rasterization before ArtifactTextLayer::draw() could emit GPU
    // glyph packets.
    const bool canUseDirectGpuText =
        !layerHasRasterizerEffectsOrMasks(layer) && !layer->hasModifiers() &&
        !layerHasEnabledMatteReferences(layer) &&
        std::abs(opacityOverride - layer->opacity()) <= 0.0001f;
    if (canUseDirectGpuText) {
      textLayer->draw(renderer);
      return;
    }

    if (textLayer->hasCurrentFrameBuffer()) {
      const auto &sourceBuffer = textLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      const auto drawTextBuffer =
          [&](const ArtifactCore::ImageF32x4_RGBA &buffer,
              bool stableSource) {
            Diligent::ITextureView* sourceTexture = stableSource
                ? renderer->textureForImage(
                      buffer, QUuid(layer->id().toString()), textLayer->contentRevision(),
                      static_cast<qint64>(layer->currentFrame()))
                : nullptr;
            drawWithClonerEffect(
                layer, globalTransform4x4,
                [&](const QMatrix4x4 &instanceTransform,
                    float instanceWeight) {
                  if (sourceTexture) {
                    renderer->drawSpriteTransformed(
                        static_cast<float>(localRect.x()),
                        static_cast<float>(localRect.y()),
                        static_cast<float>(localRect.width()),
                        static_cast<float>(localRect.height()), instanceTransform,
                        sourceTexture, baseOpacity * instanceWeight);
                  } else {
                    renderer->drawSpriteTransformed(
                        static_cast<float>(localRect.x()),
                        static_cast<float>(localRect.y()),
                        static_cast<float>(localRect.width()),
                        static_cast<float>(localRect.height()), instanceTransform,
                        buffer, baseOpacity * instanceWeight);
                  }
                });
          };

      if (layerHasRasterizerEffectsOrMasks(layer)) {
        ArtifactCore::ImageF32x4_RGBA buffer = sourceBuffer.DeepCopy();
        applyRasterizerEffectsAndMasksToSurface(layer, buffer, lod);
        drawTextBuffer(buffer, false);
      } else {
        drawTextBuffer(sourceBuffer, true);
      }

      return;

    }

    const QImage textImage = textLayer->toQImage();

    if (!textImage.isNull()) {

      applySurfaceAndDraw(textImage, localRect, true);

    }

    return;

  }



  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer);
      compLayer || dynamic_cast<ArtifactParametricCompositionLayer *>(layer)) {

    auto *parametricLayer = dynamic_cast<ArtifactParametricCompositionLayer *>(layer);

    if (auto childComp = compLayer
            ? compLayer->sourceComposition()
            : parametricLayer->sourceComposition()) {

      const QSize childSize = childComp->settings().compositionSize();

      // Master Properties are instance overrides. Apply them only while this
      // precomp instance is sampled, then restore the shared child composition
      // so sibling instances cannot leak values into one another.
      const int64_t childFrame =
          cacheFrameNumber == std::numeric_limits<int64_t>::min()
              ? childComp->framePosition().framePosition()
              : static_cast<int64_t>(std::llround(
                    layer->getSourceFrameAtCompFrame(cacheFrameNumber)));
      if (layer->is3D() && precompGpuResolver) {
        if (auto* childSRV = (*precompGpuResolver)(layer, childFrame,
                                                   childSize)) {
          renderer->draw3DTexturedCard(localRect, layer->getGlobalTransform4x4(),
                                       childSRV,
                                       opacityOverride >= 0.0f
                                           ? opacityOverride
                                           : layer->opacity());
          return;
        }
      }
      const FramePosition childRestoreFrame = childComp->framePosition();
      if (childRestoreFrame.framePosition() != childFrame) {
        childComp->goToFrame(childFrame);
      }
      // Resolve authored animation/expression state first. The instance scope
      // is then the final value layer for this sample, including recursively
      // exposed Master Properties on nested precomp layers.
      const bool overrideScopeActive = compLayer
          ? compLayer->beginExposedPropertyOverrideScope()
          : parametricLayer->beginInputBindingScope();
      QImage childImage = childComp->getThumbnailAtFrame(
          childFrame, childSize.width(), childSize.height());
      if (overrideScopeActive) {
        if (compLayer) compLayer->endExposedPropertyOverrideScope();
        else parametricLayer->endInputBindingScope();
      }
      if (childRestoreFrame.framePosition() != childFrame) {
        childComp->goToFrame(childRestoreFrame.framePosition());
      }



      if (!childImage.isNull()) {

        applySurfaceAndDraw(childImage, localRect,

                            layerHasRasterizerEffectsOrMasks(layer));
      }

    }

    return;

  }



  if (auto* mpmSandLayer =
          dynamic_cast<ArtifactMpmSand2DLayer*>(layer)) {
    const int64_t targetFrame =
        (cacheFrameNumber != std::numeric_limits<int64_t>::min())
            ? cacheFrameNumber
            : layer->currentFrame();
    mpmSandLayer->goToFrame(targetFrame);

    const bool hasRasterizerWork =
        layerHasRasterizerEffectsOrMasks(layer);
    const bool canDeferRasterizerWorkToGpu =
        deferRasterizerEffectsToGpu && !layer->hasMasks();
    if (!hasRasterizerWork || canDeferRasterizerWorkToGpu) {
      // The composition GPU pipeline owns the intermediate target and applies
      // supported raster effects/mattes before blending. Keep the granular
      // density surface on that GPU path instead of reading it back to CPU.
      const float drawOpacity = opacityOverride >= 0.0f
          ? opacityOverride
          : layer->opacity();
      mpmSandLayer->draw(renderer, drawOpacity);
      return;
    }

    // Unsupported CPU effects and layer masks use the established software
    // surface boundary. Bound the transient image by the same limits as the
    // Render Queue path while preserving the particle-expanded local bounds.
    const QRectF renderBounds = mpmSandLayer->particleRenderBounds();
    if (!renderBounds.isValid() || renderBounds.isEmpty()) return;
    constexpr double kMaximumMpmSurfaceDimension = 4096.0;
    constexpr double kMaximumMpmSurfacePixels = 4.0 * 1024.0 * 1024.0;
    const double layerWidth = localRect.width();
    const double layerHeight = localRect.height();
    if (!std::isfinite(layerWidth) || !std::isfinite(layerHeight) ||
        layerWidth <= 0.0 || layerHeight <= 0.0) {
      return;
    }
    const double lodScale = lod == DetailLevel::Low
        ? 0.5
        : lod == DetailLevel::Medium ? 0.75 : 1.0;
    const double pixelScale = lodScale * std::min(
        {kMaximumMpmSurfaceDimension / layerWidth,
         kMaximumMpmSurfaceDimension / layerHeight,
         std::sqrt(kMaximumMpmSurfacePixels / layerWidth / layerHeight)});
    if (!std::isfinite(pixelScale) || pixelScale <= 0.0) return;
    const QSize surfaceSize(
        static_cast<int>(std::clamp(
            std::floor(layerWidth * pixelScale), 1.0,
            kMaximumMpmSurfaceDimension)),
        static_cast<int>(std::clamp(
            std::floor(layerHeight * pixelScale), 1.0,
            kMaximumMpmSurfaceDimension)));
    QRectF surfaceBounds;
    QImage surface = mpmSandLayer->getSoftwareRenderSurface(
        surfaceSize.width(), surfaceSize.height(), &surfaceBounds, true);
    if (!surface.isNull()) {
      applySurfaceAndDraw(surface, surfaceBounds, false);
    }
    return;
  }

  if (layer->isParticleLayer()) {

    layer->draw(renderer);

    return;

  }



  // Fallback for layer types without a direct surface accessor.

  qCDebug(compositionViewLog) << "[CompositionView] fallback layer draw"

                              << "id=" << layer->id().toString()

                              << "type=" << layer->type_index().name();

  layer->draw(renderer);

}



// Project3D-style camera projection helper: resolves the authored source
// layer's GPU texture SRV. Returns nullptr if no GPU texture is available.
static Diligent::ITextureView* resolveProjectorTexture(
    ArtifactAbstractLayer* sourceLayer,
    int64_t /*frame*/,
    GPUTextureCacheManager* cacheManager)
{
  if (!sourceLayer || !cacheManager) {
    return nullptr;
  }
  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(sourceLayer);
      imageLayer && imageLayer->canShareSourceGpuTexture()) {
    const QString textureKey = QStringLiteral("projection-src:%1")
        .arg(imageLayer->sourceVersion());
    const auto handle = cacheManager->acquireOrCreate(
        sourceLayer->id().toString(), textureKey,
        imageLayer->currentFrameBuffer());
    if (cacheManager->isValid(handle)) {
      return cacheManager->bindingRecord(handle).srv;
    }
  }
  return nullptr;
}

// Project3D-style camera projection helpers. Source layers override the
// camera via "projection.camera.view"/"proj" property blobs (64-byte
// float16 row-major); otherwise the caller falls back to the render camera.
static QMatrix4x4 resolveProjectorViewMatrix(ArtifactAbstractLayer* sourceLayer)
{
  if (sourceLayer) {
    const auto property = sourceLayer->getProperty(
        QStringLiteral("projection.camera.view"));
    const QByteArray blob = property ? property->getValue().toByteArray()
                                     : QByteArray{};
    if (!blob.isEmpty()) {
      QMatrix4x4 m;
      std::memcpy(m.data(), blob.constData(),
                  std::min<int>(sizeof(float) * 16, blob.size()));
      return m;
    }
  }
  return QMatrix4x4();
}

static QMatrix4x4 resolveProjectorProjMatrix(ArtifactAbstractLayer* sourceLayer)
{
  if (sourceLayer) {
    const auto property = sourceLayer->getProperty(
        QStringLiteral("projection.camera.proj"));
    const QByteArray blob = property ? property->getValue().toByteArray()
                                     : QByteArray{};
    if (!blob.isEmpty()) {
      QMatrix4x4 m;
      std::memcpy(m.data(), blob.constData(),
                  std::min<int>(sizeof(float) * 16, blob.size()));
      return m;
    }
  }
  return QMatrix4x4();
}

} // namespace Artifact
