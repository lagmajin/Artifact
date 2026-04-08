module;
#include <DeviceContext.h>
#define NOMINMAX
#define QT_NO_KEYWORDS

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QMutex>
#include <QPainter>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QTransform>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.Config;
import Artifact.Render.ROI;
import Artifact.Render.Context;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Layer.Composition;
import Artifact.Layers.Model3D;
import Artifact.Render.Offscreen;
import Frame.Position;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Geometry.CameraGuide;
import Artifact.Tool.Manager;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Utils.Id;
import Time.Rational;
import Artifact.Render.Pipeline;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;
import Widgets.Utils.CSS;

import Artifact.Service.Project;
import Artifact.Service.Playback; // 追加
import Event.Bus;
import Artifact.Event.Types;
import Undo.UndoManager;
import Frame.Position;
import Color.Float;
import Image;
import CvUtils;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

namespace {
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

QImage makeSolidColorSprite(const FloatColor &color) {
  QImage image(1, 1, QImage::Format_RGBA8888);
  image.fill(QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));
  return image;
}

enum class SelectionMode { Replace, Add, Toggle };

enum class LayerDragMode { None, Move, ScaleTL, ScaleTR, ScaleBL, ScaleBR };

QColor toQColor(const FloatColor &color) {
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

bool isScaleHandle(TransformGizmo::HandleType handle) {
  switch (handle) {
  case TransformGizmo::HandleType::Scale_TL:
  case TransformGizmo::HandleType::Scale_TR:
  case TransformGizmo::HandleType::Scale_BL:
  case TransformGizmo::HandleType::Scale_BR:
  case TransformGizmo::HandleType::Scale_T:
  case TransformGizmo::HandleType::Scale_B:
  case TransformGizmo::HandleType::Scale_L:
  case TransformGizmo::HandleType::Scale_R:
    return true;
  default:
    return false;
  }
}
} // namespace

QString buildLayerSurfaceCacheKey(ArtifactAbstractLayer *layer,
                                  const QImage &surface, int64_t frameNumber) {
  if (!layer) {
    return QString();
  }

  QString key = layer->id().toString();
  key +=
      QStringLiteral("|size=%1x%2").arg(surface.width()).arg(surface.height());

  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {
    const QRectF bounds = solid2D->localBounds();
    key += QStringLiteral("|solid2D|color=%1|bounds=%2x%3")
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(solid2D->color().r(), 0, 'f', 4)
                        .arg(solid2D->color().g(), 0, 'f', 4)
                        .arg(solid2D->color().b(), 0, 'f', 4)
                        .arg(solid2D->color().a(), 0, 'f', 4))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    const QRectF bounds = solidImage->localBounds();
    key += QStringLiteral("|solidImage|color=%1|bounds=%2x%3")
               .arg(QStringLiteral("%1,%2,%3,%4")
                        .arg(solidImage->color().r(), 0, 'f', 4)
                        .arg(solidImage->color().g(), 0, 'f', 4)
                        .arg(solidImage->color().b(), 0, 'f', 4)
                        .arg(solidImage->color().a(), 0, 'f', 4))
               .arg(bounds.width(), 0, 'f', 2)
               .arg(bounds.height(), 0, 'f', 2);
    return key;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {
    key += QStringLiteral("|image|src=%1|fit=%2|size=%3x%4")
               .arg(imageLayer->sourcePath())
               .arg(imageLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {
    key += QStringLiteral("|svg|src=%1|fit=%2|size=%3x%4")
               .arg(svgLayer->sourcePath())
               .arg(svgLayer->fitToLayer() ? 1 : 0)
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
    key += QStringLiteral("|video|src=%1|frame=%2|proxy=%3|size=%4x%5")
               .arg(videoLayer->sourcePath())
               .arg(frameNumber)
               .arg(static_cast<int>(videoLayer->proxyQuality()))
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    key += QStringLiteral("|text|value=%1|surface=%2x%3")
               .arg(textLayer->text().toQString())
               .arg(surface.width())
               .arg(surface.height());
    return key;
  }

  return key;
}

QRectF viewportRectToCanvasRect(ArtifactIRenderer *renderer,
                                const QPointF &startViewportPos,
                                const QPointF &endViewportPos) {
  if (!renderer) {
    return QRectF();
  }

  const auto a =
      renderer->viewportToCanvas({static_cast<float>(startViewportPos.x()),
                                  static_cast<float>(startViewportPos.y())});
  const auto b =
      renderer->viewportToCanvas({static_cast<float>(endViewportPos.x()),
                                  static_cast<float>(endViewportPos.y())});
  return QRectF(QPointF(std::min(a.x, b.x), std::min(a.y, b.y)),
                QPointF(std::max(a.x, b.x), std::max(a.y, b.y)));
}

DetailLevel detailLevelFromZoom(float zoom) {
  if (zoom < 0.50f) {
    return DetailLevel::Low;
  }
  if (zoom < 0.90f) {
    return DetailLevel::Medium;
  }
  return DetailLevel::High;
}

SelectionMode selectionModeFromModifiers(Qt::KeyboardModifiers modifiers) {
  if (modifiers.testFlag(Qt::ControlModifier)) {
    return SelectionMode::Toggle;
  }
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    return SelectionMode::Add;
  }
  return SelectionMode::Replace;
}

QStringList selectedLayerIdList() {
  QStringList ids;
  auto *app = ArtifactApplicationManager::instance();
  auto *selection = app ? app->layerSelectionManager() : nullptr;
  if (!selection) {
    return ids;
  }

  for (const auto &layer : selection->selectedLayers()) {
    if (layer) {
      ids.push_back(layer->id().toString());
    }
  }
  return ids;
}

bool isLayerSelected(const QStringList &selectedIds,
                     const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return false;
  }
  const QString layerId = layer->id().toString();
  for (const auto &selectedId : selectedIds) {
    if (selectedId == layerId) {
      return true;
    }
  }
  return false;
}

enum class MotionPathSampleKind { Keyframe, Current };

struct MotionPathSample {
  QPointF position;
  MotionPathSampleKind kind = MotionPathSampleKind::Keyframe;
  int64_t framePosition = -1;
};

QVector3D unprojectClipCorner(const QMatrix4x4 &invViewProj, float x, float y,
                              float z) {
  const QVector4D clipPos(x, y, z, 1.0f);
  const QVector4D worldPos = invViewProj * clipPos;
  if (std::abs(worldPos.w()) < 1e-6f) {
    return {};
  }
  return {worldPos.x() / worldPos.w(), worldPos.y() / worldPos.w(),
          worldPos.z() / worldPos.w()};
}

CompositionRenderController::CameraFrustumVisual
buildCameraFrustumVisual(const ArtifactCompositionPtr &comp,
                         const LayerID &selectedLayerId) {
  CompositionRenderController::CameraFrustumVisual visual;
  if (!comp || selectedLayerId.isNil()) {
    return visual;
  }

  auto layer = comp->layerById(selectedLayerId);
  auto *layerPtr = layer.get();
  const auto camera = dynamic_cast<ArtifactCameraLayer *>(layerPtr);
  if (!camera) {
    return visual;
  }

  const auto size = comp->settings().compositionSize();
  const float width =
      static_cast<float>(size.width() > 0 ? size.width() : 1920);
  const float height =
      static_cast<float>(size.height() > 0 ? size.height() : 1080);
  const float aspect = std::max(0.001f, width / std::max(0.001f, height));

  const QMatrix4x4 view = camera->viewMatrix();
  const QMatrix4x4 proj = camera->projectionMatrix(aspect);
  bool invertible = false;
  const QMatrix4x4 invViewProj = (proj * view).inverted(&invertible);
  if (!invertible) {
    return visual;
  }

  visual.valid = true;
  visual.layerId = camera->id();
  visual.cameraPosition = view.inverted().map(QVector3D(0.0f, 0.0f, 0.0f));
  visual.viewMatrix = view;
  visual.projectionMatrix = proj;
  visual.guide = makeNukeStyleCameraGuidePrimitive();
  visual.aspect = aspect;
  visual.zoom = camera->zoom();

  visual.nearPlaneCorners.reserve(4);
  visual.farPlaneCorners.reserve(4);
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, -1.0f));
  visual.nearPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, -1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, -1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, 1.0f, 1.0f, 1.0f));
  visual.farPlaneCorners.push_back(
      unprojectClipCorner(invViewProj, -1.0f, 1.0f, 1.0f));
  return visual;
}

QPointF motionPathAnchorPositionAt(const ArtifactAbstractLayerPtr &layer,
                                   const RationalTime &time) {
  if (!layer) {
    return {};
  }

  const auto &transform = layer->transform3D();
  const float anchorX = transform.anchorXAt(time);
  const float anchorY = transform.anchorYAt(time);
  const float anchorZ = transform.anchorZAt(time);
  const auto local = transform.getAllMatrixAt(time);
  return QPointF(anchorX * local.m00 + anchorY * local.m10 +
                     anchorZ * local.m20 + local.m30,
                 anchorX * local.m01 + anchorY * local.m11 +
                     anchorZ * local.m21 + local.m31);
}

// Forward declaration
FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp);

QVector<MotionPathSample>
buildMotionPathSamples(const ArtifactAbstractLayerPtr &layer,
                       const ArtifactCompositionPtr &comp) {
  QVector<MotionPathSample> samples;
  if (!layer || !comp) {
    return samples;
  }

  const auto keyTimes = layer->transform3D().getPositionKeyFrameTimes();
  if (keyTimes.empty()) {
    return samples;
  }

  samples.reserve(static_cast<int>(keyTimes.size()) + 1);
  const int fps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));

  for (const auto &time : keyTimes) {
    samples.push_back({motionPathAnchorPositionAt(layer, time),
                       MotionPathSampleKind::Keyframe, time.value()});
  }

  const FramePosition currentFrame = currentFrameForComposition(comp);
  const RationalTime currentTime(currentFrame.framePosition(), fps);
  samples.push_back({motionPathAnchorPositionAt(layer, currentTime),
                     MotionPathSampleKind::Current,
                     currentFrame.framePosition()});

  return samples;
}

LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr &layer,
                                   const QPointF &viewportPos,
                                   ArtifactIRenderer *renderer) {
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

  if (containsHandle(static_cast<float>(bbox.left()),
                     static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTL;
  }
  if (containsHandle(static_cast<float>(bbox.right()),
                     static_cast<float>(bbox.top()))) {
    return LayerDragMode::ScaleTR;
  }
  if (containsHandle(static_cast<float>(bbox.left()),
                     static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBL;
  }
  if (containsHandle(static_cast<float>(bbox.right()),
                     static_cast<float>(bbox.bottom()))) {
    return LayerDragMode::ScaleBR;
  }

  return LayerDragMode::Move;
}

bool layerIntersectsCanvasRect(const ArtifactAbstractLayerPtr &layer,
                               const QRectF &rect,
                               const FramePosition &currentFrame) {
  if (!layer || !rect.isValid() || !layer->isVisible() ||
      !layer->isActiveAt(currentFrame)) {
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

FramePosition currentFrameForComposition(const ArtifactCompositionPtr &comp) {
  if (!comp) {
    return FramePosition(0);
  }
  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }
  return currentFrame;
}

ArtifactAbstractLayerPtr
hitTopmostLayerAtViewportPos(const ArtifactCompositionPtr &comp,
                             ArtifactIRenderer *renderer,
                             const QPointF &viewportPos) {
  if (!comp || !renderer) {
    return ArtifactAbstractLayerPtr();
  }

  const auto currentFrame = currentFrameForComposition(comp);
  const auto canvasPos =
      renderer->viewportToCanvas({static_cast<float>(viewportPos.x()),
                                  static_cast<float>(viewportPos.y())});
  const auto layers = comp->allLayer();
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
    const auto &layer = layers[static_cast<size_t>(i)];
    if (!layer || !layer->isVisible() || !layer->isActiveAt(currentFrame)) {
      continue;
    }

    const QTransform globalTransform = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);
    if (invertible) {
      const QPointF localPos =
          invTransform.map(QPointF(canvasPos.x, canvasPos.y));
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

void drawLayerForCompositionView(
    ArtifactAbstractLayer *layer, ArtifactIRenderer *renderer,
    float opacityOverride = -1.0f, QString *videoDebugOut = nullptr,
    QHash<QString, LayerSurfaceCacheEntry> *surfaceCache = nullptr,
    GPUTextureCacheManager *gpuTextureCacheManager = nullptr,
    int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),
    bool useGpuPath = false, const DetailLevel lod = DetailLevel::High,
    const QMatrix4x4 *cameraView = nullptr,
    const QMatrix4x4 *cameraProj = nullptr) {
  if (!layer || !renderer) {
    qCDebug(compositionViewLog)
        << "[CompositionView] drawLayerForCompositionView: invalid "
           "layer/renderer";
    return;
  }

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

  // Handle 3D layers separately
  if (layer->is3D()) {
    if (cameraView && cameraProj) {
      renderer->set3DCameraMatrices(*cameraView, *cameraProj);
    }
    layer->draw(renderer);
    if (cameraView && cameraProj) {
      renderer->reset3DCameraMatrices();
    }
    return;
  }

  auto applyRasterizerEffectsAndMasksToSurface =
      [&](ArtifactAbstractLayer *targetLayer, QImage &surface) {
        if (!targetLayer || surface.isNull()) {
          return;
        }

        const bool hasMasks = targetLayer->hasMasks();
        const auto effects = targetLayer->getEffects();
        bool hasRasterizerEffect = false;
        for (const auto &effect : effects) {
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

          for (const auto &effect : effects) {
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

  auto hasRasterizerEffectsOrMasks =
      [](ArtifactAbstractLayer *targetLayer) {
        if (!targetLayer) {
          return false;
        }
        if (targetLayer->hasMasks()) {
          return true;
        }

        for (const auto &effect : targetLayer->getEffects()) {
          if (effect && effect->isEnabled() &&
              effect->pipelineStage() == EffectPipelineStage::Rasterizer) {
            return true;
          }
        }
        return false;
      };

  auto applySurfaceAndDraw = [&](QImage surface, const QRectF &rect,
                                 bool allowSurfaceCache) {
    if (surface.isNull()) {
      return false;
    }

    const QString ownerId = layer->id().toString();
    const QString cacheSignature =
        buildLayerSurfaceCacheKey(layer, surface, cacheFrameNumber);
    LayerSurfaceCacheEntry *cacheEntry = nullptr;

    if (surfaceCache && !cacheSignature.isEmpty()) {
      auto cacheIt = surfaceCache->find(ownerId);
      if (cacheIt != surfaceCache->end() && cacheIt->ownerId == ownerId &&
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
        if (gpuTextureCacheManager &&
            layerUsesGpuTextureCacheForCompositionView(layer)) {
          entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
              ownerId, cacheSignature, surface);
        }
        (*surfaceCache)[ownerId] = entry;
        cacheEntry = &(*surfaceCache)[ownerId];
      }
    } else if (allowSurfaceCache) {
      applyRasterizerEffectsAndMasksToSurface(layer, surface);
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
              cacheEntry->gpuTextureHandle =
                  gpuTextureCacheManager->acquireOrCreate(
                      layer->id().toString(), cacheSignature, uploadSurface);
            }
            if (auto *srv = gpuTextureCacheManager->textureView(
                    cacheEntry->gpuTextureHandle)) {
              renderer->drawSpriteTransformed(
                  static_cast<float>(rect.x()), static_cast<float>(rect.y()),
                  static_cast<float>(rect.width()),
                  static_cast<float>(rect.height()), instanceTransform, srv,
                  finalOpacity);
              return;
            }
          }

          renderer->drawSpriteTransformed(
              static_cast<float>(rect.x()), static_cast<float>(rect.y()),
              static_cast<float>(rect.width()),
              static_cast<float>(rect.height()), instanceTransform, surface,
              finalOpacity);
        });
    return true;
  };

  if (auto *solid2D = dynamic_cast<ArtifactSolid2DLayer *>(layer)) {
    const auto color = solid2D->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, false);
    } else {
      renderer->drawSolidRectTransformed(
          static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
          static_cast<float>(localRect.width()),
          static_cast<float>(localRect.height()), globalTransform4x4, color,
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity()));
    }
    return;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    // [Fix] SolidImage も QImage 経由で GPU テクスチャキャッシュを利用する
    const QImage img = solidImage->toQImage();
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
    } else {
      // Fallback: 直接描画
      const auto color = solidImage->color();
      renderer->drawSolidRectTransformed(
          static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
          static_cast<float>(localRect.width()),
          static_cast<float>(localRect.height()), globalTransform4x4, color,
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity()));
    }
    return;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {
    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {
    if (svgLayer->isLoaded()) {
      const QImage svgImage = svgLayer->toQImage();
      if (!svgImage.isNull()) {
        applySurfaceAndDraw(svgImage, localRect,
                            hasRasterizerEffectsOrMasks(layer));
      } else {
        svgLayer->draw(renderer);
      }
      return;
    }
  }

  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
    const QImage frame = videoLayer->currentFrameToQImage();
    // デバッグ文字列生成は
    // デバッグカテゴリ有効時のみ実行（毎フレームのコスト削減）
    if (videoDebugOut) {
      const bool loaded = videoLayer->isLoaded();
      const int64_t cf = layer->currentFrame();
      const FramePosition ip = layer->inPoint();
      const FramePosition op = layer->outPoint();
      const bool active =
          layer->isActiveAt(FramePosition(static_cast<int>(cf)));
      *videoDebugOut = QString("[Video] loaded=%1 frame.isNull=%2 size=%3x%4 "
                               "active=%5 range=[%6,%7] curFrame=%8")
                           .arg(loaded)
                           .arg(frame.isNull())
                           .arg(frame.isNull() ? 0 : frame.width())
                           .arg(frame.isNull() ? 0 : frame.height())
                           .arg(active)
                           .arg(ip.framePosition())
                           .arg(op.framePosition())
                           .arg(cf);
    }
    if (!frame.isNull()) {
      applySurfaceAndDraw(frame, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect,
                          hasRasterizerEffectsOrMasks(layer));
    }
    return;
  }

  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer)) {
    if (auto childComp = compLayer->sourceComposition()) {
      const QSize childSize = childComp->settings().compositionSize();
      const int64_t childFrame =
          layer->currentFrame() - layer->inPoint().framePosition();
      childComp->goToFrame(childFrame);
      QImage childImage =
          childComp->getThumbnail(childSize.width(), childSize.height());

      if (!childImage.isNull()) {
        applySurfaceAndDraw(childImage, localRect,
                            hasRasterizerEffectsOrMasks(layer));
      }
    }
    return;
  }

  // Fallback for layer types without a direct surface accessor.
  qCDebug(compositionViewLog) << "[CompositionView] fallback layer draw"
                              << "id=" << layer->id().toString()
                              << "type=" << layer->type_index().name();
  layer->draw(renderer);
}

void drawCompositionRegionOverlay(ArtifactIRenderer* renderer,
                                  const ArtifactCompositionPtr& comp,
                                  bool showCheckerboard) {
  if (!renderer || !comp) {
    return;
  }

  const QSize compSize = comp->settings().compositionSize();
  const float cw =
      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
  const float ch =
      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  if (cw <= 0.0f || ch <= 0.0f) {
    return;
  }

  if (showCheckerboard) {
    renderer->drawCheckerboard(
        0.0f, 0.0f, cw, ch, 16.0f, {0.18f, 0.18f, 0.18f, 0.08f},
        {0.24f, 0.24f, 0.24f, 0.05f});
  }

  renderer->drawRectOutline(0.0f, 0.0f, cw, ch,
                            FloatColor{0.02f, 0.02f, 0.02f, 0.85f});
  renderer->drawRectOutline(0.0f, 0.0f, cw, ch,
                            FloatColor{0.42f, 0.68f, 0.96f, 0.95f});
}

// CompositionChangeDetector - 差分レンダリング用の変更検出器
class CompositionChangeDetector {
private:
  QSet<QString> changedLayers_;
  bool compositionSettingsChanged_ = false;
  mutable QMutex mutex_; // スレッドセーフ

public:
  // レイヤー変更をマーク
  void markLayerChanged(const QString &layerId) {
    QMutexLocker locker(&mutex_);
    changedLayers_.insert(layerId);
  }

  // Composition設定変更をマーク
  void markCompositionChanged() {
    QMutexLocker locker(&mutex_);
    compositionSettingsChanged_ = true;
  }

  // 全再描画が必要か判定
  bool needsFullRedraw() const {
    QMutexLocker locker(&mutex_);
    return compositionSettingsChanged_ || changedLayers_.size() > 2;
  }

  // 変更されたレイヤー一覧を取得
  QSet<QString> getChangedLayers() const {
    QMutexLocker locker(&mutex_);
    return changedLayers_;
  }

  // 変更状態をリセット
  void reset() {
    QMutexLocker locker(&mutex_);
    changedLayers_.clear();
    compositionSettingsChanged_ = false;
  }

