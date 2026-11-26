module;

#include <DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#define NOMINMAX

#define QT_NO_KEYWORDS



#include <QApplication>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>

#include <QByteArray>

#include <QColor>
#include <QDebug>

#include <QElapsedTimer>

#include <QFont>
#include <QFontDatabase>

#include <QFontMetrics>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <QFuture>

#include <QHash>

#include <QImage>

#include <QList>

#include <QLoggingCategory>

#include <QLineF>

#include <QMatrix4x4>

#include <QMetaObject>

#include <QMutex>

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <QPointer>
#include <QPointF>

#include <QRect>

#include <QRectF>
#include <QInputDialog>

#include <QSizeF>

#include <QSet>
#include <QSettings>

#include <QStringList>
#include <QString>
#include <QTextDocument>
#include <QUuid>

#include <QTimer>

#include <QTransform>
#include <QWidget>
#include <QScreen>
#include <QtGlobal>

#include <QVector3D>
#include <QVector2D>

#include <QVector4D>

#include <QVector>
#include <opencv2/opencv.hpp>
#include <deque>
#include <cstdio>

#include <QtConcurrent>

#include <algorithm>

#include <atomic>

#include <array>

#include <chrono>

#include <cmath>

#include <cstddef>

#include <cstdint>

#include <functional>

#include <limits>

#include <memory>

#include <string_view>

#include <opencv2/core.hpp>

#include <opencv2/imgproc.hpp>

#include <utility>

#include <vector>





module Artifact.Widgets.CompositionRenderController;

import Memory.SharedPtr;
import Core.ArtifactArray;
import Core.ArtifactTuple;
import Artifact.Layer.Construction;
import Artifact.Widgets.CompositionConstructionUndoCommand;

import Artifact.Render.IRenderer;
import Graphics.RayTracingManager;
import Graphics.SurfaceColorContract;
import Material.Material;
import Artifact.Widgets.ViewportOverlay;

import Artifact.Grid.System;
import ArtifactCore.Rig2D;

import Artifact.Render.GPUTextureCacheManager;

import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionMatteSurface;
import Artifact.Render.CompositionRenderer;

import Artifact.Render.Queue.Service;

import Artifact.Render.Config;

import Settings.Accessibility;

import Artifact.Render.ROI;

import Artifact.Render.Context;
import Analyze.SmartPalette;
import Color.Harmonizer;

import Artifact.Widgets.CompositionRenderOverlay;
import Artifact.Layer.Audio;
import Audio.Effect.Spectrum;

import Artifact.Preview.Pipeline;

import Core.Camera;

import Frame.Debug;

import Graphics.RenderGraph;

import Core.Diagnostics.Trace;

import Artifact.Composition.Abstract;

import Artifact.Layer.Abstract;
import Artifact.Layer.Particle;
import Artifact.Layer.MpmSand2D;
import Artifact.Layer.EnvironmentMap;
import Artifact.Layers.Abstract._2D;

import Artifact.Layer.Clone;

import Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Camera;

import Artifact.Layer.Light;

import Core.Light;

import Artifact.Effect.Abstract;

import Artifact.Effect.Context;
import Artifact.Effect.FrameSampler;


import Artifact.Layer.Image;

import Artifact.Layer.Svg;

import Artifact.Layer.Video;

import Video.VideoFrame;

import Artifact.Layer.Solid2D;

import Artifact.Layers.SolidImage;

import Artifact.Layer.Text;

import Artifact.Layer.Composition;
import Artifact.Layer.ParametricComposition;

import Artifact.Layer.Shape;

import Layer.Matte;

import Artifact.Layers.Model3D;

import Artifact.Render.Offscreen;

import Image.ImageF32x4_RGBA;
import Image.Cryptomatte.Pixel;

import FloatRGBA;

import Frame.Position;

import Artifact.Application.Manager;

import Artifact.Layers.Selection.Manager;

import Artifact.Service.ActiveContext;

import Artifact.Widgets.TransformGizmo;

import Artifact.Widgets.TextGizmo;

import Artifact.Widgets.ContentGizmo;

import Artifact.Widgets.Gizmo3D;

import Artifact.Widgets.ViewportMath;

import Artifact.Widgets.PointTrackerGizmo;

import Artifact.Tool.PointTracker;

import Tracking.MotionTracker;

import Artifact.Render.OffscreenComposition;
import Artifact.Render.PointwiseEffectFusion;
import Artifact.Widgets.CompositionHostFrameSnapshot;

