module;
#include <Layer/ArtifactCloneEffectSupport.hpp>

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <opencv2/opencv.hpp>

export module Artifact.Render.CompositionViewDrawing;

import Artifact.Render.IRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Text;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Composition;
import Artifact.Effect.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Composition.Abstract;
import Image.ImageF32x4_RGBA;
import CvUtils;
import Color.Float;
import FloatRGBA;
import Frame.Position;

namespace Artifact {

export struct LayerSurfaceCacheEntry
{
  QString ownerId;
  QString cacheSignature;
  QImage processedSurface;
  GPUTextureCacheHandle gpuTextureHandle;
  int64_t frameNumber = std::numeric_limits<int64_t>::min();
};

export bool layerHasCpuRasterizerWork(const ArtifactAbstractLayerPtr& layer);
export bool layerUsesSurfaceUploadForCompositionView(const ArtifactAbstractLayerPtr& layer);
export bool layerUsesGpuTextureCacheForCompositionView(const ArtifactAbstractLayerPtr& layer);
export void drawLayerForCompositionView(const ArtifactAbstractLayerPtr &layer,
                                        ArtifactIRenderer *renderer,
                                        float opacityOverride = -1.0f,
                                        QString* videoDebugOut = nullptr,
                                        QHash<QString, LayerSurfaceCacheEntry>* surfaceCache = nullptr,
                                        GPUTextureCacheManager* gpuTextureCacheManager = nullptr,
                                        int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),
                                        bool offlineRender = false,
                                        DetailLevel lod = DetailLevel::High);

