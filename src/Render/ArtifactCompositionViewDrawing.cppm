module;
#include <utility>
#include <Layer/ArtifactCloneEffectSupport.hpp>

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QUuid>
#include <Layer/ArtifactSolidGradientUtil.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <opencv2/opencv.hpp>

//#include <QImage>

export module Artifact.Render.CompositionViewDrawing;

import Artifact.Render.IRenderer;
import Artifact.Render.Context;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Asset.Manager;
import Video.VideoFrame;
import Artifact.Layer.Text;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Particle;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Composition;
import Artifact.Layer.AdjustableLayer;
import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Mask.LayerMask;
import Artifact.Composition.Abstract;
import Layer.Matte;
import Image.ImageF32x4_RGBA;
import Core.Light;
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
  std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;
  GPUTextureCacheHandle gpuTextureHandle;
  int64_t frameNumber = std::numeric_limits<int64_t>::min();
};

export struct StaticLayerGpuCacheEntry
{
  QString ownerId;
  QString cacheSignature;
  QImage processedSurface;
  std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;
  GPUTextureCacheHandle gpuTextureHandle;
  int64_t lastFrameNumber = std::numeric_limits<int64_t>::min();
  size_t byteSize = 0;
};

export bool layerHasCpuRasterizerWork(ArtifactAbstractLayer* layer);
export bool layerUsesSurfaceUploadForCompositionView(ArtifactAbstractLayer* layer);
export bool layerUsesGpuTextureCacheForCompositionView(ArtifactAbstractLayer* layer);
export bool layerUsesStaticLayerGpuCacheForCompositionView(ArtifactAbstractLayer* layer);
export bool applyCompositionFinalEffectsToImage(ArtifactAbstractComposition* composition,
                                                QImage& image,
                                                DetailLevel lod = DetailLevel::High);
export void applyRasterizerEffectsAndMasksToSurface(
    ArtifactAbstractLayer* targetLayer, QImage& surface, DetailLevel lod);
export void applyRasterizerEffectsAndMasksToSurface(
    ArtifactAbstractLayer* targetLayer,
    ArtifactCore::ImageF32x4_RGBA& surface, DetailLevel lod);
export void drawLayerForCompositionView(ArtifactAbstractLayer* layer,
                                        ArtifactIRenderer *renderer,
                                        float opacityOverride = -1.0f,
                                        QString* videoDebugOut = nullptr,
                                        QHash<QString, LayerSurfaceCacheEntry>* surfaceCache = nullptr,
                                        GPUTextureCacheManager* gpuTextureCacheManager = nullptr,
                                        int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),
                                        bool offlineRender = false,
                                        DetailLevel lod = DetailLevel::High,
                                        const std::vector<ArtifactCore::Light>* sceneLights = nullptr);

