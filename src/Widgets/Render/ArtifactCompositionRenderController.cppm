module;
#include <DeviceContext.h>
#define NOMINMAX
#define QT_NO_KEYWORDS
#include <opencv2/opencv.hpp>

#include <QDebug>
#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QTransform>
#include <QRectF>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QByteArray>
#include <limits>
#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>
#include <utility>
#include <wobjectimpl.h>
#include <Layer/ArtifactCloneEffectSupport.hpp>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.Config;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Frame.Position;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Tool.Manager;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Utils.Id;
import Artifact.Render.Pipeline;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;

import Artifact.Service.Project;
import Artifact.Service.Playback; // 追加
import Undo.UndoManager;
import Frame.Position;
import Color.Float;
import Image;
import CvUtils;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

namespace {
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

enum class SelectionMode {
  Replace,
  Add,
  Toggle
};

enum class LayerDragMode {
  None,
  Move,
  ScaleTL,
  ScaleTR,
  ScaleBL,
  ScaleBR
};

struct LayerSurfaceCacheEntry
{
  QString ownerId;
  QString cacheSignature;
  QImage processedSurface;
  GPUTextureCacheHandle gpuTextureHandle;
  int64_t frameNumber = std::numeric_limits<int64_t>::min();
};

QColor toQColor(const FloatColor& color)
{
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

template <typename ColorT>
QString colorKey(const ColorT& color)
{
  return QStringLiteral("%1,%2,%3,%4")
      .arg(color.r(), 0, 'f', 4)
      .arg(color.g(), 0, 'f', 4)
      .arg(color.b(), 0, 'f', 4)
      .arg(color.a(), 0, 'f', 4);
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
               .arg(colorKey(solid2D->color()))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (const auto solidImage = std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral("|solidImage|color=%1|bounds=%2x%3")
               .arg(colorKey(solidImage->color()))
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
               .arg(colorKey(textLayer->textColor()))
               .arg(textLayer->isStrokeEnabled() ? 1 : 0)
               .arg(colorKey(textLayer->strokeColor()))
               .arg(textLayer->strokeWidth(), 0, 'f', 2)
               .arg(textLayer->isShadowEnabled() ? 1 : 0)
               .arg(colorKey(textLayer->shadowColor()))
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

bool layerHasCpuRasterizerWork(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }
  if (layer->hasMasks()) {
    return true;
  }
  for (const auto& effect : layer->getEffects()) {
    if (effect && effect->isEnabled() &&
        effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
      return true;
    }
  }
  return false;
}

bool layerUsesSurfaceUploadForCompositionView(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }
  if (layerHasCpuRasterizerWork(layer)) {
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

  // 動画は毎フレーム別内容になりやすく、GPU texture cache を汚しやすいので除外する。
  if (std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    return false;
  }

  return std::dynamic_pointer_cast<ArtifactImageLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSvgLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactTextLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSolid2DLayer>(layer) != nullptr ||
         std::dynamic_pointer_cast<ArtifactSolidImageLayer>(layer) != nullptr;
}

QRectF viewportRectToCanvasRect(ArtifactIRenderer* renderer,
                               const QPointF& startViewportPos,
                               const QPointF& endViewportPos)
{
  if (!renderer) {
    return QRectF();
  }

  const auto a = renderer->viewportToCanvas(
      {static_cast<float>(startViewportPos.x()), static_cast<float>(startViewportPos.y())});
  const auto b = renderer->viewportToCanvas(
      {static_cast<float>(endViewportPos.x()), static_cast<float>(endViewportPos.y())});
  return QRectF(QPointF(std::min(a.x, b.x), std::min(a.y, b.y)),
                QPointF(std::max(a.x, b.x), std::max(a.y, b.y)));
}

SelectionMode selectionModeFromModifiers(Qt::KeyboardModifiers modifiers)
{
  if (modifiers.testFlag(Qt::ControlModifier)) {
    return SelectionMode::Toggle;
  }
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    return SelectionMode::Add;
  }
  return SelectionMode::Replace;
}

QStringList selectedLayerIdList()
{
  QStringList ids;
  auto* app = ArtifactApplicationManager::instance();
  auto* selection = app ? app->layerSelectionManager() : nullptr;
  if (!selection) {
    return ids;
  }

  for (const auto& layer : selection->selectedLayers()) {
    if (layer) {
      ids.push_back(layer->id().toString());
    }
  }
  return ids;
}

bool isLayerSelected(const QStringList& selectedIds, const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return false;
  }
  const QString layerId = layer->id().toString();
  for (const auto& selectedId : selectedIds) {
    if (selectedId == layerId) {
      return true;
    }
  }
  return false;
}

struct MotionPathSample {
  QPointF position;
  bool keyframe = false;
};

// Forward declaration
FramePosition currentFrameForComposition(const ArtifactCompositionPtr& comp);

QVector<MotionPathSample> buildMotionPathSamples(const ArtifactAbstractLayerPtr& layer,
                                                 const ArtifactCompositionPtr& comp)
{
  QVector<MotionPathSample> samples;
  if (!layer || !comp) {
    return samples;
  }

  const auto keyTimes = layer->transform3D().getPositionKeyFrameTimes();
  if (keyTimes.empty()) {
    return samples;
  }

  samples.reserve(static_cast<int>(keyTimes.size()) + 1);
  const int fps = std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));

  for (const auto& time : keyTimes) {
    samples.push_back({
      QPointF(layer->transform3D().positionXAt(time),
              layer->transform3D().positionYAt(time)),
      true
    });
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const RationalTime currentTime(currentFrame.framePosition(), fps);
  samples.push_back({
    QPointF(layer->transform3D().positionXAt(currentTime),
            layer->transform3D().positionYAt(currentTime)),
    false
  });

  return samples;
}

LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr& layer,
                                   const QPointF& viewportPos,
                                   ArtifactIRenderer* renderer)
{
  if (!layer || !renderer) {
    return LayerDragMode::None;
  }

  const QRectF bbox = layer->transformedBoundingBox();
  if (!bbox.isValid() || bbox.width() <= 0.0 || bbox.height() <= 0.0) {
    return LayerDragMode::None;
  }

  constexpr float kHandleHitSize = 16.0f;
  const auto containsHandle = [&](float x, float y) {
    const auto p = renderer->canvasToViewport({x, y});
    const QRectF rect(p.x - kHandleHitSize * 0.5f, p.y - kHandleHitSize * 0.5f,
                      kHandleHitSize, kHandleHitSize);
    return rect.contains(viewportPos);
  };

  if (containsHandle(static_cast<float>(bbox.left()), static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTL;
  }
  if (containsHandle(static_cast<float>(bbox.right()), static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTR;
  }
  if (containsHandle(static_cast<float>(bbox.left()), static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBL;
  }
  if (containsHandle(static_cast<float>(bbox.right()), static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBR;
  }

  return LayerDragMode::Move;
}

bool layerIntersectsCanvasRect(const ArtifactAbstractLayerPtr& layer,
                               const QRectF& rect,
                               const FramePosition& currentFrame)
{
  if (!layer || !rect.isValid() || !layer->isVisible() || !layer->isActiveAt(currentFrame)) {
    return false;
  }

  const QRectF bbox = layer->transformedBoundingBox();
  if (!bbox.isValid()) {
    return false;
  }

  return bbox.intersects(rect);
}

ArtifactCompositionPtr
resolvePreferredComposition(ArtifactProjectService *service) {
  // ProjectService を最優先
  if (service) {
    if (auto comp = service->currentComposition().lock()) {
      return comp;
    }
  }

  // フォールバック: ActiveContextService
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      if (auto comp = active->activeComposition()) {
        return comp;
      }
    }
  }

  // フォールバック: PlaybackService
  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (auto comp = playback->currentComposition()) {
      return comp;
    }
  }

  return ArtifactCompositionPtr();
}

FramePosition currentFrameForComposition(const ArtifactCompositionPtr& comp)
{
  if (!comp) {
    return FramePosition(0);
  }
  FramePosition currentFrame = comp->framePosition();
  if (auto* playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }
  return currentFrame;
}

ArtifactAbstractLayerPtr hitTopmostLayerAtViewportPos(const ArtifactCompositionPtr& comp,
                                                      ArtifactIRenderer* renderer,
                                                      const QPointF& viewportPos)
{
  if (!comp || !renderer) {
    return ArtifactAbstractLayerPtr();
  }

  const auto currentFrame = currentFrameForComposition(comp);
  const auto canvasPos =
      renderer->viewportToCanvas({static_cast<float>(viewportPos.x()),
                                  static_cast<float>(viewportPos.y())});
  const auto layers = comp->allLayer();
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
    const auto& layer = layers[static_cast<size_t>(i)];
    if (!layer || !layer->isVisible() || !layer->isActiveAt(currentFrame)) {
      continue;
    }

    const QTransform globalTransform = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);
    if (invertible) {
      const QPointF localPos = invTransform.map(QPointF(canvasPos.x, canvasPos.y));
      if (layer->localBounds().contains(localPos)) {
        return layer;
      }
      continue;
    }

    const QRectF bbox = layer->transformedBoundingBox();
    if (bbox.contains(canvasPos.x, canvasPos.y)) {
      return layer;
    }
  }

  return ArtifactAbstractLayerPtr();
}

void drawLayerForCompositionView(const ArtifactAbstractLayerPtr &layer,
                                 ArtifactIRenderer *renderer,
                                 float opacityOverride = -1.0f,
                                 QString* videoDebugOut = nullptr,
                                 QHash<QString, LayerSurfaceCacheEntry>* surfaceCache = nullptr,
                                 GPUTextureCacheManager* gpuTextureCacheManager = nullptr,
                                 int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min()) {
  if (!layer || !renderer) {
    qCDebug(compositionViewLog) << "[CompositionView] drawLayerForCompositionView: invalid "
                "layer/renderer";
    return;
  }

  const QRectF localRect = layer->localBounds();
  if (!localRect.isValid() || localRect.width() <= 0.0 ||
      localRect.height() <= 0.0) {
    qCDebug(compositionViewLog) << "[CompositionView] skip layer: invalid local bounds"
             << "id=" << layer->id().toString() << "rect=" << localRect;
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const QMatrix4x4 globalTransform4x4 = layer->getGlobalTransform4x4();

  auto applyRasterizerEffectsAndMasksToSurface = [&](const ArtifactAbstractLayerPtr& targetLayer,
                                            QImage& surface) {
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

    // Convert to float mat for processing
    cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
    if (mat.type() != CV_32FC4) {
        mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
    }

    // Apply Effects
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

    // Apply Masks
    if (hasMasks) {
        for (int m = 0; m < targetLayer->maskCount(); ++m) {
            LayerMask mask = targetLayer->mask(m);
            mask.applyToImage(mat.cols, mat.rows, &mat);
        }
    }

    surface = ArtifactCore::CvUtils::cvMatToQImage(mat);
  };

  auto hasRasterizerEffectsOrMasks = [](const ArtifactAbstractLayerPtr& targetLayer) {
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
  };

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
    const QImage img = imageLayer->toQImage();
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
    const QImage frame = videoLayer->currentFrameToQImage();
    // デバッグ文字列生成は デバッグカテゴリ有効時のみ実行（毎フレームのコスト削減）
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
      return;
    }
    return;
  }

  // Fallback for layer types without a direct surface accessor.
  qCDebug(compositionViewLog) << "[CompositionView] fallback layer draw"
           << "id=" << layer->id().toString()
           << "type=" << layer->type_index().name();
  layer->draw(renderer);
}

} // namespace

