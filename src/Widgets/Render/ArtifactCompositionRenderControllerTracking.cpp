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

void CompositionRenderController::trackerInitialize() {

  if (!impl_ || trackerJobRunning() ||
      !impl_->previewPipeline_.composition()) {
    return;
  }

  auto* previousTracker = impl_->trackerMotionTracker_;
  int previousTrackerId = impl_->trackerMotionTrackerId_;
  auto& trackerManager = ArtifactCore::TrackerManager::instance();
  if (previousTrackerId <= 0 ||
      trackerManager.tracker(previousTrackerId) != previousTracker) {
    previousTracker = nullptr;
    previousTrackerId = 0;
  }

  if (previousTracker && impl_->trackerGizmo_) {
    const auto comp = impl_->previewPipeline_.composition();
    if (comp) {
      persistProjectTrackerGeometry(
          comp->id(), previousTrackerId,
          impl_->trackerGizmo_->state());
    }
  }

  auto* tm = ArtifactCore::TrackerManager::instance().createTracker(

      QStringLiteral("Point Tracker"));

  impl_->trackerMotionTracker_ = tm;
  impl_->trackerMotionTrackerId_ = tm ? tm->id() : 0;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;

  if (impl_->trackerMotionTracker_) {
    impl_->trackerMotionTracker_->setTrackerType(
        ArtifactCore::TrackerType::Point);
    if (const auto comp = impl_->previewPipeline_.composition()) {
      const int trackerId = impl_->trackerMotionTrackerId_;
      registerProjectTrackerSession(comp->id(),
                                    trackerId);
      if (impl_->trackerGizmo_) {
        persistProjectTrackerGeometry(
            comp->id(), trackerId,
            impl_->trackerGizmo_->state());
      }
      const QJsonObject manifest = projectTrackerSessionManifest(comp->id());
      const QJsonObject geometry = manifest
          .value(QStringLiteral("geometryByTracker"))
          .toObject()
          .value(QString::number(trackerId))
          .toObject();
      bool creationRecorded = true;
      if (auto* undo = UndoManager::instance()) {
        creationRecorded = undo->push(std::make_unique<TrackerSessionUndoCommand>(
            comp->id(), trackerId, impl_->trackerMotionTracker_->toJson(),
            geometry,
            projectTrackerSourceLayerId(comp->id(), trackerId), true,
            previousTrackerId));
      }
      if (!creationRecorded) {
        removeProjectTrackerSession(comp->id(), trackerId);
        ArtifactCore::TrackerManager::instance().removeTracker(trackerId);
        if (previousTracker &&
            ArtifactCore::TrackerManager::instance().tracker(
                previousTrackerId) == previousTracker) {
          setProjectActiveTrackerSession(comp->id(), previousTrackerId);
          impl_->trackerMotionTracker_ = previousTracker;
          impl_->trackerMotionTrackerId_ = previousTrackerId;
        } else {
          impl_->trackerMotionTracker_ = nullptr;
          impl_->trackerMotionTrackerId_ = 0;
        }
        tm = nullptr;
        setInfoOverlayText(QStringLiteral("Tracker"),
                           QStringLiteral("Could not record tracker creation in Undo history"));
      }
    }
  }

  if (impl_->trackerGizmo_) {

    impl_->trackerGizmo_->setTracker(impl_->trackerMotionTracker_);

  }

}

void CompositionRenderController::trackerUsePointMode() {
  if (trackerJobRunning()) return;
  if (!impl_->trackerMotionTracker_) {
    trackerInitialize();
  }
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerLastJobFailed_ = false;
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->setTrackerType(ArtifactCore::TrackerType::Point);
  impl_->trackerMotionTracker_->clearTrackingData();
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(impl_->trackerMotionTracker_);
  }
  setInfoOverlayText(QStringLiteral("Point Tracker"),
                     QStringLiteral("Set the feature point and search region"));
  markRenderDirty();
}

void CompositionRenderController::trackerUsePlanarMode() {
  if (trackerJobRunning()) return;
  if (!impl_->trackerMotionTracker_) {
    trackerInitialize();
  }
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerLastJobFailed_ = false;
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->setTrackerType(ArtifactCore::TrackerType::Planar);
  impl_->trackerMotionTracker_->clearTrackingData();
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(impl_->trackerMotionTracker_);
  }
  setInfoOverlayText(QStringLiteral("Planar Tracker"),
                     QStringLiteral("Planar mode enabled; track the four-corner region"));
  markRenderDirty();
}

int CompositionRenderController::trackerTrackPointCount() const {
  return impl_ && impl_->trackerMotionTracker_ &&
                 impl_->trackerMotionTracker_->trackerType() ==
                     ArtifactCore::TrackerType::Point
             ? impl_->trackerMotionTracker_->trackPointCount()
             : 0;
}

bool CompositionRenderController::trackerPointPlacementArmed() const {
  return impl_ && impl_->trackerPointPlacementArmed_;
}

void CompositionRenderController::trackerTogglePointPlacement() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      impl_->trackerMotionTracker_->trackerType() !=
          ArtifactCore::TrackerType::Point ||
      impl_->trackerMotionTracker_->hasResult()) {
    return;
  }
  impl_->trackerPointPlacementArmed_ = !impl_->trackerPointPlacementArmed_;
  markRenderDirty();
}

void CompositionRenderController::trackerRemoveLastPoint() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      impl_->trackerMotionTracker_->trackerType() !=
          ArtifactCore::TrackerType::Point ||
      impl_->trackerMotionTracker_->hasResult()) {
    return;
  }
  const int pointCount = impl_->trackerMotionTracker_->trackPointCount();
  if (pointCount <= 0) return;
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  const auto point = impl_->trackerMotionTracker_->trackPoint(pointCount - 1);
  impl_->trackerMotionTracker_->removeTrackPoint(point.id);
  impl_->trackerPointPlacementArmed_ = false;
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  markRenderDirty();
}

// Pushes an undo entry for a tracker mutation that already happened. `cleared`
// selects the post-state JSON because reset produces an empty tracker.
void CompositionRenderController::pushTrackerUndo(
    const QString& previousJson, const QString& currentJson) {
  if (!impl_ || !impl_->trackerMotionTracker_) return;
  const QString previous = previousJson.isEmpty()
                               ? impl_->trackerMotionTracker_->toJson()
                               : previousJson;
  if (previous.isEmpty() || previous == currentJson) return;
  if (auto* undo = UndoManager::instance()) {
    undo->push(std::make_unique<TrackerResultCommand>(
        impl_->trackerMotionTracker_->id(), previous, currentJson, true));
  }
}

void CompositionRenderController::trackerReset() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerMotionTracker_->stopTracking();
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->clearTrackingData();
  impl_->trackerMotionTracker_->clearTrackPoints();
  impl_->trackerMotionTracker_->clearTrackRegions();
  pushTrackerUndo(previousJson, QString());
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(impl_->trackerMotionTracker_);
  }
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Track points, regions, and results cleared"));
  markRenderDirty();
}



static void ensureOffscreenRenderer(

    std::unique_ptr<Artifact::OffscreenCompositionRenderer>& renderer,

    ArtifactIRenderer* mainRenderer, int width, int height)

{

  if (!renderer && mainRenderer) {

    auto device = mainRenderer->device();
    auto context = mainRenderer->immediateContext();

    if (device && context) {

      renderer = std::make_unique<Artifact::OffscreenCompositionRenderer>(

          device, context, static_cast<unsigned int>(width),
          static_cast<unsigned int>(height));

    }

  }

}

void CompositionRenderController::trackerTrackForward() {
  if (!impl_ || trackerJobRunning()) return;
  if (!impl_->trackerMotionTracker_) trackerInitialize();
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::Forward;
  trackerCaptureNextFrame(++impl_->trackerJobGeneration_);
}



void CompositionRenderController::trackerTrackBackward() {
  if (!impl_ || trackerJobRunning()) return;
  if (!impl_->trackerMotionTracker_) trackerInitialize();
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::Backward;
  trackerCaptureNextFrame(++impl_->trackerJobGeneration_);
}