namespace {

QHash<QString, StaticLayerGpuCacheEntry> &staticLayerGpuCache()
{
  static QHash<QString, StaticLayerGpuCacheEntry> cache;
  return cache;
}

EffectContext makeLayerEffectContext(ArtifactAbstractLayer* layer,
                                     const QRectF& roi = QRectF())
{
  EffectContext ctx;
  ctx.roi = roi;
  ctx.isInteractive = true;
  ctx.layerFrame = layer ? layer->currentFrame() : 0;
  if (auto* composition =
          layer ? dynamic_cast<ArtifactAbstractComposition*>(
                      layer->compositionObject())
                : nullptr) {
    ctx.compositionFrame = composition->framePosition().framePosition();
    ctx.frameRate = std::max(
        1.0f, static_cast<float>(composition->frameRate().framerate()));
  } else {
    ctx.compositionFrame = ctx.layerFrame;
    ctx.frameRate = 30.0;
  }
  ctx.timeSeconds = ctx.frameRate > 0.0
                        ? static_cast<double>(ctx.compositionFrame) / ctx.frameRate
                        : 0.0;
  if (layer) {
    ctx.effectStrength = layer->effectEnvelope()
        .sample(ctx.layerFrame).effectStrength;
  }
  ctx.sampler = nullptr;

  return ctx;
}

EffectContext makeCompositionEffectContext(ArtifactAbstractComposition* composition,
                                           const QRectF& roi = QRectF())
{
  EffectContext ctx;
  ctx.roi = roi;
  ctx.isInteractive = true;
  ctx.compositionFrame =
      composition ? composition->framePosition().framePosition() : 0;
  ctx.layerFrame = ctx.compositionFrame;
  ctx.frameRate = composition
                      ? std::max(
                            1.0f,
                            static_cast<float>(composition->frameRate().framerate()))
                      : 30.0;
  ctx.timeSeconds = ctx.frameRate > 0.0
                        ? static_cast<double>(ctx.compositionFrame) / ctx.frameRate
                        : 0.0;
  ctx.sampler = nullptr;
  return ctx;
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

void registerCompositionViewContextSnapshot(ArtifactAbstractLayer* layer,
                                            bool offlineRender,
                                            DetailLevel lod)
{
  if (!layer) {
    return;
  }
  const auto sourceSize = layer->sourceSize();
  const int sourceWidth = std::max(1, sourceSize.width);
  const int sourceHeight = std::max(1, sourceSize.height);
  auto ctx = offlineRender
      ? createFinalExportContext(layer->currentFrame(),
                                 QSize(sourceWidth, sourceHeight),
                                 QSizeF(sourceWidth, sourceHeight),
                                 RenderROI(0.0f,
                                           0.0f,
                                           static_cast<float>(sourceWidth),
                                           static_cast<float>(sourceHeight)),
                                 lodScale(lod))
      : createEditorPreviewContext(layer->currentFrame(),
                                   QSize(sourceWidth, sourceHeight),
                                   QSizeF(sourceWidth, sourceHeight),
                                   RenderROI(0.0f,
                                             0.0f,
                                             static_cast<float>(sourceWidth),
                                             static_cast<float>(sourceHeight)),
                                   lodScale(lod));

  const QString key = RenderContextRegistry::instance().makeKey(
      offlineRender ? RenderPurpose::FinalExport : RenderPurpose::EditorPreview,
      layer->id().toString(),
      layer->currentFrame(),
      ctx->resolutionScale);
  auto snapshot = createRenderContextSnapshot(*ctx,
                                              offlineRender ? RenderPurpose::FinalExport : RenderPurpose::EditorPreview,
                                              key);
  RenderContextRegistry::instance().registerSnapshot(snapshot);
}

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

ArtifactCore::ImageF32x4_RGBA downsampleForLOD(const ArtifactCore::ImageF32x4_RGBA& image,
                                               DetailLevel lod)
{
  if (image.isEmpty() || lod == DetailLevel::High) {
    return image;
  }

  const float scale = lodScale(lod);
  const int targetW =
      std::max(1, static_cast<int>(std::round(image.width() * scale)));
  const int targetH =
      std::max(1, static_cast<int>(std::round(image.height() * scale)));
  if (targetW == image.width() && targetH == image.height()) {
    return image;
  }

  cv::Mat resized;
  cv::resize(image.toCVMat(),
             resized,
             cv::Size(targetW, targetH),
             0.0,
             0.0,
             cv::INTER_LINEAR);

  ArtifactCore::ImageF32x4_RGBA result;
  result.setFromCVMat(resized);
  return result;
}

QString buildLayerSurfaceCacheKey(ArtifactAbstractLayer* layer,
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

  bool hasAnimatedEffectProperty = false;
  for (const auto& effect : layer->getEffects()) {
    if (!effect || !effect->isEnabled()) {
      continue;
    }
    for (const auto& property : effect->editableProperties()) {
      if (property && (!property->getKeyFrames().empty() ||
                       property->hasExpression() ||
                       property->hasEnvelopes())) {
        hasAnimatedEffectProperty = true;
        break;
      }
    }
    if (hasAnimatedEffectProperty) {
      break;
    }
  }
  if (hasAnimatedEffectProperty) {
    key += QStringLiteral("|effectFrame=%1").arg(frameNumber);
  }

  if (auto* solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layer)) {
    const QRectF bounds = solid2D->localBounds();
    key += QStringLiteral("|solid2D|color=%1|bounds=%2x%3")
               .arg(rgbaKey(solid2D->color().r(), solid2D->color().g(), solid2D->color().b(), solid2D->color().a()))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto* solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral("|solidImage|color=%1|fill=%2|g0=%3|g1=%4|ang=%5|rev=%6|cx=%7|cy=%8|scale=%9|off=%10|bounds=%11x%12")
               .arg(rgbaKey(solidImage->color().r(), solidImage->color().g(), solidImage->color().b(), solidImage->color().a()))
               .arg(static_cast<int>(solidImage->fillType()))
               .arg(rgbaKey(solidImage->gradientStartColor().r(), solidImage->gradientStartColor().g(),
                            solidImage->gradientStartColor().b(), solidImage->gradientStartColor().a()))
               .arg(rgbaKey(solidImage->gradientEndColor().r(), solidImage->gradientEndColor().g(),
                            solidImage->gradientEndColor().b(), solidImage->gradientEndColor().a()))
               .arg(solidImage->gradientAngleDegrees(), 0, 'f', 4)
               .arg(solidImage->gradientReverse() ? 1 : 0)
               .arg(solidImage->gradientCenterX(), 0, 'f', 4)
               .arg(solidImage->gradientCenterY(), 0, 'f', 4)
               .arg(solidImage->gradientScale(), 0, 'f', 4)
               .arg(solidImage->gradientOffset(), 0, 'f', 4)
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer)) {
    key += QStringLiteral("|image|src=%1|fit=%2|size=%3x%4")
               .arg(imageLayer->sourcePath())
               .arg(imageLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto* svgLayer = dynamic_cast<ArtifactSvgLayer*>(layer)) {
    key += QStringLiteral("|svg|src=%1|fit=%2|size=%3x%4")
               .arg(svgLayer->sourcePath())
               .arg(svgLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer)) {
    key += QStringLiteral("|video|src=%1|frame=%2|proxy=%3|size=%4x%5")
               .arg(videoLayer->sourcePath())
               .arg(frameNumber)
               .arg(static_cast<int>(videoLayer->proxyQuality()))
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto* textLayer = dynamic_cast<ArtifactTextLayer*>(layer)) {
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

  if (dynamic_cast<ArtifactParticleLayer*>(layer) ||
      dynamic_cast<ArtifactFormParticleLayer*>(layer)) {
    // Particle layers are time-dependent and can change every frame even when
    // their size stays the same, so we avoid the generic surface cache.
    return QString();
  }

  return QString();
}

bool buildRasterizedSurfaceBuffer(ArtifactAbstractLayer* targetLayer,
                                  const QImage& surface,
                                  ArtifactCore::ImageF32x4_RGBA* outBuffer)
{
  if (!targetLayer || surface.isNull() || !outBuffer) {
    return false;
  }

  const bool hasMasks = targetLayer->hasMasks();
  const bool hasMattes = !targetLayer->matteReferences().empty();
  const auto effects = targetLayer->getEffects();
  bool hasRasterizerEffect = false;
  for (const auto& effect : effects) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      hasRasterizerEffect = true;
      break;
    }
  }

  if (!hasRasterizerEffect && !hasMasks && !hasMattes) {
    return false;
  }

  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
  if (mat.type() != CV_32FC4) {
    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
  }

  if (hasMasks) {
    // Layer masks define the source alpha seen by rasterizer effects. Applying
    // them first prevents effects such as Drop Shadow from sampling pixels
    // that the layer mask has already removed.
    const QRectF lb = targetLayer->localBounds();
    const float scaleX = static_cast<float>(mat.cols) /
                         std::max(1.0f, static_cast<float>(lb.width()));
    const float scaleY = static_cast<float>(mat.rows) /
                         std::max(1.0f, static_cast<float>(lb.height()));
    const float maskOffsetX = static_cast<float>(-lb.x() * scaleX);
    const float maskOffsetY = static_cast<float>(-lb.y() * scaleY);
    for (int m = 0; m < targetLayer->maskCount(); ++m) {
      LayerMask mask = targetLayer->mask(m);
      mask.applyToImage(mat.cols, mat.rows, &mat, maskOffsetX, maskOffsetY,
                        scaleX, scaleY);
    }
  }

