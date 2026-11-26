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
#include <QPixmap>
#include <QWidget>
#include <QScreen>
#include <QtGlobal>

#include <QVector3D>
#include <QVector2D>

#include <QVector4D>

#include <QVector>
#include <deque>
#include <cstdio>


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

namespace CompositionRenderControllerDetail {

static QString renderBackendToString(ArtifactRenderQueueService::RenderBackend backend)

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

static double clamp01(double value)

{

  return std::clamp(value, 0.0, 1.0);

}

static QString densityLevelFromScore(double score)

{

  if (score >= 0.85) {

    return QStringLiteral("high");

  }

  if (score >= 0.55) {

    return QStringLiteral("medium");

  }

  return QStringLiteral("low");

}

static QString densityWarningForDominantAxis(const QString &axis, double score)

{

  const QString level = densityLevelFromScore(score);

  if (level == QStringLiteral("low")) {

    return QStringLiteral("density is readable");

  }



  if (axis == QStringLiteral("visual")) {

    return QStringLiteral("visual density is high");

  }

  if (axis == QStringLiteral("information")) {

    return QStringLiteral("information density is high");

  }

  if (axis == QStringLiteral("luminance")) {

    return QStringLiteral("luminance density is high");

  }

  if (axis == QStringLiteral("motion")) {

    return QStringLiteral("motion density is high");

  }

  return QStringLiteral("density is high");

}

static QString densityNextActionForAxis(const QString &axis)

{

  if (axis == QStringLiteral("visual")) {

    return QStringLiteral("collapse repeated regions or add spacing");

  }

  if (axis == QStringLiteral("information")) {

    return QStringLiteral("reduce labels or move notes into debugger");

  }

  if (axis == QStringLiteral("luminance")) {

    return QStringLiteral("soften bright clusters or separate highlights");

  }

  if (axis == QStringLiteral("motion")) {

    return QStringLiteral("spread key activity or add rest frames");

  }

  return QStringLiteral("reduce the busiest region");

}

static QString playbackStateToString(PlaybackState state)

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

} // namespace CompositionRenderControllerDetail

QImage CompositionRenderController::captureCurrentFrameImage() const {

  if (impl_->renderer_ && impl_->lastPresentedReadbackSRV_) {

    const QImage frame =

        impl_->renderer_->readbackTextureViewToImage(
            impl_->lastPresentedReadbackSRV_,
            impl_->lastPresentedReadbackChannelOrder_);

    if (!frame.isNull()) {

      return frame;

    }

  }



  if (auto *host = impl_->hostWidget_.data()) {

    // QWidget::grab() repaints Qt backing stores and misses the native
    // Diligent child surface (a non-null, entirely black image). Capture the
    // visible viewport rectangle at this explicit screenshot boundary.
    const QPoint origin = host->mapToGlobal(QPoint(0, 0));
    const QImage grabbed = host->isVisible() && host->screen()
        ? host->screen()->grabWindow(0, origin.x(), origin.y(),
                                     host->width(), host->height()).toImage()
        : QImage();

    if (!grabbed.isNull()) {

      return grabbed;

    }

  }



  if (impl_->renderer_) {

    return impl_->renderer_->readbackToImage();

  }



  return QImage();

}

QImage CompositionRenderController::readbackPresentedFrameImage() const {
  if (!impl_ || !impl_->renderer_ || !impl_->lastPresentedReadbackSRV_) {
    return QImage();
  }
  return impl_->renderer_->readbackTextureViewToImage(
      impl_->lastPresentedReadbackSRV_,
      impl_->lastPresentedReadbackChannelOrder_);
}

QImage CompositionRenderController::readbackViewportChannelDisplayImage() const {
  if (!impl_ || !impl_->renderer_ || !impl_->viewportChannelDisplaySRV_) {
    return QImage();
  }
  return impl_->renderer_->readbackTextureViewToImage(
      impl_->viewportChannelDisplaySRV_, ArtifactCore::SurfaceChannelOrder::RGBA);
}

bool CompositionRenderController::requestCurrentFrameImageAsync(
    std::function<void(QImage, quint64)> completion) const {
  if (!impl_ || !impl_->renderer_ || !completion) {
    return false;
  }

  const quint64 frameSerial = impl_->renderFrameCounter_;
  auto publish = [completion = std::move(completion), frameSerial](
                     const QImage &capturedFrame) mutable {
    completion(capturedFrame, frameSerial);
  };

  if (impl_->lastPresentedReadbackSRV_) {
    return impl_->renderer_->readbackTextureViewToImageAsync(
        impl_->lastPresentedReadbackSRV_,
        impl_->lastPresentedReadbackChannelOrder_, std::move(publish));
  }
  return impl_->renderer_->readbackToImageAsync(std::move(publish));
}

quint64 CompositionRenderController::currentFrameSerial() const {
  return impl_ ? impl_->renderFrameCounter_ : 0;
}



ArtifactCore::FrameDebugSnapshot

CompositionRenderController::frameDebugCounters() const {
  ArtifactCore::FrameDebugSnapshot snapshot;
  if (!impl_) return snapshot;
  snapshot.renderLastFrameMs = impl_->lastFrameTimeMs_;
  snapshot.renderAverageFrameMs = impl_->averageFrameTimeMs_;
  if (impl_->renderer_) {
    // Read published counters only: no capture, GPU polling or resource readback.
    snapshot.renderCost = impl_->renderer_->frameCostStats();
    snapshot.renderGpuFrameMs = impl_->renderer_->lastFrameGpuTimeMs();
  }
  return snapshot;
}