void CompositionRenderController::trackerTrackStepForward() {
  if (!impl_ || trackerJobRunning()) return;
  const auto comp = impl_->previewPipeline_.composition();
  auto* playback = ArtifactPlaybackService::instance();
  if (!comp || !playback || playback->currentComposition() != comp ||
      playback->isPlaying()) return;
  if (!impl_->trackerMotionTracker_) trackerInitialize();
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::StepForward;
  trackerCaptureNextFrame(++impl_->trackerJobGeneration_);
}


void CompositionRenderController::trackerTrackStepBackward() {
  if (!impl_ || trackerJobRunning()) return;
  const auto comp = impl_->previewPipeline_.composition();
  auto* playback = ArtifactPlaybackService::instance();
  if (!comp || !playback || playback->currentComposition() != comp ||
      playback->isPlaying()) return;
  if (!impl_->trackerMotionTracker_) trackerInitialize();
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::StepBackward;
  trackerCaptureNextFrame(++impl_->trackerJobGeneration_);
}



void CompositionRenderController::trackerTrackAll() {
  if (!impl_ || trackerJobRunning()) return;
  if (!impl_->trackerMotionTracker_) trackerInitialize();
  if (!impl_->trackerMotionTracker_) return;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::All;
  trackerCaptureNextFrame(++impl_->trackerJobGeneration_);
}

bool CompositionRenderController::trackerJobRunning() const {
  return impl_ &&
         impl_->trackerJobDirection_ != Impl::TrackerJobDirection::None;
}

bool CompositionRenderController::trackerCancellationRequested() const {
  return impl_ &&
         impl_->trackerCancelRequested_.load(std::memory_order_acquire);
}

bool CompositionRenderController::trackerHasSession() const {
  if (!impl_ || !impl_->trackerMotionTracker_) return false;
  auto* registered = ArtifactCore::TrackerManager::instance().tracker(
      impl_->trackerMotionTrackerId_);
  if (registered == impl_->trackerMotionTracker_) return true;
  // Project loading replaces TrackerManager's objects. Drop the stale raw
  // binding before a UI refresh asks for tracker data through it.
  impl_->trackerMotionTracker_ = nullptr;
  impl_->trackerMotionTrackerId_ = 0;
  impl_->trackerPointPlacementArmed_ = false;
  if (impl_->trackerGizmo_) impl_->trackerGizmo_->setTracker(nullptr);
  return false;
}

bool CompositionRenderController::trackerLastJobFailed() const {
  return impl_ && impl_->trackerLastJobFailed_;
}

double CompositionRenderController::trackerSolveProgress() const {
  return impl_ ? std::clamp(
                     impl_->trackerSolveProgress_.load(std::memory_order_acquire),
                     0.0, 1.0)
               : 0.0;
}

double CompositionRenderController::trackerOverallProgress() const {
  if (!impl_) {
    return 0.0;
  }
  if (impl_->trackerCaptureActive_ && impl_->trackerCaptureFrameCount_ > 0) {
    const double captureProgress =
        static_cast<double>(impl_->trackerCapturedFrameCount_) /
        static_cast<double>(impl_->trackerCaptureFrameCount_);
    return std::clamp(captureProgress * 0.5, 0.0, 0.5);
  }
  if (impl_->trackerJobDirection_ != Impl::TrackerJobDirection::None) {
    const double solveProgress = trackerSolveProgress();
    return std::clamp(0.5 + solveProgress * 0.5, 0.5, 1.0);
  }
  return trackerSolveProgress();
}

QString CompositionRenderController::trackerModeLabel() const {
  return trackerModeTitle(impl_ ? impl_->trackerMotionTracker_ : nullptr);
}

double CompositionRenderController::trackerAverageConfidence() const {
  return impl_ && impl_->trackerMotionTracker_
             ? impl_->trackerMotionTracker_->averageConfidence()
             : 0.0;
}

int CompositionRenderController::trackerProblemFrameCount() const {
  return impl_ && impl_->trackerMotionTracker_
             ? static_cast<int>(impl_->trackerMotionTracker_->problemFrames().size())
             : 0;
}

int CompositionRenderController::trackerConfidenceSamples(
    float* confidenceValues, float* timeFractions, bool* breakBefore,
    int capacity) const {
  if (!confidenceValues || !timeFractions || !breakBefore || capacity <= 0 || !impl_ ||
      !impl_->trackerMotionTracker_) {
    return 0;
  }
  const auto& frames = impl_->trackerMotionTracker_->resultRef().frames;
  if (frames.empty()) return 0;
  const int sampleCount = static_cast<int>(
      std::min<std::size_t>(frames.size(), static_cast<std::size_t>(capacity)));
  const std::size_t last = frames.size() - 1;
  const double firstTime = frames.front().time;
  const double lastTime = frames.back().time;
  const double timeSpan = lastTime - firstTime;
  const auto composition = impl_->previewPipeline_.composition();
  const double fps = composition ? composition->frameRate().framerate() : 0.0;
  const double expectedStep = sampleCount > 1
                                  ? timeSpan / (sampleCount - 1)
                                  : 0.0;
  const double gapThreshold = expectedStep +
                              (std::isfinite(fps) && fps > 0.0
                                   ? 0.5 / fps
                                   : expectedStep * 0.25);
  double previousTime = firstTime;
  for (int i = 0; i < sampleCount; ++i) {
    const std::size_t index = sampleCount == 1
                                  ? last
                                  : (static_cast<std::size_t>(i) * last) /
                                        static_cast<std::size_t>(sampleCount - 1);
    const double confidence = frames[index].overallConfidence;
    confidenceValues[i] = std::isfinite(confidence)
                              ? static_cast<float>(std::clamp(confidence, 0.0, 1.0))
                              : 0.0f;
    const double sampleTime = frames[index].time;
    timeFractions[i] = std::isfinite(timeSpan) && timeSpan > 0.0
                           ? static_cast<float>(std::clamp(
                                 (sampleTime - firstTime) / timeSpan, 0.0, 1.0))
                           : (sampleCount > 1
                                  ? static_cast<float>(i) / (sampleCount - 1)
                                  : 0.5f);
    breakBefore[i] = i > 0 && std::isfinite(sampleTime) &&
                     sampleTime - previousTime > gapThreshold;
    previousTime = sampleTime;
  }
  return sampleCount;
}

double CompositionRenderController::trackerFrameConfidenceAt(
    int64_t frame, int64_t& resultFrame, bool& isFailureFrame,
    bool& isLowConfidence) const {
  resultFrame = -1;
  isFailureFrame = false;
  isLowConfidence = false;
  if (!impl_ || !impl_->trackerMotionTracker_) return -1.0;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return -1.0;
  const double fps = comp->frameRate().framerate();
  if (!std::isfinite(fps) || fps <= 0.0) return -1.0;
  const double time = static_cast<double>(frame) / fps;
  const auto& tracker = *impl_->trackerMotionTracker_;
  const double frameTolerance = 0.5 / fps;
  for (const double failedTime : tracker.resultRef().failureFrames) {
    if (std::isfinite(failedTime) && std::abs(failedTime - time) <= frameTolerance) {
      isFailureFrame = true;
      break;
    }
  }
  const auto* sample = tracker.frameAt(time);
  if (!sample) return -1.0;
  resultFrame = static_cast<int64_t>(std::llround(sample->time * fps));
  isLowConfidence = !std::isfinite(sample->overallConfidence) ||
                    sample->overallConfidence < tracker.settings().confidenceThreshold;
  return sample->overallConfidence;
}

int CompositionRenderController::trackerResultFrameCount() const {
  return impl_ && impl_->trackerMotionTracker_
             ? static_cast<int>(impl_->trackerMotionTracker_->resultRef().frames.size())
             : 0;
}

int CompositionRenderController::trackerMethodIndex() const {
  return impl_ && impl_->trackerMotionTracker_
             ? static_cast<int>(impl_->trackerMotionTracker_->settings().method)
             : static_cast<int>(ArtifactCore::TrackingMethod::OpticalFlow);
}

void CompositionRenderController::trackerCycleMethod() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  auto settings = impl_->trackerMotionTracker_->settings();
  const int methodCount = 4;
  settings.method = static_cast<ArtifactCore::TrackingMethod>(
      (static_cast<int>(settings.method) + 1) % methodCount);
  impl_->trackerMotionTracker_->setSettings(settings);
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  markRenderDirty();
}