  if (hasRasterizerEffect) {
    ArtifactCore::ImageF32x4_RGBA cpuImage;
    cpuImage.setFromCVMat(mat);
    ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

    // 調整レイヤーのエフェクトは背面全体に作用するため、
    // requiresFullFrame として扱い ROI 縮小によるサンプリング欠けを防ぐ。
    const bool isAdjustment = targetLayer->isAdjustmentLayer();

    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled() ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        continue;
      }

      // ROI hint を収集して必要な拡張量を計算する。
      // 調整レイヤーは hint に関わらず常に full-frame が必要。
      const EffectROIHint hint = effect->roiHint();
      if (isAdjustment && !hint.requiresFullFrame) {
        // 調整レイヤーの場合は必ず full-frame 前提で処理するため、
        // 縮小 ROI でエフェクトが欠けることはない（surface 自体が full-frame）。
        qDebug()
            << "[Rasterizer][AdjLayer] effect"
            << effect->displayName().toQString()
            << "treated as full-frame due to adjustment layer";
      } else if (!hint.isEmpty()) {
        // ROI 拡張が必要なエフェクト（ブラー、グローなど）のログ。
        // 実際の ROI 拡張は上位の描画ループが担う。
        // ここでは surface がすでに適切なサイズで渡されている前提。
        qDebug()
            << "[Rasterizer] effect" << effect->displayName().toQString()
            << "roiHint: kind=" << static_cast<int>(hint.kind)
            << "expansionPx=" << hint.expansionPixels
            << "fullFrame=" << hint.requiresFullFrame;
      }

      ArtifactCore::ImageF32x4RGBAWithCache next;
      effect->setContext(makeLayerEffectContext(
          targetLayer,
          QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
                 static_cast<qreal>(current.height()))));
      effect->applyConfigured(current, next);
      current = next;
    }

    // Store frame for temporal effect lookback.
    mat = current.image().toCVMat();
  }

  outBuffer->setFromCVMat(mat);
  return true;
}

bool buildRasterizedSurfaceBuffer(ArtifactAbstractLayer* targetLayer,
                                  const ArtifactCore::ImageF32x4_RGBA& surface,
                                  ArtifactCore::ImageF32x4_RGBA* outBuffer)
{
  if (!targetLayer || surface.isEmpty() || !outBuffer) {
    return false;
  }

  const bool hasMasks = targetLayer->hasMasks();
  const bool hasMattes = !targetLayer->matteReferences().empty();
  const auto effects = targetLayer->getEffects();
  bool hasRasterizerEffect = false;
  for (const auto& effect : effects) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      hasRasterizerEffect = true;
      break;
    }
  }

  if (!hasRasterizerEffect && !hasMasks && !hasMattes) {
    return false;
  }

  cv::Mat mat = surface.toCVMat();

  if (hasMasks) {
    const QRectF lb = targetLayer->localBounds();
    const float scaleX = static_cast<float>(mat.cols) /
                         std::max(1.0f, static_cast<float>(lb.width()));
    const float scaleY = static_cast<float>(mat.rows) /
                         std::max(1.0f, static_cast<float>(lb.height()));
    const float maskOffsetX = static_cast<float>(-lb.x() * scaleX);
    const float maskOffsetY = static_cast<float>(-lb.y() * scaleY);
    for (int m = 0; m < targetLayer->maskCount(); ++m) {
      LayerMask mask = targetLayer->mask(m);
      mask.applyToImage(mat.cols, mat.rows, &mat, maskOffsetX, maskOffsetY,
                        scaleX, scaleY);
    }
  }

  if (hasRasterizerEffect) {
    ArtifactCore::ImageF32x4RGBAWithCache current(surface);

    const bool isAdjustment = targetLayer->isAdjustmentLayer();

    for (const auto& effect : effects) {
      if (!effect || !effect->isEnabled() ||
          effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        continue;
      }

      const EffectROIHint hint = effect->roiHint();
      if (isAdjustment && !hint.requiresFullFrame) {
        qDebug()
            << "[Rasterizer][AdjLayer] effect"
            << effect->displayName().toQString()
            << "treated as full-frame due to adjustment layer";
      } else if (!hint.isEmpty()) {
        qDebug()
            << "[Rasterizer] effect" << effect->displayName().toQString()
            << "roiHint: kind=" << static_cast<int>(hint.kind)
            << "expansionPx=" << hint.expansionPixels
            << "fullFrame=" << hint.requiresFullFrame;
      }

      ArtifactCore::ImageF32x4RGBAWithCache next;
      effect->setContext(makeLayerEffectContext(
          targetLayer,
          QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
                 static_cast<qreal>(current.height()))));
      effect->applyConfigured(current, next);
      current = next;
    }

    mat = current.image().toCVMat();
  }

  outBuffer->setFromCVMat(mat);
  return true;
}

void applyRasterizerEffectsAndMasksToSurfaceImpl(
    ArtifactAbstractLayer* targetLayer, QImage& surface, DetailLevel lod)
{
  // Feature 1: Effect LOD Integration
  // Downsample surface based on LOD before applying effects to reduce cost.
  const QImage processedSurface = downsampleForLOD(surface, lod);
  if (processedSurface.isNull()) return;

  ArtifactCore::ImageF32x4_RGBA buffer;
  if (buildRasterizedSurfaceBuffer(targetLayer, processedSurface, &buffer)) {
    surface = buffer.toQImage();
  }
}

