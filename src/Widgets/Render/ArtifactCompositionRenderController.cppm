module;
#include <DeviceContext.h>
#define NOMINMAX
#define QT_NO_KEYWORDS

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QFuture>
#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QMutex>
#include <QPainter>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <deque>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <opencv2/core.hpp>
#include <utility>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderController;

import Artifact.Render.IRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.Queue.Service;
import Artifact.Render.Config;
import Artifact.Render.ROI;
import Artifact.Render.Context;
import Artifact.Preview.Pipeline;
import Frame.Debug;
import Core.Diagnostics.Trace;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Particle;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Layer.Composition;
import Artifact.Layer.Shape;
import Layer.Matte;
import Artifact.Layers.Model3D;
import Artifact.Render.Offscreen;
import Image.ImageF32x4_RGBA;
import Frame.Position;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import UI.View.Orientation.Navigator;
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
import Core.Diagnostics.Trace;

import Artifact.Service.Project;
import Artifact.Service.Playback; // 追加
import Playback.State;
import Thread.Helper;
import Event.Bus;
import Artifact.Event.Types;
import Undo.UndoManager;
import Frame.Position;
import Color.Float;
import Image;
import CvUtils;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

W_OBJECT_IMPL(CompositionRenderController)

namespace {
Q_LOGGING_CATEGORY(compositionViewLog, "artifact.compositionview")

QString renderBackendToString(ArtifactRenderQueueService::RenderBackend backend)
{
  switch (backend) {
  case ArtifactRenderQueueService::RenderBackend::CPU:
    return QStringLiteral("cpu");
  case ArtifactRenderQueueService::RenderBackend::GPU:
    return QStringLiteral("gpu");
  case ArtifactRenderQueueService::RenderBackend::Auto:
  default:
    return QStringLiteral("auto");
  }
}

qint64 latestTimerMs(const std::string& timerName)
{
  const auto latest = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();
  const auto it = latest.find(timerName);
  if (it == latest.end()) {
    return 0;
  }
  return static_cast<qint64>(std::llround(it->second.durationMs));
}

QString playbackStateToString(PlaybackState state)
{
  switch (state) {
  case PlaybackState::Playing:
    return QStringLiteral("playing");
  case PlaybackState::Paused:
    return QStringLiteral("paused");
  case PlaybackState::Stopped:
  default:
    return QStringLiteral("stopped");
  }
}

bool floatColorEquals(const FloatColor &lhs, const FloatColor &rhs,
                      float epsilon = 0.0f) {
  if (epsilon <= 0.0f) {
    return lhs.r() == rhs.r() && lhs.g() == rhs.g() && lhs.b() == rhs.b() &&
           lhs.a() == rhs.a();
  }
  return std::abs(lhs.r() - rhs.r()) <= epsilon &&
         std::abs(lhs.g() - rhs.g()) <= epsilon &&
         std::abs(lhs.b() - rhs.b()) <= epsilon &&
         std::abs(lhs.a() - rhs.a()) <= epsilon;
}

QImage makeSolidColorSprite(const FloatColor &color) {
  QImage image(1, 1, QImage::Format_RGBA8888);
  image.fill(QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));
  return image;
}

QImage makeMayaGradientSprite(const FloatColor &baseColor) {
  constexpr int kWidth = 4;
  constexpr int kHeight = 4096; // 1024→4096 to reduce banding
  QImage image(kWidth, kHeight, QImage::Format_RGBA64); // 16-bit per channel
  image.fill(Qt::transparent);

  QPainter painter(&image);
  QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(kHeight));
  // Use more color stops for smoother transitions
  gradient.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));
  gradient.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));
  gradient.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));
  gradient.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));
  gradient.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));
  gradient.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));
  gradient.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));
  gradient.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));
  gradient.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));
  gradient.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));
  gradient.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));
  painter.fillRect(image.rect(), gradient);

  QLinearGradient glow(0.0, 0.0, static_cast<qreal>(kWidth), static_cast<qreal>(kHeight));
  QColor tint = QColor::fromRgbF(baseColor.r(), baseColor.g(), baseColor.b(), 1.0f);
  tint.setAlpha(72);
  glow.setColorAt(0.0, tint.lighter(112));
  QColor tintDark = tint.darker(140);
  tintDark.setAlpha(28);
  glow.setColorAt(1.0, tintDark);
  painter.fillRect(image.rect(), glow);
  return image;
}

enum class SelectionMode { Replace, Add, Toggle };

enum class LayerDragMode { None, Move, ScaleTL, ScaleTR, ScaleBL, ScaleBR };

enum class MaskHandleType { None, InTangent, OutTangent };