double CompositionRenderController::trackerConfidenceThreshold() const {
  return impl_ && impl_->trackerMotionTracker_
             ? impl_->trackerMotionTracker_->settings().confidenceThreshold
             : 0.5;
}

void CompositionRenderController::trackerAdjustConfidenceThreshold(
    double delta) {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !std::isfinite(delta) || delta == 0.0) {
    return;
  }
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  auto settings = impl_->trackerMotionTracker_->settings();
  settings.confidenceThreshold = std::clamp(
      settings.confidenceThreshold + delta, 0.0, 1.0);
  if (settings.confidenceThreshold ==
      impl_->trackerMotionTracker_->settings().confidenceThreshold) {
    return;
  }
  impl_->trackerMotionTracker_->setSettings(settings);
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  markRenderDirty();
}

LayerID CompositionRenderController::trackerSourceLayerId() const {
  if (!impl_) return LayerID();
  const auto comp = impl_->previewPipeline_.composition();
  if (!impl_->trackerSourceLayerId_.isNil() && comp &&
      comp->layerById(impl_->trackerSourceLayerId_)) {
    return impl_->trackerSourceLayerId_;
  }
  if (comp) {
    const LayerID savedSource = projectTrackerSourceLayerId(
        comp->id(), impl_->trackerMotionTracker_
                      ? impl_->trackerMotionTracker_->id()
                      : 0);
    if (!savedSource.isNil() && comp->layerById(savedSource)) {
      return savedSource;
    }
  }
  return impl_->selectedLayerId_;
}

void CompositionRenderController::trackerCycleSourceLayer(int direction) {
  if (!impl_ || direction == 0 || trackerJobRunning() || trackerHasResult()) {
    return;
  }
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;
  const auto& layers = comp->allLayerRef();
  int sourceCount = 0;
  int currentIndex = -1;
  const LayerID currentId = trackerSourceLayerId();
  for (const auto& layer : layers) {
    if (!layer || !dynamic_cast<ArtifactImageLayer*>(layer.get())) continue;
    if (layer->id() == currentId) currentIndex = sourceCount;
    ++sourceCount;
  }
  if (sourceCount == 0) return;
  if (currentIndex < 0) currentIndex = direction > 0 ? -1 : 0;
  const int delta = direction > 0 ? 1 : -1;
  const int targetIndex = (currentIndex + delta + sourceCount) % sourceCount;
  int sourceIndex = 0;
  for (const auto& layer : layers) {
    if (!layer || !dynamic_cast<ArtifactImageLayer*>(layer.get())) continue;
    if (sourceIndex++ == targetIndex) {
      impl_->trackerSourceLayerId_ = layer->id();
      persistProjectTrackerSourceLayerId(
          comp->id(), impl_->trackerMotionTracker_
                          ? impl_->trackerMotionTracker_->id()
                          : 0,
          layer->id());
      markRenderDirty();
      return;
    }
  }
}