ArtifactCore::FrameDebugSnapshot
CompositionRenderController::frameDebugSnapshot(bool includePreviews) const {

  ArtifactCore::TraceScopeRecord traceScope;

  traceScope.name = QStringLiteral("CompositionRenderController::frameDebugSnapshot");

  traceScope.domain = ArtifactCore::TraceDomain::Render;

  QElapsedTimer traceTimer;

  traceTimer.start();

  const auto finalizeTraceScope = [&]() {

    traceScope.endNs = traceTimer.nsecsElapsed();

    if (traceScope.endNs <= traceScope.startNs) {

      traceScope.endNs = traceScope.startNs + 1;

    }

    ArtifactCore::TraceRecorder::instance().recordScope(traceScope);

  };



  if (includePreviews && impl_->renderer_) {

    const int slotIndex = std::clamp(

        impl_->activePreviewRenderPipelineSlot_, 0,

        Impl::kPreviewRenderPipelineSlotCount - 1);

    auto &diagnosticPipeline =

        impl_->previewRenderPipelineSlots_[slotIndex].pipeline;

    if (diagnosticPipeline.ready()) {

      impl_->lastLayerRtPreview_ =

          impl_->renderer_->readbackTextureViewToImage(
              diagnosticPipeline.layerRTV(),
              ArtifactCore::SurfaceChannelOrder::RGBA);

      impl_->lastAccumRtPreview_ =

          impl_->renderer_->readbackTextureViewToImage(
              diagnosticPipeline.accumRTV(),
              ArtifactCore::SurfaceChannelOrder::BGRA);

      impl_->lastLayerRtPixelStats_ =

          previewPixelStats(impl_->lastLayerRtPreview_);

      impl_->lastAccumRtPixelStats_ =

          previewPixelStats(impl_->lastAccumRtPreview_);

    }

  }



  ArtifactCore::FrameDebugSnapshot snapshot;

  const auto comp = impl_->previewPipeline_.composition();

  const auto selectedLayerId = impl_->selectedLayerId_;

  auto *playback = ArtifactPlaybackService::instance();

  auto *queue = ArtifactRenderQueueService::instance();



  // Diagnostic-only graph. The immediate-mode render path remains unchanged.
  const int diagnosticSlotIndex = std::clamp(
      impl_->activePreviewRenderPipelineSlot_, 0,
      Impl::kPreviewRenderPipelineSlotCount - 1);
  const auto& diagnosticPipeline =
      impl_->previewRenderPipelineSlots_[diagnosticSlotIndex].pipeline;
  if (impl_->renderer_ && comp && diagnosticPipeline.ready()) {
    ArtifactCore::RenderGraph diagnosticGraph;
    const auto graphWidth = diagnosticPipeline.width();
    const auto graphHeight = diagnosticPipeline.height();
    const auto graphBytes = static_cast<std::uint64_t>(graphWidth) *
                            static_cast<std::uint64_t>(graphHeight) * 8u;
    const auto layerResource = diagnosticGraph.addResource({
        "Composition.LayerOutput", ArtifactCore::RenderResourceKind::Texture,
        ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
        graphHeight, 1, 0, graphBytes});
    const auto accumulationResource = diagnosticGraph.addResource({
        "Composition.Accumulation", ArtifactCore::RenderResourceKind::Texture,
        ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
        graphHeight, 1, 0, graphBytes});
    ArtifactCore::RenderResourceHandle pointwiseResource;
    if (impl_->pointwiseAppliedCount_ > 0) {
      pointwiseResource = diagnosticGraph.addResource({
          "Composition.PointwiseEffects",
          ArtifactCore::RenderResourceKind::Texture,
          ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
          graphHeight, 1, 0, graphBytes});
    }
    ArtifactCore::RenderResourceHandle giResource;
    if (impl_->giDispatchedCount_ > 0) {
      giResource = diagnosticGraph.addResource({
          "Composition.ScreenSpaceGI",
          ArtifactCore::RenderResourceKind::Texture,
          ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
          graphHeight, 1, 0, graphBytes});
    }
    const auto outputResource = diagnosticGraph.addResource({
        "Composition.Output", ArtifactCore::RenderResourceKind::Texture,
        ArtifactCore::RenderResourceLifetime::External, graphWidth,
        graphHeight, 1, 0, graphBytes});
    std::size_t rasterPassCount = 0;
    const auto graphFrame = comp->framePosition();
    for (const auto& layer : comp->allLayerRef()) {
      if (!layer || !isLayerEffectivelyVisible(layer) ||
          !layer->isActiveAt(graphFrame)) {
        continue;
      }
      // Per-layer raster -> mask -> blend chain. Raster stages share the
      // layer scratch (write-after-write edges serialize them, matching the
      // serial layer loop); mask nodes exist only for layers with enabled
      // track mattes so culled stages stay out of the plan; each blend
      // consumes the layer surface into the shared accumulation token.
      // Descriptors only: no RenderPass objects are allocated here, so this
      // stays free of per-frame std::function state. Mask intermediates use
      // the same full-size, eight-byte-per-pixel estimate as layer scratch.
      const std::string layerTag = std::to_string(rasterPassCount);
      diagnosticGraph.addPass({
          "Composition.LayerRaster." + layerTag,
          ArtifactCore::RenderPassQueue::Graphics, {}, {layerResource}, true});
      auto layerSurface = layerResource;
      if (layerHasEnabledMatteReferences(layer.get())) {
        auto maskedSurface = diagnosticGraph.addResource({
            "Composition.LayerMasked." + layerTag,
            ArtifactCore::RenderResourceKind::Texture,
            ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
            graphHeight, 1, 0, graphBytes});
        diagnosticGraph.addPass({
            "Composition.LayerMask." + layerTag,
            ArtifactCore::RenderPassQueue::Graphics,
            {layerResource}, {maskedSurface}, true});
        layerSurface = maskedSurface;
      }
      diagnosticGraph.addPass({
          "Composition.LayerBlend." + layerTag,
          ArtifactCore::RenderPassQueue::Compute,
          {layerSurface}, {accumulationResource}, true});
      ++rasterPassCount;
    }
    if (rasterPassCount == 0) {
      diagnosticGraph.addPass({
          "Composition.LayerRaster.Empty",
          ArtifactCore::RenderPassQueue::Graphics, {}, {layerResource}, true});
      // Keep the accumulation token written so downstream effect passes
      // still see a valid producer when no layer exists.
      diagnosticGraph.addPass({
          "Composition.LayerBlend.Empty",
          ArtifactCore::RenderPassQueue::Compute,
          {layerResource}, {accumulationResource}, true});
    }
    auto effectsInput = accumulationResource;
    if (impl_->pointwiseAppliedCount_ > 0) {
      diagnosticGraph.addPass({
          "Composition.PointwiseEffects",
          ArtifactCore::RenderPassQueue::Compute,
          {effectsInput}, {pointwiseResource}, true});
      effectsInput = pointwiseResource;
    }
    if (impl_->giDispatchedCount_ > 0) {
      diagnosticGraph.addPass({
          "Composition.ScreenSpaceGI", ArtifactCore::RenderPassQueue::Compute,
          {effectsInput}, {giResource}, true});
      effectsInput = giResource;
    }
    if (impl_->precompFastPathSkipCount_ > 0) {
      diagnosticGraph.addPass({
          "Composition.PrecompFallback", ArtifactCore::RenderPassQueue::Graphics,
          {effectsInput}, {accumulationResource}, true});
    }
    if (impl_->adjustmentFallbackCount_ > 0) {
      diagnosticGraph.addPass({
          "Composition.AdjustmentFallback", ArtifactCore::RenderPassQueue::Compute,
          {effectsInput}, {accumulationResource}, true});
    }
    diagnosticGraph.addPass({
        "Composition.FinalPostProcess", ArtifactCore::RenderPassQueue::Graphics,
        {effectsInput}, {outputResource}, true});
    snapshot.renderGraphDiagnostic =
        diagnosticGraph.compileDiagnosticSnapshot();
    snapshot.hasRenderGraphDiagnostic = snapshot.renderGraphDiagnostic.valid;
  }

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

  if (snapshot.hasRenderGraphDiagnostic) {
    std::uint64_t executionId = 0;
    if (impl_->renderer_ && impl_->renderer_->hasFrameGpuTiming()) {
      executionId = impl_->renderer_->lastFrameGpuTimingExecutionId();
    }
    if (executionId == 0) {
      executionId = static_cast<std::uint64_t>(
          std::max<std::int64_t>(0, snapshot.timestampMs));
    }
    snapshot.renderGraphDiagnostic.executionId = executionId;
  }



  if (comp) {

    snapshot.compositionName = comp->settings().compositionName().toQString();

  } else {

    snapshot.compositionName = QStringLiteral("<none>");

  }



  snapshot.selectedLayerName = QStringLiteral("<none>");

  const auto selectedLayer = (comp && !selectedLayerId.isNil())

                                 ? comp->layerById(selectedLayerId)

                                 : ArtifactAbstractLayerPtr{};

  if (comp) {

    const auto &layers = comp->allLayerRef();

    snapshot.totalLayerCount = static_cast<int>(layers.size());

    for (const auto &layer : layers) {

      if (!layer) {

        continue;

      }

      if (dynamic_cast<ArtifactTextLayer *>(layer.get())) {

        ++snapshot.textLayerCount;

      }

      if (isLayerEffectivelyVisible(layer) && layer->isActiveAt(snapshot.frame)) {

        ++snapshot.visibleLayerCount;

      }

    }

  }

  if (comp && !selectedLayerId.isNil()) {

    if (auto layer = selectedLayer) {

      snapshot.selectedLayerName = layer->layerName();

      snapshot.selectedLayerMaskCount = layer->maskCount();

      snapshot.selectedLayerEffectCount = layer->effectCount();

      snapshot.selectedLayerMatteCount =
          layerActiveMatteReferenceCount(layer.get());

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

  if (comp && !selectedLayerId.isNil()) {
    const auto selectedLayer = comp->layerById(selectedLayerId);
    if (const auto* imageLayer =
            dynamic_cast<const ArtifactImageLayer*>(selectedLayer.get())) {
      const QString sourcePath = imageLayer->sourcePath().trimmed();
      const bool hasSourcePath = !sourcePath.isEmpty();
      const bool sourceExists = hasSourcePath && QFileInfo::exists(sourcePath);
      const bool hasBuffer = imageLayer->hasCurrentFrameBuffer();
      GPUTextureOwnerStats layerGpuStats;
      GPUTextureOwnerStats assetGpuStats;
      if (impl_->gpuTextureCacheManager_) {
        layerGpuStats = impl_->gpuTextureCacheManager_->ownerStats(
            imageLayer->id().toString());
        if (!imageLayer->sourceAssetId().isNull()) {
          assetGpuStats = impl_->gpuTextureCacheManager_->ownerStats(
              QStringLiteral("asset:%1").arg(
                  imageLayer->sourceAssetId().toString(QUuid::WithoutBraces)));
        }
      }
      const int gpuPendingCount = layerGpuStats.pendingUploadCount +
                                  assetGpuStats.pendingUploadCount;
      const auto gpuPendingBytes = layerGpuStats.pendingUploadBytes +
                                   assetGpuStats.pendingUploadBytes;
      QString gpuSourceBindingState = QStringLiteral("n/a");
      if (impl_->gpuTextureCacheManager_ &&
          hasBuffer &&
          imageLayer->canShareSourceGpuTexture() &&
          !imageLayer->sourceAssetId().isNull() &&
          imageLayer->sourceVersion() > 0) {
        const QString gpuOwner = QStringLiteral("asset:%1").arg(
            imageLayer->sourceAssetId().toString(QUuid::WithoutBraces));
        const QString gpuKey =
            QStringLiteral("image-f32:v%1|cs=%2|tf=%3")
                .arg(imageLayer->sourceVersion())
                .arg(imageLayer->inputColorSpace())
                .arg(imageLayer->inputTransferFunction());
        const auto handle = impl_->gpuTextureCacheManager_->findExisting(
            gpuOwner, gpuKey, imageLayer->currentFrameBuffer());
        gpuSourceBindingState =
            impl_->gpuTextureCacheManager_->bindingRecord(handle).isValid()
                ? QStringLiteral("ready")
                : QStringLiteral("candidate-missing");
      }
      ArtifactCore::FrameDebugResourceRecord imageSourceResource;
      imageSourceResource.label = QStringLiteral("Selected Image Source");
      imageSourceResource.type = QStringLiteral("image");
      imageSourceResource.relation = QStringLiteral("source");
      imageSourceResource.cacheHit = hasBuffer;
      imageSourceResource.stale =
          (hasSourcePath && !sourceExists) || gpuPendingCount > 0;
      imageSourceResource.note =
          QStringLiteral("sourcePath=%1 exists=%2 sourceVersion=%3 "
                         "buffer=%4 sequence=%5 colorSpace=%6 transfer=%7 "
                         "gpuLayerEntries=%8 gpuAssetEntries=%9 "
                         "gpuLayerImportedFrames=%13 "
                         "gpuAssetImportedFrames=%14 "
                         "gpuPending=%10 gpuPendingBytes=%11 "
                         "gpuSourceBinding=%12")
              .arg(hasSourcePath ? sourcePath : QStringLiteral("<in-memory>"))
              .arg(sourceExists ? 1 : (hasSourcePath ? 0 : 1))
              .arg(QString::number(imageLayer->sourceVersion()))
              .arg(hasBuffer ? 1 : 0)
              .arg(imageLayer->isImageSequence() ? 1 : 0)
              .arg(imageLayer->inputColorSpace().isEmpty()
                       ? QStringLiteral("<auto>")
                       : imageLayer->inputColorSpace())
              .arg(imageLayer->inputTransferFunction().isEmpty()
                       ? QStringLiteral("<auto>")
                       : imageLayer->inputTransferFunction())
              .arg(layerGpuStats.entryCount)
              .arg(assetGpuStats.entryCount)
              .arg(gpuPendingCount)
              .arg(static_cast<qulonglong>(gpuPendingBytes))
              .arg(gpuSourceBindingState)
              .arg(static_cast<qulonglong>(
                  layerGpuStats.importedGpuFrameCount))
              .arg(static_cast<qulonglong>(
                  assetGpuStats.importedGpuFrameCount));
      snapshot.resources.push_back(imageSourceResource);
    }
  }

  if (!impl_->lastCompositionVisibilitySummary_.isEmpty()) {

    ArtifactCore::FrameDebugResourceRecord visibilityResource;

    visibilityResource.label = QStringLiteral("Composition Visibility");

    visibilityResource.type = QStringLiteral("composition");

    visibilityResource.relation = QStringLiteral("visibility");

    visibilityResource.cacheHit =

        impl_->lastCompositionVisibilitySummary_.contains(

            QStringLiteral("state=drawn"));

    visibilityResource.stale =

        impl_->lastCompositionVisibilitySummary_.contains(

            QStringLiteral("frameOutOfRange=1"));

    visibilityResource.note = impl_->lastCompositionVisibilitySummary_;

    snapshot.resources.push_back(visibilityResource);

  }



  if (!impl_->lastBlendMaskSummary_.isEmpty()) {

    ArtifactCore::FrameDebugResourceRecord blendMaskResource;

    blendMaskResource.label = QStringLiteral("Blend / Mask Contract");

    blendMaskResource.type = QStringLiteral("blendMask");

    blendMaskResource.relation = QStringLiteral("contract");

    blendMaskResource.cacheHit =

        impl_->lastBlendMaskSummary_.contains(QStringLiteral("failed=0")) &&

        impl_->lastBlendMaskSummary_.contains(QStringLiteral("directFallback=0"));

    blendMaskResource.stale =

        impl_->lastBlendMaskSummary_.contains(QStringLiteral("maskContract=pending"));

    blendMaskResource.texture.valid = true;

    blendMaskResource.texture.name = QStringLiteral("accum/temp/layer");

    blendMaskResource.texture.format =

        QStringLiteral("accum=temp=RGBA32F layer=RGBA8_sRGB");

    blendMaskResource.texture.width =

        std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));

    blendMaskResource.texture.height =

        std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));

    blendMaskResource.texture.mipLevel = 0;

    blendMaskResource.texture.mipLevels = 1;

    blendMaskResource.texture.sliceIndex = 0;

    blendMaskResource.texture.arrayLayers = 1;

    blendMaskResource.texture.sampleCount = 1;

    blendMaskResource.texture.srgb = false;

    blendMaskResource.note = impl_->lastBlendMaskSummary_;

    snapshot.resources.push_back(blendMaskResource);

  }



  if (!impl_->lastVideoDebug_.isEmpty()) {

    ArtifactCore::FrameDebugResourceRecord videoResource;

    videoResource.label = QStringLiteral("Video Decode");

    videoResource.type = QStringLiteral("video");

    videoResource.relation = QStringLiteral("decode");

    videoResource.cacheHit =

        impl_->lastVideoDebug_.contains(QStringLiteral("source=ram-cache")) ||

        impl_->lastVideoDebug_.contains(QStringLiteral("frameReady=1"));

    videoResource.stale =

        impl_->lastVideoDebug_.contains(QStringLiteral("repeatLastGood")) ||

        impl_->lastVideoDebug_.contains(QStringLiteral("decoding=true")) ||

        impl_->lastVideoDebug_.contains(QStringLiteral("stage=requested")) ||

        impl_->lastVideoDebug_.contains(QStringLiteral("stage=decoding"));

    videoResource.note = impl_->lastVideoDebug_;

    snapshot.resources.push_back(videoResource);

  }



  if (impl_->renderer_) {

    // Native validation inspection is cold work: never poll it while the
    // playback reporter requests metadata-only snapshots.
    if (includePreviews) {
      ArtifactCore::FrameDebugResourceRecord validationResource;
      validationResource.label = QStringLiteral("GPU Validation");
      validationResource.type = QStringLiteral("gpuValidation");
      validationResource.relation = QStringLiteral("shared-device");
      validationResource.note = impl_->renderer_->gpuValidationDebugState();
      snapshot.resources.push_back(validationResource);
    }

    const QString particleDebug = impl_->renderer_->particleDebugState();

    if (!particleDebug.isEmpty() && particleDebug != QStringLiteral("<none>")) {

      ArtifactCore::FrameDebugResourceRecord particleResource;

      particleResource.label = QStringLiteral("Particle Draw");

      particleResource.type = QStringLiteral("particle");

      particleResource.relation = QStringLiteral("draw");

      particleResource.cacheHit = !particleDebug.contains(QStringLiteral("skipped="));

      particleResource.stale = particleDebug.contains(QStringLiteral("skipped="));

      particleResource.note = particleDebug;

      snapshot.resources.push_back(particleResource);

    }



    const QString glyphAtlasDebug = impl_->renderer_->glyphAtlasDebugState();

    if (!glyphAtlasDebug.isEmpty() && glyphAtlasDebug != QStringLiteral("<none>")) {

      ArtifactCore::FrameDebugResourceRecord glyphAtlasResource;

      glyphAtlasResource.label = QStringLiteral("Glyph Atlas");

      glyphAtlasResource.type = QStringLiteral("glyphAtlas");

      glyphAtlasResource.relation = QStringLiteral("atlas");

      glyphAtlasResource.cacheHit = !glyphAtlasDebug.contains(QStringLiteral("dirty=true"));

      glyphAtlasResource.stale = glyphAtlasDebug.contains(QStringLiteral("dirty=true"));

      glyphAtlasResource.note = glyphAtlasDebug;

      snapshot.resources.push_back(glyphAtlasResource);

    }

  }



  if (impl_->gpuTextureCacheManager_) {

    const auto stats = impl_->gpuTextureCacheManager_->stats();

    ArtifactCore::FrameDebugResourceRecord textureCacheResource;

    textureCacheResource.label = QStringLiteral("GPU Texture Cache");

    textureCacheResource.type = QStringLiteral("cache");

    textureCacheResource.relation = QStringLiteral("upload");

    textureCacheResource.cacheHit =
        stats.pendingUploadCount == 0 &&
        (stats.entryCount == 0 || stats.hitCount >= stats.missCount);

    textureCacheResource.stale = stats.pendingUploadCount > 0;

    textureCacheResource.note = QStringLiteral(
        "entries=%1 bytes=%2 importedFrames=%18 hits=%3 misses=%4 pendingUploads=%5 "
        "pendingBytes=%6 invalidations=%7 lastReason=%8 frame=%9 "
        "expireAfter=%10 expired=%11 expiredThisFrame=%12 "
        "explicit=%13 ownerChanged=%14 budget=%15 deviceReset=%16 clearAll=%17")
        .arg(stats.entryCount)
        .arg(static_cast<qulonglong>(stats.memoryBytes))
        .arg(static_cast<qulonglong>(stats.hitCount))
        .arg(static_cast<qulonglong>(stats.missCount))
        .arg(stats.pendingUploadCount)
        .arg(static_cast<qulonglong>(stats.pendingUploadBytes))
        .arg(static_cast<qulonglong>(stats.invalidationCount))
        .arg(gpuTextureCacheInvalidationReasonText(stats.lastInvalidationReason))
        .arg(static_cast<qulonglong>(stats.currentFrameIndex))
        .arg(static_cast<qulonglong>(stats.resourceExpirationFrames))
        .arg(static_cast<qulonglong>(stats.expiredEvictionCount))
        .arg(stats.expiredEvictionsThisFrame)
        .arg(static_cast<qulonglong>(stats.explicitInvalidationCount))
        .arg(static_cast<qulonglong>(stats.ownerChangedInvalidationCount))
        .arg(static_cast<qulonglong>(stats.budgetEvictionCount))
        .arg(static_cast<qulonglong>(stats.deviceResetCount))
        .arg(static_cast<qulonglong>(stats.clearAllCount))
        .arg(static_cast<qulonglong>(stats.importedGpuFrameCount));

    snapshot.resources.push_back(textureCacheResource);

  }

  {

    ArtifactCore::FrameDebugResourceRecord damageResource;

    damageResource.label = QStringLiteral("Composition Damage");

    damageResource.type = QStringLiteral("dirty-region");

    damageResource.relation = QStringLiteral("recompose");

    damageResource.cacheHit = impl_->lastConsumedDamageLayerCount_ == 0;

    damageResource.stale = impl_->damageTracker_.hasDirtyRegions();

    damageResource.note = QStringLiteral(
        "layers=%1 tiles=%2 planScheduled=%3 planRemainder=%4 full=%5 "
        "rect=%6,%7 %8x%9 pending=%10")
        .arg(impl_->lastConsumedDamageLayerCount_)
        .arg(impl_->lastConsumedDamageTileCount_)
        .arg(impl_->lastConsumedDamageTilePlan_.scheduledCount)
        .arg(impl_->lastConsumedDamageTilePlan_.deferredCount)
        .arg(impl_->lastConsumedDamageWasFullRedraw_ ? 1 : 0)
        .arg(impl_->lastConsumedDamageRect_.x(), 0, 'f', 1)
        .arg(impl_->lastConsumedDamageRect_.y(), 0, 'f', 1)
        .arg(impl_->lastConsumedDamageRect_.width(), 0, 'f', 1)
        .arg(impl_->lastConsumedDamageRect_.height(), 0, 'f', 1)
        .arg(impl_->damageTracker_.dirtyLayerCount());

    snapshot.resources.push_back(damageResource);

  }

  if (impl_->renderer_) {
    ArtifactCore::FrameDebugResourceRecord shadowMapResource;
    shadowMapResource.label = QStringLiteral("Shadow Map");
    shadowMapResource.type = QStringLiteral("depthTexture");
    shadowMapResource.relation = QStringLiteral("lighting");
    const QString shadowState = impl_->renderer_->shadowMapDebugState();
    shadowMapResource.cacheHit = shadowState.contains(QStringLiteral("ready=true"));
    shadowMapResource.stale = shadowState.contains(QStringLiteral("enabled=true")) &&
                              !shadowMapResource.cacheHit;
    shadowMapResource.note = shadowState;
    snapshot.resources.push_back(std::move(shadowMapResource));
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

    auto addBinding = [](ArtifactCore::FrameDebugPassRecord &pass,

                         const QString &key, const QString &value,

                         const QString &stage = QString(),

                         const QString &note = QString()) {

      ArtifactCore::FrameDebugBindingRecord binding;

      binding.key = key;

      binding.value = value;

      binding.stage = stage;

      binding.note = note;

      pass.debugBindings.push_back(std::move(binding));

    };

    const QString backendName =

        snapshot.renderBackend.isEmpty() ? QStringLiteral("unknown")

                                         : snapshot.renderBackend;



    ArtifactCore::FrameDebugPassRecord setupPass =

        makePass(QStringLiteral("setup"), ArtifactCore::FrameDebugPassKind::Clear,

                 impl_->lastSetupMs_);

    ArtifactCore::FrameDebugPassRecord basePass =

        makePass(QStringLiteral("base"), ArtifactCore::FrameDebugPassKind::Clear,

                 impl_->lastBasePassMs_);

    ArtifactCore::FrameDebugPassRecord surfacePass =

        makePass(QStringLiteral("surface"), ArtifactCore::FrameDebugPassKind::Draw,

                 impl_->lastSurfacePassMs_, impl_->lastRenderPathSummary_);

    ArtifactCore::FrameDebugPassRecord maskPass =

        makePass(QStringLiteral("mask"),

                 ArtifactCore::FrameDebugPassKind::Composite,

                 impl_->lastMaskPassMs_, impl_->lastBlendMaskSummary_);

    ArtifactCore::FrameDebugPassRecord compositePass =

        makePass(QStringLiteral("composite"),

                 ArtifactCore::FrameDebugPassKind::Composite,

                 impl_->lastCompositePassMs_, impl_->lastBlendMaskSummary_);

    ArtifactCore::FrameDebugPassRecord postPass =

        makePass(QStringLiteral("post"),

                 ArtifactCore::FrameDebugPassKind::Resolve,

                 impl_->lastPostPassMs_);

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

    setupPass.name = QStringLiteral("Viewport Setup");

    setupPass.backend = backendName;

    setupPass.shaderName = QStringLiteral("fixed-function/setup");

    setupPass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(setupPass, QStringLiteral("hostWidth"),

               QString::number(std::max(1, static_cast<int>(std::lround(impl_->hostWidth_)))));

    addBinding(setupPass, QStringLiteral("hostHeight"),

               QString::number(std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)))));

    addBinding(setupPass, QStringLiteral("canvasWidth"),

               QString::number(std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)))));

    addBinding(setupPass, QStringLiteral("canvasHeight"),

               QString::number(std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)))));



    basePass.name = QStringLiteral("Composition Background");

    basePass.backend = backendName;

    basePass.shaderName = QStringLiteral("checkerboard/grid/clear");

    basePass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(basePass, QStringLiteral("viewportSize"),

               QStringLiteral("%1 x %2")

                   .arg(std::max(1, static_cast<int>(std::lround(impl_->hostWidth_))))

                   .arg(std::max(1, static_cast<int>(std::lround(impl_->hostHeight_)))),

               QStringLiteral("pixel"));

    addBinding(basePass, QStringLiteral("backgroundMode"),

               QStringLiteral("composition-editor"));

    addBinding(basePass, QStringLiteral("renderPathSummary"),

               impl_->lastRenderPathSummary_.isEmpty() ? QStringLiteral("<none>")

                                                       : impl_->lastRenderPathSummary_);



    surfacePass.name = QStringLiteral("Layer Surface Pass");

    surfacePass.backend = backendName;

    surfacePass.shaderName = QStringLiteral("layer-surface-capture");

    surfacePass.previewResourceLabel =

        snapshot.selectedLayerName.isEmpty() ||

                snapshot.selectedLayerName == QStringLiteral("<none>")

            ? QStringLiteral("viewport")

            : snapshot.selectedLayerName;

    addBinding(surfacePass, QStringLiteral("selectedLayer"),

               snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")

                                                    : snapshot.selectedLayerName);

    addBinding(surfacePass, QStringLiteral("visibleLayerCount"),

               QString::number(snapshot.visibleLayerCount));

    addBinding(surfacePass, QStringLiteral("selectedLayerEffects"),

               QString::number(snapshot.selectedLayerEffectCount));

    addBinding(surfacePass, QStringLiteral("selectedLayerMasks"),

               QString::number(snapshot.selectedLayerMaskCount));



    maskPass.name = QStringLiteral("Mask / Matte Pass");

    maskPass.backend = backendName;

    maskPass.shaderName = QStringLiteral("mask-matte-resolve");

    maskPass.previewResourceLabel = QStringLiteral("Layer RT");

    addBinding(maskPass, QStringLiteral("selectedLayerMasks"),

               QString::number(snapshot.selectedLayerMaskCount));

    addBinding(maskPass, QStringLiteral("selectedLayerMattes"),

               QString::number(snapshot.selectedLayerMatteCount));

    addBinding(maskPass, QStringLiteral("blendMaskSummary"),

               impl_->lastBlendMaskSummary_.isEmpty()

                   ? QStringLiteral("<none>")

                   : impl_->lastBlendMaskSummary_);



    compositePass.name = QStringLiteral("Composite Pass");

    compositePass.backend = backendName;

    compositePass.shaderName = QStringLiteral("blend-into-accum");

    compositePass.previewResourceLabel = QStringLiteral("Accum RT");

    addBinding(compositePass, QStringLiteral("renderPathSummary"),

               impl_->lastRenderPathSummary_.isEmpty()

                   ? QStringLiteral("<none>")

                   : impl_->lastRenderPathSummary_);



    postPass.name = QStringLiteral("Post Pass");

    postPass.backend = backendName;

    postPass.shaderName = QStringLiteral("composition-post");

    postPass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(postPass, QStringLiteral("status"),

               comp ? QStringLiteral("final-effects stage")

                    : QStringLiteral("no composition"));



    overlayPass.name = QStringLiteral("Editor Overlay / Gizmo");

    overlayPass.backend = backendName;

    overlayPass.shaderName = QStringLiteral("overlay-helpers");

    overlayPass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(overlayPass, QStringLiteral("selection"),

               snapshot.selectedLayerName.isEmpty() ? QStringLiteral("<none>")

                                                    : snapshot.selectedLayerName);

    addBinding(overlayPass, QStringLiteral("overlayStage"),

               QStringLiteral("gizmo/hud/helpers"));



    flushPass.name = QStringLiteral("Flush / Resolve");

    flushPass.backend = backendName;

    flushPass.shaderName = QStringLiteral("resolve");

    flushPass.previewResourceLabel = QStringLiteral("viewport");



    presentPass.name = QStringLiteral("Present / Readback");

    presentPass.backend = backendName;

    presentPass.shaderName = QStringLiteral("readback");

    presentPass.previewResourceLabel = QStringLiteral("viewport");

    addBinding(presentPass, QStringLiteral("readbackTarget"),

               QStringLiteral("viewport"));



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



    if (impl_->renderer_) {

      ArtifactCore::FrameDebugResourceRecord layerTargetResource;

      layerTargetResource.label = QStringLiteral("Layer RT");

      layerTargetResource.type = QStringLiteral("rt");

      layerTargetResource.relation = QStringLiteral("intermediate");

      layerTargetResource.cacheHit = true;

      layerTargetResource.texture.valid = diagnosticPipeline.ready() &&
          diagnosticPipeline.layerRTV() != nullptr;

      layerTargetResource.texture.name = QStringLiteral("Layer RT");

      layerTargetResource.texture.format = QStringLiteral("render-target");

      layerTargetResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));

      layerTargetResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));

      layerTargetResource.texture.mipLevel = 0;

      layerTargetResource.texture.mipLevels = 1;

      layerTargetResource.texture.sliceIndex = 0;

      layerTargetResource.texture.arrayLayers = 1;

      layerTargetResource.texture.sampleCount = 1;

      layerTargetResource.texture.srgb = false;

      layerTargetResource.note = layerTargetResource.texture.valid
          ? QStringLiteral("previewSource=layer-render-target")
          : QStringLiteral("unavailable=no-intermediate-pipeline");

      snapshot.resources.push_back(layerTargetResource);



      ArtifactCore::FrameDebugResourceRecord accumResource;

      accumResource.label = QStringLiteral("Accum RT");

      accumResource.type = QStringLiteral("rt");

      accumResource.relation = QStringLiteral("intermediate");

      accumResource.cacheHit = true;

      accumResource.texture.valid = diagnosticPipeline.ready() &&
          diagnosticPipeline.accumRTV() != nullptr;

      accumResource.texture.name = QStringLiteral("Accum RT");

      accumResource.texture.format = QStringLiteral("render-target");

      accumResource.texture.width = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasWidth_)));

      accumResource.texture.height = std::max(1, static_cast<int>(std::lround(impl_->lastCanvasHeight_)));

      accumResource.texture.mipLevel = 0;

      accumResource.texture.mipLevels = 1;

      accumResource.texture.sliceIndex = 0;

      accumResource.texture.arrayLayers = 1;

      accumResource.texture.sampleCount = 1;

      accumResource.texture.srgb = false;

      accumResource.note = accumResource.texture.valid
          ? QStringLiteral("previewSource=accum-render-target")
          : QStringLiteral("unavailable=no-intermediate-pipeline");

      snapshot.resources.push_back(accumResource);

    }



    if (includePreviews && impl_->renderer_) {

      QImage viewportAfter = captureCurrentFrameImage();

      if (comp) {

        applyCompositionFinalEffectsToImage(comp.get(), viewportAfter);

      }

      const QImage viewportAlpha =

          impl_->renderer_->readbackChannelToImage(ArtifactIRenderer::ChannelType::Alpha);

      addSnapshotPreview(snapshot, QStringLiteral("viewport"),

                         QStringLiteral("Viewport Final"),

                         QStringLiteral("before=unavailable after=final-present alpha=readback"),

                         QImage(), viewportAfter, viewportAlpha);

      addSnapshotPreview(snapshot, QStringLiteral("Particle Draw"),

                         QStringLiteral("Particle Draw"),

                         QStringLiteral("previewSource=final-viewport before=unavailable"),

                         QImage(), viewportAfter, viewportAlpha);

      addSnapshotPreview(snapshot, QStringLiteral("Blend / Mask Contract"),

                         QStringLiteral("Blend / Mask Contract"),

                         QStringLiteral("previewSource=final-viewport before=unavailable"),

                         QImage(), viewportAfter, viewportAlpha);

      addSnapshotPreview(snapshot, QStringLiteral("Layer RT"),

                         QStringLiteral("Layer RT"),

                         QStringLiteral("previewSource=layer-render-target before=unavailable"),

                         QImage(), impl_->lastLayerRtPreview_, viewportAlpha);

      addSnapshotPreview(snapshot, QStringLiteral("Accum RT"),

                         QStringLiteral("Accum RT"),

                         QStringLiteral("previewSource=accum-render-target before=unavailable"),

                         QImage(), impl_->lastAccumRtPreview_, viewportAlpha);

      if (!snapshot.selectedLayerName.isEmpty() &&

          snapshot.selectedLayerName != QStringLiteral("<none>")) {

        addSnapshotPreview(snapshot, snapshot.selectedLayerName,

                           snapshot.selectedLayerName,

                           QStringLiteral("previewSource=final-viewport before=unavailable"),

                           QImage(), viewportAfter, viewportAlpha);

      }

    }



    if (!snapshot.selectedLayerName.isEmpty() &&

        snapshot.selectedLayerName != QStringLiteral("<none>")) {

      const auto selectedLayer = comp ? comp->layerById(selectedLayerId)

                                      : ArtifactAbstractLayerPtr{};

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

      if (selectedLayer) {

        auto *layer = selectedLayer.get();

        if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(layer)) {

          selectedResource.type = QStringLiteral("video");

          selectedResource.note = videoLayer->decodeState();

        } else if (layer->isParticleLayer()) {

          selectedResource.type = QStringLiteral("particle");

          selectedResource.note = QStringLiteral("particle layer");

        } else if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {

          selectedResource.type = QStringLiteral("text");

          selectedResource.note = textLayer->debugState();

        } else {

          selectedResource.note = impl_->lastVideoDebug_;

        }



        if (layer->maskCount() > 0) {

          QStringList maskNotes;

          maskNotes.reserve(layer->maskCount());

          int enabledMaskCount = 0;

          int totalPathCount = 0;

          for (int m = 0; m < layer->maskCount(); ++m) {

            const LayerMask mask = layer->mask(m);

            const int pathCount = mask.maskPathCount();

            totalPathCount += pathCount;

            if (mask.isEnabled()) {

              ++enabledMaskCount;

            }

            maskNotes.push_back(

                QStringLiteral("#%1 paths=%2 enabled=%3")

                    .arg(m + 1)

                    .arg(pathCount)

                    .arg(mask.isEnabled() ? QStringLiteral("on")

                                          : QStringLiteral("off")));

          }



          ArtifactCore::FrameDebugResourceRecord maskResource;

          maskResource.label = QStringLiteral("Mask Inspector");

          maskResource.type = QStringLiteral("mask");

          maskResource.relation = QStringLiteral("selected");

          maskResource.cacheHit = enabledMaskCount > 0;

          maskResource.stale = false;

          maskResource.note = QStringLiteral("maskCount=%1 enabled=%2 paths=%3 | %4")

                                 .arg(layer->maskCount())

                                 .arg(enabledMaskCount)

                                 .arg(totalPathCount)

                                 .arg(maskNotes.join(QStringLiteral("; ")));

          snapshot.resources.push_back(maskResource);

        }

      } else {

        selectedResource.note = impl_->lastVideoDebug_;

      }

      if (includePreviews && impl_->renderer_) {

        QImage viewportAfter = captureCurrentFrameImage();

        if (comp) {

          applyCompositionFinalEffectsToImage(comp.get(), viewportAfter);

        }

        const QImage viewportAlpha =

            impl_->renderer_->readbackChannelToImage(ArtifactIRenderer::ChannelType::Alpha);

        addSnapshotPreview(snapshot, selectedResource.label, selectedResource.label,

                           QStringLiteral("resource-preview=selected-layer final-readback"),

                           QImage(), viewportAfter, viewportAlpha);

      }

      snapshot.resources.push_back(selectedResource);

    }



    snapshot.passes.push_back(setupPass);

    snapshot.passes.push_back(basePass);

    snapshot.passes.push_back(surfacePass);

    snapshot.passes.push_back(maskPass);

    snapshot.passes.push_back(compositePass);

    snapshot.passes.push_back(postPass);

    snapshot.passes.push_back(overlayPass);

    snapshot.passes.push_back(flushPass);

    snapshot.passes.push_back(presentPass);

    if (impl_->renderer_) {

      const auto rendererPasses = impl_->renderer_->frameDebugPasses();

      snapshot.passes.insert(snapshot.passes.end(), rendererPasses.begin(),

                             rendererPasses.end());

    }

  }



  const double visualRaw = snapshot.visibleLayerCount * 0.08 +

                           snapshot.selectedLayerMaskCount * 0.16 +

                           snapshot.selectedLayerEffectCount * 0.12 +

                           snapshot.selectedLayerMatteCount * 0.10;

  const double informationRaw = snapshot.resources.size() * 0.08 +

                                snapshot.attachments.size() * 0.12 +

                                snapshot.passes.size() * 0.10 +

                                snapshot.textLayerCount * 0.05;

  const double luminanceRaw = snapshot.textLayerCount * 0.06 +

                              (selectedLayer ? 0.10 : 0.0) +

                              (snapshot.renderBackend == QStringLiteral("gpu") ? 0.04 : 0.0);

  const double motionRaw = clamp01(snapshot.renderLastFrameMs / 24.0) * 0.45 +

                           clamp01(snapshot.renderGpuFrameMs / 24.0) * 0.25 +

                           clamp01(static_cast<double>(snapshot.renderCost.drawCalls) / 5000.0) * 0.30;



  snapshot.visualDensityScore = clamp01(visualRaw);

  snapshot.informationDensityScore = clamp01(informationRaw);

  snapshot.luminanceDensityScore = clamp01(luminanceRaw);

  snapshot.motionDensityScore = clamp01(motionRaw);



  const std::array<std::pair<const char *, double>, 4> densityScores{{

      {"visual", snapshot.visualDensityScore},

      {"information", snapshot.informationDensityScore},

      {"luminance", snapshot.luminanceDensityScore},

      {"motion", snapshot.motionDensityScore},

  }};

  const auto dominantIt = std::max_element(

      densityScores.begin(), densityScores.end(),

      [](const auto &a, const auto &b) { return a.second < b.second; });

  const QString dominantAxis = QString::fromLatin1(dominantIt->first);

  const double dominantScore = dominantIt->second;

  snapshot.densityLabel = densityLevelFromScore(dominantScore);

  snapshot.densityWarning =

      densityWarningForDominantAxis(dominantAxis, dominantScore);

  snapshot.densityNextAction = densityNextActionForAxis(dominantAxis);



  ArtifactCore::TraceRecorder::instance().recordFrameDebugSnapshot(snapshot);

  finalizeTraceScope();

  return snapshot;

}