class CompositionRenderController::Impl {
public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  std::unique_ptr<CompositionRenderer> compositionRenderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  std::unique_ptr<TransformGizmo> gizmo_;
  std::unique_ptr<Artifact3DGizmo> gizmo3D_;
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;
  RenderPipeline renderPipeline_;
  QTimer *renderTimer_ = nullptr;
  bool initialized_ = false;
  bool running_ = false;
  bool renderScheduled_ = false;
  bool renderInProgress_ = false;
  bool renderRescheduleRequested_ = false;
  bool blendPipelineReady_ = false;
  bool gpuBlendEnabled_ =
      !qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
  QString lastVideoDebug_;
  QString lastEmittedVideoDebug_;
  QVector<QMetaObject::Connection> layerChangedConnections_;
  QMetaObject::Connection compositionChangedConnection_;

  LayerID selectedLayerId_;
  bool isDraggingLayer_ = false;
  LayerDragMode dragMode_ = LayerDragMode::None;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  QRectF dragStartBoundingBox_;
  int64_t dragFrame_ = 0;
  QPointF dragAppliedDelta_;
  bool showGrid_ = false;
  bool showCheckerboard_ = false;
  bool showGuides_ = false;
  bool showSafeMargins_ = false;
  bool showMotionPathOverlay_ = true;
  bool showFrameInfo_ = false; // Changed to false by default
  int currentFrameForOverlay_ = 0;
  quint64 renderFrameCounter_ = 0;
  int lastPipelineStateMask_ = -1;
  QSize lastDispatchWarningSize_;
  QByteArray lastFinalPresentKey_;
  quint64 baseInvalidationSerial_ = 1;
  quint64 overlayInvalidationSerial_ = 1;
  FloatColor clearColor_ = {0.12f, 0.13f, 0.18f, 1.0f};
  QHash<QString, LayerSurfaceCacheEntry> surfaceCache_;
  std::unique_ptr<GPUTextureCacheManager> gpuTextureCacheManager_;

  // Guide positions (composition-space pixels)
  QVector<float> guideVerticals_;   // X positions
  QVector<float> guideHorizontals_; // Y positions
  float lastCanvasWidth_ = 1920.0f;
  float lastCanvasHeight_ = 1080.0f;

  // Resolution scaling
  int previewDownsample_ = 1;
  int interactivePreviewDownsampleFloor_ = 2;
  float hostWidth_ = 0.0f;
  float hostHeight_ = 0.0f;
  QPointer<QWidget> hostWidget_;
  bool viewportInteracting_ = false;
  QTimer *viewportInteractionTimer_ = nullptr;
  QTimer *resizeDebounceTimer_ = nullptr;
  QSize pendingResizeSize_;
  bool isRubberBandSelecting_ = false;
  bool dragGroupMove_ = false;
  QPointF rubberBandStartViewportPos_;
  QPointF rubberBandCurrentViewportPos_;
  SelectionMode selectionMode_ = SelectionMode::Replace;
  QVector<ArtifactAbstractLayerPtr> dragGroupLayers_;
  QHash<QString, QPointF> dragGroupStartPositions_;

  // Mask editing state
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  bool isDraggingVertex_ = false;
  bool maskEditPending_ = false;
  bool maskEditDirty_ = false;
  ArtifactAbstractLayerWeak maskEditLayer_;
  std::vector<LayerMask> maskEditBefore_;

  // Cyclic selection state
  QPointF lastHitPosition_;
  LayerID lastHitLayerId_;

  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer) {
    if (!layer) {
      return;
    }
    maskEditPending_ = true;
    maskEditDirty_ = false;
    maskEditLayer_ = layer;
    maskEditBefore_.clear();
    maskEditBefore_.reserve(static_cast<size_t>(layer->maskCount()));
    for (int i = 0; i < layer->maskCount(); ++i) {
      maskEditBefore_.push_back(layer->mask(i));
    }
  }

  void markMaskEditDirty() {
    if (maskEditPending_) {
      maskEditDirty_ = true;
    }
  }

  void commitMaskEditTransaction() {
    if (!maskEditPending_) {
      return;
    }

    auto layer = maskEditLayer_.lock();
    maskEditPending_ = false;
    maskEditLayer_.reset();

    if (!layer || !maskEditDirty_) {
      maskEditBefore_.clear();
      maskEditDirty_ = false;
      return;
    }

    std::vector<LayerMask> afterMasks;
    afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int i = 0; i < layer->maskCount(); ++i) {
      afterMasks.push_back(layer->mask(i));
    }

    if (auto* undo = UndoManager::instance()) {
      undo->push(std::make_unique<MaskEditCommand>(layer, maskEditBefore_, std::move(afterMasks)));
    }

    maskEditBefore_.clear();
    maskEditDirty_ = false;
  }

  void syncSelectedLayerOverlayState(const ArtifactCompositionPtr& composition) {
    ArtifactAbstractLayerPtr layer;
    if (composition && !selectedLayerId_.isNil()) {
      layer = composition->layerById(selectedLayerId_);
    }

    if (gizmo_) {
      gizmo_->setLayer(layer);
    }

    if (gizmo3D_ && layer) {
      syncGizmo3DFromLayer(layer);
    }
  }

  void syncGizmo3DFromLayer(const ArtifactAbstractLayerPtr& layer) {
    if (!gizmo3D_ || !layer) {
      return;
    }

    if (layer->is3D()) {
      const auto& t3 = layer->transform3D();
      gizmo3D_->setDepthEnabled(true);
      gizmo3D_->setTransform(layer->position3D(), layer->rotation3D());
      gizmo3D_->setScale(QVector3D(t3.scaleX(), t3.scaleY(), 1.0f));
      return;
    }

    const QRectF localRect = layer->localBounds();
    const QTransform globalTransform = layer->getGlobalTransform();
    const QPointF center =
        localRect.isValid() ? globalTransform.map(localRect.center())
                            : QPointF(globalTransform.dx(), globalTransform.dy());
    const float scaleX = std::max<float>(0.01f,
                                         static_cast<float>(std::hypot(globalTransform.m11(), globalTransform.m12())));
    const float scaleY = std::max<float>(0.01f,
                                         static_cast<float>(std::hypot(globalTransform.m21(), globalTransform.m22())));
    const float rotationZ =
        std::atan2(globalTransform.m12(), globalTransform.m11()) * (180.0f / 3.14159265358979323846f);

    gizmo3D_->setDepthEnabled(false);
    gizmo3D_->setTransform(QVector3D(static_cast<float>(center.x()),
                                     static_cast<float>(center.y()),
                                     0.0f),
                           QVector3D(0.0f, 0.0f, rotationZ));
    gizmo3D_->setScale(QVector3D(scaleX, scaleY, 1.0f));
  }

  QRectF rubberBandCanvasRect() const {
    return viewportRectToCanvasRect(renderer_.get(),
                                    rubberBandStartViewportPos_,
                                    rubberBandCurrentViewportPos_);
  }

  void clearSelectionGestureState() {
    isDraggingLayer_ = false;
    isRubberBandSelecting_ = false;
    dragGroupMove_ = false;
    dragGroupLayers_.clear();
    dragGroupStartPositions_.clear();
  }

  void applyCompositionState(const ArtifactCompositionPtr &composition) {
    if (!renderer_ || !composition) {
      return;
    }

    const auto size = composition->settings().compositionSize();
    const float cw = static_cast<float>(size.width() > 0 ? size.width() : 1920);
    const float ch =
        static_cast<float>(size.height() > 0 ? size.height() : 1080);
    lastCanvasWidth_ = cw;
    lastCanvasHeight_ = ch;
    if (compositionRenderer_) {
      compositionRenderer_->SetCompositionSize(cw, ch);
      compositionRenderer_->ApplyCompositionSpace();
      renderer_->setCanvasSize(cw, ch);
    } else {
      renderer_->setCanvasSize(cw, ch);
    }

    // レンダーパイプラインの中間テクスチャを初期化
    if (auto device = renderer_->device()) {
      renderPipeline_.initialize(device, static_cast<Uint32>(cw), static_cast<Uint32>(ch),
                                 RenderConfig::PipelineFormat);
    }
  }

  void bindCompositionChanged(CompositionRenderController *owner,
                              const ArtifactCompositionPtr &composition) {
    if (compositionChangedConnection_) {
      QObject::disconnect(compositionChangedConnection_);
      compositionChangedConnection_ = {};
    }
    if (!owner || !composition) {
      return;
    }

    compositionChangedConnection_ = QObject::connect(
        composition.get(), &ArtifactAbstractComposition::changed, owner,
        [this, owner, composition]() {
          surfaceCache_.clear();
          if (gpuTextureCacheManager_) {
            gpuTextureCacheManager_->clear();
          }
          invalidateBaseComposite();
          applyCompositionState(composition);
          owner->renderOneFrame();
        });
  }

  void invalidateLayerSurfaceCache(const ArtifactAbstractLayerPtr& layer) {
    if (!layer) {
      return;
    }
    const QString ownerId = layer->id().toString();
    surfaceCache_.remove(ownerId);
    if (gpuTextureCacheManager_) {
      gpuTextureCacheManager_->invalidateOwner(ownerId);
    }
  }

  void invalidateBaseComposite() {
    ++baseInvalidationSerial_;
    lastFinalPresentKey_.clear();
  }

  void invalidateOverlayComposite() {
    ++overlayInvalidationSerial_;
    lastFinalPresentKey_.clear();
  }

  void renderOneFrameImpl(CompositionRenderController *owner);
};

CompositionRenderController::CompositionRenderController(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  impl_->gizmo_ = std::make_unique<TransformGizmo>();
  impl_->gizmo3D_ = std::make_unique<Artifact3DGizmo>(this);

  // Connect to project service to track layer selection
  if (auto *svc = ArtifactProjectService::instance()) {
    connect(svc, &ArtifactProjectService::layerSelected, this,
            [this](const LayerID &id) {
              setSelectedLayerId(id);
            });

    // Always follow the active composition even if upstream wiring misses one
    // path.
    connect(svc, &ArtifactProjectService::currentCompositionChanged, this,
            [this, svc](const CompositionID &id) {
              ArtifactCompositionPtr comp;
              if (!id.isNil()) {
                const auto found = svc->findComposition(id);
                if (found.success && !found.ptr.expired()) {
                  comp = found.ptr.lock();
                }
              }
              if (!comp) {
                comp = resolvePreferredComposition(svc);
              }
              setComposition(comp);
            });

    // Project-level mutations can replace composition/layer instances; resync
    // aggressively.
    connect(svc, &ArtifactProjectService::projectChanged, this, [this, svc]() {
      auto latest = resolvePreferredComposition(svc);
      auto current = impl_->previewPipeline_.composition();
      if (latest != current) {
        setComposition(latest);
      } else {
        impl_->invalidateBaseComposite();
      }
    });

    // Ensure layers created after setComposition() are also bound to redraw.
    connect(svc, &ArtifactProjectService::layerCreated, this,
            [this](const CompositionID &compId, const LayerID &layerId) {
              auto comp = impl_->previewPipeline_.composition();
              if (!comp || comp->id() != compId) {
                return;
              }
              if (auto layer = comp->layerById(layerId)) {
                impl_->layerChangedConnections_.push_back(
                    connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                            [this, layer]() {
                              impl_->invalidateLayerSurfaceCache(layer);
                              impl_->invalidateBaseComposite();
                              impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());
                              renderOneFrame();
                            }));
              }
            });

    // Handle resolution changes
    connect(svc, &ArtifactProjectService::previewQualityPresetChanged, this,
            &CompositionRenderController::setPreviewQualityPreset);
    
    // Initial sync
    setPreviewQualityPreset(svc->previewQualityPreset());
  }
}