void CompositionRenderController::trackerUseSelectedLayerAsSource() {
  if (!impl_ || impl_->selectedLayerId_.isNil() || trackerJobRunning() ||
      trackerHasResult()) {
    return;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto selectedLayer = comp
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  if (!selectedLayer ||
      !dynamic_cast<ArtifactImageLayer*>(selectedLayer.get())) {
    return;
  }
  impl_->trackerSourceLayerId_ = impl_->selectedLayerId_;
  if (comp) {
    persistProjectTrackerSourceLayerId(
        comp->id(), impl_->trackerMotionTracker_
                        ? impl_->trackerMotionTracker_->id()
                        : 0,
        impl_->selectedLayerId_);
  }
  markRenderDirty();
}

int CompositionRenderController::trackerSessionCount() const {
  if (!impl_) return 0;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return impl_->trackerMotionTracker_ ? 1 : 0;
  const QJsonArray trackerIds = projectTrackerSessionManifest(comp->id())
      .value(QStringLiteral("trackerIds"))
      .toArray();
  int count = 0;
  auto& manager = ArtifactCore::TrackerManager::instance();
  for (const QJsonValue& value : trackerIds) {
    if (value.isDouble() && manager.tracker(value.toInt())) ++count;
  }
  if (count == 0 && impl_->trackerMotionTracker_) return 1;
  return count;
}

QString CompositionRenderController::trackerSessionLabel() const {
  if (!impl_ || !impl_->trackerMotionTracker_) return QString();
  const auto comp = impl_->previewPipeline_.composition();
  const QString name = impl_->trackerMotionTracker_->name().trimmed().isEmpty()
                           ? QStringLiteral("Tracker %1")
                                 .arg(impl_->trackerMotionTracker_->id())
                           : impl_->trackerMotionTracker_->name();
  if (!comp) return name;
  const QJsonArray trackerIds = projectTrackerSessionManifest(comp->id())
      .value(QStringLiteral("trackerIds"))
      .toArray();
  int index = -1;
  int validCount = 0;
  auto& manager = ArtifactCore::TrackerManager::instance();
  for (const QJsonValue& value : trackerIds) {
    const int id = value.toInt();
    if (!manager.tracker(id)) continue;
    if (id == impl_->trackerMotionTracker_->id()) index = validCount;
    ++validCount;
  }
  return index >= 0 && validCount > 1
             ? QStringLiteral("%1/%2 · %3")
                   .arg(index + 1).arg(validCount).arg(name)
             : name;
}

void CompositionRenderController::trackerCycleSession(int direction) {
  if (!impl_ || direction == 0 || trackerJobRunning()) return;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;
  trackerRestoreCompositionSession();
  const QJsonArray trackerIds = projectTrackerSessionManifest(comp->id())
      .value(QStringLiteral("trackerIds"))
      .toArray();
  auto& manager = ArtifactCore::TrackerManager::instance();
  int validCount = 0;
  int currentIndex = -1;
  int targetId = 0;
  for (const QJsonValue& value : trackerIds) {
    const int id = value.toInt();
    if (!manager.tracker(id)) continue;
    if (impl_->trackerMotionTracker_ &&
        id == impl_->trackerMotionTracker_->id()) {
      currentIndex = validCount;
    }
    ++validCount;
  }
  if (validCount <= 1) return;
  if (currentIndex < 0) {
    currentIndex = direction > 0 ? -1 : 0;
  }
  const int targetIndex = currentIndex < 0
      ? (direction > 0 ? 0 : validCount - 1)
      : (currentIndex + (direction > 0 ? 1 : -1) + validCount) % validCount;
  int index = 0;
  for (const QJsonValue& value : trackerIds) {
    const int id = value.toInt();
    if (!manager.tracker(id)) continue;
    if (index++ == targetIndex) {
      targetId = id;
      break;
    }
  }
  if (targetId > 0) trackerSelectSession(targetId);
}

void CompositionRenderController::trackerSelectSession(int trackerId) {
  if (!impl_ || trackerJobRunning() || trackerId <= 0) return;
  auto* tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
  const auto comp = impl_->previewPipeline_.composition();
  if (!tracker || !comp) return;
  const bool currentTrackerIsRegistered = impl_->trackerMotionTracker_ &&
      ArtifactCore::TrackerManager::instance().tracker(
          impl_->trackerMotionTrackerId_) == impl_->trackerMotionTracker_;
  if (currentTrackerIsRegistered && impl_->trackerGizmo_) {
    persistProjectTrackerGeometry(
        comp->id(), impl_->trackerMotionTrackerId_,
        impl_->trackerGizmo_->state());
  }
  registerProjectTrackerSession(comp->id(), trackerId, false);
  impl_->trackerMotionTracker_ = tracker;
  impl_->trackerMotionTrackerId_ = trackerId;
  impl_->trackerLastJobFailed_ = false;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerSourceLayerId_ = LayerID();
  impl_->trackerCaptureSourceLayerId_ = LayerID();

  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(tracker);
    auto state = impl_->trackerGizmo_->state();
    const QSize compositionSize = comp->settings().compositionSize();
    state.innerCenter = QPointF(compositionSize.width() * 0.5,
                                compositionSize.height() * 0.5);
    auto& config = ArtifactCore::LayeredConfigStore::instance();
    state.innerHalfW = std::clamp(
        static_cast<float>(config.valueDouble(
            std::string_view("Viewport/TrackPoint/FeatureWidth"),
            32.0) * 0.5), 2.0f, 4096.0f);
    state.innerHalfH = std::clamp(
        static_cast<float>(config.valueDouble(
            std::string_view("Viewport/TrackPoint/FeatureHeight"),
            32.0) * 0.5), 2.0f, 4096.0f);
    state.outerHalfW = std::max(
        state.innerHalfW + 2.0f,
        static_cast<float>(config.valueDouble(
            std::string_view("Viewport/TrackPoint/SearchWidth"),
            64.0) * 0.5));
    state.outerHalfH = std::max(
        state.innerHalfH + 2.0f,
        static_cast<float>(config.valueDouble(
            std::string_view("Viewport/TrackPoint/SearchHeight"),
            64.0) * 0.5));
    if (tracker->trackerType() == ArtifactCore::TrackerType::Point) {
      if (tracker->trackPointCount() > 0) {
        state.innerCenter = tracker->trackPoint(0).position;
      }
      const auto settings = tracker->settings();
      state.innerHalfW = state.innerHalfH =
          std::clamp(static_cast<float>(settings.windowSize) * 0.5f,
                     4.0f, 50.5f);
      const QRectF searchRegion = tracker->searchRegion();
      if (searchRegion.isValid() && searchRegion.width() > 0.0 &&
          searchRegion.height() > 0.0) {
        state.outerHalfW = std::max(
            state.innerHalfW + 2.0f,
            static_cast<float>(searchRegion.width()) * 0.5f);
        state.outerHalfH = std::max(
            state.innerHalfH + 2.0f,
            static_cast<float>(searchRegion.height()) * 0.5f);
      }
    }

    const QJsonObject manifest = projectTrackerSessionManifest(comp->id());
    const QJsonObject geometry = manifest
        .value(QStringLiteral("geometryByTracker"))
        .toObject()
        .value(QString::number(trackerId))
        .toObject();
    if (!geometry.isEmpty()) {
      const double centerX = geometry.value(QStringLiteral("centerX")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      const double centerY = geometry.value(QStringLiteral("centerY")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      const double innerHalfW = geometry.value(QStringLiteral("innerHalfW")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      const double innerHalfH = geometry.value(QStringLiteral("innerHalfH")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      const double outerHalfW = geometry.value(QStringLiteral("outerHalfW")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      const double outerHalfH = geometry.value(QStringLiteral("outerHalfH")).toDouble(
          std::numeric_limits<double>::quiet_NaN());
      if (std::isfinite(centerX) && std::isfinite(centerY) &&
          std::isfinite(innerHalfW) && std::isfinite(innerHalfH) &&
          std::isfinite(outerHalfW) && std::isfinite(outerHalfH) &&
          innerHalfW >= 4.0 && innerHalfH >= 4.0 &&
          outerHalfW >= innerHalfW + 2.0 &&
          outerHalfH >= innerHalfH + 2.0) {
        state.innerCenter = QPointF(centerX, centerY);
        state.innerHalfW = static_cast<float>(innerHalfW);
        state.innerHalfH = static_cast<float>(innerHalfH);
        state.outerHalfW = static_cast<float>(outerHalfW);
        state.outerHalfH = static_cast<float>(outerHalfH);
      }
    }
    impl_->trackerGizmo_->setState(state);
  }
  setProjectActiveTrackerSession(comp->id(), trackerId);
  markRenderDirty();
}

bool CompositionRenderController::trackerSessionSelectionOutOfDate() const {
  if (!impl_ || trackerJobRunning()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return false;
  const bool hasBoundSession = trackerHasSession();
  const QJsonObject manifest = projectTrackerSessionManifest(comp->id());
  const QJsonArray trackerIds =
      manifest.value(QStringLiteral("trackerIds")).toArray();
  auto& manager = ArtifactCore::TrackerManager::instance();
  int desiredId = manifest.value(QStringLiteral("activeTrackerId")).toInt();
  if (desiredId <= 0 || !manager.tracker(desiredId)) {
    desiredId = 0;
    for (const QJsonValue& value : trackerIds) {
      const int candidateId = value.toInt();
      if (manager.tracker(candidateId)) {
        desiredId = candidateId;
        break;
      }
    }
  }
  const int boundId = hasBoundSession ? impl_->trackerMotionTrackerId_ : 0;
  return desiredId != boundId;
}

void CompositionRenderController::trackerRestoreCompositionSession() {
  if (!impl_ || trackerJobRunning()) return;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;
  auto& manager = ArtifactCore::TrackerManager::instance();
  for (const auto& layer : comp->allLayerRef()) {
    auto* videoLayer = layer
        ? dynamic_cast<ArtifactVideoLayer*>(layer.get())
        : nullptr;
    if (!videoLayer) continue;
    const int trackerId = videoLayer->motionTrackerId();
    if (trackerId > 0 && manager.tracker(trackerId)) {
      registerProjectTrackerSession(comp->id(), trackerId, false);
    }
  }
  if (impl_->trackerMotionTracker_ &&
      manager.tracker(impl_->trackerMotionTrackerId_) ==
          impl_->trackerMotionTracker_) {
    registerProjectTrackerSession(
        comp->id(), impl_->trackerMotionTrackerId_, false);
  }
  const QJsonObject manifest = projectTrackerSessionManifest(comp->id());
  const QJsonArray trackerIds = manifest
      .value(QStringLiteral("trackerIds"))
      .toArray();
  int targetId = manifest.value(QStringLiteral("activeTrackerId")).toInt();
  if (targetId <= 0 || !manager.tracker(targetId)) {
    targetId = 0;
    for (const QJsonValue& value : trackerIds) {
      const int id = value.toInt();
      if (manager.tracker(id)) {
        targetId = id;
        break;
      }
    }
  }
  if (targetId > 0) {
    trackerSelectSession(targetId);
  } else if (impl_->trackerMotionTracker_) {
    trackerDetachSession();
  }
}

void CompositionRenderController::trackerDetachSession() {
  if (!impl_) return;
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerMotionTracker_ = nullptr;
  impl_->trackerMotionTrackerId_ = 0;
  if (impl_->trackerGizmo_) impl_->trackerGizmo_->setTracker(nullptr);
  markRenderDirty();
}

bool CompositionRenderController::trackerHasResult() const {
  return impl_ && impl_->trackerMotionTracker_ &&
         impl_->trackerMotionTracker_->hasResult();
}

void CompositionRenderController::trackerNextProblemFrame() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;
  auto* playback = ArtifactPlaybackService::instance();
  if (!playback || playback->currentComposition() != comp) return;
  const float fps = comp->frameRate().framerate();
  if (fps <= 0.0f) return;
  const auto problems = impl_->trackerMotionTracker_->problemFrames();
  if (problems.empty()) {
    setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                       QStringLiteral("No problem frames"));
    return;
  }
  const double currentTime =
      static_cast<double>(playback->currentFrame().framePosition()) / fps;
  double targetTime = problems.front();
  for (const double problemTime : problems) {
    if (problemTime > currentTime + (0.5 / fps)) {
      targetTime = problemTime;
      break;
    }
  }
  const auto targetFrame = static_cast<int64_t>(std::llround(targetTime * fps));
  playback->goToFrame(FramePosition(targetFrame));
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Reviewing problem frame %1")
                         .arg(targetFrame));
  markRenderDirty();
}

void CompositionRenderController::trackerPreviousProblemFrame() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;
  auto* playback = ArtifactPlaybackService::instance();
  if (!playback || playback->currentComposition() != comp) return;
  const float fps = comp->frameRate().framerate();
  if (fps <= 0.0f) return;
  const auto problems = impl_->trackerMotionTracker_->problemFrames();
  if (problems.empty()) {
    setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                       QStringLiteral("No problem frames"));
    return;
  }
  const double currentTime =
      static_cast<double>(playback->currentFrame().framePosition()) / fps;
  double targetTime = problems.back();
  for (auto it = problems.rbegin(); it != problems.rend(); ++it) {
    if (*it < currentTime - (0.5 / fps)) {
      targetTime = *it;
      break;
    }
  }
  const auto targetFrame = static_cast<int64_t>(std::llround(targetTime * fps));
  playback->goToFrame(FramePosition(targetFrame));
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Reviewing problem frame %1")
                         .arg(targetFrame));
  markRenderDirty();
}