double CompositionRenderController::lastFrameTimeMs() const {

  return impl_ ? impl_->lastFrameTimeMs_ : 0.0;

}



double CompositionRenderController::averageFrameTimeMs() const {

  return impl_ ? impl_->averageFrameTimeMs_ : 0.0;

}

QString CompositionRenderController::Impl::previewRenderPipelineSlotStateText(

    const PreviewRenderPipelineSlot& slot) {

  switch (slot.state) {

  case PreviewRenderPipelineSlot::State::Free:

    return QStringLiteral("free");

  case PreviewRenderPipelineSlot::State::Retirable:

    return QStringLiteral("retirable");

  case PreviewRenderPipelineSlot::State::Ready:

    return QStringLiteral("ready");

  case PreviewRenderPipelineSlot::State::Submitted:

    return QStringLiteral("submitted");

  }

  return QStringLiteral("unknown");

}

quint64 CompositionRenderController::Impl::estimatePreviewRenderPipelineSlotBytes(

    const PreviewRenderPipelineSlot& slot) {

  const int width = std::max(

      static_cast<int>(slot.pipeline.width()),

      slot.depthTargetSize.width());

  const int height = std::max(

      static_cast<int>(slot.pipeline.height()),

      slot.depthTargetSize.height());

  if (width <= 0 || height <= 0) {

    return 0;

  }



  constexpr quint64 kRgba16fBytesPerPixel = 8;

  constexpr quint64 kRgba8BytesPerPixel = 4;

  constexpr quint64 kDepth32BytesPerPixel = 4;

  const quint64 pixelCount =

      static_cast<quint64>(width) * static_cast<quint64>(height);

  constexpr quint64 kPreviewPipelineColorTargetCount = 4;

  const quint64 optionalEmissionBytes = slot.pipeline.hasEmissionTarget()

                                            ? pixelCount * kRgba16fBytesPerPixel

                                            : 0;

  return pixelCount *

         ((kRgba16fBytesPerPixel * kPreviewPipelineColorTargetCount) +

          kRgba8BytesPerPixel + kDepth32BytesPerPixel) +

         optionalEmissionBytes;

}