CompositionRenderController::~CompositionRenderController() {
  destroy();
  delete impl_;
}

void CompositionRenderController::initialize(QWidget *hostWidget) {
  if (impl_->initialized_ || hostWidget == nullptr) {
    return;
  }

  impl_->hostWidget_ = hostWidget;
  impl_->renderer_ = std::make_unique<ArtifactIRenderer>();
  impl_->renderer_->initialize(hostWidget);

  if (!impl_->renderer_->isInitialized()) {
    qWarning() << "[CompositionRenderController] renderer initialize failed for"
               << hostWidget << "size=" << hostWidget->size()
               << "DPR=" << hostWidget->devicePixelRatio();
    impl_->renderer_.reset();
    return;
  }
  impl_->compositionRenderer_ =
      std::make_unique<CompositionRenderer>(*impl_->renderer_);
  impl_->renderer_->setClearColor(impl_->clearColor_);
  impl_->hostWidth_ = static_cast<float>(hostWidget->width());
  impl_->hostHeight_ = static_cast<float>(hostWidget->height());
  impl_->renderer_->setViewportSize((float)hostWidget->width(),
                                    (float)hostWidget->height());

  const auto comp = impl_->previewPipeline_.composition();
  if (comp) {
    impl_->applyCompositionState(comp);
    impl_->renderer_->fitToViewport();
  }

  impl_->renderTimer_ = new QTimer(this);
  impl_->renderTimer_->setTimerType(Qt::PreciseTimer);
  connect(impl_->renderTimer_, &QTimer::timeout, this,
          &CompositionRenderController::renderOneFrame);

  impl_->viewportInteractionTimer_ = new QTimer(this);
  impl_->viewportInteractionTimer_->setSingleShot(true);
  connect(impl_->viewportInteractionTimer_, &QTimer::timeout, this,
          &CompositionRenderController::finishViewportInteraction);

  impl_->resizeDebounceTimer_ = new QTimer(this);
  impl_->resizeDebounceTimer_->setSingleShot(true);
  connect(impl_->resizeDebounceTimer_, &QTimer::timeout, this, [this]() {
    if (!impl_->renderer_ || !impl_->hostWidget_.data()) {
      return;
    }
    const QSize pending = impl_->pendingResizeSize_;
    if (pending.isEmpty()) {
      return;
    }
    impl_->pendingResizeSize_ = QSize();
    impl_->renderer_->flushAndWait();
    setViewportSize(static_cast<float>(pending.width()),
                    static_cast<float>(pending.height()));
    recreateSwapChain(impl_->hostWidget_.data());
    renderOneFrame();
  });

  // PlaybackService のフレーム変更に合わせて再描画
  if (auto *playback = ArtifactPlaybackService::instance()) {
    connect(playback, &ArtifactPlaybackService::frameChanged, this,
            [this](const FramePosition& position) {
              // 1. 可視性チェック: 非表示（他タブの裏など）なら描画しない
              if (auto* owner = qobject_cast<QWidget*>(parent())) {
                  if (!owner->isVisible()) return;
              }
              if (auto comp = impl_->previewPipeline_.composition()) {
                  comp->goToFrame(position.framePosition());
              }
              impl_->invalidateOverlayComposite();
              // 2. 更新要求を集約（Coalescing）
              renderOneFrame();
            });
  }
  // ブレンドパイプライン初期化
  if (auto device = impl_->renderer_->device()) {
    if (auto ctx = impl_->renderer_->immediateContext()) {
      impl_->gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(device, ctx);
      impl_->blendPipeline_ = std::make_unique<ArtifactCore::LayerBlendPipeline>(*impl_->gpuContext_);
      impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
      // [Fix D] qCDebug → qDebug/qWarning に升格（カテゴリ有効化不要）
      if (impl_->blendPipelineReady_) {
        qDebug() << "[CompositionView] LayerBlendPipeline initialized OK."
                 << "executors ready for GPU blend path.";
      } else {
        qWarning() << "[CompositionView] LayerBlendPipeline FAILED to initialize."
                   << "Will fall back to CPU compositing path.";
      }
    } else {
      qWarning() << "[CompositionView] immediateContext() is null - blend pipeline skipped.";
    }
  } else {
    qWarning() << "[CompositionView] device() is null - blend pipeline skipped.";
  }

  if (!impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_ = std::make_unique<GPUTextureCacheManager>();
  }
  if (auto device = impl_->renderer_->device()) {
    impl_->gpuTextureCacheManager_->setDevice(device);
    impl_->gpuTextureCacheManager_->setBudgetBytes(512ull * 1024ull * 1024ull);
    impl_->gpuTextureCacheManager_->setMaxEntries(256);
  }

  impl_->initialized_ = true;
}

void CompositionRenderController::destroy() {
  stop();
  if (impl_->renderer_) {
    impl_->renderer_->destroy();
    impl_->renderer_.reset();
  }
  impl_->compositionRenderer_.reset();
  impl_->surfaceCache_.clear();
  if (impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_->clear();
  }
  impl_->invalidateBaseComposite();
  impl_->initialized_ = false;
}

bool CompositionRenderController::isInitialized() const {
  return impl_->initialized_;
}

void CompositionRenderController::start() {
  if (!impl_->initialized_ || impl_->running_) {
    return;
  }
  impl_->running_ = true;
  impl_->invalidateBaseComposite();
  // Continuous timer removed for performance. 
  // Rendering is now event-driven (frameChanged, propertyChanged, etc.)
  renderOneFrame(); 
}

void CompositionRenderController::stop() {
  if (!impl_->running_) {
    return;
  }
  impl_->running_ = false;
  if (impl_->renderer_) {
    impl_->renderer_->flushAndWait();
  }
}
bool CompositionRenderController::isRunning() const { return impl_->running_; }

void CompositionRenderController::recreateSwapChain(QWidget *hostWidget) {
  if (!impl_->initialized_ || !impl_->renderer_ || hostWidget == nullptr) {
    return;
  }
  impl_->hostWidget_ = hostWidget;
  impl_->renderer_->flushAndWait();
  impl_->renderer_->recreateSwapChain(hostWidget);
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setViewportSize(float width, float height) {
  if (!impl_->renderer_) {
    return;
  }
  impl_->hostWidth_ = width;
  impl_->hostHeight_ = height;
  impl_->renderer_->setViewportSize(width, height);
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setPreviewQualityPreset(PreviewQualityPreset preset) {
  int factor = 1;
  switch (preset) {
  case PreviewQualityPreset::Final:
    factor = 1;
    break;
  case PreviewQualityPreset::Preview:
    factor = 2;
    break;
  case PreviewQualityPreset::Draft:
    factor = 4;
    break;
  default:
    factor = 1;
    break;
  }

  if (impl_->previewDownsample_ != factor) {
    impl_->previewDownsample_ = factor;
    impl_->invalidateBaseComposite();
    if (impl_->hostWidth_ > 0 && impl_->hostHeight_ > 0) {
      setViewportSize(impl_->hostWidth_, impl_->hostHeight_);
    }
    renderOneFrame();
  }
}

void CompositionRenderController::panBy(const QPointF &viewportDelta) {
  if (!impl_->renderer_) {
    return;
  }
  impl_->renderer_->panBy((float)viewportDelta.x(), (float)viewportDelta.y());
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

void CompositionRenderController::notifyViewportInteractionActivity() {
  const bool wasInteracting = impl_->viewportInteracting_;
  impl_->viewportInteracting_ = true;
  if (impl_->viewportInteractionTimer_) {
    impl_->viewportInteractionTimer_->start(120);
  }
  if (!wasInteracting) {
    impl_->invalidateBaseComposite();
  }
}

void CompositionRenderController::finishViewportInteraction() {
  if (!impl_->viewportInteracting_) {
    return;
  }
  impl_->viewportInteracting_ = false;
  if (impl_->viewportInteractionTimer_) {
    impl_->viewportInteractionTimer_->stop();
  }
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

void CompositionRenderController::setComposition(
    ArtifactCompositionPtr composition) {
  qCDebug(compositionViewLog) << "[CompositionView] setComposition"
           << "isNull=" << (composition == nullptr) << "id="
           << (composition ? composition->id().toString()
                           : QStringLiteral("<null>"));

  auto currentComposition = impl_->previewPipeline_.composition();
  if (currentComposition == composition) {
    if (auto *playback = ArtifactPlaybackService::instance()) {
      if (playback->currentComposition() != composition) {
        playback->setCurrentComposition(composition);
      }
    }
    if (composition && impl_->renderer_) {
      impl_->applyCompositionState(composition);
    }
    if (composition && !impl_->selectedLayerId_.isNil()) {
      impl_->gizmo_->setLayer(composition->layerById(impl_->selectedLayerId_));
    } else if (!composition) {
      impl_->gizmo_->setLayer(nullptr);
    }
    renderOneFrame();
    return;
  }

  for (auto &connection : impl_->layerChangedConnections_) {
    disconnect(connection);
  }
  impl_->layerChangedConnections_.clear();
  if (impl_->compositionChangedConnection_) {
    disconnect(impl_->compositionChangedConnection_);
    impl_->compositionChangedConnection_ = {};
  }
  impl_->surfaceCache_.clear();
  if (impl_->gpuTextureCacheManager_) {
    impl_->gpuTextureCacheManager_->clear();
  }
  impl_->invalidateBaseComposite();

  impl_->previewPipeline_.setComposition(composition);
  impl_->bindCompositionChanged(this, composition);

  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (playback->currentComposition() != composition) {
      playback->setCurrentComposition(composition);
    }
  }

  if (composition && impl_->renderer_) {
    impl_->applyCompositionState(composition);
    impl_->renderer_->fitToViewport();

    // 各レイヤーの変更を監視
    for (auto &layer : composition->allLayer()) {
      if (layer) {
                impl_->layerChangedConnections_.push_back(
                    connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                            [this, layer]() {
                              impl_->invalidateLayerSurfaceCache(layer);
                              impl_->invalidateBaseComposite();
                              impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());
                              renderOneFrame();
                            }));
      }
    }

    impl_->syncSelectedLayerOverlayState(composition);

    // コンポジションがセットされた瞬間に1フレーム描画
    renderOneFrame();
  } else if (!composition) {
    impl_->syncSelectedLayerOverlayState(composition);
    renderOneFrame();
  }
}

ArtifactCompositionPtr CompositionRenderController::composition() const {
  return impl_->previewPipeline_.composition();
}

void CompositionRenderController::setSelectedLayerId(const LayerID &id) {
  if (impl_->selectedLayerId_ == id) {
    return;
  }
  impl_->selectedLayerId_ = id;
  impl_->invalidateOverlayComposite();
  impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());
  renderOneFrame();
}