import ExposureEffect;
import HueAndSaturation;
import LevelsEffect;
import BrightnessEffect;
import InvertEffect;
import GrayscaleEffect;
import Artifact.Effect.WhiteBalance;

import Artifact.Widgets.PieMenu;

import UI.View.Orientation.Navigator;

import Geometry.CameraGuide;

import Math.Interpolate;

import Artifact.Tool.Manager;

import Artifact.Tool.MotionSketchTool;

import Artifact.Tool.PuppetTool;

import Artifact.Tool.Brush;

import Artifact.Layer.Paint;

import Artifact.Mask.LayerMask;

import Artifact.Mask.Path;

import Artifact.Project.Health;

import Utils.Id;

import Time.Rational;

import Artifact.Render.Pipeline;
import :ChannelPresentation;
import :Impl;
import :GizmoEditing;
import :LayerDrawing;
import :MaskEditing;
import :RasterEffects;
import :ViewportGeometry;
import Artifact.Render.MotionBlurPass;
import Artifact.Render.DepthOfFieldPass;
import Artifact.Render.FinalPostProcess;
import Artifact.Color.OCIOManager;
import Artifact.Widgets.ViewportColorPipeline;

import Graphics.LayerBlendPipeline;
import Container.NamedVector;

import Graphics.GPUcomputeContext;

import Graphics.Shader.Compute.MaskCutout;

import Widgets.Utils.CSS;

import Core.Diagnostics.Trace;



import Artifact.Service.Project;
import Artifact.Project.Items;
import Artifact.Layer.InitParams;

import Artifact.Service.Playback; // 追加

import Application.AppSettings;

import Playback.State;

import Thread.Helper;

import Thread.PreciseTicker;

import Event.Bus;

import Artifact.Event.Types;

import Undo.UndoManager;
import Utils.String.UniString;
import Configuration.LayeredConfigStore;
import Configuration.ConfigLayer;

import Frame.Position;
import Frame.Rate;
import Math.Vec;

import Color.Float;

import Image;

import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

import ArtifactCore.Utils.PerformanceProfiler;
import Artifact.Render.CompositionChangeDetector;
import Artifact.Render.CompositionPassState;
import Artifact.Render.CompositionRenderPassPlan;
import Artifact.Render.CompositionRenderPassContext;
import Artifact.Render.CompositionPassExecutor;
import Artifact.Widgets.CompositionMotionPathCommands;
import Artifact.Widgets.CompositionTrackerSessionUndoCommands;
import Artifact.Widgets.CompositionOnionSkinHistory;
import Artifact.Widgets.CompositionRigUndoCommands;
import Artifact.Widgets.CompositionTextPuppetUndoCommands;
import Artifact.Widgets.CompositionLayerUndoCommands;
import Artifact.Widgets.CompositionEditUndoCommands;
import Artifact.Widgets.CompositionGizmoUndoCommands;
import Artifact.Widgets.LayerEditor.Geometry;



namespace Artifact {

using namespace CompositionRenderControllerDetail;

bool CompositionRenderController::setSelectedLayerMotionPathKeyframeAtCurrentFrame() {

  if (!impl_) {

    return false;

  }



  const auto comp = impl_->previewPipeline_.composition();

  if (!comp || impl_->selectedLayerId_.isNil()) {

    return false;

  }



  const auto layer = comp->layerById(impl_->selectedLayerId_);

  if (!layer) {

    return false;

  }



  const FramePosition currentFrame = currentFrameForComposition(comp);

  const auto time = gizmoTransformTime(layer, currentFrame.framePosition());

  auto &t3d = layer->transform3D();



  MotionPathPositionSnapshot before;

  before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);

  before.position.x = t3d.positionXAt(time);

  before.position.y = t3d.positionYAt(time);



  t3d.setPositionKeyFrameValueAt(time, before.position.x, before.position.y);



  MotionPathPositionSnapshot after;

  after.hasPositionKey = t3d.hasPositionKeyFrameAt(time);

  after.position.x = t3d.positionXAt(time);

  after.position.y = t3d.positionYAt(time);



  const bool changed = before.hasPositionKey != after.hasPositionKey ||

                       std::abs(before.position.x - after.position.x) > 0.0001f ||

                       std::abs(before.position.y - after.position.y) > 0.0001f;

  if (!changed) {

    return false;

  }