QColor toQColor(const FloatColor &color) {
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

bool buildRasterizedSurfaceBuffer(ArtifactAbstractLayer *targetLayer,
                                  const QImage &surface,
                                  ArtifactCore::ImageF32x4_RGBA *outBuffer) {
  if (!targetLayer || surface.isNull() || !outBuffer) {
    return false;
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
    return false;
  }

  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
  if (mat.type() != CV_32FC4) {
    mat.convertTo(mat, CV_32FC4, 1.0 / 255.0);
  }

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

  if (hasMasks) {
    // Mask vertices are in layer-local space (centered at 0,0).
    // Translate to pixel space: pixel = localPos - localBounds.topLeft()
    const QRectF lb = targetLayer->localBounds();
    const float maskOffsetX = static_cast<float>(-lb.x());
    const float maskOffsetY = static_cast<float>(-lb.y());
    for (int m = 0; m < targetLayer->maskCount(); ++m) {
      LayerMask mask = targetLayer->mask(m);
      mask.applyToImage(mat.cols, mat.rows, &mat, maskOffsetX, maskOffsetY);
    }
  }

  outBuffer->setFromCVMat(mat);
  return true;
}

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex, MaskHandleType handleType)
{
  const MaskVertex vertex = path.vertex(vertexIndex);
  switch (handleType) {
  case MaskHandleType::InTangent:
    return vertex.position + vertex.inTangent;
  case MaskHandleType::OutTangent:
    return vertex.position + vertex.outTangent;
  case MaskHandleType::None:
    break;
  }
  return vertex.position;
}

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPos,
                       float threshold,
                       int& outMaskIndex,
                       int& outPathIndex,
                       int& outVertexIndex,
                       MaskHandleType& outHandleType)
{
  if (!layer) {
    return false;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {
    return false;
  }

  const QPointF localPos = invTransform.map(canvasPos);
  const float thresholdSq = threshold * threshold;
  for (int m = 0; m < layer->maskCount(); ++m) {
    const LayerMask mask = layer->mask(m);
    if (!mask.isEnabled()) {
      continue;
    }
    for (int p = 0; p < mask.maskPathCount(); ++p) {
      const MaskPath path = mask.maskPath(p);
      for (int v = 0; v < path.vertexCount(); ++v) {
        const MaskVertex vertex = path.vertex(v);
        for (MaskHandleType handleType : {MaskHandleType::InTangent, MaskHandleType::OutTangent}) {
          if ((handleType == MaskHandleType::InTangent && vertex.inTangent == QPointF(0, 0)) ||
              (handleType == MaskHandleType::OutTangent && vertex.outTangent == QPointF(0, 0))) {
            continue;
          }
          const QPointF handlePos = maskHandlePosition(path, v, handleType);
          const QPointF delta = handlePos - localPos;
          if (QPointF::dotProduct(delta, delta) <= thresholdSq) {
            outMaskIndex = m;
            outPathIndex = p;
            outVertexIndex = v;
            outHandleType = handleType;
            return true;
          }
        }
      }
    }
  }

  return false;
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

bool layerNeedsFrameSyncForCompositionView(ArtifactAbstractLayer *layer) {
  if (!layer) {
    return false;
  }

  // Animated playback-critical layers still need their frame propagated.
  if (dynamic_cast<ArtifactVideoLayer *>(layer) ||
      dynamic_cast<ArtifactParticleLayer *>(layer) ||
      dynamic_cast<ArtifactCompositionLayer *>(layer)) {
    return true;
  }

  if (layer->isTimeRemapEnabled() || layer->hasMasks() ||
      !layer->getEffects().empty()) {
    return true;
  }

  const auto &transform = layer->transform3D();
  if (transform.getPositionKeyFrameCount() > 0 ||
      transform.getRotationKeyFrameCount() > 0 ||
      transform.getScaleKeyFrameCount() > 0) {
    return true;
  }

  if (auto *solidImage = dynamic_cast<ArtifactSolidImageLayer *>(layer)) {
    if (const auto property =
            solidImage->getProperty(QStringLiteral("solid.color"));
        property && !property->getKeyFrames().empty()) {
      return true;
    }
  }

  return false;
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

bool isLayerEffectivelyVisible(const ArtifactAbstractLayerPtr &layer) {
  ArtifactAbstractLayerPtr current = layer;
  int guard = 0;
  while (current && guard < 64) {
    if (!current->isVisible()) {
      return false;
    }
    current = current->parentLayer();
    ++guard;
  }
  return static_cast<bool>(layer);
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

QMatrix4x4 viewportOrientationViewMatrix(
    const ArtifactCore::ViewOrientationHotspot hotspot, const float cw,
    const float ch) {
  QMatrix4x4 view;
  const QQuaternion q =
      ArtifactCore::ViewOrientationNavigator::orientationForHotspot(hotspot);
  view.translate(cw * 0.5f, ch * 0.5f, 0.0f);
  view.rotate(q.conjugated());
  view.translate(-cw * 0.5f, -ch * 0.5f, 0.0f);
  return view;
}

QMatrix4x4 viewportOrientationProjectionMatrix(const float viewportW,
                                               const float viewportH) {
  QMatrix4x4 proj;
  const float w = std::max(1.0f, viewportW);
  const float h = std::max(1.0f, viewportH);
  proj.ortho(0.0f, w, h, 0.0f, -100000.0f, 100000.0f);
  return proj;
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
  if (!layer || !rect.isValid() || !isLayerEffectivelyVisible(layer) ||
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
    if (!isLayerEffectivelyVisible(layer) || !layer->isActiveAt(currentFrame)) {
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

/// Apply track matte to a layer surface by evaluating its MatteStack.
/// Modifies the surface's alpha channel in-place.
static void applyLayerMatteToSurface(
    ArtifactAbstractLayer *layer, QImage &surface,
    const std::function<QImage(const ArtifactCore::Id &)> &sourceResolver)
{
    auto mattes = layer->matteReferences();
    if (mattes.empty()) return;

    const int w = surface.width();
    const int h = surface.height();
    if (w <= 0 || h <= 0) return;

    // Build source alpha buffers from matte source layers
    std::vector<std::vector<float>> sources;
    for (const auto &ref : mattes) {
        if (!ref.enabled || ref.sourceLayerId.isNil()) continue;

        QImage srcImage = sourceResolver(ref.sourceLayerId);
        if (srcImage.isNull()) continue;

        // Resize to match target surface
        if (srcImage.size() != surface.size()) {
            srcImage = srcImage.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        srcImage = srcImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const int srcW = srcImage.width();
        const int srcH = srcImage.height();

        std::vector<float> alpha(static_cast<size_t>(w) * h, 0.0f);
        const bool useLuma = (ref.type == MatteType::Luma || ref.type == MatteType::InverseLuma);

        for (int y = 0; y < h && y < srcH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb *>(srcImage.constScanLine(y));
            for (int x = 0; x < w && x < srcW; ++x) {
                QRgb px = srcLine[x];
                if (useLuma) {
                    // ITU-R BT.601 luma
                    float luma = 0.299f * qRed(px) + 0.587f * qGreen(px) + 0.114f * qBlue(px);
                    alpha[static_cast<size_t>(y) * w + x] = luma / 255.0f;
                } else {
                    alpha[static_cast<size_t>(y) * w + x] = qAlpha(px) / 255.0f;
                }
            }
        }

        // Invert if needed
        if (ref.invert) {
            for (auto &v : alpha) v = 1.0f - v;
        }

        sources.push_back(std::move(alpha));
    }

    if (sources.empty()) return;

    // Use Core's evaluateMatteStack to combine sources
    MatteStack stack;
    for (const auto &ref : mattes) {
        if (ref.enabled && !ref.sourceLayerId.isNil()) {
            stack.addNode(ref.toCoreMatteNode());
        }
    }

    auto result = evaluateMatteStack(sources, stack, w, h);
    if (!result.isValid()) return;

    // Apply alpha mask to surface
    surface = surface.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(surface.scanLine(y));
        for (int x = 0; x < w; ++x) {
            float maskAlpha = result.sampleAlpha(x, y);
            int currentAlpha = qAlpha(line[x]);
            int newAlpha = static_cast<int>(currentAlpha * maskAlpha + 0.5f);
            line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]),
                            std::clamp(newAlpha, 0, 255));
        }
    }
}

void drawLayerForCompositionView(
    ArtifactAbstractLayer *layer, ArtifactIRenderer *renderer,
    float opacityOverride = -1.0f, QString *videoDebugOut = nullptr,
    QHash<QString, LayerSurfaceCacheEntry> *surfaceCache = nullptr,
    GPUTextureCacheManager *gpuTextureCacheManager = nullptr,
    int64_t cacheFrameNumber = std::numeric_limits<int64_t>::min(),
    bool useGpuPath = false, const DetailLevel lod = DetailLevel::High,
    const QMatrix4x4 *cameraView = nullptr,
    const QMatrix4x4 *cameraProj = nullptr,
    const std::function<QImage(const ArtifactCore::Id &)> *matteSourceResolver = nullptr) {
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
        ArtifactCore::ImageF32x4_RGBA processed;
        if (buildRasterizedSurfaceBuffer(targetLayer, surface, &processed)) {
          surface = processed.toQImage();
        }
      };

  auto hasRasterizerEffectsOrMasks = [](ArtifactAbstractLayer *targetLayer) {
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

    // Apply track matte before drawing
    if (matteSourceResolver && layer->matteReferences().size() > 0) {
        applyLayerMatteToSurface(layer, surface, *matteSourceResolver);
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
        std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> processedBuffer;
        if (allowSurfaceCache) {
          ArtifactCore::ImageF32x4_RGBA processed;
          if (buildRasterizedSurfaceBuffer(layer, surface, &processed)) {
            processedBuffer =
                std::make_shared<ArtifactCore::ImageF32x4_RGBA>(processed);
            if (!gpuTextureCacheManager ||
                !layerUsesGpuTextureCacheForCompositionView(layer)) {
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
        if (gpuTextureCacheManager &&
            layerUsesGpuTextureCacheForCompositionView(layer)) {
          if (entry.processedBuffer) {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                ownerId, cacheSignature, *entry.processedBuffer);
          } else {
            entry.gpuTextureHandle = gpuTextureCacheManager->acquireOrCreate(
                ownerId, cacheSignature, surface);
          }
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
              if (cacheEntry->processedBuffer) {
                cacheEntry->gpuTextureHandle =
                    gpuTextureCacheManager->acquireOrCreate(
                        layer->id().toString(), cacheSignature,
                        *cacheEntry->processedBuffer);
              } else {
                cacheEntry->gpuTextureHandle =
                    gpuTextureCacheManager->acquireOrCreate(
                        layer->id().toString(), cacheSignature, uploadSurface);
              }
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
      applySurfaceAndDraw(surface, localRect, true);
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
    const auto color = solidImage->color();
    if (hasRasterizerEffectsOrMasks(layer)) {
      const QSize surfaceSize(
          std::max(1, static_cast<int>(std::ceil(localRect.width()))),
          std::max(1, static_cast<int>(std::ceil(localRect.height()))));
      QImage surface(surfaceSize, QImage::Format_ARGB32_Premultiplied);
      surface.fill(toQColor(color));
      applySurfaceAndDraw(surface, localRect, true);
    } else {
      renderer->drawSolidRectTransformed(
          static_cast<float>(localRect.x()), static_cast<float>(localRect.y()),
          static_cast<float>(localRect.width()),
          static_cast<float>(localRect.height()), globalTransform4x4, color,
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity()));
    }
    return;
  }

  if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer) &&
        imageLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA &buffer =
          imageLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                buffer, baseOpacity * instanceWeight);
          });
      return;
    }

    const QImage img = imageLayer->toQImage();
    if (!img.isNull()) {
      applySurfaceAndDraw(img, localRect, hasRasterizerEffectsOrMasks(layer));
      return;
    }
  }

  if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(layer)) {
    if (svgLayer->isLoaded()) {
      if (!hasRasterizerEffectsOrMasks(layer) &&
          svgLayer->hasCurrentFrameBuffer()) {
        const ArtifactCore::ImageF32x4_RGBA &buffer =
            svgLayer->currentFrameBuffer();
        const float baseOpacity =
            (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
        drawWithClonerEffect(
            layer, globalTransform4x4,
            [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
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
                              hasRasterizerEffectsOrMasks(layer));
        } else {
          svgLayer->draw(renderer);
        }
      }
      return;
    }
  }

  if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {
    qCDebug(compositionViewLog)
        << "[VideoLayerT] drawLayerForCompositionView"
        << "frame=" << cacheFrameNumber
        << "opacityOverride=" << opacityOverride
        << "layerOpacity=" << layer->opacity()
        << "hasRasterizer=" << hasRasterizerEffectsOrMasks(layer)
        << videoLayer->debugState();
    if (!hasRasterizerEffectsOrMasks(layer) &&
        videoLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA &buffer =
          videoLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                buffer, baseOpacity * instanceWeight);
          });
      return;
    }

    QImage frame = videoLayer->currentFrameToQImage();
    bool usedSyncFallback = false;
    if (frame.isNull() && videoLayer->hasCurrentFrameBuffer()) {
      frame = videoLayer->currentFrameBuffer().toQImage();
    }
    if (frame.isNull()) {
      frame = videoLayer->decodeFrameToQImage(layer->currentFrame());
      usedSyncFallback = !frame.isNull();
    }
    if (videoDebugOut) {
      const bool active =
          layer->isActiveAt(FramePosition(static_cast<int>(layer->currentFrame())));
      *videoDebugOut =
          QStringLiteral("[Video] %1 active=%2 syncFallback=%3")
              .arg(videoLayer->debugState())
              .arg(active)
              .arg(usedSyncFallback ? QStringLiteral("hit")
                                    : QStringLiteral("miss"));
    }
    if (!frame.isNull()) {
      applySurfaceAndDraw(frame, localRect, true);
      return;
    }
  }

  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {
    if (!hasRasterizerEffectsOrMasks(layer)) {
      textLayer->draw(renderer);
      return;
    }
    if (textLayer->hasCurrentFrameBuffer()) {
      const ArtifactCore::ImageF32x4_RGBA &buffer =
          textLayer->currentFrameBuffer();
      const float baseOpacity =
          (opacityOverride >= 0.0f ? opacityOverride : layer->opacity());
      drawWithClonerEffect(
          layer, globalTransform4x4,
          [&](const QMatrix4x4 &instanceTransform, float instanceWeight) {
            renderer->drawSpriteTransformed(
                static_cast<float>(localRect.x()),
                static_cast<float>(localRect.y()),
                static_cast<float>(localRect.width()),
                static_cast<float>(localRect.height()), instanceTransform,
                buffer, baseOpacity * instanceWeight);
          });
      return;
    }
    const QImage textImage = textLayer->toQImage();
    if (!textImage.isNull()) {
      applySurfaceAndDraw(textImage, localRect, true);
    }
    return;
  }

  if (auto *compLayer = dynamic_cast<ArtifactCompositionLayer *>(layer)) {
    if (auto childComp = compLayer->sourceComposition()) {
      const QSize childSize = childComp->settings().compositionSize();
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

// Draws composition border outlines only (no background fill, no checkerboard).
// Background fill (bgColor) is drawn in the background phase before layer
// rendering. Checkerboard is drawn via drawCompositionCheckerboard().
// This function is called in the overlay phase (after layers) so it must NOT
// draw any solid fill that would cover layer content.
void drawCompositionRegionOverlay(ArtifactIRenderer *renderer,
                                  const ArtifactCompositionPtr &comp) {
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

  const FloatColor darkColor{0.02f, 0.02f, 0.02f, 0.85f};
  const FloatColor lightColor{0.42f, 0.68f, 0.96f, 0.95f};
  renderer->drawSolidLine({0.0f, 0.0f}, {cw, 0.0f}, darkColor, 1.0f);
  renderer->drawSolidLine({cw, 0.0f}, {cw, ch}, darkColor, 1.0f);
  renderer->drawSolidLine({cw, ch}, {0.0f, ch}, darkColor, 1.0f);
  renderer->drawSolidLine({0.0f, ch}, {0.0f, 0.0f}, darkColor, 1.0f);
  renderer->drawSolidLine({0.0f, 0.0f}, {cw, 0.0f}, lightColor, 1.0f);
  renderer->drawSolidLine({cw, 0.0f}, {cw, ch}, lightColor, 1.0f);
  renderer->drawSolidLine({cw, ch}, {0.0f, ch}, lightColor, 1.0f);
  renderer->drawSolidLine({0.0f, ch}, {0.0f, 0.0f}, lightColor, 1.0f);
}

void drawAnchorCenterOverlay(ArtifactIRenderer *renderer,
                             const ArtifactAbstractLayerPtr &layer) {
  if (!renderer || !layer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const auto &t3d = layer->transform3D();
  const QPointF anchorLocal(t3d.anchorX(), t3d.anchorY());
  const QPointF centerLocal = localBounds.center();
  const QPointF anchorCanvas = globalTransform.map(anchorLocal);
  const QPointF centerCanvas = globalTransform.map(centerLocal);

  const float zoom = std::max(0.001f, renderer->getZoom());
  const float pointSize = std::max(6.0f, 10.0f / zoom);
  const float crossSize = std::max(10.0f, 18.0f / zoom);
  const FloatColor anchorColor{1.0f, 0.64f, 0.18f, 0.98f};
  const FloatColor centerColor{0.28f, 0.82f, 1.0f, 0.98f};

  renderer->drawPoint(static_cast<float>(anchorCanvas.x()),
                      static_cast<float>(anchorCanvas.y()), pointSize,
                      anchorColor);
  renderer->drawCrosshair(static_cast<float>(anchorCanvas.x()),
                          static_cast<float>(anchorCanvas.y()), crossSize,
                          anchorColor);

  renderer->drawPoint(static_cast<float>(centerCanvas.x()),
                      static_cast<float>(centerCanvas.y()), pointSize,
                      centerColor);
  renderer->drawCrosshair(static_cast<float>(centerCanvas.x()),
                          static_cast<float>(centerCanvas.y()), crossSize,
                          centerColor);
}

void drawSelectionOverlay(ArtifactIRenderer *renderer,
                          const ArtifactAbstractLayerPtr &layer) {
  if (!renderer || !layer) {
    return;
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
      localBounds.height() <= 0.0) {
    return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const QPointF tl = globalTransform.map(localBounds.topLeft());
  const QPointF tr = globalTransform.map(localBounds.topRight());
  const QPointF br = globalTransform.map(localBounds.bottomRight());
  const QPointF bl = globalTransform.map(localBounds.bottomLeft());

  const FloatColor outerColor{0.15f, 0.95f, 1.0f, 0.92f};
  const FloatColor innerColor{0.02f, 0.08f, 0.10f, 0.72f};
  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          outerColor, 1.8f);
  renderer->drawSolidLine({static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(tr.x()), static_cast<float>(tr.y())},
                          {static_cast<float>(br.x()), static_cast<float>(br.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(br.x()), static_cast<float>(br.y())},
                          {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          innerColor, 0.8f);
  renderer->drawSolidLine({static_cast<float>(bl.x()), static_cast<float>(bl.y())},
                          {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
                          innerColor, 0.8f);

  const float zoom = std::max(0.001f, renderer->getZoom());
  const float nodeSize = std::max(4.5f, 7.5f / zoom);
  const FloatColor nodeColor{1.0f, 0.94f, 0.32f, 0.98f};

  if (const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
    const auto type = shape->shapeType();
    if (type == ShapeType::Polygon) {
      const auto points = shape->customPolygonPoints();
      if (!points.empty()) {
        QPointF prev = globalTransform.map(points.front());
        for (size_t i = 1; i < points.size(); ++i) {
          const QPointF cur = globalTransform.map(points[i]);
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
              outerColor, 1.2f);
          prev = cur;
        }
        if (shape->customPolygonClosed() && points.size() > 1) {
          const QPointF first = globalTransform.map(points.front());
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(first.x()), static_cast<float>(first.y())},
              outerColor, 1.2f);
        }
        for (const auto &pt : points) {
          const QPointF canvasPt = globalTransform.map(pt);
          renderer->drawPoint(static_cast<float>(canvasPt.x()),
                              static_cast<float>(canvasPt.y()), nodeSize,
                              nodeColor);
        }
      }
    } else if (!shape->customPathVertices().empty()) {
      const auto vertices = shape->customPathVertices();
      QPointF prev;
      bool hasPrev = false;
      for (const auto &vertex : vertices) {
        const QPointF canvasPt = globalTransform.map(vertex.pos);
        renderer->drawPoint(static_cast<float>(canvasPt.x()),
                            static_cast<float>(canvasPt.y()), nodeSize,
                            nodeColor);
        if (hasPrev) {
          renderer->drawSolidLine(
              {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
              {static_cast<float>(canvasPt.x()), static_cast<float>(canvasPt.y())},
              outerColor, 1.0f);
        }
        prev = canvasPt;
        hasPrev = true;
      }
      if (shape->customPathClosed() && vertices.size() > 1) {
        const QPointF first = globalTransform.map(vertices.front().pos);
        renderer->drawSolidLine(
            {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
            {static_cast<float>(first.x()), static_cast<float>(first.y())},
            outerColor, 1.0f);
      }
    }
  }
}

// Draws checkerboard in Viewport Space so transparent regions of the
// composition reveal the pattern against the viewport background.
// This should be called before blitting the composition result to the
// visible framebuffer.
void drawViewportCheckerboardBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh) {
  if (!renderer || vw <= 0.0f || vh <= 0.0f) {
    return;
  }

  // Save current state
  const float savedZoom = renderer->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  renderer->getPan(savedPanX, savedPanY);

  renderer->setCanvasSize(vw, vh);
  renderer->setZoom(1.0f);
  renderer->setPan(0.0f, 0.0f);
  renderer->drawCheckerboard(0.0f, 0.0f, vw, vh, 16.0f,
                             {0.18f, 0.18f, 0.18f, 1.0f},
                             {0.28f, 0.28f, 0.28f, 1.0f});

  // Restore state
  renderer->setZoom(savedZoom);
  renderer->setPan(savedPanX, savedPanY);
}

// Draws checkerboard over the composition canvas in Composition Space.
// Must be called BEFORE layer drawing so transparent regions reveal the
// pattern.
void drawCompositionCheckerboard(ArtifactIRenderer *renderer,
                                 const ArtifactCompositionPtr &comp) {
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

  renderer->drawCheckerboard(0.0f, 0.0f, cw, ch, 16.0f,
                             {0.18f, 0.18f, 0.18f, 1.0f},
                             {0.28f, 0.28f, 0.28f, 1.0f});
}

// Draws the composition background in Composition Space (0,0)-(cw,ch).
// renderer must already be configured with setCanvasSize(cw,ch), zoom, and pan.
// The vertex shader handles the Composition→View→NDC transform automatically.
// MayaGradient is handled separately in viewport space so it can cover the
// full visible viewport instead of being clipped to the composition rect.
void drawCompositionBackgroundDirect(ArtifactIRenderer *renderer, float cw,
                                     float ch, const FloatColor &bgColor,
                                     CompositionBackgroundMode mode,
                                     const QImage &mayaGradientSprite) {
  if (!renderer || cw <= 0.0f || ch <= 0.0f) {
    return;
  }
  if (mode == CompositionBackgroundMode::Solid) {
    renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    return;
  }
  if (mode == CompositionBackgroundMode::Checkerboard) {
    // Checkerboard is the viewport background; composition area shows bgColor.
    renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    return;
  }
  // MayaGradient only affects the viewport background; the composition area
  // itself is always rendered with the solid bgColor.
  if (mode == CompositionBackgroundMode::MayaGradient) {
    renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
    return;
  }
  renderer->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
}

void drawViewportMayaGradientBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh, const FloatColor &bgColor,
                                        const QImage &mayaGradientSprite) {
  if (!renderer || vw <= 0.0f || vh <= 0.0f) {
    return;
  }
  if (!mayaGradientSprite.isNull()) {
    renderer->drawSpriteTransformed(0.f, 0.f, vw, vh, QMatrix4x4{},
                                    mayaGradientSprite, 1.0f);
    return;
  }
  renderer->drawRectLocal(0.f, 0.f, vw, vh, bgColor, 1.0f);
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
  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;
  RenderPipeline renderPipeline_;
  QTimer *renderTimer_ = nullptr;
  bool initialized_ = false;
  bool running_ = false;
  float devicePixelRatio_ = 1.0f;
  bool renderScheduled_ = false;

  // Fixed-rate render tick (Phase 1: infrastructure only)
  QTimer *renderTickTimer_ = nullptr;
  std::atomic_bool renderDirty_{false};
  static constexpr int kRenderTickIntervalMs = 16; // ~60fps
  QElapsedTimer startupTimer_;
  bool blendPipelineReady_ = false;
  bool blendPipelineInitAttempted_ = false;
  bool gpuBlendEnabled_ =
      !qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");
  QString lastVideoDebug_;
  QString lastEmittedVideoDebug_;
  QString lastRenderPathSummary_;
  qint64 lastSetupMs_ = 0;
  qint64 lastBasePassMs_ = 0;
  qint64 lastLayerPassMs_ = 0;
  qint64 lastOverlayMs_ = 0;
  qint64 lastFlushMs_ = 0;
  qint64 lastSubmit2DMs_ = 0;
  qint64 lastPresentMs_ = 0;
  QVector<QMetaObject::Connection> layerChangedConnections_;
  QMetaObject::Connection compositionChangedConnection_;

  // 変更検出器 (差分レンダリング用)
  CompositionChangeDetector changeDetector_;

  // LOD (Level of Detail)
  std::unique_ptr<LODManager> lodManager_;
  bool lodEnabled_ = true;

  LayerID selectedLayerId_;
  bool isDraggingLayer_ = false;
  bool gizmoDragActive_ = false;
  bool pendingMaskCreation_ = false;
  LayerID pendingMaskLayerId_;
  MaskPath pendingMaskPath_;
  void clearPendingMaskCreation();
  void beginPendingMaskCreation(const ArtifactAbstractLayerPtr &layer,
                                const QPointF &localPos);
  bool finalizePendingMaskCreation(const ArtifactAbstractLayerPtr &layer);

  // MayaGradient sprite cache — regenerated only when bgColor changes
  QImage cachedMayaGradientSprite_;
  FloatColor cachedMayaGradientBgColor_ = {-1.f, -1.f, -1.f, -1.f};
  FloatColor cachedMayaGradientWarmupBgColor_ = {-1.f, -1.f, -1.f, -1.f};
  QFuture<QImage> cachedMayaGradientWarmupFuture_;
  bool cachedMayaGradientWarmupQueued_ = false;
  QString lastMayaGradientDebugState_;

  // Solo layer presence cache — invalidated on any layer solo/visible change
  bool hasSoloLayerCache_ = false;
  bool soloLayerCacheDirty_ = true;

  LayerDragMode dragMode_ = LayerDragMode::None;
  TransformGizmo::Mode gizmoMode_ = TransformGizmo::Mode::All;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  QRectF dragStartBoundingBox_;
  int64_t dragFrame_ = 0;
  QPointF dragAppliedDelta_;
  bool showGrid_ = false;
  // Composition canvas fill mode, separate from the renderer's viewport clear
  // color.
  CompositionBackgroundMode compositionBackgroundMode_ =
      CompositionBackgroundMode::Checkerboard;
  bool showGuides_ = false;
  bool showSafeMargins_ = false;
  bool showMotionPathOverlay_ = false;
  bool showAnchorCenterOverlay_ = false;
  bool showFrameInfo_ = false; // Changed to false by default
  bool showGizmoOverlay_ = true;
  bool showCompositionRegionOverlay_ =
      false; // Temporarily disable the blue frame.
  ArtifactCore::ViewOrientationNavigator viewportOrientationNavigator_;
  bool viewportOrientationActive_ = false;
  int currentFrameForOverlay_ = 0;
  quint64 renderFrameCounter_ = 0;
  std::deque<double> recentFrameTimesMs_;
  double recentFrameTimeSumMs_ = 0.0;
  double lastFrameTimeMs_ = 0.0;
  double averageFrameTimeMs_ = 0.0;
  bool renderQueueActive_ =
      false; // When true, suppress cache invalidation during Render Queue
  int lastPipelineStateMask_ = -1;
  QSize lastDispatchWarningSize_;

  // Packed render key state for fast change detection.
  // Replaces the per-frame QByteArray string concatenation.
  struct RenderKeyState {
    CompositionID compId;
    quint64 baseSerial = 0;
    quint64 overlaySerial = 0;
    int64_t frame = 0;
    int32_t viewW = 0, viewH = 0, downsample = 0;
    float zoom = 0, panX = 0, panY = 0;
    float bgR = 0, bgG = 0, bgB = 0, bgA = 0;
    int32_t bgMode = 0;
    int32_t gizmoMode = -1, gizmoHover = -1, gizmoActive = -1;
    uint8_t gpuBlend = 0, showGrid = 0, showGuides = 0, showSafeMargins = 0,
            showAnchorCenter = 0, viewportInteracting = 0;
    LayerID selectedLayerId;

    bool operator==(const RenderKeyState &o) const {
      return compId == o.compId && baseSerial == o.baseSerial &&
             overlaySerial == o.overlaySerial && frame == o.frame &&
             viewW == o.viewW && viewH == o.viewH &&
             downsample == o.downsample && zoom == o.zoom && panX == o.panX &&
             panY == o.panY && bgR == o.bgR && bgG == o.bgG && bgB == o.bgB &&
             bgA == o.bgA && bgMode == o.bgMode && gizmoMode == o.gizmoMode &&
             gizmoHover == o.gizmoHover && gizmoActive == o.gizmoActive &&
             gpuBlend == o.gpuBlend && showGrid == o.showGrid &&
             showGuides == o.showGuides &&
             showSafeMargins == o.showSafeMargins &&
             showAnchorCenter == o.showAnchorCenter &&
             viewportInteracting == o.viewportInteracting &&
             selectedLayerId == o.selectedLayerId;
    }
    bool operator!=(const RenderKeyState &o) const { return !(*this == o); }
  };
  RenderKeyState lastRenderKeyState_{};
  QString lastOverlayDebugSummary_;
  quint64 baseInvalidationSerial_ = 1;
  quint64 overlayInvalidationSerial_ = 1;
  RenderDamageTracker damageTracker_;

  // Motion path cache: avoids 300x getGlobalTransformAt() calls per frame.
  // Invalidated when layer, frame position, or overlay serial changes.
  struct MotionPathCacheEntry {
    LayerID layerId;
    int64_t framePos = INT64_MIN;
    quint64 overlaySerial = UINT64_MAX;
    struct Pt {
      int frame;
      float x, y;
    };
    std::vector<Pt> pathPoints;
    std::vector<Pt> keyPoints;
    bool valid = false;
  };
  MotionPathCacheEntry motionPathCache_;
  bool dropGhostVisible_ = false;
  QRectF dropGhostRect_;
  QString dropGhostTitle_;
  QString dropGhostHint_;
  QString dropCandidateLabel_;
  bool infoOverlayVisible_ = false;
  QString infoOverlayTitle_;
  QString infoOverlayDetail_;
  bool commandPaletteVisible_ = false;
  QString commandPaletteQuery_;
  QStringList commandPaletteItems_;
  bool contextMenuVisible_ = false;
  QPointF contextMenuViewportPos_;
  QStringList contextMenuItems_;
  bool pieMenuVisible_ = false;
  PieMenuModel pieMenuModel_;
  QPointF pieMenuViewportPos_;
  QPointF pieMenuMousePos_;
  int pieMenuSelectedIndex_ = -1;
  // Full-frame clear color used before composition content is drawn.
  FloatColor viewportClearColor_;
  FloatColor lastBgColorCache_ = {-1.f, -1.f, -1.f, -1.f};
  CompositionID lastBackgroundCompositionId_;
  QHash<QString, LayerSurfaceCacheEntry> surfaceCache_;
  std::unique_ptr<GPUTextureCacheManager> gpuTextureCacheManager_;

  // Render debounce timer: coalesces rapid LayerChangedEvent notifications
  // into a single renderOneFrame() call, preventing GPU saturation during drag
  QTimer *renderDebounceTimer_ = nullptr;
  static constexpr qint64 kRenderDebounceIntervalMs = 33; // ~30fps baseline
  
  // Render tick infrastructure
  bool renderInProgress_ = false;  // Guards against concurrent renderOneFrameImpl() calls
  QElapsedTimer gizmoDragRenderTimer_;  // Throttles render frequency during gizmo drag

  // Guide positions (composition-space pixels)
  QVector<float> guideVerticals_;   // X positions
  QVector<float> guideHorizontals_; // Y positions
  float lastCanvasWidth_ = 1920.0f;
  float lastCanvasHeight_ = 1080.0f;

  // Resolution scaling
  int previewDownsample_ = 1;
  int interactivePreviewDownsampleFloor_ = 4;
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
  int hoveredMaskHandleType_ = -1;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  int draggingMaskHandleType_ = -1;
  bool isDraggingVertex_ = false;
  bool isDraggingMaskHandle_ = false;
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

    // NOTE: renderPipeline_ は renderOneFrameImpl() でビューポートの実サイズ
    // (rcw, rch) を使って初期化される。ここでコンポジションサイズ (cw, ch) で
    // 初期化すると、サイズが異なる場合に未初期化の D3D12 テクスチャが生成され、
    // 次フレームで再び不一致が起き、ゴミデータが画面に出ることがある。
    // パイプラインのサイズ管理は renderOneFrameImpl() に一元化する。
  }

  void bindCompositionChanged(CompositionRenderController *owner,
                              const ArtifactCompositionPtr &composition) {
    // Layer change notifications are now handled exclusively via
    // LayerChangedEvent. composition->changed is reserved for composition-level
    // setting changes only.
    if (compositionChangedConnection_) {
      QObject::disconnect(compositionChangedConnection_);
      compositionChangedConnection_ = {};
    }
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
    LayerInvalidationRegion region;
    region.source = LayerInvalidationRegion::Source::Content;
    region.layerId = ownerId;
    damageTracker_.markDirty(ownerId, region);
  }

  void invalidateBaseComposite() {
    ++baseInvalidationSerial_;
    lastRenderKeyState_ = {};
    damageTracker_.clearAll();
  }

  void invalidateOverlayComposite() {
    ++overlayInvalidationSerial_;
    lastRenderKeyState_ = {};
  }

  RenderDamageTracker& damageTracker() { return damageTracker_; }

  void drawViewportGhostOverlay(CompositionRenderController *owner,
                                const ArtifactCompositionPtr &comp,
                                const ArtifactAbstractLayerPtr &selectedLayer,
                                const FramePosition &currentFrame);
  QRectF commandPaletteRect() const;
  QRectF contextMenuRect() const;
  QRectF pieMenuRect() const;
  QRectF viewportOverlayItemRect(int index) const;
  int viewportOverlayItemAt(const QPointF &viewportPos) const;
  int pieMenuItemAt(const QPointF &viewportPos) const;
  void drawPieMenuOverlay();
  void drawViewportUiOverlay();

  // 変更検出器へのアクセス (デバッグ用)
  const CompositionChangeDetector &changeDetector() const {
    return changeDetector_;
  }

  CompositionChangeDetector &changeDetector() { return changeDetector_; }

  void renderOneFrameImpl(CompositionRenderController *owner);
};

CompositionRenderController::CompositionRenderController(QObject *parent)
    : QObject(parent), impl_(new Impl()) {
  impl_->viewportClearColor_ =
      FloatColor{QColor(28, 40, 56).redF(), QColor(28, 40, 56).greenF(),
                 QColor(28, 40, 56).blueF(), 1.0f};
  impl_->gizmo_ = std::make_unique<TransformGizmo>();
  impl_->gizmo3D_ = std::make_unique<Artifact3DGizmo>(this);

  // Connect to project service to track layer selection
  if (auto *svc = ArtifactProjectService::instance()) {
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
            [this](const LayerSelectionChangedEvent &event) {
              auto comp = impl_->previewPipeline_.composition();
              if (comp && !event.compositionId.isEmpty() &&
                  comp->id().toString() != event.compositionId) {
                return;
              }
              // Guard: ignore spurious nil events if the selection manager
              // still has a valid current layer. This prevents property-edit
              // notifications from clearing the gizmo via the EventBus path.
              const LayerID incomingId(event.layerId);
              if (incomingId.isNil() && !impl_->selectedLayerId_.isNil()) {
                auto *app = ArtifactApplicationManager::instance();
                auto *sel = app ? app->layerSelectionManager() : nullptr;
                if (sel && sel->currentLayer()) {
                  return;
               }
              }
              setSelectedLayerId(incomingId);
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>(
            [this](const LayerChangedEvent &event) {
              auto comp = impl_->previewPipeline_.composition();
              if (!comp) {
                return;
              }
              // Accept events with empty compositionId (layer may not have
              // its composition pointer set yet, e.g. during creation) as
              // well as events matching the current composition.
              if (!event.compositionId.isEmpty() &&
                  comp->id().toString() != event.compositionId) {
                return;
              }
              if (impl_->renderQueueActive_) {
                return;
              }
              const auto layerId = LayerID(event.layerId);
              if (event.changeType == LayerChangedEvent::ChangeType::Created ||
                  event.changeType == LayerChangedEvent::ChangeType::Removed) {
                // Structural change: full cache clear + composition state sync
                if (event.changeType ==
                    LayerChangedEvent::ChangeType::Removed) {
                  if (layerId == impl_->selectedLayerId_) {
                    impl_->selectedLayerId_ = LayerID();
                  }
               }
                impl_->surfaceCache_.clear();
                if (impl_->gpuTextureCacheManager_) {
                  impl_->gpuTextureCacheManager_->clear();
               }
                impl_->applyCompositionState(comp);
              } else {
                // Property/transform modification: invalidate only this layer
                // ギズモドラッグ中は同じレイヤーのピクセルは変わらないので
                // GPU テクスチャキャッシュ無効化をスキップ（transform のみ変化）
                if (auto layer = comp->layerById(layerId)) {
                  const bool skipCacheInvalidation =
                      impl_->gizmoDragActive_ &&
                      layerId == impl_->selectedLayerId_;
                  if (!skipCacheInvalidation) {
                    impl_->invalidateLayerSurfaceCache(layer);
                  }
                  impl_->changeDetector_.markLayerChanged(
                      layer->id().toString());
               }
              }
              impl_->invalidateBaseComposite();
              // ギズモドラッグ中はオーバーレイ同期コストを省く（ドラッグ終了時に一括同期）
              if (!impl_->gizmoDragActive_) {
                impl_->syncSelectedLayerOverlayState(
                    impl_->previewPipeline_.composition());
              }
               // Debounce render: coalesce rapid property changes into single frame
               if (impl_->renderDebounceTimer_) {
                 impl_->renderDebounceTimer_->start(
                     static_cast<int>(Impl::kRenderDebounceIntervalMs));
               }
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
            [this, svc](const CurrentCompositionChangedEvent &event) {
              ArtifactCompositionPtr comp;
              if (!event.compositionId.trimmed().isEmpty()) {
                const auto found =
                    svc->findComposition(CompositionID(event.compositionId));
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
            [this, svc](const ProjectChangedEvent &) {
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
            [this](const PreviewQualityPresetChangedEvent &event) {
              setPreviewQualityPreset(
                  static_cast<PreviewQualityPreset>(event.preset));
            }));

    // Initial sync
    setPreviewQualityPreset(svc->previewQualityPreset());
  }

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ToolChangedEvent>(
          [this](const ToolChangedEvent &event) {
            switch (event.toolType) {
            case ToolType::Selection:
              setGizmoMode(TransformGizmo::Mode::All);
              break;
            case ToolType::Move:
              setGizmoMode(TransformGizmo::Mode::Translation);
              break;
            case ToolType::Rotation:
              setGizmoMode(TransformGizmo::Mode::Rotation);
              break;
            case ToolType::Scale:
              setGizmoMode(TransformGizmo::Mode::Scale);
              break;
            case ToolType::AnchorPoint:
              setGizmoMode(TransformGizmo::Mode::AnchorPoint);
              break;
            default:
              // For other tools, we might want to disable the gizmo or use a
              // basic mode
              setGizmoMode(TransformGizmo::Mode::None);
              break;
            }
          }));
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
  impl_->startupTimer_.start();
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
  impl_->renderer_->setClearColor(impl_->viewportClearColor_);
  impl_->devicePixelRatio_ = static_cast<float>(hostWidget->devicePixelRatio());
  impl_->hostWidth_ =
      static_cast<float>(hostWidget->width()) * impl_->devicePixelRatio_;
  impl_->hostHeight_ =
      static_cast<float>(hostWidget->height()) * impl_->devicePixelRatio_;
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

  impl_->renderDebounceTimer_ = new QTimer(this);
  impl_->renderDebounceTimer_->setSingleShot(true);
  connect(impl_->renderDebounceTimer_, &QTimer::timeout, this,
          &CompositionRenderController::onRenderDebounceTimeout);

  // Fixed-rate render tick: consumes renderDirty_ flag at ~60fps.
  // In Phase 1 this timer is infrastructure-only (renderDirty_ is never set).
  // Phase 2 will migrate event handlers to use markRenderDirty().
  impl_->renderTickTimer_ = new QTimer(this);
  impl_->renderTickTimer_->setInterval(Impl::kRenderTickIntervalMs);
  connect(impl_->renderTickTimer_, &QTimer::timeout, this, [this]() {
    if (!impl_->renderDirty_.exchange(false, std::memory_order_acq_rel)) {
      return; // No state change since last tick — skip
    }
    // Guard: skip if host is hidden or not initialized
    if (!impl_->initialized_ || !impl_->renderer_ || !impl_->running_) {
      return;
    }
    if (auto *host = impl_->hostWidget_.data()) {
      if (!host->isVisible()) {
        return;
      }
    }
    if (impl_->renderInProgress_) {
      // A render is already in progress — mark dirty so next tick retries
      impl_->renderDirty_.store(true, std::memory_order_release);
      return;
    }
    impl_->renderInProgress_ = true;
    impl_->renderOneFrameImpl(this);
    impl_->renderInProgress_ = false;
  });
  impl_->renderTickTimer_->start();

  // PlaybackService のフレーム変更に合わせて再描画
  if (auto *playback = ArtifactPlaybackService::instance()) {
    connect(playback, &ArtifactPlaybackService::frameChanged, this,
            [this](const FramePosition &position) {
              // 可視性チェック: 非表示（他タブの裏など）なら描画しない
              if (auto *owner = qobject_cast<QWidget *>(parent())) {
                if (!owner->isVisible())
                  return;
              }
              // comp->goToFrame() は ArtifactPlaybackService::syncCurrentCompositionFrame()
              // が publishFrame より先に投入済みのため、ここで重複呼び出しは不要。
              impl_->invalidateOverlayComposite();
              // 更新要求を集約（Coalescing）
              renderOneFrame();
            });
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

  // Schedule blend pipeline initialization off the hot render path.
  // blendPipeline_->initialize() compiles GPU shaders (slow); running it
  // inside renderOneFrameImpl would stall the render timer for hundreds of ms.
  if (impl_->gpuBlendEnabled_) {
    QTimer::singleShot(1500, this, [this]() {
      if (!impl_ || !impl_->initialized_)
        return;
      if (impl_->blendPipelineReady_ || impl_->blendPipelineInitAttempted_)
        return;
      impl_->blendPipelineInitAttempted_ = true;
      auto *renderer = impl_->renderer_.get();
      if (!renderer)
        return;
      auto device = renderer->device();
      if (!device)
        return;
      if (!impl_->blendPipeline_)
        impl_->blendPipeline_ = renderer->createLayerBlendPipeline();
      if (impl_->blendPipeline_) {
        QElapsedTimer t;
        t.start();
        impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
        if (impl_->blendPipelineReady_) {
          qInfo() << "[CompositionView][Startup] blend pipeline lazy init ms="
                  << t.elapsed();
        } else {
          qWarning()
              << "[CompositionView] LayerBlendPipeline FAILED to initialize.";
        }
      }
    });
  }
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
  // If swapchain was never created (e.g., widget was 0×0 at init time),
  // use createSwapChain which handles fresh creation + shader/PSO setup.
  if (!impl_->renderer_->hasSwapChain()) {
    qDebug() << "[CompositionRenderController] recreateSwapChain: no swapchain "
                "— calling createSwapChain";
    impl_->renderer_->createSwapChain(hostWidget);
  } else {
    impl_->renderer_->recreateSwapChain(hostWidget);
  }
  impl_->invalidateBaseComposite();
}

void CompositionRenderController::setViewportSize(float w, float h) {
  if (!impl_->renderer_) {
    return;
  }
  // Refresh DPR whenever the viewport is resized (handles window-to-monitor
  // changes)
  if (impl_->hostWidget_) {
    impl_->devicePixelRatio_ =
        static_cast<float>(impl_->hostWidget_->devicePixelRatio());
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
      // hostWidth_/hostHeight_ are already physical pixels; call renderer
      // directly
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
  impl_->renderer_->panBy((float)viewportDelta.x() * impl_->devicePixelRatio_,
                          (float)viewportDelta.y() * impl_->devicePixelRatio_);
  // Phase 2: Use fixed-rate render tick instead of renderOneFrame().
  // No invalidateBaseComposite() needed — panX/panY in RenderKeyState
  // already detects the change.
  markRenderDirty();
}

void CompositionRenderController::setGizmoMode(
    const TransformGizmo::Mode mode) {
  if (!impl_) {
    return;
  }
  impl_->gizmoMode_ = mode;
  if (impl_->gizmo_) {
    impl_->gizmo_->setMode(mode);
  }
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

TransformGizmo::Mode CompositionRenderController::gizmoMode() const {
  return impl_ ? impl_->gizmoMode_ : TransformGizmo::Mode::All;
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

void CompositionRenderController::onRenderDebounceTimeout() {
  // Debounce timer fired: render once, coalescing all pending property changes
  renderOneFrame();
}

void CompositionRenderController::setComposition(
    ArtifactCompositionPtr composition) {
  qCDebug(compositionViewLog) << "[CompositionView] setComposition"
                              << "isNull=" << (composition == nullptr) << "id="
                              << (composition ? composition->id().toString()
                                              : QStringLiteral("<null>"));

  auto currentComposition = impl_->previewPipeline_.composition();
  const bool samePointer = (currentComposition == composition);
  const bool sameId = !samePointer && composition && currentComposition &&
                      composition->id() == currentComposition->id();

  if (samePointer || sameId) {
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
    if (sameId) {
      // Pointer changed but same composition — re-bind changed signals
      impl_->previewPipeline_.setComposition(composition);
      impl_->bindCompositionChanged(this, composition);
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
  impl_->clearPendingMaskCreation();
  impl_->selectedLayerId_ = id;
  impl_->invalidateOverlayComposite();
  impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());

  auto* sel = ArtifactLayerSelectionManager::instance();
  if (sel) {
    auto comp = impl_->previewPipeline_.composition();
    if (!id.isNil() && comp) {
      auto layer = comp->layerById(id);
      if (layer) {
        sel->selectLayer(layer);
      }
    } else {
      sel->clearSelection();
    }
  }

  renderOneFrame();
}

void CompositionRenderController::setClearColor(const FloatColor &color) {
  if (toQColor(impl_->viewportClearColor_) == toQColor(color)) {
    return;
  }
  impl_->viewportClearColor_ = color;
  if (impl_->renderer_) {
    impl_->renderer_->setClearColor(color);
  }
  impl_->invalidateBaseComposite();
  renderOneFrame();
}

FloatColor CompositionRenderController::clearColor() const {
  return impl_->viewportClearColor_;
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
  const auto nextMode = show ? CompositionBackgroundMode::Checkerboard
                             : CompositionBackgroundMode::Solid;
  if (impl_->compositionBackgroundMode_ == nextMode) {
    return;
  }
  impl_->compositionBackgroundMode_ = nextMode;
  impl_->invalidateBaseComposite();
  renderOneFrame();
}
bool CompositionRenderController::isShowCheckerboard() const {
  return impl_->compositionBackgroundMode_ ==
         CompositionBackgroundMode::Checkerboard;
}
void CompositionRenderController::setCompositionBackgroundMode(int mode) {
  const auto backgroundMode = static_cast<CompositionBackgroundMode>(mode);
  if (impl_->compositionBackgroundMode_ == backgroundMode) {
    return;
  }
  impl_->compositionBackgroundMode_ = backgroundMode;
  impl_->invalidateBaseComposite();
  renderOneFrame();
}
int CompositionRenderController::compositionBackgroundMode() const {
  return static_cast<int>(impl_->compositionBackgroundMode_);
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

void CompositionRenderController::setShowAnchorCenterOverlay(bool show) {
  if (impl_->showAnchorCenterOverlay_ == show) {
    return;
  }
  impl_->showAnchorCenterOverlay_ = show;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

bool CompositionRenderController::isShowAnchorCenterOverlay() const {
  return impl_ ? impl_->showAnchorCenterOverlay_ : false;
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
  if (impl_->dropGhostVisible_ && impl_->dropGhostRect_ == viewportRect &&
      impl_->dropGhostTitle_ == title && impl_->dropGhostHint_ == hint &&
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

void CompositionRenderController::showCommandPaletteOverlay(
    const QString &query, const QStringList &items) {
  if (!impl_) {
    return;
  }
  impl_->commandPaletteVisible_ = true;
  impl_->commandPaletteQuery_ = query;
  impl_->commandPaletteItems_ = items;
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuItems_.clear();
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::showContextMenuOverlay(
    const QPointF &viewportPos, const QStringList &items) {
  if (!impl_) {
    return;
  }
  impl_->contextMenuVisible_ = true;
  impl_->contextMenuViewportPos_ = viewportPos;
  impl_->contextMenuItems_ = items;
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteItems_.clear();
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::showPieMenuOverlay(
    const PieMenuModel &model, const QPointF &viewportPos) {
  if (!impl_) {
    return;
  }
  impl_->pieMenuVisible_ = true;
  impl_->pieMenuModel_ = model;
  impl_->pieMenuViewportPos_ = viewportPos;
  impl_->pieMenuMousePos_ = viewportPos;
  impl_->pieMenuSelectedIndex_ = impl_->pieMenuItemAt(viewportPos);
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteItems_.clear();
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuItems_.clear();
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

void CompositionRenderController::hideViewportOverlay() {
  if (!impl_ ||
      (!impl_->commandPaletteVisible_ && !impl_->contextMenuVisible_ &&
       !impl_->pieMenuVisible_)) {
    return;
  }
  impl_->commandPaletteVisible_ = false;
  impl_->commandPaletteQuery_.clear();
  impl_->commandPaletteItems_.clear();
  impl_->contextMenuVisible_ = false;
  impl_->contextMenuItems_.clear();
  impl_->pieMenuVisible_ = false;
  impl_->pieMenuModel_ = PieMenuModel{};
  impl_->pieMenuSelectedIndex_ = -1;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

bool CompositionRenderController::isViewportOverlayVisible() const {
  return impl_ && (impl_->commandPaletteVisible_ || impl_->contextMenuVisible_ ||
                   impl_->pieMenuVisible_);
}

int CompositionRenderController::viewportOverlayItemAt(
    const QPointF &viewportPos) const {
  return impl_ ? impl_->viewportOverlayItemAt(viewportPos) : -1;
}

QString CompositionRenderController::confirmPieMenuOverlaySelection() {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return QString();
  }
  const int index = impl_->pieMenuSelectedIndex_;
  QString commandId;
  if (index >= 0 &&
      index < static_cast<int>(impl_->pieMenuModel_.items.size())) {
    auto &item = impl_->pieMenuModel_.items[static_cast<size_t>(index)];
    if (item.enabled) {
      commandId = item.commandId;
      if (item.action) {
        item.action();
      }
    }
  }
  hideViewportOverlay();
  return commandId;
}

void CompositionRenderController::updatePieMenuOverlayMousePos(
    const QPointF &viewportPos) {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return;
  }
  impl_->pieMenuMousePos_ = viewportPos;
  const int selected = impl_->pieMenuItemAt(viewportPos);
  if (selected != impl_->pieMenuSelectedIndex_) {
    impl_->pieMenuSelectedIndex_ = selected;
    impl_->invalidateOverlayComposite();
    renderOneFrame();
  }
}

void CompositionRenderController::cancelPieMenuOverlay() {
  if (!impl_ || !impl_->pieMenuVisible_) {
    return;
  }
  hideViewportOverlay();
}

bool CompositionRenderController::isPieMenuOverlayVisible() const {
  return impl_ && impl_->pieMenuVisible_;
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
         (float)viewportPos.y() * impl_->devicePixelRatio_},
        newZoom);
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
         (float)viewportPos.y() * impl_->devicePixelRatio_},
        newZoom);
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
    const float panX = (impl_->hostWidth_ - impl_->lastCanvasWidth_) * 0.5f;
    const float panY = (impl_->hostHeight_ - impl_->lastCanvasHeight_) * 0.5f;
    impl_->renderer_->setPan(panX, panY);
    impl_->invalidateBaseComposite();
    renderOneFrame();
  }
}

ArtifactIRenderer *CompositionRenderController::renderer() const {
  return impl_->renderer_.get();
}

ArtifactCore::FrameDebugSnapshot
CompositionRenderController::frameDebugSnapshot() const {
  struct TraceScopeGuard {
    ArtifactCore::TraceScopeRecord scope;
    QElapsedTimer timer;
    TraceScopeGuard() {
      scope.name = QStringLiteral("CompositionRenderController::frameDebugSnapshot");
      scope.domain = ArtifactCore::TraceDomain::Render;
      timer.start();
    }
    ~TraceScopeGuard() {
      scope.endNs = timer.nsecsElapsed();
      if (scope.endNs <= scope.startNs) {
        scope.endNs = scope.startNs + 1;
      }
      ArtifactCore::TraceRecorder::instance().recordScope(scope);
    }
  } traceGuard;

  ArtifactCore::FrameDebugSnapshot snapshot;
  const auto comp = impl_->previewPipeline_.composition();
  const auto selectedLayerId = impl_->selectedLayerId_;
  auto *playback = ArtifactPlaybackService::instance();
  auto *queue = ArtifactRenderQueueService::instance();

  if (playback) {
    const auto playbackComp = playback->currentComposition();
    if (!playbackComp || !comp || playbackComp->id() == comp->id()) {
      snapshot.frame = playback->currentFrame();
      snapshot.playbackState = playbackStateToString(playback->state());
    } else {
      snapshot.frame = comp ? comp->framePosition() : FramePosition(0);
      snapshot.playbackState = QStringLiteral("stopped");
    }
  } else {
    snapshot.frame = comp ? comp->framePosition() : FramePosition(0);
    snapshot.playbackState = QStringLiteral("stopped");
  }
  snapshot.renderLastFrameMs = lastFrameTimeMs();
  snapshot.renderAverageFrameMs = averageFrameTimeMs();
  if (impl_->renderer_) {
    snapshot.renderGpuFrameMs = impl_->renderer_->lastFrameGpuTimeMs();
    snapshot.renderCost = impl_->renderer_->frameCostStats();
  }

  snapshot.timestampMs =
      static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count());

  if (comp) {
    snapshot.compositionName = comp->settings().compositionName().toQString();
  } else {
    snapshot.compositionName = QStringLiteral("<none>");
  }

  snapshot.selectedLayerName = QStringLiteral("<none>");
  if (comp && !selectedLayerId.isNil()) {
    if (auto layer = comp->layerById(selectedLayerId)) {
      snapshot.selectedLayerName = layer->layerName();
    }
  }

  snapshot.renderBackend = QStringLiteral("unknown");
  if (queue) {
    snapshot.renderBackend = renderBackendToString(queue->renderBackend());
    const int jobCount = queue->jobCount();
    for (int i = 0; i < jobCount; ++i) {
      const QString status = queue->jobStatusAt(i);
      if (status.contains(QStringLiteral("fail"), Qt::CaseInsensitive) ||
          status.contains(QStringLiteral("error"), Qt::CaseInsensitive)) {
        snapshot.failed = true;
        snapshot.failureReason = queue->jobErrorMessageAt(i);
        break;
      }
    }
  }

  snapshot.compareMode = ArtifactCore::FrameDebugCompareMode::Disabled;
  snapshot.compareTargetId = QString();

  if (playback) {
    ArtifactCore::FrameDebugResourceRecord playbackResource;
    playbackResource.label = QStringLiteral("Playback");
    playbackResource.type = QStringLiteral("timeline");
    playbackResource.relation = QStringLiteral("service");
    playbackResource.cacheHit = playback->droppedFrameCount() == 0;
    playbackResource.stale = false;
    playbackResource.note =
        QStringLiteral("state=%1 droppedFrames=%2")
            .arg(snapshot.playbackState)
            .arg(playback->droppedFrameCount());
    snapshot.resources.push_back(playbackResource);
  }

  if (!impl_->lastRenderPathSummary_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord renderPathResource;
    renderPathResource.label = QStringLiteral("Render Path");
    renderPathResource.type = QStringLiteral("composition");
    renderPathResource.relation = QStringLiteral("path");
    renderPathResource.cacheHit = impl_->blendPipelineReady_;
    renderPathResource.stale = false;
    renderPathResource.note = impl_->lastRenderPathSummary_;
    snapshot.resources.push_back(renderPathResource);
  }

  if (!impl_->lastVideoDebug_.isEmpty()) {
    ArtifactCore::FrameDebugResourceRecord videoResource;
    videoResource.label = QStringLiteral("Video Decode");
    videoResource.type = QStringLiteral("video");
    videoResource.relation = QStringLiteral("decode");
    videoResource.cacheHit =
        !impl_->lastVideoDebug_.contains(QStringLiteral("syncFallback=miss"));
    videoResource.stale =
        impl_->lastVideoDebug_.contains(QStringLiteral("decoding=true"));
    videoResource.note = impl_->lastVideoDebug_;
    snapshot.resources.push_back(videoResource);
  }

  if (impl_->gpuTextureCacheManager_) {
    const auto stats = impl_->gpuTextureCacheManager_->stats();
    ArtifactCore::FrameDebugResourceRecord textureCacheResource;
    textureCacheResource.label = QStringLiteral("GPU Texture Cache");
    textureCacheResource.type = QStringLiteral("cache");
    textureCacheResource.relation = QStringLiteral("upload");
    textureCacheResource.cacheHit = stats.hitCount >= stats.missCount;
    textureCacheResource.stale = false;
    textureCacheResource.note =
        QStringLiteral("entries=%1 bytes=%2 hits=%3 misses=%4")
            .arg(stats.entryCount)
            .arg(static_cast<qulonglong>(stats.memoryBytes))
            .arg(static_cast<qulonglong>(stats.hitCount))
            .arg(static_cast<qulonglong>(stats.missCount));
    snapshot.resources.push_back(textureCacheResource);
  }

  if (comp) {
    auto makePass = [&](const QString &name, ArtifactCore::FrameDebugPassKind kind,
                        qint64 durationMs, const QString &note = QString()) {
      ArtifactCore::FrameDebugPassRecord pass;
      pass.name = name;
      pass.kind = kind;
      pass.status =
          snapshot.failed ? ArtifactCore::FrameDebugPassStatus::Failed
                          : ArtifactCore::FrameDebugPassStatus::Success;
      pass.durationUs = std::max<qint64>(0, durationMs) * 1000;
      pass.note = note;
      return pass;
    };

    ArtifactCore::FrameDebugPassRecord setupPass =
        makePass(QStringLiteral("setup"), ArtifactCore::FrameDebugPassKind::Clear,
                 impl_->lastSetupMs_);
    ArtifactCore::FrameDebugPassRecord basePass =
        makePass(QStringLiteral("base"), ArtifactCore::FrameDebugPassKind::Clear,
                 impl_->lastBasePassMs_);
    ArtifactCore::FrameDebugPassRecord layerPass =
        makePass(QStringLiteral("layer"), ArtifactCore::FrameDebugPassKind::Draw,
                 impl_->lastLayerPassMs_, impl_->lastRenderPathSummary_);
    ArtifactCore::FrameDebugPassRecord overlayPass =
        makePass(QStringLiteral("overlay"),
                 ArtifactCore::FrameDebugPassKind::Composite,
                 impl_->lastOverlayMs_);
    ArtifactCore::FrameDebugPassRecord flushPass =
        makePass(QStringLiteral("flush"),
                 ArtifactCore::FrameDebugPassKind::Resolve,
                 impl_->lastFlushMs_);
    ArtifactCore::FrameDebugPassRecord presentPass =
        makePass(QStringLiteral("present"),
                 ArtifactCore::FrameDebugPassKind::Readback,
                 impl_->lastPresentMs_);

    ArtifactCore::FrameDebugAttachmentRecord outputAttachment;
    outputAttachment.name = QStringLiteral("viewport");
    outputAttachment.role = QStringLiteral("output");
    outputAttachment.texture.valid = true;
    outputAttachment.texture.name = QStringLiteral("viewport");
    outputAttachment.texture.format = snapshot.renderBackend.isEmpty() ? QStringLiteral("viewport") : snapshot.renderBackend;
    outputAttachment.texture.width = std::max(1, static_cast<int>(std::lround(impl_->hostWidth_)));
    outputAttachment.texture.height = std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)));
    outputAttachment.texture.mipLevel = 0;
    outputAttachment.texture.mipLevels = std::max(1, 1 + static_cast<int>(std::floor(std::log2(
        static_cast<double>(std::max(outputAttachment.texture.width, outputAttachment.texture.height))))));
    outputAttachment.texture.sliceIndex = 0;
    outputAttachment.texture.arrayLayers = 1;
    outputAttachment.texture.sampleCount = 1;
    outputAttachment.texture.srgb = false;
    outputAttachment.readOnly = true;
    presentPass.outputs.push_back(outputAttachment);

    snapshot.attachments.push_back(outputAttachment);

    if (!snapshot.selectedLayerName.isEmpty() &&
        snapshot.selectedLayerName != QStringLiteral("<none>")) {
      ArtifactCore::FrameDebugResourceRecord selectedResource;
      selectedResource.label = snapshot.selectedLayerName;
      selectedResource.type = QStringLiteral("layer");
      selectedResource.relation = QStringLiteral("selected");
      selectedResource.cacheHit = true;
      selectedResource.texture.valid = true;
      selectedResource.texture.name = snapshot.selectedLayerName;
      selectedResource.texture.format = QStringLiteral("layer-proxy");
      selectedResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));
      selectedResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));
      selectedResource.texture.mipLevel = 0;
      selectedResource.texture.mipLevels = std::max(1, 1 + static_cast<int>(std::floor(std::log2(
          static_cast<double>(std::max(selectedResource.texture.width, selectedResource.texture.height))))));
      selectedResource.texture.sliceIndex = 0;
      selectedResource.texture.arrayLayers = 1;
      selectedResource.texture.sampleCount = 1;
      selectedResource.texture.srgb = false;
      selectedResource.note = impl_->lastVideoDebug_;
      snapshot.resources.push_back(selectedResource);
    }

    snapshot.passes.push_back(setupPass);
    snapshot.passes.push_back(basePass);
    snapshot.passes.push_back(layerPass);
    snapshot.passes.push_back(overlayPass);
    snapshot.passes.push_back(flushPass);
    snapshot.passes.push_back(presentPass);
  }

  ArtifactCore::TraceRecorder::instance().recordFrameDebugSnapshot(snapshot);
  return snapshot;
}

double CompositionRenderController::lastFrameTimeMs() const {
  return impl_ ? impl_->lastFrameTimeMs_ : 0.0;
}

double CompositionRenderController::averageFrameTimeMs() const {
  return impl_ ? impl_->averageFrameTimeMs_ : 0.0;
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

  qCDebug(compositionViewLog)
      << "[MousePress] ENTER pos:" << event->position()
      << "button:" << event->button() << "modifiers:" << event->modifiers()
      << "devicePixelRatio:" << impl_->devicePixelRatio_;

  // event->position() is in logical pixels; convert to physical for rendering
  // pipeline
  const QPointF viewportPos = event->position() * impl_->devicePixelRatio_;

  // 3D Gizmo hit test (GIZ-2) — only for 3D layers
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  if (selectedLayer && impl_->gizmo3D_ && selectedLayer->is3D()) {
    impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
    Ray ray = createPickingRay(viewportPos);
    GizmoAxis axis =
        impl_->gizmo3D_->hitTest(ray, impl_->renderer_->getViewMatrix(),
                                 impl_->renderer_->getProjectionMatrix());
    if (axis != GizmoAxis::None) {
      impl_->gizmo3D_->beginDrag(axis, ray);
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->invalidateOverlayComposite();
      renderOneFrame();
      return;
    }
  }

  // Gizmo hit test first (2D)
  if (impl_->gizmo_) {
    impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());
    if (impl_->gizmo_->isDragging()) {
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->gizmoDragRenderTimer_.restart();
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

          if (impl_->pendingMaskCreation_ &&
              impl_->pendingMaskLayerId_ != selectedLayer->id()) {
            impl_->clearPendingMaskCreation();
          }

          const float handleThreshold = 10.0f / impl_->renderer_->getZoom();
          int handleMaskIndex = -1;
          int handlePathIndex = -1;
          int handleVertexIndex = -1;
          MaskHandleType handleType = MaskHandleType::None;
          if (hitTestMaskHandle(selectedLayer, QPointF(cPos.x, cPos.y), handleThreshold,
                                handleMaskIndex, handlePathIndex, handleVertexIndex,
                                handleType)) {
            impl_->isDraggingVertex_ = false;
            impl_->isDraggingMaskHandle_ = true;
            impl_->draggingMaskIndex_ = handleMaskIndex;
            impl_->draggingPathIndex_ = handlePathIndex;
            impl_->draggingVertexIndex_ = handleVertexIndex;
            impl_->draggingMaskHandleType_ = static_cast<int>(handleType);
            impl_->hoveredMaskIndex_ = handleMaskIndex;
            impl_->hoveredPathIndex_ = handlePathIndex;
            impl_->hoveredVertexIndex_ = handleVertexIndex;
            impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
            return;
          }

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
                    ArtifactCore::globalEventBus().publish(LayerChangedEvent{
                        comp->id().toString(), selectedLayer->id().toString(),
                        LayerChangedEvent::ChangeType::Modified});
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

          // 2. Build a pending mask path first. We only materialize a
          //    LayerMask once the path is ready to close, so the first click
          //    does not immediately alter the layer's mask stack.
          if (impl_->pendingMaskCreation_ &&
              impl_->pendingMaskLayerId_ == selectedLayer->id() &&
              impl_->pendingMaskPath_.vertexCount() >= 3) {
            const float closeThreshold =
                8.0f / impl_->renderer_->getZoom();
            const MaskVertex firstVertex = impl_->pendingMaskPath_.vertex(0);
            if (QVector2D(firstVertex.position - localPos).length() <
                closeThreshold) {
              if (impl_->finalizePendingMaskCreation(selectedLayer)) {
                impl_->markMaskEditDirty();
                qDebug() << "[PenTool] Finalized pending mask path"
                         << "layer:" << selectedLayer->id().toString();
                ArtifactCore::globalEventBus().publish(LayerChangedEvent{
                    comp->id().toString(), selectedLayer->id().toString(),
                    LayerChangedEvent::ChangeType::Modified});
              }
              return;
            }
          }

          if (!impl_->pendingMaskCreation_ ||
              impl_->pendingMaskLayerId_ != selectedLayer->id()) {
            impl_->beginPendingMaskCreation(selectedLayer, localPos);
          } else {
            impl_->beginPendingMaskCreation(selectedLayer, localPos);
          }

          qDebug() << "[PenTool] Added pending mask vertex at local:"
                   << localPos << "layer:" << selectedLayer->id().toString()
                   << "pendingVertices:"
                   << impl_->pendingMaskPath_.vertexCount();
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
        if (!isLayerEffectivelyVisible(layer))
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
            // If svc failed to update the selection manager (e.g., its
            // currentComposition doesn't match the render controller's
            // composition), it may have called clearSelection(). Fix this
            // directly so the deferred syncSelectionState() won't nullify the
            // gizmo on the next tick.
            if (selection && !selection->currentLayer()) {
              selection->selectLayer(hitLayerRef);
            }
          }
        }

        // Use hitLayer as the fallback: if the selection manager still has no
        // current layer (composition mismatch, etc.), don't override with null.
        ArtifactAbstractLayerPtr primaryLayer = hitLayer;
        if (selection) {
          if (auto current = selection->currentLayer()) {
            primaryLayer = current;
          }
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

void CompositionRenderController::handleMouseMove(
    const QPointF &viewportPosLogical) {
  qCDebug(compositionViewLog)
      << "[MouseMove] ENTER logicalPos:" << viewportPosLogical
      << "devicePixelRatio:" << impl_->devicePixelRatio_;

  // Convert logical pixels (from Qt event) to physical pixels for the rendering
  // pipeline
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

          ArtifactCore::globalEventBus().publish(LayerChangedEvent{
              comp->id().toString(), selectedLayer->id().toString(),
              LayerChangedEvent::ChangeType::Modified});
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
    const int prevHoveredMaskHandleType = impl_->hoveredMaskHandleType_;
    impl_->hoveredMaskIndex_ = -1;
    impl_->hoveredPathIndex_ = -1;
    impl_->hoveredVertexIndex_ = -1;
    impl_->hoveredMaskHandleType_ = -1;

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

          int handleMaskIndex = -1;
          int handlePathIndex = -1;
          int handleVertexIndex = -1;
          MaskHandleType handleType = MaskHandleType::None;
          if (hitTestMaskHandle(selectedLayer, QPointF(cPos.x, cPos.y),
                                hitThreshold, handleMaskIndex, handlePathIndex,
                                handleVertexIndex, handleType)) {
            impl_->hoveredMaskIndex_ = handleMaskIndex;
            impl_->hoveredPathIndex_ = handlePathIndex;
            impl_->hoveredVertexIndex_ = handleVertexIndex;
            impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
          } else {
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
    }
    if (prevHoveredMaskIndex != impl_->hoveredMaskIndex_ ||
        prevHoveredPathIndex != impl_->hoveredPathIndex_ ||
        prevHoveredVertexIndex != impl_->hoveredVertexIndex_ ||
        prevHoveredMaskHandleType != impl_->hoveredMaskHandleType_) {
      impl_->invalidateOverlayComposite();
      needsRender = true;
    }
  }

  // 3D Gizmo interaction (GIZ-2, GIZ-3) — only for 3D layers
  if (impl_->gizmo3D_) {
    auto comp3D = impl_->previewPipeline_.composition();
    auto sel3DLayer = (!impl_->selectedLayerId_.isNil() && comp3D)
                          ? comp3D->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
    if (sel3DLayer && sel3DLayer->is3D()) {
      Ray ray = createPickingRay(viewportPos);
      if (impl_->gizmo3D_->isDragging()) {
        notifyViewportInteractionActivity();
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
            ArtifactCore::globalEventBus().publish(
                LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
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
  }

  if (impl_->gizmo_) {
    impl_->gizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
    if (impl_->gizmo_->isDragging()) {
      notifyViewportInteractionActivity();
      // Phase 3: Use fixed-rate render tick instead of 33ms throttle + renderOneFrame().
      markRenderDirty();
      return;
    }
  }

  if (needsRender) {
    markRenderDirty();
  }
}

void CompositionRenderController::handleMouseRelease() {
  qCDebug(compositionViewLog) << "[MouseRelease] ENTER";

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
  impl_->isDraggingMaskHandle_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;
  impl_->draggingMaskHandleType_ = -1;
  impl_->commitMaskEditTransaction();

  if (impl_->gizmo3D_) {
    const bool wasDragging = impl_->gizmoDragActive_;
    impl_->gizmoDragActive_ = false;
    impl_->gizmo3D_->endDrag();
    impl_->invalidateOverlayComposite();
    if (wasDragging) {
      finishViewportInteraction();
    }
    renderOneFrame();
  }

  if (impl_->gizmo_) {
    const bool wasDragging = impl_->gizmoDragActive_;
    impl_->gizmoDragActive_ = false;
    impl_->gizmo_->handleMouseRelease();
    impl_->invalidateOverlayComposite();
    if (wasDragging) {
      finishViewportInteraction();
    }
    renderOneFrame();
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

void CompositionRenderController::setViewportOrientation(
    ArtifactCore::ViewOrientationHotspot hotspot) {
  if (!impl_) {
    return;
  }
  if (impl_->viewportOrientationActive_ &&
      impl_->viewportOrientationNavigator_.activeHotspot() == hotspot) {
    return;
  }
  impl_->viewportOrientationNavigator_.snapTo(hotspot, true);
  impl_->viewportOrientationActive_ = true;
  impl_->invalidateOverlayComposite();
  renderOneFrame();
}

ArtifactCore::ViewOrientationHotspot
CompositionRenderController::viewportOrientation() const {
  if (!impl_) {
    return ArtifactCore::ViewOrientationHotspot::Front;
  }
  return impl_->viewportOrientationNavigator_.activeHotspot();
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
    return;
  }
  if (!impl_->running_) {
    return;
  }
  if (auto *host = impl_->hostWidget_.data()) {
    if (!host->isVisible()) {
      return;
    }
  }
  // Re-entrancy guard: renderOneFrameImpl must not be called recursively.
  if (impl_->renderScheduled_) {
    return;
  }
  impl_->renderScheduled_ = true;
  impl_->renderOneFrameImpl(this);
  impl_->renderScheduled_ = false;
}

void CompositionRenderController::markRenderDirty() {
  impl_->renderDirty_.store(true, std::memory_order_release);
}

void CompositionRenderController::Impl::clearPendingMaskCreation() {
  pendingMaskCreation_ = false;
  pendingMaskLayerId_ = LayerID();
  pendingMaskPath_.clearVertices();
  pendingMaskPath_.setClosed(false);
}

void CompositionRenderController::Impl::beginPendingMaskCreation(
    const ArtifactAbstractLayerPtr &layer, const QPointF &localPos) {
  if (!layer) {
    return;
  }

  if (!pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {
    clearPendingMaskCreation();
    pendingMaskCreation_ = true;
    pendingMaskLayerId_ = layer->id();
  }

  MaskVertex vertex;
  vertex.position = localPos;
  vertex.inTangent = QPointF(0, 0);
  vertex.outTangent = QPointF(0, 0);
  pendingMaskPath_.addVertex(vertex);
}

bool CompositionRenderController::Impl::finalizePendingMaskCreation(
    const ArtifactAbstractLayerPtr &layer) {
  if (!layer || !pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {
    return false;
  }

  if (pendingMaskPath_.vertexCount() < 3) {
    return false;
  }

  MaskPath path = pendingMaskPath_;
  path.setClosed(true);

  beginMaskEditTransaction(layer);

  LayerMask newMask;
  newMask.addMaskPath(path);
  layer->addMask(newMask);
  markMaskEditDirty();

  clearPendingMaskCreation();
  return true;
}

void CompositionRenderController::Impl::renderOneFrameImpl(
    CompositionRenderController *owner) {
  if (!owner || !initialized_ || !renderer_ || !running_) {
    return;
  }
  // Swapchain may not exist yet (deferred from 0×0 init).
  // Skip rendering — the first resize will create the swapchain and
  // trigger a new frame via the debounce timer.
  if (!renderer_->hasSwapChain()) {
    return;
  }

  // 変更検出器のデバッグログ (カテゴリ制御)
  static int renderCount = 0;
  if (renderCount++ % 60 == 0) { // 2秒に1回
    qCDebug(compositionViewLog)
        << "[CompositionChangeDetector]" << changeDetector_.debugInfo();
  }
  if (auto *host = hostWidget_.data()) {
    if (!host->isVisible()) {
      return;
    }
  }

  struct RenderCostCaptureGuard {
    ArtifactIRenderer* renderer = nullptr;
    explicit RenderCostCaptureGuard(ArtifactIRenderer* r) : renderer(r) {
      if (renderer) {
        renderer->beginFrameCostCapture();
      }
    }
    ~RenderCostCaptureGuard() {
      if (renderer) {
        renderer->endFrameCostCapture();
      }
    }
  } renderCostGuard(renderer_.get());

  // 強制的なサイズ同期:
  // ホストウィジェットの物理サイズとスワップチェーンを一致させる
  if (auto *host = hostWidget_.data()) {
    const float curW = static_cast<float>(host->width()) * devicePixelRatio_;
    const float curH = static_cast<float>(host->height()) * devicePixelRatio_;
    if (std::abs(curW - hostWidth_) > 0.5f ||
        std::abs(curH - hostHeight_) > 0.5f) {
      qCDebug(compositionViewLog)
          << "[CompositionView] Widget size changed, scheduling swapchain "
             "update:"
          << curW << "x" << curH;
      // Store logical pixels — setViewportSize applies DPR internally
      pendingResizeSize_ = QSize(host->width(), host->height());
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

  struct TraceScopeGuard {
    ArtifactCore::TraceScopeRecord scope;
    QElapsedTimer timer;
    TraceScopeGuard() {
      scope.name = QStringLiteral("CompositionRenderController::renderOneFrameImpl");
      scope.domain = ArtifactCore::TraceDomain::Render;
      scope.startNs = 0;
      timer.start();
    }
    ~TraceScopeGuard() {
      scope.endNs = timer.nsecsElapsed();
      if (scope.endNs <= scope.startNs) {
        scope.endNs = scope.startNs + 1;
      }
      ArtifactCore::TraceRecorder::instance().recordScope(scope);
    }
  } traceGuard;

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

  // --- Profiler frame begin ---
  {
    bool isPlayback = false;
    if (auto *pb = ArtifactPlaybackService::instance())
      isPlayback = pb->isPlaying();
    ArtifactCore::Profiler::instance().beginFrame(
        static_cast<std::int64_t>(renderFrameCounter_), compSize.width(),
        compSize.height(), isPlayback);
  }

  // Update MayaGradient sprite cache once; it is viewport background art and
  // should not be tinted by the composition fill color.
  if (!floatColorEquals(cachedMayaGradientBgColor_, bgColor)) {
    cachedMayaGradientBgColor_ = bgColor;
    cachedMayaGradientSprite_ = QImage();
  }
  if (compositionBackgroundMode_ == CompositionBackgroundMode::MayaGradient &&
      cachedMayaGradientSprite_.isNull() && !cachedMayaGradientWarmupQueued_ &&
      !cachedMayaGradientWarmupFuture_.isRunning()) {
    cachedMayaGradientWarmupQueued_ = true;
    cachedMayaGradientWarmupBgColor_ = bgColor;
    cachedMayaGradientWarmupFuture_ =
        QtConcurrent::run(&sharedBackgroundThreadPool(), [bgColor]() {
          return makeMayaGradientSprite(bgColor);
        });
  }
  if (cachedMayaGradientWarmupQueued_ &&
      cachedMayaGradientWarmupFuture_.isFinished()) {
    QImage warmed = cachedMayaGradientWarmupFuture_.result();
    cachedMayaGradientWarmupQueued_ = false;
    if (floatColorEquals(cachedMayaGradientWarmupBgColor_,
                         cachedMayaGradientBgColor_)) {
      cachedMayaGradientSprite_ = std::move(warmed);
    }
  }
  if (compositionBackgroundMode_ == CompositionBackgroundMode::MayaGradient) {
    const QString mayaDebugState =
        QStringLiteral(
            "mode=MayaGradient spriteNull=%1 spriteSize=%2x%3 bg=%4,%5,%6,%7")
            .arg(cachedMayaGradientSprite_.isNull() ? 1 : 0)
            .arg(cachedMayaGradientSprite_.width())
            .arg(cachedMayaGradientSprite_.height())
            .arg(bgColor.r(), 0, 'f', 3)
            .arg(bgColor.g(), 0, 'f', 3)
            .arg(bgColor.b(), 0, 'f', 3)
            .arg(bgColor.a(), 0, 'f', 3);
    if (mayaDebugState != lastMayaGradientDebugState_) {
      lastMayaGradientDebugState_ = mayaDebugState;
      qCDebug(compositionViewLog)
          << "[CompositionView][Background]" << mayaDebugState;
    }
  } else if (!lastMayaGradientDebugState_.isEmpty()) {
    lastMayaGradientDebugState_.clear();
  }

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

  // Build matte source resolver: find layer by ID, render to QImage
  auto matteResolverLambda = [&layers](const ArtifactCore::Id &layerId) -> QImage {
      for (const auto &l : layers) {
          if (l && l->id() == layerId) {
              if (auto *imgLayer = dynamic_cast<ArtifactImageLayer *>(l.get())) {
                  return imgLayer->toQImage();
              }
              if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(l.get())) {
                  return videoLayer->currentFrameToQImage();
              }
              if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(l.get())) {
                  return textLayer->toQImage();
              }
              if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(l.get())) {
                  return svgLayer->toQImage();
              }
          }
      }
      return {};
  };
  std::function<QImage(const ArtifactCore::Id &)> matteResolver = matteResolverLambda;

  // Find active camera layer for 3D rendering
  ArtifactCameraLayer *activeCamera = nullptr;
  for (const auto &l : layers) {
    auto layerCopy = l;
    if (auto cam = dynamic_cast<ArtifactCameraLayer *>(layerCopy.get())) {
      if (isLayerEffectivelyVisible(layerCopy) && cam->isActiveAt(currentFrame)) {
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
  const QRectF visibleCanvasRect = viewportRectToCanvasRect(
      renderer_.get(), QPointF(0.0f, 0.0f), QPointF(viewportW, viewportH));
  const float roiPad = std::max(48.0f, 64.0f / std::max(0.001f, zoom));
  const QRectF roiRect =
      visibleCanvasRect.adjusted(-roiPad, -roiPad, roiPad, roiPad);
  // --- Change detection via packed struct instead of string concatenation ---
  const auto currentBgColor = comp->backgroundColor();
  const bool bgChanged = currentBgColor.r() != lastBgColorCache_.r() ||
                         currentBgColor.g() != lastBgColorCache_.g() ||
                         currentBgColor.b() != lastBgColorCache_.b() ||
                         currentBgColor.a() != lastBgColorCache_.a();
  if (bgChanged || lastBackgroundCompositionId_ != comp->id()) {
    lastBgColorCache_ = currentBgColor;
    lastBackgroundCompositionId_ = comp->id();
    qCDebug(compositionViewLog) << "[CompositionView][Background]"
                                << "compositionId=" << comp->id().toString();
  }
  const auto backgroundMode = compositionBackgroundMode_;
  const FloatColor layerBgColor = currentBgColor;
  const Impl::RenderKeyState currentKey{
      comp->id(),
      baseInvalidationSerial_,
      overlayInvalidationSerial_,
      framePos,
      static_cast<int32_t>(viewportW),
      static_cast<int32_t>(viewportH),
      effectivePreviewDownsample,
      zoom,
      panX,
      panY,
      currentBgColor.r(),
      currentBgColor.g(),
      currentBgColor.b(),
      currentBgColor.a(),
      static_cast<int32_t>(backgroundMode),
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->mode()) : -1,
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->hoverAxis()) : -1,
      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->activeAxis()) : -1,
      static_cast<uint8_t>(gpuBlendEnabled_ ? 1 : 0),
      static_cast<uint8_t>(showGrid_ ? 1 : 0),
      static_cast<uint8_t>(showGuides_ ? 1 : 0),
      static_cast<uint8_t>(showSafeMargins_ ? 1 : 0),
      static_cast<uint8_t>(showAnchorCenterOverlay_ ? 1 : 0),
      static_cast<uint8_t>(viewportInteracting_ ? 1 : 0),
      selectedLayerId_};
  if (currentKey == lastRenderKeyState_) {
    return;
  }
  lastRenderKeyState_ = currentKey;
  renderer_->setClearColor(viewportClearColor_);
  renderer_->clear();

  {

    const bool hasGpuBlendJustification =
        std::any_of(layers.begin(), layers.end(),
                    [&](const ArtifactAbstractLayerPtr &layer) {
                      if (!isLayerEffectivelyVisible(layer) ||
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
    const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;
    const bool gpuBlendPathRequested =
        gpuBlendRequested && hasGpuBlendJustification;

    // Avoid paying render-pipeline setup cost when GPU blending is disabled.
    if (gpuBlendPathRequested) {
      if (auto device = renderer_->device()) {
        renderPipeline_.initialize(device, static_cast<Uint32>(rcw),
                                   static_cast<Uint32>(rch),
                                   RenderConfig::LinearColorFormat);
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

    ArtifactCore::ProfileScope _profSetup(
        "RenderFrame", ArtifactCore::ProfileCategory::Render);

    // hasSoloLayer: dirty-flag キャッシュで毎フレームの O(N) スキャンを回避
    if (soloLayerCacheDirty_) {
      hasSoloLayerCache_ = std::any_of(
          layers.begin(), layers.end(), [](const ArtifactAbstractLayerPtr &l) {
            return l && l->isVisible() && l->isSolo();
          });
      soloLayerCacheDirty_ = false;
    }
    const bool hasSoloLayer = hasSoloLayerCache_;
    const QStringList selectedIds = selectedLayerIdList();
    const bool hasSelection = !selectedIds.isEmpty();
    constexpr float kGhostOpacityScale = 0.22f;

    if (compositionViewLog().isDebugEnabled()) {
      const ArtifactAbstractLayerPtr overlaySelectedLayer =
          (!selectedLayerId_.isNil() && comp)
              ? comp->layerById(selectedLayerId_)
              : ArtifactAbstractLayerPtr{};
      const int overlayMaskCount =
          overlaySelectedLayer ? overlaySelectedLayer->maskCount() : 0;
      const int overlayActiveHandle =
          gizmo_ ? static_cast<int>(gizmo_->activeHandle()) : -1;
      const QString overlaySummary =
          QStringLiteral("frame=%1 selCount=%2 selectedLayer=%3 gizmo=%4 "
                         "gizmoMode=%5 gizmoDragging=%6 activeHandle=%7 "
                         "motionPath=%8 anchorCenter=%9 masks=%10 region=%11")
              .arg(currentFrame.framePosition())
              .arg(selectedIds.size())
              .arg(selectedLayerId_.isNil() ? QStringLiteral("<none>")
                                            : selectedLayerId_.toString())
              .arg(showGizmoOverlay_ ? 1 : 0)
              .arg(static_cast<int>(gizmoMode_))
              .arg(gizmo_ && gizmo_->isDragging() ? 1 : 0)
              .arg(overlayActiveHandle)
              .arg(showMotionPathOverlay_ ? 1 : 0)
              .arg(showAnchorCenterOverlay_ ? 1 : 0)
              .arg(overlayMaskCount)
              .arg(showCompositionRegionOverlay_ ? 1 : 0);
      if (overlaySummary != lastOverlayDebugSummary_ ||
          (renderFrameCounter_ % 120u) == 0u) {
        lastOverlayDebugSummary_ = overlaySummary;
        qCDebug(compositionViewLog)
            << "[CompositionView][OverlayState]" << overlaySummary;
      }
    }

    // バックバッファ全体をクリア (外側ゴミ表示修正)
    renderer_->clearRenderTarget(viewportClearColor_);

    // ============================================================
    // GPU パイプライン: レイヤー 0 枚でも frameOutOfRange でも常に描画
    // ============================================================
    if (pipelineEnabled) {
      ArtifactCore::ProfileScope _profBase(
          "BasePass", ArtifactCore::ProfileCategory::Composite);
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

      // -- 1: 背景を offscreen 座標系で準備 --
      // 背景矩形は一度 layerRTV に rasterize してから compute blend で
      // accum にシードする。これで accum/temp は Vulkan 互換の
      // linear storage image に保てる。
      renderer_->setViewportSize(rcw, rch);
      renderer_->setCanvasSize(cw, ch);      // ← Composition Space に設定
      renderer_->setZoom(origZoom);          // ← 現在のカメラズームを適用
      renderer_->setPan(origPanX, origPanY); // ← 現在のカメラパンを適用
      renderer_->setViewportRect(rcw, rch);

      const FloatColor layerBgColor = comp->backgroundColor();
      if (compositionViewLog().isDebugEnabled()) {
        qCDebug(compositionViewLog)
            << "[CompositionView] background pass (gpu)"
            << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))
            << "rtSize=" << QSize(static_cast<int>(rcw), static_cast<int>(rch))
            << "viewport="
            << QSize(static_cast<int>(origViewW), static_cast<int>(origViewH))
            << "zoom=" << origZoom << "pan=" << QPointF(origPanX, origPanY)
            << "bg="
            << QColor::fromRgbF(layerBgColor.r(), layerBgColor.g(),
                                layerBgColor.b(), layerBgColor.a())
            << "bgMode=" << static_cast<int>(backgroundMode)
            << "compositionSpaceApplied=" << true;
      }
      renderer_->setOverrideRTV(renderPipeline_.accumRTV());
        renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
        renderer_->clear();
        renderer_->setClearColor(viewportClearColor_);
        renderer_->setOverrideRTV(nullptr);

      // Switch to offscreen-scaled coordinate system so the background aligns
      // with the layer renders (both use offscreenScale-adjusted zoom/pan).
      renderer_->setCanvasSize(rcw, rch);
      renderer_->setZoom(origZoom * offscreenScale);
      renderer_->setPan(origPanX * offscreenScale, origPanY * offscreenScale);

      // Seed accum through the same graphics->compute path used by layers.
      // This keeps compute intermediates in a linear, storage-compatible format
      // while the raster pass still targets the regular sRGB layer RT.
      if (backgroundMode == CompositionBackgroundMode::Solid ||
          backgroundMode == CompositionBackgroundMode::MayaGradient ||
          backgroundMode == CompositionBackgroundMode::Checkerboard) {
        renderer_->setOverrideRTV(layerRTV);
        renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
        renderer_->clear();
        renderer_->setClearColor(viewportClearColor_);
        renderer_->drawRectLocal(0.f, 0.f, cw, ch, bgColor, 1.0f);
        renderer_->flush();
        renderer_->setOverrideRTV(nullptr);
        renderer_->unbindColorTargetsForCompute();
        const bool seeded = renderer_->blendLayers(
            blendPipeline_.get(), layerSRV, accumSRV, tempUAV,
            ArtifactCore::BlendMode::Normal, 1.0f);
        if (seeded) {
          renderPipeline_.swapAccumAndTemp();
          accumSRV = renderPipeline_.accumSRV();
          tempUAV = renderPipeline_.tempUAV();
        }
      }
      basePassMs = markPhaseMs();

      // Already in offscreen-scale coordinate system — layer loop uses it
      // as-is.

      // -- 2: レイヤーブレンド（frameOutOfRange ならスキップ）--
      ArtifactCore::ProfileScope _profLayer(
          "LayerPass", ArtifactCore::ProfileCategory::Composite);
      if (!frameOutOfRange) {
        const DetailLevel lod = detailLevelFromZoom(renderer_->getZoom());
        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(
            lod)); // Pass LOD to renderer/effects
        for (const auto &layer : layers) {
          if (!isLayerEffectivelyVisible(layer))
            continue;
          if (hasSoloLayer && !layer->isSolo())
            continue;
          if (!layer->isActiveAt(currentFrame))
            continue;
          const QRectF layerBounds = layer->transformedBoundingBox();
          if (layerBounds.isValid() &&
              layerBounds.intersected(roiRect).isEmpty()) {
            continue;
          }

          // --- Feature 3: Layer Drawing Skip (LOD-based) ---
          // Skip rendering layers that are too small to be visible on screen.
          if (layerBounds.isValid()) {
            const auto tl = renderer_->canvasToViewport(
                {(float)layerBounds.left(), (float)layerBounds.top()});
            const auto br = renderer_->canvasToViewport(
                {(float)layerBounds.right(), (float)layerBounds.bottom()});
            const float screenW = std::abs(br.x - tl.x);
            const float screenH = std::abs(br.y - tl.y);

            if (lod == DetailLevel::Low) {
              if (screenW < 8.0f || screenH < 8.0f)
                continue;
            } else if (lod == DetailLevel::Medium) {
              if (screenW < 2.0f || screenH < 2.0f)
                continue;
            }
          }
          // ------------------------------------------------

          ++drawnLayerCount;
          if (layerUsesSurfaceUploadForCompositionView(layer.get())) {
            ++surfaceUploadLayerCount;
          }
          if (layerHasCpuRasterizerWork(layer.get())) {
            ++cpuRasterLayerCount;
          }

          if (layerNeedsFrameSyncForCompositionView(layer.get())) {
            layer->goToFrame(currentFrame.framePosition());
          }
          const auto blendMode =
              ArtifactCore::toBlendMode(layer->layerBlendType());
          const float opacity =
              layer->opacity() *
              ((hasSelection && !isLayerSelected(selectedIds, layer))
                   ? kGhostOpacityScale
                   : 1.0f);
          if (opacity <= 0.0f) {
            qCDebug(compositionViewLog)
                << "[LayerSkip] opacity <= 0"
                << "layer=" << layer->id().toString()
                << "layerName=" << layer->layerName()
                << "opacity=" << opacity
                << "rawOpacity=" << layer->opacity()
                << "hasSelection=" << hasSelection;
            continue;
          }

          // -- Adjustment Layer or Normal Layer? --
          renderer_->setOverrideRTV(layerRTV);
          if (layer->isAdjustmentLayer()) {
            renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
            renderer_->clear();
            // -- Adjustment Layer: Capture the background --
            // We draw the current composition result (accumSRV) into our layer
            // buffer. This makes the 'background' available as a source for
            // this layer's effects.
            const QSize compSize = comp->settings().compositionSize();
            const float cw = static_cast<float>(compSize.width());
            const float ch = static_cast<float>(compSize.height());
            renderer_->drawSprite(0.0f, 0.0f, cw, ch, accumSRV, 1.0f);
          } else {
            renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
            renderer_->clear();
          }

          QString *dbgOut =
              QLoggingCategory::defaultCategory()->isDebugEnabled()
                  ? &lastVideoDebug_
                  : nullptr;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), 1.0f, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(), true,
              lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr, &matteResolver);
          // Keep the command-buffer architecture, but make the graphics ->
          // compute boundary explicit. The blend pipeline samples layerSRV,
          // so pending draw packets must be submitted before dispatch.
          renderer_->flush();
          renderer_->setOverrideRTV(nullptr);

          // CS 実行前に RTV を解除
          renderer_->unbindColorTargetsForCompute();

          const bool blendOk = renderer_->blendLayers(
              blendPipeline_.get(), layerSRV, accumSRV, tempUAV, blendMode,
              opacity);
          if (!blendOk) {
            continue;
          }
          renderPipeline_.swapAccumAndTemp();
          accumSRV = renderPipeline_.accumSRV();
          tempUAV = renderPipeline_.tempUAV();
        }
      }

      // ==== オフスクリーン描画後: ホスト viewport に戻す ====
      renderer_->setViewportRect(origViewW, origViewH);

      // -- 3: main FB に背景を描画してから accum を blit --
      // MayaGradient と Checkerboard はビューポート全体を覆う背景として main FB
      // に描く。 accum を SRC_ALPHA でブリットすると、composition 外は alpha=0
      // のため main FB
      // の背景（グラディエント/チェッカーボード/ビューポートクリアカラー）が
      // 透けて見える。composition 内は bgColor（不透明）で遮蔽される。
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      if (backgroundMode == CompositionBackgroundMode::MayaGradient) {
        drawViewportMayaGradientBackground(renderer_.get(), origViewW,
                                           origViewH, bgColor,
                                           cachedMayaGradientSprite_);
      } else if (backgroundMode == CompositionBackgroundMode::Checkerboard) {
        drawViewportCheckerboardBackground(renderer_.get(), origViewW,
                                           origViewH);
      }
      renderer_->setCanvasSize(origViewW, origViewH);
      renderer_->setZoom(origZoom);
      renderer_->setPan(origPanX, origPanY);

      // -- 4: オフスクリーン RT を画面全体に描画（スクリーン座標、SRC_ALPHA
      // ブレンド） -- accum には背景色 (Solid/Checkerboard)
      // とレイヤー合成結果が 入っている。コンポジション外は alpha=0 なので
      // SRC_ALPHA ブレンドで main FB のビューポートカラーが透けて見える。
      renderer_->setCanvasSize(origViewW, origViewH);
      renderer_->setZoom(1.0f);
      renderer_->setPan(0.0f, 0.0f);
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
      renderer_->setClearColor(
          origClearColor); // Bug A fix: GPUパス前に保存したクリアカラーを復元
      layerPassMs = markPhaseMs();
    } else {
      // === Fallback path (GPU パイプラインなし) ===
      renderer_->setViewportRect(viewportW, viewportH);
      const float prevZoom = renderer_->getZoom();
      float prevPanX = 0.0f;
      float prevPanY = 0.0f;
      renderer_->getPan(prevPanX, prevPanY);
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      if (backgroundMode == CompositionBackgroundMode::MayaGradient) {
        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        drawViewportMayaGradientBackground(renderer_.get(), viewportW,
                                           viewportH, layerBgColor,
                                           cachedMayaGradientSprite_);
        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      renderer_->setCanvasSize(cw, ch); // キャンバスを Composition Space に設定
      if (backgroundMode == CompositionBackgroundMode::Checkerboard) {
        renderer_->setCanvasSize(viewportW, viewportH);
        renderer_->setZoom(1.0f);
        renderer_->setPan(0.0f, 0.0f);
        drawViewportCheckerboardBackground(renderer_.get(), viewportW,
                                           viewportH);
        renderer_->setCanvasSize(cw, ch);
        renderer_->setZoom(prevZoom);
        renderer_->setPan(prevPanX, prevPanY);
      }
      // Composition Space で直接 fill する（viewport-space 変換不要）
      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,
                                      backgroundMode,
                                      cachedMayaGradientSprite_);
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      renderer_->setZoom(prevZoom);
      renderer_->setPan(prevPanX, prevPanY);
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
        const DetailLevel lod = detailLevelFromZoom(renderer_->getZoom());
        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(
            lod)); // Pass LOD to renderer/effects

        for (const auto &layer : layers) {
          if (!isLayerEffectivelyVisible(layer))
            continue;
          if (hasSoloLayer && !layer->isSolo())
            continue;
          if (!layer->isActiveAt(currentFrame))
            continue;

          // === 段階 2: ROI 計算 ===
          const QRectF layerBounds = layer->transformedBoundingBox();
          const QRectF intersected = layerBounds.intersected(roiRect);

          // --- Feature 3: Layer Drawing Skip (LOD-based) ---
          // Skip rendering layers that are too small to be visible on screen.
          if (layerBounds.isValid()) {
            const auto tl = renderer_->canvasToViewport(
                {(float)layerBounds.left(), (float)layerBounds.top()});
            const auto br = renderer_->canvasToViewport(
                {(float)layerBounds.right(), (float)layerBounds.bottom()});
            const float screenW = std::abs(br.x - tl.x);
            const float screenH = std::abs(br.y - tl.y);

            if (lod == DetailLevel::Low) {
              if (screenW < 8.0f || screenH < 8.0f)
                continue;
            } else if (lod == DetailLevel::Medium) {
              if (screenW < 2.0f || screenH < 2.0f)
                continue;
            }
          }
          // ------------------------------------------------

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
          if (layerNeedsFrameSyncForCompositionView(layer.get())) {
            layer->goToFrame(currentFrame.framePosition());
          }
          const float opacity =
              layer->opacity() *
              ((hasSelection && !isLayerSelected(selectedIds, layer))
                   ? kGhostOpacityScale
                   : 1.0f);
          if (opacity <= 0.0f) {
            continue;
          }
          QString *dbgOut =
              QLoggingCategory::defaultCategory()->isDebugEnabled()
                  ? &lastVideoDebug_
                  : nullptr;
          drawLayerForCompositionView(
              layer.get(), renderer_.get(), opacity, dbgOut, &surfaceCache_,
              gpuTextureCacheManager_.get(), currentFrame.framePosition(),
              false, lod, has3DCamera ? &cameraViewMatrix : nullptr,
              has3DCamera ? &cameraProjMatrix : nullptr, &matteResolver);

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

    // Temporarily disable motion path overlay while debugging stray
    // frame-like rectangles in the viewport.
    // if (renderer_ && showMotionPathOverlay_ && comp &&
    //     !selectedLayerId_.isNil()) {
    //   ArtifactCore::ProfileScope _profMotion1(
    //       "MotionPath1", ArtifactCore::ProfileCategory::Render);
    //   if (auto selectedLayer = comp->layerById(selectedLayerId_)) {
    //     const auto motionPath = buildMotionPathSamples(selectedLayer, comp);
    //     QVector<MotionPathSample> keyframes;
    //     keyframes.reserve(motionPath.size());
    //     const MotionPathSample *currentSample = nullptr;
    //     for (const auto &sample : motionPath) {
    //       if (sample.kind == MotionPathSampleKind::Current) {
    //         currentSample = &sample;
    //       } else {
    //         keyframes.push_back(sample);
    //       }
    //     }
    //
    //     auto samePoint = [](const QPointF &a, const QPointF &b) {
    //       return qFuzzyCompare(a.x(), b.x()) && qFuzzyCompare(a.y(), b.y());
    //     };
    //
    //     const bool currentMatchesKeyframe =
    //         currentSample &&
    //         std::any_of(keyframes.begin(), keyframes.end(),
    //                     [&](const MotionPathSample &sample) {
    //                       return samePoint(sample.position,
    //                                        currentSample->position);
    //                     });
    //
    //     const bool hasMotion = keyframes.size() >= 2 ||
    //                            (currentSample != nullptr &&
    //                             !keyframes.empty() && !currentMatchesKeyframe);
    //
    //     if (hasMotion) {
    //       const FloatColor pathColor{0.95f, 0.65f, 0.22f, 0.85f};
    //       const FloatColor keyColor{1.0f, 0.92f, 0.28f, 1.0f};
    //       const FloatColor currentColor{0.28f, 0.9f, 1.0f, 1.0f};
    //       QPointF prev = keyframes[0].position;
    //       for (int i = 1; i < keyframes.size(); ++i) {
    //         const QPointF cur = keyframes[i].position;
    //         renderer_->drawSolidLine(
    //             {static_cast<float>(prev.x()), static_cast<float>(prev.y())},
    //             {static_cast<float>(cur.x()), static_cast<float>(cur.y())},
    //             pathColor, 1.2f);
    //         prev = cur;
    //       }
    //       for (const auto &sample : keyframes) {
    //         renderer_->drawPoint(static_cast<float>(sample.position.x()),
    //                              static_cast<float>(sample.position.y()), 6.0f,
    //                              keyColor);
    //       }
    //       if (currentSample && !currentMatchesKeyframe) {
    //         renderer_->drawPoint(
    //             static_cast<float>(currentSample->position.x()),
    //             static_cast<float>(currentSample->position.y()), 4.0f,
    //             currentColor);
    //       }
    //     }
    //   }
    // }

    if (showGizmoOverlay_ && gizmo_) {
      ArtifactCore::ProfileScope _profGizmo(
          "GizmoMask", ArtifactCore::ProfileCategory::Render);
      auto selectedLayer = (!selectedLayerId_.isNil() && comp)
                               ? comp->layerById(selectedLayerId_)
                               : ArtifactAbstractLayerPtr{};
      if (selectedLayer && isLayerEffectivelyVisible(selectedLayer)) {
        gizmo_->setMode(gizmoMode_);
        {
          ArtifactCore::ProfileScope _profG2D(
              "Gizmo2D", ArtifactCore::ProfileCategory::Render);
          {
            ArtifactCore::ProfileScope _profG2DSetLayer(
                "Gizmo2DSetLayer", ArtifactCore::ProfileCategory::Render);
            gizmo_->setLayer(selectedLayer);
          }
          {
            ArtifactCore::ProfileScope _profG2DDrawCall(
                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);
            gizmo_->draw(renderer_.get());
          }
        }

        if (gizmo3D_ && selectedLayer->is3D()) {
          ArtifactCore::ProfileScope _profG3D(
              "Gizmo3D", ArtifactCore::ProfileCategory::Render);
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

            {
              ArtifactCore::ProfileScope _profG3DDraw(
                  "Gizmo3DDraw", ArtifactCore::ProfileCategory::Render);
              gizmo3D_->draw(renderer_.get(), view, proj);
            }
            {
              ArtifactCore::ProfileScope _profG3DFlush(
                  "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);
              renderer_->flushGizmo3D();
            }
          } else {
            {
              ArtifactCore::ProfileScope _profG3DDraw(
                  "Gizmo3DDraw", ArtifactCore::ProfileCategory::Render);
              gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(),
                             renderer_->getProjectionMatrix());
            }
            {
              ArtifactCore::ProfileScope _profG3DFlush(
                  "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);
              renderer_->flushGizmo3D();
            }
          }
        }

        // Mask Overlay Drawing
        const int maskCount = selectedLayer->maskCount();
        if (maskCount > 0 && renderer_ &&
            selectedLayer->isActiveAt(currentFrame)) {
          ArtifactCore::ProfileScope _profMask(
              "MaskDraw", ArtifactCore::ProfileCategory::Render);
          const QTransform globalTransform =
              selectedLayer->getGlobalTransform();
          const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
          const FloatColor maskLineColor = {0.26f, 0.84f, 0.96f, 0.96f};
          const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
          const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
          const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
          const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};
          const FloatColor handleLineColor = {0.74f, 0.82f, 0.92f, 0.55f};
          const FloatColor handlePointColor = {0.70f, 0.90f, 1.0f, 0.95f};
          const FloatColor handleHoverColor = {1.0f, 0.78f, 0.32f, 1.0f};
          const FloatColor handleDragColor = {1.0f, 0.44f, 0.24f, 1.0f};

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
              {
                ArtifactCore::ProfileScope _profMaskLines(
                    "MaskDrawLines", ArtifactCore::ProfileCategory::Render);
                for (int v = 0; v < vertexCount; ++v) {
                  MaskVertex vertex = path.vertex(v);
                  QPointF canvasPos = globalTransform.map(vertex.position);
                  Detail::float2 currentCanvasPos = {(float)canvasPos.x(),
                                                     (float)canvasPos.y()};

                  const QPointF inHandlePos = globalTransform.map(vertex.position + vertex.inTangent);
                  const QPointF outHandlePos = globalTransform.map(vertex.position + vertex.outTangent);
                  const Detail::float2 inHandleCanvas = {(float)inHandlePos.x(), (float)inHandlePos.y()};
                  const Detail::float2 outHandleCanvas = {(float)outHandlePos.x(), (float)outHandlePos.y()};

                  if (vertex.inTangent != QPointF(0, 0)) {
                    renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, 4.0f, maskLineShadowColor);
                    renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, 2.0f, handleLineColor);
                    FloatColor handleColor = handlePointColor;
                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
                      handleColor = handleDragColor;
                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                               hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
                      handleColor = handleHoverColor;
                    }
                    renderer_->drawPoint(inHandleCanvas.x, inHandleCanvas.y, 10.0f, maskPointShadowColor);
                    renderer_->drawPoint(inHandleCanvas.x, inHandleCanvas.y, 6.5f, handleColor);
                  }
                  if (vertex.outTangent != QPointF(0, 0)) {
                    renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, 4.0f, maskLineShadowColor);
                    renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, 2.0f, handleLineColor);
                    FloatColor handleColor = handlePointColor;
                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
                      handleColor = handleDragColor;
                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                               hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
                      handleColor = handleHoverColor;
                    }
                    renderer_->drawPoint(outHandleCanvas.x, outHandleCanvas.y, 10.0f, maskPointShadowColor);
                    renderer_->drawPoint(outHandleCanvas.x, outHandleCanvas.y, 6.5f, handleColor);
                  }

                  if (v > 0) {
                    renderer_->drawThickLineLocal(lastCanvasPos,
                                                  currentCanvasPos, 6.0f,
                                                  maskLineShadowColor);
                    renderer_->drawThickLineLocal(
                        lastCanvasPos, currentCanvasPos, 3.5f, maskLineColor);
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

              {
                ArtifactCore::ProfileScope _profMaskPoints(
                    "MaskDrawPoints", ArtifactCore::ProfileCategory::Render);
                for (const auto &marker : markers) {
                  renderer_->drawPoint(marker.pos.x, marker.pos.y,
                                       marker.radius + 3.0f,
                                       maskPointShadowColor);
                  renderer_->drawPoint(marker.pos.x, marker.pos.y,
                                       marker.radius, marker.color);
               }
              }
              }
            }
          }

        }

        if (pendingMaskCreation_ &&
            pendingMaskLayerId_ == selectedLayer->id() &&
            pendingMaskPath_.vertexCount() > 0) {
          const MaskPath &path = pendingMaskPath_;
          const int vertexCount = path.vertexCount();
          const FloatColor pendingLineShadowColor = {0.0f, 0.0f, 0.0f, 0.24f};
          const FloatColor pendingLineColor = {0.40f, 0.92f, 0.98f, 0.70f};
          const FloatColor pendingPointShadowColor = {0.0f, 0.0f, 0.0f, 0.36f};
          const FloatColor pendingPointColor = {0.84f, 0.98f, 1.0f, 0.88f};
          const QTransform globalTransform = selectedLayer->getGlobalTransform();

          Detail::float2 lastCanvasPos;
          for (int v = 0; v < vertexCount; ++v) {
            const MaskVertex vertex = path.vertex(v);
            const QPointF canvasPos = globalTransform.map(vertex.position);
            const Detail::float2 currentCanvasPos = {
                static_cast<float>(canvasPos.x()),
                static_cast<float>(canvasPos.y())};
            if (v > 0) {
              renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                            5.0f, pendingLineShadowColor);
              renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos,
                                            2.5f, pendingLineColor);
            }
            renderer_->drawPoint(currentCanvasPos.x, currentCanvasPos.y, 11.0f,
                                 pendingPointShadowColor);
            renderer_->drawPoint(currentCanvasPos.x, currentCanvasPos.y, 7.0f,
                                 pendingPointColor);
            lastCanvasPos = currentCanvasPos;
          }
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

    if (renderer_) {
      // Reset to composition space with the CURRENT viewport zoom/pan so that
      // overlays (bounding box, motion path) are drawn at the correct position.
      // Using hardcoded zoom=1/pan=0 here placed the bounding box at wrong
      // screen coordinates (appeared as an orange L-shape at top-left corner).
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      // zoom and pan are already restored to origZoom/origPan by the render
      // path above — do NOT override them here.
    }

    if (renderer_ && !selectedIds.isEmpty()) {
      // Reuse the layer list already fetched above; avoids a second allLayer()
      // call.
      const auto &layersForOverlay = layers;

      // M-UI-6 Composition Motion Path Overlay
      // Guard: only run when the overlay is enabled — the 300-iteration
      // getGlobalTransformAt() loop is the main >1000ms bottleneck.
      if (comp && showMotionPathOverlay_) {
        ArtifactCore::ProfileScope _profMotion(
            "MotionPath", ArtifactCore::ProfileCategory::Render);
        const float zoom = renderer_->getZoom();
        const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
        for (const auto &layer : layersForOverlay) {
          if (!layer || !isLayerSelected(selectedIds, layer))
            continue;

          const auto &t3d = layer->transform3D();
          auto posTimes = t3d.getPositionKeyFrameTimes();
          if (posTimes.size() < 2)
            continue;

          int minFrame = static_cast<int>(posTimes.front().value());
          int maxFrame = static_cast<int>(posTimes.back().value());
          const int currentFrameNum = currentFrame.framePosition();

          // Limit drawing range for performance (±300 frames around playhead).
          minFrame = std::max(minFrame, currentFrameNum - 300);
          maxFrame = std::min(maxFrame, currentFrameNum + 300);

          if (minFrame >= maxFrame)
            continue;

          const int64_t rate = posTimes.front().scale();
          const FloatColor pathColor{0.9f, 0.4f, 0.8f, 0.9f};
          const float lineThickness = std::max(1.0f, 1.5f * invZoom);
          const float dotRadius = std::max(1.5f, 2.5f * invZoom);

          // --- Motion path cache -------------------------------------------
          // Key: (layerId, currentFrameNum, overlaySerial).
          // On a cache miss re-sample all path points; on a hit skip all
          // getGlobalTransformAt() calls (the hot loop that caused >1000ms).
          const bool cacheHit =
              motionPathCache_.valid &&
              motionPathCache_.layerId == layer->id() &&
              motionPathCache_.framePos ==
                  static_cast<int64_t>(currentFrameNum) &&
              motionPathCache_.overlaySerial == overlayInvalidationSerial_;

          if (!cacheHit) {
            motionPathCache_.valid = false;
            motionPathCache_.layerId = layer->id();
            motionPathCache_.framePos = static_cast<int64_t>(currentFrameNum);
            motionPathCache_.overlaySerial = overlayInvalidationSerial_;
            motionPathCache_.pathPoints.clear();
            motionPathCache_.keyPoints.clear();

            for (int f = minFrame; f <= maxFrame; f += 2) {
              if (f > maxFrame)
                f = maxFrame;
              ArtifactCore::RationalTime t(f, rate);
              QTransform gTrans = layer->getGlobalTransformAt(f);
              float ax = t3d.anchorXAt(t);
              float ay = t3d.anchorYAt(t);
              QPointF wPos = gTrans.map(QPointF(ax, ay));
              motionPathCache_.pathPoints.push_back(
                  {f, (float)wPos.x(), (float)wPos.y()});
            }

            for (const auto &kfTime : posTimes) {
              int f = static_cast<int>(kfTime.value());
              if (f < minFrame || f > maxFrame)
                continue;
              QTransform gTrans = layer->getGlobalTransformAt(f);
              float ax = t3d.anchorXAt(kfTime);
              float ay = t3d.anchorYAt(kfTime);
              QPointF wPos = gTrans.map(QPointF(ax, ay));
              motionPathCache_.keyPoints.push_back(
                  {f, (float)wPos.x(), (float)wPos.y()});
            }
            motionPathCache_.valid = true;
          }

          // Render from cache (no getGlobalTransformAt() calls on a hit).
          Detail::float2 lastPos;
          bool hasLastPos = false;
          for (const auto &pt : motionPathCache_.pathPoints) {
            Detail::float2 currentPos(pt.x, pt.y);
            if (hasLastPos)
              renderer_->drawSolidLine(lastPos, currentPos, pathColor,
                                       lineThickness);
            renderer_->drawPoint(pt.x, pt.y, dotRadius * 0.6f,
                                 {0.8f, 0.8f, 0.8f, 0.7f});
            lastPos = currentPos;
            hasLastPos = true;
          }
          for (const auto &pt : motionPathCache_.keyPoints) {
            // Temporarily disable square key markers in the viewport overlay.
            // The motion path line remains, but the small frame-only squares were
            // visually confusing during Shape Layer debugging.
            // renderer_->drawSolidRect(pt.x - dotRadius, pt.y - dotRadius,
            //                          dotRadius * 2, dotRadius * 2,
            //                          (pt.frame == currentFrameNum)
            //                              ? FloatColor{1.0f, 1.0f, 0.0f, 1.0f}
            //                              : FloatColor{1.0f, 1.0f, 1.0f, 1.0f});
            // renderer_->drawRectOutline(pt.x - dotRadius, pt.y - dotRadius,
            //                            dotRadius * 2, dotRadius * 2,
            //                            {0.0f, 0.0f, 0.0f, 1.0f});
          }
        }
      }
      {
        ArtifactCore::ProfileScope _profBBox(
            "BoundingBox", ArtifactCore::ProfileCategory::Render);
        const FloatColor primaryColor{1.0f, 0.72f, 0.22f, 1.0f};
        const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};
        for (const auto &layer : layersForOverlay) {
          if (!isLayerEffectivelyVisible(layer) ||
              !layer->isActiveAt(currentFrame)) {
            continue;
          }
          if (!isLayerSelected(selectedIds, layer)) {
            continue;
          }

          const QRectF localBounds = layer->localBounds();
          if (!localBounds.isValid() || localBounds.width() <= 0.0 ||
              localBounds.height() <= 0.0) {
            continue;
          }

          const bool primary =
              !selectedLayerId_.isNil() && layer->id() == selectedLayerId_;
          const QTransform globalTransform = layer->getGlobalTransform();
          const QPointF tl = globalTransform.map(localBounds.topLeft());
          const QPointF tr = globalTransform.map(localBounds.topRight());
          const QPointF br = globalTransform.map(localBounds.bottomRight());
          const QPointF bl = globalTransform.map(localBounds.bottomLeft());
          const FloatColor color = primary ? primaryColor : secondaryColor;
          const float thickness = primary ? 1.9f : 1.4f;
          renderer_->drawSolidLine(
              {static_cast<float>(tl.x()), static_cast<float>(tl.y())},
              {static_cast<float>(tr.x()), static_cast<float>(tr.y())}, color,
              thickness);
          renderer_->drawSolidLine(
              {static_cast<float>(tr.x()), static_cast<float>(tr.y())},
              {static_cast<float>(br.x()), static_cast<float>(br.y())}, color,
              thickness);
          renderer_->drawSolidLine(
              {static_cast<float>(br.x()), static_cast<float>(br.y())},
              {static_cast<float>(bl.x()), static_cast<float>(bl.y())}, color,
              thickness);
          renderer_->drawSolidLine(
              {static_cast<float>(bl.x()), static_cast<float>(bl.y())},
              {static_cast<float>(tl.x()), static_cast<float>(tl.y())}, color,
              thickness);
        }
      } // BoundingBox scope
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
      // Safe area is a screen-space 2D guide. Keep it out of any 3D camera
      // path.
      const float actionSafeW = cw * 0.9f;
      const float actionSafeH = ch * 0.9f;
      const float titleSafeW = cw * 0.8f;
      const float titleSafeH = ch * 0.8f;
      const FloatColor outlineColor = {0.0f, 0.0f, 0.0f, 0.72f};
      const FloatColor innerColor = {0.95f, 0.97f, 1.0f, 0.94f};
      const auto snap = [](float value) { return std::round(value) + 0.5f; };

      const float actionX = snap((cw - actionSafeW) * 0.5f);
      const float actionY = snap((ch - actionSafeH) * 0.5f);
      const float actionX2 = snap(actionX + actionSafeW);
      const float actionY2 = snap(actionY + actionSafeH);
      const float titleX = snap((cw - titleSafeW) * 0.5f);
      const float titleY = snap((ch - titleSafeH) * 0.5f);
      const float titleX2 = snap(titleX + titleSafeW);
      const float titleY2 = snap(titleY + titleSafeH);
      const auto drawSafeRect = [&](float x1, float y1, float x2, float y2) {
        if (!renderer_) {
          return;
        }
        const float w = x2 - x1;
        const float h = y2 - y1;
        if (w <= 0.0f || h <= 0.0f) {
          return;
        }

        renderer_->drawRectOutline(x1, y1, w, h, outlineColor);
        if (w > 4.0f && h > 4.0f) {
          renderer_->drawRectOutline(x1 + 1.0f, y1 + 1.0f, w - 2.0f, h - 2.0f,
                                     innerColor);
        }
      };

      drawSafeRect(actionX, actionY, actionX2, actionY2);
      drawSafeRect(titleX, titleY, titleX2, titleY2);

      const float crossSize = std::max(20.0f, std::min(cw, ch) * 0.05f);
      renderer_->drawCrosshair(snap(cw * 0.5f), snap(ch * 0.5f), crossSize,
                               innerColor);
    }

    guideVerticals_.clear();
    guideHorizontals_.clear();
    if (showGuides_ && comp && !selectedLayerId_.isNil()) {
      const auto guideLayer = comp->layerById(selectedLayerId_);
      if (guideLayer) {
        const QRectF bounds = guideLayer->transformedBoundingBox();
        if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
          const float left = static_cast<float>(bounds.left());
          const float right = static_cast<float>(bounds.right());
          const float top = static_cast<float>(bounds.top());
          const float bottom = static_cast<float>(bounds.bottom());
          const float centerX = static_cast<float>(bounds.center().x());
          const float centerY = static_cast<float>(bounds.center().y());
          guideVerticals_.push_back(left);
          guideVerticals_.push_back(centerX);
          guideVerticals_.push_back(right);
          guideHorizontals_.push_back(top);
          guideHorizontals_.push_back(centerY);
          guideHorizontals_.push_back(bottom);
        }
      }
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
    {
      ArtifactCore::ProfileScope _profOverlay(
          "Overlay", ArtifactCore::ProfileCategory::UI);
      const ArtifactAbstractLayerPtr selectedLayer =
          (!selectedLayerId_.isNil() && comp)
              ? comp->layerById(selectedLayerId_)
              : ArtifactAbstractLayerPtr{};
      renderer_->setUseExternalMatrices(false);
      renderer_->resetGizmoCameraMatrices();
      renderer_->reset3DCameraMatrices();
      renderer_->setCanvasSize(cw, ch);
      if (showCompositionRegionOverlay_) {
        drawCompositionRegionOverlay(renderer_.get(), comp);
      }
      if (selectedLayer) {
        drawSelectionOverlay(renderer_.get(), selectedLayer);
      }
      if (showAnchorCenterOverlay_ && selectedLayer) {
        drawAnchorCenterOverlay(renderer_.get(), selectedLayer);
      }
      drawViewportGhostOverlay(owner, comp, selectedLayer, currentFrame);
      drawViewportUiOverlay();
    }
    overlayMs = markPhaseMs();

    flushMs = 0;

    if (!lastVideoDebug_.isEmpty() &&
        lastVideoDebug_ != lastEmittedVideoDebug_) {
      lastEmittedVideoDebug_ = lastVideoDebug_;
      Q_EMIT owner->videoDebugMessage(lastVideoDebug_);
    }

    // renderer_->flushAndWait(); // 毎フレーム同期を削除し、性能改善を試む
    {
      ArtifactCore::ProfileScope _profPresent(
          "Present", ArtifactCore::ProfileCategory::Render);
      renderer_->present();
    }
    presentMs = markPhaseMs();
    lastSubmit2DMs_ = latestTimerMs("Submit2D");

    ++renderFrameCounter_;
    const qint64 frameMs = frameTimer.elapsed();
    lastFrameTimeMs_ = static_cast<double>(frameMs);
    recentFrameTimesMs_.push_back(lastFrameTimeMs_);
    recentFrameTimeSumMs_ += lastFrameTimeMs_;
    constexpr std::size_t kRecentFrameTimeHistory = 120;
    while (recentFrameTimesMs_.size() > kRecentFrameTimeHistory) {
      recentFrameTimeSumMs_ -= recentFrameTimesMs_.front();
      recentFrameTimesMs_.pop_front();
    }
    if (!recentFrameTimesMs_.empty()) {
      averageFrameTimeMs_ =
          recentFrameTimeSumMs_ /
          static_cast<double>(recentFrameTimesMs_.size());
    } else {
      averageFrameTimeMs_ = 0.0;
    }
    lastSetupMs_ = setupMs;
    lastBasePassMs_ = basePassMs;
    lastLayerPassMs_ = layerPassMs;
    lastOverlayMs_ = overlayMs;
    lastFlushMs_ = flushMs;
    lastSubmit2DMs_ = std::max<qint64>(lastSubmit2DMs_, 0);
    lastPresentMs_ = presentMs;
    const auto textureCacheStats = gpuTextureCacheManager_
                                       ? gpuTextureCacheManager_->stats()
                                       : GPUTextureCacheStats{};
    lastRenderPathSummary_ =
        QStringLiteral(
            "path=%1 gpuBlendEnabled=%2 gpuBlendReady=%3 layersTotal=%4 "
            "layersDrawn=%5 surfaceUploadLayers=%6 cpuRasterLayers=%7 "
            "frameOutOfRange=%8 previewDownsample=%9 effectiveDownsample=%10 "
            "viewportInteracting=%11 cacheEntries=%12 cacheBytes=%13")
            .arg(pipelineEnabled ? QStringLiteral("gpu-blend")
                                 : QStringLiteral("fallback"))
            .arg(gpuBlendEnabled_)
            .arg(blendPipelineReady_)
            .arg(layers.size())
            .arg(drawnLayerCount)
            .arg(surfaceUploadLayerCount)
            .arg(cpuRasterLayerCount)
            .arg(frameOutOfRange)
            .arg(previewDownsample_)
            .arg(effectivePreviewDownsample)
            .arg(viewportInteracting_)
            .arg(textureCacheStats.entryCount)
            .arg(static_cast<qulonglong>(textureCacheStats.memoryBytes));
    if (compositionViewLog().isDebugEnabled()) {
      if (frameMs >= 16) {
        qCDebug(compositionViewLog)
            << "[CompositionView][Perf][Slow]"
            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            << "layersDrawn=" << drawnLayerCount
            << "surfaceUploadLayers=" << surfaceUploadLayerCount
            << "cpuRasterLayers=" << cpuRasterLayerCount
            << "frameOutOfRange=" << frameOutOfRange
            << "previewDownsample=" << previewDownsample_
            << "effectivePreviewDownsample=" << effectivePreviewDownsample
            << "viewportInteracting=" << viewportInteracting_
            << "setupMs=" << setupMs << "basePassMs=" << basePassMs
            << "layerPassMs=" << layerPassMs << "overlayMs=" << overlayMs
            << "flushMs=" << flushMs << "submit2DMs=" << lastSubmit2DMs_
            << "presentMs=" << presentMs;
      } else if ((renderFrameCounter_ % 120u) == 0u) {
        qCDebug(compositionViewLog)
            << "[CompositionView][Perf]"
            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled
            << "layersTotal=" << layers.size()
            << "layersDrawn=" << drawnLayerCount << "setupMs=" << setupMs
            << "basePassMs=" << basePassMs << "layerPassMs=" << layerPassMs
            << "overlayMs=" << overlayMs << "flushMs=" << flushMs
            << "submit2DMs=" << lastSubmit2DMs_ << "presentMs=" << presentMs;
      }
    }
  } // _profSetup ("RenderFrame") destructs here — BEFORE endFrame() so its
    // duration is correctly recorded (popScope requires inFrame == true).
  ArtifactCore::Profiler::instance().endFrame();
} // end renderOneFrameImpl

QRectF CompositionRenderController::Impl::commandPaletteRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  const float panelW = std::min(560.0f, std::max(320.0f, overlayWf - 48.0f));
  const int visibleRows =
      std::min(8, static_cast<int>(commandPaletteItems_.size()));
  const float panelH = 54.0f + static_cast<float>(visibleRows) * 30.0f + 12.0f;
  const float x = std::max(12.0f, (overlayWf - panelW) * 0.5f);
  const float y = std::max(12.0f, std::min(96.0f, overlayHf * 0.16f));
  return QRectF(x, y, panelW, std::min(panelH, std::max(96.0f, overlayHf - y - 12.0f)));
}

QRectF CompositionRenderController::Impl::contextMenuRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  QFont font = QApplication::font();
  font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
  const QFontMetrics fm(font);
  int textW = 140;
  for (const QString &item : contextMenuItems_) {
    textW = std::max(textW, fm.horizontalAdvance(item));
  }
  const float panelW = static_cast<float>(std::min(280, textW + 40));
  const float panelH = 12.0f + static_cast<float>(contextMenuItems_.size()) * 28.0f;
  float x = static_cast<float>(contextMenuViewportPos_.x());
  float y = static_cast<float>(contextMenuViewportPos_.y());
  if (x + panelW > overlayWf - 8.0f) {
    x = overlayWf - panelW - 8.0f;
  }
  if (y + panelH > overlayHf - 8.0f) {
    y = overlayHf - panelH - 8.0f;
  }
  return QRectF(std::max(8.0f, x), std::max(8.0f, y), panelW, panelH);
}