void CompositionRenderController::trackerSmooth() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  if (!impl_->trackerMotionTracker_->hasResult() ||
      impl_->trackerMotionTracker_->resultRef().frames.size() < 5u) {
    setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                       QStringLiteral("Track at least five frames before smoothing"));
    return;
  }

  // Use the Core default deliberately: it is bounded, leaves inactive points
  // untouched, and recomputes per-frame confidence after filtering.
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->smoothTrack();
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Applied five-frame track smoothing"));
  markRenderDirty();
}

void CompositionRenderController::trackerRemoveOutliers() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  if (!impl_->trackerMotionTracker_->hasResult()) {
    setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                       QStringLiteral("Track a range before removing outliers"));
    return;
  }

  // Keep the UI action aligned with MotionTracker's public default (3.0),
  // rather than inventing a second app-only threshold contract.
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->removeOutliers();
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Removed velocity outliers; review flagged frames"));
  markRenderDirty();
}

void CompositionRenderController::trackerFilterByConfidence() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_) return;
  if (!impl_->trackerMotionTracker_->hasResult()) {
    setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                       QStringLiteral("Run tracking before filtering confidence"));
    return;
  }
  // Keep the repair command aligned with the solve-time quality gate. The
  // default is 0.50, but the tracker owns the effective setting.
  const double threshold =
      impl_->trackerMotionTracker_->settings().confidenceThreshold;
  const QString previousJson = impl_->trackerMotionTracker_->toJson();
  impl_->trackerMotionTracker_->filterByConfidence(threshold);
  pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
  setInfoOverlayText(trackerModeTitle(impl_->trackerMotionTracker_),
                     QStringLiteral("Deactivated points below %1 confidence")
                         .arg(threshold, 0, 'f', 2));
  markRenderDirty();
}

void CompositionRenderController::trackerStop() {
  if (!impl_ || !trackerJobRunning()) return;
  impl_->trackerCancelRequested_.store(true, std::memory_order_release);
  setInfoOverlayText(impl_->trackerSolveModeTitle_,
                     QStringLiteral("Cancelling…"));
  markRenderDirty();
}