void applyRasterizerEffectsAndMasksToSurfaceImpl(
    ArtifactAbstractLayer* targetLayer,
    ArtifactCore::ImageF32x4_RGBA& surface,
    DetailLevel lod)
{
  ArtifactCore::ImageF32x4_RGBA processedSurface = downsampleForLOD(surface, lod);
  if (processedSurface.isEmpty()) {
    return;
  }

  ArtifactCore::ImageF32x4_RGBA buffer;
  if (buildRasterizedSurfaceBuffer(targetLayer, processedSurface, &buffer)) {
    surface = std::move(buffer);
  } else {
    surface = std::move(processedSurface);
  }
}

bool hasRasterizerEffectsOrMasks(ArtifactAbstractLayer* targetLayer)
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

QImage applyMatteStackToSurface(const QImage& surface,
                                const ArtifactCore::MatteStack& matteStack,
                                const std::vector<QImage>& sourceImages)
{
  if (matteStack.isEmpty() || surface.isNull()) {
    return surface;
  }

  const int w = surface.width();
  const int h = surface.height();
  if (w <= 0 || h <= 0) {
    return surface;
  }

  const size_t pixelCount = static_cast<size_t>(w) * h;
  std::vector<float> combinedMask(pixelCount, 0.0f);

  const auto& nodes = matteStack.nodes();
  int sourceIndex = 0;

  for (const auto& node : nodes) {
    if (!node.isEnabled()) {
      continue;
    }
    if (sourceIndex >= static_cast<int>(sourceImages.size())) {
      break;
    }

    const QImage& srcImg = sourceImages[sourceIndex];
    ++sourceIndex;

    if (srcImg.isNull()) {
      continue;
    }

    const int srcW = srcImg.width();
    const int srcH = srcImg.height();
    if (srcW <= 0 || srcH <= 0) {
      continue;
    }

    std::vector<float> matteMask(pixelCount, 0.0f);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const int sx = std::min(x * srcW / w, srcW - 1);
        const int sy = std::min(y * srcH / h, srcH - 1);
        const QRgb pixel = srcImg.pixel(sx, sy);

        float v = 0.0f;
        if (ArtifactCore::MatteModeUtils::isLuminance(node.mode())) {
          v = (qRed(pixel) * 0.299f + qGreen(pixel) * 0.587f + qBlue(pixel) * 0.114f) / 255.0f;
        } else {
          v = qAlpha(pixel) / 255.0f;
        }

        if (ArtifactCore::MatteModeUtils::isInverted(node.mode())) {
          v = 1.0f - v;
        }
        matteMask[static_cast<size_t>(y) * w + x] = std::clamp(v, 0.0f, 1.0f);
      }
    }

    switch (matteStack.stackMode()) {
    case ArtifactCore::MatteStackMode::Add:
      for (size_t i = 0; i < pixelCount; ++i) {
        combinedMask[i] = std::min(1.0f, combinedMask[i] + matteMask[i]);
      }
      break;
    case ArtifactCore::MatteStackMode::Common:
      for (size_t i = 0; i < pixelCount; ++i) {
        combinedMask[i] = std::min(combinedMask[i], matteMask[i]);
      }
      break;
    case ArtifactCore::MatteStackMode::Subtract:
      for (size_t i = 0; i < pixelCount; ++i) {
        combinedMask[i] = std::max(0.0f, combinedMask[i] - matteMask[i]);
      }
      break;
    }
  }

  QImage result = surface.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = static_cast<size_t>(y) * w + x;
      QRgb pixel = result.pixel(x, y);
      const float mask = combinedMask[idx];
      int a = static_cast<int>(qAlpha(pixel) * mask);
      const int r = static_cast<int>(qRed(pixel) * mask);
      const int g = static_cast<int>(qGreen(pixel) * mask);
      const int b = static_cast<int>(qBlue(pixel) * mask);
      result.setPixel(x, y, qRgba(std::clamp(r, 0, 255),
                                  std::clamp(g, 0, 255),
                                  std::clamp(b, 0, 255),
                                  std::clamp(a, 0, 255)));
    }
  }
  return result;
}

} // namespace

void applyRasterizerEffectsAndMasksToSurface(
    ArtifactAbstractLayer* targetLayer, QImage& surface, DetailLevel lod)
{
  applyRasterizerEffectsAndMasksToSurfaceImpl(targetLayer, surface, lod);
}

void applyRasterizerEffectsAndMasksToSurface(
    ArtifactAbstractLayer* targetLayer,
    ArtifactCore::ImageF32x4_RGBA& surface, DetailLevel lod)
{
  applyRasterizerEffectsAndMasksToSurfaceImpl(targetLayer, surface, lod);
}

bool layerHasCpuRasterizerWork(ArtifactAbstractLayer* layer)
{
  return hasRasterizerEffectsOrMasks(layer);
}

bool layerUsesSurfaceUploadForCompositionView(ArtifactAbstractLayer* layer)
{
  if (!layer) {
    return false;
  }
  if (hasRasterizerEffectsOrMasks(layer)) {
    return true;
  }
  return dynamic_cast<ArtifactImageLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSvgLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactVideoLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactTextLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolid2DLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolidImageLayer*>(layer) != nullptr;
}

bool layerUsesGpuTextureCacheForCompositionView(ArtifactAbstractLayer* layer)
{
  if (!layer) {
    return false;
  }

  return dynamic_cast<ArtifactImageLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSvgLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactVideoLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactTextLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolid2DLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolidImageLayer*>(layer) != nullptr;
}

bool layerUsesStaticLayerGpuCacheForCompositionView(ArtifactAbstractLayer* layer)
{
  if (!layer || !layer->usesLayerCache()) {
    return false;
  }

  if (dynamic_cast<ArtifactVideoLayer*>(layer) != nullptr ||
      dynamic_cast<ArtifactParticleLayer*>(layer) != nullptr ||
      dynamic_cast<ArtifactFormParticleLayer*>(layer) != nullptr ||
      dynamic_cast<ArtifactCompositionLayer*>(layer) != nullptr) {
    return false;
  }

  return dynamic_cast<ArtifactImageLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSvgLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactTextLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolid2DLayer*>(layer) != nullptr ||
         dynamic_cast<ArtifactSolidImageLayer*>(layer) != nullptr;
}