  // デバッグ情報
  QString debugInfo() const {
    QMutexLocker locker(&mutex_);
    return QString("ChangedLayers: %1, CompositionChanged: %2")
        .arg(changedLayers_.size())
        .arg(compositionSettingsChanged_);
  }
};

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
  float devicePixelRatio_ = 1.0f;
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

  // 変更検出器 (差分レンダリング用)
  CompositionChangeDetector changeDetector_;

  // LOD (Level of Detail)
  std::unique_ptr<LODManager> lodManager_;
  bool lodEnabled_ = true;

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
  bool renderQueueActive_ = false; // When true, suppress cache invalidation during Render Queue
  int lastPipelineStateMask_ = -1;
  QSize lastDispatchWarningSize_;
  QByteArray lastFinalPresentKey_;
  quint64 baseInvalidationSerial_ = 1;
  quint64 overlayInvalidationSerial_ = 1;
  bool dropGhostVisible_ = false;
  QRectF dropGhostRect_;
  QString dropGhostTitle_;
  QString dropGhostHint_;
  QString dropCandidateLabel_;
  bool infoOverlayVisible_ = false;
  QString infoOverlayTitle_;
  QString infoOverlayDetail_;
  FloatColor clearColor_;
  QString lastBackgroundKey_;
  CompositionID lastBackgroundCompositionId_;
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
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
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

  // ROI debug state
  bool debugMode_ = false; // ROI デバッグ表示フラグ

  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr &layer) {
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

    if (auto *undo = UndoManager::instance()) {
      undo->push(std::make_unique<MaskEditCommand>(layer, maskEditBefore_,
                                                   std::move(afterMasks)));
    }

    maskEditBefore_.clear();
    maskEditDirty_ = false;
  }

  void
  syncSelectedLayerOverlayState(const ArtifactCompositionPtr &composition) {
    ArtifactAbstractLayerPtr layer;
    if (composition && !selectedLayerId_.isNil()) {
      layer = composition->layerById(selectedLayerId_);
    }

    if (gizmo_) {
      gizmo_->setLayer(layer);
    }

    if (gizmo3D_ && layer && layer->is3D()) {
      syncGizmo3DFromLayer(layer);
    }
  }

  void syncGizmo3DFromLayer(const ArtifactAbstractLayerPtr &layer) {
    if (!gizmo3D_ || !layer) {
      return;
    }

    if (layer->is3D()) {
      const auto &t3 = layer->transform3D();
      gizmo3D_->setDepthEnabled(true);
      gizmo3D_->setTransform(layer->position3D(), layer->rotation3D());
      gizmo3D_->setScale(QVector3D(t3.scaleX(), t3.scaleY(), 1.0f));
      return;
    }

    const QRectF localRect = layer->localBounds();
    const QTransform globalTransform = layer->getGlobalTransform();
    const QPointF center =
        localRect.isValid()
            ? globalTransform.map(localRect.center())
            : QPointF(globalTransform.dx(), globalTransform.dy());
    const float scaleX = std::max<float>(
        0.01f, static_cast<float>(
                   std::hypot(globalTransform.m11(), globalTransform.m12())));
    const float scaleY = std::max<float>(
        0.01f, static_cast<float>(
                   std::hypot(globalTransform.m21(), globalTransform.m22())));
    const float rotationZ =
        std::atan2(globalTransform.m12(), globalTransform.m11()) *
        (180.0f / 3.14159265358979323846f);

    gizmo3D_->setDepthEnabled(false);
    gizmo3D_->setTransform(QVector3D(static_cast<float>(center.x()),
                                     static_cast<float>(center.y()), 0.0f),
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

    // Ensure renderer is initialized before any rendering operations
    if (!renderer_->isInitialized() && hostWidget_) {
      renderer_->initialize(hostWidget_.data());
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
      renderPipeline_.initialize(device, static_cast<Uint32>(cw),
                                 static_cast<Uint32>(ch),
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
          if (renderQueueActive_) {
            return; // Render Queue 実行中はキャッシュクリアをスキップ
          }
          surfaceCache_.clear();
          if (gpuTextureCacheManager_) {
            gpuTextureCacheManager_->clear();
          }
          invalidateBaseComposite();
          applyCompositionState(composition);
          owner->renderOneFrame();
        });
  }

  void invalidateLayerSurfaceCache(const ArtifactAbstractLayerPtr &layer) {
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

  void drawViewportGhostOverlay(CompositionRenderController *owner,
                                const ArtifactCompositionPtr &comp,
                                const ArtifactAbstractLayerPtr &selectedLayer,
                                const FramePosition &currentFrame);

  // 変更検出器へのアクセス (デバッグ用)
  const CompositionChangeDetector &changeDetector() const {
    return changeDetector_;
  }

  CompositionChangeDetector &changeDetector() { return changeDetector_; }

  void renderOneFrameImpl(CompositionRenderController *owner);
};

CompositionRenderController::CompositionRenderController(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  const auto theme = ArtifactCore::currentDCCTheme();
  impl_->clearColor_ = FloatColor{
      QColor(theme.backgroundColor).redF(),
      QColor(theme.backgroundColor).greenF(),
      QColor(theme.backgroundColor).blueF(),
      1.0f};
  impl_->gizmo_ = std::make_unique<TransformGizmo>();
  impl_->gizmo3D_ = std::make_unique<Artifact3DGizmo>(this);

  // Connect to project service to track layer selection
  if (auto *svc = ArtifactProjectService::instance()) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
            [this](const LayerSelectionChangedEvent& event) {
              auto comp = impl_->previewPipeline_.composition();
              if (comp && !event.compositionId.isEmpty() &&
                  comp->id().toString() != event.compositionId) {
                return;
              }
              setSelectedLayerId(LayerID(event.layerId));
            }));

    // Ensure layers created after setComposition() are also bound to redraw.
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>(
            [this](const LayerChangedEvent& event) {
              if (event.changeType != LayerChangedEvent::ChangeType::Created) {
                return;
              }
              auto comp = impl_->previewPipeline_.composition();
              if (!comp || event.compositionId.isEmpty() ||
                  comp->id().toString() != event.compositionId) {
                return;
              }
              const auto layerId = LayerID(event.layerId);
              if (auto layer = comp->layerById(layerId)) {
                impl_->layerChangedConnections_.push_back(
                    connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                            [this, layer]() {
                              impl_->invalidateLayerSurfaceCache(layer);
                              impl_->invalidateBaseComposite();
                              impl_->syncSelectedLayerOverlayState(
                                  impl_->previewPipeline_.composition());

                              // 変更検出器にマーク (実装のみ、レンダー接続なし)
                              impl_->changeDetector_.markLayerChanged(
                                  layer->id().toString());

                              renderOneFrame();
                            }));
              }
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
            [this, svc](const CurrentCompositionChangedEvent& event) {
              ArtifactCompositionPtr comp;
              if (!event.compositionId.trimmed().isEmpty()) {
                const auto found = svc->findComposition(CompositionID(event.compositionId));
                if (found.success && !found.ptr.expired()) {
                  comp = found.ptr.lock();
                }
              }
              if (!comp) {
                comp = resolvePreferredComposition(svc);
              }
              setComposition(comp);
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ProjectChangedEvent>(
            [this, svc](const ProjectChangedEvent&) {
              auto latest = resolvePreferredComposition(svc);
              auto current = impl_->previewPipeline_.composition();
              if (latest != current) {
                setComposition(latest);
              } else {
                impl_->invalidateBaseComposite();
              }
            }));

    // Handle resolution changes via internal event bus
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<PreviewQualityPresetChangedEvent>(
            [this](const PreviewQualityPresetChangedEvent& event) {
              setPreviewQualityPreset(static_cast<PreviewQualityPreset>(event.preset));
            }));

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
  impl_->devicePixelRatio_ = static_cast<float>(hostWidget->devicePixelRatio());
  impl_->hostWidth_ = static_cast<float>(hostWidget->width()) * impl_->devicePixelRatio_;
  impl_->hostHeight_ = static_cast<float>(hostWidget->height()) * impl_->devicePixelRatio_;
  impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);

  const auto comp = impl_->previewPipeline_.composition();
  if (comp) {
    impl_->applyCompositionState(comp);
    impl_->renderer_->fillToViewport();
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
            [this](const FramePosition &position) {
              // 1. 可視性チェック: 非表示（他タブの裏など）なら描画しない
              if (auto *owner = qobject_cast<QWidget *>(parent())) {
                if (!owner->isVisible())
                  return;
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
      impl_->gpuContext_ =
          std::make_unique<ArtifactCore::GpuContext>(device, ctx);
      impl_->blendPipeline_ =
          std::make_unique<ArtifactCore::LayerBlendPipeline>(
              *impl_->gpuContext_);
      impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
      // [Fix D] qCDebug → qDebug/qWarning に升格（カテゴリ有効化不要）
      if (impl_->blendPipelineReady_) {
        qDebug() << "[CompositionView] LayerBlendPipeline initialized OK."
                 << "executors ready for GPU blend path.";
      } else {
        qWarning()
            << "[CompositionView] LayerBlendPipeline FAILED to initialize."
            << "Will fall back to CPU compositing path.";
      }
    } else {
      qWarning() << "[CompositionView] immediateContext() is null - blend "
                    "pipeline skipped.";
    }
  } else {
    qWarning()
        << "[CompositionView] device() is null - blend pipeline skipped.";
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

void CompositionRenderController::setViewportSize(float w, float h) {
  if (!impl_->renderer_) {
    return;
  }
  // Refresh DPR whenever the viewport is resized (handles window-to-monitor changes)
  if (impl_->hostWidget_) {
    impl_->devicePixelRatio_ = static_cast<float>(impl_->hostWidget_->devicePixelRatio());
  }
  // Callers pass logical pixels; convert to physical pixels for the renderer
  const float newHostWidth = w * impl_->devicePixelRatio_;
  const float newHostHeight = h * impl_->devicePixelRatio_;
  if (std::abs(newHostWidth - impl_->hostWidth_) < 0.5f &&
      std::abs(newHostHeight - impl_->hostHeight_) < 0.5f) {
    return;
  }
  impl_->hostWidth_ = newHostWidth;
  impl_->hostHeight_ = newHostHeight;
  impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setPreviewQualityPreset(
    PreviewQualityPreset preset) {
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
    if (impl_->hostWidth_ > 0 && impl_->hostHeight_ > 0) {
      // hostWidth_/hostHeight_ are already physical pixels; call renderer directly
      impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);
    }
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::panBy(const QPointF &viewportDelta) {
  if (!impl_->renderer_) {
    return;
  }
  // viewportDelta is in logical pixels from Qt; convert to physical for the renderer
  impl_->renderer_->panBy((float)viewportDelta.x() * impl_->devicePixelRatio_,
                           (float)viewportDelta.y() * impl_->devicePixelRatio_);
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
    impl_->renderer_->fillToViewport();

    // 各レイヤーの変更を監視
    for (auto &layer : composition->allLayer()) {
      if (layer) {
        impl_->layerChangedConnections_.push_back(
            connect(layer.get(), &ArtifactAbstractLayer::changed, this,
                    [this, layer]() {
                      impl_->invalidateLayerSurfaceCache(layer);
                      impl_->invalidateBaseComposite();
                      impl_->syncSelectedLayerOverlayState(
                          impl_->previewPipeline_.composition());
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

LayerID CompositionRenderController::selectedLayerId() const {
  return impl_->selectedLayerId_;
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
  if (toQColor(impl_->clearColor_) == toQColor(color)) {
    return;
  }
  impl_->clearColor_ = color;
  if (impl_->renderer_) {
    impl_->renderer_->setClearColor(color);
  }
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

void CompositionRenderController::setShowGrid(bool show) {
  if (impl_->showGrid_ == show) {
    return;
  }
  impl_->showGrid_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowGrid() const {
  return impl_->showGrid_;
}
void CompositionRenderController::setShowCheckerboard(bool show) {
  if (impl_->showCheckerboard_ == show) {
    return;
  }
  impl_->showCheckerboard_ = show;
  impl_->invalidateBaseComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowCheckerboard() const {
  return impl_->showCheckerboard_;
}
void CompositionRenderController::setShowGuides(bool show) {
  if (impl_->showGuides_ == show) {
    return;
  }
  impl_->showGuides_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowGuides() const {
  return impl_->showGuides_;
}
void CompositionRenderController::setShowSafeMargins(bool show) {
  if (impl_->showSafeMargins_ == show) {
    return;
  }
  impl_->showSafeMargins_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowSafeMargins() const {
  return impl_->showSafeMargins_;
}

void CompositionRenderController::setShowMotionPathOverlay(bool show) {
  if (impl_->showMotionPathOverlay_ == show) {
    return;
  }
  impl_->showMotionPathOverlay_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

bool CompositionRenderController::isShowMotionPathOverlay() const {
  return impl_ ? impl_->showMotionPathOverlay_ : false;
}

void CompositionRenderController::setDropGhostPreview(
    const QRectF &viewportRect, const QString &title, const QString &hint,
    const QString &label) {
  if (!impl_) {
    return;
  }
  if (impl_->dropGhostVisible_ &&
      impl_->dropGhostRect_ == viewportRect &&
      impl_->dropGhostTitle_ == title &&
      impl_->dropGhostHint_ == hint &&
      impl_->dropCandidateLabel_ == label) {
    return;
  }
  impl_->dropGhostVisible_ = true;
  impl_->dropGhostRect_ = viewportRect;
  impl_->dropGhostTitle_ = title;
  impl_->dropGhostHint_ = hint;
  impl_->dropCandidateLabel_ = label;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::clearDropGhostPreview() {
  if (!impl_ ||
      (!impl_->dropGhostVisible_ && impl_->dropGhostRect_.isNull() &&
       impl_->dropGhostTitle_.isEmpty() && impl_->dropGhostHint_.isEmpty() &&
       impl_->dropCandidateLabel_.isEmpty())) {
    return;
  }
  impl_->dropGhostVisible_ = false;
  impl_->dropGhostRect_ = QRectF();
  impl_->dropGhostTitle_.clear();
  impl_->dropGhostHint_.clear();
  impl_->dropCandidateLabel_.clear();
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::setInfoOverlayText(const QString &title,
                                                     const QString &detail) {
  if (!impl_) {
    return;
  }
  const QString normalizedTitle = title.trimmed();
  const QString normalizedDetail = detail.trimmed();
  if (impl_->infoOverlayVisible_ &&
      impl_->infoOverlayTitle_ == normalizedTitle &&
      impl_->infoOverlayDetail_ == normalizedDetail) {
    return;
  }
  impl_->infoOverlayVisible_ = true;
  impl_->infoOverlayTitle_ = normalizedTitle;
  impl_->infoOverlayDetail_ = normalizedDetail;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::clearInfoOverlayText() {
  if (!impl_ ||
      (!impl_->infoOverlayVisible_ && impl_->infoOverlayTitle_.isEmpty() &&
       impl_->infoOverlayDetail_.isEmpty())) {
    return;
  }
  impl_->infoOverlayVisible_ = false;
  impl_->infoOverlayTitle_.clear();
  impl_->infoOverlayDetail_.clear();
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::setGpuBlendEnabled(bool enabled) {
  if (impl_->gpuBlendEnabled_ == enabled) {
    return;
  }
  impl_->gpuBlendEnabled_ = enabled;
  impl_->invalidateBaseComposite();
  qWarning() << "[CompositionView] GPU blend user toggle changed"
             << "enabled=" << impl_->gpuBlendEnabled_ << "envDisable="
             << qEnvironmentVariableIsSet(
                    "ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
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
    // viewportPos is in logical pixels; convert to physical
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x() * impl_->devicePixelRatio_,
         (float)viewportPos.y() * impl_->devicePixelRatio_}, newZoom);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomOutAt(const QPointF &viewportPos) {
  if (impl_->renderer_) {
    notifyViewportInteractionActivity();
    const float currentZoom = impl_->renderer_->getZoom();
    const float newZoom = std::clamp(currentZoom / 1.1f, 0.05f, 64.0f);
    // viewportPos is in logical pixels; convert to physical
    impl_->renderer_->zoomAroundViewportPoint(
        {(float)viewportPos.x() * impl_->devicePixelRatio_,
         (float)viewportPos.y() * impl_->devicePixelRatio_}, newZoom);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::zoomFit() {
  if (impl_->renderer_) {
    impl_->renderer_->fitToViewport(0.0f);
    impl_->invalidateBaseComposite();
    float panX, panY;
    impl_->renderer_->getPan(panX, panY);
    const float zoom = impl_->renderer_->getZoom();
    qDebug() << "[CompositionRenderController] zoomFit done:"
             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"
             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;
    renderOneFrame();
  }
}

void CompositionRenderController::zoomFill() {
  if (impl_->renderer_) {
    impl_->renderer_->fillToViewport();
    impl_->invalidateBaseComposite();
    float panX, panY;
    impl_->renderer_->getPan(panX, panY);
    const float zoom = impl_->renderer_->getZoom();
    qDebug() << "[CompositionRenderController] zoomFill done:"
             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"
             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;
    renderOneFrame();
  }
}

void CompositionRenderController::zoom100() {
  if (impl_->renderer_) {
    impl_->renderer_->setZoom(1.0f);
    // Center the canvas in the viewport at 100% zoom
    const float panX = (impl_->hostWidth_  - impl_->lastCanvasWidth_)  * 0.5f;
    const float panY = (impl_->hostHeight_ - impl_->lastCanvasHeight_) * 0.5f;
    impl_->renderer_->setPan(panX, panY);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

ArtifactIRenderer *CompositionRenderController::renderer() const {
  return impl_->renderer_.get();
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
                           viewH * 0.5f -
                               static_cast<float>(center.y()) * zoom);
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

LayerID CompositionRenderController::layerAtViewportPos(
    const QPointF &viewportPos) const {
  auto comp = impl_->previewPipeline_.composition();
  // viewportPos is in logical pixels; convert to physical for hit testing
  const QPointF physPos = viewportPos * impl_->devicePixelRatio_;
  const auto layer =
      hitTopmostLayerAtViewportPos(comp, impl_->renderer_.get(), physPos);
  return layer ? layer->id() : LayerID::Nil();
}

Ray CompositionRenderController::createPickingRay(
    const QPointF &viewportPos) const {
  if (!impl_->renderer_)
    return {};

  QMatrix4x4 view = impl_->renderer_->getViewMatrix();
  QMatrix4x4 proj = impl_->renderer_->getProjectionMatrix();
  QRect viewport(0, 0, (int)impl_->hostWidth_, (int)impl_->hostHeight_);

  QVector3D nearPos = QVector3D(viewportPos.x(), viewportPos.y(), 0.0f)
                          .unproject(view, proj, viewport);
  QVector3D farPos = QVector3D(viewportPos.x(), viewportPos.y(), 1.0f)
                         .unproject(view, proj, viewport);

  return {nearPos, (farPos - nearPos).normalized()};
}

void CompositionRenderController::handleMousePress(QMouseEvent *event) {
  if (!event || !impl_->renderer_)
    return;

  // event->position() is in logical pixels; convert to physical for rendering pipeline
  const QPointF viewportPos = event->position() * impl_->devicePixelRatio_;

  // 3D Gizmo hit test (GIZ-2)
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  if (selectedLayer && impl_->gizmo3D_) {
    impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
    Ray ray = createPickingRay(viewportPos);
    GizmoAxis axis =
        impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(),
                                 impl_->renderer_->getProjectionMatrix());
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
    auto activeTool =
        toolManager ? toolManager->activeTool() : ToolType::Selection;

    auto comp = impl_->previewPipeline_.composition();
    if (comp && impl_->renderer_) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      auto *selection =
          ArtifactApplicationManager::instance()
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
          const float hitThreshold =
              8.0f / impl_->renderer_->getZoom(); // 8px in viewport space
          for (int m = 0; m < selectedLayer->maskCount(); ++m) {
            LayerMask mask = selectedLayer->mask(m);
            for (int p = 0; p < mask.maskPathCount(); ++p) {
              MaskPath path = mask.maskPath(p);
              for (int v = 0; v < path.vertexCount(); ++v) {
                MaskVertex vertex = path.vertex(v);
                if (QVector2D(vertex.position - localPos).length() <
                    hitThreshold) {
                  // If it's the first vertex and we have more than 2, close the
                  // path
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
            // TODO: Logic to start a new path or insert vertex into existing
            // edge
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

          qDebug() << "[PenTool] Added vertex at local:" << localPos
                   << "layer:" << selectedLayer->id().toString();
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
        auto &layer = layers[i];
        if (!layer || !layer->isVisible())
          continue;
        if (layer->isLocked() && !ignoreLocked)
          continue;
        if (!layer->isActiveAt(currentFrame))
          continue;

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
          const bool sameSpot =
              QVector2D(viewportPos - impl_->lastHitPosition_).length() <
              posThreshold;

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
        const std::shared_ptr<ArtifactAbstractLayer> hitLayerRef = hitLayer;
        if (selection) {
          const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
          const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
          if (ctrl) {
            if (selection->isSelected(hitLayerRef)) {
              selection->removeFromSelection(hitLayerRef);
            } else {
              selection->addToSelection(hitLayerRef);
            }
          } else if (shift) {
            selection->addToSelection(hitLayerRef);
          } else {
            if (auto *svc = ArtifactProjectService::instance()) {
              svc->selectLayer(hitLayer->id());
            } else {
              selection->selectLayer(hitLayerRef);
            }
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
          impl_->dragGroupMove_ = selection &&
                                  selection->selectedLayers().size() > 1 &&
                                  selection->isSelected(hitLayerRef);
          impl_->dragGroupLayers_.clear();
          impl_->dragGroupStartPositions_.clear();
          if (impl_->dragGroupMove_ && selection) {
            const auto selected = selection->selectedLayers();
            impl_->dragGroupLayers_.reserve(selected.size());
            for (const auto &layer : selected) {
              if (!layer) {
                continue;
              }
              const QString id = layer->id().toString();
              impl_->dragGroupLayers_.push_back(layer);
              impl_->dragGroupStartPositions_.insert(
                  id, QPointF(layer->transform3D().positionX(),
                              layer->transform3D().positionY()));
            }
            impl_->dragMode_ = LayerDragMode::Move;
          } else {
            impl_->dragMode_ = hitTestLayerDragMode(hitLayer, event->position(),
                                                    impl_->renderer_.get());
            if (impl_->dragMode_ == LayerDragMode::None) {
              impl_->dragMode_ = LayerDragMode::Move;
            }
          }

          impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
          impl_->dragStartLayerPos_ =
              QPointF(hitLayer->transform3D().positionX(),
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
          impl_->selectionMode_ =
              selectionModeFromModifiers(event->modifiers());
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

void CompositionRenderController::handleMouseMove(const QPointF &viewportPosLogical) {
  // Convert logical pixels (from Qt event) to physical pixels for the rendering pipeline
  const QPointF viewportPos = viewportPosLogical * impl_->devicePixelRatio_;
  auto toolManager = ArtifactApplicationManager::instance()->toolManager();
  auto activeTool =
      toolManager ? toolManager->activeTool() : ToolType::Selection;
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
                if (QVector2D(vertex.position - localPos).length() <
                    hitThreshold) {
                  impl_->hoveredMaskIndex_ = m;
                  impl_->hoveredPathIndex_ = p;
                  impl_->hoveredVertexIndex_ = v;
                  break;
                }
              }
              if (impl_->hoveredVertexIndex_ != -1)
                break;
            }
            if (impl_->hoveredVertexIndex_ != -1)
              break;
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
              layer->setPosition3D(
                  QVector3D(gizmoPos.x(), gizmoPos.y(), currentPos.z()));
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
      impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(),
                               impl_->renderer_->getProjectionMatrix());
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
    auto *selection =
        ArtifactApplicationManager::instance()
            ? ArtifactApplicationManager::instance()->layerSelectionManager()
            : nullptr;
    if (comp && selection && impl_->renderer_) {
      const QRectF rect = impl_->rubberBandCanvasRect().normalized();
      const auto currentFrame = currentFrameForComposition(comp);
      const auto layers = comp->allLayer();
      QVector<ArtifactAbstractLayerPtr> hits;
      hits.reserve(layers.size());
      for (const auto &layer : layers) {
        if (!layerIntersectsCanvasRect(layer, rect, currentFrame)) {
          continue;
        }
        hits.push_back(layer);
      }

      if (impl_->selectionMode_ == SelectionMode::Replace) {
        selection->clearSelection();
      }

      for (const auto &layer : hits) {
        if (!layer) {
          continue;
        }
        const std::shared_ptr<ArtifactAbstractLayer> layerRef = layer;
        if (impl_->selectionMode_ == SelectionMode::Toggle) {
          if (selection->isSelected(layerRef)) {
            selection->removeFromSelection(layerRef);
          } else {
            selection->addToSelection(layerRef);
          }
        } else {
          selection->addToSelection(layerRef);
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

CompositionRenderController::CameraFrustumVisual
CompositionRenderController::cameraFrustumVisual() const {
  return buildCameraFrustumVisual(impl_->previewPipeline_.composition(),
                                  impl_->selectedLayerId_);
}

// ROI Debug
void CompositionRenderController::setDebugMode(bool enabled) {
  impl_->debugMode_ = enabled;
}

bool CompositionRenderController::isDebugMode() const {
  return impl_->debugMode_;
}

Qt::CursorShape CompositionRenderController::cursorShapeForViewportPos(
    const QPointF &viewportPos) const {
  if (!impl_->gizmo_ || !impl_->renderer_) {
    return Qt::ArrowCursor;
  }
  // viewportPos is in logical pixels; convert to physical for gizmo hit testing
  const QPointF physPos = viewportPos * impl_->devicePixelRatio_;
  return impl_->gizmo_->cursorShapeForViewportPos(physPos,
                                                  impl_->renderer_.get());
}

void CompositionRenderController::renderOneFrame() {
  if (!impl_->initialized_ || !impl_->renderer_) {
    qCDebug(compositionViewLog)
        << "[CompositionView] renderOneFrame skipped: not initialized";
    return;
  }
  if (!impl_->running_) {
    qCDebug(compositionViewLog)
        << "[CompositionView] renderOneFrame skipped: controller stopped";
    return;
  }
  if (auto *host = impl_->hostWidget_.data()) {
    if (!host->isVisible()) {
      qCDebug(compositionViewLog)
          << "[CompositionView] renderOneFrame skipped: host hidden";
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

void CompositionRenderController::Impl::renderOneFrameImpl(
    CompositionRenderController *owner) {
  if (!owner || !initialized_ || !renderer_ || !running_) {
    return;
  }

  // 変更検出器のデバッグログ (実装確認用)
  static int renderCount = 0;
  if (renderCount++ % 60 == 0) { // 2秒に1回
    qDebug() << "[CompositionChangeDetector]" << changeDetector_.debugInfo();
  }
  if (auto *host = hostWidget_.data()) {
    if (!host->isVisible()) {
      return;
    }
  }

  // 強制的なサイズ同期:
  // ホストウィジェットの物理サイズとスワップチェーンを一致させる
  if (auto *host = hostWidget_.data()) {
    const float curW = static_cast<float>(host->width()) * devicePixelRatio_;
    const float curH = static_cast<float>(host->height()) * devicePixelRatio_;
    if (std::abs(curW - hostWidth_) > 0.5f ||
        std::abs(curH - hostHeight_) > 0.5f) {
      qDebug() << "[CompositionView] Widget size changed, scheduling swapchain "
                  "update:"
               << curW << "x" << curH;
      // Store logical pixels — setViewportSize applies DPR internally
      pendingResizeSize_ = QSize(host->width(), host->height());
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
  const float cw =
      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
  const float ch =
      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
  const FloatColor bgColor = comp->backgroundColor();
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
          : (gpuBlendEnabled_ ? std::max(previewDownsample_, 2)
                              : previewDownsample_);
  const float rcw = std::max(
      1.0f, viewportW / static_cast<float>(effectivePreviewDownsample));
  const float rch = std::max(
      1.0f, viewportH / static_cast<float>(effectivePreviewDownsample));

  if (compositionRenderer_) {
    compositionRenderer_->SetCompositionSize(cw, ch);
    // Note: ApplyCompositionSpace sets renderer canvas size to FULL size.
    // We override it below if pipeline is enabled.
    compositionRenderer_->ApplyCompositionSpace();
  } else {
    renderer_->setCanvasSize(cw, ch);
  }

  const auto layers = comp->allLayer();
  FramePosition currentFrame = comp->framePosition();
  if (auto *playback = ArtifactPlaybackService::instance()) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || playbackComp->id() == comp->id()) {
      currentFrame = playback->currentFrame();
    }
  }

  // Find active camera layer for 3D rendering
  ArtifactCameraLayer *activeCamera = nullptr;
  for (const auto &l : layers) {
    auto layerCopy = l;
    if (auto cam = dynamic_cast<ArtifactCameraLayer *>(layerCopy.get())) {
      if (cam->isVisible() && cam->isActiveAt(currentFrame)) {
        activeCamera = cam;
        break; // Use first visible camera
      }
    }
  }

  // Compute camera matrices if we have a visible camera
  bool has3DCamera = false;
  QMatrix4x4 cameraViewMatrix;
  QMatrix4x4 cameraProjMatrix;
  if (activeCamera) {
    const QSize compSize = comp->settings().compositionSize();
    const float cw =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
    const float ch =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
    const float aspect = std::max(0.001f, cw / std::max(0.001f, ch));

    cameraViewMatrix = activeCamera->viewMatrix();
    cameraProjMatrix = activeCamera->projectionMatrix(aspect);
    has3DCamera = true;
  }

  int64_t effectiveEndFrame = 0;
  for (const auto &l : layers) {
    if (l) {
      effectiveEndFrame =
          std::max(effectiveEndFrame, l->outPoint().framePosition());
    }
  }
  const int64_t framePos = currentFrame.framePosition();
  const bool frameOutOfRange =
      (framePos < 0 ||
       (effectiveEndFrame > 0 && framePos >= effectiveEndFrame));
  float panX = 0.0f;
  float panY = 0.0f;
  renderer_->getPan(panX, panY);
  const float zoom = renderer_->getZoom();
  const QString backgroundKey =
      QStringLiteral("%1,%2,%3,%4")
          .arg(comp->backgroundColor().r(), 0, 'f', 4)
          .arg(comp->backgroundColor().g(), 0, 'f', 4)
          .arg(comp->backgroundColor().b(), 0, 'f', 4)
          .arg(comp->backgroundColor().a(), 0, 'f', 4);
  if (backgroundKey != lastBackgroundKey_ || lastBackgroundCompositionId_ != comp->id()) {
    lastBackgroundKey_ = backgroundKey;
    lastBackgroundCompositionId_ = comp->id();
    qInfo() << "[CompositionView][Background]"
            << "compositionId=" << comp->id().toString()
            << "background=" << backgroundKey
            << "clearColor="
            << QStringLiteral("%1,%2,%3,%4")
                   .arg(clearColor_.r(), 0, 'f', 4)
                   .arg(clearColor_.g(), 0, 'f', 4)
                   .arg(clearColor_.b(), 0, 'f', 4)
                   .arg(clearColor_.a(), 0, 'f', 4);
  }
  const QByteArray baseRenderKey =
      QByteArray("comp=") + comp->id().toString().toUtf8() +
      "|baseSerial=" + QByteArray::number(baseInvalidationSerial_) +
      "|frame=" + QByteArray::number(framePos) +
      "|size=" + QByteArray::number(static_cast<int>(viewportW)) + "x" +
      QByteArray::number(static_cast<int>(viewportH)) +
      "|downsample=" + QByteArray::number(effectivePreviewDownsample) +
      "|zoom=" + QByteArray::number(zoom, 'f', 4) +
      "|pan=" + QByteArray::number(panX, 'f', 2) + "," +
      QByteArray::number(panY, 'f', 2) + "|clear=" + backgroundKey.toUtf8() +
      "|checker=" + QByteArray::number(showCheckerboard_ ? 1 : 0) +
      "|gpuBlend=" + QByteArray::number(gpuBlendEnabled_ ? 1 : 0);
  const QByteArray renderKey =
      baseRenderKey +
      "|overlaySerial=" + QByteArray::number(overlayInvalidationSerial_) +
      "|selected=" + selectedLayerId_.toString().toUtf8() + "|gizmoMode=" +
      QByteArray::number(gizmo3D_ ? static_cast<int>(gizmo3D_->mode()) : -1) +
      "|gizmoHover=" +
      QByteArray::number(gizmo3D_ ? static_cast<int>(gizmo3D_->hoverAxis())
                                  : -1) +
      "|gizmoActive=" +
      QByteArray::number(gizmo3D_ ? static_cast<int>(gizmo3D_->activeAxis())
                                  : -1) +
      "|flags=" + QByteArray::number(showGrid_ ? 1 : 0) +
      QByteArray::number(showGuides_ ? 1 : 0) +
      QByteArray::number(showSafeMargins_ ? 1 : 0) +
      QByteArray::number(viewportInteracting_ ? 1 : 0);
  renderer_->setClearColor(clearColor_);
  renderer_->clear();

  {

    const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;
    const bool hasGpuBlendJustification =
        std::any_of(layers.begin(), layers.end(),
                    [&](const ArtifactAbstractLayerPtr &layer) {
                      if (!layer || !layer->isVisible() ||
                          !layer->isActiveAt(currentFrame)) {
                        return false;
                      }
                      if (layer->layerBlendType() !=
                          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL) {
                        return true;
                      }
                      if (layer->maskCount() > 0) {
                        return true;
                      }
                      return layerHasCpuRasterizerWork(layer.get());
                    });
    const bool gpuBlendPathRequested =
        gpuBlendRequested && hasGpuBlendJustification;

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

    const bool pipelineEnabled =
        gpuBlendPathRequested && renderPipeline_.ready();
    const int pipelineStateMask = (gpuBlendEnabled_ ? 0x1 : 0x0) |
                                  (renderPipeline_.ready() ? 0x2 : 0x0) |
                                  (blendPipelineReady_ ? 0x4 : 0x0);
    if (pipelineStateMask != lastPipelineStateMask_) {
      lastPipelineStateMask_ = pipelineStateMask;
      if (!pipelineEnabled) {
        qWarning() << "[CompositionView] GPU blend path disabled"
                   << "gpuBlendEnabled=" << gpuBlendEnabled_
                   << "renderPipelineReady=" << renderPipeline_.ready()
                   << "blendPipelineReady=" << blendPipelineReady_ << "size="
                   << QSize(static_cast<int>(cw), static_cast<int>(ch));
      } else {
        qDebug() << "[CompositionView] GPU blend path enabled"
                 << "size="
                 << QSize(static_cast<int>(cw), static_cast<int>(ch));
      }
    }
    if (pipelineEnabled) {
      const QSize pipelineSize(static_cast<int>(renderPipeline_.width()),
                               static_cast<int>(renderPipeline_.height()));
      // Compute shaders now have explicit bounds guards.
      if (((pipelineSize.width() & 7) != 0 ||
           (pipelineSize.height() & 7) != 0) &&
          pipelineSize != lastDispatchWarningSize_) {
        lastDispatchWarningSize_ = pipelineSize;
        qCDebug(compositionViewLog) << "[CompositionView] GPU blend path uses "
                                       "non-8-aligned render size: "
                                    << pipelineSize;
      }
    }

    int drawnLayerCount = 0;
    int surfaceUploadLayerCount = 0;
    int cpuRasterLayerCount = 0;
    const float targetViewportW = hostWidth_;
    const float targetViewportH = hostHeight_;
    const float legacyDownsampleViewportW =
        hostWidth_ > 0.0f
            ? hostWidth_ / static_cast<float>(effectivePreviewDownsample)
            : 0.0f;
    const float legacyDownsampleViewportH =
        hostHeight_ > 0.0f
            ? hostHeight_ / static_cast<float>(effectivePreviewDownsample)
            : 0.0f;
    qint64 setupMs = markPhaseMs();
    qint64 basePassMs = 0;
    qint64 layerPassMs = 0;
    qint64 overlayMs = 0;
    qint64 flushMs = 0;
    qint64 presentMs = 0;

    // hasSoloLayer: solo レイヤーの存在確認
    const bool hasSoloLayer = std::any_of(
        layers.begin(), layers.end(), [](const ArtifactAbstractLayerPtr &l) {
          return l && l->isVisible() && l->isSolo();
        });
    const QStringList selectedIds = selectedLayerIdList();
    const bool hasSelection = !selectedIds.isEmpty();
    constexpr float kGhostOpacityScale = 0.22f;

    // ============================================================
    // GPU パイプライン: レイヤー 0 枚でも frameOutOfRange でも常に描画
    // ============================================================
    if (pipelineEnabled) {
      auto ctx = renderer_->immediateContext();
      auto accumRTV = renderPipeline_.accumRTV();
      auto accumSRV = renderPipeline_.accumSRV();
      auto tempUAV = renderPipeline_.tempUAV();
      auto layerRTV = renderPipeline_.layerRTV();
      auto layerSRV = renderPipeline_.layerSRV();

      // ==== オフスクリーン描画前の状態保存 ====
      const float origZoom = renderer_->getZoom();
      const FloatColor origClearColor = renderer_->getClearColor();
      float origPanX, origPanY;
      renderer_->getPan(origPanX, origPanY);
      const float origViewW = hostWidth_;
      const float origViewH = hostHeight_;
      const float offscreenScale =
          (origViewW > 0.0f) ? (rcw / origViewW) : 1.0f;

      // -- 1: 背景を accum に直接描画（オフスクリーン座標系）--
      // 背景は Composition Space で現在の Pan/Zoom を適用して描画する。
      renderer_->setViewportSize(rcw, rch);
      renderer_->setCanvasSize(cw, ch);  // ← Composition Space に設定
      renderer_->setZoom(origZoom);      // ← 現在のカメラズームを適用
      renderer_->setPan(origPanX, origPanY); // ← 現在のカメラパンを適用
      {
        Diligent::Viewport offVP;
        offVP.TopLeftX = 0.0f;
        offVP.TopLeftY = 0.0f;
        offVP.Width = rcw;
        offVP.Height = rch;
        offVP.MinDepth = 0.0f;
        offVP.MaxDepth = 1.0f;
        ctx->SetViewports(1, &offVP, static_cast<Diligent::Uint32>(rcw),
                          static_cast<Diligent::Uint32>(rch));
      }

      const FloatColor bgColor = comp->backgroundColor();
      if (compositionViewLog().isDebugEnabled()) {
        qCDebug(compositionViewLog)
            << "[CompositionView] background pass (gpu)"
            << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))
            << "rtSize=" << QSize(static_cast<int>(rcw), static_cast<int>(rch))
            << "viewport=" << QSize(static_cast<int>(origViewW),
                                    static_cast<int>(origViewH))
            << "zoom=" << origZoom << "pan=" << QPointF(origPanX, origPanY)
            << "bg="
            << QColor::fromRgbF(bgColor.r(), bgColor.g(), bgColor.b(),
                                bgColor.a())
            << "checker=" << showCheckerboard_
            << "compositionSpaceApplied=" << true;
      }
      renderer_->setOverrideRTV(accumRTV);
      // accum を透明でクリア（bgColor はレイヤーブレンドに影響しないよう後で Composition Space で描画）
      renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
      renderer_->clear();
      renderer_->setClearColor(clearColor_);
      renderer_->setOverrideRTV(nullptr);
      basePassMs = markPhaseMs();

      // レイヤー描画用に、ダウンサンプル后的なオフスクリーン座標系に切り替え
      renderer_->setCanvasSize(rcw, rch);  // ← レイヤー描画用に戻す
      renderer_->setZoom(origZoom * offscreenScale);
      renderer_->setPan(origPanX * offscreenScale, origPanY * offscreenScale);

      // -- 2: レイヤーブレンド（frameOutOfRange ならスキップ）--
      bool blendPerformed = false;  // accumがUNORDERED_ACCESS状態になったか追跡
      if (!frameOutOfRange) {
        const DetailLevel lod = detailLevelFromZoom(renderer_->getZoom());
        for (const auto &layer : layers) {
          if (!layer || !layer->isVisible())
            continue;
          if (hasSoloLayer && !layer->isSolo())
            continue;
          if (!layer->isActiveAt(currentFrame))
            continue;

          ++drawnLayerCount;
          if (layerUsesSurfaceUploadForCompositionView(layer.get())) {
            ++surfaceUploadLayerCount;
          }
          if (layerHasCpuRasterizerWork(layer.get())) {
            ++cpuRasterLayerCount;
          }

          layer->goToFrame(currentFrame.framePosition());
          const auto blendMode =
              ArtifactCore::toBlendMode(layer->layerBlendType());
          const float opacity =
              layer->opacity() *
              ((hasSelection && !isLayerSelected(selectedIds, layer))
                   ? kGhostOpacityScale
                   : 1.0f);
          if (opacity <= 0.0f)
            continue;

          renderer_->setOverrideRTV(layerRTV);
          renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
          renderer_->clear();
          QString *dbgOut =
              QLoggingCategory::defaultCategory()->isDebugEnabled()
                  ? &lastVideoDebug_
                  : nullptr;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), 1.0f, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(), true,
              lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr);
          renderer_->setOverrideRTV(nullptr);

          // CS 実行前に RTV を解除
          ctx->SetRenderTargets(0, nullptr, nullptr,
                                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

          const bool blendOk = blendPipeline_->blend(
              ctx, layerSRV, accumSRV, tempUAV, blendMode, opacity);
          if (!blendOk) {
            continue;
          }
          renderPipeline_.swapAccumAndTemp();
          blendPerformed = true;  // accumはUNORDERED_ACCESS状態
          accumSRV = renderPipeline_.accumSRV();
          tempUAV = renderPipeline_.tempUAV();
        }
      }

      // ==== オフスクリーン描画後: ホスト viewport に戻す ====
      renderer_->setViewportSize(origViewW, origViewH);
      {
        Diligent::Viewport hostVP;
        hostVP.TopLeftX = 0.0f;
        hostVP.TopLeftY = 0.0f;
        hostVP.Width = origViewW;
        hostVP.Height = origViewH;
        hostVP.MinDepth = 0.0f;
        hostVP.MaxDepth = 1.0f;
        ctx->SetViewports(1, &hostVP, static_cast<Diligent::Uint32>(origViewW),
                          static_cast<Diligent::Uint32>(origViewH));
      }

      // -- 3: bgColor 背景矩形を描画（D3D12 viewport 確定後、accum blit より前） --
      // SetViewports 済みの状態で swap chain 上に描画する。
      // accum の SRC_ALPHA blit は透明ピクセル (α=0) の領域でこの bgColor を透過させる。
      // レイヤーブレンドは accum 内で完結するため bgColor は一切影響しない。
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);
      renderer_->drawSolidRect(0.0f, 0.0f, cw, ch, bgColor, 1.0f);

      // -- 4: オフスクリーン RT を画面全体に描画（スクリーン座標、SRC_ALPHA ブレンド） --
      // drawSpriteTransformed (opacity 付き) を使うことで SRC_ALPHA ブレンドが適用され、
      // accum の透明ピクセル（レイヤーなし領域）から手前の bgColor 矩形が透けて見える。
      renderer_->setCanvasSize(origViewW, origViewH);
      renderer_->setZoom(1.0f);
      renderer_->setPan(0.0f, 0.0f);
      // Bug B fix: ブレンドCSが UNORDERED_ACCESS に書いた accum テクスチャを
      // SRV として読む前に SHADER_RESOURCE へ状態遷移する。
      // ブレンドが一度も実行されなかった場合、accum は RENDER_TARGET 状態のままなので
      // この明示的バリアはスキップし、drawSpriteTransformed 内の auto-TRANSITION に任せる。
      if (blendPerformed) {
        if (auto *accumTex = renderPipeline_.accumSRV()
                                 ? renderPipeline_.accumSRV()->GetTexture()
                                 : nullptr) {
          Diligent::StateTransitionDesc accumBarrier;
          accumBarrier.pResource           = accumTex;
          accumBarrier.OldState            = RESOURCE_STATE_UNORDERED_ACCESS;
          accumBarrier.NewState            = RESOURCE_STATE_SHADER_RESOURCE;
          ctx->TransitionResourceStates(1, &accumBarrier);
        }
      }
      {
        QMatrix4x4 screenIdentity;
        screenIdentity.setToIdentity();
        renderer_->drawSpriteTransformed(0.0f, 0.0f, origViewW, origViewH,
                                         screenIdentity,
                                         renderPipeline_.accumSRV(), 1.0f);
      }

      // コンポジションのキャンバス座標系に戻す
      if (compositionRenderer_) {
        compositionRenderer_->SetCompositionSize(cw, ch);
        compositionRenderer_->ApplyCompositionSpace();
      } else {
        renderer_->setCanvasSize(cw, ch);
      }
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);
      renderer_->setClearColor(origClearColor); // Bug A fix: GPUパス前に保存したクリアカラーを復元
      layerPassMs = markPhaseMs();
    } else {
      // === Fallback path (GPU パイプラインなし) ===
      renderer_->setViewportSize(viewportW, viewportH);
      if (auto ctx = renderer_->immediateContext()) {
        Diligent::Viewport hostVP;
        hostVP.TopLeftX = 0.0f;
        hostVP.TopLeftY = 0.0f;
        hostVP.Width = viewportW;
        hostVP.Height = viewportH;
        hostVP.MinDepth = 0.0f;
        hostVP.MaxDepth = 1.0f;
        ctx->SetViewports(1, &hostVP,
                          static_cast<Diligent::Uint32>(viewportW),
                          static_cast<Diligent::Uint32>(viewportH));
      }
      renderer_->setCanvasSize(cw, ch);  // キャンバスを Composition Space に設定
      renderer_->drawSolidRect(0.0f, 0.0f, cw, ch, bgColor, 1.0f);
      if (showGrid_) {
        renderer_->drawGrid(0, 0, cw, ch, 100.0f, 1.0f,
                            {0.3f, 0.3f, 0.3f, 0.5f});
      }

      if (compositionViewLog().isDebugEnabled()) {
        const auto tl = renderer_->canvasToViewport({0.0f, 0.0f});
        const auto br = renderer_->canvasToViewport({cw, ch});
        const float rectX = std::min(tl.x, br.x);
        const float rectY = std::min(tl.y, br.y);
        const float rectW = std::fabs(br.x - tl.x);
        const float rectH = std::fabs(br.y - tl.y);

        qCDebug(compositionViewLog)
            << "[CompositionView] fallback comp rect in viewport"
            << QRectF(rectX, rectY, rectW, rectH);

        const float prevZoom = renderer_->getZoom();
        float prevPanX = 0.0f;
        float prevPanY = 0.0f;
        renderer_->getPan(prevPanX, prevPanY);

        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);

        renderer_->drawRectLocal(12.0f, 12.0f, 72.0f, 72.0f,
                                 {1.0f, 0.0f, 1.0f, 0.9f}, 1.0f);
        renderer_->drawRectOutline(rectX, rectY, rectW, rectH,
                                   {1.0f, 0.2f, 0.2f, 1.0f});

        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      basePassMs = markPhaseMs();

      if (!frameOutOfRange) {
        // ROI 計算は「コンポジション枠」ではなく「実際に見えている canvas 範囲」で行う。
        // これを comp size で切ると、画面内に残っているのにオブジェクトや gizmo が消える。
        const QRectF viewportRect =
            viewportRectToCanvasRect(renderer_.get(), QPointF(0.0f, 0.0f),
                                     QPointF(viewportW, viewportH));
        const float roiPad = std::max(48.0f, 64.0f / std::max(0.001f, renderer_->getZoom()));
        const QRectF roiRect = viewportRect.adjusted(-roiPad, -roiPad, roiPad, roiPad);
        const DetailLevel lod = detailLevelFromZoom(renderer_->getZoom());

        for (const auto &layer : layers) {
          if (!layer || !layer->isVisible())
            continue;
          if (hasSoloLayer && !layer->isSolo())
            continue;
          if (!layer->isActiveAt(currentFrame))
            continue;

          // === 段階 2: ROI 計算 ===
          const QRectF layerBounds = layer->transformedBoundingBox();
          const QRectF intersected = layerBounds.intersected(roiRect);

          // === 段階 3: 空 ROI スキップ ===
          if (intersected.isEmpty()) {
            continue; // 画面外レイヤーをスキップ
          }

          ++drawnLayerCount;
          if (layerUsesSurfaceUploadForCompositionView(layer.get())) {
            ++surfaceUploadLayerCount;
          }
          if (layerHasCpuRasterizerWork(layer.get())) {
            ++cpuRasterLayerCount;
          }
          layer->goToFrame(currentFrame.framePosition());
          const float opacity =
              layer->opacity() *
              ((hasSelection && !isLayerSelected(selectedIds, layer))
                   ? kGhostOpacityScale
                   : 1.0f);
          QString *dbgOut =
              QLoggingCategory::defaultCategory()->isDebugEnabled()
                  ? &lastVideoDebug_
                  : nullptr;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), opacity, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(),
              false, lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr);

          // === 段階 7: ROI デバッグ表示 ===
          if (debugMode_) {
            // ROI を赤い枠で表示
            renderer_->drawRectOutline(
                intersected.x(), intersected.y(), intersected.width(),
                intersected.height(), FloatColor{1.0f, 0.0f, 0.0f, 1.0f});
          }
        }
      }
      layerPassMs = markPhaseMs();
    }

    if (renderer_ && showMotionPathOverlay_ && comp &&
        !selectedLayerId_.isNil()) {
      if (auto selectedLayer = comp->layerById(selectedLayerId_)) {
        const auto motionPath = buildMotionPathSamples(selectedLayer, comp);
        QVector<MotionPathSample> keyframes;
        keyframes.reserve(motionPath.size());
        const MotionPathSample *currentSample = nullptr;
        for (const auto &sample : motionPath) {
          if (sample.kind == MotionPathSampleKind::Current) {
            currentSample = &sample;
          } else {
            keyframes.push_back(sample);
          }
        }

        auto samePoint = [](const QPointF &a, const QPointF &b) {
          return qFuzzyCompare(a.x(), b.x()) && qFuzzyCompare(a.y(), b.y());
        };

        const bool currentMatchesKeyframe =
            currentSample &&
            std::any_of(keyframes.begin(), keyframes.end(),
                        [&](const MotionPathSample &sample) {
                          return samePoint(sample.position,
                                           currentSample->position);
                        });

        const bool hasMotion = keyframes.size() >= 2 ||
                               (currentSample != nullptr &&
                                !keyframes.empty() && !currentMatchesKeyframe);

        if (hasMotion) {
          const FloatColor pathColor{0.95f, 0.65f, 0.22f, 0.85f};
          const FloatColor keyColor{1.0f, 0.92f, 0.28f, 1.0f};
          const FloatColor currentColor{0.28f, 0.9f, 1.0f, 1.0f};
          QPointF prev = keyframes[0].position;
          for (int i = 1; i < keyframes.size(); ++i) {
            const QPointF cur = keyframes[i].position;
            renderer_->drawSolidLine(
                {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
                {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
                pathColor, 1.2f);
            prev = cur;
          }
          for (const auto &sample : keyframes) {
            renderer_->drawPoint(static_cast<float>(sample.position.x()),
                                 static_cast<float>(sample.position.y()), 6.0f,
                                 keyColor);
          }
          if (currentSample && !currentMatchesKeyframe) {
            renderer_->drawPoint(
                static_cast<float>(currentSample->position.x()),
                static_cast<float>(currentSample->position.y()), 4.0f,
                currentColor);
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
          const float viewportW =
              hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
          const float viewportH =
              hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
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
            gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(),
                           renderer_->getProjectionMatrix());
          }
        }

        // Mask Overlay Drawing
        const int maskCount = selectedLayer->maskCount();
        if (maskCount > 0 && renderer_ &&
            selectedLayer->isActiveAt(currentFrame)) {
          const QTransform globalTransform =
              selectedLayer->getGlobalTransform();
          const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
          const FloatColor maskLineColor = {0.26f, 0.84f, 0.96f, 0.96f};
          const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
          const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
          const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
          const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

          for (int m = 0; m < maskCount; ++m) {
            LayerMask mask = selectedLayer->mask(m);
            if (!mask.isEnabled())
              continue;

            for (int p = 0; p < mask.maskPathCount(); ++p) {
              MaskPath path = mask.maskPath(p);
              const int vertexCount = path.vertexCount();
              if (vertexCount == 0)
                continue;

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
                Detail::float2 currentCanvasPos = {(float)canvasPos.x(),
                                                   (float)canvasPos.y()};

                if (v > 0) {
                  renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                                6.0f, maskLineShadowColor);
                  renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                                3.5f, maskLineColor);
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

                markers.push_back(
                    {currentCanvasPos, currentColor, currentPointRadius});
                lastCanvasPos = currentCanvasPos;
              }

              if (path.isClosed() && vertexCount > 1) {
                MaskVertex firstVertex = path.vertex(0);
                QPointF firstCanvasPos =
                    globalTransform.map(firstVertex.position);
                renderer_->drawThickLineLocal(
                    lastCanvasPos,
                    {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                    7.0f, maskLineShadowColor);
                renderer_->drawThickLineLocal(
                    lastCanvasPos,
                    {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                    4.0f, maskLineColor);
              }

              for (const auto &marker : markers) {
                renderer_->drawPoint(marker.pos.x, marker.pos.y,
                                     marker.radius + 3.0f,
                                     maskPointShadowColor);
                renderer_->drawPoint(marker.pos.x, marker.pos.y, marker.radius,
                                     marker.color);
              }
            }
          }
        }
      } else {
        gizmo_->setLayer(nullptr);
      }
    }

    // M-CE Phase 2: ピクセルグリッド自動表示 (ズーム 800% 以上でフェードイン)
    if (renderer_ && renderer_->getZoom() >= 8.0f) {
      const float zoom = renderer_->getZoom();
      const float alpha = std::clamp((zoom - 8.0f) / 8.0f, 0.0f, 0.5f) * 0.4f;
      if (alpha > 0.0f && comp) {
        const auto compSize = comp->settings().compositionSize();
        const float compW =
            static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
        const float compH = static_cast<float>(
            compSize.height() > 0 ? compSize.height() : 1080);
        renderer_->drawGrid(0.0f, 0.0f, compW, compH, 1.0f, 1.0f / zoom,
                            {0.6f, 0.6f, 0.6f, alpha});
      }
    }

    if (renderer_ && !selectedIds.isEmpty()) {
      const auto layersForOverlay = comp
                                        ? comp->allLayer()
                                        : decltype(comp->allLayer()){};

      // M-UI-6 Composition Motion Path Overlay
      if (comp) {
        const float zoom = renderer_->getZoom();
        const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
        for (const auto &layer : layersForOverlay) {
          if (!layer || !isLayerSelected(selectedIds, layer))
            continue;
          const std::shared_ptr<ArtifactAbstractLayer> &layerRef = layer;

          const auto &t3d = layer->transform3D();
          auto posTimes = t3d.getPositionKeyFrameTimes();
          if (posTimes.size() < 2)
            continue;

          int minFrame = static_cast<int>(posTimes.front().value());
          int maxFrame = static_cast<int>(posTimes.back().value());
          const int currentFrameNum = currentFrame.framePosition();

          // Limit drawing range for performance (render ±300 frames around
          // playhead)
          minFrame = std::max(minFrame, currentFrameNum - 300);
          maxFrame = std::min(maxFrame, currentFrameNum + 300);

          if (minFrame >= maxFrame)
            continue;

          // AE differentiation style: Distinctive color for path
          const FloatColor pathColor{0.9f, 0.4f, 0.8f, 0.9f};
          const float lineThickness = std::max(1.0f, 1.5f * invZoom);
          const float dotRadius = std::max(1.5f, 2.5f * invZoom);
          const int64_t rate = posTimes.front().scale();

          Detail::float2 lastPos;
          bool hasLastPos = false;

          for (int f = minFrame; f <= maxFrame; f += 2) {
            if (f > maxFrame)
              f = maxFrame;
            ArtifactCore::RationalTime t(f, rate);
            QTransform gTrans = layer->getGlobalTransformAt(f);
            float ax = t3d.anchorXAt(t);
            float ay = t3d.anchorYAt(t);
            QPointF wPos = gTrans.map(QPointF(ax, ay));

            Detail::float2 currentPos((float)wPos.x(), (float)wPos.y());

            if (hasLastPos) {
              renderer_->drawSolidLine(lastPos, currentPos, pathColor,
                                       lineThickness);
            }
            renderer_->drawPoint(currentPos.x, currentPos.y, dotRadius * 0.6f,
                                 {0.8f, 0.8f, 0.8f, 0.7f});

            lastPos = currentPos;
            hasLastPos = true;
          }

          // Draw actual keyframes as emphasized boxes
          for (const auto &kfTime : posTimes) {
            int f = static_cast<int>(kfTime.value());
            if (f < minFrame || f > maxFrame)
              continue;
            QTransform gTrans = layer->getGlobalTransformAt(f);
            float ax = t3d.anchorXAt(kfTime);
            float ay = t3d.anchorYAt(kfTime);
            QPointF wPos = gTrans.map(QPointF(ax, ay));

            float px = (float)wPos.x();
            float py = (float)wPos.y();

            renderer_->drawSolidRect(
                px - dotRadius, py - dotRadius, dotRadius * 2, dotRadius * 2,
                (f == currentFrameNum) ? FloatColor{1.0f, 1.0f, 0.0f, 1.0f}
                                       : FloatColor{1.0f, 1.0f, 1.0f, 1.0f});
            renderer_->drawRectOutline(px - dotRadius, py - dotRadius,
                                       dotRadius * 2, dotRadius * 2,
                                       {0.0f, 0.0f, 0.0f, 1.0f});
          }
        }
      }
      const FloatColor primaryColor{1.0f, 0.72f, 0.22f, 1.0f};
      const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};
      for (const auto &layer : layersForOverlay) {
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

        const bool primary =
            !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;
        renderer_->drawRectOutline(
            static_cast<float>(bbox.left()), static_cast<float>(bbox.top()),
            static_cast<float>(bbox.width()), static_cast<float>(bbox.height()),
            primary ? primaryColor : secondaryColor);
      }
    }

    if (renderer_ && isRubberBandSelecting_) {
      const QRectF rubberBandRect = rubberBandCanvasRect().normalized();
      if (rubberBandRect.isValid() && rubberBandRect.width() > 0.0f &&
          rubberBandRect.height() > 0.0f) {
        renderer_->drawSolidRect(static_cast<float>(rubberBandRect.left()),
                                 static_cast<float>(rubberBandRect.top()),
                                 static_cast<float>(rubberBandRect.width()),
                                 static_cast<float>(rubberBandRect.height()),
                                 {0.25f, 0.55f, 1.0f, 0.14f}, 1.0f);
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
      renderer_->drawSolidRect(infoX, infoY, infoW, infoH,
                               {0.0f, 0.0f, 0.0f, 0.6f}, 0.8f);
      const int frame = currentFrame.framePosition();
      const float barRatio =
          (frame > 0) ? std::min(1.0f, static_cast<float>(frame) / 1000.0f)
                      : 0.0f;
      const float barW = infoW * barRatio;
      if (barW > 1.0f) {
        renderer_->drawSolidRect(infoX, infoY, barW, infoH,
                                 {0.2f, 0.6f, 1.0f, 0.5f}, 0.6f);
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
                               {cw * 0.5f + crossSize, ch * 0.5f}, marginColor,
                               1.0f);
      renderer_->drawSolidLine({cw * 0.5f, ch * 0.5f - crossSize},
                               {cw * 0.5f, ch * 0.5f + crossSize}, marginColor,
                               1.0f);
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
        renderer_->drawSolidLine({cw * 0.5f, 0}, {cw * 0.5f, ch}, guideColor,
                                 1.0f);
        renderer_->drawSolidLine({0, ch * 0.5f}, {cw, ch * 0.5f}, guideColor,
                                 1.0f);
      }
    }
    const ArtifactAbstractLayerPtr selectedLayer =
        (!selectedLayerId_.isNil() && comp) ? comp->layerById(selectedLayerId_)
                                            : ArtifactAbstractLayerPtr{};
    drawCompositionRegionOverlay(renderer_.get(), comp, showCheckerboard_);
    drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);
    overlayMs = markPhaseMs();

    flushMs = 0;

    if (!lastVideoDebug_.isEmpty() &&
        lastVideoDebug_ != lastEmittedVideoDebug_) {
      lastEmittedVideoDebug_ = lastVideoDebug_;
      Q_EMIT owner->videoDebugMessage(lastVideoDebug_);
    }

    // renderer_->flushAndWait(); // 毎フレーム同期を削除し、性能改善を試す
    renderer_->present();
    presentMs = markPhaseMs();
    lastFinalPresentKey_ = renderKey;

    ++renderFrameCounter_;
    const qint64 frameMs = frameTimer.elapsed();
    if (frameMs >= 16) {
      qInfo() << "[CompositionView][Perf]"
              << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
              << "layersTotal=" << layers.size()
              << "layersDrawn=" << drawnLayerCount
              << "surfaceUploadLayers=" << surfaceUploadLayerCount
              << "cpuRasterLayers=" << cpuRasterLayerCount
              << "frameOutOfRange=" << frameOutOfRange
              << "previewDownsample=" << previewDownsample_
              << "effectivePreviewDownsample=" << effectivePreviewDownsample
              << "viewportInteracting=" << viewportInteracting_ << "compSize="
              << QSize(static_cast<int>(cw), static_cast<int>(ch))
              << "pipelineSize="
              << QSize(static_cast<int>(renderPipeline_.width()),
                       static_cast<int>(renderPipeline_.height()))
              << "hostSize="
              << QSize(static_cast<int>(hostWidth_),
                       static_cast<int>(hostHeight_))
              << "setupMs=" << setupMs << "basePassMs=" << basePassMs
              << "layerPassMs=" << layerPassMs << "overlayMs=" << overlayMs
              << "flushMs=" << flushMs << "presentMs=" << presentMs;
    } else if (compositionViewLog().isDebugEnabled() &&
               (renderFrameCounter_ % 120u) == 0u) {
      qCDebug(compositionViewLog)
          << "[CompositionView][Perf]"
          << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
          << "layersTotal=" << layers.size()
          << "layersDrawn=" << drawnLayerCount
          << "surfaceUploadLayers=" << surfaceUploadLayerCount
          << "cpuRasterLayers=" << cpuRasterLayerCount
          << "previewDownsample=" << previewDownsample_
          << "effectivePreviewDownsample=" << effectivePreviewDownsample
          << "viewportInteracting=" << viewportInteracting_ << "pipelineSize="
          << QSize(static_cast<int>(renderPipeline_.width()),
                   static_cast<int>(renderPipeline_.height()))
          << "setupMs=" << setupMs << "basePassMs=" << basePassMs
          << "layerPassMs=" << layerPassMs << "overlayMs=" << overlayMs
          << "flushMs=" << flushMs << "presentMs=" << presentMs;
    }
  }
} // end renderOneFrameImpl

void CompositionRenderController::Impl::drawViewportGhostOverlay(
    CompositionRenderController *owner, const ArtifactCompositionPtr &comp,
    const ArtifactAbstractLayerPtr &selectedLayer,
    const FramePosition &currentFrame) {
  Q_UNUSED(owner);
  Q_UNUSED(currentFrame);
  if (!renderer_) {
    return;
  }

  const bool scaleActive = gizmo_ && gizmo_->isDragging() && selectedLayer &&
                           selectedLayer->isVisible() &&
                           isScaleHandle(gizmo_->activeHandle());
  const bool dropActive = dropGhostVisible_ && !dropGhostRect_.isNull();
  if (!scaleActive && !dropActive) {
    return;
  }

  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const int overlayW = std::max(1, static_cast<int>(std::ceil(overlayWf)));
  const int overlayH = std::max(1, static_cast<int>(std::ceil(overlayHf)));
  QImage overlayImage(overlayW, overlayH, QImage::Format_ARGB32_Premultiplied);
  overlayImage.fill(Qt::transparent);

  QPainter p(&overlayImage);
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  p.setFont(font);

  auto drawLabelBox = [&](const QRectF &boxRect, const QColor &fill,
                          const QColor &border, const QString &title,
                          const QString &subtitle) {
    if (!boxRect.isValid()) {
      return;
    }
    const QRectF outer = boxRect.normalized();
    const QRectF inner = outer.adjusted(6.0, 6.0, -6.0, -6.0);
    p.setPen(QPen(border, 2.0, Qt::DashLine));
    p.setBrush(fill);
    p.drawRoundedRect(outer, 8.0, 8.0);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 18));
    p.drawRoundedRect(inner, 6.0, 6.0);

    const QFontMetrics fm(p.font());
    const int innerWidth = std::max(10, static_cast<int>(inner.width()) - 20);
    const QRect titleRect(static_cast<int>(inner.left()) + 10,
                          static_cast<int>(inner.top()) + 8, innerWidth,
                          fm.height() + 2);
    const QRect hintRect(static_cast<int>(inner.left()) + 10,
                         static_cast<int>(inner.top()) + 8 + fm.height() + 4,
                         innerWidth, fm.height() + 2);
    p.setPen(QColor(235, 245, 255));
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(title, Qt::ElideRight, titleRect.width()));
    p.setPen(QColor(180, 195, 210));
    p.drawText(hintRect, Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(subtitle, Qt::ElideRight, hintRect.width()));
  };

  if (dropActive) {
    p.fillRect(QRectF(0.0, 0.0, overlayWf, overlayHf),
               QColor(60, 120, 240, 30));
    p.setPen(QPen(QColor(100, 160, 255, 180), 2.0, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(4.0, 4.0, overlayWf - 8.0, overlayHf - 8.0));

    const QRectF ghostRect = dropGhostRect_.normalized();
    const QString ghostTitle = dropGhostTitle_.isEmpty()
                                   ? QStringLiteral("Drop to add layer")
                                   : dropGhostTitle_;
    const QString ghostHint = dropGhostHint_.isEmpty()
                                  ? QStringLiteral("Release to place")
                                  : dropGhostHint_;
    drawLabelBox(ghostRect, QColor(30, 40, 60, 165), QColor(220, 235, 255, 220),
                 ghostTitle, ghostHint);

    if (!dropCandidateLabel_.isEmpty()) {
      const QFontMetrics fm(p.font());
      const int labelW = std::min(
          overlayW - 24,
          std::max(180, fm.horizontalAdvance(dropCandidateLabel_) + 24));
      const int labelH = fm.height() + 12;
      const QRect labelRect(std::max(12, overlayW / 2 - labelW / 2),
                            std::max(8, overlayH / 2 - labelH / 2), labelW,
                            labelH);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(20, 30, 60, 200));
      p.drawRoundedRect(labelRect, 6, 6);
      p.setPen(QColor(200, 220, 255));
      p.drawText(labelRect, Qt::AlignCenter,
                 fm.elidedText(dropCandidateLabel_, Qt::ElideMiddle,
                               labelRect.width() - 16));
    }
  }

  if (scaleActive) {
    const QRectF bbox = selectedLayer->transformedBoundingBox();
    if (bbox.isValid() && renderer_) {
      const auto tl = renderer_->canvasToViewport(
          {static_cast<float>(bbox.left()), static_cast<float>(bbox.top())});
      const auto tr = renderer_->canvasToViewport(
          {static_cast<float>(bbox.right()), static_cast<float>(bbox.top())});
      const auto bl = renderer_->canvasToViewport(
          {static_cast<float>(bbox.left()), static_cast<float>(bbox.bottom())});
      const auto br =
          renderer_->canvasToViewport({static_cast<float>(bbox.right()),
                                       static_cast<float>(bbox.bottom())});
      const QRectF viewRect(QPointF(std::min({tl.x, tr.x, bl.x, br.x}),
                                    std::min({tl.y, tr.y, bl.y, br.y})),
                            QPointF(std::max({tl.x, tr.x, bl.x, br.x}),
                                    std::max({tl.y, tr.y, bl.y, br.y})));

      p.setPen(QPen(QColor(0, 0, 0, 120), 6.0, Qt::DashLine));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(viewRect.adjusted(-6.0, -6.0, 6.0, 6.0), 4.0, 4.0);
      p.setPen(QPen(QColor(255, 200, 72, 220), 2.0, Qt::DashLine));
      p.drawRoundedRect(viewRect.adjusted(-4.0, -4.0, 4.0, 4.0), 4.0, 4.0);

      const auto &t3 = selectedLayer->transform3D();
      const QString text =
          QStringLiteral("Scale  %1%%  x  %2%%")
              .arg(QString::number(t3.scaleX() * 100.0f, 'f', 0))
              .arg(QString::number(t3.scaleY() * 100.0f, 'f', 0));
      const QFontMetrics fm(p.font());
      const QSize textSize = fm.size(Qt::TextSingleLine, text);
      QRect labelRect(static_cast<int>(viewRect.right()) + 12,
                      static_cast<int>(viewRect.top()) - textSize.height() - 14,
                      textSize.width() + 22, textSize.height() + 12);
      if (labelRect.right() > overlayW - 8) {
        labelRect.moveRight(overlayW - 8);
      }
      if (labelRect.left() < 8) {
        labelRect.moveLeft(8);
      }
      if (labelRect.top() < 8) {
        labelRect.moveTop(8);
      }
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(12, 14, 17, 220));
      p.drawRoundedRect(labelRect, 6, 6);
      p.setPen(QColor(230, 235, 240));
      p.drawText(labelRect.adjusted(10, 6, -10, -6),
                 Qt::AlignLeft | Qt::AlignVCenter,
                 fm.elidedText(text, Qt::ElideRight, labelRect.width() - 20));
    }
  }

  if (infoOverlayVisible_ &&
      (!infoOverlayTitle_.trimmed().isEmpty() ||
       !infoOverlayDetail_.trimmed().isEmpty())) {
    const QString title = infoOverlayTitle_.trimmed().isEmpty()
                              ? QStringLiteral("Info")
                              : infoOverlayTitle_.trimmed();
    const QString detail = infoOverlayDetail_.trimmed();
    const QFontMetrics fm(p.font());
    const int lineHeight = fm.height();
    const int contentWidth = std::max(
        fm.horizontalAdvance(title),
        detail.isEmpty() ? 0 : fm.horizontalAdvance(detail));
    const int contentHeight = detail.isEmpty() ? lineHeight : lineHeight * 2 + 4;
    QRect labelRect(12, 12, contentWidth + 24, contentHeight + 12);
    if (labelRect.right() > overlayW - 8) {
      labelRect.moveRight(overlayW - 8);
    }
    if (labelRect.bottom() > overlayH - 8) {
      labelRect.moveBottom(overlayH - 8);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 10, 14, 210));
    p.drawRoundedRect(labelRect, 7, 7);
    p.setPen(QColor(232, 238, 244));
    p.drawText(labelRect.adjusted(10, 6, -10, -6),
               Qt::AlignLeft | Qt::AlignTop, title);
    if (!detail.isEmpty()) {
      p.setPen(QColor(178, 190, 204));
      const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
      p.drawText(detailRect, Qt::AlignLeft | Qt::AlignTop,
                 fm.elidedText(detail, Qt::ElideRight, detailRect.width()));
    }
  }

  const bool snapHintActive = gizmo_ && gizmo_->isDragging() && selectedLayer;
  if (snapHintActive) {
    const bool snapBypassed =
        QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    const QString snapTitle =
        snapBypassed ? QStringLiteral("Snap Off") : QStringLiteral("Snap On");
    const QString snapDetail =
        snapBypassed ? QStringLiteral("Hold Alt to enable free move")
                     : QStringLiteral("Hold Alt to bypass snapping");
    const QFontMetrics fm(p.font());
    const int lineHeight = fm.height();
    const int contentWidth = std::max(
        fm.horizontalAdvance(snapTitle), fm.horizontalAdvance(snapDetail));
    QRect labelRect(12, overlayH - (lineHeight * 2 + 28),
                    contentWidth + 24, lineHeight * 2 + 12);
    if (labelRect.bottom() > overlayH - 8) {
      labelRect.moveBottom(overlayH - 8);
    }
    if (labelRect.left() < 8) {
      labelRect.moveLeft(8);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 10, 14, 210));
    p.drawRoundedRect(labelRect, 7, 7);
    p.setPen(QColor(232, 238, 244));
    p.drawText(labelRect.adjusted(10, 6, -10, -6),
               Qt::AlignLeft | Qt::AlignTop, snapTitle);
    p.setPen(QColor(178, 190, 204));
    const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
    p.drawText(detailRect, Qt::AlignLeft | Qt::AlignTop,
               fm.elidedText(snapDetail, Qt::ElideRight, detailRect.width()));
  }

  p.end();

  const float drawW = static_cast<float>(overlayImage.width());
  const float drawH = static_cast<float>(overlayImage.height());

  const auto prevZoom = renderer_->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer_->getPan(prevPanX, prevPanY);
  renderer_->setCanvasSize(drawW, drawH);
  renderer_->setZoom(1.0f);
  renderer_->setPan(0.0f, 0.0f);
  renderer_->drawSprite(0.0f, 0.0f, drawW, drawH, overlayImage, 1.0f);
  renderer_->setZoom(prevZoom);
  renderer_->setPan(prevPanX, prevPanY);
  if (comp) {
    const QSize compSize = comp->settings().compositionSize();
    const float cw =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
    const float ch =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
    renderer_->setCanvasSize(cw, ch);
   }
  }

  void CompositionRenderController::setRenderQueueActive(bool active) {
    if (impl_) {
      impl_->renderQueueActive_ = active;
    }
  }

  bool CompositionRenderController::isRenderQueueActive() const {
    return impl_ ? impl_->renderQueueActive_ : false;
  }

 } // namespace Artifact