QRectF CompositionRenderController::Impl::pieMenuRect() const {
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  const float diameter = std::min(overlayWf, overlayHf) * 0.42f;
  const float size = std::clamp(diameter, 180.0f, 420.0f);
  const float minX = 12.0f;
  const float minY = 12.0f;
  const float maxX = std::max(minX, overlayWf - size - 12.0f);
  const float maxY = std::max(minY, overlayHf - size - 12.0f);
  const float x = std::min(std::max(static_cast<float>(pieMenuViewportPos_.x()) - size * 0.5f, minX), maxX);
  const float y = std::min(std::max(static_cast<float>(pieMenuViewportPos_.y()) - size * 0.5f, minY), maxY);
  return QRectF(x, y, size, size);
}

QRectF CompositionRenderController::Impl::viewportOverlayItemRect(int index) const {
  if (index < 0) {
    return {};
  }
  if (commandPaletteVisible_) {
    if (index >= static_cast<int>(commandPaletteItems_.size()) || index >= 8) {
      return {};
    }
    const QRectF panel = commandPaletteRect();
    return QRectF(panel.left() + 10.0, panel.top() + 54.0 + index * 30.0,
                  panel.width() - 20.0, 28.0);
  }
  if (contextMenuVisible_) {
    if (index >= static_cast<int>(contextMenuItems_.size())) {
      return {};
    }
    const QRectF panel = contextMenuRect();
    return QRectF(panel.left() + 6.0, panel.top() + 6.0 + index * 28.0,
                  panel.width() - 12.0, 26.0);
  }
  return {};
}