namespace {

QColor toQColor(const FloatColor& color)
{
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

QColor toQColor(const FloatRGBA& color)
{
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

QString rgbaKey(float r, float g, float b, float a)
{
  return QStringLiteral("%1,%2,%3,%4")
      .arg(r, 0, 'f', 4)
      .arg(g, 0, 'f', 4)
      .arg(b, 0, 'f', 4)
      .arg(a, 0, 'f', 4);
}

float lodScale(DetailLevel lod)
{
  switch (lod) {
  case DetailLevel::Low:
    return 0.5f;
  case DetailLevel::Medium:
    return 0.75f;
  case DetailLevel::High:
  default:
    return 1.0f;
  }
}

QImage downsampleForLOD(const QImage& image, DetailLevel lod)
{
  if (image.isNull() || lod == DetailLevel::High) {
    return image;
  }

  const float scale = lodScale(lod);
  const int targetW = std::max(1, static_cast<int>(std::round(image.width() * scale)));
  const int targetH = std::max(1, static_cast<int>(std::round(image.height() * scale)));
  if (targetW == image.width() && targetH == image.height()) {
    return image;
  }

  return image.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

QString buildLayerSurfaceCacheKey(const ArtifactAbstractLayerPtr& layer,
                                  const QImage& surface,
                                  int64_t frameNumber)
{
  if (!layer) {
    return QString();
  }

  QString key = layer->id().toString();
  key += QStringLiteral("|size=%1x%2")
             .arg(surface.width())
             .arg(surface.height());

  if (const auto solid2D = std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
    const QRectF bounds = solid2D->localBounds();
    key += QStringLiteral("|solid2D|color=%1|bounds=%2x%3")
               .arg(rgbaKey(solid2D->color().r(), solid2D->color().g(), solid2D->color().b(), solid2D->color().a()))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (const auto solidImage = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral("|solidImage|color=%1|bounds=%2x%3")
               .arg(rgbaKey(solidImage->color().r(), solidImage->color().g(), solidImage->color().b(), solidImage->color().a()))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (const auto imageLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    key += QStringLiteral("|image|src=%1|fit=%2|size=%3x%4")
               .arg(imageLayer->sourcePath())
               .arg(imageLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (const auto svgLayer = std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    key += QStringLiteral("|svg|src=%1|fit=%2|size=%3x%4")
               .arg(svgLayer->sourcePath())
               .arg(svgLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    key += QStringLiteral("|video|src=%1|frame=%2|proxy=%3|size=%4x%5")
               .arg(videoLayer->sourcePath())
               .arg(frameNumber)
               .arg(static_cast<int>(videoLayer->proxyQuality()))
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    key += QStringLiteral("|text|value=%1|font=%2|size=%3|bold=%4|italic=%5|allCaps=%6|underline=%7|strike=%8|fill=%9|strokeEnabled=%10|strokeColor=%11|strokeWidth=%12|shadowEnabled=%13|shadowColor=%14|shadowOffset=%15,%16|shadowBlur=%17|tracking=%18|leading=%19|wrap=%20|mw=%21|bh=%22|va=%23|ha=%24|ps=%25|surface=%26x%27")
               .arg(textLayer->text().toQString())
               .arg(textLayer->fontFamily().toQString())
               .arg(textLayer->fontSize(), 0, 'f', 2)
               .arg(textLayer->isBold() ? 1 : 0)
               .arg(textLayer->isItalic() ? 1 : 0)
               .arg(textLayer->isAllCaps() ? 1 : 0)
               .arg(textLayer->isUnderline() ? 1 : 0)
               .arg(textLayer->isStrikethrough() ? 1 : 0)
               .arg(rgbaKey(textLayer->textColor().r(), textLayer->textColor().g(), textLayer->textColor().b(), textLayer->textColor().a()))
               .arg(textLayer->isStrokeEnabled() ? 1 : 0)
               .arg(rgbaKey(textLayer->strokeColor().r(), textLayer->strokeColor().g(), textLayer->strokeColor().b(), textLayer->strokeColor().a()))
               .arg(textLayer->strokeWidth(), 0, 'f', 2)
               .arg(textLayer->isShadowEnabled() ? 1 : 0)
               .arg(rgbaKey(textLayer->shadowColor().r(), textLayer->shadowColor().g(), textLayer->shadowColor().b(), textLayer->shadowColor().a()))
               .arg(textLayer->shadowOffsetX(), 0, 'f', 2)
               .arg(textLayer->shadowOffsetY(), 0, 'f', 2)
               .arg(textLayer->shadowBlur(), 0, 'f', 2)
               .arg(textLayer->tracking(), 0, 'f', 2)
               .arg(textLayer->leading(), 0, 'f', 2)
               .arg(static_cast<int>(textLayer->wrapMode()))
               .arg(textLayer->maxWidth(), 0, 'f', 2)
               .arg(textLayer->boxHeight(), 0, 'f', 2)
               .arg(static_cast<int>(textLayer->verticalAlignment()))
               .arg(static_cast<int>(textLayer->horizontalAlignment()))
               .arg(textLayer->paragraphSpacing(), 0, 'f', 2)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  return QString();
}

void applyRasterizerEffectsAndMasksToSurface(const ArtifactAbstractLayerPtr& targetLayer,
                                             QImage& surface)
{
  if (!targetLayer || surface.isNull()) {
    return;
  }

  const bool hasMasks = targetLayer->hasMasks();
  const auto effects = targetLayer->getEffects();
  bool hasRasterizerEffect = false;
  for (const auto& effect : effects) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      hasRasterizerEffect = true;
      break;
    }
  }

  if (!hasRasterizerEffect && !hasMasks) {
    return;
  }

  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
  if (mat.type() != CV_32FC4) {
    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
  }

  if (hasRasterizerEffect) {
    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromCVMat(mat);
    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled() ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        continue;
      }

      ArtifactCore::ImageF32x4RGBAWithCache next;
      effect->applyCPUOnly(current, next);
      current = next;
    }
    mat = current.image().toCVMat();
  }

  if (hasMasks) {
    for (int m = 0; m < targetLayer->maskCount(); ++m) {
      LayerMask mask = targetLayer->mask(m);
      mask.applyToImage(mat.cols, mat.rows, &mat);
    }
  }

  surface = ArtifactCore::CvUtils::cvMatToQImage(mat);
}

bool hasRasterizerEffectsOrMasks(const ArtifactAbstractLayerPtr& targetLayer)
{
  if (!targetLayer) {
    return false;
  }
  if (targetLayer->hasMasks()) {
    return true;
  }
  for (const auto& effect : targetLayer->getEffects()) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
    }
  }
  return false;
}

} // namespace

bool layerHasCpuRasterizerWork(const ArtifactAbstractLayerPtr& layer)
{
  return hasRasterizerEffectsOrMasks(layer);
}

bool layerUsesSurfaceUploadForCompositionView(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }
  if (hasRasterizerEffectsOrMasks(layer)) {
    return true;
  }
  return std::dynamic_pointer_cast<ArtifactImageLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSvgLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactTextLayer>(layer) != nullptr;
}

bool layerUsesGpuTextureCacheForCompositionView(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }

  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    return false;
  }