bool applyCompositionFinalEffectsToImage(ArtifactAbstractComposition* composition,
                                         QImage& image,
                                         DetailLevel lod)
{
  if (!composition || image.isNull()) {
    return false;
  }

  const auto effects = composition->getEffects();
  bool hasRasterizerEffect = false;
  for (const auto& effect : effects) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      hasRasterizerEffect = true;
      break;
    }
  }
  if (!hasRasterizerEffect) {
    return false;
  }

  const QImage processedImage = downsampleForLOD(image, lod);
  if (processedImage.isNull()) {
    return false;
  }

  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(processedImage, true);
  if (mat.type() != CV_32FC4) {
    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
  }

  ArtifactCore::ImageF32x4_RGBA cpuImage;
  cpuImage.setFromCVMat(mat);
  ArtifactCore::ImageF32x4RGBAWithCache current(cpuImage);

  for (const auto& effect : effects) {
    if (!effect || !effect->isEnabled() ||
        effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
      continue;
    }
    ArtifactCore::ImageF32x4RGBAWithCache next;
    effect->setContext(makeCompositionEffectContext(
        composition,
        QRectF(0.0, 0.0, static_cast<qreal>(current.width()),
               static_cast<qreal>(current.height()))));
    effect->applyConfigured(current, next);
    current = next;
  }

  image = current.image().toQImage();
  return true;
}