void CompositionRenderController::trackerCaptureNextFrame(
    std::uint64_t generation) {
  if (!impl_ || generation != impl_->trackerJobGeneration_) return;
  if (impl_->trackerMotionTracker_ &&
      ArtifactCore::TrackerManager::instance().tracker(
          impl_->trackerMotionTrackerId_) != impl_->trackerMotionTracker_) {
    impl_->trackerCancelRequested_.store(true, std::memory_order_release);
    impl_->trackerCaptureActive_ = false;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    impl_->trackerMotionTracker_ = nullptr;
    impl_->trackerMotionTrackerId_ = 0;
    impl_->trackerSolveWorkingCopy_.reset();
    impl_->trackerPreSolveJson_.clear();
    if (impl_->trackerGizmo_) impl_->trackerGizmo_->setTracker(nullptr);
    setInfoOverlayText(QStringLiteral("Tracker"),
                       QStringLiteral("Tracker session changed; operation discarded"));
    markRenderDirty();
    return;
  }
  auto *tracker = impl_->trackerCaptureActive_
                      ? impl_->trackerSolveWorkingCopy_.get()
                      : impl_->trackerMotionTracker_;
  const auto discardCapture = [this]() {
    impl_->trackerSolveWorkingCopy_.reset();
    impl_->trackerPreSolveJson_.clear();
    impl_->trackerCaptureActive_ = false;
  };
  if (tracker) impl_->trackerSolveModeTitle_ = trackerModeTitle(tracker);
  auto *gizmo = impl_->trackerGizmo_.get();
  auto *renderer = impl_->renderer_.get();
  const auto comp = impl_->previewPipeline_.composition();
  if (!tracker || !gizmo || !renderer || !comp) {
    discardCapture();
    impl_->trackerLastJobFailed_ = true;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    return;
  }
  if (!impl_->trackerCaptureActive_) {
    impl_->trackerCaptureSourceLayerId_ = trackerSourceLayerId();
    if (impl_->trackerCaptureSourceLayerId_.isNil()) {
      impl_->trackerCaptureSourceLayerId_ = impl_->selectedLayerId_;
    }
    impl_->trackerSourceLayerId_ = impl_->trackerCaptureSourceLayerId_;
    persistProjectTrackerSourceLayerId(
        comp->id(), tracker->id(), impl_->trackerCaptureSourceLayerId_);
  }
  const LayerID sourceLayerId = impl_->trackerCaptureActive_
                                    ? impl_->trackerCaptureSourceLayerId_
                                    : trackerSourceLayerId();
  const auto selectedLayer = sourceLayerId.isNil()
                                 ? ArtifactAbstractLayerPtr{}
                                 : comp->layerById(sourceLayerId);
  if (!selectedLayer ||
      !dynamic_cast<ArtifactImageLayer *>(selectedLayer.get())) {
    discardCapture();
    impl_->trackerLastJobFailed_ = true;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    setInfoOverlayText(
        impl_->trackerSolveModeTitle_,
        QStringLiteral("Select a still-image or image-sequence layer"));
    markRenderDirty();
    return;
  }

  if (!impl_->trackerCaptureActive_) {
    const auto range = comp->frameRange();
    const float fps = comp->frameRate().framerate();
    const QSize size = comp->settings().compositionSize();
    if (!range.isValid() || fps <= 0.0f || size.isEmpty()) {
      discardCapture();
      impl_->trackerLastJobFailed_ = true;
      impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
      return;
    }
    ensureOffscreenRenderer(impl_->trackerOffscreenRenderer_, renderer,
                            size.width(), size.height());
    if (!impl_->trackerOffscreenRenderer_) {
      discardCapture();
      impl_->trackerLastJobFailed_ = true;
      impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
      return;
    }

    impl_->trackerPreSolveJson_ = tracker->toJson();
    impl_->trackerSolveWorkingCopy_.reset(
        new ArtifactCore::MotionTracker(*tracker));
    tracker = impl_->trackerSolveWorkingCopy_.get();
    const auto configuredTrackPoints = tracker->trackPoints();
    tracker->clearTrackingData();
    const auto &state = gizmo->state();
    const QRectF featureRect(
        state.innerCenter.x() - state.innerHalfW,
        state.innerCenter.y() - state.innerHalfH,
        state.innerHalfW * 2.0f, state.innerHalfH * 2.0f);
    const QRectF searchRect(
        state.innerCenter.x() - state.outerHalfW,
        state.innerCenter.y() - state.outerHalfH,
        state.outerHalfW * 2.0f, state.outerHalfH * 2.0f);
    if (tracker->trackerType() == ArtifactCore::TrackerType::Planar) {
      tracker->addTrackPoint({state.innerCenter.x() - state.innerHalfW,
                              state.innerCenter.y() - state.innerHalfH});
      tracker->addTrackPoint({state.innerCenter.x() + state.innerHalfW,
                              state.innerCenter.y() - state.innerHalfH});
      tracker->addTrackPoint({state.innerCenter.x() + state.innerHalfW,
                              state.innerCenter.y() + state.innerHalfH});
      tracker->addTrackPoint({state.innerCenter.x() - state.innerHalfW,
                              state.innerCenter.y() + state.innerHalfH});
      tracker->addTrackRegion(
          QRectF(state.innerCenter.x() - state.innerHalfW,
                 state.innerCenter.y() - state.innerHalfH,
                 state.innerHalfW * 2.0f, state.innerHalfH * 2.0f));
      tracker->setSearchRegion(QRectF());
    } else {
      if (configuredTrackPoints.empty()) {
        tracker->addTrackPoint(state.innerCenter);
      } else {
        for (const auto& point : configuredTrackPoints) {
          tracker->addTrackPoint(point.position);
        }
      }
      tracker->setSearchRegion(searchRect);
      auto trackerSettings = tracker->settings();
      const int featureSize = std::max(
          5, static_cast<int>(std::round(std::min(featureRect.width(),
                                                   featureRect.height()))));
      trackerSettings.windowSize = std::clamp(featureSize | 1, 5, 101);
      tracker->setSettings(trackerSettings);
    }

    auto* playback = ArtifactPlaybackService::instance();
    const int64_t currentFrame = playback &&
            playback->currentComposition() == comp
        ? playback->currentFrame().framePosition()
        : comp->framePosition().framePosition();
    const int64_t current = std::clamp(
        currentFrame, range.start(), range.end());
    const auto direction = impl_->trackerJobDirection_;
    impl_->trackerCaptureStartFrame_ =
        direction == Impl::TrackerJobDirection::All ? range.start() : current;
    if (direction == Impl::TrackerJobDirection::Backward) {
      impl_->trackerCaptureEndFrame_ = range.start();
    } else if (direction == Impl::TrackerJobDirection::StepBackward) {
      if (current <= range.start()) {
        discardCapture();
        impl_->trackerLastJobFailed_ = true;
        impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
        return;
      }
      impl_->trackerCaptureEndFrame_ = current - 1;
    } else if (direction == Impl::TrackerJobDirection::StepForward) {
      if (current >= range.end()) {
        discardCapture();
        impl_->trackerLastJobFailed_ = true;
        impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
        return;
      }
      impl_->trackerCaptureEndFrame_ = current + 1;
    } else {
      impl_->trackerCaptureEndFrame_ = range.end();
    }
    impl_->trackerCaptureNextFrame_ = impl_->trackerCaptureStartFrame_;
    impl_->trackerCaptureFrameCount_ =
        std::abs(impl_->trackerCaptureEndFrame_ -
                 impl_->trackerCaptureStartFrame_) + 1;
    impl_->trackerCapturedFrameCount_ = 0;
    impl_->trackerCaptureFrameRetryCount_ = 0;
    impl_->trackerFrameStep_ = 1.0 / static_cast<double>(fps);
    impl_->trackerCancelRequested_.store(false, std::memory_order_release);
    impl_->trackerSolveFinished_.store(false, std::memory_order_release);
    impl_->trackerSolveSucceeded_.store(false, std::memory_order_release);
    impl_->trackerSolveProgress_.store(0.0, std::memory_order_release);
    impl_->trackerCaptureActive_ = true;
  }

  if (impl_->trackerCancelRequested_.load(std::memory_order_acquire)) {
    discardCapture();
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    setInfoOverlayText(impl_->trackerSolveModeTitle_,
                       QStringLiteral("Cancelled"));
    markRenderDirty();
    return;
  }

  const int64_t frame = impl_->trackerCaptureNextFrame_;
  const QImage image = impl_->trackerOffscreenRenderer_->renderLayerToQImage(
      ArtifactCore::FramePosition(frame), selectedLayer.get());
  auto* sequenceLayer = dynamic_cast<ArtifactImageLayer*>(selectedLayer.get());
  if (sequenceLayer && sequenceLayer->isImageSequence()) {
    const double sourceFrame = std::floor(
        (static_cast<double>(frame) / comp->frameRate().framerate()) *
        sequenceLayer->sequenceFrameRate());
    const auto expectedSequenceFrame = std::max<qint64>(
        0, static_cast<qint64>(sourceFrame));
    if (sequenceLayer->sequenceCachedFrameIndex() != expectedSequenceFrame) {
      constexpr int kMaximumSequenceFrameRetries = 300;
      if (++impl_->trackerCaptureFrameRetryCount_ <=
          kMaximumSequenceFrameRetries) {
        setInfoOverlayText(
            impl_->trackerSolveModeTitle_,
            QStringLiteral("Decoding source frame %1…").arg(frame));
        QTimer::singleShot(10, this, [this, generation]() {
          trackerCaptureNextFrame(generation);
        });
        return;
      }
      discardCapture();
      impl_->trackerLastJobFailed_ = true;
      impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
      setInfoOverlayText(
          impl_->trackerSolveModeTitle_,
          QStringLiteral("Could not decode sequence frame %1; tracking stopped")
              .arg(expectedSequenceFrame));
      markRenderDirty();
      return;
    }
    impl_->trackerCaptureFrameRetryCount_ = 0;
  }
  const QString trackerCaptureDump = qEnvironmentVariable(
      "ARTIFACT_UI_TEST_TRACKER_CAPTURE_DUMP_DIR");
  if (!trackerCaptureDump.isEmpty() && impl_->trackerCapturedFrameCount_ < 3) {
    const QString dumpPath = QDir(trackerCaptureDump).filePath(
        QStringLiteral("captured-%1.png").arg(impl_->trackerCapturedFrameCount_));
    if (!image.isNull() && !image.save(dumpPath, "PNG")) {
      qWarning() << "[TrackerUiFixture] capture dump failed" << dumpPath;
    }
  }
  if (image.isNull()) {
    discardCapture();
    impl_->trackerLastJobFailed_ = true;
    impl_->trackerCaptureActive_ = false;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    setInfoOverlayText(impl_->trackerSolveModeTitle_,
                       QStringLiteral("Could not capture frame %1; tracking stopped")
                           .arg(frame));
    markRenderDirty();
    return;
  }
  tracker->setFrame(static_cast<double>(frame) * impl_->trackerFrameStep_, image);
  ++impl_->trackerCapturedFrameCount_;
  const int capturePercent = static_cast<int>(std::round(
      50.0 * static_cast<double>(impl_->trackerCapturedFrameCount_) /
      static_cast<double>(impl_->trackerCaptureFrameCount_)));
  setInfoOverlayText(
      trackerModeTitle(tracker),
      QStringLiteral("Preparing image sequence • %1% • Stop available")
          .arg(capturePercent));
  markRenderDirty();

  if (frame != impl_->trackerCaptureEndFrame_) {
    impl_->trackerCaptureFrameRetryCount_ = 0;
    impl_->trackerCaptureNextFrame_ +=
        (impl_->trackerJobDirection_ == Impl::TrackerJobDirection::Backward ||
         impl_->trackerJobDirection_ == Impl::TrackerJobDirection::StepBackward)
            ? -1
            : 1;
    QTimer::singleShot(0, this, [this, generation]() {
      trackerCaptureNextFrame(generation);
    });
    return;
  }

  impl_->trackerCaptureActive_ = false;
  gizmo->setTracker(nullptr);
  auto *jobImpl = impl_;
  const auto direction = impl_->trackerJobDirection_;
  const double startTime =
      static_cast<double>(impl_->trackerCaptureStartFrame_) *
      impl_->trackerFrameStep_;
  const double endTime =
      static_cast<double>(impl_->trackerCaptureEndFrame_) *
      impl_->trackerFrameStep_;
  auto* solveTracker = impl_->trackerSolveWorkingCopy_.get();
  impl_->trackerSolveModeTitle_ = trackerModeTitle(tracker);
  impl_->trackerSolveFuture_ = QtConcurrent::run(
      &sharedBackgroundThreadPool(),
      [jobImpl, solveTracker, direction, startTime, endTime]() {
        const auto progress = [jobImpl](double value) {
          jobImpl->trackerSolveProgress_.store(value,
                                               std::memory_order_release);
          return !jobImpl->trackerCancelRequested_.load(
              std::memory_order_acquire);
        };
        bool succeeded = false;
        try {
          if (direction == Impl::TrackerJobDirection::Backward ||
              direction == Impl::TrackerJobDirection::StepBackward) {
            succeeded = solveTracker->trackBackwardRange(startTime, endTime,
                                                           progress);
          } else if (direction == Impl::TrackerJobDirection::All) {
            succeeded = solveTracker->trackAll(progress);
          } else {
            succeeded = solveTracker->trackRange(startTime, endTime, progress);
          }
        } catch (...) {
          succeeded = false;
        }
        jobImpl->trackerSolveSucceeded_.store(succeeded,
                                              std::memory_order_release);
        jobImpl->trackerSolveFinished_.store(true,
                                             std::memory_order_release);
      });
  trackerPollJob(generation);
}