  return std::dynamic_pointer_cast<ArtifactImageLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSvgLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactTextLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer) != nullptr;
}

void drawLayerForCompositionView(const ArtifactAbstractLayerPtr &layer,
                                 ArtifactIRenderer *renderer,
                                 float opacityOverride,
                                 QString* videoDebugOut,
                                 QHash<QString, LayerSurfaceCacheEntry>* surfaceCache,
                                 GPUTextureCacheManager* gpuTextureCacheManager,
                                 int64_t cacheFrameNumber,
                                 bool offlineRender,
                                 DetailLevel lod)
{
  if (!layer || !renderer) {
    return;
  }

  const QRectF localRect = layer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    return;
  }

  const QMatrix4x4 globalTransform4x4 = layer->getGlobalTransform4x4();

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF& rect, bool allowSurfaceCache) {
    if (surface.isNull()) {
      return false;
    }

    const QString ownerId = layer->id().toString();
    const QString cacheSignature = buildLayerSurfaceCacheKey(layer, surface, cacheFrameNumber);
    LayerSurfaceCacheEntry* cacheEntry = nullptr;

    if (surfaceCache && !cacheSignature.isEmpty()) {
      auto cacheIt = surfaceCache->find(ownerId);
      if (cacheIt != surfaceCache->end() &&
          cacheIt->ownerId == ownerId &&
          cacheIt->cacheSignature == cacheSignature &&
          !cacheIt->processedSurface.isNull()) {
        cacheEntry = &(*cacheIt);
        surface = cacheIt->processedSurface;
      } else {
        if (allowSurfaceCache) {
          applyRasterizerEffectsAndMasksToSurface(layer, surface);
        }

        LayerSurfaceCacheEntry entry;
        entry.ownerId = ownerId;
        entry.cacheSignature = cacheSignature;
        entry.processedSurface = surface;
        entry.frameNumber = cacheFrameNumber;
        if (gpuTextureCacheManager && layerUsesGpuTextureCacheForCompositionView(layer)) {
          entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(ownerId, cacheSignature, surface);
        }
        (*surfaceCache)[ownerId] = entry;
        cacheEntry = &(*surfaceCache)[ownerId];
      }
    } else if (allowSurfaceCache) {
      applyRasterizerEffectsAndMasksToSurface(layer, surface);
    }

    const float baseOpacity = (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
    drawWithClonerEffect(layer, globalTransform4x4,
      [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
        const float finalOpacity = baseOpacity * instanceWeight;

        if (gpuTextureCacheManager && cacheEntry && layerUsesGpuTextureCacheForCompositionView(layer)) {
          if (!gpuTextureCacheManager->isValid(cacheEntry->gpuTextureHandle)) {
            const QImage& uploadSurface =
                cacheEntry->processedSurface.isNull() ? surface : cacheEntry->processedSurface;
            cacheEntry->gpuTextureHandle =
                gpuTextureCacheManager->acquireOrCreate(layer->id().toString(), cacheSignature, uploadSurface);
          }
          if (auto* srv = gpuTextureCacheManager->textureView(cacheEntry->gpuTextureHandle)) {
            renderer->drawSpriteTransformed(static_cast<float>(rect.x()),
                                 static_cast<float>(rect.y()),
                                 static_cast<float>(rect.width()),
                                 static_cast<float>(rect.height()),
                                 instanceTransform,
                                 srv,
                                 finalOpacity);
            return;
          }
        }

        renderer->drawSpriteTransformed(static_cast<float>(rect.x()),
                             static_cast<float>(rect.y()),
                             static_cast<float>(rect.width()),
                             static_cast<float>(rect.height()),
                             instanceTransform,
                             surface,
                             finalOpacity);
      });
    return true;
  };

  if (const auto solid2D =
          std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer)) {
    const auto color = solid2D->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, false);
    } else {
      renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                              static_cast<float>(localRect.y()),
                              static_cast<float>(localRect.width()),
                              static_cast<float>(localRect.height()),
                              globalTransform4x4,
                              color, (opacityOverride >= 0.0f ? opacityOverride : layer->opacity()));
    }
    return;
  }

  if (const auto solidImage =
          std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    const auto color = solidImage->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, false);
    } else {
      renderer->drawSolidRectTransformed(static_cast<float>(localRect.x()),
                              static_cast<float>(localRect.y()),
                              static_cast<float>(localRect.width()),
                              static_cast<float>(localRect.height()),
                              globalTransform4x4,
                              color, (opacityOverride >= 0.0f ? opacityOverride : layer->opacity()));
    }
    return;
  }

  if (const auto imageLayer =
          std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
    const QImage img = downsampleForLOD(imageLayer->toQImage(), lod);
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (const auto svgLayer =
          std::dynamic_pointer_cast<ArtifactSvgLayer>(layer)) {
    if (svgLayer->isLoaded()) {
      const QImage svgImage = svgLayer->toQImage();
      if (!svgImage.isNull()) {
        applySurfaceAndDraw(svgImage, localRect, hasRasterizerEffectsOrMasks(layer));
      } else {
        svgLayer->draw(renderer);
      }
      return;
    }
  }

  if (const auto videoLayer =
          std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    const QImage frame = downsampleForLOD(offlineRender
        ? videoLayer->decodeFrameToQImage(cacheFrameNumber >= 0 ? cacheFrameNumber : layer->currentFrame())
        : videoLayer->currentFrameToQImage(), lod);
    if (videoDebugOut) {
      const bool loaded = videoLayer->isLoaded();
      const int64_t cf = layer->currentFrame();
      const FramePosition ip = layer->inPoint();
      const FramePosition op = layer->outPoint();
      const bool active = layer->isActiveAt(FramePosition(static_cast<int>(cf)));
      *videoDebugOut = QString("[Video] loaded=%1 frame.isNull=%2 size=%3x%4 active=%5 range=[%6,%7] curFrame=%8")
        .arg(loaded).arg(frame.isNull())
        .arg(frame.isNull() ? 0 : frame.width())
        .arg(frame.isNull() ? 0 : frame.height())
        .arg(active)
        .arg(ip.framePosition()).arg(op.framePosition()).arg(cf);
    }
    if (!frame.isNull()) {
      applySurfaceAndDraw(frame, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (const auto textLayer =
          std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect, hasRasterizerEffectsOrMasks(layer));
    }
    return;
  }

  if (const auto compLayer =
          std::dynamic_pointer_cast<ArtifactCompositionLayer>(layer)) {
    if (auto childComp = compLayer->sourceComposition()) {
      const QSize childSize = childComp->settings().compositionSize();
      const int64_t childFrame = layer->currentFrame() - layer->inPoint().framePosition();
      childComp->goToFrame(childFrame);
      QImage childImage = childComp->getThumbnail(childSize.width(), childSize.height());

      if (!childImage.isNull()) {
        applySurfaceAndDraw(childImage, localRect, hasRasterizerEffectsOrMasks(layer));
      }
    }
    return;
  }

  layer->draw(renderer);
}

} // namespace Artifact