void drawLayerForCompositionView(ArtifactAbstractLayer* layer,
                                 ArtifactIRenderer *renderer,
                                 float opacityOverride,
                                 QString* videoDebugOut,
                                 QHash<QString, LayerSurfaceCacheEntry>* surfaceCache,
                                 GPUTextureCacheManager* gpuTextureCacheManager,
                                 int64_t cacheFrameNumber,
                                 bool offlineRender,
                                 DetailLevel lod,
                                 const std::vector<ArtifactCore::Light>* sceneLights)
{
  if (!layer || !renderer) {
    return;
  }

  if (offlineRender && !layer->shouldIncludeInFinalRender()) {
    return;
  }

  const bool layerCacheEnabled = layer->usesLayerCache();

  const QRectF localRect = layer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    qDebug() << "[drawLayerForCompositionView] skip layer: invalid local bounds"
             << "id=" << layer->id().toString() << "rect=" << localRect
             << "sourceSize=" << layer->sourceSize().width << "x" << layer->sourceSize().height;
    return;
  }

  registerCompositionViewContextSnapshot(layer, offlineRender, lod);

  const QMatrix4x4 globalTransform4x4 = layer->getGlobalTransform4x4();

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF& rect, bool allowSurfaceCache) {
    if (surface.isNull()) {
      return false;
    }

    const bool usesGpuTextureCache =
        layerCacheEnabled && gpuTextureCacheManager &&
        layerUsesGpuTextureCacheForCompositionView(layer);
    const bool usesStaticGpuCache =
        layerUsesStaticLayerGpuCacheForCompositionView(layer);
    const bool usesSurfaceCache =
        surfaceCache && (allowSurfaceCache || usesGpuTextureCache ||
                         usesStaticGpuCache);
    const QString ownerId = usesSurfaceCache || usesStaticGpuCache
                                ? layer->id().toString()
                                : QString{};
    const QString cacheSignature = usesSurfaceCache || usesStaticGpuCache
                                       ? buildLayerSurfaceCacheKey(
                                             layer, surface, cacheFrameNumber)
                                       : QString{};
    QString gpuOwnerId = ownerId;
    QString gpuCacheSignature = cacheSignature;
    if (!allowSurfaceCache) {
      if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer);
          imageLayer && imageLayer->canShareSourceGpuTexture()) {
        const auto version = imageLayer->sourceVersion();
        if (version > 0) {
          gpuOwnerId = QStringLiteral("asset:%1").arg(
              imageLayer->sourceAssetId().toString(QUuid::WithoutBraces));
          gpuCacheSignature = QStringLiteral("image-f32:v%1").arg(version);
        }
      }
    }
    LayerSurfaceCacheEntry* cacheEntry = nullptr;
    StaticLayerGpuCacheEntry* staticCacheEntry = nullptr;

    if (layerUsesStaticLayerGpuCacheForCompositionView(layer) &&
        !cacheSignature.isEmpty()) {
      auto &staticCache = staticLayerGpuCache();
      auto it = staticCache.find(ownerId);
      if (it != staticCache.end() && it->ownerId == ownerId &&
          it->cacheSignature == cacheSignature) {
        staticCacheEntry = &(*it);
        if (!staticCacheEntry->processedSurface.isNull()) {
          surface = staticCacheEntry->processedSurface;
        }
      }
    }

    if (usesSurfaceCache && !cacheSignature.isEmpty()) {
      auto cacheIt = surfaceCache->find(ownerId);
      if (cacheIt != surfaceCache->end() &&
          cacheIt->ownerId == ownerId &&
          cacheIt->cacheSignature == cacheSignature &&
          !cacheIt->processedSurface.isNull()) {
        cacheEntry = &(*cacheIt);
        surface = cacheIt->processedSurface;
      } else {
        std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;
        if (allowSurfaceCache) {
          ArtifactCore::ImageF32x4_RGBA processed;
          if (buildRasterizedSurfaceBuffer(layer, surface, &processed)) {
            processedBuffer = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);
            if (!layerCacheEnabled || !gpuTextureCacheManager || !layerUsesGpuTextureCacheForCompositionView(layer)) {
              surface = processed.toQImage();
            }
          }
        }

        LayerSurfaceCacheEntry entry;
        entry.ownerId = ownerId;
        entry.cacheSignature = cacheSignature;
        entry.processedBuffer = processedBuffer;
        entry.processedSurface = surface;
        entry.frameNumber = cacheFrameNumber;
        if (usesGpuTextureCache) {
          if (entry.processedBuffer) {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(gpuOwnerId, gpuCacheSignature, *entry.processedBuffer);
          } else {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(gpuOwnerId, gpuCacheSignature, surface);
          }
        }
        (*surfaceCache)[ownerId] = entry;
        cacheEntry = &(*surfaceCache)[ownerId];
      }
    } else if (allowSurfaceCache) {
      applyRasterizerEffectsAndMasksToSurface(layer, surface, lod);
    }

    if (layerUsesStaticLayerGpuCacheForCompositionView(layer) &&
        !cacheSignature.isEmpty()) {
      auto &staticCache = staticLayerGpuCache();
      if (!staticCacheEntry) {
        StaticLayerGpuCacheEntry entry;
        entry.ownerId = ownerId;
        entry.cacheSignature = cacheSignature;
        entry.processedSurface = surface;
        entry.lastFrameNumber = cacheFrameNumber;
        entry.byteSize = static_cast<size_t>(surface.bytesPerLine()) *
                         static_cast<size_t>(std::max(0, surface.height()));
        if (allowSurfaceCache) {
          ArtifactCore::ImageF32x4_RGBA processed;
          if (buildRasterizedSurfaceBuffer(layer, surface, &processed)) {
            entry.processedBuffer = std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);
          }
        }
        if (usesGpuTextureCache) {
          if (entry.processedBuffer) {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                gpuOwnerId, gpuCacheSignature, *entry.processedBuffer);
          } else {
            entry.gpuTextureHandle =
                gpuTextureCacheManager->acquireOrCreate(gpuOwnerId, gpuCacheSignature, surface);
          }
        }
        staticCache[ownerId] = entry;
        staticCacheEntry = &staticCache[ownerId];
      } else {
        staticCacheEntry->lastFrameNumber = cacheFrameNumber;
      }
    }

    if (sceneLights && !sceneLights->empty() && !layer->is3D()) {
      const float lift = std::min(0.18f, 0.03f * static_cast<float>(sceneLights->size()));
      if (lift > 0.0f) {
        QImage lit = surface.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < lit.height(); ++y) {
          QRgb* row = reinterpret_cast<QRgb*>(lit.scanLine(y));
          for (int x = 0; x < lit.width(); ++x) {
            const int r = qRed(row[x]);
            const int g = qGreen(row[x]);
            const int b = qBlue(row[x]);
            const int a = qAlpha(row[x]);
            row[x] = qRgba(std::clamp(static_cast<int>(r + (255 - r) * lift), 0, 255),
                           std::clamp(static_cast<int>(g + (255 - g) * lift), 0, 255),
                           std::clamp(static_cast<int>(b + (255 - b) * lift), 0, 255),
                           a);
          }
        }
        surface = std::move(lit);
      }
    }

    const float baseOpacity = (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
    drawWithClonerEffect(layer, globalTransform4x4,
      [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
        const float finalOpacity = baseOpacity * instanceWeight;

        if (usesGpuTextureCache) {
          auto *textureEntry = staticCacheEntry
                                   ? static_cast<StaticLayerGpuCacheEntry*>(staticCacheEntry)
                                   : nullptr;
          if (textureEntry && !gpuTextureCacheManager->isValid(textureEntry->gpuTextureHandle)) {
            const QImage& uploadSurface =
                textureEntry->processedSurface.isNull() ? surface : textureEntry->processedSurface;
            if (textureEntry->processedBuffer) {
              textureEntry->gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                  gpuOwnerId, gpuCacheSignature, *textureEntry->processedBuffer);
            } else {
              textureEntry->gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                  gpuOwnerId, gpuCacheSignature, uploadSurface);
            }
          }
          const auto binding = gpuTextureCacheManager->bindingRecord(
              textureEntry ? textureEntry->gpuTextureHandle : GPUTextureCacheHandle{});
          if (binding.isValid()) {
            renderer->drawSpriteTransformed(static_cast<float>(rect.x()),
                                 static_cast<float>(rect.y()),
                                 static_cast<float>(rect.width()),
                                 static_cast<float>(rect.height()),
                                 instanceTransform,
                                 binding.srv,
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

    if (auto* solid2D = dynamic_cast<ArtifactSolid2DLayer*>(layer)) {
    const auto color = solid2D->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
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
            [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
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
          [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
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

  if (auto* solidImage = dynamic_cast<ArtifactSolidImageLayer*>(layer)) {
    const auto color = solidImage->color();
    const bool gradientEnabled = solidImage->isGradientEnabled();
    if (hasRasterizerEffectsOrMasks(layer)) {
      QImage surface = gradientEnabled
                           ? solidImage->toQImage()
                           : QImage(std::max(1, static_cast<int>(std::ceil(localRect.width()))),
                                    std::max(1, static_cast<int>(std::ceil(localRect.height()))),
                                    QImage::Format_ARGB32_Premultiplied);
      if (!gradientEnabled) {
        surface.fill(toQColor(color));
      }
      applySurfaceAndDraw(surface, localRect, true);
    } else if (gradientEnabled) {
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
            renderer->drawGradientRectTransformed(
                static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()), static_cast<float>(localRect.height()),
                instanceTransform, solidImage->gradientStartColor(), solidImage->gradientEndColor(),
                static_cast<int>(solidImage->fillType()), solidImage->gradientAngleDegrees(),
                solidImage->gradientReverse(), solidImage->gradientCenterX(), solidImage->gradientCenterY(),
                solidImage->gradientScale(), solidImage->gradientOffset(), baseOpacity * instanceWeight);
          });
    } else {
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
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

  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer) && imageLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA& buffer = imageLayer->currentFrameBuffer();
      const float baseOpacity = (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(layer, globalTransform4x4,
        [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
          renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                               static_cast<float>(localRect.y()),
                               static_cast<float>(localRect.width()),
                               static_cast<float>(localRect.height()),
                               instanceTransform,
                               buffer,
                               baseOpacity * instanceWeight);
        });
      return;
    }

    const QImage img = downsampleForLOD(imageLayer->toQImage(), lod);
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (auto* svgLayer = dynamic_cast<ArtifactSvgLayer*>(layer)) {
    if (svgLayer->isLoaded()) {
      if (!hasRasterizerEffectsOrMasks(layer) &&
          svgLayer->hasCurrentFrameBuffer()) {
        const ArtifactCore::ImageF32x4_RGBA& buffer =
            svgLayer->currentFrameBuffer();
        const float baseOpacity =
            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
        drawWithClonerEffect(layer, globalTransform4x4,
          [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                 static_cast<float>(localRect.y()),
                                 static_cast<float>(localRect.width()),
                                 static_cast<float>(localRect.height()),
                                 instanceTransform,
                                 buffer,
                                 baseOpacity * instanceWeight);
          });
      } else {
        const QImage svgImage = svgLayer->toQImage();
        if (!svgImage.isNull()) {
          applySurfaceAndDraw(svgImage, localRect, hasRasterizerEffectsOrMasks(layer));
        } else {
          svgLayer->draw(renderer);
        }
      }
      return;
    }
  }

  if (auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer)) {
    const bool hasRasterizer = hasRasterizerEffectsOrMasks(layer);
    const bool hasBuffer = videoLayer->hasCurrentFrameBuffer();
    const bool loaded = videoLayer->isLoaded();
    const bool active =
        layer->isActiveAt(FramePosition(static_cast<int>(layer->currentFrame())));
    const FramePosition ip = layer->inPoint();
    const FramePosition op = layer->outPoint();
    const int64_t targetFrame =
        cacheFrameNumber >= 0 ? cacheFrameNumber : layer->currentFrame();
    if (!hasRasterizer && !offlineRender) {
      const ArtifactCore::ImageF32x4_RGBA buffer =
          videoLayer->cachedFrameImageBuffer(targetFrame);
      if (!buffer.isEmpty()) {
        const float baseOpacity =
            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
        if (videoDebugOut) {
          *videoDebugOut = QStringLiteral(
                               "[Video] branch=buffer loaded=%1 hasBuffer=%2 "
                               "rasterizer=%3 active=%4 range=[%5,%6] curFrame=%7")
                               .arg(loaded)
                               .arg(hasBuffer)
                               .arg(hasRasterizer)
                               .arg(active)
                               .arg(ip.framePosition())
                               .arg(op.framePosition())
                               .arg(layer->currentFrame());
        }
        drawWithClonerEffect(layer, globalTransform4x4,
          [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                 static_cast<float>(localRect.y()),
                                 static_cast<float>(localRect.width()),
                                 static_cast<float>(localRect.height()),
                                 instanceTransform,
                                 buffer,
                                 baseOpacity * instanceWeight);
        });
        return;
      }
    }

    ArtifactCore::ImageF32x4_RGBA frameBuffer;
    bool usedSyncFallback = false;
    bool usedBufferFallback = false;
    QString reason;
    if (!hasRasterizer && gpuTextureCacheManager && !offlineRender) {
      const ArtifactCore::GpuVideoFrame gpuFrame =
          videoLayer->decodeFrameToGpuFrame(targetFrame);
      if (gpuFrame.isValid()) {
        QString gpuOwnerId = layer->id().toString();
        QString gpuCacheSignature = QStringLiteral("video-gpu:%1").arg(targetFrame);
        const QUuid sourceAssetId = videoLayer->sourceAssetId();
        if (!sourceAssetId.isNull()) {
          const auto sourceVersion = ArtifactCore::AssetManager::instance().sourceVersion(sourceAssetId);
          gpuOwnerId = QStringLiteral("asset:%1").arg(
              sourceAssetId.toString(QUuid::WithoutBraces));
          gpuCacheSignature = QStringLiteral("video-gpu:v%1:f%2")
                                  .arg(sourceVersion)
                                  .arg(targetFrame);
        }
        const auto handle = gpuTextureCacheManager->acquireOrCreate(
            gpuOwnerId, gpuCacheSignature, gpuFrame);
        const auto binding = gpuTextureCacheManager->bindingRecord(handle);
        if (binding.isValid()) {
          const float baseOpacity =
              (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
          if (videoDebugOut) {
            const QSize source = QSize(std::max(0, layer->sourceSize().width),
                                       std::max(0, layer->sourceSize().height));
            *videoDebugOut = QString("[Video] branch=gpu-frame loaded=%1 "
                                     "size=%2x%3 hasBuffer=%4 rasterizer=%5 "
                                     "active=%6 range=[%7,%8] curFrame=%9")
                                 .arg(loaded)
                                 .arg(source.width())
                                 .arg(source.height())
                                 .arg(hasBuffer)
                                 .arg(hasRasterizer)
                                 .arg(active)
                                 .arg(ip.framePosition())
                                 .arg(op.framePosition())
                                 .arg(layer->currentFrame());
          }
          drawWithClonerEffect(layer, globalTransform4x4,
            [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
              renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                                   static_cast<float>(localRect.y()),
                                   static_cast<float>(localRect.width()),
                                   static_cast<float>(localRect.height()),
                                   instanceTransform,
                                   binding.srv,
                                   baseOpacity * instanceWeight);
          });
          return;
        }
      }
    }
    if (loaded) {
      frameBuffer = offlineRender
          ? videoLayer->decodeFrameToImageBuffer(static_cast<double>(targetFrame))
          : videoLayer->cachedFrameImageBuffer(targetFrame);
      usedBufferFallback = !frameBuffer.isEmpty();
    } else {
      reason = QStringLiteral("notLoaded");
    }
    if (frameBuffer.isEmpty() && loaded) {
      frameBuffer = videoLayer->decodeFrameToImageBuffer(static_cast<double>(targetFrame));
      usedSyncFallback = !frameBuffer.isEmpty();
    }
    if (!frameBuffer.isEmpty()) {
      frameBuffer = downsampleForLOD(frameBuffer, lod);
    }
    if (videoDebugOut) {
      if (reason.isEmpty()) {
        if (!loaded) {
          reason = QStringLiteral("notLoaded");
        } else if (usedSyncFallback) {
          reason = QStringLiteral("syncDecode");
        } else if (usedBufferFallback) {
          reason = QStringLiteral("bufferFallback");
        } else if (frameBuffer.isEmpty()) {
          reason = hasBuffer ? QStringLiteral("decodeNull")
                             : QStringLiteral("noBuffer");
        } else {
          reason = QStringLiteral("ok");
        }
      }
      const QSize source = QSize(std::max(0, layer->sourceSize().width), std::max(0, layer->sourceSize().height));
      *videoDebugOut = QString("[Video] branch=preview loaded=%1 size=%2x%3 "
                               "hasBuffer=%4 rasterizer=%5 active=%6 "
                               "syncFallback=%7 bufferFallback=%8 reason=%9 "
                               "range=[%10,%11] curFrame=%12")
        .arg(loaded)
        .arg(source.width())
        .arg(source.height())
        .arg(hasBuffer)
        .arg(hasRasterizer)
        .arg(active)
        .arg(usedSyncFallback)
        .arg(usedBufferFallback)
        .arg(reason)
        .arg(ip.framePosition())
        .arg(op.framePosition())
        .arg(layer->currentFrame());
    }
    if (!frameBuffer.isEmpty()) {
      if (hasRasterizer) {
        applyRasterizerEffectsAndMasksToSurface(layer, frameBuffer, lod);
      }
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(layer, globalTransform4x4,
        [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
          renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                               static_cast<float>(localRect.y()),
                               static_cast<float>(localRect.width()),
                               static_cast<float>(localRect.height()),
                               instanceTransform,
                               frameBuffer,
                               baseOpacity * instanceWeight);
      });
      return;
    }
  }

  if (auto* textLayer = dynamic_cast<ArtifactTextLayer*>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer)) {
      textLayer->draw(renderer);
      return;
    }
    if (textLayer->hasCurrentFrameBuffer()) {
      ArtifactCore::ImageF32x4_RGBA buffer =
          textLayer->currentFrameBuffer().DeepCopy();
      applyRasterizerEffectsAndMasksToSurface(layer, buffer, lod);
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(layer, globalTransform4x4,
        [&](const QMatrix4x4& instanceTransform, float instanceWeight) {
          renderer->drawSpriteTransformed(static_cast<float>(localRect.x()),
                               static_cast<float>(localRect.y()),
                               static_cast<float>(localRect.width()),
                               static_cast<float>(localRect.height()),
                               instanceTransform,
                               buffer,
                               baseOpacity * instanceWeight);
        });
      return;
    }
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect, true);
    }
    return;
  }

  if (auto* compLayer = dynamic_cast<ArtifactCompositionLayer*>(layer)) {
    if (auto childComp = compLayer->sourceComposition()) {
      const QSize childSize = childComp->settings().compositionSize();
      // Map the parent timeline frame into the child composition's local time.
      // When time remap is enabled on the precomp layer this applies the remap
      // curve; otherwise it falls back to the layer's startTime/inPoint offset
      // so the child is sampled at the right moment rather than always frame 0.
      const int64_t parentFrame =
          (cacheFrameNumber != std::numeric_limits<int64_t>::min())
              ? cacheFrameNumber
              : layer->currentFrame();
      const double mappedFrameD =
          compLayer->getSourceFrameAtCompFrame(parentFrame);
      const int64_t childFrame = static_cast<int64_t>(std::llround(mappedFrameD));
      QImage childImage = childComp->getThumbnailAtFrame(
          childFrame, childSize.width(), childSize.height());

      if (!childImage.isNull()) {
        applySurfaceAndDraw(childImage, localRect, hasRasterizerEffectsOrMasks(layer));
      }
    }
    return;
  }

  if (auto* particleLayer = dynamic_cast<ArtifactParticleLayer*>(layer)) {
    const int64_t targetFrame =
        (cacheFrameNumber != std::numeric_limits<int64_t>::min())
            ? cacheFrameNumber
            : layer->currentFrame();
    const bool hasRasterizer = hasRasterizerEffectsOrMasks(layer);
    if (renderer && renderer->isInitialized() && !hasRasterizer) {
      particleLayer->goToFrame(targetFrame);
      particleLayer->draw(renderer);
      return;
    }

    const QSize surfaceSize(
        std::max(1, static_cast<int>(std::ceil(localRect.width()))),
        std::max(1, static_cast<int>(std::ceil(localRect.height()))));
    particleLayer->goToFrame(targetFrame);
    QImage particleSurface;
    const bool cacheHit =
        particleLayer->getCachedFrame(targetFrame, particleSurface) &&
        particleSurface.size() == surfaceSize;
    if (!cacheHit) {
      float fps = 30.0f;
      if (auto* comp =
              static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        fps = comp->frameRate().framerate();
      }
      particleSurface = particleLayer->renderFrame(
          surfaceSize.width(), surfaceSize.height(),
          static_cast<float>(targetFrame) / std::max(0.001f, fps));
    }
    particleSurface = downsampleForLOD(particleSurface, lod);
    if (!particleSurface.isNull() &&
        particleSurface.format() != QImage::Format_ARGB32_Premultiplied) {
      particleSurface =
          particleSurface.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    if (!particleSurface.isNull()) {
      applySurfaceAndDraw(particleSurface, localRect, true);
      return;
    }
  }

  if (auto* formParticleLayer = dynamic_cast<ArtifactFormParticleLayer*>(layer)) {
    if (renderer && renderer->isInitialized()) {
      formParticleLayer->draw(renderer);
    }
    return;
  }

  if (layer->isAdjustmentLayer()) {
    // 調整レイヤー: 背面の画像をキャプチャしてエフェクトを適用する
    // GPUパスの場合は既に controller 側で background がコピーされている前提。
    // readbackToImage は現在の描画ターゲット（RTV）の内容を QImage として取得する。
    QImage background = renderer->readbackToImage();
    if (!background.isNull()) {
      applySurfaceAndDraw(background, localRect, true);
    }
    return;
  }

  layer->draw(renderer);
}

} // namespace Artifact