QString CompositionRenderController::Impl::previewRenderPipelineSlotsSummary() const {

  QStringList slotNotes;

  slotNotes.reserve(kPreviewRenderPipelineSlotCount);

  for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

    const auto& slot = previewRenderPipelineSlots_[i];

    const QSize pipelineSize(slot.pipeline.width(), slot.pipeline.height());

    const QSize slotSize = pipelineSize.isValid() ? pipelineSize

                                                  : slot.depthTargetSize;

    slotNotes << QStringLiteral("#%1:%2:%3x%4:depth=%5:emission=%6:frame=%7")

                     .arg(i)

                     .arg(previewRenderPipelineSlotStateText(slot))

                     .arg(std::max(0, slotSize.width()))

                     .arg(std::max(0, slotSize.height()))

                     .arg(slot.depthTargetView ? 1 : 0)

                     .arg(slot.pipeline.hasEmissionTarget() ? 1 : 0)

                     .arg(slot.lastSubmittedFrame);

  }

  return slotNotes.join(QStringLiteral(","));

}

quint64 CompositionRenderController::Impl::previewRenderPipelineEstimatedBytes() const {

  quint64 total = 0;

  for (const auto& slot : previewRenderPipelineSlots_) {

    total += estimatePreviewRenderPipelineSlotBytes(slot);

  }

  return total;

}