void CompositionRenderController::setClearColor(const FloatColor &color) {
  impl_->clearColor_ = color;
  if (impl_->renderer_) {
    impl_->renderer_->setClearColor(color);
  }
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

void CompositionRenderController::setShowGrid(bool show) {
  impl_->showGrid_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowGrid() const {
  return impl_->showGrid_;
}
void CompositionRenderController::setShowCheckerboard(bool show) {
  impl_->showCheckerboard_ = show;
  impl_->invalidateBaseComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowCheckerboard() const {
  return impl_->showCheckerboard_;
}
void CompositionRenderController::setShowGuides(bool show) {
  impl_->showGuides_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowGuides() const {
  return impl_->showGuides_;
}
void CompositionRenderController::setShowSafeMargins(bool show) {
  impl_->showSafeMargins_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowSafeMargins() const {
  return impl_->showSafeMargins_;
}

void CompositionRenderController::setShowMotionPathOverlay(bool show) {
  impl_->showMotionPathOverlay_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

bool CompositionRenderController::isShowMotionPathOverlay() const {
  return impl_ ? impl_->showMotionPathOverlay_ : false;
}

void CompositionRenderController::setGpuBlendEnabled(bool enabled) {
  if (impl_->gpuBlendEnabled_ == enabled) {
    return;
  }
  impl_->gpuBlendEnabled_ = enabled;
  impl_->invalidateBaseComposite();
  qWarning() << "[CompositionView] GPU blend user toggle changed"
             << "enabled=" << impl_->gpuBlendEnabled_
             << "envDisable="
             << qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
  renderOneFrame();
}

bool CompositionRenderController::isGpuBlendEnabled() const {
  return impl_->gpuBlendEnabled_;
}

void CompositionRenderController::resetView() {
  if (impl_->renderer_) {
    impl_->renderer_->resetView();
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomInAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    notifyViewportInteractionActivity();
    const float currentZoom = impl_->renderer_->getZoom();
    const float newZoom = std::clamp(currentZoom * 1.1f, 0.05f, 64.0f);
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomOutAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    notifyViewportInteractionActivity();
    const float currentZoom = impl_->renderer_->getZoom();
    const float newZoom = std::clamp(currentZoom / 1.1f, 0.05f, 64.0f);
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomFit() {
  if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoom100() {
  if (impl_->renderer_) {
    impl_->renderer_->setZoom(1.0f);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::focusSelectedLayer() {
  if (!impl_->renderer_) {
    return;
  }

  auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    zoomFit();
    return;
  }

  const auto layer = comp->layerById(impl_->selectedLayerId_);
  if (!layer) {
    zoomFit();
    return;
  }

  const QRectF bounds = layer->transformedBoundingBox();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    zoomFit();
    return;
  }

  const float viewW = impl_->hostWidth_ > 0.0f ? impl_->hostWidth_ : 1.0f;
  const float viewH = impl_->hostHeight_ > 0.0f ? impl_->hostHeight_ : 1.0f;
  const float margin = 48.0f;
  const float availableW = std::max(1.0f, viewW - margin * 2.0f);
  const float availableH = std::max(1.0f, viewH - margin * 2.0f);
  const float zoomX = availableW / static_cast<float>(bounds.width());
  const float zoomY = availableH / static_cast<float>(bounds.height());
  const float zoom = std::clamp(std::min(zoomX, zoomY), 0.02f, 64.0f);
  const QPointF center = bounds.center();

  impl_->renderer_->setZoom(zoom);
  impl_->renderer_->setPan(viewW * 0.5f - static_cast<float>(center.x()) * zoom,
                           viewH * 0.5f - static_cast<float>(center.y()) * zoom);
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

LayerID CompositionRenderController::layerAtViewportPos(const QPointF& viewportPos) const {
  auto comp = impl_->previewPipeline_.composition();
  const auto layer = hitTopmostLayerAtViewportPos(comp, impl_->renderer_.get(), viewportPos);
  return layer ? layer->id() : LayerID::Nil();
}

Ray CompositionRenderController::createPickingRay(const QPointF& viewportPos) const {
  if (!impl_->renderer_) return {};
  
  QMatrix4x4 view = impl_->renderer_->getViewMatrix();
  QMatrix4x4 proj = impl_->renderer_->getProjectionMatrix();
  QRect viewport(0, 0, (int)impl_->hostWidth_, (int)impl_->hostHeight_);
  
  QVector3D nearPos = QVector3D(viewportPos.x(), viewportPos.y(), 0.0f).unproject(view, proj, viewport);
  QVector3D farPos = QVector3D(viewportPos.x(), viewportPos.y(), 1.0f).unproject(view, proj, viewport);
  
  return { nearPos, (farPos - nearPos).normalized() };
}

void CompositionRenderController::handleMousePress(QMouseEvent *event) {
  if (!event || !impl_->renderer_) return;

  const QPointF viewportPos = event->position();

  // 3D Gizmo hit test (GIZ-2)
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  if (selectedLayer && impl_->gizmo3D_) {
      impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
      Ray ray = createPickingRay(viewportPos);
      GizmoAxis axis = impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(), impl_->renderer_->getProjectionMatrix());
      if (axis != GizmoAxis::None) {
          impl_->gizmo3D_->beginDrag(axis, ray);
          impl_->invalidateOverlayComposite();
          renderOneFrame();
          return;
      }
  }

  // Gizmo hit test first (2D)
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
    if (impl_->gizmo_->isDragging()) {
      return;
    }
  }

  if (event->button() == Qt::LeftButton) {
    auto toolManager = ArtifactApplicationManager::instance()->toolManager();
    auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;

    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      auto* selection = ArtifactApplicationManager::instance()
                            ? ArtifactApplicationManager::instance()->layerSelectionManager()
                            : nullptr;
      const auto currentFrame = currentFrameForComposition(comp);
      
      // Get selected layer for Pen tool operations
      auto selectedLayer = (!impl_->selectedLayerId_.isNil())
                               ? comp->layerById(impl_->selectedLayerId_)
                               : ArtifactAbstractLayerPtr{};

      if (activeTool == ToolType::Pen && selectedLayer) {
          // Convert canvas position to layer local position
          const QTransform globalTransform = selectedLayer->getGlobalTransform();
          bool invertible = false;
          const QTransform invTransform = globalTransform.inverted(&invertible);
          
          if (invertible) {
              impl_->beginMaskEditTransaction(selectedLayer);
              const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
              
              // 1. Hit test existing vertices for dragging or closing path
              const float hitThreshold = 8.0f / impl_->renderer_->getZoom(); // 8px in viewport space
              for (int m = 0; m < selectedLayer->maskCount(); ++m) {
                  LayerMask mask = selectedLayer->mask(m);
                  for (int p = 0; p < mask.maskPathCount(); ++p) {
                      MaskPath path = mask.maskPath(p);
                      for (int v = 0; v < path.vertexCount(); ++v) {
                          MaskVertex vertex = path.vertex(v);
                          if (QVector2D(vertex.position - localPos).length() < hitThreshold) {
                              // If it's the first vertex and we have more than 2, close the path
                              if (v == 0 && !path.isClosed() && path.vertexCount() > 2) {
                                  path.setClosed(true);
                                  mask.setMaskPath(p, path);
                                  mask.addMaskPath(MaskPath());
                                  selectedLayer->setMask(m, mask);
                                  impl_->markMaskEditDirty();
                                  qDebug() << "[PenTool] Closed path" << p;
                                  selectedLayer->changed();
                                  impl_->isDraggingVertex_ = false;
                                  impl_->draggingMaskIndex_ = m;
                                  impl_->draggingPathIndex_ = p;
                                  impl_->draggingVertexIndex_ = -1;
                                  impl_->hoveredMaskIndex_ = m;
                                  impl_->hoveredPathIndex_ = p;
                                  impl_->hoveredVertexIndex_ = -1;
                                  return;
                              }
                              
                              // Start dragging vertex
                              impl_->isDraggingVertex_ = true;
                              impl_->draggingMaskIndex_ = m;
                              impl_->draggingPathIndex_ = p;
                              impl_->draggingVertexIndex_ = v;
                              qDebug() << "[PenTool] Started dragging vertex" << v;
                              return;
                          }
                      }
                  }
              }

              // 2. Add new vertex if no existing vertex was hit
              if (selectedLayer->maskCount() == 0) {
                  LayerMask newMask;
                  MaskPath newPath;
                  newMask.addMaskPath(newPath);
                  selectedLayer->addMask(newMask);
              }
              
              LayerMask mask = selectedLayer->mask(0);
              if (mask.maskPathCount() == 0) {
                  mask.addMaskPath(MaskPath());
              }
              
              MaskPath path = mask.maskPath(0);
              // Don't add more vertices if already closed
              if (path.isClosed()) {
                  // TODO: Logic to start a new path or insert vertex into existing edge
                  return;
              }

              MaskVertex vertex;
              vertex.position = localPos;
              vertex.inTangent = QPointF(0, 0);
              vertex.outTangent = QPointF(0, 0);
              
              path.addVertex(vertex);
              mask.setMaskPath(0, path);
              selectedLayer->setMask(0, mask);
              impl_->markMaskEditDirty();
              
              qDebug() << "[PenTool] Added vertex at local:" << localPos << "layer:" << selectedLayer->id().toString();
              selectedLayer->changed();
              return; // Handled
          }
      }

      const auto layers = comp->allLayer();

      ArtifactAbstractLayerPtr hitLayer = nullptr;
      QVector<ArtifactAbstractLayerPtr> hitLayers;
      const bool ignoreLocked = event->modifiers().testFlag(Qt::AltModifier);
      const bool backPick = event->modifiers().testFlag(Qt::ControlModifier);
      
      // Collect all layers at this position
      for (int i = (int)layers.size() - 1; i >= 0; --i) {
        auto& layer = layers[i];
        if (!layer || !layer->isVisible()) continue;
        if (layer->isLocked() && !ignoreLocked) continue;
        if (!layer->isActiveAt(currentFrame)) continue;

        const QTransform globalTransform = layer->getGlobalTransform();
        bool invertible = false;
        const QTransform invTransform = globalTransform.inverted(&invertible);
        
        bool isHit = false;
        if (invertible) {
          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
          if (layer->localBounds().contains(localPos)) {
            isHit = true;
          }
        } else {
          auto bbox = layer->transformedBoundingBox();
          if (bbox.contains(cPos.x, cPos.y)) {
            isHit = true;
          }
        }

        if (isHit) {
            hitLayers.push_back(layer);
        }
      }

      // Cyclic selection logic
      if (!hitLayers.isEmpty()) {
          if (backPick && hitLayers.size() > 1) {
              hitLayer = hitLayers[1];
          } else {
              const float posThreshold = 3.0f; // px
              const bool sameSpot = QVector2D(viewportPos - impl_->lastHitPosition_).length() < posThreshold;

              if (sameSpot && !impl_->lastHitLayerId_.isNil()) {
                  int currentHitIdx = -1;
                  for (int i = 0; i < hitLayers.size(); ++i) {
                      if (hitLayers[i]->id() == impl_->lastHitLayerId_) {
                          currentHitIdx = i;
                          break;
                      }
                  }

                  if (currentHitIdx != -1) {
                      int nextIdx = (currentHitIdx + 1) % hitLayers.size();
                      hitLayer = hitLayers[nextIdx];
                  } else {
                      hitLayer = hitLayers[0];
                  }
              } else {
                  hitLayer = hitLayers[0];
              }
          }
          
          impl_->lastHitPosition_ = viewportPos;
          impl_->lastHitLayerId_ = hitLayer->id();
      } else {
          impl_->lastHitLayerId_ = LayerID::Nil();
      }

      if (hitLayer) {
        if (selection) {
          const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
          const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
          if (ctrl) {
            if (selection->isSelected(hitLayer)) {
              selection->removeFromSelection(hitLayer);
            } else {
              selection->addToSelection(hitLayer);
            }
          } else if (shift) {
            selection->addToSelection(hitLayer);
          } else {
            selection->selectLayer(hitLayer);
          }
        }

        ArtifactAbstractLayerPtr primaryLayer = hitLayer;
        if (selection) {
          primaryLayer = selection->currentLayer();
        }
        setSelectedLayerId(primaryLayer ? primaryLayer->id() : LayerID::Nil());
        impl_->gizmo_->setLayer(primaryLayer);
        if (impl_->gizmo3D_ && primaryLayer) {
          impl_->syncGizmo3DFromLayer(primaryLayer);
        }

        if (activeTool == ToolType::Selection) {
          impl_->clearSelectionGestureState();
          impl_->isRubberBandSelecting_ = false;
          impl_->dragGroupMove_ = selection && selection->selectedLayers().size() > 1 && selection->isSelected(hitLayer);
          impl_->dragGroupLayers_.clear();
          impl_->dragGroupStartPositions_.clear();
          if (impl_->dragGroupMove_ && selection) {
            const auto selected = selection->selectedLayers();
            impl_->dragGroupLayers_.reserve(selected.size());
            for (const auto& layer : selected) {
              if (!layer) {
                continue;
              }
              const QString id = layer->id().toString();
              impl_->dragGroupLayers_.push_back(layer);
              impl_->dragGroupStartPositions_.insert(
                  id, QPointF(layer->transform3D().positionX(), layer->transform3D().positionY()));
            }
            impl_->dragMode_ = LayerDragMode::Move;
          } else {
            impl_->dragMode_ = hitTestLayerDragMode(hitLayer, event->position(), impl_->renderer_.get());
            if (impl_->dragMode_ == LayerDragMode::None) {
              impl_->dragMode_ = LayerDragMode::Move;
            }
          }

          impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
          impl_->dragStartLayerPos_ = QPointF(hitLayer->transform3D().positionX(),
                                              hitLayer->transform3D().positionY());
          impl_->dragStartScaleX_ = hitLayer->transform3D().scaleX();
          impl_->dragStartScaleY_ = hitLayer->transform3D().scaleY();
          impl_->dragStartBoundingBox_ = hitLayer->transformedBoundingBox();
          impl_->dragFrame_ = comp->framePosition().framePosition();
          impl_->dragAppliedDelta_ = QPointF(0.0, 0.0);
          impl_->isDraggingLayer_ = true;
        } else {
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
        }
      } else {
        const bool selectionTool = activeTool == ToolType::Selection;
        if (selectionTool) {
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
          impl_->isRubberBandSelecting_ = true;
          impl_->rubberBandStartViewportPos_ = viewportPos;
          impl_->rubberBandCurrentViewportPos_ = viewportPos;
          impl_->selectionMode_ = selectionModeFromModifiers(event->modifiers());
        } else {
          if (!(event->modifiers() & Qt::ShiftModifier)) {
            if (selection) {
              selection->clearSelection();
            }
            setSelectedLayerId(LayerID::Nil());
            impl_->gizmo_->setLayer(nullptr);
          }
          impl_->clearSelectionGestureState();
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
        }
      }
    }
  }
}

void CompositionRenderController::handleMouseMove(const QPointF &viewportPos) {
  auto toolManager = ArtifactApplicationManager::instance()->toolManager();
  auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;
  bool needsRender = false;

  if (impl_->isRubberBandSelecting_) {
    impl_->rubberBandCurrentViewportPos_ = viewportPos;
    renderOneFrame();
    return;
  }

  if (activeTool == ToolType::Pen && impl_->isDraggingVertex_) {
      auto comp = impl_->previewPipeline_.composition();
      if (comp && impl_->renderer_) {
          auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
          if (selectedLayer) {
              const auto cPos = impl_->renderer_->viewportToCanvas(
                  {(float)viewportPos.x(), (float)viewportPos.y()});
              const QTransform globalTransform = selectedLayer->getGlobalTransform();
              bool invertible = false;
              const QTransform invTransform = globalTransform.inverted(&invertible);
              
              if (invertible) {
                  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
                  LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);
                  MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
                  MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
                  
                  vertex.position = localPos;
                  path.setVertex(impl_->draggingVertexIndex_, vertex);
                  mask.setMaskPath(impl_->draggingPathIndex_, path);
                  selectedLayer->setMask(impl_->draggingMaskIndex_, mask);
                  impl_->markMaskEditDirty();
                  
                  selectedLayer->changed();
                  return;
              }
          }
      }
  }

  // Hover detection for Pen tool
  if (activeTool == ToolType::Pen) {
      const int prevHoveredMaskIndex = impl_->hoveredMaskIndex_;
      const int prevHoveredPathIndex = impl_->hoveredPathIndex_;
      const int prevHoveredVertexIndex = impl_->hoveredVertexIndex_;
      impl_->hoveredMaskIndex_ = -1;
      impl_->hoveredPathIndex_ = -1;
      impl_->hoveredVertexIndex_ = -1;

      auto comp = impl_->previewPipeline_.composition();
      if (comp && impl_->renderer_) {
          auto selectedLayer = comp->layerById(impl_->selectedLayerId_);
          if (selectedLayer) {
              const auto cPos = impl_->renderer_->viewportToCanvas(
                  {(float)viewportPos.x(), (float)viewportPos.y()});
              const QTransform globalTransform = selectedLayer->getGlobalTransform();
              bool invertible = false;
              const QTransform invTransform = globalTransform.inverted(&invertible);
              
              if (invertible) {
                  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
                  const float hitThreshold = 8.0f / impl_->renderer_->getZoom();
                  
                  for (int m = 0; m < selectedLayer->maskCount(); ++m) {
                      LayerMask mask = selectedLayer->mask(m);
                      for (int p = 0; p < mask.maskPathCount(); ++p) {
                          MaskPath path = mask.maskPath(p);
                          for (int v = 0; v < path.vertexCount(); ++v) {
                              MaskVertex vertex = path.vertex(v);
                              if (QVector2D(vertex.position - localPos).length() < hitThreshold) {
                                  impl_->hoveredMaskIndex_ = m;
                                  impl_->hoveredPathIndex_ = p;
                                  impl_->hoveredVertexIndex_ = v;
                                  break;
                              }
                          }
                          if (impl_->hoveredVertexIndex_ != -1) break;
                      }
                      if (impl_->hoveredVertexIndex_ != -1) break;
                  }
              }
          }
      }
      if (prevHoveredMaskIndex != impl_->hoveredMaskIndex_ ||
          prevHoveredPathIndex != impl_->hoveredPathIndex_ ||
          prevHoveredVertexIndex != impl_->hoveredVertexIndex_) {
          impl_->invalidateOverlayComposite();
          needsRender = true;
      }
  }

  // 3D Gizmo interaction (GIZ-2, GIZ-3)
  if (impl_->gizmo3D_) {
      Ray ray = createPickingRay(viewportPos);
      if (impl_->gizmo3D_->isDragging()) {
          impl_->gizmo3D_->updateDrag(ray);
          
          // Update layer transform from gizmo
          auto comp = impl_->previewPipeline_.composition();
          if (comp && !impl_->selectedLayerId_.isNil()) {
              if (auto layer = comp->layerById(impl_->selectedLayerId_)) {
                  if (impl_->gizmo3D_->mode() == GizmoMode::Scale) {
                      auto &t3 = layer->transform3D();
                      const ArtifactCore::RationalTime time(layer->currentFrame(), 30);
                      const QVector3D scale = impl_->gizmo3D_->scale();
                      t3.setScale(time, scale.x(), scale.y());
                  } else {
                      const QVector3D currentPos = layer->position3D();
                      const QVector3D gizmoPos = impl_->gizmo3D_->position();
                      if (impl_->gizmo3D_->depthEnabled()) {
                          layer->setPosition3D(gizmoPos);
                      } else {
                          layer->setPosition3D(QVector3D(gizmoPos.x(), gizmoPos.y(), currentPos.z()));
                      }
                      layer->setRotation3D(impl_->gizmo3D_->rotation());
                  }
                  layer->changed();
              }
          }
          return;
      } else {
          // Hover highlighting
          const auto prevHoverAxis = impl_->gizmo3D_->hoverAxis();
          impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(), impl_->renderer_->getProjectionMatrix());
          if (prevHoverAxis != impl_->gizmo3D_->hoverAxis()) {
              impl_->invalidateOverlayComposite();
              needsRender = true;
          }
      }
  }

  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
  }

  if (needsRender) {
    renderOneFrame();
  }
}

void CompositionRenderController::handleMouseRelease() {
  impl_->isDraggingLayer_ = false;

  if (impl_->isRubberBandSelecting_) {
    auto comp = impl_->previewPipeline_.composition();
    auto* selection = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->layerSelectionManager()
                          : nullptr;
    if (comp && selection && impl_->renderer_) {
      const QRectF rect = impl_->rubberBandCanvasRect().normalized();
      const auto currentFrame = currentFrameForComposition(comp);
      const auto layers = comp->allLayer();
      QVector<ArtifactAbstractLayerPtr> hits;
      hits.reserve(layers.size());
      for (const auto& layer : layers) {
        if (!layerIntersectsCanvasRect(layer, rect, currentFrame)) {
          continue;
        }
        hits.push_back(layer);
      }

      if (impl_->selectionMode_ == SelectionMode::Replace) {
        selection->clearSelection();
      }

      for (const auto& layer : hits) {
        if (!layer) {
          continue;
        }
        if (impl_->selectionMode_ == SelectionMode::Toggle) {
          if (selection->isSelected(layer)) {
            selection->removeFromSelection(layer);
          } else {
            selection->addToSelection(layer);
          }
        } else {
          selection->addToSelection(layer);
        }
      }

      if (impl_->selectionMode_ == SelectionMode::Replace && hits.isEmpty()) {
        selection->clearSelection();
      }

      const auto primaryLayer = selection->currentLayer();
      setSelectedLayerId(primaryLayer ? primaryLayer->id() : LayerID::Nil());
      impl_->gizmo_->setLayer(primaryLayer);
      if (impl_->gizmo3D_ && primaryLayer) {
        impl_->syncGizmo3DFromLayer(primaryLayer);
      }
    }
    impl_->clearSelectionGestureState();
    renderOneFrame();
    return;
  }

  impl_->isDraggingVertex_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;
  impl_->commitMaskEditTransaction();

  if (impl_->gizmo3D_) {
      impl_->gizmo3D_->endDrag();
      impl_->invalidateOverlayComposite();
      renderOneFrame();
  }

  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseRelease();
  }
}

bool CompositionRenderController::hasPendingMaskEdit() const {
  return impl_ && impl_->maskEditPending_;
}

TransformGizmo *CompositionRenderController::gizmo() const {
  return impl_->gizmo_.get();
}

Artifact3DGizmo *CompositionRenderController::gizmo3D() const {
  return impl_->gizmo3D_.get();
}

Qt::CursorShape CompositionRenderController::cursorShapeForViewportPos(const QPointF& viewportPos) const
{
  if (!impl_->gizmo_ || !impl_->renderer_) {
    return Qt::ArrowCursor;
  }
  return impl_->gizmo_->cursorShapeForViewportPos(viewportPos, impl_->renderer_.get());
}

void CompositionRenderController::renderOneFrame() {
  if (!impl_->initialized_ || !impl_->renderer_) {
    qCDebug(compositionViewLog) << "[CompositionView] renderOneFrame skipped: not initialized";
    return;
  }
  if (!impl_->running_) {
    qCDebug(compositionViewLog) << "[CompositionView] renderOneFrame skipped: controller stopped";
    return;
  }
  if (auto *host = impl_->hostWidget_.data()) {
    if (!host->isVisible()) {
      qCDebug(compositionViewLog) << "[CompositionView] renderOneFrame skipped: host hidden";
      return;
    }
  }

  if (impl_->renderInProgress_) {
    impl_->renderRescheduleRequested_ = true;
    return;
  }

  if (impl_->renderScheduled_) {
    return;
  }

  impl_->renderScheduled_ = true;
  const int scheduleDelayMs = impl_->viewportInteracting_ ? 16 : 0;
  QTimer::singleShot(scheduleDelayMs, this, [this]() {
    impl_->renderScheduled_ = false;
    if (!impl_->initialized_ || !impl_->renderer_ || !impl_->running_) {
      return;
    }
    if (auto *host = impl_->hostWidget_.data()) {
      if (!host->isVisible()) {
        return;
      }
    }
    if (impl_->renderInProgress_) {
      impl_->renderRescheduleRequested_ = true;
      return;
    }
    impl_->renderInProgress_ = true;
    impl_->renderOneFrameImpl(this);
    impl_->renderInProgress_ = false;
    if (impl_->renderRescheduleRequested_) {
      impl_->renderRescheduleRequested_ = false;
      renderOneFrame();
    }
  });
}

void CompositionRenderController::Impl::renderOneFrameImpl(CompositionRenderController *owner) {
  if (!owner || !initialized_ || !renderer_ || !running_) {
    return;
  }
  if (auto *host = hostWidget_.data()) {
    if (!host->isVisible()) {
      return;
    }
  }

  // 強制的なサイズ同期: ホストウィジェットの物理サイズとスワップチェーンを一致させる
  if (auto* host = hostWidget_.data()) {
      const float curW = static_cast<float>(host->width());
      const float curH = static_cast<float>(host->height());
      if (std::abs(curW - hostWidth_) > 0.5f || std::abs(curH - hostHeight_) > 0.5f) {
          qDebug() << "[CompositionView] Widget size changed, scheduling swapchain update:" << curW << "x" << curH;
          pendingResizeSize_ = QSize(static_cast<int>(curW), static_cast<int>(curH));
          if (resizeDebounceTimer_) {
            resizeDebounceTimer_->start(80);
          }
          return;
      }
  }

  QElapsedTimer frameTimer;
  frameTimer.start();
  qint64 phaseNs = 0;
  auto markPhaseMs = [&frameTimer, &phaseNs]() -> qint64 {
    const qint64 nowNs = frameTimer.nsecsElapsed();
    const qint64 phaseMs = (nowNs - phaseNs) / 1000000;
    phaseNs = nowNs;
    return phaseMs;
  };

  auto comp = previewPipeline_.composition();
  if (auto *service = ArtifactProjectService::instance()) {
    const auto preferred = resolvePreferredComposition(service);
    if (preferred && preferred != comp) {
      comp = preferred;
      previewPipeline_.setComposition(comp);
      qCDebug(compositionViewLog)
          << "[CompositionView] renderOneFrame resynced preferred composition"
          << "id=" << comp->id().toString()
          << "layers=" << comp->allLayer().size();
    }
  }
  if (!comp) {
    renderer_->clear();
    renderer_->present();
    return;
  }

  const QSize compSize = comp->settings().compositionSize();
  const float cw = static_cast<float>(compSize.width()  > 0 ? compSize.width()  : 1920);
  const float ch = static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  lastCanvasWidth_ = cw;
  lastCanvasHeight_ = ch;

  // GPU path should represent the currently visible viewport, not only the
  // composition rect. Otherwise layers that extend outside the comp get clipped
  // at the intermediate RT stage.
  const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : cw;
  const float viewportH = hostHeight_ > 0.0f ? hostHeight_ : ch;
  const int effectivePreviewDownsample =
      viewportInteracting_
          ? std::max(previewDownsample_, interactivePreviewDownsampleFloor_)
          : (gpuBlendEnabled_ ? std::max(previewDownsample_, 2) : previewDownsample_);
  const float rcw =
      std::max(1.0f, viewportW / static_cast<float>(effectivePreviewDownsample));
  const float rch =
      std::max(1.0f, viewportH / static_cast<float>(effectivePreviewDownsample));

  if (compositionRenderer_) {
    compositionRenderer_->SetCompositionSize(cw, ch);
    // Note: ApplyCompositionSpace sets renderer canvas size to FULL size.
    // We override it below if pipeline is enabled.
    compositionRenderer_->ApplyCompositionSpace();
  } else {
    renderer_->setCanvasSize(cw, ch);
  }

  renderer_->clear();

  const auto layers = comp->allLayer();
  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }
  int64_t effectiveEndFrame = 0;
  for (const auto &l : layers) {
    if (l) {
      effectiveEndFrame = std::max(effectiveEndFrame, l->outPoint().framePosition());
    }
  }
  const int64_t framePos = currentFrame.framePosition();
  const bool frameOutOfRange =
      (framePos < 0 || (effectiveEndFrame > 0 && framePos >= effectiveEndFrame));
  float panX = 0.0f;
  float panY = 0.0f;
  renderer_->getPan(panX, panY);
  const float zoom = renderer_->getZoom();
  const QString backgroundKey = QStringLiteral("%1,%2,%3,%4")
      .arg(comp->backgroundColor().r(), 0, 'f', 4)
      .arg(comp->backgroundColor().g(), 0, 'f', 4)
      .arg(comp->backgroundColor().b(), 0, 'f', 4)
      .arg(comp->backgroundColor().a(), 0, 'f', 4);
  const QByteArray baseRenderKey =
      QByteArray("comp=") + comp->id().toString().toUtf8() +
      "|baseSerial=" + QByteArray::number(baseInvalidationSerial_) +
      "|frame=" + QByteArray::number(framePos) +
      "|size=" + QByteArray::number(static_cast<int>(viewportW)) + "x" +
      QByteArray::number(static_cast<int>(viewportH)) +
      "|downsample=" + QByteArray::number(effectivePreviewDownsample) +
      "|zoom=" + QByteArray::number(zoom, 'f', 4) +
      "|pan=" + QByteArray::number(panX, 'f', 2) + "," +
      QByteArray::number(panY, 'f', 2) +
      "|clear=" + backgroundKey.toUtf8() +
      "|checker=" + QByteArray::number(showCheckerboard_ ? 1 : 0) +
      "|gpuBlend=" + QByteArray::number(gpuBlendEnabled_ ? 1 : 0);
  const QByteArray renderKey =
      baseRenderKey +
      "|overlaySerial=" + QByteArray::number(overlayInvalidationSerial_) +
      "|selected=" + selectedLayerId_.toString().toUtf8() +
      "|gizmoMode=" + QByteArray::number(
          gizmo3D_ ? static_cast<int>(gizmo3D_->mode()) : -1) +
      "|gizmoHover=" + QByteArray::number(
          gizmo3D_ ? static_cast<int>(gizmo3D_->hoverAxis()) : -1) +
      "|gizmoActive=" + QByteArray::number(
          gizmo3D_ ? static_cast<int>(gizmo3D_->activeAxis()) : -1) +
      "|flags=" + QByteArray::number(showGrid_ ? 1 : 0) +
      QByteArray::number(showGuides_ ? 1 : 0) +
      QByteArray::number(showSafeMargins_ ? 1 : 0) +
      QByteArray::number(viewportInteracting_ ? 1 : 0);
  if (!lastFinalPresentKey_.isEmpty() && lastFinalPresentKey_ == renderKey) {
    qCDebug(compositionViewLog)
        << "[CompositionView] skipped redundant composite frame"
        << "frame=" << framePos
        << "zoom=" << zoom
        << "pan=" << QPointF(panX, panY);
    return;
  }

  {

  const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;
  const bool hasGpuBlendJustification =
      std::any_of(layers.begin(), layers.end(),
                  [&](const ArtifactAbstractLayerPtr &layer) {
                    if (!layer || !layer->isVisible() || !layer->isActiveAt(currentFrame)) {
                      return false;
                    }
                    if (layer->layerBlendType() != ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL) {
                      return true;
                    }
                    if (layer->maskCount() > 0) {
                      return true;
                    }
                    return layerHasCpuRasterizerWork(layer);
                  });
  const bool gpuBlendPathRequested = gpuBlendRequested && hasGpuBlendJustification;

  // Avoid paying render-pipeline setup cost when GPU blending is disabled.
  if (gpuBlendPathRequested) {
    if (auto device = renderer_->device()) {
      renderPipeline_.initialize(device, static_cast<Uint32>(rcw),
                                 static_cast<Uint32>(rch),
                                 RenderConfig::PipelineFormat);
    }
  } else if (gpuBlendRequested && lastPipelineStateMask_ != -1) {
    qCDebug(compositionViewLog)
        << "[CompositionView] GPU blend bypassed for simple layers"
        << "layers=" << layers.size()
        << "frameOutOfRange=" << frameOutOfRange;
  }

  const bool pipelineEnabled = gpuBlendPathRequested && renderPipeline_.ready();
  const int pipelineStateMask =
      (gpuBlendEnabled_ ? 0x1 : 0x0) |
      (renderPipeline_.ready() ? 0x2 : 0x0) |
      (blendPipelineReady_ ? 0x4 : 0x0);
  if (pipelineStateMask != lastPipelineStateMask_) {
    lastPipelineStateMask_ = pipelineStateMask;
    if (!pipelineEnabled) {
      qWarning() << "[CompositionView] GPU blend path disabled"
                 << "gpuBlendEnabled=" << gpuBlendEnabled_
                 << "renderPipelineReady=" << renderPipeline_.ready()
                 << "blendPipelineReady=" << blendPipelineReady_
                 << "size=" << QSize(static_cast<int>(cw), static_cast<int>(ch));
    } else {
      qDebug() << "[CompositionView] GPU blend path enabled"
               << "size=" << QSize(static_cast<int>(cw), static_cast<int>(ch));
    }
  }
  if (pipelineEnabled) {
    const QSize pipelineSize(static_cast<int>(renderPipeline_.width()),
                             static_cast<int>(renderPipeline_.height()));
    // Compute shaders now have explicit bounds guards.
    if (((pipelineSize.width() & 7) != 0 || (pipelineSize.height() & 7) != 0) &&
        pipelineSize != lastDispatchWarningSize_) {
      lastDispatchWarningSize_ = pipelineSize;
      qCDebug(compositionViewLog) << "[CompositionView] GPU blend path uses non-8-aligned render size: " << pipelineSize;
    }
  }

  int drawnLayerCount = 0;
  int surfaceUploadLayerCount = 0;
  int cpuRasterLayerCount = 0;
  const float targetViewportW = hostWidth_;
  const float targetViewportH = hostHeight_;
  const float legacyDownsampleViewportW =
      hostWidth_ > 0.0f ? hostWidth_ / static_cast<float>(effectivePreviewDownsample) : 0.0f;
  const float legacyDownsampleViewportH =
      hostHeight_ > 0.0f ? hostHeight_ / static_cast<float>(effectivePreviewDownsample) : 0.0f;
  qint64 setupMs = markPhaseMs();
  qint64 basePassMs = 0;
  qint64 layerPassMs = 0;
  qint64 overlayMs = 0;
  qint64 flushMs = 0;
  qint64 presentMs = 0;

  // hasSoloLayer: solo レイヤーの存在確認
  const bool hasSoloLayer =
      std::any_of(layers.begin(), layers.end(),
                  [](const ArtifactAbstractLayerPtr& l) {
                    return l && l->isVisible() && l->isSolo();
                  });
  const QStringList selectedIds = selectedLayerIdList();
  const bool hasSelection = !selectedIds.isEmpty();
  constexpr float kGhostOpacityScale = 0.22f;

  // ============================================================
  // GPU パイプライン: レイヤー 0 枚でも frameOutOfRange でも常に描画
  // ============================================================
  if (pipelineEnabled) {
    auto ctx      = renderer_->immediateContext();
    auto accumRTV = renderPipeline_.accumRTV();
    auto accumSRV = renderPipeline_.accumSRV();
    auto tempUAV  = renderPipeline_.tempUAV();
    auto layerRTV = renderPipeline_.layerRTV();
    auto layerSRV = renderPipeline_.layerSRV();

    // ==== オフスクリーン描画前の状態保存 ====
    // GPU path は現在の viewer 表示結果を 1 枚の中間 RT に合成する。
    // そのため offscreen 側でも「現在の zoom/pan を縮小した状態」を再現する。
    const float origZoom  = renderer_->getZoom();
    const FloatColor origClearColor = renderer_->getClearColor();
    float       origPanX, origPanY;
    renderer_->getPan(origPanX, origPanY);
    const float origViewW = hostWidth_;
    const float origViewH = hostHeight_;
    const float offscreenScale =
        (origViewW > 0.0f) ? (rcw / origViewW) : 1.0f;

    // オフスクリーン描画用の座標系設定。
    // viewport を縮小した RT へ落とすため zoom/pan も同倍率で縮小する。
    renderer_->setViewportSize(rcw, rch);
    renderer_->setZoom(origZoom * offscreenScale);
    renderer_->setPan(origPanX * offscreenScale, origPanY * offscreenScale);
    {
      Diligent::Viewport offVP;
      offVP.TopLeftX = 0.0f;
      offVP.TopLeftY = 0.0f;
      offVP.Width    = rcw;
      offVP.Height   = rch;
      offVP.MinDepth = 0.0f;
      offVP.MaxDepth = 1.0f;
      ctx->SetViewports(1, &offVP, static_cast<Diligent::Uint32>(rcw),
                        static_cast<Diligent::Uint32>(rch));
    }

    // -- 1: accum を透明クリア、背景は layerRTV に Composition Space で描画して compute で積む --
    renderer_->setOverrideRTV(accumRTV);
    renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
    renderer_->clear();
    renderer_->setOverrideRTV(nullptr);

    renderer_->setOverrideRTV(layerRTV);
    renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
    renderer_->clear();
    // 背景は Composition Space (0,0)-(cw,ch) で描いて、コンポジションエリアだけが塚まる。
    // スクリーン全体が塗れるのはテーマクリアカラー（きっかけは renderer_->clear()）。
    const FloatColor bgColor = comp->backgroundColor();
    if (showCheckerboard_) {
      renderer_->drawCheckerboard(0.0f, 0.0f, cw, ch, 16.0f,
                                       {0.25f, 0.25f, 0.25f, 1.0f},
                                       {0.35f, 0.35f, 0.35f, 1.0f});
    } else if (compositionRenderer_) {
      compositionRenderer_->DrawCompositionBackground(bgColor);
    } else {
      renderer_->drawRectLocal(0.0f, 0.0f, cw, ch, bgColor, 1.0f);
    }
    if (showGrid_) {
      renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f, {0.3f, 0.3f, 0.3f, 0.5f});
    }
    renderer_->setOverrideRTV(nullptr);

    // CS 実行前に RTV を完全解除してリソース遺留を防ぐ
    ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (!blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV,
                               ArtifactCore::BlendMode::Normal, 1.0f)) {
      qWarning() << "[CompositionView] background blend() failed";
    } else {
      renderPipeline_.swapAccumAndTemp();
      accumSRV = renderPipeline_.accumSRV();
      tempUAV = renderPipeline_.tempUAV();
    }
    basePassMs = markPhaseMs();

    // -- 2: レイヤーブレンド（frameOutOfRange ならスキップ）--
    if (!frameOutOfRange) {
      for (const auto& layer : layers) {
        if (!layer || !layer->isVisible()) continue;
        if (hasSoloLayer && !layer->isSolo()) continue;
        if (!layer->isActiveAt(currentFrame)) continue;

        ++drawnLayerCount;
        if (layerUsesSurfaceUploadForCompositionView(layer)) {
          ++surfaceUploadLayerCount;
        }
        if (layerHasCpuRasterizerWork(layer)) {
          ++cpuRasterLayerCount;
        }

        layer->goToFrame(currentFrame.framePosition());
        const auto  blendMode = ArtifactCore::toBlendMode(layer->layerBlendType());
        const float opacity   = layer->opacity() *
                                ((hasSelection && !isLayerSelected(selectedIds, layer))
                                     ? kGhostOpacityScale
                                     : 1.0f);
        if (opacity <= 0.0f) continue;

        renderer_->setOverrideRTV(layerRTV);
        renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
        renderer_->clear();
        QString* dbgOut = QLoggingCategory::defaultCategory()->isDebugEnabled()
                          ? &lastVideoDebug_ : nullptr;
        drawLayerForCompositionView(layer, renderer_.get(), 1.0f, dbgOut,
                                    &surfaceCache_, gpuTextureCacheManager_.get(),
                                    currentFrame.framePosition());
        renderer_->setOverrideRTV(nullptr);

        // CS 実行前に RTV を解除
        ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const bool blendOk = blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV, blendMode, opacity);
        if (!blendOk) {
          continue;
        }
        renderPipeline_.swapAccumAndTemp();
        accumSRV = renderPipeline_.accumSRV();
        tempUAV  = renderPipeline_.tempUAV();
      }
    }

    // ==== オフスクリーン描画後: renderer の状態をホスト viewport に戻す ====
    renderer_->setViewportSize(origViewW, origViewH);
    renderer_->setZoom(origZoom);
    renderer_->setPan(origPanX, origPanY);
    renderer_->setClearColor(origClearColor);
    {
      Diligent::Viewport hostVP;
      hostVP.TopLeftX = 0.0f;
      hostVP.TopLeftY = 0.0f;
      hostVP.Width    = origViewW;
      hostVP.Height   = origViewH;
      hostVP.MinDepth = 0.0f;
      hostVP.MaxDepth = 1.0f;
      ctx->SetViewports(1, &hostVP, static_cast<Diligent::Uint32>(origViewW),
                        static_cast<Diligent::Uint32>(origViewH));
    }

    // -- 3: オフスクリーン RT は "viewer pixels" を表しているので、
    // screen-space のフル viewport quad として貼り戻す。
    renderer_->setCanvasSize(origViewW, origViewH);
    renderer_->setZoom(1.0f);
    renderer_->setPan(0.0f, 0.0f);
    renderer_->drawSprite(0, 0, origViewW, origViewH, renderPipeline_.accumSRV());
    if (compositionRenderer_) {
      compositionRenderer_->SetCompositionSize(cw, ch);
      compositionRenderer_->ApplyCompositionSpace();
    } else {
      renderer_->setCanvasSize(cw, ch);
    }
    renderer_->setZoom(origZoom);
    renderer_->setPan(origPanX, origPanY);
    layerPassMs = markPhaseMs();
  } else {
    // === Fallback path (GPU パイプラインなし) ===
    const FloatColor bgColor = comp->backgroundColor();
    if (showCheckerboard_) {
      renderer_->drawCheckerboard(0.0f, 0.0f, cw, ch, 16.0f,
                                       {0.25f, 0.25f, 0.25f, 1.0f},
                                       {0.35f, 0.35f, 0.35f, 1.0f});
    } else if (compositionRenderer_) {
      compositionRenderer_->DrawCompositionBackground(bgColor);
    } else {
      renderer_->drawRectLocal(0.0f, 0.0f, cw, ch, bgColor, 1.0f);
    }
    if (showGrid_) {
      renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f, {0.3f, 0.3f, 0.3f, 0.5f});
    }
    basePassMs = markPhaseMs();

    if (!frameOutOfRange) {
      for (const auto& layer : layers) {
        if (!layer || !layer->isVisible()) continue;
        if (hasSoloLayer && !layer->isSolo()) continue;
        if (!layer->isActiveAt(currentFrame)) continue;
        ++drawnLayerCount;
        if (layerUsesSurfaceUploadForCompositionView(layer)) {
          ++surfaceUploadLayerCount;
        }
        if (layerHasCpuRasterizerWork(layer)) {
          ++cpuRasterLayerCount;
        }
        layer->goToFrame(currentFrame.framePosition());
        const float opacity = layer->opacity() *
                              ((hasSelection && !isLayerSelected(selectedIds, layer))
                                   ? kGhostOpacityScale
                                   : 1.0f);
        QString* dbgOut = QLoggingCategory::defaultCategory()->isDebugEnabled()
                          ? &lastVideoDebug_ : nullptr;
        drawLayerForCompositionView(layer, renderer_.get(), opacity, dbgOut,
                                    &surfaceCache_, gpuTextureCacheManager_.get(),
                                    currentFrame.framePosition());
      }
    }
    layerPassMs = markPhaseMs();
  }

  if (renderer_ && showMotionPathOverlay_ && comp && !selectedLayerId_.isNil()) {
    if (auto selectedLayer = comp->layerById(selectedLayerId_)) {
      const auto motionPath = buildMotionPathSamples(selectedLayer, comp);
      if (motionPath.size() >= 2) {
        const FloatColor pathColor{0.95f, 0.65f, 0.22f, 0.85f};
        const FloatColor keyColor{1.0f, 0.92f, 0.28f, 1.0f};
        const FloatColor currentColor{0.28f, 0.9f, 1.0f, 1.0f};
        QPointF prev = motionPath[0].position;
        for (int i = 1; i < motionPath.size(); ++i) {
          const QPointF cur = motionPath[i].position;
          renderer_->drawSolidLine({static_cast<float>(prev.x()), static_cast<float>(prev.y())},
                                   {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
                                   pathColor, 1.2f);
          prev = cur;
        }
        for (const auto& sample : motionPath) {
          const float size = sample.keyframe ? 6.0f : 4.0f;
          const FloatColor color = sample.keyframe ? keyColor : currentColor;
          renderer_->drawPoint(static_cast<float>(sample.position.x()),
                               static_cast<float>(sample.position.y()),
                               size, color);
        }
      }
    }
  }

  if (gizmo_) {
    auto selectedLayer = (!selectedLayerId_.isNil() && comp)
                             ? comp->layerById(selectedLayerId_)
                             : ArtifactAbstractLayerPtr{};
    if (selectedLayer && selectedLayer->isVisible()) {
      gizmo_->setLayer(selectedLayer);
      gizmo_->draw(renderer_.get());

      if (gizmo3D_) {
          syncGizmo3DFromLayer(selectedLayer);
          const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
          const float viewportH = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
          if (viewportW > 0.0f && viewportH > 0.0f) {
              float panX = 0.0f;
              float panY = 0.0f;
              float zoom = 1.0f;
              renderer_->getPan(panX, panY);
              zoom = std::max(0.001f, renderer_->getZoom());

              QMatrix4x4 view;
              view.translate(panX, panY, 0.0f);
              view.scale(zoom, zoom, 1.0f);

              QMatrix4x4 proj;
              proj.ortho(0.0f, viewportW, viewportH, 0.0f, -1000.0f, 1000.0f);

              gizmo3D_->draw(renderer_.get(), view, proj);
          } else {
              gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(), renderer_->getProjectionMatrix());
          }
      }

      // Mask Overlay Drawing
      const int maskCount = selectedLayer->maskCount();
      if (maskCount > 0 && renderer_ && selectedLayer->isActiveAt(currentFrame)) {
          const QTransform globalTransform = selectedLayer->getGlobalTransform();
          const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
          const FloatColor maskLineColor = {0.26f, 0.84f, 0.96f, 0.96f};
          const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
          const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
          const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
          const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

          for (int m = 0; m < maskCount; ++m) {
              LayerMask mask = selectedLayer->mask(m);
              if (!mask.isEnabled()) continue;

              for (int p = 0; p < mask.maskPathCount(); ++p) {
                  MaskPath path = mask.maskPath(p);
                  const int vertexCount = path.vertexCount();
                  if (vertexCount == 0) continue;

                  struct VertexMarker {
                      Detail::float2 pos;
                      FloatColor color;
                      float radius;
                  };
                  std::vector<VertexMarker> markers;
                  markers.reserve(static_cast<size_t>(vertexCount));

                  Detail::float2 lastCanvasPos;
                  for (int v = 0; v < vertexCount; ++v) {
                      MaskVertex vertex = path.vertex(v);
                      QPointF canvasPos = globalTransform.map(vertex.position);
                      Detail::float2 currentCanvasPos = {(float)canvasPos.x(), (float)canvasPos.y()};

                      if (v > 0) {
                          renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 6.0f, maskLineShadowColor);
                          renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 3.5f, maskLineColor);
                      }

                      FloatColor currentColor = maskPointColor;
                      float currentPointRadius = 17.0f;
                      
                      if (isDraggingVertex_ && draggingMaskIndex_ == m && 
                          draggingPathIndex_ == p && draggingVertexIndex_ == v) {
                          currentColor = dragColor;
                          currentPointRadius = 21.0f;
                      } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && 
                                 hoveredVertexIndex_ == v) {
                          currentColor = hoverColor;
                          currentPointRadius = 21.0f;
                      }

                      markers.push_back({currentCanvasPos, currentColor, currentPointRadius});
                      lastCanvasPos = currentCanvasPos;
                  }

                  if (path.isClosed() && vertexCount > 1) {
                      MaskVertex firstVertex = path.vertex(0);
                      QPointF firstCanvasPos = globalTransform.map(firstVertex.position);
                      renderer_->drawThickLineLocal(
                          lastCanvasPos,
                          {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                          7.0f,
                          maskLineShadowColor);
                      renderer_->drawThickLineLocal(
                          lastCanvasPos,
                          {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                          4.0f,
                          maskLineColor);
                  }

                  for (const auto& marker : markers) {
                      renderer_->drawPoint(marker.pos.x, marker.pos.y, marker.radius + 3.0f, maskPointShadowColor);
                      renderer_->drawPoint(marker.pos.x, marker.pos.y, marker.radius, marker.color);
                  }
              }
          }
      }
    } else {
      gizmo_->setLayer(nullptr);
    }
  }

  if (renderer_ && !selectedIds.isEmpty()) {
    const auto layersForOverlay = comp ? comp->allLayer() : QVector<ArtifactAbstractLayerPtr>{};
    const FloatColor primaryColor{1.0f, 0.72f, 0.22f, 1.0f};
    const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};
    for (const auto& layer : layersForOverlay) {
      if (!layer || !layer->isVisible() || !layer->isActiveAt(currentFrame)) {
        continue;
      }
      if (!isLayerSelected(selectedIds, layer)) {
        continue;
      }

      const QRectF bbox = layer->transformedBoundingBox();
      if (!bbox.isValid()) {
        continue;
      }

      const bool primary = !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;
      renderer_->drawRectOutline(static_cast<float>(bbox.left()),
                                 static_cast<float>(bbox.top()),
                                 static_cast<float>(bbox.width()),
                                 static_cast<float>(bbox.height()),
                                 primary ? primaryColor : secondaryColor);
    }
  }

  if (renderer_ && isRubberBandSelecting_) {
    const QRectF rubberBandRect = rubberBandCanvasRect().normalized();
    if (rubberBandRect.isValid() && rubberBandRect.width() > 0.0f && rubberBandRect.height() > 0.0f) {
      renderer_->drawSolidRect(static_cast<float>(rubberBandRect.left()),
                               static_cast<float>(rubberBandRect.top()),
                               static_cast<float>(rubberBandRect.width()),
                               static_cast<float>(rubberBandRect.height()),
                               {0.25f, 0.55f, 1.0f, 0.14f},
                               1.0f);
      renderer_->drawRectOutline(static_cast<float>(rubberBandRect.left()),
                                 static_cast<float>(rubberBandRect.top()),
                                 static_cast<float>(rubberBandRect.width()),
                                 static_cast<float>(rubberBandRect.height()),
                                 {0.25f, 0.70f, 1.0f, 0.95f});
    }
  }

  if (showFrameInfo_ && renderer_) {
    const float infoW = 60.0f;
    const float infoH = 14.0f;
    const float infoX = 4.0f;
    const float infoY = lastCanvasHeight_ - infoH - 4.0f;
    renderer_->drawSolidRect(infoX, infoY, infoW, infoH, {0.0f, 0.0f, 0.0f, 0.6f}, 0.8f);
    const int frame = currentFrame.framePosition();
    const float barRatio = (frame > 0) ? std::min(1.0f, static_cast<float>(frame) / 1000.0f) : 0.0f;
    const float barW = infoW * barRatio;
    if (barW > 1.0f) {
      renderer_->drawSolidRect(infoX, infoY, barW, infoH, {0.2f, 0.6f, 1.0f, 0.5f}, 0.6f);
    }
  }

  if (showSafeMargins_) {
    const float actionSafeW = cw * 0.9f;
    const float actionSafeH = ch * 0.9f;
    const float titleSafeW = cw * 0.8f;
    const float titleSafeH = ch * 0.8f;
    const FloatColor marginColor = {0.5f, 0.5f, 0.5f, 0.6f};

    renderer_->drawRectOutline((cw - actionSafeW) * 0.5f,
                                      (ch - actionSafeH) * 0.5f, actionSafeW,
                                      actionSafeH, marginColor);
    renderer_->drawRectOutline((cw - titleSafeW) * 0.5f,
                                      (ch - titleSafeH) * 0.5f, titleSafeW,
                                      titleSafeH, marginColor);

    const float crossSize = 20.0f;
    renderer_->drawSolidLine({cw * 0.5f - crossSize, ch * 0.5f},
                                    {cw * 0.5f + crossSize, ch * 0.5f},
                                    marginColor, 1.0f);
    renderer_->drawSolidLine({cw * 0.5f, ch * 0.5f - crossSize},
                                    {cw * 0.5f, ch * 0.5f + crossSize},
                                    marginColor, 1.0f);
  }

  if (showGuides_) {
    const FloatColor guideColor = {0.2f, 0.8f, 1.0f, 0.7f};
    for (float x : guideVerticals_) {
      if (x >= 0 && x <= cw) {
        renderer_->drawSolidLine({x, 0}, {x, ch}, guideColor, 1.0f);
      }
    }
    for (float y : guideHorizontals_) {
      if (y >= 0 && y <= ch) {
        renderer_->drawSolidLine({0, y}, {cw, y}, guideColor, 1.0f);
      }
    }
    if (guideVerticals_.isEmpty() && guideHorizontals_.isEmpty()) {
      renderer_->drawSolidLine({cw * 0.5f, 0}, {cw * 0.5f, ch}, guideColor, 1.0f);
      renderer_->drawSolidLine({0, ch * 0.5f}, {cw, ch * 0.5f}, guideColor, 1.0f);
    }
  }
  overlayMs = markPhaseMs();

  flushMs = 0;

  if (!lastVideoDebug_.isEmpty() && lastVideoDebug_ != lastEmittedVideoDebug_) {
    lastEmittedVideoDebug_ = lastVideoDebug_;
    Q_EMIT owner->videoDebugMessage(lastVideoDebug_);
  }

  renderer_->flushAndWait();
  renderer_->present();
  presentMs = markPhaseMs();
  lastFinalPresentKey_ = renderKey;

  ++renderFrameCounter_;
  const qint64 frameMs = frameTimer.elapsed();
  if (frameMs >= 16) {
    qInfo() << "[CompositionView][Perf]"
            << "frameMs=" << frameMs
            << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            << "layersDrawn=" << drawnLayerCount
            << "surfaceUploadLayers=" << surfaceUploadLayerCount
            << "cpuRasterLayers=" << cpuRasterLayerCount
            << "frameOutOfRange=" << frameOutOfRange
            << "previewDownsample=" << previewDownsample_
            << "effectivePreviewDownsample=" << effectivePreviewDownsample
            << "viewportInteracting=" << viewportInteracting_
            << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))
            << "pipelineSize=" << QSize(static_cast<int>(renderPipeline_.width()),
                                        static_cast<int>(renderPipeline_.height()))
            << "hostSize=" << QSize(static_cast<int>(hostWidth_),
                                    static_cast<int>(hostHeight_))
            << "setupMs=" << setupMs
            << "basePassMs=" << basePassMs
            << "layerPassMs=" << layerPassMs
            << "overlayMs=" << overlayMs
            << "flushMs=" << flushMs
            << "presentMs=" << presentMs;
  } else if (compositionViewLog().isDebugEnabled() &&
             (renderFrameCounter_ % 120u) == 0u) {
    qCDebug(compositionViewLog) << "[CompositionView][Perf]"
                                << "frameMs=" << frameMs
                                << "pipelineEnabled=" << pipelineEnabled
                                << "layersTotal=" << layers.size()
                                << "layersDrawn=" << drawnLayerCount
                                << "surfaceUploadLayers=" << surfaceUploadLayerCount
                                << "cpuRasterLayers=" << cpuRasterLayerCount
                                << "previewDownsample=" << previewDownsample_
                                << "effectivePreviewDownsample=" << effectivePreviewDownsample
                                << "viewportInteracting=" << viewportInteracting_
                                << "pipelineSize=" << QSize(static_cast<int>(renderPipeline_.width()),
                                                            static_cast<int>(renderPipeline_.height()))
                                << "setupMs=" << setupMs
                                << "basePassMs=" << basePassMs
                                << "layerPassMs=" << layerPassMs
                                << "overlayMs=" << overlayMs
                                << "flushMs=" << flushMs
                                << "presentMs=" << presentMs;
  }
}
}

} // namespace Artifact