void CompositionRenderController::trackerPollJob(std::uint64_t generation) {
  if (!impl_ || generation != impl_->trackerJobGeneration_) return;
  if (!impl_->trackerSolveFinished_.load(std::memory_order_acquire)) {
    const int progress = 50 + static_cast<int>(std::round(
        50.0 * impl_->trackerSolveProgress_.load(std::memory_order_acquire)));
    setInfoOverlayText(
        impl_->trackerSolveModeTitle_,
        QStringLiteral("Solving track • %1% • Stop available").arg(progress));
    markRenderDirty();
    QTimer::singleShot(50, this,
                       [this, generation]() { trackerPollJob(generation); });
    return;
  }

  impl_->trackerSolveFuture_.waitForFinished();
  const bool cancelled =
      impl_->trackerCancelRequested_.load(std::memory_order_acquire);
  bool succeeded =
      impl_->trackerSolveSucceeded_.load(std::memory_order_acquire);
  const auto completedDirection = impl_->trackerJobDirection_;
  const bool trackerStillRegistered = impl_->trackerMotionTracker_ &&
      ArtifactCore::TrackerManager::instance().tracker(
          impl_->trackerMotionTrackerId_) == impl_->trackerMotionTracker_;
  if (succeeded && !cancelled && trackerStillRegistered &&
      impl_->trackerSolveWorkingCopy_) {
    succeeded = impl_->trackerMotionTracker_->fromJson(
        impl_->trackerSolveWorkingCopy_->toJson());
  } else if (!trackerStillRegistered) {
    succeeded = false;
    impl_->trackerMotionTracker_ = nullptr;
    impl_->trackerMotionTrackerId_ = 0;
  }
  impl_->trackerSolveWorkingCopy_.reset();
  impl_->trackerLastJobFailed_ = !succeeded && !cancelled;
  if (impl_->trackerGizmo_) {
    impl_->trackerGizmo_->setTracker(impl_->trackerMotionTracker_);
  }
  // A completed solve replaces the whole result set, so the previous snapshot
  // has to become undoable or the user cannot get the old track back.
  if (succeeded && !cancelled && impl_->trackerMotionTracker_ &&
      !impl_->trackerPreSolveJson_.isEmpty()) {
    const QString previousJson = impl_->trackerPreSolveJson_;
    pushTrackerUndo(previousJson, impl_->trackerMotionTracker_->toJson());
    markRenderDirty();
  }
  impl_->trackerPreSolveJson_.clear();
  const QString detail = !trackerStillRegistered
          ? QStringLiteral("Project changed; tracking result discarded")
          : cancelled
          ? QStringLiteral("Cancelled; partial result was not accepted")
          : succeeded
                ? QStringLiteral("Complete • confidence %1 • problem frames %2")
                      .arg(impl_->trackerMotionTracker_->averageConfidence(),
                           0, 'f', 2)
                      .arg(static_cast<int>(impl_->trackerMotionTracker_
                                                ->problemFrames()
                                                .size()))
                : QStringLiteral("Tracking failed: check the feature region and image detail");
  if (succeeded && trackerStillRegistered && !cancelled &&
      (completedDirection == Impl::TrackerJobDirection::StepForward ||
       completedDirection == Impl::TrackerJobDirection::StepBackward)) {
    const auto composition = impl_->previewPipeline_.composition();
    auto *playback = ArtifactPlaybackService::instance();
    if (composition && playback &&
        playback->currentComposition() == composition &&
        playback->currentFrame().framePosition() ==
            impl_->trackerCaptureStartFrame_) {
      playback->setCurrentFrame(
          ArtifactCore::FramePosition(impl_->trackerCaptureEndFrame_));
    }
  }
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
  setInfoOverlayText(impl_->trackerMotionTracker_
                         ? trackerModeTitle(impl_->trackerMotionTracker_)
                         : QStringLiteral("Tracker"),
                     detail);
  markRenderDirty();
}



void CompositionRenderController::trackerApplyToPosition() {

  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !impl_->trackerGizmo_) return;



  auto comp = impl_->previewPipeline_.composition();

  if (!comp) return;



  ArtifactAbstractLayerPtr targetLayer;

  if (!impl_->selectedLayerId_.isNil()) {

    targetLayer = comp->layerById(impl_->selectedLayerId_);

  }



  Artifact::ArtifactPointTrackerTool::ApplyOptions opts;

  opts.pointId = impl_->trackerMotionTracker_->firstTrackPointId();
  opts.createNullLayer = true;
  opts.writeAnchor = false;

  opts.applyToSelectedLayer = (targetLayer != nullptr);



  const bool applied = Artifact::ArtifactPointTrackerTool::applyTrackingResult(
      comp, *impl_->trackerMotionTracker_, opts, targetLayer);
  if (applied && !targetLayer && !comp->allLayerRef().empty()) {
    const auto createdLayer = comp->allLayerRef().back();
    if (createdLayer) {
      setSelectedLayerId(createdLayer->id());
    }
  }

  setInfoOverlayText(
      QStringLiteral("TrackPoint • Apply Position"),
      !applied ? QStringLiteral("No tracking keyframes were available")
               : targetLayer ? QStringLiteral("Tracking applied to selected layer")
                             : QStringLiteral("Tracking applied to a new Null layer"));


  markRenderDirty();

}



void CompositionRenderController::trackerApplyToAnchor() {

  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !impl_->trackerGizmo_) return;



  auto comp = impl_->previewPipeline_.composition();

  if (!comp) return;



  ArtifactAbstractLayerPtr targetLayer;

  if (!impl_->selectedLayerId_.isNil()) {

    targetLayer = comp->layerById(impl_->selectedLayerId_);

  }



  Artifact::ArtifactPointTrackerTool::ApplyOptions opts;

  opts.pointId = impl_->trackerMotionTracker_->firstTrackPointId();
  opts.createNullLayer = false;

  opts.applyToSelectedLayer = (targetLayer != nullptr);

  opts.writeAnchor = true;



  const bool applied = Artifact::ArtifactPointTrackerTool::applyTrackingResult(
      comp, *impl_->trackerMotionTracker_, opts, targetLayer);

  setInfoOverlayText(
      QStringLiteral("TrackPoint • Apply Anchor"),
      !applied ? QStringLiteral("No tracking keyframes were available")
               : targetLayer ? QStringLiteral("Tracking written to selected anchor")
                             : QStringLiteral("Select a layer to write the anchor"));


  markRenderDirty();

}

void CompositionRenderController::trackerApplyAllPoints() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !impl_->trackerGizmo_) return;
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) return;

  Artifact::ArtifactPointTrackerTool::ApplyOptions opts;
  opts.createNullLayer = true;
  opts.applyToSelectedLayer = false;
  const int applied = Artifact::ArtifactPointTrackerTool::applyAllTrackingPoints(
      comp, *impl_->trackerMotionTracker_, opts);
  setInfoOverlayText(
      QStringLiteral("TrackPoint • Apply All"),
      applied > 0
          ? QStringLiteral("Created %1 Null layer(s)").arg(applied)
          : QStringLiteral("No tracking points were available"));
  markRenderDirty();
}

void CompositionRenderController::trackerApplyPlanarCornerPin() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !impl_->trackerGizmo_ ||
      impl_->trackerMotionTracker_->trackerType() != ArtifactCore::TrackerType::Planar) {
    setInfoOverlayText(QStringLiteral("Planar Tracker"),
                      QStringLiteral("Planar tracking is not active"));
    return;
  }
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    setInfoOverlayText(QStringLiteral("Planar Tracker"),
                      QStringLiteral("Select a target layer first"));
    return;
  }
  const auto targetLayer = comp->layerById(impl_->selectedLayerId_);
  if (!targetLayer) return;
  const auto& state = impl_->trackerGizmo_->state();
  const QRectF sourceRect(
      state.innerCenter.x() - state.innerHalfW,
      state.innerCenter.y() - state.innerHalfH,
      state.innerHalfW * 2.0, state.innerHalfH * 2.0);
  const bool applied = Artifact::ArtifactPointTrackerTool::applyPlanarResultAsCornerPin(
      comp.get(), *impl_->trackerMotionTracker_, sourceRect, targetLayer);
  setInfoOverlayText(QStringLiteral("Planar Tracker • Corner Pin"),
                     applied ? QStringLiteral("Corner Pin keyframes applied")
                             : QStringLiteral("No planar keyframes were available"));
  markRenderDirty();
}