int CompositionRenderController::Impl::pieMenuItemAt(const QPointF &viewportPos) const {
  if (!pieMenuVisible_ || pieMenuModel_.items.empty()) {
    return -1;
  }
  const QRectF rect = pieMenuRect();
  const QPointF center = rect.center();
  const QPointF delta = viewportPos - center;
  const double dist = std::hypot(delta.x(), delta.y());
  const double innerRadius = rect.width() * 0.19;
  const double outerRadius = rect.width() * 0.48;
  if (dist < innerRadius || dist > outerRadius) {
    return -1;
  }
  const double angle = std::atan2(-delta.y(), delta.x()) * 180.0 / M_PI;
  double normalized = angle < 0.0 ? angle + 360.0 : angle;
  const int count = static_cast<int>(pieMenuModel_.items.size());
  if (count <= 0) {
    return -1;
  }
  const double sectorSize = 360.0 / static_cast<double>(count);
  double shifted = normalized - (90.0 - sectorSize * 0.5);
  while (shifted < 0.0) {
    shifted += 360.0;
  }
  while (shifted >= 360.0) {
    shifted -= 360.0;
  }
  const int index = static_cast<int>(shifted / sectorSize);
  if (index < 0 || index >= count) {
    return -1;
  }
  return index;
}

int CompositionRenderController::Impl::viewportOverlayItemAt(
    const QPointF &viewportPos) const {
  const int count = commandPaletteVisible_
                        ? std::min(8, static_cast<int>(commandPaletteItems_.size()))
                        : (contextMenuVisible_
                               ? static_cast<int>(contextMenuItems_.size())
                               : 0);
  for (int i = 0; i < count; ++i) {
    if (viewportOverlayItemRect(i).contains(viewportPos)) {
      return i;
    }
  }
  return -1;
}