  auto *mgr = UndoManager::instance();
  if (mgr && !mgr->push(std::make_unique<MotionPathUndoCommand>(
                            layer, time, before, after))) {
    if (before.hasPositionKey) {
      t3d.setPositionKeyFrameValueAt(time, before.position.x, before.position.y);
    } else {
      t3d.removePositionKeyFrameAt(time);
    }
    return false;
  }

  layer->setDirty(LayerDirtyFlag::Transform);

  layer->changed();

  if (auto *compPtr = static_cast<ArtifactAbstractComposition *>(layer->composition())) {

    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),

                          LayerChangedEvent::ChangeType::Modified});

  }

  impl_->motionPathCache_.valid = false;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

  return true;

}



bool CompositionRenderController::removeSelectedLayerMotionPathKeyframeAtCurrentFrame() {

  if (!impl_) {

    return false;

  }



  const auto comp = impl_->previewPipeline_.composition();

  if (!comp || impl_->selectedLayerId_.isNil()) {

    return false;

  }



  const auto layer = comp->layerById(impl_->selectedLayerId_);

  if (!layer) {

    return false;

  }



  const FramePosition currentFrame = currentFrameForComposition(comp);

  const auto time = gizmoTransformTime(layer, currentFrame.framePosition());

  auto &t3d = layer->transform3D();

  if (!t3d.hasPositionKeyFrameAt(time)) {

    return false;

  }



  MotionPathPositionSnapshot before;

  before.hasPositionKey = true;

  before.position.x = t3d.positionXAt(time);

  before.position.y = t3d.positionYAt(time);



  t3d.removePositionKeyFrameAt(time);



  MotionPathPositionSnapshot after;

  after.hasPositionKey = false;

  after.position.x = t3d.positionXAt(time);

  after.position.y = t3d.positionYAt(time);



  auto *mgr = UndoManager::instance();
  if (mgr && !mgr->push(std::make_unique<MotionPathUndoCommand>(
                            layer, time, before, after))) {
    t3d.setPositionKeyFrameValueAt(time, before.position.x, before.position.y);
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return false;
  }

  layer->setDirty(LayerDirtyFlag::Transform);

  layer->changed();

  if (auto *compPtr = static_cast<ArtifactAbstractComposition *>(layer->composition())) {

    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),

                          LayerChangedEvent::ChangeType::Modified});

  }

  impl_->motionPathCache_.valid = false;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

  return true;

}



bool CompositionRenderController::setSelectedLayerMotionPathInterpolationAtCurrentFrame(

    int interpolationType) {

  if (!impl_) {

    return false;

  }



  const auto comp = impl_->previewPipeline_.composition();

  if (!comp || impl_->selectedLayerId_.isNil()) {

    return false;

  }



  const auto layer = comp->layerById(impl_->selectedLayerId_);

  if (!layer) {

    return false;

  }



  const FramePosition currentFrame = currentFrameForComposition(comp);

  const auto time = gizmoTransformTime(layer, currentFrame.framePosition());

  auto &t3d = layer->transform3D();

  if (!t3d.hasPositionKeyFrameAt(time)) {

    return false;

  }



  MotionPathInterpolationSnapshot before;

  before.hasPositionKey = true;

  before.xInterpolation =

      static_cast<int>(t3d.positionXKeyFrameInterpolationAt(time));

  before.yInterpolation =

      static_cast<int>(t3d.positionYKeyFrameInterpolationAt(time));



  const auto interp =

      static_cast<ArtifactCore::InterpolationType>(interpolationType);

  if (before.xInterpolation == interpolationType &&

      before.yInterpolation == interpolationType) {

    return false;

  }



  t3d.setPositionKeyFrameInterpolationAt(time, interp, interp);



  MotionPathInterpolationSnapshot after;

  after.hasPositionKey = true;

  after.xInterpolation =

      static_cast<int>(t3d.positionXKeyFrameInterpolationAt(time));

  after.yInterpolation =

      static_cast<int>(t3d.positionYKeyFrameInterpolationAt(time));



  auto *mgr = UndoManager::instance();
  if (mgr && !mgr->push(std::make_unique<MotionPathInterpolationUndoCommand>(
                            layer, time, before, after))) {
    t3d.setPositionKeyFrameInterpolationAt(
        time,
        static_cast<ArtifactCore::InterpolationType>(before.xInterpolation),
        static_cast<ArtifactCore::InterpolationType>(before.yInterpolation));
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return false;
  }

  layer->setDirty(LayerDirtyFlag::Transform);

  layer->changed();

  if (auto *compPtr =

          static_cast<ArtifactAbstractComposition *>(layer->composition())) {

    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

        LayerChangedEvent{compPtr->id().toString(), layer->id().toString(),

                          LayerChangedEvent::ChangeType::Modified});

  }

  impl_->motionPathCache_.valid = false;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

  return true;

}