QString CompositionRenderController::Impl::previewRenderPipelinePressureSummary() const {

  return QStringLiteral("acq=%1 free=%2 reuseRetirable=%3 reuseReady=%4 "

                        "reuseSubmitted=%5 reuseStreak=%6 reuseStreakMax=%7")

      .arg(previewRenderPipelineAcquireCount_)

      .arg(previewRenderPipelineFreeAcquireCount_)

      .arg(previewRenderPipelineReuseRetirableCount_)

      .arg(previewRenderPipelineReuseReadyCount_)

      .arg(previewRenderPipelineReuseSubmittedCount_)

      .arg(previewRenderPipelineConsecutiveReuseCount_)

      .arg(previewRenderPipelineMaxConsecutiveReuseCount_);

}

int CompositionRenderController::Impl::previewRenderPipelineSlotCountByState(

    const PreviewRenderPipelineSlot::State state) const {

  int count = 0;

  for (const auto& slot : previewRenderPipelineSlots_) {

    if (slot.state == state) {

      ++count;

    }

  }

  return count;

}

QString CompositionRenderController::Impl::previewRenderPipelineAcquirePolicySummary() const {

  const int freeCount = previewRenderPipelineSlotCountByState(

      PreviewRenderPipelineSlot::State::Free);

  const int retirableCount = previewRenderPipelineSlotCountByState(

      PreviewRenderPipelineSlot::State::Retirable);

  const int readyCount = previewRenderPipelineSlotCountByState(

      PreviewRenderPipelineSlot::State::Ready);

  const int submittedCount = previewRenderPipelineSlotCountByState(

      PreviewRenderPipelineSlot::State::Submitted);

  QString risk = QStringLiteral("safe");

  if (submittedCount == kPreviewRenderPipelineSlotCount) {

    risk = QStringLiteral("submitted-only");

  } else if (submittedCount > 0 && freeCount == 0 && retirableCount == 0) {

    risk = QStringLiteral("submitted-pressure");

  } else if (freeCount == 0 && retirableCount == 0) {

    risk = QStringLiteral("ready-pressure");

  } else if (freeCount == 0) {

    risk = QStringLiteral("reuse-preferred");

  }



  return QStringLiteral("risk=%1 free=%2 retirable=%3 ready=%4 submitted=%5")

      .arg(risk)

      .arg(freeCount)

      .arg(retirableCount)

      .arg(readyCount)

      .arg(submittedCount);

}

namespace CompositionRenderControllerDetail {

QString densityAxisDisplayName(const QString &axis)

{

  if (axis == QStringLiteral("visual")) {

    return QStringLiteral("visual");

  }

  if (axis == QStringLiteral("information")) {

    return QStringLiteral("info");

  }

  if (axis == QStringLiteral("luminance")) {

    return QStringLiteral("luma");

  }

  if (axis == QStringLiteral("motion")) {

    return QStringLiteral("motion");

  }

  return QStringLiteral("density");

}













} // namespace CompositionRenderControllerDetail

namespace CompositionRenderControllerDetail {

qint64 latestTimerMs(const std::string& timerName)

{

  const auto latest = ArtifactCore::PerformanceRegistry::instance().getLatestSamples();

  const auto it = latest.find(timerName);

  if (it == latest.end()) {

    return 0;

  }

  return static_cast<qint64>(std::llround(it->second.durationMs));

}

} // namespace CompositionRenderControllerDetail

} // namespace Artifact