void CompositionRenderController::Impl::drawPieMenuOverlay() {
  if (!renderer_ || !pieMenuVisible_ || pieMenuModel_.items.empty()) {
    return;
  }

  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const float prevZoom = renderer_->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer_->getPan(prevPanX, prevPanY);
  renderer_->setUseExternalMatrices(false);
  renderer_->setCanvasSize(overlayWf, overlayHf);
  renderer_->setZoom(1.0f);
  renderer_->setPan(0.0f, 0.0f);

  const QRectF rect = pieMenuRect();
  const QPointF center = rect.center();
  const float outerRadius = rect.width() * 0.48f;
  const float innerRadius = rect.width() * 0.19f;
  const int count = static_cast<int>(pieMenuModel_.items.size());
  const float sectorSize = 360.0f / static_cast<float>(std::max(1, count));
  renderer_->drawSolidRect(0.0f, 0.0f, overlayWf, overlayHf,
                           FloatColor{0.0f, 0.0f, 0.0f, 0.16f}, 1.0f);
  renderer_->drawCircle(static_cast<float>(center.x()),
                        static_cast<float>(center.y()),
                        outerRadius + 8.0f,
                        FloatColor{0.08f, 0.10f, 0.13f, 0.94f}, 1.0f, true);
  renderer_->drawCircle(static_cast<float>(center.x()),
                        static_cast<float>(center.y()),
                        innerRadius - 2.0f,
                        FloatColor{0.05f, 0.06f, 0.08f, 0.98f}, 1.0f, true);

  QFont labelFont = QApplication::font();
  labelFont.setPointSizeF(std::max(9.0, static_cast<double>(labelFont.pointSizeF())));
  labelFont.setWeight(QFont::DemiBold);
  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);

  for (int i = 0; i < count; ++i) {
    const auto &item = pieMenuModel_.items[static_cast<size_t>(i)];
    const float startAngle = 90.0f - (i + 1) * sectorSize + sectorSize * 0.5f;
    const float endAngle = startAngle + sectorSize;
    const int steps = 10;
    std::vector<Detail::float2> polygon;
    polygon.reserve(static_cast<size_t>(steps + 3));
    polygon.push_back({static_cast<float>(center.x()), static_cast<float>(center.y())});
    for (int s = 0; s <= steps; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(steps);
      const float ang = (startAngle + (endAngle - startAngle) * t) * static_cast<float>(M_PI) / 180.0f;
      polygon.push_back({
          static_cast<float>(center.x() + std::cos(ang) * outerRadius),
          static_cast<float>(center.y() - std::sin(ang) * outerRadius)});
    }
    const bool selected = (i == pieMenuSelectedIndex_);
    renderer_->drawSolidPolygonLocal(polygon,
                                     selected ? FloatColor{0.18f, 0.34f, 0.52f, 0.95f}
                                              : FloatColor{0.10f, 0.12f, 0.15f, 0.88f});

    std::vector<Detail::float2> innerEdge;
    innerEdge.reserve(static_cast<size_t>(steps + 3));
    for (int s = 0; s <= steps; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(steps);
      const float ang = (startAngle + (endAngle - startAngle) * t) * static_cast<float>(M_PI) / 180.0f;
      innerEdge.push_back({
          static_cast<float>(center.x() + std::cos(ang) * innerRadius),
          static_cast<float>(center.y() - std::sin(ang) * innerRadius)});
    }
    renderer_->drawSolidPolygonLocal(innerEdge,
                                     FloatColor{0.04f, 0.05f, 0.07f, 0.98f});

    const float midAngle = (startAngle + sectorSize * 0.5f) * static_cast<float>(M_PI) / 180.0f;
    const float labelRadius = (innerRadius + outerRadius) * 0.5f;
    const QPointF labelPos(center.x() + std::cos(midAngle) * labelRadius,
                           center.y() - std::sin(midAngle) * labelRadius);
    const QRectF textRect(labelPos.x() - sectorSize * 1.0f,
                          labelPos.y() - 14.0f,
                          sectorSize * 2.0f, 28.0f);
    renderer_->drawText(textRect, item.label,
                        labelFont,
                        item.enabled ? FloatColor{0.92f, 0.95f, 0.98f, 1.0f}
                                     : FloatColor{0.55f, 0.58f, 0.62f, 1.0f},
                        Qt::AlignCenter);
  }

  renderer_->drawCircle(static_cast<float>(center.x()),
                        static_cast<float>(center.y()),
                        innerRadius - 4.0f,
                        FloatColor{0.03f, 0.04f, 0.06f, 1.0f}, 1.0f, true);
  renderer_->drawText(QRectF(center.x() - innerRadius, center.y() - innerRadius,
                             innerRadius * 2.0f, innerRadius * 2.0f),
                      pieMenuModel_.title.isEmpty() ? QStringLiteral("Menu")
                                                   : pieMenuModel_.title,
                      titleFont, FloatColor{0.95f, 0.97f, 0.99f, 1.0f},
                      Qt::AlignCenter);

  renderer_->setZoom(prevZoom);
  renderer_->setPan(prevPanX, prevPanY);
}