void CompositionRenderController::Impl::renderMotionPathOverlayForLayer(
    const ArtifactAbstractLayerPtr &layer, const ArtifactCompositionPtr &comp,
    int currentFrameNum, float zoom, float invZoom,
    const LayerID &selectedLayerId, const QVector<LayerID> &selectedIds,
    bool hasSelectedIds, quint64 overlayInvalidationSerial,
    bool useSelectedIdFilter) {
  if (!layer || !comp || !renderer_) {
    return;
  }

  if (hasSelectedIds) {
    if (!selectedIds.contains(layer->id())) {
      return;
    }
  } else if (useSelectedIdFilter && (selectedLayerId.isNil() ||
                                     layer->id() != selectedLayerId)) {
    return;
  }

  const auto &t3d = layer->transform3D();
  const QString layerCacheKey = comp->id().toString() + QLatin1Char(':') +
                                layer->id().toString();
  const int motionPathFps =
      std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));
  const auto posTimes = motionPathPositionKeyTimes(layer, motionPathFps);
  if (posTimes.empty()) {
    motionPathCache_.valid = false;
    motionPathCacheByLayer_.remove(layerCacheKey);
    return;
  }

  const int keyMinFrame = static_cast<int>(posTimes.front().value());
  const int keyMaxFrame = static_cast<int>(posTimes.back().value());
  const int minFrame = keyMinFrame;
  const int maxFrame = keyMaxFrame;
  if (minFrame > maxFrame) {
    return;
  }

  const int64_t rate = posTimes.front().scale();

  // Camera layers need a world-space path. The standard path overlay below is
  // intentionally 2D so it can offer handle editing for regular layers; using
  // it for a camera makes depth movement appear flattened into the canvas.
  if (dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
    using namespace Artifact::Detail;
    const auto cameraPositionAt = [&t3d, rate](const int frame) {
      const ArtifactCore::RationalTime time(frame, rate);
      return QVector3D(static_cast<float>(t3d.positionXAt(time)),
                       static_cast<float>(t3d.positionYAt(time)),
                       static_cast<float>(t3d.positionZAt(time)));
    };
    const float lineThickness = std::max(1.0f, 1.6f * invZoom);
    const float markerSize = std::max(3.0f, 7.0f * invZoom);
    const int sampleStep = motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
    const FloatColor pastPathColor{1.0f, 0.60f, 0.16f, 0.95f};
    const FloatColor futurePathColor{0.38f, 0.82f, 1.0f, 0.92f};

    QVector3D previous = cameraPositionAt(minFrame);
    int previousFrame = minFrame;
    for (int frame = minFrame + sampleStep;; frame += sampleStep) {
      const int nextFrame = std::min(frame, maxFrame);
      const QVector3D current = cameraPositionAt(nextFrame);
      const FloatColor color = (previousFrame + nextFrame) <= currentFrameNum * 2
          ? pastPathColor
          : futurePathColor;
      renderer_->drawGizmoLine(float3{previous.x(), previous.y(), previous.z()},
                               float3{current.x(), current.y(), current.z()},
                               color, lineThickness);
      if (nextFrame == maxFrame) {
        break;
      }
      previous = current;
      previousFrame = nextFrame;
    }

    for (const auto &keyTime : posTimes) {
      const int frame = static_cast<int>(keyTime.value());
      const QVector3D keyPosition = cameraPositionAt(frame);
      const bool isCurrent = frame == currentFrameNum;
      renderer_->drawGizmoCube(
          float3{keyPosition.x(), keyPosition.y(), keyPosition.z()},
          isCurrent ? markerSize * 0.72f : markerSize * 0.45f,
          isCurrent ? FloatColor{1.0f, 0.92f, 0.30f, 1.0f}
                    : FloatColor{0.95f, 0.72f, 0.30f, 0.90f});
    }

    if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
      const QVector3D current = cameraPositionAt(currentFrameNum);
      renderer_->drawGizmoRing(float3{current.x(), current.y(), current.z()},
                               float3{0.0f, 1.0f, 0.0f}, markerSize * 1.25f,
                               FloatColor{1.0f, 0.96f, 0.38f, 0.95f}, 1.4f);
    }
    return;
  }

  const FloatColor pastPathColor{0.96f, 0.46f, 0.72f, 0.96f};
  const FloatColor futurePathColor{0.42f, 0.76f, 1.0f, 0.92f};
  const FloatColor pathShadowColor{0.0f, 0.0f, 0.0f, 0.52f};
  const float lineThickness = std::max(1.0f, 1.5f * invZoom);
  const float dotRadius = std::max(1.5f, 2.5f * invZoom);
  const bool hasPathSegment = posTimes.size() >= 2;
  const auto cachedLayer = motionPathCacheByLayer_.constFind(layerCacheKey);
  if (cachedLayer != motionPathCacheByLayer_.cend()) {
    motionPathCache_ = cachedLayer.value();
  } else {
    motionPathCache_.valid = false;
  }
  const bool cacheHit =
      motionPathCache_.valid &&
      motionPathCache_.layerId == layer->id() &&
      motionPathCache_.framePos == static_cast<int64_t>(currentFrameNum) &&
      std::abs(motionPathCache_.zoom - zoom) <= 0.00001f &&
      motionPathCache_.overlaySerial == overlayInvalidationSerial;

  if (!cacheHit) {
    motionPathCache_.valid = false;
    motionPathCache_.layerId = layer->id();
    motionPathCache_.framePos = static_cast<int64_t>(currentFrameNum);
    motionPathCache_.zoom = zoom;
    motionPathCache_.overlaySerial = overlayInvalidationSerial;
    motionPathCache_.pathPoints.clear();
    motionPathCache_.timeDotPoints.clear();
    motionPathCache_.keyPoints.clear();

    if (hasPathSegment) {
      const int sampleStep =
          motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
      const float curvatureTolerance = std::max(0.75f, 1.5f * invZoom);
      constexpr size_t kMaxCurveSamples = 2048;
      const auto evaluatePoint = [&](int frame) {
        const ArtifactCore::RationalTime t(frame, rate);
        const QTransform transform = layer->getGlobalTransformAt(frame);
        const QPointF position = transform.map(
            QPointF(t3d.anchorXAt(t), t3d.anchorYAt(t)));
        return MotionPathCacheEntry::Pt{frame, static_cast<float>(position.x()),
                                        static_cast<float>(position.y())};
      };
      const auto appendAdaptiveSegment =
          [&](auto &&self, const MotionPathCacheEntry::Pt &start,
              const MotionPathCacheEntry::Pt &end, int depth) -> void {
        if (motionPathCache_.pathPoints.size() >= kMaxCurveSamples) {
          // Preserve the segment endpoint without growing the cache. The final
          // retained segment becomes a coarse fallback once the safety cap is
          // reached.
          motionPathCache_.pathPoints.back() = end;
          return;
        }
        const int frameSpan = end.frame - start.frame;
        if (frameSpan <= 1 || depth >= 7) {
          motionPathCache_.pathPoints.push_back(end);
          return;
        }

        const int midpointFrame = start.frame + frameSpan / 2;
        const auto midpoint = evaluatePoint(midpointFrame);
        const float chordMidX = (start.x + end.x) * 0.5f;
        const float chordMidY = (start.y + end.y) * 0.5f;
        const float deviation = std::hypot(midpoint.x - chordMidX,
                                           midpoint.y - chordMidY);
        if (deviation <= curvatureTolerance) {
          motionPathCache_.pathPoints.push_back(end);
          return;
        }
        self(self, start, midpoint, depth + 1);
        self(self, midpoint, end, depth + 1);
      };

      auto previous = evaluatePoint(minFrame);
      motionPathCache_.pathPoints.push_back(previous);
      for (int frame = minFrame + sampleStep;; frame += sampleStep) {
        const int nextFrame = std::min(frame, maxFrame);
        const auto next = evaluatePoint(nextFrame);
        appendAdaptiveSegment(appendAdaptiveSegment, previous, next, 0);
        if (nextFrame == maxFrame) {
          break;
        }
        previous = next;
      }

      constexpr int kTargetVelocityDots = 72;
      const int64_t frameSpan = static_cast<int64_t>(maxFrame) - minFrame;
      const int timeDotStep = static_cast<int>(std::max<int64_t>(
          1, (frameSpan + kTargetVelocityDots - 1) / kTargetVelocityDots));
      for (int frame = minFrame;; frame += timeDotStep) {
        motionPathCache_.timeDotPoints.push_back(evaluatePoint(frame));
        if (frame >= maxFrame - timeDotStep) {
          if (frame != maxFrame) {
            motionPathCache_.timeDotPoints.push_back(evaluatePoint(maxFrame));
          }
          break;
        }
      }
    }

    for (const auto &kfTime : posTimes) {
      const int f = static_cast<int>(kfTime.value());
      if (f < keyMinFrame || f > keyMaxFrame) {
        continue;
      }
      const QTransform gTrans = layer->getGlobalTransformAt(f);
      const QPointF wPos =
          gTrans.map(QPointF(t3d.anchorXAt(kfTime), t3d.anchorYAt(kfTime)));
      MotionPathCacheEntry::Pt pt;
      pt.frame = f;
      pt.x = static_cast<float>(wPos.x());
      pt.y = static_cast<float>(wPos.y());
      pt.interpolation = motionPathPositionInterpolation(layer, kfTime);
      ArtifactCore::PositionSpatialTangents tangents;
      if (t3d.positionKeyFrameSpatialTangentsAt(kfTime, tangents)) {
        QTransform tangentTransform;
        if (const auto parent = layer->parentLayer()) {
          tangentTransform = parent->getGlobalTransformAt(f);
        }
        const QPointF tangentOrigin = tangentTransform.map(QPointF(0.0, 0.0));
        const auto mapTangent = [&](const auto &tangent) {
          const QPointF transformed = tangentTransform.map(
              QPointF(tangent.x, tangent.y));
          return wPos + (transformed - tangentOrigin);
        };
        const QPointF inHandle = mapTangent(tangents.inTangent);
        const QPointF outHandle = mapTangent(tangents.outTangent);
        pt.inTangentX = static_cast<float>(inHandle.x());
        pt.inTangentY = static_cast<float>(inHandle.y());
        pt.outTangentX = static_cast<float>(outHandle.x());
        pt.outTangentY = static_cast<float>(outHandle.y());
        pt.hasSpatialTangents = true;
        pt.tangentsLinked = tangents.linked;
      }
      motionPathCache_.keyPoints.push_back(pt);
    }
    motionPathCache_.valid = true;
    motionPathCacheByLayer_.insert(layerCacheKey, motionPathCache_);
  }

  if (viewportOrientationMatricesValid_) {
    renderer_->setViewMatrix(viewportOrientationViewForOverlay_);
    renderer_->setProjectionMatrix(viewportOrientationProjectionForOverlay_);
    renderer_->setUseExternalMatrices(true);
  }

  if (layer->id() == selectedLayerId) {
    for (const auto frame : historicalBoundsFrames(layer, currentFrameNum, motionPathFps)) {
      const auto bounds = layer->localBounds();
      const auto matrix = layer->getGlobalTransformAt(frame);
      const QPointF corners[4] = {matrix.map(bounds.topLeft()),
          matrix.map(bounds.topRight()), matrix.map(bounds.bottomRight()),
          matrix.map(bounds.bottomLeft())};
      const bool active = selectedMotionPathFrames_.contains(frame) ||
          (historicalScaleDragging_ && historicalScaleFrame_ == frame);
      const FloatColor color = active ? FloatColor{1.0f, 0.72f, 0.25f, 0.95f}
                                     : FloatColor{0.58f, 0.65f, 0.73f, 0.55f};
      for (int i = 0; i < 4; ++i) {
        const auto &a = corners[i];
        const auto &b = corners[(i + 1) % 4];
        renderer_->drawDashedLineLocal(
            {static_cast<float>(a.x()), static_cast<float>(a.y())},
            {static_cast<float>(b.x()), static_cast<float>(b.y())},
            std::max(1.0f, invZoom), 2.0f * invZoom, 4.0f * invZoom, color);
        renderer_->drawPoint(static_cast<float>(a.x()), static_cast<float>(a.y()),
                             4.0f * invZoom, color);
      }
    }
  }
  if (!motionPathCache_.pathPoints.empty()) {
    Detail::float2 lastPos;
    int lastFrame = 0;
    bool hasLastPos = false;
    for (const auto &pt : motionPathCache_.pathPoints) {
      Detail::float2 currentPos(pt.x, pt.y);
      if (hasLastPos) {
        const bool isPastSegment =
            (lastFrame + pt.frame) <= currentFrameNum * 2;
        const FloatColor segmentColor =
            isPastSegment ? pastPathColor : futurePathColor;
        drawTaggedSolidLine(renderer_.get(), {lastPos.x, lastPos.y},
                            {currentPos.x, currentPos.y}, pathShadowColor,
                            lineThickness + std::max(1.5f, 2.0f * invZoom),
                            true);
        drawTaggedSolidLine(renderer_.get(), {lastPos.x, lastPos.y},
                            {currentPos.x, currentPos.y}, segmentColor,
                            lineThickness, true);
      }
      lastPos = currentPos;
      lastFrame = pt.frame;
      hasLastPos = true;
    }
    for (const auto &dot : motionPathCache_.timeDotPoints) {
      const FloatColor dotColor =
          dot.frame <= currentFrameNum
              ? FloatColor{1.0f, 0.72f, 0.86f, 0.86f}
              : FloatColor{0.68f, 0.88f, 1.0f, 0.82f};
      renderer_->drawPoint(dot.x, dot.y, dotRadius, pathShadowColor);
      renderer_->drawPoint(dot.x, dot.y, dotRadius * 0.53f, dotColor);
    }
    if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
      const ArtifactCore::RationalTime currentTime(
          static_cast<int64_t>(currentFrameNum), rate);
      const QTransform currentTransform =
          layer->getGlobalTransformAt(currentFrameNum);
      const QPointF currentPosition = currentTransform.map(
          QPointF(t3d.anchorXAt(currentTime), t3d.anchorYAt(currentTime)));
      renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                 static_cast<float>(currentPosition.y()),
                                 dotRadius * 2.0f,
                                 {0.12f, 0.10f, 0.02f, 0.85f});
      renderer_->drawPoint(static_cast<float>(currentPosition.x()),
                                 static_cast<float>(currentPosition.y()),
                                 dotRadius * 1.25f,
                                 {0.98f, 0.88f, 0.35f, 1.0f});
    }
  }
  for (const auto &pt : motionPathCache_.keyPoints) {
    const bool isCurrent = pt.frame == currentFrameNum;
    const bool isHovered = pt.frame == hoveredMotionPathFrame_;
    const bool isSelected = selectedMotionPathFrames_.contains(pt.frame);
    const FloatColor keyColor =
        motionPathInterpolationColor(pt.interpolation, isCurrent);
    if (pt.hasSpatialTangents) {
      const FloatColor tangentLine =
          pt.tangentsLinked ? FloatColor{0.50f, 0.88f, 1.0f,
                                         isCurrent ? 0.92f : 0.58f}
                            : FloatColor{1.0f, 0.60f, 0.28f,
                                         isCurrent ? 0.96f : 0.72f};
      const FloatColor tangentHandle =
          pt.tangentsLinked ? FloatColor{0.76f, 0.95f, 1.0f,
                                         isCurrent ? 1.0f : 0.84f}
                            : FloatColor{1.0f, 0.78f, 0.42f,
                                         isCurrent ? 1.0f : 0.90f};
      const float handleRadius = isCurrent ? dotRadius * 1.15f : dotRadius;
      const float hoveredHandleRadius = dotRadius * 1.65f;
      drawTaggedSolidLine(renderer_.get(), {pt.x, pt.y},
                          {pt.inTangentX, pt.inTangentY}, tangentLine,
                          std::max(1.0f, invZoom), true);
      drawTaggedSolidLine(renderer_.get(), {pt.x, pt.y},
                          {pt.outTangentX, pt.outTangentY}, tangentLine,
                          std::max(1.0f, invZoom), true);
      renderer_->drawPoint(
          pt.inTangentX, pt.inTangentY,
          pt.frame == hoveredMotionPathFrame_ &&
                  hoveredMotionPathTangentHandle_ == MotionPathTangentHandle::In
              ? hoveredHandleRadius
              : handleRadius,
                           tangentHandle);
      renderer_->drawPoint(
          pt.outTangentX, pt.outTangentY,
          pt.frame == hoveredMotionPathFrame_ &&
                  hoveredMotionPathTangentHandle_ == MotionPathTangentHandle::Out
              ? hoveredHandleRadius
              : handleRadius,
                           tangentHandle);
    }
    const float outerRadius =
        isHovered ? dotRadius * 3.4f : dotRadius * 2.8f;
    const float innerRadius =
        isHovered ? dotRadius * 2.0f : dotRadius * 1.65f;
    const FloatColor ringColor =
        isSelected ? FloatColor{1.0f, 0.95f, 0.25f, 1.0f}
        : isHovered ? FloatColor{1.0f, 1.0f, 1.0f, 0.95f}
                  : FloatColor{0.96f, 0.96f, 1.0f, 0.92f};
    renderer_->drawPoint(pt.x, pt.y, outerRadius, ringColor);
    renderer_->drawPoint(pt.x, pt.y, innerRadius, keyColor);
  }
  if (selectedMotionPathFrames_.size() > 1) {
    QRectF selectionBounds;
    bool hasSelectionBounds = false;
    for (const auto &pt : motionPathCache_.keyPoints) {
      if (!selectedMotionPathFrames_.contains(pt.frame)) continue;
      const QPointF point(pt.x, pt.y);
      if (!hasSelectionBounds) {
        selectionBounds = QRectF(point, QSizeF(0.0, 0.0));
        hasSelectionBounds = true;
      } else {
        selectionBounds |= QRectF(point, QSizeF(0.0, 0.0));
      }
    }
    if (hasSelectionBounds) {
      const qreal padding = std::max<qreal>(10.0, 12.0 * invZoom);
      selectionBounds.adjust(-padding, -padding, padding, padding);
      const FloatColor selectionBoxColor{1.0f, 0.82f, 0.22f, 0.72f};
      const QPointF topLeft = selectionBounds.topLeft();
      const QPointF topRight = selectionBounds.topRight();
      const QPointF bottomLeft = selectionBounds.bottomLeft();
      const QPointF bottomRight = selectionBounds.bottomRight();
      renderer_->drawDashedLineLocal(
          {static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y())},
          {static_cast<float>(topRight.x()), static_cast<float>(topRight.y())},
          std::max(1.0f, invZoom), 5.0f, 3.0f, selectionBoxColor);
      renderer_->drawDashedLineLocal(
          {static_cast<float>(topRight.x()), static_cast<float>(topRight.y())},
          {static_cast<float>(bottomRight.x()),
           static_cast<float>(bottomRight.y())},
          std::max(1.0f, invZoom), 5.0f, 3.0f, selectionBoxColor);
      renderer_->drawDashedLineLocal(
          {static_cast<float>(bottomRight.x()),
           static_cast<float>(bottomRight.y())},
          {static_cast<float>(bottomLeft.x()), static_cast<float>(bottomLeft.y())},
          std::max(1.0f, invZoom), 5.0f, 3.0f, selectionBoxColor);
      renderer_->drawDashedLineLocal(
          {static_cast<float>(bottomLeft.x()), static_cast<float>(bottomLeft.y())},
          {static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y())},
          std::max(1.0f, invZoom), 5.0f, 3.0f, selectionBoxColor);
      const QPointF pivot = selectionBounds.center();
      const float pivotRadius = std::max(5.0f, 7.0f * invZoom);
      const FloatColor pivotColor{1.0f, 0.92f, 0.30f, 0.95f};
      renderer_->drawSolidLine(
          {static_cast<float>(pivot.x() - pivotRadius),
           static_cast<float>(pivot.y())},
          {static_cast<float>(pivot.x() + pivotRadius),
           static_cast<float>(pivot.y())},
          pivotColor, std::max(1.0f, invZoom));
      renderer_->drawSolidLine(
          {static_cast<float>(pivot.x()),
           static_cast<float>(pivot.y() - pivotRadius)},
          {static_cast<float>(pivot.x()),
           static_cast<float>(pivot.y() + pivotRadius)},
          pivotColor, std::max(1.0f, invZoom));
    }
  }
  // Keep frame labels sparse: only the active selection or hovered key gets
  // a viewport-space label, which preserves path readability at high key
  // counts while making the editing target unambiguous.
  if (!selectedMotionPathFrames_.isEmpty() ||
      hoveredMotionPathFrame_ >= 0) {
    renderer_->setUseExternalMatrices(false);
    const QFont frameLabelFont = fixedWidthFont(8);
    const FloatColor frameLabelColor{0.96f, 0.98f, 1.0f, 0.92f};
    for (const auto &pt : motionPathCache_.keyPoints) {
      if (!selectedMotionPathFrames_.contains(pt.frame) &&
          pt.frame != hoveredMotionPathFrame_) {
        continue;
      }
      const auto viewportPoint = renderer_->canvasToViewport({pt.x, pt.y});
      renderer_->drawText(
          QRectF(viewportPoint.x + 6.0f, viewportPoint.y - 18.0f, 52.0f,
                 16.0f),
          QStringLiteral("F%1").arg(pt.frame), frameLabelFont,
          frameLabelColor, Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
  if (viewportOrientationMatricesValid_) {
    renderer_->setUseExternalMatrices(false);
  }
}

} // namespace Artifact