void CompositionRenderController::trackerApplyToMask() {
  if (!impl_ || trackerJobRunning() || !impl_->trackerMotionTracker_ ||
      !impl_->trackerGizmo_ ||
      impl_->trackerMotionTracker_->trackerType() != ArtifactCore::TrackerType::Planar) {
    setInfoOverlayText(QStringLiteral("Planar Tracker"),
                       QStringLiteral("Planar tracking is not active"));
    return;
  }
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp || impl_->selectedLayerId_.isNil()) {
    setInfoOverlayText(QStringLiteral("Planar Tracker"),
                       QStringLiteral("Select a target layer first"));
    return;
  }
  const auto targetLayer = comp->layerById(impl_->selectedLayerId_);
  if (!targetLayer) return;
  if (targetLayer->maskCount() <= 0) {
    setInfoOverlayText(QStringLiteral("Planar Tracker • Mask"),
                       QStringLiteral("The selected layer has no mask"));
    return;
  }

  const int maskIndex = impl_->trackerMaskIndex_;
  const int pathIndex = impl_->trackerMaskPathIndex_;
  if (maskIndex < 0 || maskIndex >= targetLayer->maskCount()) return;
  LayerMask mask = targetLayer->mask(maskIndex);
  if (pathIndex < 0 || pathIndex >= mask.maskPathCount()) {
    setInfoOverlayText(QStringLiteral("Planar Tracker • Mask"),
                       QStringLiteral("The selected mask has no path"));
    return;
  }
  // Use the mask's own bounds as the tracked region: that is the shape the
  // planar solve must follow, and it avoids a second user gesture to define it.
  // maskPathCanvasBounds() gives canvas space, and the tracker solves in
  // layer-local space, so the rect is mapped back through the inverse.
  const MaskPath path = mask.maskPath(pathIndex);
  const QRectF canvasRect =
      maskPathCanvasBounds(path, targetLayer->getGlobalTransform());
  if (!canvasRect.isValid() || canvasRect.width() <= 0.0 ||
      canvasRect.height() <= 0.0) {
    setInfoOverlayText(QStringLiteral("Planar Tracker • Mask"),
                       QStringLiteral("The selected mask has no usable bounds"));
    return;
  }
  const QTransform worldToLocal = targetLayer->getGlobalTransform().inverted();
  const QRectF sourceRect = worldToLocal.mapRect(canvasRect);
  if (!sourceRect.isValid() || sourceRect.width() <= 0.0 ||
      sourceRect.height() <= 0.0) {
    setInfoOverlayText(QStringLiteral("Planar Tracker • Mask"),
                       QStringLiteral("The selected mask has no usable bounds"));
    return;
  }

  const bool applied = Artifact::ArtifactPointTrackerTool::applyPlanarResultAsMask(
      comp.get(), *impl_->trackerMotionTracker_, sourceRect, targetLayer,
      maskIndex, pathIndex);
  setInfoOverlayText(QStringLiteral("Planar Tracker • Mask"),
                     applied ? QStringLiteral("Mask keyframes applied")
                             : QStringLiteral("No planar keyframes were available"));
  if (applied) {
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
  }
  markRenderDirty();
}

int CompositionRenderController::trackerMaskIndex() const {
  return impl_ ? impl_->trackerMaskIndex_ : 0;
}

int CompositionRenderController::trackerMaskPathIndex() const {
  return impl_ ? impl_->trackerMaskPathIndex_ : 0;
}

int CompositionRenderController::trackerMaskPathCount() const {
  if (!impl_) return 0;
  const auto comp = impl_->previewPipeline_.composition();
  const auto target = comp && !impl_->selectedLayerId_.isNil()
                          ? comp->layerById(impl_->selectedLayerId_)
                          : nullptr;
  if (!target) return 0;
  int total = 0;
  for (int i = 0; i < target->maskCount(); ++i) {
    if (const auto *mask = target->maskView(i)) total += mask->maskPathCount();
  }
  return total;
}

void CompositionRenderController::trackerCycleMaskPath(int direction) {
  if (!impl_ || direction == 0 || trackerJobRunning()) return;
  const auto comp = impl_->previewPipeline_.composition();
  const auto target = comp && !impl_->selectedLayerId_.isNil()
                          ? comp->layerById(impl_->selectedLayerId_)
                          : nullptr;
  if (!target) return;
  int total = 0;
  for (int i = 0; i < target->maskCount(); ++i) {
    if (const auto *mask = target->maskView(i)) total += mask->maskPathCount();
  }
  if (total <= 0) {
    impl_->trackerMaskIndex_ = 0;
    impl_->trackerMaskPathIndex_ = 0;
    return;
  }
  int flat = 0;
  bool found = false;
  for (int i = 0; i < target->maskCount(); ++i) {
    const auto *mask = target->maskView(i);
    if (!mask) continue;
    if (i == impl_->trackerMaskIndex_ &&
        impl_->trackerMaskPathIndex_ >= 0 &&
        impl_->trackerMaskPathIndex_ < mask->maskPathCount()) {
      flat += impl_->trackerMaskPathIndex_;
      found = true;
      break;
    }
    flat += mask->maskPathCount();
  }
  if (!found) flat = direction > 0 ? -1 : 0;
  flat = (flat + (direction > 0 ? 1 : total - 1)) % total;
  for (int i = 0; i < target->maskCount(); ++i) {
    const auto *mask = target->maskView(i);
    if (!mask) continue;
    if (flat < mask->maskPathCount()) {
      impl_->trackerMaskIndex_ = i;
      impl_->trackerMaskPathIndex_ = flat;
      break;
    }
    flat -= mask->maskPathCount();
  }
  markRenderDirty();
}



void CompositionRenderController::trackerDelete() {
  if (!impl_) return;
  trackerStop();
  impl_->trackerPointPlacementArmed_ = false;
  impl_->trackerSolveFuture_.waitForFinished();
  impl_->trackerSolveWorkingCopy_.reset();
  impl_->trackerPreSolveJson_.clear();
  ++impl_->trackerJobGeneration_;
  impl_->trackerCaptureActive_ = false;
  impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;

  const auto comp = impl_->previewPipeline_.composition();
  const bool trackerStillRegistered = impl_->trackerMotionTracker_ &&
          ArtifactCore::TrackerManager::instance().tracker(
              impl_->trackerMotionTrackerId_) == impl_->trackerMotionTracker_;
  const int trackerId = trackerStillRegistered
                            ? impl_->trackerMotionTrackerId_
                            : 0;
  QString trackerJson;
  QJsonObject geometry;
  LayerID sourceLayerId;
  if (trackerStillRegistered && comp) {
    trackerJson = impl_->trackerMotionTracker_->toJson();
    if (impl_->trackerGizmo_) {
      persistProjectTrackerGeometry(comp->id(), trackerId,
                                    impl_->trackerGizmo_->state());
    }
    const QJsonObject manifest = projectTrackerSessionManifest(comp->id());
    geometry = manifest.value(QStringLiteral("geometryByTracker"))
                   .toObject()
                   .value(QString::number(trackerId))
                   .toObject();
    sourceLayerId = projectTrackerSourceLayerId(comp->id(), trackerId);
  }
  if (trackerId > 0 && comp && !trackerJson.isEmpty()) {
    if (auto* undo = UndoManager::instance()) {
      const bool deletionRecorded = undo->push(
          std::make_unique<TrackerSessionUndoCommand>(
              comp->id(), trackerId, trackerJson, geometry, sourceLayerId,
              false));
      if (!deletionRecorded) {
        setInfoOverlayText(QStringLiteral("Tracker"),
                           QStringLiteral("Could not record tracker deletion in Undo history"));
        return;
      }
    }
  }
  if (trackerId > 0) {
    if (comp) removeProjectTrackerSession(comp->id(), trackerId);
    ArtifactCore::TrackerManager::instance().removeTracker(trackerId);
  }

  trackerDetachSession();
  if (comp) trackerRestoreCompositionSession();
}

} // namespace Artifact