void CompositionRenderController::Impl::drawViewportUiOverlay() {
  if (!renderer_ || (!commandPaletteVisible_ && !contextMenuVisible_ &&
                     !pieMenuVisible_)) {
    return;
  }
  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const float prevZoom = renderer_->getZoom();
  float prevPanX = 0.0f;
  float prevPanY = 0.0f;
  renderer_->getPan(prevPanX, prevPanY);
  renderer_->setUseExternalMatrices(false);
  renderer_->setCanvasSize(overlayWf, overlayHf);
  renderer_->setZoom(1.0f);
  renderer_->setPan(0.0f, 0.0f);

  QFont titleFont = QApplication::font();
  titleFont.setPointSizeF(std::max(10.0, static_cast<double>(titleFont.pointSizeF()) + 1.0));
  titleFont.setWeight(QFont::DemiBold);
  QFont itemFont = QApplication::font();
  itemFont.setPointSizeF(std::max(9.0, static_cast<double>(itemFont.pointSizeF())));

  if (commandPaletteVisible_) {
    renderer_->drawSolidRect(0.0f, 0.0f, overlayWf, overlayHf,
                             FloatColor{0.0f, 0.0f, 0.0f, 0.22f}, 1.0f);
    const QRectF panel = commandPaletteRect();
    renderer_->drawSolidRect(static_cast<float>(panel.left()),
                             static_cast<float>(panel.top()),
                             static_cast<float>(panel.width()),
                             static_cast<float>(panel.height()),
                             FloatColor{0.055f, 0.065f, 0.078f, 0.96f}, 1.0f);
    renderer_->drawRectOutline(static_cast<float>(panel.left()),
                               static_cast<float>(panel.top()),
                               static_cast<float>(panel.width()),
                               static_cast<float>(panel.height()),
                               FloatColor{0.35f, 0.50f, 0.70f, 0.90f});
    renderer_->drawText(panel.adjusted(14.0, 8.0, -14.0, -panel.height() + 34.0),
                        QStringLiteral("Command Palette"), titleFont,
                        FloatColor{0.90f, 0.94f, 0.98f, 1.0f},
                        Qt::AlignLeft | Qt::AlignVCenter);
    const QString queryText = commandPaletteQuery_.trimmed().isEmpty()
                                  ? QStringLiteral("Type to filter commands")
                                  : commandPaletteQuery_.trimmed();
    renderer_->drawText(QRectF(panel.left() + 14.0, panel.top() + 30.0,
                               panel.width() - 28.0, 18.0),
                        queryText, itemFont,
                        FloatColor{0.56f, 0.64f, 0.72f, 1.0f},
                        Qt::AlignLeft | Qt::AlignVCenter);
    const int count =
        std::min(8, static_cast<int>(commandPaletteItems_.size()));
    for (int i = 0; i < count; ++i) {
      const QRectF row = viewportOverlayItemRect(i);
      if (i == 0) {
        renderer_->drawSolidRect(static_cast<float>(row.left()),
                                 static_cast<float>(row.top()),
                                 static_cast<float>(row.width()),
                                 static_cast<float>(row.height()),
                                 FloatColor{0.16f, 0.28f, 0.44f, 0.86f}, 1.0f);
      }
      renderer_->drawText(row.adjusted(10.0, 0.0, -8.0, 0.0),
                          commandPaletteItems_.at(i), itemFont,
                          FloatColor{0.88f, 0.91f, 0.94f, 1.0f},
                          Qt::AlignLeft | Qt::AlignVCenter);
    }
  }

  if (contextMenuVisible_) {
    const QRectF panel = contextMenuRect();
    renderer_->drawSolidRect(static_cast<float>(panel.left()),
                             static_cast<float>(panel.top()),
                             static_cast<float>(panel.width()),
                             static_cast<float>(panel.height()),
                             FloatColor{0.060f, 0.068f, 0.078f, 0.97f}, 1.0f);
    renderer_->drawRectOutline(static_cast<float>(panel.left()),
                               static_cast<float>(panel.top()),
                               static_cast<float>(panel.width()),
                               static_cast<float>(panel.height()),
                               FloatColor{0.30f, 0.34f, 0.40f, 0.96f});
    for (int i = 0; i < static_cast<int>(contextMenuItems_.size()); ++i) {
      const QRectF row = viewportOverlayItemRect(i);
      if (i == 0) {
        renderer_->drawSolidRect(static_cast<float>(row.left()),
                                 static_cast<float>(row.top()),
                                 static_cast<float>(row.width()),
                                 static_cast<float>(row.height()),
                                 FloatColor{0.15f, 0.22f, 0.31f, 0.80f}, 1.0f);
      }
      renderer_->drawText(row.adjusted(10.0, 0.0, -8.0, 0.0),
                          contextMenuItems_.at(i), itemFont,
                          FloatColor{0.88f, 0.90f, 0.92f, 1.0f},
                          Qt::AlignLeft | Qt::AlignVCenter);
    }
  }

  if (pieMenuVisible_) {
    drawPieMenuOverlay();
  }

  renderer_->setZoom(prevZoom);
  renderer_->setPan(prevPanX, prevPanY);
}

void CompositionRenderController::Impl::drawViewportGhostOverlay(
    CompositionRenderController *owner, const ArtifactCompositionPtr &comp,
    const ArtifactAbstractLayerPtr &selectedLayer,
    const FramePosition &currentFrame) {
  Q_UNUSED(owner);
  Q_UNUSED(currentFrame);
  if (!renderer_) {
    return;
  }

  const bool dropActive = dropGhostVisible_ && !dropGhostRect_.isNull();
  if (!dropActive) {
    return;
  }

  if (compositionViewLog().isDebugEnabled()) {
    qCDebug(compositionViewLog)
        << "[CompositionView][ViewportGhost]"
        << "dropActive=" << dropActive << "rect=" << dropGhostRect_
        << "title=" << dropGhostTitle_;
  }

  const float overlayWf = hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;
  const float overlayHf = hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;
  if (overlayWf <= 0.0f || overlayHf <= 0.0f) {
    return;
  }

  const int overlayW = std::max(1, static_cast<int>(std::ceil(overlayWf)));
  const int overlayH = std::max(1, static_cast<int>(std::ceil(overlayHf)));
  const bool snapHintActive = gizmo_ && gizmo_->isDragging() && selectedLayer;
  const bool infoActive = infoOverlayVisible_ &&
                          (!infoOverlayTitle_.trimmed().isEmpty() ||
                           !infoOverlayDetail_.trimmed().isEmpty());
  if (dropActive && !infoActive && !snapHintActive) {
    const float prevZoom = renderer_->getZoom();
    float prevPanX = 0.0f;
    float prevPanY = 0.0f;
    renderer_->getPan(prevPanX, prevPanY);
    renderer_->setCanvasSize(overlayWf, overlayHf);
    renderer_->setZoom(1.0f);
    renderer_->setPan(0.0f, 0.0f);

    renderer_->drawSolidRect(0.0f, 0.0f, overlayWf, overlayHf,
                             FloatColor{0.24f, 0.47f, 0.94f, 0.10f}, 1.0f);
    renderer_->drawRectOutline(4.0f, 4.0f, overlayWf - 8.0f,
                               overlayHf - 8.0f,
                               FloatColor{0.39f, 0.63f, 1.0f, 0.70f});
    const QRectF ghostRect = dropGhostRect_.normalized();
    renderer_->drawSolidRect(static_cast<float>(ghostRect.left()),
                             static_cast<float>(ghostRect.top()),
                             static_cast<float>(ghostRect.width()),
                             static_cast<float>(ghostRect.height()),
                             FloatColor{0.12f, 0.16f, 0.24f, 0.52f}, 1.0f);
    renderer_->drawRectOutline(static_cast<float>(ghostRect.left()),
                               static_cast<float>(ghostRect.top()),
                               static_cast<float>(ghostRect.width()),
                               static_cast<float>(ghostRect.height()),
                               FloatColor{0.86f, 0.92f, 1.0f, 0.88f});
    renderer_->setZoom(prevZoom);
    renderer_->setPan(prevPanX, prevPanY);
    const QSize compSize = comp ? comp->settings().compositionSize() : QSize();
    const float cw = static_cast<float>(
        compSize.width() > 0 ? compSize.width()
                             : (lastCanvasWidth_ > 0.0f ? lastCanvasWidth_ : 1920.0f));
    const float ch = static_cast<float>(
        compSize.height() > 0 ? compSize.height()
                              : (lastCanvasHeight_ > 0.0f ? lastCanvasHeight_ : 1080.0f));
    renderer_->setCanvasSize(cw, ch);
    return;
  }

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

  if (infoActive) {
    const QString title = infoOverlayTitle_.trimmed().isEmpty()
                              ? QStringLiteral("Info")
                              : infoOverlayTitle_.trimmed();
    const QString detail = infoOverlayDetail_.trimmed();
    const QFontMetrics fm(p.font());
    const int lineHeight = fm.height();
    const int contentWidth =
        std::max(fm.horizontalAdvance(title),
                 detail.isEmpty() ? 0 : fm.horizontalAdvance(detail));
    const int contentHeight =
        detail.isEmpty() ? lineHeight : lineHeight * 2 + 4;
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
    p.drawText(labelRect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignTop,
               title);
    if (!detail.isEmpty()) {
      p.setPen(QColor(178, 190, 204));
      const QRect detailRect = labelRect.adjusted(10, 6 + lineHeight, -10, -6);
      p.drawText(detailRect, Qt::AlignLeft | Qt::AlignTop,
                 fm.elidedText(detail, Qt::ElideRight, detailRect.width()));
    }
  }

  if (snapHintActive) {
    const bool snapBypassed =
        QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    QString snapTitle =
        snapBypassed ? QStringLiteral("Snap Off") : QStringLiteral("Snap On");
    const QString snapDetail =
        snapBypassed ? QStringLiteral("Hold Alt to enable free move")
                     : QStringLiteral("Hold Alt to bypass snapping");
    if (!snapBypassed && gizmo_) {
      int verticalCount = 0;
      int horizontalCount = 0;
      for (const auto &line : gizmo_->activeSnapLines()) {
        if (line.isVertical) {
          ++verticalCount;
        } else {
          ++horizontalCount;
        }
      }
      if (verticalCount > 0 || horizontalCount > 0) {
        QStringList parts;
        if (verticalCount > 0) {
          parts << QStringLiteral("X");
        }
        if (horizontalCount > 0) {
          parts << QStringLiteral("Y");
        }
        if (!parts.isEmpty()) {
          snapTitle += QStringLiteral(" - ");
          snapTitle += parts.join(QStringLiteral("/"));
        }
      }
    }
    const QFontMetrics fm(p.font());
    const int lineHeight = fm.height();
    const int contentWidth = std::max(fm.horizontalAdvance(snapTitle),
                                      fm.horizontalAdvance(snapDetail));
    QRect labelRect(12, overlayH - (lineHeight * 2 + 28), contentWidth + 24,
                    lineHeight * 2 + 12);
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
    p.drawText(labelRect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignTop,
               snapTitle);
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
