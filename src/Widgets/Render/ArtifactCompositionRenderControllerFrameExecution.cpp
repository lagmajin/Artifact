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
#include <QGuiApplication>
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

namespace CompositionRenderControllerDetail {

static bool renderCrashTraceEnabled()

{

  static const bool enabled =

      qEnvironmentVariableIsSet("ARTIFACT_RENDER_TRACE_CRASH") &&

      qEnvironmentVariable("ARTIFACT_RENDER_TRACE_CRASH") != QStringLiteral("0");

  return enabled;

}

static bool continuousRenderDiagnosticsEnabled()

{

  static const bool enabled =

      qEnvironmentVariableIsSet("ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS") &&

      qEnvironmentVariable("ARTIFACT_ENABLE_CONTINUOUS_RENDER_DIAGNOSTICS") !=

          QStringLiteral("0");

  return enabled;

}

static QString toolTypeToOverlayLabel(ToolType toolType)

{

  switch (toolType) {

  case ToolType::Selection:

    return QStringLiteral("Select");

  case ToolType::Hand:

    return QStringLiteral("Hand");

  case ToolType::Zoom:

    return QStringLiteral("Zoom");

  case ToolType::Rotation:

    return QStringLiteral("Rotate");

  case ToolType::AnchorPoint:

    return QStringLiteral("Anchor");

  case ToolType::Pen:

    return QStringLiteral("Mask");

  case ToolType::Text:

    return QStringLiteral("Text");

  case ToolType::Shape:

    return QStringLiteral("Shape");

  case ToolType::Rectangle:

    return QStringLiteral("Rect");

  case ToolType::Ellipse:

    return QStringLiteral("Ellipse");

  case ToolType::Move:

    return QStringLiteral("Move");

  case ToolType::Scale:

    return QStringLiteral("Scale");

  case ToolType::Ripple:

    return QStringLiteral("Ripple");

  case ToolType::Rolling:

    return QStringLiteral("Rolling");

  case ToolType::Slip:

    return QStringLiteral("Slip");

  case ToolType::Slide:

    return QStringLiteral("Slide");

  case ToolType::MotionSketch:

    return QStringLiteral("Sketch");

  case ToolType::Puppet:

    return QStringLiteral("Puppet");

  case ToolType::RigSelect:

    return QStringLiteral("Rig Select");

  case ToolType::RigWeight:

    return QStringLiteral("Rig Weight");

  case ToolType::TrackPoint:

    return QStringLiteral("Track Point");

  case ToolType::Brush:

    return QStringLiteral("Brush");

  case ToolType::Fill:

    return QStringLiteral("Fill");

  case ToolType::RotoBrush:

    return QStringLiteral("Roto Brush");

  case ToolType::Clone:

    return QStringLiteral("Clone Stamp");

  case ToolType::Eraser:

    return QStringLiteral("Eraser");

  case ToolType::ScrubPreview:

    return QStringLiteral("Scrub Preview");

  }

  return QStringLiteral("Tool");

}

static bool floatColorEquals(const FloatColor &lhs, const FloatColor &rhs,

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

static bool layerNeedsFrameSyncForCompositionView(ArtifactAbstractLayer *layer) {

  if (!layer) {

    return false;

  }



  // Animated playback-critical layers still need their frame propagated.

  if (dynamic_cast<ArtifactVideoLayer *>(layer) ||

      layer->isParticleLayer() ||

      dynamic_cast<ArtifactCompositionLayer *>(layer)) {

    return true;

  }



  if (layer->revealEnabled() || layer->hasSourceTimeMapping() || layer->hasMasks() ||

      layer->hasModifiers() ||

      layer->effectCount() > 0) {

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

        property && property->hasKeyFrames()) {

      return true;

    }

  }



  if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(layer)) {

    if (textLayer->animatorCount() > 0) {

      return true;

    }

  }



  return false;

}

} // namespace CompositionRenderControllerDetail

static bool gpuFrameProfilingEnabled()

{

  static const bool enabled =

      qEnvironmentVariableIsSet("ARTIFACT_ENABLE_GPU_FRAME_QUERY") &&

      qEnvironmentVariable("ARTIFACT_ENABLE_GPU_FRAME_QUERY") !=

          QStringLiteral("0");

  return enabled;

}

void CompositionRenderController::renderOneFrame() {
  impl_->recordInteractionPerfEvent(6);

  if (!impl_->initialized_ || !impl_->renderer_) {

    return;

  }

  if (!impl_->running_) {

    return;

  }

  const auto hostSnapshot = impl_->captureHostFrameSnapshot(this);
  if (hostSnapshot.hasHost && !hostSnapshot.visible) return;

  // When a RotoBrush base frame exists, advance its matte before drawing the
  // next frame so playback and scrubbing use the propagated mask.
  if (impl_->rotoBrushMaskIndex_ >= 0 &&
      impl_->rotoBrushLayerId_ == impl_->selectedLayerId_) {
    (void)propagateRotoBrushToCurrentFrame();
  }

  // Re-entrancy guard: renderOneFrameImpl must not be called recursively.

  if (impl_->renderScheduled_) {

    return;

  }

  impl_->renderScheduled_ = true;

  if (!impl_->prepareHostFrame(this, hostSnapshot)) {
    impl_->renderScheduled_ = false;
    return;
  }

  impl_->renderOneFrameImpl(this, hostSnapshot, true);
  impl_->recordInteractionPerfEvent(8);

  impl_->renderScheduled_ = false;

}

quintptr CompositionRenderController::lastFrameExecutionThreadId() const noexcept {
  return impl_ ? impl_->lastFrameExecutionThreadId_.load(
                     std::memory_order_acquire)
               : 0;
}



void CompositionRenderController::markRenderDirty() {

  impl_->failedFrameRetryIssued_.store(false, std::memory_order_release);

  impl_->renderDirty_.store(true, std::memory_order_release);

  if (impl_->running_ && impl_->renderTickDriver_ &&

      !impl_->renderTickDriver_->isRunning()) {

    impl_->renderTickDriver_->start();

  }

}

CompositionRenderController::Impl::HostFrameSnapshot
CompositionRenderController::Impl::captureHostFrameSnapshot(
    CompositionRenderController *owner) {
  HostFrameSnapshot snapshot;
  auto *host = hostWidget_.data();
  snapshot.viewportInteracting = viewportInteracting_;
  snapshot.interactiveRenderRegionActive = interactiveRenderRegionActive_;
  snapshot.interactiveRenderRegionRect = interactiveRenderRegionRect_;
  snapshot.interactiveRenderRegionResolutionScale =
      interactiveRenderRegionResolutionScale_;
  if (owner) {
    snapshot.cancelInvalidGizmoSession =
        !gizmoSessionEnding_ && !gizmoSessionValid();
    snapshot.showDebugProbeLines =
        owner->isLineDebugKindVisible(LineDebugKind::DebugProbe);
    snapshot.showMaskPathLines =
        owner->isLineDebugKindVisible(LineDebugKind::MaskPath);
    snapshot.showMaskHandleLines =
        owner->isLineDebugKindVisible(LineDebugKind::MaskHandle);
    snapshot.showBoundsLines =
        owner->isLineDebugKindVisible(LineDebugKind::Bounds);
    snapshot.showSelectionRectLines =
        owner->isLineDebugKindVisible(LineDebugKind::SelectionRect);
  }
  const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
  snapshot.shiftModifierActive = modifiers.testFlag(Qt::ShiftModifier);
  snapshot.altModifierActive = modifiers.testFlag(Qt::AltModifier);
  snapshot.controlModifierActive = modifiers.testFlag(Qt::ControlModifier);

  const auto *app = ArtifactApplicationManager::instance();
  const auto *toolManager = app ? app->toolManager() : nullptr;
  const ToolType activeTool = toolManager
      ? toolManager->activeTool() : ToolType::Selection;
  snapshot.activeToolLabel = toolTypeToOverlayLabel(activeTool);
  snapshot.trackerToolActive = activeTool == ToolType::TrackPoint;
  switch (activeTool) {
  case ToolType::TrackPoint:
    snapshot.activeToolKind = CompositionHostToolKind::TrackPoint;
    break;
  case ToolType::RigSelect:
    snapshot.activeToolKind = CompositionHostToolKind::RigSelect;
    break;
  case ToolType::RigWeight:
    snapshot.activeToolKind = CompositionHostToolKind::RigWeight;
    break;
  case ToolType::Clone:
    snapshot.activeToolKind = CompositionHostToolKind::Clone;
    break;
  case ToolType::Pen:
    snapshot.activeToolKind = CompositionHostToolKind::Pen;
    break;
  case ToolType::Rectangle:
    snapshot.activeToolKind = CompositionHostToolKind::Rectangle;
    break;
  default:
    snapshot.activeToolKind = CompositionHostToolKind::Other;
    break;
  }
  if (app) {
    if (auto *selectionManager = app->layerSelectionManager()) {
      snapshot.selectedLayerCount = static_cast<int>(
          selectionManager->selectedLayers().size());
    }
  }

  auto composition = previewPipeline_.composition();
  // Resolve a missing editor binding while capturing the request on the GUI
  // owner. The render body should consume an established Composition rather
  // than query ProjectService and mutate the pipeline during GPU submission.
  if (!composition && owner && initialized_ && running_ &&
      (!host || host->isVisible())) {
    if (auto *service = ArtifactProjectService::instance()) {
      composition = resolvePreferredComposition(service);
      if (composition) {
        // Use the controller's normal binding path so project state,
        // playback ownership, layer subscriptions, and renderer caches are
        // synchronized before the frame packet is captured.
        owner->setComposition(composition);
        qCDebug(compositionViewLog)
            << "[CompositionView] frame capture resolved missing composition"
            << "id=" << composition->id().toString()
            << "layers=" << composition->allLayerRef().size();
      }
    }
  }
  if (composition) {
    snapshot.compositionRevision = composition->revision();
    snapshot.sceneMutationGeneration =
        sceneMutationGeneration_.load(std::memory_order_acquire);
    snapshot.compositionSize = composition->effectiveCompositionSize();
    snapshot.compositionSizeCaptured = true;
    snapshot.compositionFramePosition =
        composition->framePosition().framePosition();
    snapshot.compositionFrameCaptured = true;

  }
  if (const auto *playback = ArtifactPlaybackService::instance()) {
    if (QThread::currentThread() == playback->thread()) {
      snapshot.playbackRunning = playback->isPlaying();
      (void)captureCompositionPlaybackFrameSnapshot(
          snapshot, composition, playback);
    }
  }
  if (renderer_) {
    snapshot.viewportZoom = renderer_->getZoom();
    renderer_->getPan(snapshot.viewportPanX, snapshot.viewportPanY);
    snapshot.viewportTransformCaptured = true;
  }

  if (host) {
    snapshot.hasHost = true;
    snapshot.visible = host->isVisible();
    snapshot.logicalSize = host->size();
    snapshot.devicePixelRatio = host->devicePixelRatio();
    const bool captureWindowId = renderFrameCounter_ <= 5 ||
        (renderer_ &&
         renderer_->lastPresentStatus() != QStringLiteral("ok"));
    if (captureWindowId) {
      snapshot.windowId = static_cast<quintptr>(host->winId());
      snapshot.windowIdCaptured = true;
    }
  }
  (void)captureCompositionViewportCamera(snapshot);
  return snapshot;
}

bool CompositionRenderController::Impl::prepareHostFrame(
    CompositionRenderController *owner,
    const HostFrameSnapshot &snapshot) {
  if (snapshot.hasHost && !snapshot.visible) {
    renderCrashTrace("render-skip-host-hidden", renderFrameCounter_);
    return false;
  }
  if (owner && snapshot.cancelInvalidGizmoSession) {
    owner->cancelGizmoInteraction();
  }
  if (!snapshot.hasHost || !renderer_ || !renderer_->hasSwapChain()) {
    return true;
  }

  const float curW = static_cast<float>(snapshot.logicalSize.width()) *
                     snapshot.devicePixelRatio;
  const float curH = static_cast<float>(snapshot.logicalSize.height()) *
                     snapshot.devicePixelRatio;
  if (std::abs(curW - hostWidth_) <= 0.5f &&
      std::abs(curH - hostHeight_) <= 0.5f) {
    return true;
  }

  qCDebug(compositionViewLog)
      << "[CompositionView] Widget size changed, scheduling swapchain update:"
      << curW << "x" << curH;
  pendingResizeSize_ = QSize();
  if (owner) {
    renderCrashTrace("render-resize-begin", renderFrameCounter_,
                     QStringLiteral("size=%1x%2")
                         .arg(snapshot.logicalSize.width())
                         .arg(snapshot.logicalSize.height()));
    owner->setViewportSize(ArtifactCore::Coordinates::screenLogicalExtent(
        static_cast<float>(snapshot.logicalSize.width()),
        static_cast<float>(snapshot.logicalSize.height())));
    auto *host = hostWidget_.data();
    if (!host) {
      renderCrashTrace("render-resize-host-lost", renderFrameCounter_);
      return false;
    }
    owner->recreateSwapChain(host);
    owner->markRenderDirty();
    renderCrashTrace("render-resize-end", renderFrameCounter_);
    // Resize and swapchain recreation are synchronous on the GUI thread. Render
    // this requested frame against the new swapchain instead of returning with
    // no frame serial advance; the render ticker may also schedule a later one.
    return renderer_ && renderer_->hasSwapChain();
  }
  return false;
}

void CompositionRenderController::Impl::renderOneFrameImpl(
    CompositionRenderController *owner,
    const HostFrameSnapshot &hostSnapshot,
    bool forceRender) {

  lastFrameExecutionThreadId_.store(
      reinterpret_cast<quintptr>(QThread::currentThreadId()),
      std::memory_order_release);

  renderCrashTrace("render-enter", renderFrameCounter_);

  if (!owner || !initialized_ || !renderer_ || !running_) {

    renderCrashTrace("render-skip-not-ready", renderFrameCounter_);

    return;

  }

  // Swapchain may not exist yet (deferred from 0×0 init).

  // Skip rendering — the first resize will create the swapchain and

  // trigger a new frame via the debounce timer.

  if (!renderer_->hasSwapChain()) {

    renderCrashTrace("render-skip-no-swapchain", renderFrameCounter_);

    return;

  }

  pointwiseAppliedCount_ = 0;
  giDispatchedCount_ = 0;
  precompFastPathSkipCount_ = 0;
  adjustmentFallbackCount_ = 0;
  compositeFinalizedThisFrame_ = false;
  viewportCullingStats_ = CompositionRenderController::ViewportCullingStats{};



  // 変更検出器のデバッグログ (カテゴリ制御)

  static int renderCount = 0;

  if (renderCount++ % 60 == 0) { // 2秒に1回

    qCDebug(compositionViewLog)

        << "[CompositionChangeDetector]" << changeDetector_.debugInfo();

  }

  // P0-3b.1: synchronize the owned RenderContext with the current frame
  // state. The order matters: setViewportSize first so the subsequent
  // setROI() can compute scissorROI against the latest viewport height.
  // The final setROI() invocation is what triggers updateViewportROI()
  // and updateScissorROI() in one shot; calling them explicitly earlier
  // would double the work.
  {
    const float liveZoom = std::max(
        0.001f, hostSnapshot.viewportTransformCaptured
                     ? hostSnapshot.viewportZoom
                     : renderer_->getZoom());
    float livePanX = hostSnapshot.viewportTransformCaptured
        ? hostSnapshot.viewportPanX : 0.0f;
    float livePanY = hostSnapshot.viewportTransformCaptured
        ? hostSnapshot.viewportPanY : 0.0f;
    if (!hostSnapshot.viewportTransformCaptured) {
      renderer_->getPan(livePanX, livePanY);
    }
    const auto composition = previewPipeline_.composition();
    const QSize compositionSize = composition
        ? composition->effectiveCompositionSize()
        : QSize();
    const float compositionW = static_cast<float>(
        compositionSize.width() > 0 ? compositionSize.width() : 1920);
    const float compositionH = static_cast<float>(
        compositionSize.height() > 0 ? compositionSize.height() : 1080);
    renderContext_.viewportSize = QSize(
        static_cast<int>(hostWidth_), static_cast<int>(hostHeight_));
    renderContext_.canvasSize = QSizeF(compositionW, compositionH);
    renderContext_.setZoom(liveZoom);
    renderContext_.setPan(livePanX, livePanY);
    // P0-3d: when an IRR is active, RenderContext::mode must reflect the
    // forced Preview preset (decision doc D4). Otherwise the renderer's
    // RenderModeSettings (useScissorTest/useROICache) would revert to the
    // Final-mode defaults that disable ROI handling.
    renderContext_.setMode(hostSnapshot.interactiveRenderRegionActive
                               ? RenderMode::Preview
                               : RenderMode::Editor);
    renderContext_.setResolutionScale(
        hostSnapshot.interactiveRenderRegionActive
            ? hostSnapshot.interactiveRenderRegionResolutionScale
            : 1.0f);
    if (hostSnapshot.interactiveRenderRegionActive) {
      // setROI() runs updateViewportROI() and updateScissorROI()
      // internally; an empty input produces empty outputs which the
      // downstream renderer treats as full-frame.
      renderContext_.setROI(RenderROI(hostSnapshot.interactiveRenderRegionRect));
    } else {
      renderContext_.setROI(RenderROI());
    }
  }



  struct RenderCostCaptureGuard {

    ArtifactIRenderer* renderer = nullptr;

    quint64 frame = 0;

    bool captureCost = false;

    bool profileGpu = false;

    RenderCostCaptureGuard(ArtifactIRenderer* r, quint64 f,
                           bool captureCostDiagnostics,
                           bool profileGpuFrame)

        : renderer(r), frame(f), captureCost(captureCostDiagnostics),
          profileGpu(profileGpuFrame) {

      if (renderer && captureCost) {

        renderer->beginFrameCostCapture();

      }

      if (renderer && profileGpu) {

        renderer->beginFrameGpuProfiling(frame);

      }

    }

    void finish() {

      if (!renderer || (!captureCost && !profileGpu)) {

        return;

      }

      if (profileGpu) {
        renderCrashTrace("render-cost-guard-gpu-end-begin", frame);

        renderer->endFrameGpuProfiling();

        renderCrashTrace("render-cost-guard-gpu-end-end", frame);
      }

      if (captureCost) {
        renderCrashTrace("render-cost-guard-cost-end-begin", frame);

        renderer->endFrameCostCapture();

        renderCrashTrace("render-cost-guard-cost-end-end", frame);
      }

      captureCost = false;

      profileGpu = false;

    }

    ~RenderCostCaptureGuard() {

      finish();

    }

  };



  QElapsedTimer frameTimer;

  frameTimer.start();
  static const bool profileVideoPreviewSetup = [] {
    return qEnvironmentVariableIsSet("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") &&
           qEnvironmentVariable("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") !=
               QStringLiteral("0");
  }();
  const bool profilePlaybackSetup = profileVideoPreviewSetup &&
      hostSnapshot.playbackRunning && playbackSetupProfileCount_ < 64;
  if (profilePlaybackSetup) ++playbackSetupProfileCount_;
  const bool profileThisVideoPreviewSetup = profileVideoPreviewSetup &&
      (renderFrameCounter_ < 32 || profilePlaybackSetup);
  std::array<qint64, 17> videoPreviewSetupMarksNs{};
  const auto markVideoPreviewSetupStage = [&](const std::size_t index) {
    if (profileThisVideoPreviewSetup &&
        index < videoPreviewSetupMarksNs.size()) {
      videoPreviewSetupMarksNs[index] = frameTimer.nsecsElapsed();
    }
  };
  markVideoPreviewSetupStage(0);
  recordInteractionPerfEvent(7);

  // Cost/trace/RenderGraph diagnostics are opt-in.  Sampling them every 30
  // frames creates a visible CPU spike during otherwise steady interaction.
  const bool captureRenderDiagnostics = continuousRenderDiagnosticsEnabled();
  const bool profileGpuFrame = gpuFrameProfilingEnabled();

  qint64 phaseNs = 0;
  qint64 lastPhaseDurationNs = 0;

  auto markPhaseMs = [&frameTimer, &phaseNs, &lastPhaseDurationNs,
                      profileThisVideoPreviewSetup]() -> qint64 {

    const qint64 nowNs = frameTimer.nsecsElapsed();

    const qint64 phaseDurationNs = nowNs - phaseNs;
    lastPhaseDurationNs = profileThisVideoPreviewSetup
        ? phaseDurationNs
        : 0;
    const qint64 phaseMs = phaseDurationNs / 1000000;

    phaseNs = nowNs;

    return phaseMs;

  };



  struct TraceScopeGuard {

    ArtifactCore::TraceScopeRecord scope;

    QElapsedTimer timer;

    bool enabled = false;

    explicit TraceScopeGuard(bool capture) : enabled(capture) {

      scope.name = QStringLiteral("CompositionRenderController::renderOneFrameImpl");

      scope.domain = ArtifactCore::TraceDomain::Render;

      scope.startNs = 0;

      timer.start();

    }

    ~TraceScopeGuard() {

      if (!enabled) {

        return;

      }

      scope.endNs = timer.nsecsElapsed();

      if (scope.endNs <= scope.startNs) {

        scope.endNs = scope.startNs + 1;

      }

      ArtifactCore::TraceRecorder::instance().recordScope(scope);

    }

  } traceGuard(captureRenderDiagnostics);



  renderer_->setSceneLights(std::vector<ArtifactCore::Light>{});

  // Each frame owns its channel display surface.  In particular, component
  // modes (RGB/Alpha) populate this in finalizeGpuRenderToViewport(); do not
  // let an older frame survive if this frame falls back before finalization.
  viewportChannelDisplaySRV_ = nullptr;

  renderCrashTrace("render-after-scene-lights", renderFrameCounter_);



  const auto comp = previewPipeline_.composition();

  if (!comp) {

    renderCrashTrace("render-no-comp-present-begin", renderFrameCounter_);

    renderer_->setOverrideRTV(nullptr);

    renderer_->setClearColor(viewportClearColor_);

    renderer_->clearRenderTarget(viewportClearColor_);

    renderer_->present();
    if (gpuTextureCacheManager_) gpuTextureCacheManager_->endFrame();

    renderCrashTrace("render-no-comp-present-end", renderFrameCounter_);

    return;

  }

  bool irrScissorApplied = false;
  if (hostSnapshot.interactiveRenderRegionActive &&
      !renderContext_.scissorROI.isEmpty()) {
    const RenderROI &scissor = renderContext_.scissorROI;
    const float targetWidth =
        std::max(1.0f, static_cast<float>(hostWidth_));
    const float targetHeight =
        std::max(1.0f, static_cast<float>(hostHeight_));
    renderer_->setViewportRect(
        scissor.x(), scissor.y(), std::max(1.0f, scissor.width()),
        std::max(1.0f, scissor.height()), targetWidth, targetHeight);
    irrScissorApplied = true;
  }

  // Normal composition changes seed this cache in setComposition(). The lazy
  // path covers a controller that adopted a composition through the fallback
  // route above without adding an O(layer-count) operation to every frame.
  const qint64 damageBoundsRefreshStartNs =
      profileThisVideoPreviewSetup ? frameTimer.nsecsElapsed() : 0;
  const bool refreshDamageBounds = !layerDamageBoundsInitialized_;
  if (refreshDamageBounds) {

    resetLayerDamageBounds(comp);

  }
  if (profileThisVideoPreviewSetup && refreshDamageBounds) {
    std::fprintf(stderr,
                 "[VideoPreviewSetupProfile] frame=%llu "
                 "damageBoundsRefreshMs=%.2f\n",
                 static_cast<unsigned long long>(renderFrameCounter_),
                 static_cast<double>(frameTimer.nsecsElapsed() -
                                     damageBoundsRefreshStartNs) /
                     1000000.0);
  }



  RenderCostCaptureGuard renderCostGuard(

      renderer_.get(), renderFrameCounter_, captureRenderDiagnostics,
      profileGpuFrame);

  markVideoPreviewSetupStage(1);

  renderCrashTrace("render-cost-begin", renderFrameCounter_);



  const QSize compSize = comp->settings().compositionSize();

  const float cw =

      static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);

  const float ch =

      static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);

  const FloatColor bgColor = comp->backgroundColor();

  lastCanvasWidth_ = cw;

  lastCanvasHeight_ = ch;

  if (gpuTextureCacheManager_) {

    gpuTextureCacheManager_->beginFrame(renderFrameCounter_);

  }



  // --- Profiler frame begin ---

  {

    const bool isPlayback = hostSnapshot.playbackRunning;

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

  // Pass resolution in one place: render size drives pipeline targets and
  // pass dispatches, view size (float, like cw/ch) drives presentation.
  // Downsampled passes derive from halfWidth()/halfHeight().
  struct CompositionPassResolution {
    int renderWidth = 1;
    int renderHeight = 1;
    float viewWidth = 1.0f;
    float viewHeight = 1.0f;
    float renderWidthF() const { return static_cast<float>(renderWidth); }
    float renderHeightF() const { return static_cast<float>(renderHeight); }
    unsigned renderWidthU() const { return static_cast<unsigned>(renderWidth); }
    unsigned renderHeightU() const {
      return static_cast<unsigned>(renderHeight);
    }
    int halfWidth() const { return std::max(1, (renderWidth + 1) / 2); }
    int halfHeight() const { return std::max(1, (renderHeight + 1) / 2); }
  };

  const int effectivePreviewDownsample =

      hostSnapshot.viewportInteracting

          ? std::max(previewDownsample_, interactivePreviewDownsampleFloor_)

          : 1;

  const bool draftRendering =

      hostSnapshot.viewportInteracting &&

      effectivePreviewDownsample > 1;

  const int previewRenderWidth = std::max(

      1, static_cast<int>(std::ceil(

             viewportW / static_cast<float>(effectivePreviewDownsample))));

  const int previewRenderHeight = std::max(

      1, static_cast<int>(std::ceil(

             viewportH / static_cast<float>(effectivePreviewDownsample))));

  const CompositionPassResolution passResolution{
      previewRenderWidth, previewRenderHeight, cw, ch};

  // Keep the settled GPU blend path at the same physical resolution as the

  // direct Normal path. Downsampling is an interaction-only optimization;

  // otherwise Add/Multiply look permanently softer than Normal.

  if (compositionRenderer_) {

    compositionRenderer_->SetCompositionSize(cw, ch);

    // Note: ApplyCompositionSpace sets renderer canvas size to FULL size.

    // We override it below if pipeline is enabled.

    compositionRenderer_->ApplyCompositionSpace();

  } else {

    renderer_->setCanvasSize(cw, ch);

  }



  const auto &layers = comp->allLayerRef();

  const bool capturedPlaybackFrameMatchesComposition =
      hostSnapshot.playbackFrameCaptured &&
      hostSnapshot.playbackCompositionId == comp->id().toString();
  const FramePosition currentFrame = capturedPlaybackFrameMatchesComposition
      ? FramePosition(hostSnapshot.playbackFramePosition)
      : comp->framePosition();

  // Crowd and clone collision are owned by a composition-wide fixed-step
  // session. The layer-local code remains only as a fallback for isolated
  // layer previews that do not have a composition owner.
  const qint64 componentSimulationStartNs =
      profileThisVideoPreviewSetup ? frameTimer.nsecsElapsed() : 0;
  comp->evaluateLayerComponentSimulation(currentFrame, hostSnapshot.viewportInteracting);
  if (profileThisVideoPreviewSetup) {
    std::fprintf(stderr,
                 "[VideoPreviewSetupProfile] frame=%llu componentSimulationMs=%.2f\n",
                 static_cast<unsigned long long>(renderFrameCounter_),
                 static_cast<double>(frameTimer.nsecsElapsed() -
                                     componentSimulationStartNs) /
                     1000000.0);
  }



  const int sceneLightFps = std::max(

      1, static_cast<int>(std::llround(comp->frameRate().framerate())));

  const RationalTime sceneLightTime(currentFrame.framePosition(), sceneLightFps);

  std::vector<SceneLightEntry> sceneLights;

  sceneLights.reserve(layers.size());

  for (const auto &layer : layers) {

    if (!layer || !isLayerEffectivelyVisible(layer) ||

        !layer->isActiveAt(currentFrame)) {

      continue;

    }



    auto *lightLayer = dynamic_cast<ArtifactLightLayer *>(layer.get());

    if (!lightLayer) {

      continue;

    }

    sceneLights.push_back(

        SceneLightEntry{makeSceneLightFromLayer(lightLayer, sceneLightTime), lightLayer});

  }

  // MeshRenderer keeps a fixed SceneLightGpu[8] array and silently `continue`s
  // past that limit, so a composition with more lights loses whichever ones
  // happen to sit last in layer order. Report it once per frame instead of
  // letting the drop stay invisible.
  if (sceneLights.size() > kMaxForwardSceneLights) {
    reportSceneLightOverflow(sceneLights.size(), kMaxForwardSceneLights);
  }

  std::vector<ArtifactCore::Light> rendererSceneLights;

  rendererSceneLights.reserve(sceneLights.size());

  for (const auto &entry : sceneLights) {

    rendererSceneLights.push_back(entry.light);

  }

  renderer_->setSceneLights(rendererSceneLights);
  markVideoPreviewSetupStage(2);

  // The environment layer is a scene-level lighting source rather than a
  // drawable 2D layer. Select the first active environment map deterministically
  // and pass it to the mesh renderer; later layers remain available for explicit
  // multi-environment blending without changing the current contract.
  QString environmentMapPath;
  float environmentMapIntensity = 0.0f;
  float environmentMapRotation = 0.0f;
  bool environmentMapVisibleAsBackground = false;
  for (const auto& layer : layers) {
    if (!layer || !isLayerEffectivelyVisible(layer) ||
        !layer->isActiveAt(currentFrame)) {
      continue;
    }
    auto* environmentLayer =
        dynamic_cast<ArtifactEnvironmentMapLayer*>(layer.get());
    if (!environmentLayer ||
        environmentLayer->hdriPath().trimmed().isEmpty()) {
      continue;
    }
    environmentMapPath = environmentLayer->hdriPath().trimmed();
    environmentMapIntensity = environmentLayer->intensity();
    environmentMapRotation = environmentLayer->rotation();
    environmentMapVisibleAsBackground = environmentLayer->visibleAsBackground();
    break;
  }
  renderer_->setEnvironmentMap(environmentMapPath, environmentMapIntensity);
  renderer_->setEnvironmentRotation(environmentMapRotation);
  renderer_->setEnvironmentBackgroundVisible(environmentMapVisibleAsBackground);

  // Shadow maps currently support the two single-projection light types.  A
  // point light needs six faces and an area light needs an approximation, so
  // those lights continue to illuminate without being selected as casters.
  QMatrix4x4 shadowLightViewProjection;
  const ArtifactCore::Light* shadowMapLight = nullptr;
  for (const auto& entry : sceneLights) {
    if (!entry.source || !entry.source->castsShadows() ||
        !entry.light.enabled()) {
      continue;
    }
    const auto type = entry.light.type();
    if (type != ArtifactCore::LightType::Directional &&
        type != ArtifactCore::LightType::Spot) {
      continue;
    }
    const auto directionValue = entry.light.direction();
    QVector3D direction(directionValue.x, directionValue.y, directionValue.z);
    if (direction.lengthSquared() < 1.0e-6f) {
      continue;
    }
    direction.normalize();
    QVector3D up = std::abs(QVector3D::dotProduct(direction, QVector3D(0, 1, 0)))
        > 0.98f ? QVector3D(0, 0, 1) : QVector3D(0, 1, 0);
    QMatrix4x4 shadowView;
    QMatrix4x4 shadowProjection;
    if (type == ArtifactCore::LightType::Directional) {
      constexpr float shadowExtent = 2048.0f;
      shadowView.lookAt(-direction * shadowExtent, QVector3D(), up);
      shadowProjection.ortho(-shadowExtent, shadowExtent, -shadowExtent,
                             shadowExtent, 0.1f, shadowExtent * 4.0f);
    } else {
      const auto positionValue = entry.light.position();
      const QVector3D position(positionValue.x, positionValue.y, positionValue.z);
      shadowView.lookAt(position, position + direction, up);
      // Match the far plane to the light's own reach. It used to read the
      // layer's Range while the shading attenuation is derived from Cone
      // Length, so with the defaults (300 vs 500) the shadow frustum reached
      // 200px into a region the light had already stopped illuminating.
      const float farPlane = std::max(1.0f, entry.source->coneLength().value);
      shadowProjection.perspective(
          std::clamp(entry.light.spotOuterCutoff().value * 2.0f,
                     1.0f, 175.0f),
          1.0f, 0.05f, farPlane);
    }
    // Keep the light projection in the same top-down viewport convention as
    // the active 3D camera matrices used by MeshRenderer.
    shadowProjection(1, 1) = -shadowProjection(1, 1);
    shadowLightViewProjection = shadowProjection * shadowView;
    shadowMapLight = &entry.light;
    break;
  }
  renderer_->beginShadowMapFrame(shadowLightViewProjection, shadowMapLight);
  markVideoPreviewSetupStage(3);



  // Resolve each matte source at most once per rendered frame. Multiple matte

  // consumers then share the same implicitly-shared QImage until the frame is

  // complete.

  QHash<ArtifactCore::Id, QImage> matteSourceFrameCache;

  auto matteResolverLambda =

      [&layers, &matteSourceFrameCache, currentFrame](const ArtifactCore::Id &layerId)

      -> QImage {

      const auto cached = matteSourceFrameCache.constFind(layerId);

      if (cached != matteSourceFrameCache.constEnd()) {

          return cached.value();

      }

      QImage resolved;

      for (const auto &l : layers) {

          if (l && l->id() == layerId) {

              l->goToFrame(currentFrame.framePosition());

              if (auto *imgLayer = dynamic_cast<ArtifactImageLayer *>(l.get())) {

                  if (!imgLayer->sourcePath().trimmed().isEmpty() &&
                      (imgLayer->sourceAssetId().isNull() ||
                       imgLayer->sourceVersion() == 0)) {
                      break;
                  }

                  resolved = imgLayer->toQImage();

                  break;

              }

              if (auto *videoLayer = dynamic_cast<ArtifactVideoLayer *>(l.get())) {

                  resolved = videoLayer->currentFrameToQImage();

                  break;

              }

              if (auto *textLayer = dynamic_cast<ArtifactTextLayer *>(l.get())) {

                  resolved = textLayer->toQImage();

                  break;

              }

              if (auto *svgLayer = dynamic_cast<ArtifactSvgLayer *>(l.get())) {

                  resolved = svgLayer->toQImage();

                  break;

              }

          }

      }

      if (resolved.isNull()) {
          resolved = resolveProjectRenderInputImage(
              layerId, currentFrame.framePosition());
      }

      matteSourceFrameCache.insert(layerId, resolved);

      return resolved;

  };

  std::function<QImage(const ArtifactCore::Id &)> matteResolver = matteResolverLambda;



  // Find active camera layer for 3D rendering

  ArtifactCameraLayer *activeCamera = nullptr;

  for (const auto &l : layers) {

    auto layerCopy = l;

    if (auto cam = dynamic_cast<ArtifactCameraLayer *>(layerCopy.get())) {

      if (isLayerEffectivelyVisible(layerCopy) && cam->isActiveAt(currentFrame) &&
          cam->isActiveCamera() &&
          (!activeCamera || cam->cameraPriority() > activeCamera->cameraPriority())) {

        // Higher priority wins; composition order intentionally keeps the
        // existing first-match behavior when priorities are equal.
        activeCamera = cam;

      }

    }

  }



  // Compute camera matrices if we have a visible camera

  bool has3DCamera = false;

  QMatrix4x4 cameraViewMatrix;

  QMatrix4x4 cameraProjMatrix;
  QMatrix4x4 previousCameraViewMatrix;
  QMatrix4x4 previousCameraProjMatrix;
  if (activeCamera) {

    const QSize compSize = comp->settings().compositionSize();

    const float cw =

        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);

    const float ch =

        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);

    const float aspect = std::max(0.001f, cw / std::max(0.001f, ch));

    const double currentShakeTime = currentFrame.toSeconds(comp->frameRate().framerate());
    const double frameDelta =
        comp->frameRate().framerate() > 0.0
            ? 1.0 / comp->frameRate().framerate()
            : 0.0;
    activeCamera->advanceShake(currentShakeTime, frameDelta);



    cameraViewMatrix = activeCamera->viewMatrix();

    cameraProjMatrix = activeCamera->projectionMatrix(aspect);
    previousCameraViewMatrix = cameraViewMatrix;
    previousCameraProjMatrix = cameraProjMatrix;
    const int64_t currentFrameNumber = currentFrame.framePosition();
    const auto appSettings = ArtifactCore::ArtifactAppSettings::instance();
    const bool previousCameraSamplingRequired =
        (appSettings && appSettings->timelineMotionBlurActive()) ||
        (activeCamera->motionBlur() &&
         activeCamera->blurAmount().value > 0.0f) ||
        (renderer_ &&
         (renderer_->isChannelEnabled(
              ArtifactIRenderer::ChannelType::VelocityX) ||
          renderer_->isChannelEnabled(
              ArtifactIRenderer::ChannelType::VelocityY)));
    if (currentFrameNumber > 0 && previousCameraSamplingRequired) {
      comp->goToFrame(currentFrameNumber - 1);
      activeCamera->advanceShake(currentShakeTime - frameDelta, 0.0);
      previousCameraViewMatrix = activeCamera->viewMatrix();
      previousCameraProjMatrix = activeCamera->projectionMatrix(aspect);
      comp->goToFrame(currentFrameNumber);
      activeCamera->advanceShake(currentShakeTime, 0.0);
    }
    if (activeCamera->stereoMode() != StereoMode::Mono) {

      const ArtifactCore::StereoCamera stereoCamera =

          ArtifactCore::StereoCamera::fromHmd(cameraViewMatrix.inverted(),

                                              activeCamera->ipd(),

                                              activeCamera->nearClipPlane(),

                                              activeCamera->farClipPlane());

      cameraViewMatrix = stereoCamera.leftEyeView;
      const ArtifactCore::StereoCamera previousStereoCamera =
          ArtifactCore::StereoCamera::fromHmd(previousCameraViewMatrix.inverted(),
                                              activeCamera->ipd(),
                                              activeCamera->nearClipPlane(),
                                              activeCamera->farClipPlane());
      previousCameraViewMatrix = previousStereoCamera.leftEyeView;
    }

    has3DCamera = true;

  }

  // A newly created 3D layer is centered in the composition, but a normal
  // 2D composition has no camera layer yet. Keep that first 3D object visible
  // until an explicit camera or viewport orientation takes ownership.
  if (!has3DCamera) {
    const QSize compSize = comp->settings().compositionSize();
    const float fallbackWidth =
        static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920);
    const float fallbackHeight =
        static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080);
    constexpr float fallbackFovDegrees = 45.0f;
    constexpr float degreesToRadians =
        3.14159265358979323846f / 180.0f;
    const float fallbackDistance =
        (fallbackHeight * 0.5f) /
        std::tan(fallbackFovDegrees * 0.5f * degreesToRadians);
    const QVector3D fallbackTarget(fallbackWidth * 0.5f,
                                   fallbackHeight * 0.5f, 0.0f);
    cameraViewMatrix.lookAt(
        fallbackTarget + QVector3D(0.0f, 0.0f, fallbackDistance),
        fallbackTarget, QVector3D(0.0f, 1.0f, 0.0f));
    cameraProjMatrix.perspective(fallbackFovDegrees,
                                 fallbackWidth / fallbackHeight, 1.0f,
                                 100000.0f);
    cameraProjMatrix(1, 1) = -cameraProjMatrix(1, 1);
    previousCameraViewMatrix = cameraViewMatrix;
    previousCameraProjMatrix = cameraProjMatrix;
    has3DCamera = true;
  }

  viewportOrientationMatricesValid_ = false;
  {

    const float orientationViewportW = std::max(1.0f, hostWidth_);
    const float orientationViewportH = std::max(1.0f, hostHeight_);
    const float orientationZoom = std::max(
        0.001f, hostSnapshot.viewportTransformCaptured
                     ? hostSnapshot.viewportZoom
                     : renderer_->getZoom());
    float orientationPanX = hostSnapshot.viewportTransformCaptured
        ? hostSnapshot.viewportPanX : 0.0f;
    float orientationPanY = hostSnapshot.viewportTransformCaptured
        ? hostSnapshot.viewportPanY : 0.0f;
    if (!hostSnapshot.viewportTransformCaptured) {
      renderer_->getPan(orientationPanX, orientationPanY);
    }

    constexpr float orientationFovDegrees = 45.0f;
    constexpr float degreesToRadians =
        3.14159265358979323846f / 180.0f;
    const float orientationDistance =
        (orientationViewportH * 0.5f) /
        (orientationZoom *
         std::tan(orientationFovDegrees * 0.5f * degreesToRadians));
    const QPointF orientationTarget(
        (orientationViewportW * 0.5f - orientationPanX) / orientationZoom,
        (orientationViewportH * 0.5f - orientationPanY) / orientationZoom);

    const QQuaternion orientation =
        viewportOrientationNavigator_.currentOrientation();

    // P0-2 tumble pivot under cursor: when a temporary pivot is set and the
    // viewport uses a spatial orientation, feed it into the view matrix as
    // the look-at target. Front orthographic mode ignores the override.
    const QPointF effectiveTarget =
        (tumblePivotOverrideEnabled_ && usesSpatialViewportOrientation())
            ? ArtifactCore::Coordinates::toQPointF(tumblePivotCanvasPos_)
            : orientationTarget;

    if (viewportNavigationMode_ !=
        CompositionRenderController::ViewportNavigationMode::Off) {
      cameraViewMatrix = viewportNavigationViewMatrix();
    } else {
      cameraViewMatrix = viewportOrientationViewMatrix(
          orientation, effectiveTarget, orientationDistance);
    }

    const bool frontOrthographic = isFrontOrthographicViewport();
    if (frontOrthographic) {
      const float halfWidth = orientationViewportW / (2.0f * orientationZoom);
      const float halfHeight = orientationViewportH / (2.0f * orientationZoom);
      cameraProjMatrix.setToIdentity();
      cameraProjMatrix.ortho(-halfWidth, halfWidth, -halfHeight, halfHeight,
                              1.0f, 100000.0f);
      // Qt's ortho produces [-1, 1] depth; Diligent's D3D12/Vulkan
      // shaders consume [0, 1]. Keep the same corrected matrix for picking.
      for (int column = 0; column < 4; ++column) {
        cameraProjMatrix(2, column) =
            0.5f * (cameraProjMatrix(2, column) + cameraProjMatrix(3, column));
      }
      cameraProjMatrix(1, 1) = -cameraProjMatrix(1, 1);
    } else {
      cameraProjMatrix = viewportOrientationProjectionMatrix(
          orientationViewportW, orientationViewportH, orientationFovDegrees);
    }

    previousCameraViewMatrix = cameraViewMatrix;

    previousCameraProjMatrix = cameraProjMatrix;

    has3DCamera = true;
    viewportOrientationViewForOverlay_ = cameraViewMatrix;
    viewportOrientationProjectionForOverlay_ = cameraProjMatrix;
    viewportOrientationMatricesValid_ = true;

  }

  gizmo3DCameraMatricesValid_ = has3DCamera;
  if (gizmo3DCameraMatricesValid_) {
    gizmo3DViewMatrix_ = cameraViewMatrix;
    gizmo3DProjectionMatrix_ = cameraProjMatrix;
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
  markVideoPreviewSetupStage(4);

  Diligent::ITextureView* ramPreviewTexture = nullptr;
  // A retained video frame is valid for live presentation, but must not be
  // published as the completed composite for a different timeline frame.
  bool videoFramesReadyForRamPreview = true;
  bool useRamPreviewFallback = false;

  // RGB inspection must use the transparent composite before viewport
  // backgrounds and overlays are drawn, never the presented swap-chain.
  const bool viewportRgbChannelRequested =
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Red ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Green ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Blue;

  QString ramPreviewFallbackReason = QStringLiteral("no-playback-service");
  bool playbackSameComposition = false;
  ArtifactRamPreviewFrameCacheState playbackPreviewState;
  bool playbackPreviewStateValid = false;
  bool playbackPreviewPendingBuild = false;
  bool playbackAllowsRamFallbackWhilePlaying = false;
  bool playbackPlaying = false;

  const bool snapshotMatchesComposition =
      hostSnapshot.ramPreviewStateCaptured &&
      hostSnapshot.playbackCompositionId == comp->id().toString();
  if (snapshotMatchesComposition) {
    playbackSameComposition = true;
    playbackPreviewState = hostSnapshot.ramPreviewFrameState;
    playbackPreviewStateValid = true;
    playbackPreviewPendingBuild = hostSnapshot.ramPreviewPendingBuild;
    playbackAllowsRamFallbackWhilePlaying =
        hostSnapshot.ramPreviewFallbackWhilePlaying;
    playbackPlaying = hostSnapshot.playbackRunning;

    if (frameOutOfRange) {
      ramPreviewFallbackReason = QStringLiteral("frame-out-of-range");
    } else if (!playbackPreviewState.ready) {
      ramPreviewFallbackReason = ramPreviewNotReadyReason(playbackPreviewState);
    } else if (!playbackPreviewState.imageAvailable) {
      ramPreviewFallbackReason = QStringLiteral("ready-missing-image");
    } else if (!hostSnapshot.ramPreviewUpload.upload ||
               hostSnapshot.ramPreviewUpload.width <= 0 ||
               hostSnapshot.ramPreviewUpload.height <= 0) {
      ramPreviewFallbackReason = QStringLiteral("ready-missing-upload");
    } else if (hostSnapshot.viewportInteracting) {
      ramPreviewFallbackReason = QStringLiteral("viewport-interacting");
    } else if (playbackPlaying && !playbackAllowsRamFallbackWhilePlaying) {
      ramPreviewFallbackReason = QStringLiteral("playing-policy-disabled");
    } else {
      useRamPreviewFallback = true;
      ramPreviewFallbackReason = playbackPlaying
          ? QStringLiteral("ready-playing") : QStringLiteral("ready");
    }
  } else {
    ramPreviewFallbackReason = QStringLiteral("composition-mismatch");
  }

  static thread_local int64_t lastPlaybackDiagnosticFrame =
      std::numeric_limits<int64_t>::min();
  static thread_local QString lastPlaybackDiagnosticReason;
  if (playbackPlaying) {
    const bool reasonChanged =
        ramPreviewFallbackReason != lastPlaybackDiagnosticReason;
    const bool periodicFrame = framePos != lastPlaybackDiagnosticFrame &&
                               (framePos % 120 == 0);
    if (reasonChanged || periodicFrame || playbackPreviewState.failed) {
      qDebug() << "[PlaybackRender]"
               << "frame=" << framePos
               << "sameComposition=" << playbackSameComposition
               << "previewState.valid=" << playbackPreviewStateValid
               << "previewState.ready=" << playbackPreviewState.ready
               << "previewState.imageAvailable="
               << playbackPreviewState.imageAvailable
               << "previewState.failed=" << playbackPreviewState.failed
               << "previewState.reason=" << playbackPreviewState.reason
               << "pendingBuild=" << playbackPreviewPendingBuild
               << "ramPreviewFallback=" << useRamPreviewFallback
               << "fallbackReason=" << ramPreviewFallbackReason
               << "viewportInteracting=" << hostSnapshot.viewportInteracting;
      lastPlaybackDiagnosticFrame = framePos;
      lastPlaybackDiagnosticReason = ramPreviewFallbackReason;
    }
  } else {
    lastPlaybackDiagnosticFrame = std::numeric_limits<int64_t>::min();
    lastPlaybackDiagnosticReason.clear();
  }

  markVideoPreviewSetupStage(5);

  const float panX = hostSnapshot.viewportTransformCaptured
      ? hostSnapshot.viewportPanX : 0.0f;
  const float panY = hostSnapshot.viewportTransformCaptured
      ? hostSnapshot.viewportPanY : 0.0f;
  const float zoom = hostSnapshot.viewportTransformCaptured
      ? hostSnapshot.viewportZoom : renderer_->getZoom();
  const bool viewportTransformChangedSinceLastFrame =
      std::abs(lastRenderKeyState_.zoom - zoom) > 0.0001f ||
      std::abs(lastRenderKeyState_.panX - panX) > 0.5f ||
      std::abs(lastRenderKeyState_.panY - panY) > 0.5f;

  if (useRamPreviewFallback && viewportRgbChannelRequested) {
    useRamPreviewFallback = false;
    ramPreviewFallbackReason = QStringLiteral("rgb-channel-requires-live-composite");
  }
  if (useRamPreviewFallback && viewportTransformChangedSinceLastFrame) {
    useRamPreviewFallback = false;
    ramPreviewFallbackReason = QStringLiteral("viewport-transform-changing");
  }

  if (profileThisVideoPreviewSetup) {
    std::fprintf(stderr,
        "[VideoPreviewFallbackDecision] frame=%lld playbackMatch=%d "
        "stateValid=%d ready=%d image=%d upload=%d pending=%d "
        "interacting=%d playing=%d viewportTransformChanged=%d "
        "fallback=%d reason=%s\n",
        static_cast<long long>(framePos), playbackSameComposition ? 1 : 0,
        playbackPreviewStateValid ? 1 : 0,
        playbackPreviewState.ready ? 1 : 0,
        playbackPreviewState.imageAvailable ? 1 : 0,
        hostSnapshot.ramPreviewUpload.upload ? 1 : 0,
        playbackPreviewPendingBuild ? 1 : 0,
        hostSnapshot.viewportInteracting ? 1 : 0, playbackPlaying ? 1 : 0,
        viewportTransformChangedSinceLastFrame ? 1 : 0,
        useRamPreviewFallback ? 1 : 0,
        ramPreviewFallbackReason.toUtf8().constData());
  }

  if (useRamPreviewFallback) {
    const auto& uploadSnapshot = hostSnapshot.ramPreviewUpload;
    const auto frameKey = static_cast<qint64>(uploadSnapshot.contentRevision);
    const auto residentSlot = static_cast<std::uint64_t>(framePos) %
        ramPreviewResidentOwners_.size();
    ramPreviewTexture = renderer_->textureForVideoFrame(
        uploadSnapshot.width, uploadSnapshot.height,
        ramPreviewResidentOwners_[residentSlot], frameKey,
        uploadSnapshot.contentRevision, *uploadSnapshot.upload, true);
    if (!ramPreviewTexture) {
      ramPreviewTexture = renderer_->textureForVideoFrame(
          uploadSnapshot.width, uploadSnapshot.height,
          ramPreviewTextureOwner_, frameKey,
          uploadSnapshot.contentRevision, *uploadSnapshot.upload, false);
    }
    if (!ramPreviewTexture) {
      useRamPreviewFallback = false;
      ramPreviewFallbackReason = QStringLiteral("upload-texture-unavailable");
    }
  }

  if (useRamPreviewFallback) {
    // Start timing before drawing, preserving the composition-frame request
    // timestamp. Resetting tickets after present hides cached render latency.
    for (const auto& layer : layers) {
      if (layer && isLayerEffectivelyVisible(layer) && layer->isActiveAt(currentFrame)) {
        if (auto* video = dynamic_cast<ArtifactVideoLayer*>(layer.get())) {
          video->markFrameCompositionCacheReady(video->currentFrame(), false);
          video->markFrameRenderQueued(video->currentFrame());
        }
      }
    }
  }

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

      checkerboardTileSize_,

      gridSettings_.majorInterval,

      gridSettings_.subdivisions,

      static_cast<uint8_t>(gridSettings_.showMajor ? 1 : 0),

      static_cast<uint8_t>(gridSettings_.showMinor ? 1 : 0),

      static_cast<uint8_t>(gridSettings_.showAxis ? 1 : 0),

      gridSettings_.majorColor.r(),

      gridSettings_.majorColor.g(),

      gridSettings_.majorColor.b(),

      gridSettings_.majorColor.a(),

      gridSettings_.minorColor.r(),

      gridSettings_.minorColor.g(),

      gridSettings_.minorColor.b(),

      gridSettings_.minorColor.a(),

      gridSettings_.axisColor.r(),

      gridSettings_.axisColor.g(),

      gridSettings_.axisColor.b(),

      gridSettings_.axisColor.a(),

      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->mode()) : -1,

      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->hoverAxis()) : -1,

      gizmo3D_ ? static_cast<int32_t>(gizmo3D_->activeAxis()) : -1,

      static_cast<uint8_t>(gpuBlendEnabled_ ? 1 : 0),

      static_cast<uint8_t>(blendPipelineReady_ ? 1 : 0),

      static_cast<uint8_t>(showGrid_ ? 1 : 0),

      static_cast<uint8_t>(showGuides_ ? 1 : 0),

      static_cast<uint8_t>(showSafeMargins_ ? 1 : 0),

      static_cast<uint8_t>(showAnchorCenterOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showCameraFrustumOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showXRayOverlay_ ? 1 : 0),

      static_cast<uint8_t>(showIsolationOverlay_ ? 1 : 0),

      static_cast<uint8_t>(hostSnapshot.viewportInteracting ? 1 : 0),

      static_cast<int32_t>(viewportChannelDisplayMode_),

      selectedLayerId_};

  const bool forceContinuousRedraw = interactionBusy() || forceRender;

  const auto damageViewportWidth =
      static_cast<int32_t>(std::ceil(viewportW));
  const auto damageViewportHeight =
      static_cast<int32_t>(std::ceil(viewportH));

  if (!damageStateInitialized_ ||
      damageCompositionId_ != comp->id() ||
      damageFrame_ != framePos ||
      damageViewportWidth_ != damageViewportWidth ||
      damageViewportHeight_ != damageViewportHeight ||
      damageDownsample_ != effectivePreviewDownsample ||
      damageCanvasWidth_ != cw || damageCanvasHeight_ != ch) {
    damageTracker_.markFullRedraw();
    markAllPreviewSlotsFullRedraw();
    damageCompositionId_ = comp->id();
    damageFrame_ = framePos;
    damageViewportWidth_ = damageViewportWidth;
    damageViewportHeight_ = damageViewportHeight;
    damageDownsample_ = effectivePreviewDownsample;
    damageCanvasWidth_ = cw;
    damageCanvasHeight_ = ch;
    damageStateInitialized_ = true;
  }

  if (fullRedrawPending_) {
    damageTracker_.markFullRedraw();
    markAllPreviewSlotsFullRedraw();
  }

  const bool damageContinuationPending =
      damageContinuationPending_.load(std::memory_order_acquire);
  if (!forceContinuousRedraw && !damageContinuationPending &&
      currentKey == lastRenderKeyState_) {

    return;

  }

  lastRenderKeyState_ = currentKey;
  markVideoPreviewSetupStage(6);

  // A previous GPU-blend/offscreen pass may leave renderer target overrides
  // alive even when this frame chooses the direct path. Reset both targets
  // before clear() initializes the shared 2D command buffer; otherwise valid
  // layer packets are submitted to an old offscreen RTV while the swap chain
  // keeps showing only viewportClearColor_.
  renderer_->setOverrideRTV(nullptr);
  renderer_->setOverrideDSV(nullptr);

  renderer_->setClearColor(viewportClearColor_);

  renderer_->clear();



  {

    const bool hasVisible3DLayer =
        std::any_of(layers.begin(), layers.end(),
                    [&](const ArtifactAbstractLayerPtr &layer) {
                      return layer && layer->is3D() &&
                             isLayerEffectivelyVisible(layer) &&
                             layer->isActiveAt(currentFrame);
                    });



    // Opt-in (default off) blanket enable of the blend path for Normal-only

    // compositions.  The direct path already reuses cached layer surfaces, so

    // flipping this without measuring trades cached sprite draws for two

    // full-screen compute passes per layer.  See

    // docs/analysis/COMPOSITION_VIEWPORT_PERFORMANCE_AUDIT_2026-09-09.md.

    const bool gpuBlendForNormalComposition =

        gpuBlendEnabled_ && gpuBlendForNormalCompositionEnabled() &&

        !layers.empty();

    const bool hasGpuBlendJustification =

        gpuBlendForNormalComposition ||

        std::any_of(layers.begin(), layers.end(),

                    [&](const ArtifactAbstractLayerPtr &layer) {

                      if (!layer || !isLayerEffectivelyVisible(layer) ||

                          !layer->isActiveAt(currentFrame)) {

                        return false;

                      }

                       return layer->revealEnabled() || layer->layerBlendType() !=

                                  ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL;

                    });

    const bool hasCpuRasterizerUpload =

        std::any_of(layers.begin(), layers.end(),

                    [&](const ArtifactAbstractLayerPtr &layer) {

                      return layer && isLayerEffectivelyVisible(layer) &&

                             layer->isActiveAt(currentFrame) &&

                             layerHasCpuRasterizerWork(layer.get());

                    });

    const auto globalIlluminationState =
        renderer_->globalIlluminationState();
    const auto globalIlluminationSettings =
        renderer_->globalIlluminationSettings();
    const bool screenSpaceGlobalIlluminationRequested =
        globalIlluminationState.selectedMode ==
            GlobalIlluminationMode::SSGI ||
        globalIlluminationState.selectedMode ==
            GlobalIlluminationMode::Hybrid;
    const bool viewportMultiChannelRequested =
        renderer_->isMultiChannelEnabled();

    if (gpuBlendEnabled_ &&
        (hasGpuBlendJustification || hasVisible3DLayer ||
         screenSpaceGlobalIlluminationRequested ||
         viewportMultiChannelRequested || viewportRgbChannelRequested) &&
        !blendPipelineReady_) {
      scheduleBlendPipelineInitialization(
          owner, 0,
          screenSpaceGlobalIlluminationRequested
              ? QStringLiteral("screen-space-gi-requested")
              : (gpuBlendForNormalComposition
                     ? QStringLiteral("normal-layer-gpu-blend-opt-in")
                     : QStringLiteral("non-normal-layer-visible")));
    }

    const bool gpuBlendRequested = gpuBlendEnabled_ && blendPipelineReady_;

    const bool gpuBlendPathRequested =

        gpuBlendRequested &&
        (hasGpuBlendJustification || hasVisible3DLayer ||
         screenSpaceGlobalIlluminationRequested ||
         viewportMultiChannelRequested || viewportRgbChannelRequested);



    auto &previewRenderSlot = acquirePreviewRenderPipelineSlot();
    damageContinuationPending_.store(false, std::memory_order_release);

    auto &renderPipeline = previewRenderSlot.pipeline;

    const bool emissionChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::Emission);

    const bool objectIdChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId);

    const bool materialIdChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::MaterialId);

    const bool albedoChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB);

    const bool normalChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalX) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalY) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ);

    const bool velocityChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY);

    const bool positionChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::PositionX) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::PositionY) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::PositionZ);

    const bool uvChannelRequested =

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::U) ||

        renderer_->isChannelEnabled(ArtifactIRenderer::ChannelType::V);

    const bool auxiliary3DChannelRequested =

        emissionChannelRequested || objectIdChannelRequested ||

        materialIdChannelRequested || albedoChannelRequested ||

        normalChannelRequested || velocityChannelRequested ||

        positionChannelRequested || uvChannelRequested ||

        screenSpaceGlobalIlluminationRequested;



    // Motion blur reads the velocity target, so the emission-family targets
    // must exist whenever timeline motion blur is active even without any
    // auxiliary debug channel requested.
    const bool motionBlurVelocityRequested = []() {
      const auto appSettings = ArtifactCore::ArtifactAppSettings::instance();
      return appSettings && appSettings->timelineMotionBlurActive();
    }();
    // Camera-layer motion blur also consumes the velocity target.
    const bool cameraMotionBlurRequested = [&]() {
      for (const auto &l : layers) {
        if (auto cam = dynamic_cast<ArtifactCameraLayer *>(l.get())) {
          if (cam->motionBlur() && cam->blurAmount().value > 0.0f) return true;
        }
      }
      return false;
    }();
    struct CompositionPostPassMask {
      bool screenSpaceGi = false;
      bool auxiliaryTargets = false;
      bool motionBlurVelocity = false;
      bool cameraMotionBlur = false;
      bool timelineMotionBlur = false;
      bool depthOfField = false;
      int antiAliasingMode = 1;
    };
    // Post-pass on/off for this frame in one place. Values mirror the
    // pipeline-setup consts above (still used by target allocation); the
    // layer loop and the Resolve/Post passes below read the mask.
    CompositionPostPassMask postPassMask;
    postPassMask.screenSpaceGi = screenSpaceGlobalIlluminationRequested;
    postPassMask.auxiliaryTargets = auxiliary3DChannelRequested;
    postPassMask.motionBlurVelocity = motionBlurVelocityRequested;
    postPassMask.cameraMotionBlur = cameraMotionBlurRequested;
    // Same single-threaded read as motionBlurVelocityRequested above; kept
    // separate because velocity-target need != effect enable.
    postPassMask.timelineMotionBlur = []() {
      const auto appSettings = ArtifactCore::ArtifactAppSettings::instance();
      return appSettings && appSettings->timelineMotionBlurActive();
    }();
    // Depth of field consumes the active camera lens parameters.
    postPassMask.depthOfField =
        activeCamera &&
        activeCamera->depthOfFieldParameters().cocScale.value > 0.0f;
    // Anti-aliasing quality mode: 0=Off, 1=FXAA, 2=MSAA 4x.
    postPassMask.antiAliasingMode = []() {
      const auto appSettings = ArtifactCore::ArtifactAppSettings::instance();
      return appSettings ? appSettings->compositionAntiAliasingMode() : 1;
    }();

    // Auxiliary (AOV) targets, allocated on demand. Each entry mirrors its
    // draw gate below (plus SSGI inputs), so e.g. motion-blur-only frames
    // allocate velocity without the full debug bundle.
    RenderPipeline::AuxiliaryTargetRequest auxiliaryRequest;
    auxiliaryRequest.emission = emissionChannelRequested;
    auxiliaryRequest.normal =
        normalChannelRequested || postPassMask.screenSpaceGi;
    auxiliaryRequest.velocity = postPassMask.motionBlurVelocity ||
        postPassMask.cameraMotionBlur || velocityChannelRequested;
    auxiliaryRequest.motionBlurScratch = postPassMask.motionBlurVelocity ||
        postPassMask.cameraMotionBlur;
    auxiliaryRequest.objectId = objectIdChannelRequested;
    auxiliaryRequest.materialId = materialIdChannelRequested;
    auxiliaryRequest.albedo =
        albedoChannelRequested || postPassMask.screenSpaceGi;
    auxiliaryRequest.position = positionChannelRequested;
    auxiliaryRequest.uv = uvChannelRequested;

    // Avoid paying render-pipeline setup cost when GPU blending is disabled.

    if (gpuBlendPathRequested) {

      if (auto device = renderer_->device()) {

        const bool initializedWithF16 = renderPipeline.initialize(

              device,

              static_cast<Uint32>(passResolution.renderWidth),

              static_cast<Uint32>(passResolution.renderHeight),

            RenderConfig::PipelineFormatF16,

            auxiliaryRequest);

        if (!initializedWithF16) {
          qWarning() << "[CompositionView] RGBA16_FLOAT pipeline unavailable;"
                        " falling back to RGBA32_FLOAT";
          renderPipeline.initialize(

              device,

              static_cast<Uint32>(passResolution.renderWidth),

              static_cast<Uint32>(passResolution.renderHeight),

              RenderConfig::PipelineFormatF32,

              auxiliaryRequest);
        }

        // Resource preparation belongs to the pipeline setup boundary, before
        // any layer dispatch. Reveal dispatch itself only updates constants.
        bool revealPrepared = false;
        for (const auto& layer : layers) {
          if (!layer || !layer->revealEnabled() || !layer->isVisible() || !layer->isActiveAt(currentFrame)) continue;
          if (!revealPrepared) {
            if (!renderPipeline.prepareReveal(renderer_->immediateContext().RawPtr())) return;
            renderPipeline.prepareRevealMaps(renderer_->immediateContext().RawPtr(), layers);
            revealPrepared = true;
          }
          const auto map = layer->revealMapView();
          if (!map.failed && map.width > 0 && !renderPipeline.revealMapSRV(layer.get())) {
            // Keep the last presented frame while a map cannot be prepared.
            // A partial composition with a dropped layer must not be presented.
            return;
          }
        }

        if (!ensurePreviewRenderPipelineDepthSlot(

                previewRenderSlot,

                passResolution.renderWidth,

                passResolution.renderHeight)) {

          qWarning() << "[CompositionView] failed to allocate preview depth slot"

                     << "slot=" << activePreviewRenderPipelineSlot_

                     << "size="

                     << QSize(passResolution.renderWidth,
                             passResolution.renderHeight);

        }

      }

    } else if (gpuBlendRequested && lastPipelineStateMask_ != -1) {

      qCDebug(compositionViewLog)

          << "[CompositionView] GPU blend path not used for this frame"

          << "layers=" << layers.size()

          << "frameOutOfRange=" << frameOutOfRange;

    }



    const bool previewRenderSlotAcquireHazard =

        lastPreviewRenderPipelineAcquireHazard_;

    const bool pipelineEnabled =

        gpuBlendPathRequested && renderPipeline.ready() &&

        previewRenderSlot.depthTargetView != nullptr &&

        !previewRenderSlotAcquireHazard;

    Diligent::ITexture* currentAccumTexture = nullptr;
    if (auto* retainedView = renderPipeline.accumSRV()) {
      currentAccumTexture = retainedView->GetTexture();
    }

    if (pipelineEnabled && previewRenderSlot.retainedCompositeValid) {
      float currentPanX = 0.0f;
      float currentPanY = 0.0f;
      renderer_->getPan(currentPanX, currentPanY);
      const bool retainedStateMatches =
          previewRenderSlot.retainedCompositionId ==
              (comp ? comp->id() : CompositionID()) &&
          previewRenderSlot.retainedFramePosition ==
              currentFrame.framePosition() &&
          previewRenderSlot.retainedTargetSize ==
              QSize(static_cast<int>(renderPipeline.width()),
                    static_cast<int>(renderPipeline.height())) &&
          previewRenderSlot.retainedAccumTexture == currentAccumTexture &&
          previewRenderSlot.retainedDownsample == effectivePreviewDownsample &&
          std::abs(previewRenderSlot.retainedCanvasWidth - cw) <= 1.0e-4f &&
          std::abs(previewRenderSlot.retainedCanvasHeight - ch) <= 1.0e-4f &&
          std::abs(previewRenderSlot.retainedZoom - renderer_->getZoom()) <=
              1.0e-5f &&
          std::abs(previewRenderSlot.retainedPanX - currentPanX) <= 1.0e-4f &&
          std::abs(previewRenderSlot.retainedPanY - currentPanY) <= 1.0e-4f;
      if (!retainedStateMatches) {
        previewRenderSlot.retainedCompositeValid = false;
        previewRenderSlot.damageTracker.clearAll();
        previewRenderSlot.damageTracker.markFullRedraw();
        damageTracker_.markFullRedraw();
      }
    }

    if (pipelineEnabled && !previewRenderSlot.retainedCompositeValid) {
      previewRenderSlot.damageTracker.clearAll();
      previewRenderSlot.damageTracker.markFullRedraw();
      damageTracker_.markFullRedraw();
    }

    const auto isCompositionSpaceCacheLayer =
        [](const ArtifactAbstractLayerPtr& layer) {
          if (!layer || layer->is3D() || layer->isAdjustmentLayer() ||
              layer->maskCount() != 0 ||
              layer->layerBlendType() !=
                  ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
              layer->effectCount() > 0) {
            return false;
          }
          if (const auto imageLayer =
                  dynamic_cast<ArtifactImageLayer*>(layer.get())) {
            return !imageLayer->isImageSequence();
          }
          return dynamic_cast<ArtifactSolid2DLayer*>(layer.get()) ||
                 dynamic_cast<ArtifactSolidImageLayer*>(layer.get());
        };
    const auto isPartialRecomposeLayer =
        [&](const ArtifactAbstractLayerPtr& layer) {
          if (isCompositionSpaceCacheLayer(layer)) {
            return true;
          }
          return layer && !layer->is3D() && !layer->isAdjustmentLayer() &&
                 layer->maskCount() == 0 && layer->effectCount() == 0 &&
                 layer->layerBlendType() ==
                     ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL &&
                 dynamic_cast<ArtifactShapeLayer*>(layer.get());
        };

    // Keep the composition-space cache opt-in until its offscreen target
    // presentation is verified on the active GPU/backend. It is selected only
    // for ordinary solid and still-image layers; defaulting to false preserves
    // the known-good direct path while allowing focused runtime validation.
    const bool compositionSpaceCacheEligible =
        compositionSpaceGpuCachePresentationReady_ && !pipelineEnabled &&
        !frameOutOfRange && !has3DCamera && isFrontOrthographicViewport() &&
        !showIsolationOverlay_ &&
        !showXRayOverlay_ &&
        std::all_of(layers.cbegin(), layers.cend(),
                    isCompositionSpaceCacheLayer);
    float compositionCacheScale = 1.0f;
    QSize compositionCacheSize;
    QString compositionCacheKey;
    bool compositionSpaceCacheActive = false;
    bool compositionSpaceCacheHit = false;
    bool compositionSpaceCacheBuild = false;
    if (compositionSpaceCacheEligible) {
      constexpr int compositionCacheMaxPixels = 16 * 1024 * 1024;
      const float compositionArea =
          std::max(1.0f, cw) * std::max(1.0f, ch);
      compositionCacheScale = std::min(
          1.0f / std::max(1, effectivePreviewDownsample),
          std::sqrt(static_cast<float>(compositionCacheMaxPixels) /
                    compositionArea));
      compositionCacheSize = QSize(
          std::max(1, static_cast<int>(std::ceil(cw * compositionCacheScale))),
          std::max(1, static_cast<int>(std::ceil(ch * compositionCacheScale))));
      compositionCacheKey =
          QStringLiteral("%1|revision=%2|base=%3|frame=%4|size=%5x%6")
              .arg(comp->id().toString())
              .arg(comp->revision())
              .arg(baseInvalidationSerial_)
              .arg(framePos)
              .arg(compositionCacheSize.width())
              .arg(compositionCacheSize.height());
      compositionSpaceCacheActive =
          ensureCompositionSpaceGpuCache(compositionCacheSize);
      compositionSpaceCacheHit =
          compositionSpaceCacheActive &&
          compositionSpaceGpuCache_.contentKey == compositionCacheKey;
      compositionSpaceCacheBuild =
          compositionSpaceCacheActive && !compositionSpaceCacheHit;
    }

    if (!pipelineEnabled) {

      lastLayerRtPixelStats_.clear();

      lastAccumRtPixelStats_.clear();

    }



    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalX,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalY,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::NormalZ,

        (gpuBlendPathRequested && renderPipeline.hasNormalTarget())

            ? renderPipeline.normalSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::VelocityX,

        (gpuBlendPathRequested && renderPipeline.hasVelocityTarget())

            ? renderPipeline.velocitySRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::VelocityY,

        (gpuBlendPathRequested && renderPipeline.hasVelocityTarget())

            ? renderPipeline.velocitySRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::Emission,

        (gpuBlendPathRequested && renderPipeline.hasEmissionTarget())

            ? renderPipeline.emissionSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::ObjectId,

        (gpuBlendPathRequested && renderPipeline.hasObjectIdTarget())

            ? renderPipeline.objectIdSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::MaterialId,

        (gpuBlendPathRequested && renderPipeline.hasMaterialIdTarget())

            ? renderPipeline.materialIdSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoR,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoG,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::AlbedoB,

        (gpuBlendPathRequested && renderPipeline.hasAlbedoTarget())

            ? renderPipeline.albedoSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::PositionX,

        (gpuBlendPathRequested && renderPipeline.hasPositionTarget())

            ? renderPipeline.positionSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::PositionY,

        (gpuBlendPathRequested && renderPipeline.hasPositionTarget())

            ? renderPipeline.positionSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::PositionZ,

        (gpuBlendPathRequested && renderPipeline.hasPositionTarget())

            ? renderPipeline.positionSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::U,

        (gpuBlendPathRequested && renderPipeline.hasUvTarget())

            ? renderPipeline.uvSRV()

            : nullptr);

    renderer_->setAuxiliaryChannelSource(

        ArtifactIRenderer::ChannelType::V,

        (gpuBlendPathRequested && renderPipeline.hasUvTarget())

            ? renderPipeline.uvSRV()

            : nullptr);

    // Component modes already produced a display-ready surface in
    // finalizeGpuRenderToViewport().  Resetting it here discarded that result
    // and forced a readback of the partially presented beauty frame, which
    // appeared as a recursive/perspective image for Green (and RGB/Alpha).
    if (!viewportChannelDisplaySRV_ && pipelineEnabled) {
      switch (viewportChannelDisplayMode_) {
      case ViewportChannelDisplayMode::Emission:
        viewportChannelDisplaySRV_ = renderPipeline.emissionSRV();
        break;
      // Object/Material ID are uint32 payloads in float bits (Cryptomatte).
      // Drawing the raw float SRV shows black/garbage because the viewport
      // shader expects display-ready RGBA. Leave these to the CPU
      // composition fallback (pseudo-color), same as the composite AOVs below.
      // Composite AOV modes need channel remapping (normal [-1,1] -> RGB and
      // velocity X/Y -> RGB).  Leave these to the CPU composition fallback;
      // drawing the raw float SRV makes them appear black or incorrectly
      // colored because the viewport shader expects display-ready RGBA.
      default:
        break;
      }
    }

    previewRenderSlot.state = pipelineEnabled

                                  ? PreviewRenderPipelineSlot::State::Ready

                                  : PreviewRenderPipelineSlot::State::Free;

    markVideoPreviewSetupStage(7);

    const QString multiFrameReason = multiFramePreviewFallbackReason(

        gpuBlendEnabled_, blendPipelineReady_, hasGpuBlendJustification,

        hasCpuRasterizerUpload, gpuBlendPathRequested, renderPipeline.ready(),

        previewRenderSlot.depthTargetView != nullptr,

        previewRenderSlotAcquireHazard);

    const int pipelineStateMask = (gpuBlendEnabled_ ? 0x1 : 0x0) |

                                  (renderPipeline.ready() ? 0x2 : 0x0) |

                                  (blendPipelineReady_ ? 0x4 : 0x0);

    renderCrashTrace("render-pipeline-state", renderFrameCounter_,

                     QStringLiteral("enabled=%1 requested=%2 blendReady=%3 renderReady=%4 layers=%5")

                         .arg(pipelineEnabled ? 1 : 0)

                         .arg(gpuBlendPathRequested ? 1 : 0)

                         .arg(blendPipelineReady_ ? 1 : 0)

                         .arg(renderPipeline.ready() ? 1 : 0)

                         .arg(layers.size()));

    if (previewRenderSlotAcquireHazard && gpuBlendPathRequested &&

               renderPipeline.ready()) {

      qCDebug(compositionViewLog)

          << "[CompositionView] submitted slot hazard forces fallback path"

          << "slot=" << activePreviewRenderPipelineSlot_

          << "acquire=" << lastPreviewRenderPipelineSlotAcquireReason_;

    }

    // The legacy executors still own this frame work. This frame-level graph
    // compiles the phase chain for validation; it is separate from the
    // executable RenderGraph wrapper used by smaller pass arrays below, which
    // currently stays serial because those passes share renderer resources.
    // Migrating this phase chain needs per-pass resource and context ownership.
    const int framePassPlanKey =
        (useRamPreviewFallback ? 0x1 : 0x0) |
        (pipelineEnabled ? 0x2 : 0x0) |
        (comp ? 0x4 : 0x0);
    const bool framePassPlanChanged =
        framePassPlanKey != lastFrameRenderPassPlanKey_;
    if (framePassPlanChanged || captureRenderDiagnostics) {
      const auto framePassPlan =
          buildFrameRenderPassPlan(useRamPreviewFallback, pipelineEnabled,
                                   static_cast<bool>(comp));
      const QString framePassPlanSummary =
          summarizeFrameRenderPassPlan(framePassPlan);
      ArtifactCore::RenderGraph frameRenderGraph;
      const auto graphWidth = static_cast<std::uint32_t>(std::max(1.0f, viewportW));
      const auto graphHeight = static_cast<std::uint32_t>(std::max(1.0f, viewportH));
      constexpr std::uint32_t graphFormat = 1; // RGBA8 diagnostic estimate.
      auto frameToken = frameRenderGraph.addResource({
          "Frame.Begin", ArtifactCore::RenderResourceKind::Texture,
          ArtifactCore::RenderResourceLifetime::External, graphWidth,
          graphHeight, 1, graphFormat,
          static_cast<std::uint64_t>(graphWidth) * graphHeight * 4});
      for (const auto& framePass : framePassPlan) {
        auto nextToken = frameRenderGraph.addResource({
            "Frame." + framePass.name.toStdString(),
            ArtifactCore::RenderResourceKind::Texture,
            ArtifactCore::RenderResourceLifetime::Transient, graphWidth,
            graphHeight, 1, graphFormat,
            static_cast<std::uint64_t>(graphWidth) * graphHeight * 4});
        frameRenderGraph.addPass({
            framePass.name.toStdString(),
            ArtifactCore::RenderPassQueue::Graphics,
            {frameToken}, {nextToken}, true});
        frameToken = nextToken;
      }
      const auto compiledFrameGraph = frameRenderGraph.compile();
      if (!compiledFrameGraph.valid) {
        qWarning() << "[CompositionView] frame RenderGraph compile failed:"
                   << QString::fromStdString(compiledFrameGraph.error);
      }
      if (framePassPlanChanged) {
        lastFrameRenderPassPlanKey_ = framePassPlanKey;
        lastFrameRenderPassPlanSummary_ = framePassPlanSummary;
        qCDebug(compositionViewLog)
            << "[CompositionView][FramePassPlan]" << framePassPlanSummary
            << "mode="
            << (useRamPreviewFallback
                    ? QStringLiteral("ram-preview")
                    : (pipelineEnabled ? QStringLiteral("gpu")
                                       : QStringLiteral("direct")));
      }
    }

    if (pipelineStateMask != lastPipelineStateMask_) {

      lastPipelineStateMask_ = pipelineStateMask;

      if (!pipelineEnabled) {

        renderCrashTrace("render-gpu-blend-disabled-log-begin", renderFrameCounter_);

        qWarning() << "[CompositionView] GPU blend path disabled"

                   << "gpuBlendEnabled=" << gpuBlendEnabled_

                   << "renderPipelineReady=" << renderPipeline.ready()

                   << "slot=" << activePreviewRenderPipelineSlot_

                   << "blendPipelineReady=" << blendPipelineReady_ << "size="

                   << QSize(static_cast<int>(cw), static_cast<int>(ch));

        renderCrashTrace("render-gpu-blend-disabled-log-end", renderFrameCounter_);

      } else {

        renderCrashTrace("render-gpu-blend-enabled-log-begin", renderFrameCounter_);

        qDebug() << "[CompositionView] GPU blend path enabled"

                 << "slot=" << activePreviewRenderPipelineSlot_

                 << "size="

                 << QSize(static_cast<int>(cw), static_cast<int>(ch));

        renderCrashTrace("render-gpu-blend-enabled-log-end", renderFrameCounter_);

      }

    }

    renderCrashTrace("render-after-pipeline-state-log", renderFrameCounter_);

    if (pipelineEnabled) {

      const QSize pipelineSize(static_cast<int>(renderPipeline.width()),

                               static_cast<int>(renderPipeline.height()));

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

    int maskedLayerCount = 0;

    int totalMaskCount = 0;

    int nonNormalBlendLayerCount = 0;

    int blendDispatchCount = 0;

    int blendFailureCount = 0;

    int blendRetryNormalCount = 0;

    int directBlendFallbackCount = 0;

    int layerToFloatConvertCount = 0;

    int skipInvisibleCount = 0;

    int skipSoloCount = 0;

    int skipInactiveCount = 0;

    int skipRoiCount = 0;

    int skipPartialDamageCount = 0;

    int skipCategoryCount = 0;

    int skipLodCount = 0;

    int opacityZeroCount = 0;

    int compositedLayerCount = 0;

    bool gpuPartialRecomposeCandidate = false;
    bool gpuPartialRecomposeStarted = false;
    QRectF gpuPartialRecomposeRequestedCanvasRect;
    BoundedTileRefinementPlan gpuPartialDamageTilePlan;
    std::uint64_t gpuPartialRegionPixels = 0;
    std::uint64_t gpuPartialTargetPixels = 0;

    QStringList blendMaskLayerNotes;

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
    markVideoPreviewSetupStage(8);

    if (profileThisVideoPreviewSetup) {
      phaseNs = videoPreviewSetupMarksNs[8];
    }

    const auto addProfileBoundary = [&](qint64& accumulatedNs) {
      if (profileThisVideoPreviewSetup) {
        accumulatedNs += lastPhaseDurationNs;
      }
    };

    qint64 basePassMs = 0;

    qint64 profileBaseBoundaryNs = 0;

    qint64 profileSurfaceBoundaryNs = 0;

    qint64 profileMaskBoundaryNs = 0;

    qint64 profileCompositeBoundaryNs = 0;

    qint64 profileDirectLayerBoundaryNs = 0;

    qint64 profilePostBoundaryNs = 0;

    qint64 profileFinalizeNs = 0;

    qint64 profileResolveExecuteNs = 0;

    qint64 profileGiBlockNs = 0;

    qint64 profileGiDispatchNs = 0;

    qint64 profileGiApplyNs = 0;

    qint64 profileFxaaNs = 0;

    qint64 profileOverlayBoundaryNs = 0;

    qint64 profilePresentBoundaryNs = 0;

    qint64 baseSetupNs = 0;

    qint64 backgroundSeedNs = 0;

    qint64 depthClearNs = 0;

    qint64 accumulationClearNs = 0;

    qint64 partialRegionClearNs = 0;

    qint64 fullTargetClearNs = 0;

    qint64 computeUnbindNs = 0;

    qint64 surfacePassMs = 0;

    qint64 maskPassMs = 0;

    qint64 compositePassMs = 0;

    qint64 postPassMs = 0;

    qint64 overlayMs = 0;

    qint64 flushMs = 0;

    const quint64 flushCountAtFrameStart = renderer_->flushCount();
    const qint64 flushContextTimeUsAtFrameStart =
        renderer_->flushContextTimeUs();
    quint64 flushCountThisFrame = 0;

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

    renderCrashTrace("render-clear-begin", renderFrameCounter_);

    RenderPassContext setupContext{renderer_.get(), renderFrameCounter_};

    RenderPassResources setupResources;

    FunctionalRenderPass setupPass(

        FrameRenderPassKind::Setup, QStringLiteral("Viewport Setup / Clear"),

        [](RenderPassResources&) { return true; },

        [&](RenderPassContext& context, RenderPassResources&) {

          context.renderer->clearRenderTarget(viewportClearColor_);

          return true;

        });

    bool partialGpuRecomposeFailed = false;
    bool framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
        setupPass, setupContext, setupResources);

    renderCrashTrace("render-clear-end", renderFrameCounter_);

    // Draw the HDR environment after clearing the swap chain. Drawing it in
    // the scene-setup block above is lost when this frame-level clear runs.
    // The later composition sprite uses alpha blending, so empty canvas pixels
    // retain this background while rendered layers cover it.
    renderer_->drawEnvironmentSkybox();



    Diligent::ITextureView* ramPreviewReadbackSRV = nullptr;
    bool shadowMapRenderedBeforeFinalize = false;

    lastPresentedReadbackSRV_ = nullptr;

    if (useRamPreviewFallback) {
      markVideoPreviewSetupStage(10);

      renderCrashTrace("render-branch-ram-preview", renderFrameCounter_);

      RenderPassContext ramContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources ramResources;

      FunctionalRenderPass ramBasePass(

          FrameRenderPassKind::Base, QStringLiteral("RAM Preview Base"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

            if (backgroundMode == CompositionBackgroundMode::MayaGradient) {

              drawViewportMayaGradientBackground(

                  context.renderer, viewportW, viewportH, layerBgColor,

                  cachedMayaGradientSprite_);

            } else if (backgroundMode ==

                       CompositionBackgroundMode::Checkerboard) {

              drawViewportCheckerboardBackground(

                  context.renderer, viewportW, viewportH,

                  checkerboardTileSize_);

            }

            context.renderer->setUseExternalMatrices(false);

            context.renderer->resetGizmoCameraMatrices();

            context.renderer->reset3DCameraMatrices();

            context.renderer->setCanvasSize(cw, ch);

            context.renderer->setZoom(zoom);

            context.renderer->setPan(panX, panY);

            drawThreeDimensionalGroundGrid();

            drawCompositionBackgroundDirect(

                context.renderer, cw, ch, layerBgColor, backgroundMode,

                checkerboardTileSize_, cachedMayaGradientSprite_,
                usesSpatialViewportOrientation()
                    ? &viewportOrientationViewForOverlay_
                    : nullptr,
                usesSpatialViewportOrientation()
                    ? &viewportOrientationProjectionForOverlay_
                    : nullptr);

            if (showCompositionRegionOverlay_ && comp &&
                !usesSpatialViewportOrientation()) {
              ::Artifact::drawCompositionRegionOverlay(context.renderer, comp);
            }
            if (previewOrbitActive_ && comp) {
              ::Artifact::drawNavigationCrossOverlay(context.renderer, comp);
            }

            basePassMs = markPhaseMs();
            addProfileBoundary(profileBaseBoundaryNs);

            return true;

          });

      FunctionalRenderPass ramCompositePass(

          FrameRenderPassKind::Composite,

          QStringLiteral("RAM Preview Composite"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

            QMatrix4x4 identity;

            if (ramPreviewTexture) {
              context.renderer->drawSpriteTransformed(
                  0.0f, 0.0f, cw, ch, identity, ramPreviewTexture, 1.0f);
            }

            surfacePassMs = markPhaseMs();
            addProfileBoundary(profileSurfaceBoundaryNs);

            return true;

          });

      const std::array<RenderPass*, 2> ramPasses{

          &ramBasePass, &ramCompositePass};

      RenderPassExecutor::runAllWithRenderGraph(ramPasses, ramContext,
                                                ramResources);
      markVideoPreviewSetupStage(11);

      maskPassMs = surfacePassMs;

      compositePassMs = surfacePassMs;

      postPassMs = compositePassMs;

    } else if (pipelineEnabled) {

    renderCrashTrace("render-branch-gpu-pipeline", renderFrameCounter_);

    // ============================================================

    // GPU パイプライン: レイヤー 0 枚でも frameOutOfRange でも常に描画

    // ============================================================

      ArtifactCore::ProfileScope _profBase(

          "BasePass", ArtifactCore::ProfileCategory::Composite);

      auto accumSRV = renderPipeline.accumSRV();

      auto tempUAV = renderPipeline.tempUAV();

      auto layerRTV = renderPipeline.layerRTV();

      auto layerSRV = renderPipeline.layerSRV();

      auto layerFloatSRV = renderPipeline.layerFloatSRV();

      auto layerFloatUAV = renderPipeline.layerFloatUAV();

      auto emissionRTV = renderPipeline.emissionRTV();

      auto normalRTV = renderPipeline.normalRTV();

      auto velocityRTV = renderPipeline.velocityRTV();

      auto objectIdRTV = renderPipeline.objectIdRTV();

      auto materialIdRTV = renderPipeline.materialIdRTV();

      auto albedoRTV = renderPipeline.albedoRTV();

      auto positionRTV = renderPipeline.positionRTV();

      auto uvRTV = renderPipeline.uvRTV();



      // Pre-render matte source layers for GPU track matte

      QHash<ArtifactCore::Id, QImage> matteSourceImages;
      QHash<ArtifactCore::Id, Diligent::ITextureView*> matteSourceGpuViews;

      {

        for (const auto &layerForMatte : layers) {

          if (!layerForMatte || !layerForMatte->isActiveAt(currentFrame))

            continue;

          if (!layerForMatte->hasEnabledExternalMatteReference()) {

            continue;

          }

          auto mattes = layerForMatte->matteReferences();

          for (const auto &matteRef : mattes) {

            if (!matteRef.enabled || matteRef.sourceLayerId.isNil() ||
                matteRef.sourceLayerId == layerForMatte->id())
              continue;

            if (matteSourceImages.contains(matteRef.sourceLayerId) ||
                matteSourceGpuViews.contains(matteRef.sourceLayerId)) {
              continue;
            }

            if (const auto sourceLayer =
                    comp->layerById(matteRef.sourceLayerId)) {
              const auto sourceParent = sourceLayer->parentLayer();
              const bool sourceCanUseGpuIntermediate =
                  !sourceLayer->isAdjustmentLayer() &&
                  sourceLayer->shouldIncludeInFinalRender() &&
                  (!sourceParent || !sourceParent->isGroupLayer());
              if (matteRef.fitMode == MatteFitMode::Stretch &&
                  sourceCanUseGpuIntermediate &&
                  sourceLayer->isVisible() &&
                  sourceLayer->isActiveAt(currentFrame)) {
                if (auto* gpuSource = renderMatteGpuOutput(
                        sourceLayer.get(), currentFrame.framePosition(),
                        QSize(passResolution.renderWidth,
                              passResolution.renderHeight),
                        passResolution.renderWidthF(),
                        passResolution.renderHeightF(), cw, ch, accumSRV, matteResolver, sceneLights,
                        has3DCamera, cameraViewMatrix, cameraProjMatrix,
                        &renderPipeline)) {
                  matteSourceGpuViews.insert(matteRef.sourceLayerId, gpuSource);
                  continue;
                }
              }
            }

            QImage matteImg = matteResolver(matteRef.sourceLayerId);

            if (matteImg.isNull()) continue;

            matteSourceImages.insert(
                matteRef.sourceLayerId,
                matteImg.convertToFormat(QImage::Format_RGBA8888));

          }

        }

      }



      const FloatColor layerBgColor = comp->backgroundColor();

      const auto pendingSlotDamage =
          previewRenderSlot.damageTracker.combinedDirtyROI();
      const bool damageLayersAreSupported = tgfxPartialRecomposeEnabled_ &&
          std::all_of(layers.cbegin(), layers.cend(),
                      [&](const ArtifactAbstractLayerPtr& layer) {
                        if (!layer ||
                            dynamic_cast<const ArtifactParticleLayer*>(layer.get()) ||
                            !isPartialRecomposeLayer(layer) ||
                            !effectExpandedLayerBounds(layer.get()).isValid()) {
                          return false;
                        }
                        return !layer->revealEnabled() && !layer->hasModifiers() &&
                               !layerHasEnabledMatteReferences(layer.get());
                      });
      const bool partialGpuRecomposeEligible =
          tgfxPartialRecomposeEnabled_ && pipelineEnabled &&
          currentAccumTexture != nullptr && !layers.empty() &&
          previewRenderSlot.retainedCompositeValid &&
          previewRenderSlot.damageTracker.hasDirtyRegions() &&
          !previewRenderSlot.damageTracker.combinedNeedsFullRedraw() &&
          !pendingSlotDamage.isEmpty() && !forceContinuousRedraw &&
          !frameOutOfRange && !has3DCamera && !hasVisible3DLayer &&
          !hasSoloLayer &&
          layerRenderFilter_ == CompositionLayerRenderFilter::All &&
          viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Color &&
          isFrontOrthographicViewport() && !showIsolationOverlay_ &&
          !showXRayOverlay_ && !showCompositionRegionOverlay_ &&
          !previewOrbitActive_ && !auxiliary3DChannelRequested &&
          !postPassMask.screenSpaceGi && !postPassMask.motionBlurVelocity &&
          !postPassMask.cameraMotionBlur && !postPassMask.timelineMotionBlur &&
          !postPassMask.depthOfField && !viewportMultiChannelRequested &&
          damageLayersAreSupported;
      const TileGrid damageTileGrid(
          std::max(1, static_cast<int>(std::ceil(cw))),
          std::max(1, static_cast<int>(std::ceil(ch))), 256);
      if (partialGpuRecomposeEligible) {
        gpuPartialDamageTilePlan =
            previewRenderSlot.damageTracker.makeBoundedDirtyTilePlan(
                damageTileGrid, RenderROI(visibleCanvasRect), 8, QString(),
                previewRenderSlot.damageTileCursor);
        for (int index = 0;
             index < gpuPartialDamageTilePlan.scheduledCount; ++index) {
          const QRectF tileRect = damageTileGrid.tileRectClamped(
              gpuPartialDamageTilePlan.tiles[static_cast<size_t>(index)]);
          gpuPartialRecomposeRequestedCanvasRect =
              gpuPartialRecomposeRequestedCanvasRect.isEmpty()
                  ? tileRect
                  : gpuPartialRecomposeRequestedCanvasRect.united(tileRect);
        }
      }
      bool partialGpuRecomposeActive =
          partialGpuRecomposeEligible &&
          !gpuPartialDamageTilePlan.empty() &&
          !gpuPartialRecomposeRequestedCanvasRect.isEmpty();
      gpuPartialRecomposeCandidate = partialGpuRecomposeEligible;
      ArtifactCore::ComputeRegion gpuDamageRegion;
      QRectF gpuDamageCanvasRect;

      GpuBasePassState gpuBasePassState;

      RenderPassContext baseContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources baseResources{

          &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

          layerFloatUAV, accumSRV, tempUAV};

      FunctionalRenderPass basePass(

          FrameRenderPassKind::Base, QStringLiteral("Clear / Base"),

          [](RenderPassResources& resources) {

            return resources.pipeline && resources.layerRTV &&

                   resources.layerSRV && resources.accumSRV &&

                   resources.tempUAV;

          },

          [&](RenderPassContext&, RenderPassResources& resources) {

            const qint64 baseSetupStartedNs = profileThisVideoPreviewSetup
                ? frameTimer.nsecsElapsed()
                : 0;
            gpuBasePassState = beginGpuBasePass(

                *resources.pipeline, previewRenderSlot,
                passResolution.renderWidthF(), passResolution.renderHeightF(),
                cw, ch,

                layerBgColor, backgroundMode,
                partialGpuRecomposeActive
                    ? &gpuPartialRecomposeRequestedCanvasRect
                    : nullptr,
                &gpuDamageRegion, &partialGpuRecomposeActive,
                profileThisVideoPreviewSetup ? &frameTimer : nullptr,
                profileThisVideoPreviewSetup ? &depthClearNs : nullptr,
                profileThisVideoPreviewSetup ? &accumulationClearNs : nullptr,
                profileThisVideoPreviewSetup ? &partialRegionClearNs : nullptr,
                profileThisVideoPreviewSetup ? &fullTargetClearNs : nullptr,
                profileThisVideoPreviewSetup ? &computeUnbindNs : nullptr);
            if (profileThisVideoPreviewSetup) {
              baseSetupNs =
                  frameTimer.nsecsElapsed() - baseSetupStartedNs;
            }
            if (partialGpuRecomposeActive) {
              gpuDamageCanvasRect =
                  mapRenderTargetRegionToCanvas(gpuDamageRegion);
            }
            if (tgfxPartialRecomposeEnabled_ &&
                previewRenderSlot.retainedCompositeValid &&
                previewRenderSlot.damageTracker.hasDirtyRegions() &&
                !previewRenderSlot.damageTracker.combinedNeedsFullRedraw() &&
                !partialGpuRecomposeActive) {
              previewRenderSlot.damageTracker.clearAll();
              previewRenderSlot.damageTracker.markFullRedraw();
              damageTracker_.markFullRedraw();
            }
            previewRenderSlot.retainedCompositionId =
                comp ? comp->id() : CompositionID();
            previewRenderSlot.retainedFramePosition =
                currentFrame.framePosition();
            previewRenderSlot.retainedTargetSize =
                QSize(static_cast<int>(resources.pipeline->width()),
                      static_cast<int>(resources.pipeline->height()));
            previewRenderSlot.retainedDownsample = effectivePreviewDownsample;
            previewRenderSlot.retainedCanvasWidth = cw;
            previewRenderSlot.retainedCanvasHeight = ch;
            previewRenderSlot.retainedZoom = gpuBasePassState.origZoom;
            previewRenderSlot.retainedPanX = gpuBasePassState.origPanX;
            previewRenderSlot.retainedPanY = gpuBasePassState.origPanY;

            qint64 backgroundSeedStartedNs = profileThisVideoPreviewSetup
                ? frameTimer.nsecsElapsed()
                : 0;
            bool backgroundSeeded = seedGpuBasePassBackground(

                *resources.pipeline, resources.layerRTV, resources.layerSRV,

                resources.layerFloatSRV, resources.layerFloatUAV,

                resources.accumSRV, resources.tempUAV,

                layerToFloatConvertCount, blendDispatchCount,

                blendFailureCount, cw, ch, layerBgColor, backgroundMode, comp,
                partialGpuRecomposeActive ? &gpuDamageRegion : nullptr);
            if (profileThisVideoPreviewSetup) {
              backgroundSeedNs +=
                  frameTimer.nsecsElapsed() - backgroundSeedStartedNs;
            }
            if (!backgroundSeeded && partialGpuRecomposeActive) {
              partialGpuRecomposeActive = false;
              previewRenderSlot.damageTracker.clearAll();
              previewRenderSlot.damageTracker.markFullRedraw();
              damageTracker_.markFullRedraw();
              renderer_->pushRenderTarget(resources.pipeline->accumRTV(),
                                          previewRenderSlot.depthTargetView);
              renderer_->clearRenderTarget(
                  resources.pipeline->accumRTV(),
                  FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
              renderer_->popRenderTarget();
              renderer_->unbindColorTargetsForCompute();
              backgroundSeedStartedNs = profileThisVideoPreviewSetup
                  ? frameTimer.nsecsElapsed()
                  : 0;
              backgroundSeeded = seedGpuBasePassBackground(
                  *resources.pipeline, resources.layerRTV, resources.layerSRV,
                  resources.layerFloatSRV, resources.layerFloatUAV,
                  resources.accumSRV, resources.tempUAV,
                  layerToFloatConvertCount, blendDispatchCount,
                  blendFailureCount, cw, ch, layerBgColor, backgroundMode,
                  comp);
              if (profileThisVideoPreviewSetup) {
                backgroundSeedNs +=
                    frameTimer.nsecsElapsed() - backgroundSeedStartedNs;
              }
            }
            if (!backgroundSeeded) {
              return false;
            }

            if (partialGpuRecomposeActive) {
              gpuPartialRecomposeStarted = true;
              gpuPartialRegionPixels =
                  static_cast<std::uint64_t>(gpuDamageRegion.width) *
                  gpuDamageRegion.height;
              gpuPartialTargetPixels =
                  static_cast<std::uint64_t>(resources.pipeline->width()) *
                  resources.pipeline->height();
            }

            accumSRV = resources.accumSRV;

            tempUAV = resources.tempUAV;

            return true;

          });

      framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
          basePass, baseContext, baseResources) && framePassesSucceeded;

      const float origZoom = gpuBasePassState.origZoom;

      const FloatColor origClearColor = gpuBasePassState.origClearColor;

      const float origPanX = gpuBasePassState.origPanX;

      const float origPanY = gpuBasePassState.origPanY;

      const float origViewW = gpuBasePassState.origViewW;

      const float origViewH = gpuBasePassState.origViewH;



      if (emissionRTV) {

        renderer_->clearRenderTarget(emissionRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (normalRTV) {

        renderer_->clearRenderTarget(normalRTV,

                                     FloatColor{0.5f, 0.5f, 1.0f, 0.0f});

      }

      if (velocityRTV) {

        renderer_->clearRenderTarget(velocityRTV,

                                     FloatColor{0.5f, 0.5f, 0.5f, 0.0f});

      }

      if (objectIdRTV) {

        renderer_->clearRenderTarget(objectIdRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (materialIdRTV) {

        renderer_->clearRenderTarget(materialIdRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (albedoRTV) {

        renderer_->clearRenderTarget(albedoRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (positionRTV) {

        renderer_->clearRenderTarget(positionRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }

      if (uvRTV) {

        renderer_->clearRenderTarget(uvRTV,

                                     FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

      }



      basePassMs = markPhaseMs();
      addProfileBoundary(profileBaseBoundaryNs);



      // Already in composition-space offscreen coordinates; layer loop uses it

      // as-is.



      // -- 2: レイヤーブレンド（frameOutOfRange ならスキップ）--

      ArtifactCore::ProfileScope _profLayer(

          "LayerPass", ArtifactCore::ProfileCategory::Composite);

      if (!frameOutOfRange) {

        const DetailLevel lod = detailLevelFromZoom(origZoom);

        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(

            lod)); // Pass LOD to renderer/effects

        comp->buildTimelineTransitionDrawPlan(
            currentFrame.framePosition(), transitionEvaluationWorkspace_,
            transitionDrawStepWorkspace_);

        bool shared3DSceneDepthOpen = false;

        for (const auto &drawStep : transitionDrawStepWorkspace_) {
          if (drawStep.kind != CompositionTimelineTransitionDrawStep::Kind::Layer) {
            // Reserved for the A/B temporary composite (P2/P3).
            continue;
          }
          if (drawStep.layerIndex < 0 || drawStep.layerIndex >= layers.size()) {
            continue;
          }
          const auto &layer = layers[drawStep.layerIndex];

          if (layerRenderFilter_ == CompositionLayerRenderFilter::SelectedOnly) {
            if (!passesLayerRenderFilter(layerRenderFilter_, selectedIds,
                                         selectedLayerId_, layer)) {
              continue;
            }
          }

          // P0-4: per-viewport category mask. Lookup the cached
          // category; if absent, classify once and store.
          const auto cacheIt = layerCategoryCache_.constFind(layer->id());
          CompositionViewportLayerCategory layerCategory;
          if (cacheIt != layerCategoryCache_.constEnd()) {
            layerCategory = cacheIt.value();
          } else {
            layerCategory = classifyLayerCategory(layer);
            layerCategoryCache_.insert(layer->id(), layerCategory);
          }
          if (!passesLayerCategoryMask(viewportLayerCategoryMask_, layer,
                                      layerCategory)) {
            ++skipCategoryCount;
            continue;
          }

          if (!isLayerEffectivelyVisible(layer)) {

            ++skipInvisibleCount;

            continue;

          }

          if (hasSoloLayer && !layer->isSolo()) {

            ++skipSoloCount;

            continue;

          }

          if (!layer->isActiveAt(currentFrame)) {

            ++skipInactiveCount;

            continue;

          }

          const QRectF layerBounds = effectExpandedLayerBounds(layer.get());
          // Particle positions and trails can extend beyond localBounds().
          // Culling by the layer rectangle makes them pop in at its edge.
          const bool particle2D =
              dynamic_cast<const ArtifactParticleLayer*>(layer.get()) != nullptr &&
              !layer->is3D();

          if (!particle2D && layerBounds.isValid() &&

              layerBounds.intersected(roiRect).isEmpty()) {

            ++skipRoiCount;

            continue;

          }

          if (!particle2D && partialGpuRecomposeActive && gpuDamageCanvasRect.isValid() &&
              layerBounds.isValid() &&
              layerBounds.intersected(gpuDamageCanvasRect).isEmpty()) {
            ++skipRoiCount;
            ++skipPartialDamageCount;
            continue;
          }



          // --- Feature 3: Layer Drawing Skip (LOD-based) ---

          // Skip rendering layers that are too small to be visible on screen.

          // Text remains useful as an orientation cue at every zoom level.
          // Do not let the generic tiny-layer optimization cull it before its
          // sprite/GPU-buffer draw path can run.
          const bool preserveTextAtAnyZoom =
              dynamic_cast<const ArtifactTextLayer *>(layer.get()) != nullptr;
          if (layerBounds.isValid() && !preserveTextAtAnyZoom && !particle2D) {

            float screenW =

                std::abs(static_cast<float>(layerBounds.width()) * origZoom);

            float screenH =

                std::abs(static_cast<float>(layerBounds.height()) * origZoom);

            // For 3D layers, LOD must use projected pixel size rather than
            // world-space size multiplied by the 2D zoom value.
            if (layer->is3D() && has3DCamera && hostWidth_ > 0.0f &&
                hostHeight_ > 0.0f) {
              const QRectF bounds = layer->localBounds();
              const QMatrix4x4 transform = layer->getGlobalTransform4x4();
              const QRect viewport(0, 0,
                                   std::max(1, static_cast<int>(hostWidth_)),
                                   std::max(1, static_cast<int>(hostHeight_)));
              const std::array<QVector3D, 4> corners = {
                  QVector3D(bounds.left(), bounds.top(), 0.0f),
                  QVector3D(bounds.right(), bounds.top(), 0.0f),
                  QVector3D(bounds.right(), bounds.bottom(), 0.0f),
                  QVector3D(bounds.left(), bounds.bottom(), 0.0f)};
              float minX = std::numeric_limits<float>::max();
              float minY = std::numeric_limits<float>::max();
              float maxX = std::numeric_limits<float>::lowest();
              float maxY = std::numeric_limits<float>::lowest();
              for (const auto &corner : corners) {
                const QVector3D projected = ViewportMath::projectToTopDown(
                    transform.map(corner), cameraViewMatrix, cameraProjMatrix,
                    viewport);
                minX = std::min(minX, projected.x());
                minY = std::min(minY, projected.y());
                maxX = std::max(maxX, projected.x());
                maxY = std::max(maxY, projected.y());
              }
              screenW = std::abs(maxX - minX);
              screenH = std::abs(maxY - minY);
            }



            if (lod == DetailLevel::Low) {

              if (screenW < 8.0f || screenH < 8.0f) {

                ++skipLodCount;

                continue;

              }

            } else if (lod == DetailLevel::Medium) {

              if (screenW < 2.0f || screenH < 2.0f) {

                ++skipLodCount;

                continue;

              }

            }

          }

          // ------------------------------------------------



          ++drawnLayerCount;
          if (const auto* video = dynamic_cast<const ArtifactVideoLayer*>(layer.get())) {
            videoFramesReadyForRamPreview = videoFramesReadyForRamPreview &&
                static_cast<bool>(video->cachedFrameImageBufferShared(framePos));
          }

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

          const bool isolateSkipThisLayer =
              showIsolationOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          if (isolateSkipThisLayer) {
            continue;
          }

          const bool xRayDimThisLayer =
              showXRayOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          float opacity =
              layer->opacity() *
              (xRayDimThisLayer ? xRayOverlayOpacity_ : 1.0f);

          // Opacity-only transitions share the same immutable frame
          // evaluation as the offline CPU/GPU render queue paths.
          opacity *= static_cast<float>(
              CompositionTimelineTransitionEvaluation::
                  combinedOpacityMultiplierForLayer(
                      transitionEvaluationWorkspace_, layer->idView()));

          if (opacity <= 0.0f) {

            ++opacityZeroCount;

            continue;

          }

          ++compositedLayerCount;

          const int layerMaskCount = layer->maskCount();

          if (layerMaskCount > 0) {

            ++maskedLayerCount;

            totalMaskCount += layerMaskCount;

          }

          if (blendMode != ArtifactCore::BlendMode::Normal) {

            ++nonNormalBlendLayerCount;

          }

          // Model3D, solid/vector cards and alpha-split still/video cards share
          // one depth attachment while retaining the layer blend pipeline.
          const auto* sceneModelLayer =
              dynamic_cast<Artifact3DLayer*>(layer.get());
          const bool opaqueSolidCard = isOpaqueSolid3DCard(layer.get());
          DirectShape3DCard shapeCard;
          const bool opaqueShapeCard =
              directShape3DCard(layer.get(), shapeCard) &&
              (shapeCard.fillPoints.empty() ||
               shapeCard.fillColor.a() >= 0.9999f) &&
              (shapeCard.strokePoints.empty() ||
               shapeCard.strokeColor.a() >= 0.9999f);
          const bool texturedCard =
              gpuTextureCacheManager_ &&
              textured3DCardBuffer(layer.get()) != nullptr;
          const bool videoCard =
              gpuTextureCacheManager_ &&
              readyVideo3DCardLayer(layer.get(), currentFrame.framePosition()) !=
                  nullptr;
          const bool shared3DSceneDepthMember =
              ((sceneModelLayer != nullptr &&
                !sceneModelLayer->hasTransparentMaterial()) ||
               opaqueSolidCard || opaqueShapeCard || texturedCard ||
               videoCard) &&
              !layer->isAdjustmentLayer() &&
              blendMode == ArtifactCore::BlendMode::Normal &&
              opacity >= 0.9999f && layerMaskCount == 0 &&
              !layerHasEnabledMatteReferences(layer.get());
          const bool preserveSceneDepth =
              shared3DSceneDepthMember && shared3DSceneDepthOpen;
          if (!shared3DSceneDepthMember) {
            shared3DSceneDepthOpen = false;
          }

          // MSAA is isolated per 3D layer.  A shared-depth scene and AOV
          // capture stay on the established single-sample path because their
          // depth/color attachments must remain mutually compatible.
          const bool useLayerMsaa = postPassMask.antiAliasingMode == 2 && layer->is3D() && !preserveSceneDepth &&
              !postPassMask.auxiliaryTargets && !postPassMask.motionBlurVelocity &&
                  ensurePreviewRenderPipelineMsaaSlot(
                      previewRenderSlot, passResolution.renderWidth,
                      passResolution.renderHeight);



          RenderPassContext passContext{renderer_.get(), renderFrameCounter_};

          RenderPassResources passResources{

              &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

              layerFloatUAV, accumSRV, tempUAV};
          GpuRasterEffectPlan layerRasterEffectPlan;

          // The layer pipeline is a strict raster -> mask -> blend chain.
          // It does not use RenderGraph resource scheduling: creating three
          // FunctionalRenderPass objects here allocates std::function state for
          // every layer/frame without changing the execution order.
          if (!passContext.renderer || !passResources.pipeline ||
              !passResources.layerRTV || !passResources.layerSRV) {
            shared3DSceneDepthOpen = false;
            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }
            continue;
          }

          drawGpuLayerToIntermediate(

                    layer.get(), passResources.layerRTV,

                    static_cast<Diligent::ITextureView*>(
                        previewRenderSlot.depthTargetView),
                    passResources.accumSRV,
                    passResolution.renderWidthF(),

                    passResolution.renderHeightF(), cw, ch, lod, currentFrame,
                    matteResolver, sceneLights,

                    has3DCamera, cameraViewMatrix, cameraProjMatrix,

                    preserveSceneDepth, passResources.pipeline,
                    useLayerMsaa
                        ? static_cast<Diligent::ITextureView*>(
                              previewRenderSlot.msaaColorTargetView)
                        : nullptr,
                    useLayerMsaa
                        ? static_cast<Diligent::ITextureView*>(
                              previewRenderSlot.msaaDepthTargetView)
                        : nullptr,
                    &layerRasterEffectPlan);
                // applyPointwise() may swap the accumulation ping-pong
                // textures. Keep the following mask/blend passes on the
                // resulting resource rather than the pre-effect SRV.
                passResources.accumSRV = passResources.pipeline->accumSRV();
                passResources.tempUAV = passResources.pipeline->tempUAV();
                accumSRV = passResources.accumSRV;
                tempUAV = passResources.tempUAV;

                if ((!draftRendering || emissionChannelRequested) && emissionRTV) {

                  drawGpuLayerEmissionToTarget(
                      layer.get(), emissionRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

                if ((!draftRendering || normalChannelRequested ||
                     postPassMask.screenSpaceGi) && normalRTV) {

                  drawGpuLayerNormalToTarget(
                      layer.get(), normalRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

                if ((!draftRendering || velocityChannelRequested) && velocityRTV) {

                  drawGpuLayerVelocityToTarget(
                      layer.get(), velocityRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

                if ((!draftRendering || objectIdChannelRequested) && objectIdRTV) {

                  const float objectId =
                      ArtifactCore::Image::Cryptomatte::CryptoSample::nameToFloat(
                          layer->id().toString());

                  drawGpuLayerIdToTarget(

                      layer.get(), objectIdRTV,

                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView),

                      ArtifactIRenderer::ChannelType::ObjectId,

                      objectId);

                }

                if ((!draftRendering || materialIdChannelRequested) && materialIdRTV) {

                  QString materialKey =

                      layer->layerName() + QStringLiteral("|material");

                  if (auto* modelLayer =

                          dynamic_cast<Artifact3DLayer*>(layer.get())) {

                    materialKey = modelLayer->materialSignature();

                  }

                  const float materialId =
                      ArtifactCore::Image::Cryptomatte::CryptoSample::nameToFloat(
                          materialKey);

                  drawGpuLayerIdToTarget(

                      layer.get(), materialIdRTV,

                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView),

                      ArtifactIRenderer::ChannelType::MaterialId,

                      materialId);

                }

                if ((!draftRendering || albedoChannelRequested ||
                     postPassMask.screenSpaceGi) && albedoRTV) {

                  drawGpuLayerAlbedoToTarget(
                      layer.get(), albedoRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

                if ((!draftRendering || positionChannelRequested) && positionRTV) {

                  drawGpuLayerPositionToTarget(
                      layer.get(), positionRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

                if ((!draftRendering || uvChannelRequested) && uvRTV) {

                  drawGpuLayerUvToTarget(
                      layer.get(), uvRTV,
                      static_cast<Diligent::ITextureView*>(
                          previewRenderSlot.depthTargetView));

                }

          surfacePassMs = markPhaseMs();
          addProfileBoundary(profileSurfaceBoundaryNs);

          Diligent::ITextureView* preparedBlendSRV = layerSRV;

          bool convertedLayerToFloat = false;

          if (!passResources.pipeline || !passResources.layerSRV ||
              !passResources.layerFloatSRV || !passResources.layerFloatUAV ||
              !passResources.tempUAV) {
            shared3DSceneDepthOpen = false;
            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }
            continue;
          }

          preparedBlendSRV = prepareGpuLayerForBlend(

                    layer.get(), *passResources.pipeline, passResources.layerSRV,

                    passResources.layerFloatSRV, passResources.layerFloatUAV,

                    passResources.tempUAV, &layerRasterEffectPlan,
                    matteSourceImages,
                    matteSourceGpuViews, layerToFloatConvertCount,
                    convertedLayerToFloat,
                    partialGpuRecomposeActive ? &gpuDamageRegion : nullptr);

          maskPassMs = markPhaseMs();
          addProfileBoundary(profileMaskBoundaryNs);

          if (!preparedBlendSRV) {
            shared3DSceneDepthOpen = false;
            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }
            continue;
          }

          GpuLayerBlendResult blendResult;

          if (!passResources.pipeline || !passResources.layerSRV ||
              !passResources.accumSRV || !passResources.tempUAV) {
            shared3DSceneDepthOpen = false;
            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }
            continue;
          }

          blendResult = blendGpuLayerIntoAccum(

                    layer.get(), *passResources.pipeline, passResources.layerSRV,

                    preparedBlendSRV, passResources.accumSRV, passResources.tempUAV,

                    blendMode, opacity, cw, ch, blendDispatchCount,

                    blendRetryNormalCount, blendFailureCount,

                    directBlendFallbackCount, convertedLayerToFloat,
                    partialGpuRecomposeActive ? &gpuDamageRegion : nullptr);

                accumSRV = passResources.accumSRV;

                tempUAV = passResources.tempUAV;

          compositePassMs = markPhaseMs();
          addProfileBoundary(profileCompositeBoundaryNs);

          if (!blendResult.blended && !blendResult.directFallbackUsed) {
            shared3DSceneDepthOpen = false;
            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }
            continue;
          }

          if (!blendResult.blended) {

            shared3DSceneDepthOpen = false;

            if (partialGpuRecomposeActive) {
              partialGpuRecomposeFailed = true;
              break;
            }

            continue;

          }

          shared3DSceneDepthOpen = shared3DSceneDepthMember;

          if (blendMaskLayerNotes.size() < 4 &&

              (blendMode != ArtifactCore::BlendMode::Normal ||

               layerMaskCount > 0)) {

            blendMaskLayerNotes

                << QStringLiteral("%1:blend=%2 opacity=%3 masks=%4")

                       .arg(layer->layerName(),

                            ArtifactCore::BlendModeUtils::toString(blendMode),

                            QString::number(opacity, 'f', 3))

                       .arg(layerMaskCount);

          }

        }

      }

      if (postPassMask.screenSpaceGi) {
        const qint64 giBlockStartedNs = profileThisVideoPreviewSetup
            ? frameTimer.nsecsElapsed()
            : 0;
        renderer_->unbindColorTargetsForCompute();
        auto* depthSRV = renderer_->offscreenTextureShaderResourceView(
            previewRenderSlot.depthTargetView);
        const auto giInputs =
            renderPipeline.globalIlluminationInputs(depthSRV);
        auto context = renderer_->immediateContext();
        const qint64 giDispatchStartedNs = profileThisVideoPreviewSetup
            ? frameTimer.nsecsElapsed()
            : 0;
        const bool dispatched = context &&
            renderPipeline.dispatchScreenSpaceGlobalIllumination(
                context.RawPtr(), giInputs,
                globalIlluminationSettings.ssgiResolutionScale,
                globalIlluminationSettings.ssgiRaySteps,
                1.0f, 0.01f,
                globalIlluminationSettings.temporalAccumulation,
                globalIlluminationSettings.denoise);
        if (profileThisVideoPreviewSetup) {
          profileGiDispatchNs +=
              frameTimer.nsecsElapsed() - giDispatchStartedNs;
        }
        if (dispatched) {
          ++giDispatchedCount_;
          auto* aoMask = renderPipeline.screenSpaceGlobalIlluminationSRV();
          const qint64 giApplyStartedNs = profileThisVideoPreviewSetup
              ? frameTimer.nsecsElapsed()
              : 0;
          if (aoMask && renderPipeline.applyScreenSpaceAmbientOcclusion(
                            context.RawPtr(), accumSRV, tempUAV, aoMask)) {
            renderPipeline.swapAccumAndTemp();
            accumSRV = renderPipeline.accumSRV();
            tempUAV = renderPipeline.tempUAV();
          }
          if (profileThisVideoPreviewSetup) {
            profileGiApplyNs +=
                frameTimer.nsecsElapsed() - giApplyStartedNs;
          }
        }
        if (!dispatched && compositionViewLog().isDebugEnabled()) {
          qCDebug(compositionViewLog)
              << "[CompositionView] SSGI dispatch skipped"
              << renderer_->globalIlluminationDebugState()
              << "inputs=" << giInputs.validForScreenSpace();
        }
        if (profileThisVideoPreviewSetup) {
          profileGiBlockNs +=
              frameTimer.nsecsElapsed() - giBlockStartedNs;
        }
      }



      RenderPassContext resolveContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources resolveResources{

          &renderPipeline, layerRTV, layerSRV, layerFloatSRV,

          layerFloatUAV, accumSRV, tempUAV};

  FunctionalRenderPass resolvePass(

          FrameRenderPassKind::Post, QStringLiteral("Resolve"),

          [](RenderPassResources& resources) {

            return resources.pipeline && resources.accumSRV &&

                   resources.layerFloatSRV && resources.layerFloatUAV;

          },

          [&](RenderPassContext&, RenderPassResources& resources) {

              if (resources.pipeline && resources.accumSRV &&
                resources.pipeline->hasVelocityTarget() &&
                renderer_ && renderer_->device() &&
                renderer_->immediateContext()) {
              if (!motionBlurPass_) {
                motionBlurPass_ = std::make_unique<MotionBlurPass>();
              }
              const bool motionBlurPassReady = motionBlurPass_->initialize(
                  renderer_->device(), renderer_->immediateContext());
              const auto depthSRV = renderer_->offscreenTextureShaderResourceView(
                  previewRenderSlot.depthTargetView);
              MotionBlurSettings motionBlurSettings;
              const auto appSettings =
                  ArtifactCore::ArtifactAppSettings::instance();
              motionBlurSettings.enabled = postPassMask.timelineMotionBlur;
              motionBlurSettings.shutterAngle = appSettings
                  ? static_cast<float>(appSettings->timelineMotionBlurShutterAngle())
                  : 180.0f;
              motionBlurSettings.shutterPhase = appSettings
                  ? static_cast<float>(
                        appSettings->timelineMotionBlurShutterPhase() / 360.0)
                  : 0.0f;
              motionBlurSettings.shutterShape = appSettings
                  ? static_cast<MotionBlurShutterShape>(
                        appSettings->timelineMotionBlurShutterShape())
                  : MotionBlurShutterShape::Rectangular;
              motionBlurSettings.trapezoidPlateau = appSettings
                  ? static_cast<float>(
                        appSettings->timelineMotionBlurTrapezoidPlateau())
                  : 0.5f;
              motionBlurSettings.sampleCount = appSettings
                  ? static_cast<unsigned>(appSettings->timelineMotionBlurSampleCount())
                  : static_cast<unsigned>(previewDownsample_ > 1 ? 4 : 8);
              if (motionBlurPassReady && motionBlurPass_->applyWithCandidates(
                                         renderer_->immediateContext(),
                                         resources.accumSRV,
                                         resources.pipeline->velocitySRV(),
                                         depthSRV,
                                         resources.pipeline->tempUAV(),
                                         resources.pipeline->motionBlurScratchSRV(0),
                                         resources.pipeline->motionBlurScratchUAV(0),
                                         resources.pipeline->motionBlurScratchSRV(1),
                                         resources.pipeline->motionBlurScratchUAV(1),
                                         passResolution.renderWidthU(),
                                         passResolution.renderHeightU(),
                                         motionBlurSettings)) {
                resources.pipeline->swapAccumAndTemp();
                resources.accumSRV = resources.pipeline->accumSRV();
                resources.tempUAV = resources.pipeline->tempUAV();
                accumSRV = resources.accumSRV;
                tempUAV = resources.tempUAV;
              }
              }

            // Camera-layer motion blur: same velocity/depth contract,
            // shutter driven by the active camera's Blur Amount.
            const auto cameraPostDepthSRV =
                (renderer_ && previewRenderSlot.depthTargetView)
                    ? renderer_->offscreenTextureShaderResourceView(
                          previewRenderSlot.depthTargetView)
                    : nullptr;
            if (has3DCamera && activeCamera && activeCamera->motionBlur() &&
                cameraPostDepthSRV &&
                resources.pipeline && resources.accumSRV &&
                resources.pipeline->hasVelocityTarget() &&
                renderer_ && renderer_->device() &&
                renderer_->immediateContext()) {
              MotionBlurSettings cameraMotionBlurSettings;
              cameraMotionBlurSettings.enabled = true;
              cameraMotionBlurSettings.shutterAngle =
                  360.0f * std::clamp(activeCamera->blurAmount().value / 100.0f,
                                      0.0f, 1.0f);
              cameraMotionBlurSettings.shutterPhase = 0.0f;
              const auto appSettings =
                  ArtifactCore::ArtifactAppSettings::instance();
              cameraMotionBlurSettings.shutterShape = appSettings
                  ? static_cast<MotionBlurShutterShape>(
                        appSettings->timelineMotionBlurShutterShape())
                  : MotionBlurShutterShape::Rectangular;
              cameraMotionBlurSettings.trapezoidPlateau = appSettings
                  ? static_cast<float>(
                        appSettings->timelineMotionBlurTrapezoidPlateau())
                  : 0.5f;
              cameraMotionBlurSettings.sampleCount =
                  static_cast<unsigned>(previewDownsample_ > 1 ? 4 : 8);
              const bool motionBlurPassReady = motionBlurPass_ &&
                  motionBlurPass_->initialize(
                      renderer_->device(), renderer_->immediateContext());
              if (motionBlurPassReady &&
                  cameraMotionBlurSettings.shutterAngle > 0.0f &&
                  motionBlurPass_->applyWithCandidates(
                      renderer_->immediateContext(), resources.accumSRV,
                      resources.pipeline->velocitySRV(), cameraPostDepthSRV,
                      resources.pipeline->tempUAV(),
                      resources.pipeline->motionBlurScratchSRV(0),
                      resources.pipeline->motionBlurScratchUAV(0),
                      resources.pipeline->motionBlurScratchSRV(1),
                      resources.pipeline->motionBlurScratchUAV(1),
                      passResolution.renderWidthU(),
                      passResolution.renderHeightU(),
                      cameraMotionBlurSettings)) {
                resources.pipeline->swapAccumAndTemp();
                resources.accumSRV = resources.pipeline->accumSRV();
                resources.tempUAV = resources.pipeline->tempUAV();
                accumSRV = resources.accumSRV;
                tempUAV = resources.tempUAV;
              }
            }

            // Depth of field from the active camera lens parameters.
            if (postPassMask.depthOfField && has3DCamera && cameraPostDepthSRV &&
                resources.pipeline && renderer_ && renderer_->device() &&
                renderer_->immediateContext()) {
              if (!depthOfFieldPass_) {
                depthOfFieldPass_ = std::make_unique<DepthOfFieldPass>();
                depthOfFieldPass_->initialize(renderer_->device(),
                                              renderer_->immediateContext());
              }
              const auto lensParams =
                  activeCamera ? activeCamera->depthOfFieldParameters()
                               : CameraDOFParameters{};
              DepthOfFieldSettings dofSettings;
              dofSettings.enabled = depthOfFieldPass_->ready();
              dofSettings.focusDistance = lensParams.focusDistance;
              dofSettings.nearClip =
                  activeCamera ? activeCamera->nearClipPlane()
                               : ArtifactCore::Units::Pixels{1.0f};
              dofSettings.farClip =
                  activeCamera ? activeCamera->farClipPlane()
                               : ArtifactCore::Units::Pixels{100000.0f};
              dofSettings.maxCocRadius = ArtifactCore::Units::Pixels{16.0f};
              dofSettings.cocScale = lensParams.cocScale;
              // Thin-lens model: authored aperture acts as an f-stop scale
              // and focal length comes from the camera's 35mm-equivalent.
              dofSettings.fStop =
                  activeCamera && activeCamera->depthOfField()
                      ? activeCamera->aperture()
                      : ArtifactCore::Units::FStop{0.0f};
              dofSettings.focalLength =
                  activeCamera ? activeCamera->focalLength()
                               : ArtifactCore::Units::Millimeters{50.0f};
              dofSettings.sampleCount = previewDownsample_ > 1 ? 8u : 16u;
              if (depthOfFieldPass_->apply(
                      renderer_->immediateContext(), resources.accumSRV,
                      cameraPostDepthSRV, resources.pipeline->tempUAV(),
                      passResolution.renderWidthU(),
                      passResolution.renderHeightU(),
                      dofSettings)) {
                resources.pipeline->swapAccumAndTemp();
                resources.accumSRV = resources.pipeline->accumSRV();
                resources.tempUAV = resources.pipeline->tempUAV();
                accumSRV = resources.accumSRV;
                tempUAV = resources.tempUAV;
              }
            }

            // Geometry resolves into a single-sample composition target.
            // Apply FXAA only when the frame contains visible 3D, after AO and
            // motion blur, so 2D/text content keeps its exact pixels.
            const qint64 fxaaStartedNs = profileThisVideoPreviewSetup
                ? frameTimer.nsecsElapsed()
                : 0;
            bool fxaaApplied = false;
            const bool fxaaRequested =
                postPassMask.antiAliasingMode == 1 && hasVisible3DLayer &&
                resources.pipeline && renderer_ &&
                renderer_->immediateContext();
            bool fxaaPrepared = false;
            bool deferredRecordingStarted = false;
            if (fxaaRequested) {
              struct DeferredFxaaRequest {
                RenderPipeline* pipeline = nullptr;
                Diligent::ITextureView* source = nullptr;
                Diligent::ITextureView* destination = nullptr;
              } request{resources.pipeline, resources.accumSRV,
                        resources.pipeline->tempUAV()};
              const auto recordFxaa = +[](void* opaque,
                                          Diligent::IDeviceContext* context) {
                auto* deferredRequest =
                    static_cast<DeferredFxaaRequest*>(opaque);
                return deferredRequest && deferredRequest->pipeline && context &&
                       deferredRequest->pipeline->applyFastApproximateAntiAliasing(
                           context, deferredRequest->source,
                           deferredRequest->destination);
              };
              fxaaPrepared =
                  resources.pipeline->prepareFastApproximateAntiAliasing(
                      renderer_->immediateContext());
              deferredRecordingStarted = fxaaPrepared &&
                  renderer_->startDeferredContextWork(&request, recordFxaa);
              if (deferredRecordingStarted) {
                // Every visible layer has been collected by Resolve. The
                // shadow prepass touches separate targets and can run on the
                // owner context while the deferred worker records FXAA.
                renderer_->renderShadowMapFrame();
                shadowMapRenderedBeforeFinalize = true;
                fxaaApplied = renderer_->executeDeferredContextWork();
              }
              if (!fxaaApplied) {
                fxaaApplied =
                    resources.pipeline->applyFastApproximateAntiAliasing(
                        renderer_->immediateContext(), resources.accumSRV,
                        resources.pipeline->tempUAV());
              }
            }
            static thread_local unsigned int deferredFxaaDecisionSamples = 0;
            if (profileThisVideoPreviewSetup &&
                deferredFxaaDecisionSamples < 64) {
              std::fprintf(stderr,
                  "[VideoPreviewDeferredFxaa] requested=%d mode=%d visible3D=%d "
                  "pipeline=%d renderer=%d prepared=%d workerStarted=%d applied=%d\n",
                  fxaaRequested ? 1 : 0, postPassMask.antiAliasingMode,
                  hasVisible3DLayer ? 1 : 0, resources.pipeline ? 1 : 0,
                  renderer_ ? 1 : 0, fxaaPrepared ? 1 : 0,
                  deferredRecordingStarted ? 1 : 0, fxaaApplied ? 1 : 0);
              ++deferredFxaaDecisionSamples;
            }
            if (profileThisVideoPreviewSetup) {
              profileFxaaNs += frameTimer.nsecsElapsed() - fxaaStartedNs;
            }
            if (fxaaApplied) {
              resources.pipeline->swapAccumAndTemp();
              resources.accumSRV = resources.pipeline->accumSRV();
              resources.tempUAV = resources.pipeline->tempUAV();
              accumSRV = resources.accumSRV;
              tempUAV = resources.tempUAV;
            }

            const qint64 finalizeStartedNs = profileThisVideoPreviewSetup
                ? frameTimer.nsecsElapsed()
                : 0;
            ramPreviewReadbackSRV = finalizeGpuRenderToViewport(

                *resources.pipeline, resources.accumSRV,

                resources.layerFloatSRV, resources.layerFloatUAV,
                passResolution.renderWidthF(), passResolution.renderHeightF(),

                cw, ch, origViewW, origViewH, origZoom, origPanX, origPanY,

                bgColor, layerBgColor, origClearColor, backgroundMode,

                layerToFloatConvertCount);
            if (profileThisVideoPreviewSetup) {
              profileFinalizeNs =
                  frameTimer.nsecsElapsed() - finalizeStartedNs;
            }

            return ramPreviewReadbackSRV != nullptr;

          });

      const qint64 resolveExecuteStartedNs = profileThisVideoPreviewSetup
          ? frameTimer.nsecsElapsed()
          : 0;
      framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
          resolvePass, resolveContext, resolveResources) && framePassesSucceeded;
      if (profileThisVideoPreviewSetup) {
        profileResolveExecuteNs =
            frameTimer.nsecsElapsed() - resolveExecuteStartedNs;
      }

      postPassMs = markPhaseMs();
      addProfileBoundary(profilePostBoundaryNs);

    } else {

      // === Fallback path (GPU パイプラインなし) ===

      renderCrashTrace("render-branch-fallback", renderFrameCounter_);

      RenderPassContext fallbackContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources fallbackResources;

      FunctionalRenderPass fallbackBasePass(

          FrameRenderPassKind::Base, QStringLiteral("Direct Base"),

          [](RenderPassResources&) { return true; },

          [&](RenderPassContext& context, RenderPassResources&) {

      context.renderer->setViewportRect(viewportW, viewportH);

      const float prevZoom = renderer_->getZoom();

      float prevPanX = 0.0f;

      float prevPanY = 0.0f;

      renderer_->getPan(prevPanX, prevPanY);

      renderer_->setUseExternalMatrices(false);

      renderer_->resetGizmoCameraMatrices();

      renderer_->reset3DCameraMatrices();

      const bool transparentCompositionBackground = layerBgColor.a() < 0.999f;

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

      if (backgroundMode == CompositionBackgroundMode::Checkerboard ||

          transparentCompositionBackground) {

        renderer_->setCanvasSize(viewportW, viewportH);

        renderer_->setZoom(1.0f);

        renderer_->setPan(0.0f, 0.0f);

        drawViewportCheckerboardBackground(renderer_.get(), viewportW,

                                           viewportH,

                                           checkerboardTileSize_);

        renderer_->setCanvasSize(cw, ch);

        renderer_->setZoom(prevZoom);

        renderer_->setPan(prevPanX, prevPanY);

      }

      // Composition Space で直接 fill する（viewport-space 変換不要）

      drawThreeDimensionalGroundGrid();

      drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,

                                      backgroundMode, checkerboardTileSize_,

                                      cachedMayaGradientSprite_,
                                      usesSpatialViewportOrientation()
                                          ? &viewportOrientationViewForOverlay_
                                          : nullptr,
                                      usesSpatialViewportOrientation()
                                          ? &viewportOrientationProjectionForOverlay_
                                          : nullptr);

      if (showCompositionRegionOverlay_ && comp &&
          !usesSpatialViewportOrientation()) {
        ::Artifact::drawCompositionRegionOverlay(renderer_.get(), comp);
      }
      if (previewOrbitActive_ && comp) {
        ::Artifact::drawNavigationCrossOverlay(renderer_.get(), comp);
      }

      lastPresentedReadbackSRV_ = nullptr;

      renderer_->setUseExternalMatrices(false);

      renderer_->resetGizmoCameraMatrices();

      renderer_->reset3DCameraMatrices();

      renderer_->setCanvasSize(cw, ch);

      renderer_->setZoom(prevZoom);

      renderer_->setPan(prevPanX, prevPanY);

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

      return true;

          });

      framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
          fallbackBasePass, fallbackContext, fallbackResources) &&
          framePassesSucceeded;

      basePassMs = markPhaseMs();
      addProfileBoundary(profileBaseBoundaryNs);



      const bool renderToCompositionSpaceCache = compositionSpaceCacheBuild;
      float savedRenderZoom = renderer_->getZoom();
      float savedRenderPanX = 0.0f;
      float savedRenderPanY = 0.0f;
      renderer_->getPan(savedRenderPanX, savedRenderPanY);

      if (!frameOutOfRange) {
        const float renderZoom = renderToCompositionSpaceCache
                                     ? compositionCacheScale
                                     : renderer_->getZoom();
        const QRectF renderRoi = renderToCompositionSpaceCache
                                     ? QRectF(0.0, 0.0, cw, ch)
                                     : roiRect;
        const DetailLevel renderLod =
            renderToCompositionSpaceCache
                ? detailLevelFromZoom(renderZoom)
                : detailLevelFromZoom(renderer_->getZoom());
        if (renderToCompositionSpaceCache) {
          renderer_->pushRenderTarget(
              compositionSpaceGpuCache_.colorTargetView);
          renderer_->setViewportRect(compositionCacheSize.width(),
                                     compositionCacheSize.height());
          renderer_->setCanvasSize(cw, ch);
          renderer_->setZoom(compositionCacheScale);
          renderer_->setPan(0.0f, 0.0f);
          renderer_->clearRenderTarget(
              FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
        } else if (compositionSpaceCacheHit) {
          // Presentation is deferred until the common post-process path below.
        }

        renderer_->setDetailLevel(static_cast<LODManager::DetailLevel>(

            renderLod)); // Pass LOD to renderer/effects



        if (!compositionSpaceCacheHit) {
          for (const auto &layer : layers) {

          if (!isLayerEffectivelyVisible(layer)) {

            ++skipInvisibleCount;

            continue;

          }

          if (hasSoloLayer && !layer->isSolo()) {

            ++skipSoloCount;

            continue;

          }

          if (!layer->isActiveAt(currentFrame)) {

            ++skipInactiveCount;

            continue;

          }



          // === 段階 2: ROI 計算 ===

          const QRectF layerBounds = effectExpandedLayerBounds(layer.get());
          const bool particle2D =
              dynamic_cast<const ArtifactParticleLayer*>(layer.get()) != nullptr &&
              !layer->is3D();

          const QRectF intersected = layerBounds.intersected(renderRoi);



          // --- Feature 3: Layer Drawing Skip (LOD-based) ---

          // Skip rendering layers that are too small to be visible on screen.

          // Keep Text Layer visible while zoomed out; unlike raster footage,
          // its small on-screen footprint is still meaningful to the editor.
          const bool preserveTextAtAnyZoom =
              dynamic_cast<const ArtifactTextLayer *>(layer.get()) != nullptr;
          if (layerBounds.isValid() && !preserveTextAtAnyZoom && !particle2D) {

            const auto tl = renderer_->canvasToViewport(

                {(float)layerBounds.left(), (float)layerBounds.top()});

            const auto br = renderer_->canvasToViewport(

                {(float)layerBounds.right(), (float)layerBounds.bottom()});

            const float screenW = std::abs(br.x - tl.x);

            const float screenH = std::abs(br.y - tl.y);



            if (renderLod == DetailLevel::Low) {

              if (screenW < 8.0f || screenH < 8.0f) {

                ++skipLodCount;

                continue;

              }

            } else if (renderLod == DetailLevel::Medium) {

              if (screenW < 2.0f || screenH < 2.0f) {

                ++skipLodCount;

                continue;

              }

            }

          }

          // ------------------------------------------------



          // === 段階 3: 空 ROI スキップ ===

          if (!particle2D && intersected.isEmpty()) {

            ++skipRoiCount;

            continue; // 画面外レイヤーをスキップ

          }



          ++drawnLayerCount;
          if (const auto* video = dynamic_cast<const ArtifactVideoLayer*>(layer.get())) {
            videoFramesReadyForRamPreview = videoFramesReadyForRamPreview &&
                static_cast<bool>(video->cachedFrameImageBufferShared(framePos));
          }

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

          const bool isolateSkipThisLayer =
              showIsolationOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          if (isolateSkipThisLayer) {
            continue;
          }

          const bool xRayDimThisLayer =
              showXRayOverlay_ && !selectedLayerId_.isNil() &&
              layer->id() != selectedLayerId_;
          const float opacity =
              layer->opacity() *
              (xRayDimThisLayer ? xRayOverlayOpacity_ : 1.0f);

          if (opacity <= 0.0f) {

            ++opacityZeroCount;

            continue;

          }

          ++compositedLayerCount;

          const int layerMaskCount = layer->maskCount();

          if (layerMaskCount > 0) {

            ++maskedLayerCount;

            totalMaskCount += layerMaskCount;

          }

          if (blendMode != ArtifactCore::BlendMode::Normal) {

            ++nonNormalBlendLayerCount;

          }

          if (blendMaskLayerNotes.size() < 4 &&

              (blendMode != ArtifactCore::BlendMode::Normal ||

               layerMaskCount > 0)) {

            blendMaskLayerNotes

                << QStringLiteral("%1:blend=%2 opacity=%3 masks=%4")

                       .arg(layer->layerName(),

                            ArtifactCore::BlendModeUtils::toString(blendMode),

                            QString::number(opacity, 'f', 3))

                       .arg(layerMaskCount);

          }

          QString *dbgOut =

              QLoggingCategory::defaultCategory()->isDebugEnabled()

                  ? &lastVideoDebug_

                  : nullptr;

          RenderPassContext directContext{renderer_.get(),

                                          renderFrameCounter_};

          RenderPassResources directResources;

          if (layer->revealEnabled() && !revealFallbackReported_) {
            qWarning() << "[CompositionView] Reveal requires GPU compositing; direct preview bypasses Reveal";
            revealFallbackReported_ = true;
          }

          FunctionalRenderPass directCompositePass(

              FrameRenderPassKind::Composite,

              QStringLiteral("Direct Layer Composite"),

              [](RenderPassResources&) { return true; },

              [&](RenderPassContext& context, RenderPassResources&) {

                drawLayerForCompositionView(

                    layer.get(), context.renderer, opacity, dbgOut,

                    &surfaceCache_, &layerSurfaceCacheDiagnosticsEnabled_,
                    &layerSurfaceCacheHitCounts_,
                    &layerSurfaceCacheMissCounts_,
                    &layerSurfaceCacheBuildCounts_,
                    gpuTextureCacheManager_.get(),

                    currentFrame.framePosition(), false, renderLod,

                    has3DCamera ? &cameraViewMatrix : nullptr,

                    has3DCamera ? &cameraProjMatrix : nullptr,
                    has3DCamera ? &previousCameraViewMatrix : nullptr,
                    has3DCamera ? &previousCameraProjMatrix : nullptr, &matteResolver,
                    &sceneLights, draftRendering,
                    usesSpatialViewportOrientation(),

                    surfaceGeneration(layer.get()), nullptr, false, nullptr,
                    &viewportCullingStats_);

                return true;

              });

          framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
              directCompositePass, directContext, directResources) &&
              framePassesSucceeded;

          surfacePassMs = markPhaseMs();
          addProfileBoundary(profileDirectLayerBoundaryNs);

          maskPassMs = surfacePassMs;

          compositePassMs = surfacePassMs;



          // === 段階 7: ROI デバッグ表示 ===

          if (debugMode_) {

            // ROI を赤い枠で表示

            const bool showDebugProbe = hostSnapshot.showDebugProbeLines;

            drawTaggedRectOutline(renderer_.get(), intersected,

                                  FloatColor{1.0f, 0.0f, 0.0f, 1.0f},

                                  showDebugProbe);

          }

          }
          }
        }

        if (renderToCompositionSpaceCache) {
          renderer_->flush();
          renderer_->popRenderTarget();
          renderer_->setViewportRect(viewportW, viewportH);
          renderer_->setCanvasSize(cw, ch);
          renderer_->setZoom(savedRenderZoom);
          renderer_->setPan(savedRenderPanX, savedRenderPanY);
          Diligent::ITextureView* displayTexture =
              compositionSpaceGpuCache_.colorShaderResourceView;
          displayTexture = applyDisplayColorTransform(
              displayTexture, compositionSpaceGpuCache_.postProcessUnorderedAccessView,
              compositionSpaceGpuCache_.size.width(),
              compositionSpaceGpuCache_.size.height());
          QMatrix4x4 identity;
          renderer_->drawSpriteTransformed(
              0.0f, 0.0f, cw, ch, identity,
              displayTexture, 1.0f);
          compositionSpaceGpuCache_.contentKey = compositionCacheKey;
        }

        if (compositionSpaceCacheHit && !renderToCompositionSpaceCache) {
          Diligent::ITextureView* displayTexture =
              compositionSpaceGpuCache_.colorShaderResourceView;
          displayTexture = applyDisplayColorTransform(
              displayTexture, compositionSpaceGpuCache_.postProcessUnorderedAccessView,
              compositionSpaceGpuCache_.size.width(),
              compositionSpaceGpuCache_.size.height());
          QMatrix4x4 identity;
          renderer_->drawSpriteTransformed(
              0.0f, 0.0f, cw, ch, identity, displayTexture, 1.0f);
        }

      }

      // Record the direct background/layer batch on the bounded deferred worker.
      // Overlay packets use the second command buffer while recording proceeds;
      // any intervening immediate-render barrier drains this batch first, and
      // present executes it before the overlays.
      markVideoPreviewSetupStage(14);
      if (!shadowMapRenderedBeforeFinalize) {
        renderer_->renderShadowMapFrame();
      }
      const auto flushContextBeforeUs = profileThisVideoPreviewSetup
          ? renderer_->flushContextTimeUs() : 0;
      const auto flushStartedNs = profileThisVideoPreviewSetup
          ? frameTimer.nsecsElapsed() : 0;
      renderer_->flushDeferredPacketsAsync();
      if (profileThisVideoPreviewSetup && useRamPreviewFallback) {
        const auto flushFinishedNs = frameTimer.nsecsElapsed();
        std::fprintf(stderr,
            "[VideoPreviewRamFlushProfile] frame=%llu totalMs=%.3f contextFlushMs=%.3f\n",
            static_cast<unsigned long long>(renderFrameCounter_),
            static_cast<double>(flushFinishedNs - flushStartedNs) / 1.0e6,
            static_cast<double>(renderer_->flushContextTimeUs() - flushContextBeforeUs) / 1000.0);
      }
      renderer_->setOverrideRTV(nullptr);
      compositeFinalizedThisFrame_ = true;
      markVideoPreviewSetupStage(15);

      postPassMs = markPhaseMs();
      addProfileBoundary(profilePostBoundaryNs);



    if (!compositeFinalizedThisFrame_ &&
        compositionViewLog().isDebugEnabled()) {
      qCDebug(compositionViewLog)
          << "[CompositionView] overlay ran before composite finalize";
    }

    drawSelectionEditingOverlay(hostSnapshot, comp, layers, selectedIds,

                                currentFrame, cw, ch, has3DCamera,

                                cameraViewMatrix, cameraProjMatrix);
    markVideoPreviewSetupStage(16);



    /* if (showGizmoOverlay_ && gizmo_) {

      ArtifactCore::ProfileScope _profGizmo(

          "GizmoMask", ArtifactCore::ProfileCategory::Render);

      auto selectedLayer = (!selectedLayerId_.isNil() && comp)

                               ? comp->layerById(selectedLayerId_)

                               : ArtifactAbstractLayerPtr{};

      if (selectedLayer && isLayerEffectivelyVisible(selectedLayer)) {

        gizmo_->setMode(gizmoMode_);

        if (!selectedLayer->is3D()) {

          ArtifactCore::ProfileScope _profG2D(

              "Gizmo2D", ArtifactCore::ProfileCategory::Render);

          sync2DGizmosForLayer(selectedLayer);

          if (contentEditMode_ && layerUsesContentGizmo(selectedLayer) &&
              contentGizmo_) {

            ArtifactCore::ProfileScope _profG2DDrawCall(

                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);

            contentGizmo_->draw(renderer_.get());

          } else if (layerUsesTextGizmo(selectedLayer) && textGizmo_) {

            ArtifactCore::ProfileScope _profG2DDrawCall(

                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);

            textGizmo_->draw(renderer_.get());

          } else if (gizmo_) {

            ArtifactCore::ProfileScope _profG2DDrawCall(

                "Gizmo2DDrawCall", ArtifactCore::ProfileCategory::Render);

            const bool showDuringDrag =

                ArtifactCore::ArtifactAppSettings::instance()

                    ? ArtifactCore::ArtifactAppSettings::instance()

                          ->compositionShowGizmoDuringDrag()

                    : false;

            const bool suppressMoveDragVisual =

                gizmo_->isDragging() &&

                gizmo_->activeHandle() == TransformGizmo::HandleType::Move &&

                !showDuringDrag;

            if (!suppressMoveDragVisual) {

              gizmo_->draw(renderer_.get());

            }

          }

        } else {

          sync2DGizmosForLayer(nullptr);

        }



        if (gizmo3D_ && selectedLayer->is3D()) {

          ArtifactCore::ProfileScope _profG3D(

              "Gizmo3D", ArtifactCore::ProfileCategory::Render);

          syncGizmo3DFromLayer(selectedLayer);

          const float viewportW =

              hostWidth_ > 0.0f ? hostWidth_ : lastCanvasWidth_;

          const float viewportH =

              hostHeight_ > 0.0f ? hostHeight_ : lastCanvasHeight_;

          if (!gizmo3DCameraMatricesValid_ && viewportW > 0.0f && viewportH > 0.0f) {

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

              gizmo3D_->draw(renderer_.get(), view, proj, viewportW, viewportH);

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

            gizmo3D_->draw(
                renderer_.get(),
                gizmo3DCameraMatricesValid_ ? gizmo3DViewMatrix_
                                             : renderer_->getViewMatrix(),
                gizmo3DCameraMatricesValid_ ? gizmo3DProjectionMatrix_
                                             : renderer_->getProjectionMatrix(), viewportW, viewportH);

            }

            {

              ArtifactCore::ProfileScope _profG3DFlush(

                  "Gizmo3DFlush", ArtifactCore::ProfileCategory::Render);

          renderer_->flushGizmo3D();

          }

        }

        if (gizmo3D_->isDragging()) {
          const QVector3D hudPosition = ArtifactCore::Coordinates::toQVector3D(
              gizmo3D_->position());
          const QVector3D hudRotation = ArtifactCore::Units::toQVector3D(gizmo3D_->rotation());
          const QVector3D hudScale = ArtifactCore::Units::toQVector3D(
              gizmo3D_->scale());
          const QString activeOperation =
              gizmo3D_->activeOperation() == GizmoOperation::Rotate
                  ? QStringLiteral("Rotate")
                  : (gizmo3D_->activeOperation() == GizmoOperation::Scale
                         ? QStringLiteral("Scale")
                         : (gizmo3D_->activeOperation() == GizmoOperation::Translate
                                ? QStringLiteral("Move")
                                : (gizmo3D_->mode() == GizmoMode::Rotate
                                       ? QStringLiteral("Rotate")
                                       : (gizmo3D_->mode() == GizmoMode::Scale
                                              ? QStringLiteral("Scale")
                                              : QStringLiteral("Move"))));
          const float panelWidth = std::clamp(
              viewportW - 16.0f, 176.0f, 276.0f);
          const float panelHeight = 86.0f;
          const float panelX = std::clamp(
              viewportW - panelWidth - 12.0f, 8.0f,
              std::max(8.0f, viewportW - panelWidth - 8.0f));
          const float panelY = 12.0f;
          const QFont hudFont = fixedWidthFont(9);
          renderer_->drawRoundedPanel(
              panelX, panelY, panelWidth, panelHeight, 7.0f,
              FloatColor{0.03f, 0.04f, 0.06f, 0.88f},
              FloatColor{0.55f, 0.78f, 1.0f, 0.95f}, 1.0f, 1.0f);
          renderer_->drawText(
              QRectF(panelX + 10.0f, panelY + 7.0f, panelWidth - 20.0f, 16.0f),
              QStringLiteral("3D %1  Axis %2")
                  .arg(activeOperation)
                  .arg(gizmo3D_->activeAxis() == GizmoAxis::X
                           ? QStringLiteral("X")
                           : gizmo3D_->activeAxis() == GizmoAxis::Y
                                 ? QStringLiteral("Y")
                                 : gizmo3D_->activeAxis() == GizmoAxis::Z
                                       ? QStringLiteral("Z")
                                       : gizmo3D_->activeAxis() == GizmoAxis::Screen
                                             ? QStringLiteral("Screen")
                                             : gizmo3D_->activeAxis() == GizmoAxis::XY
                                                   ? QStringLiteral("XY")
                                                   : gizmo3D_->activeAxis() == GizmoAxis::YZ
                                                         ? QStringLiteral("YZ")
                                                         : gizmo3D_->activeAxis() == GizmoAxis::XZ
                                                               ? QStringLiteral("XZ")
                                                               : QStringLiteral("Plane")),
              hudFont, FloatColor{0.92f, 0.97f, 1.0f, 1.0f},
              Qt::AlignLeft | Qt::AlignTop);
          renderer_->drawText(
              QRectF(panelX + 10.0f, panelY + 25.0f, panelWidth - 20.0f, 16.0f),
              QStringLiteral("P %1, %2, %3  R %4, %5, %6")
                  .arg(hudPosition.x(), 0, 'f', 1)
                  .arg(hudPosition.y(), 0, 'f', 1)
                  .arg(hudPosition.z(), 0, 'f', 1)
                  .arg(hudRotation.x(), 0, 'f', 1)
                  .arg(hudRotation.y(), 0, 'f', 1)
                  .arg(hudRotation.z(), 0, 'f', 1),
              hudFont, FloatColor{0.72f, 0.82f, 0.90f, 1.0f},
              Qt::AlignLeft | Qt::AlignTop);
          renderer_->drawText(
              QRectF(panelX + 10.0f, panelY + 43.0f, panelWidth - 20.0f, 16.0f),
              QStringLiteral("Size %1 × %2 px")
                  .arg(selectedLayer->localBounds().width() *
                           std::abs(hudScale.x()),
                       0, 'f', 1)
                  .arg(selectedLayer->localBounds().height() *
                           std::abs(hudScale.y()),
                       0, 'f', 1),
              hudFont, FloatColor{0.72f, 0.82f, 0.90f, 1.0f},
              Qt::AlignLeft | Qt::AlignTop);
          renderer_->drawText(
              QRectF(panelX + 10.0f, panelY + 61.0f, panelWidth - 20.0f, 16.0f),
              QStringLiteral("S %1, %2, %3")
                  .arg(hudScale.x(), 0, 'f', 2)
                  .arg(hudScale.y(), 0, 'f', 2)
                  .arg(hudScale.z(), 0, 'f', 2),
              hudFont, FloatColor{0.72f, 0.82f, 0.90f, 1.0f},
              Qt::AlignLeft | Qt::AlignTop);
        }

      }



        // Tracker Gizmo overlay (independent of selection)

        {
          if (trackerGizmo_ && hostSnapshot.trackerToolActive) {

            ArtifactCore::ProfileScope _profTracker(

                "TrackerGizmo", ArtifactCore::ProfileCategory::Render);

            const double trackerFps =
                std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
            trackerGizmo_->setCurrentFrame(
                static_cast<double>(currentFrame.framePosition()) / trackerFps);

            trackerGizmo_->draw(renderer_.get());

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

          const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};

          const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};

          const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};

          const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

          const FloatColor handleStrokeColor = {0.82f, 0.92f, 1.0f, 0.88f};

          const FloatColor handlePointColor = {0.70f, 0.90f, 1.0f, 0.95f};

          const FloatColor handleHoverColor = {1.0f, 0.78f, 0.32f, 1.0f};

          const FloatColor handleDragColor = {1.0f, 0.44f, 0.24f, 1.0f};

          const FloatColor activeMaskStrokeColor = {1.0f, 0.86f, 0.42f, 0.60f};

          const FloatColor activeMaskPointColor = {1.0f, 0.90f, 0.54f, 1.0f};

          const FloatColor activeMaskHandleColor = {1.0f, 0.82f, 0.40f, 1.0f};

          if (isMaskRubberBandSelecting_) {
            const QRectF selectionRect =
                QRectF(ArtifactCore::Coordinates::toQPointF(
                           maskRubberBandStartCanvas_),
                       ArtifactCore::Coordinates::toQPointF(
                           maskRubberBandCurrentCanvas_)).normalized();
            renderer_->drawRectOutlineLocal(
                static_cast<float>(selectionRect.left()),
                static_cast<float>(selectionRect.top()),
                static_cast<float>(selectionRect.width()),
                static_cast<float>(selectionRect.height()),
                FloatColor{0.35f, 0.82f, 1.0f, 0.95f});
          }

          constexpr float maskStrokeWidth = 5.2f;

          constexpr float handleStrokeWidth = 3.0f;

          const bool showMaskPath = hostSnapshot.showMaskPathLines;
          const bool showMaskHandle = hostSnapshot.showMaskHandleLines;

          const auto isSelectedVertex = [this](int maskIndex, int pathIndex,
                                                int vertexIndex) {
            return std::find(selectedMaskVertices_.begin(),
                             selectedMaskVertices_.end(),
                             ArtifactCore::artifactMakeTuple(maskIndex, pathIndex, vertexIndex)) !=
                   selectedMaskVertices_.end();
          };

          const auto hasSelectedVertexOnMask = [this](int maskIndex) {
            return std::any_of(selectedMaskVertices_.begin(),
                               selectedMaskVertices_.end(),
                               [maskIndex](const auto& selection) {
                                 return ArtifactCore::artifactGet<0>(selection) == maskIndex;
                               });
          };



          for (int m = 0; m < maskCount; ++m) {

            LayerMask mask = selectedLayer->mask(m);

            if (!mask.isEnabled())

              continue;

            const bool isActiveMask =

                (isDraggingMaskHandle_ && draggingMaskIndex_ == m) ||

                (isDraggingVertex_ && draggingMaskIndex_ == m) ||

                (hoveredMaskIndex_ == m) || hasSelectedVertexOnMask(m);

            const FloatColor pathStrokeColor = isActiveMask
                ? handleStrokeColor
                : FloatColor{0.55f, 0.72f, 0.84f, 0.38f};



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

                  const bool showVertexTangents = showMaskHandle && isActiveMask &&
                      (isSelectedVertex(m, p, v) ||
                       (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
                        hoveredVertexIndex_ == v) ||
                       (isDraggingMaskHandle_ && draggingMaskIndex_ == m &&
                        draggingPathIndex_ == p && draggingVertexIndex_ == v));



                  if (showVertexTangents && vertex.inTangent != QPointF(0, 0)) {

                    const FloatColor strokeColor =

                        isActiveMask ? activeMaskStrokeColor : handleStrokeColor;

                    renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas,

                                                  handleStrokeWidth, strokeColor);

                    FloatColor handleColor = handlePointColor;

                    if (isActiveMask) {

                      handleColor = activeMaskHandleColor;

                    }

                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&

                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskEditHandleType::InTangent)) {

                      handleColor = handleDragColor;

                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&

                               hoveredMaskHandleType_ == static_cast<int>(MaskEditHandleType::InTangent)) {

                      handleColor = handleHoverColor;

                    }

                    drawMaskSquareMarker(renderer_.get(), inHandleCanvas, 7.0f,

                                         handleColor, &maskPointShadowColor,

                                         4.0f);

                  }

                  if (showVertexTangents && vertex.outTangent != QPointF(0, 0)) {

                    const FloatColor strokeColor =

                        isActiveMask ? activeMaskStrokeColor : handleStrokeColor;

                    renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas,

                                                  handleStrokeWidth, strokeColor);

                    FloatColor handleColor = handlePointColor;

                    if (isActiveMask) {

                      handleColor = activeMaskHandleColor;

                    }

                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&

                        draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskEditHandleType::OutTangent)) {

                      handleColor = handleDragColor;

                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&

                               hoveredMaskHandleType_ == static_cast<int>(MaskEditHandleType::OutTangent)) {

                      handleColor = handleHoverColor;

                    }

                    drawMaskSquareMarker(renderer_.get(), outHandleCanvas, 7.0f,

                                         handleColor, &maskPointShadowColor,

                                         4.0f);

                  }

                  // A zero-value feather keeps a screen-space handle so it can
                  // be enabled directly without first opening a property panel.
                  if (showMaskHandle && isActiveMask) {

                    const QPointF featherCanvasPt =

                        maskFeatherHandleCanvasPosition(path, v, globalTransform,
                                                        renderer_.get());

                    const Detail::float2 featherCanvas = {

                        (float)featherCanvasPt.x(), (float)featherCanvasPt.y()};

                    FloatColor featherStroke = isActiveMask

                        ? activeMaskStrokeColor : handleStrokeColor;

                    renderer_->drawThickLineLocal(currentCanvasPos, featherCanvas,

                                                  1.0f, featherStroke);

                    FloatColor featherColor = {0.32f, 0.72f, 1.0f, 0.95f};

                    if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&

                        draggingVertexIndex_ == v &&

                        draggingMaskHandleType_ == static_cast<int>(MaskEditHandleType::FeatherHandle)) {

                      featherColor = handleDragColor;

                    } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&

                               hoveredVertexIndex_ == v &&

                               hoveredMaskHandleType_ == static_cast<int>(MaskEditHandleType::FeatherHandle)) {

                      featherColor = handleHoverColor;

                    }

                    drawMaskSquareMarker(renderer_.get(), featherCanvas, 6.0f,

                                         featherColor, &maskPointShadowColor,

                                         3.0f);

                  }



                  if (showMaskPath && v > 0) {

                    const MaskVertex previousVertex = path.vertex(v - 1);

                    const QVector<QPointF> samples =

                        maskSegmentPolyline(previousVertex, vertex, 18);

                    const float featherRange = std::max(
                        {path.feather().value, path.featherHorizontal().value,
                         path.featherVertical().value, path.featherInner().value,
                         path.featherOuter().value});
                    const float expansionRange =
                        std::abs(path.expansion().value);
                    if (featherRange > 0.01f || expansionRange > 0.01f) {
                      const FloatColor rangeColor =
                          {0.32f, 0.72f, 1.0f, isActiveMask ? 0.30f : 0.18f};
                      const float rangeWidth = std::clamp(
                          2.0f + 2.0f * featherRange + expansionRange,
                          2.0f, 80.0f);
                      for (int i = 1; i < samples.size(); ++i) {
                        const QPointF a = globalTransform.map(samples[i - 1]);
                        const QPointF b = globalTransform.map(samples[i]);
                        renderer_->drawThickLineLocal(
                            {static_cast<float>(a.x()), static_cast<float>(a.y())},
                            {static_cast<float>(b.x()), static_cast<float>(b.y())},
                            rangeWidth, rangeColor);
                      }
                    }

                    for (int i = 1; i < samples.size(); ++i) {

                      const QPointF a = globalTransform.map(samples[i - 1]);

                      const QPointF b = globalTransform.map(samples[i]);

                      if (isActiveMask) {

                        renderer_->drawThickLineLocal(

                            {static_cast<float>(a.x()), static_cast<float>(a.y())},

                            {static_cast<float>(b.x()), static_cast<float>(b.y())},

                            maskStrokeWidth + 2.0f, activeMaskStrokeColor);

                      }

                      renderer_->drawThickLineLocal(

                          {static_cast<float>(a.x()),

                           static_cast<float>(a.y())},

                          {static_cast<float>(b.x()),

                           static_cast<float>(b.y())},

                          maskStrokeWidth, pathStrokeColor);

                    }

                  }



                  FloatColor currentColor = maskPointColor;

                  float currentPointRadius = 14.0f;



                  if (isDraggingVertex_ && draggingMaskIndex_ == m &&

                      draggingPathIndex_ == p && draggingVertexIndex_ == v) {

                    currentColor = dragColor;

                    currentPointRadius = 17.0f;

                  } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&

                             hoveredVertexIndex_ == v) {

                    currentColor = hoverColor;

                    currentPointRadius = 17.0f;

                  } else if (isSelectedVertex(m, p, v)) {

                    currentColor = {0.35f, 0.82f, 1.0f, 1.0f};

                    currentPointRadius = 17.0f;

                  } else if (isActiveMask) {

                    currentColor = activeMaskPointColor;

                    currentPointRadius = 16.0f;

                  }



                  markers.push_back(

                      {currentCanvasPos, currentColor, currentPointRadius});

                  lastCanvasPos = currentCanvasPos;

               }

              }



              if (showMaskPath && path.isClosed() && vertexCount > 1) {

                MaskVertex firstVertex = path.vertex(0);

                MaskVertex lastVertex = path.vertex(vertexCount - 1);

                const QVector<QPointF> samples =

                    maskSegmentPolyline(lastVertex, firstVertex, 18);

                for (int i = 1; i < samples.size(); ++i) {

                  const QPointF a = globalTransform.map(samples[i - 1]);

                  const QPointF b = globalTransform.map(samples[i]);

                  if (isActiveMask) {

                    renderer_->drawThickLineLocal(

                        {static_cast<float>(a.x()), static_cast<float>(a.y())},

                        {static_cast<float>(b.x()), static_cast<float>(b.y())},

                        maskStrokeWidth + 2.0f, activeMaskStrokeColor);

                  }

                  renderer_->drawThickLineLocal(

                      {static_cast<float>(a.x()), static_cast<float>(a.y())},

                      {static_cast<float>(b.x()), static_cast<float>(b.y())},

                      maskStrokeWidth, pathStrokeColor);

                }

              }



              {

                ArtifactCore::ProfileScope _profMaskPoints(

                    "MaskDrawPoints", ArtifactCore::ProfileCategory::Render);

                if (showMaskHandle) {

                  for (const auto &marker : markers) {

                    drawMaskSquareMarker(renderer_.get(), marker.pos,

                                          std::max(5.5f, marker.radius * 0.42f),

                                          marker.color, &maskPointShadowColor,

                                          3.0f);

                  }

               }

              }

              }

            }

        }



      }



        // Pending custom shape path preview (Pen tool on shape layers).
        // Custom path vertices + tangent handles for the selected shape.
        if (!pendingShapePathCreation_) {
          if (auto *selectedShape = dynamic_cast<ArtifactShapeLayer *>(
                  selectedLayer.get());
              selectedShape && selectedShape->hasCustomPath()) {
            const QTransform pathGlobal =
                selectedLayer->getGlobalTransform();
            const auto verts = selectedShape->customPathVertices();
            const FloatColor vertexColor = {0.55f, 0.95f, 1.0f, 0.95f};
            const FloatColor vertexShadow = {0.0f, 0.0f, 0.0f, 0.36f};
            const FloatColor tangentColor = {0.85f, 0.9f, 1.0f, 0.8f};
            const FloatColor handleLinkColor =
                {0.7f, 0.85f, 1.0f, 0.45f};
            for (size_t vI = 0; vI < verts.size(); ++vI) {
              const auto &vertex = verts[vI];
              const QPointF worldPos = pathGlobal.map(vertex.pos);
              const Detail::float2 vertexCanvas = {
                  static_cast<float>(worldPos.x()),
                  static_cast<float>(worldPos.y())};
              if (vertex.inTangent != QPointF(0, 0) ||
                  vertex.outTangent != QPointF(0, 0)) {
                const QPointF inWorld =
                    pathGlobal.map(vertex.pos + vertex.inTangent);
                const QPointF outWorld =
                    pathGlobal.map(vertex.pos + vertex.outTangent);
                renderer_->drawSolidLine(
                    vertexCanvas,
                    {static_cast<float>(inWorld.x()),
                     static_cast<float>(inWorld.y())},
                    handleLinkColor, 1.2f);
                renderer_->drawSolidLine(
                    vertexCanvas,
                    {static_cast<float>(outWorld.x()),
                     static_cast<float>(outWorld.y())},
                    handleLinkColor, 1.2f);
                drawMaskSquareMarker(
                    renderer_.get(),
                    {static_cast<float>(inWorld.x()),
                     static_cast<float>(inWorld.y())},
                    5.5f, tangentColor, nullptr, 0.0f);
                drawMaskSquareMarker(
                    renderer_.get(),
                    {static_cast<float>(outWorld.x()),
                     static_cast<float>(outWorld.y())},
                    5.5f, tangentColor, nullptr, 0.0f);
              }
              drawMaskSquareMarker(renderer_.get(), vertexCanvas, 7.0f,
                                   vertexColor, &vertexShadow, 4.0f);
            }
          }
        }

        if (pendingShapePathCreation_ &&
            pendingShapePathLayerId_ == selectedLayer->id() &&
            !pendingShapePathVertices_.empty()) {
          const QTransform shapeGlobal =
              selectedLayer->getGlobalTransform();
          const FloatColor pendingLineColor = {0.55f, 0.95f, 1.0f, 0.9f};
          const FloatColor pendingPointShadowColor = {0.0f, 0.0f, 0.0f, 0.36f};
          const FloatColor pendingPointColor = {0.62f, 0.98f, 1.0f, 0.92f};
          Detail::float2 previousCanvasPos{};
          for (size_t v = 0; v < pendingShapePathVertices_.size(); ++v) {
            const QPointF canvasPos = shapeGlobal.map(
                pendingShapePathVertices_[v].pos);
            const Detail::float2 currentCanvasPos = {
                static_cast<float>(canvasPos.x()),
                static_cast<float>(canvasPos.y())};
            if (v > 0) {
              renderer_->drawThickLineLocal(
                  previousCanvasPos, currentCanvasPos, 5.8f,
                  pendingLineColor);
            }
            drawMaskSquareMarker(renderer_.get(), currentCanvasPos, 7.0f,
                                 pendingPointColor,
                                 &pendingPointShadowColor, 4.0f);
            previousCanvasPos = currentCanvasPos;
          }
          if (penMaskPreviewValid_) {
            renderer_->drawThickLineLocal(
                previousCanvasPos, penMaskPreviewCanvasPos_, 3.4f,
                FloatColor{pendingLineColor.r(), pendingLineColor.g(),
                           pendingLineColor.b(), 0.45f});
          }
        }

        if (pendingMaskCreation_ &&

            pendingMaskLayerId_ == selectedLayer->id() &&

            pendingMaskPath_.vertexCount() > 0) {

          const MaskPath &path = pendingMaskPath_;

          const int vertexCount = path.vertexCount();

          const FloatColor pendingLineColor = {0.82f, 0.92f, 1.0f, 0.88f};

          const FloatColor pendingPointShadowColor = {0.0f, 0.0f, 0.0f, 0.36f};

          const FloatColor pendingPointColor = {0.84f, 0.98f, 1.0f, 0.88f};

          const QTransform globalTransform = selectedLayer->getGlobalTransform();

          constexpr float pendingStrokeWidth = 5.8f;



          Detail::float2 lastCanvasPos;

          for (int v = 0; v < vertexCount; ++v) {

            const MaskVertex vertex = path.vertex(v);

            const QPointF canvasPos = globalTransform.map(vertex.position);

            const Detail::float2 currentCanvasPos = {

                static_cast<float>(canvasPos.x()),

                static_cast<float>(canvasPos.y())};

            if (v > 0) {

              const MaskVertex previousVertex = path.vertex(v - 1);

              const QVector<QPointF> samples =

                  maskSegmentPolyline(previousVertex, vertex, 18);

              for (int i = 1; i < samples.size(); ++i) {

                const QPointF a = globalTransform.map(samples[i - 1]);

                const QPointF b = globalTransform.map(samples[i]);

                renderer_->drawThickLineLocal(

                    {static_cast<float>(a.x()), static_cast<float>(a.y())},

                    {static_cast<float>(b.x()), static_cast<float>(b.y())},

                    pendingStrokeWidth, pendingLineColor);

              }

            }

            drawMaskSquareMarker(renderer_.get(), currentCanvasPos, 7.5f,

                                 pendingPointColor, &pendingPointShadowColor,

                                 4.0f);

            lastCanvasPos = currentCanvasPos;

          }

          if (penToolPreviewVisible_ && penMaskPreviewValid_) {

            const FloatColor previewLineShadowColor = {0.0f, 0.0f, 0.0f, 0.20f};

            const FloatColor previewLineColor = {0.50f, 0.95f, 1.0f, 0.58f};

            renderer_->drawThickLineLocal(lastCanvasPos,

                                          penMaskPreviewCanvasPos_, 4.0f,

                                          previewLineShadowColor);

            renderer_->drawThickLineLocal(lastCanvasPos,

                                          penMaskPreviewCanvasPos_, 2.0f,

                                          previewLineColor);

          }

        }



        if (penToolPreviewVisible_ && penMaskPreviewValid_) {

          const bool closingSoon =

              pendingMaskCreation_ &&

              pendingMaskLayerId_ == selectedLayer->id() &&

              pendingMaskPath_.vertexCount() >= 3;

          const FloatColor previewShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};

          const FloatColor previewColor = closingSoon

                                              ? FloatColor{1.0f, 0.88f, 0.46f, 0.95f}

                                              : FloatColor{0.82f, 0.97f, 1.0f, 0.92f};

          drawMaskSquareMarker(renderer_.get(), penMaskPreviewCanvasPos_, 6.0f,

                               previewColor, &previewShadowColor, 4.0f);

        }

        // Keep the active Bezier handle and its tangent relationship explicit
        // while editing.  The handle colors communicate hover/drag state, but
        // the mode itself is otherwise easy to miss when several handles are
        // close together.  This compact viewport HUD mirrors AE's contextual
        // affordance without adding another global event or toolbar state.
        const bool handleInteraction =
            (isDraggingMaskHandle_ && draggingMaskIndex_ >= 0 &&
             draggingPathIndex_ >= 0 && draggingVertexIndex_ >= 0) ||
            (hoveredMaskHandleType_ >= 0 && hoveredMaskIndex_ >= 0 &&
             hoveredPathIndex_ >= 0 && hoveredVertexIndex_ >= 0);
        if (handleInteraction) {
          const int hudMaskIndex = isDraggingMaskHandle_
                                       ? draggingMaskIndex_
                                       : hoveredMaskIndex_;
          const int hudPathIndex = isDraggingMaskHandle_
                                       ? draggingPathIndex_
                                       : hoveredPathIndex_;
          const int hudVertexIndex = isDraggingMaskHandle_
                                         ? draggingVertexIndex_
                                         : hoveredVertexIndex_;
          const MaskEditHandleType hudHandleType = static_cast<MaskEditHandleType>(
              isDraggingMaskHandle_ ? draggingMaskHandleType_
                                    : hoveredMaskHandleType_);

          if (hudMaskIndex >= 0 && hudPathIndex >= 0 && hudVertexIndex >= 0 &&
              hudMaskIndex < selectedLayer->maskCount()) {
            const LayerMask hudMask = selectedLayer->mask(hudMaskIndex);
            if (hudPathIndex < hudMask.maskPathCount()) {
              const MaskPath hudPath = hudMask.maskPath(hudPathIndex);
              if (hudVertexIndex < hudPath.vertexCount() &&
                  hudHandleType != MaskEditHandleType::None) {
                const MaskVertex hudVertex = hudPath.vertex(hudVertexIndex);
                const QPointF handleCanvas =
                    hudHandleType == MaskEditHandleType::FeatherHandle
                        ? maskFeatherHandleCanvasPosition(
                              hudPath, hudVertexIndex, globalTransform,
                              renderer_.get())
                        : globalTransform.map(maskHandlePosition(
                              hudPath, hudVertexIndex, hudHandleType));
                const bool linked =
                    std::abs(hudVertex.inTangent.x() +
                             hudVertex.outTangent.x()) < 0.001 &&
                    std::abs(hudVertex.inTangent.y() +
                             hudVertex.outTangent.y()) < 0.001;
                const QString handleName =

                    hudHandleType == MaskEditHandleType::InTangent

                        ? QStringLiteral("In handle")

                        : hudHandleType == MaskEditHandleType::OutTangent

                              ? QStringLiteral("Out handle")

                              : hudHandleType == MaskEditHandleType::FeatherHandle

                                    ? QStringLiteral("Feather")

                                    : QStringLiteral("Handle");
                const QString relation = linked
                                             ? QStringLiteral("Linked")
                                             : QStringLiteral("Broken");
                const float zoom = std::max(0.001f, renderer_->getZoom());
                const float hudX = static_cast<float>(handleCanvas.x()) +
                                   12.0f / zoom;
                const float hudY = static_cast<float>(handleCanvas.y()) -
                                   38.0f / zoom;
                const float hudWidth = 172.0f / zoom;
                const float hudHeight = 32.0f / zoom;
                const QFont hudFont = fixedWidthFont(8);
                renderer_->drawSolidRect(
                    hudX, hudY, hudWidth, hudHeight,
                    FloatColor{0.03f, 0.04f, 0.06f, 0.82f}, 0.8f);
                renderer_->drawText(
                    QRectF(hudX + 5.0f / zoom, hudY + 2.0f / zoom,
                           hudWidth - 10.0f / zoom, 13.0f / zoom),
                    hudHandleType == MaskEditHandleType::FeatherHandle
                        ? QStringLiteral("Feather %1").arg(
                              hudPath.feather().value, 0, 'f', 1)
                        : QStringLiteral("Bezier %1  •  %2")
                              .arg(handleName, relation),
                    hudFont, FloatColor{0.92f, 0.97f, 1.0f, 0.98f},
                    Qt::AlignLeft | Qt::AlignVCenter);
                const QString hudHint =
                    hudHandleType == MaskEditHandleType::FeatherHandle
                        ? QStringLiteral("Drag: adjust feather")
                        : QStringLiteral("Alt: break  •  Ctrl: reset");
                renderer_->drawText(
                    QRectF(hudX + 5.0f / zoom, hudY + 16.0f / zoom,
                           hudWidth - 10.0f / zoom, 12.0f / zoom),
                    hudHint, hudFont,
                    FloatColor{0.70f, 0.82f, 0.92f, 0.94f},
                    Qt::AlignLeft | Qt::AlignVCenter);
              }
            }
          }
        }

        if (!handleInteraction && hoveredMaskIndex_ >= 0 &&
            hoveredPathIndex_ >= 0 && hoveredMaskIndex_ < selectedLayer->maskCount()) {
          const LayerMask hudMask = selectedLayer->mask(hoveredMaskIndex_);
          if (hoveredPathIndex_ < hudMask.maskPathCount()) {
            const MaskPath hudPath = hudMask.maskPath(hoveredPathIndex_);
            const QPointF anchor = hudPath.vertexCount() > 0
                                       ? globalTransform.map(hudPath.vertex(0).position)
                                       : QPointF(12.0, 12.0);
            const QString modeName =
                hudPath.mode() == MaskMode::Add
                    ? QStringLiteral("Add")
                    : hudPath.mode() == MaskMode::Subtract
                          ? QStringLiteral("Subtract")
                          : hudPath.mode() == MaskMode::Intersect
                                ? QStringLiteral("Intersect")
                                : QStringLiteral("Difference");
            const float zoom = std::max(0.001f, renderer_->getZoom());
            const float hudX = static_cast<float>(anchor.x()) + 12.0f / zoom;
            const float hudY = static_cast<float>(anchor.y()) + 12.0f / zoom;
            const QFont hudFont = fixedWidthFont(8);
            renderer_->drawSolidRect(
                hudX, hudY, 186.0f / zoom, 32.0f / zoom,
                FloatColor{0.03f, 0.04f, 0.06f, 0.74f}, 0.8f);
            renderer_->drawText(
                QRectF(hudX + 5.0f / zoom, hudY + 1.0f / zoom,
                       176.0f / zoom, 16.0f / zoom),
                QStringLiteral("%1  F %2  E %3  O %4%")
                    .arg(modeName)
                    .arg(hudPath.feather().value, 0, 'f', 1)
                    .arg(hudPath.expansion().value, 0, 'f', 1)
                    .arg(hudPath.opacity() * 100.0f, 0, 'f', 0),
                hudFont, FloatColor{0.86f, 0.94f, 1.0f, 0.94f},
                Qt::AlignLeft | Qt::AlignVCenter);
            renderer_->drawText(
                QRectF(hudX + 5.0f / zoom, hudY + 16.0f / zoom,
                       176.0f / zoom, 14.0f / zoom),
                QStringLiteral("Shift: Feather  •  Alt: Expansion"),
                hudFont, FloatColor{0.68f, 0.80f, 0.90f, 0.92f},
                Qt::AlignLeft | Qt::AlignVCenter);
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



    if (renderer_ &&
        (!selectedIds.isEmpty() || !selectedLayerId_.isNil())) {

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

          if (!layer)

            continue;

          if (!selectedIds.isEmpty()) {

            if (!isLayerSelected(selectedIds, layer))

              continue;

          } else if (selectedLayerId_.isNil() ||

                     layer->id() != selectedLayerId_) {

            continue;

          }



          const auto &t3d = layer->transform3D();

          const int motionPathFps = std::max(
              1, static_cast<int>(std::round(comp->frameRate().framerate())));
          auto posTimes =
              motionPathPositionKeyTimes(layer, motionPathFps);

          if (posTimes.empty()) {

            motionPathCache_.valid = false;

            continue;

          }



          const int keyMinFrame = static_cast<int>(posTimes.front().value());

          const int keyMaxFrame = static_cast<int>(posTimes.back().value());

          int minFrame = keyMinFrame;

          int maxFrame = keyMaxFrame;

          const int currentFrameNum = currentFrame.framePosition();

          const bool hasPathSegment = posTimes.size() >= 2;



          if (minFrame > maxFrame)

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



            if (hasPathSegment && minFrame <= maxFrame) {

              const int sampleStep =
                  motionPathAdaptiveSampleStep(minFrame, maxFrame, zoom);
              for (int f = minFrame;; f += sampleStep) {
                if (f > maxFrame) f = maxFrame;

                ArtifactCore::RationalTime t(f, rate);

                QTransform gTrans = layer->getGlobalTransformAt(f);

                float ax = t3d.anchorXAt(t);

                float ay = t3d.anchorYAt(t);

                QPointF wPos = gTrans.map(QPointF(ax, ay));

                motionPathCache_.pathPoints.push_back(

                    {f, (float)wPos.x(), (float)wPos.y()});

                if (f == maxFrame) break;

              }

            }



            for (const auto &kfTime : posTimes) {

              int f = static_cast<int>(kfTime.value());

              if (f < keyMinFrame || f > keyMaxFrame)

                continue;

              QTransform gTrans = layer->getGlobalTransformAt(f);

              float ax = t3d.anchorXAt(kfTime);

              float ay = t3d.anchorYAt(kfTime);

              QPointF wPos = gTrans.map(QPointF(ax, ay));

              const int interp =
                  motionPathPositionInterpolation(layer, kfTime);

              MotionPathCacheEntry::Pt pt;

              pt.frame = f;

              pt.x = static_cast<float>(wPos.x());

              pt.y = static_cast<float>(wPos.y());

              pt.interpolation = interp;

              motionPathCache_.keyPoints.push_back(pt);

            }

            motionPathCache_.valid = true;

          }



          // Historical bounds must use the four transformed corners. An
          // axis-aligned QRect loses the layer's rotation and makes the past
          // scale look identical at several keyframes.
          if (layer->id() == selectedLayerId_) {
            for (const auto frame : historicalBoundsFrames(
                     layer, currentFrameNum, motionPathFps)) {
              const QRectF bounds = layer->localBounds();
              const QTransform transform = layer->getGlobalTransformAt(frame);
              const QPointF corners[4] = {
                  transform.map(bounds.topLeft()),
                  transform.map(bounds.topRight()),
                  transform.map(bounds.bottomRight()),
                  transform.map(bounds.bottomLeft())};
              const bool active = selectedMotionPathFrames_.contains(frame) ||
                  (historicalScaleDragging_ && historicalScaleFrame_ == frame);
              const FloatColor color =
                  active ? FloatColor{1.0f, 0.72f, 0.25f, 0.95f}
                         : FloatColor{0.58f, 0.65f, 0.73f, 0.55f};
              for (int corner = 0; corner < 4; ++corner) {
                const QPointF &a = corners[corner];
                const QPointF &b = corners[(corner + 1) % 4];
                renderer_->drawDashedLineLocal(
                    {static_cast<float>(a.x()), static_cast<float>(a.y())},
                    {static_cast<float>(b.x()), static_cast<float>(b.y())},
                    std::max(1.0f, invZoom), 2.0f * invZoom, 4.0f * invZoom, color);
                renderer_->drawPoint(static_cast<float>(a.x()),
                                     static_cast<float>(a.y()),
                                     4.0f * invZoom, color);
              }
            }
          }

          // Render from cache (no getGlobalTransformAt() calls on a hit).

          if (!motionPathCache_.pathPoints.empty()) {

            Detail::float2 lastPos;

            bool hasLastPos = false;
            float previousDistance = 0.0f;

            for (const auto &pt : motionPathCache_.pathPoints) {

              Detail::float2 currentPos(pt.x, pt.y);

              if (hasLastPos) {
                previousDistance = std::hypot(currentPos.x - lastPos.x,
                                              currentPos.y - lastPos.y);

                drawTaggedSolidLine(renderer_.get(),

                                    {lastPos.x, lastPos.y},

                                    {currentPos.x, currentPos.y}, pathColor,

                                    lineThickness, true);

              }

              const float speedRadius = std::clamp(
                  dotRadius * (0.45f + previousDistance * 0.015f),
                  dotRadius * 0.45f, dotRadius * 1.25f);
              const FloatColor dotColor =
                  pt.frame < currentFrameNum
                      ? FloatColor{0.98f, 0.48f, 0.72f, 0.78f}
                      : FloatColor{0.42f, 0.72f, 1.0f, 0.78f};
              renderer_->drawPoint(pt.x, pt.y, speedRadius,
                                   dotColor);

              lastPos = currentPos;

              hasLastPos = true;

            }

            // Evaluate the current marker directly instead of snapping it to
            // the nearest adaptive sample.
            if (currentFrameNum >= minFrame && currentFrameNum <= maxFrame) {
              const ArtifactCore::RationalTime currentTime(
                  static_cast<int64_t>(currentFrameNum), rate);
              const QTransform currentTransform =
                  layer->getGlobalTransformAt(currentFrameNum);
              const QPointF currentPosition = currentTransform.map(
                  QPointF(t3d.anchorXAt(currentTime),
                          t3d.anchorYAt(currentTime)));
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

            const FloatColor keyColor =

                motionPathInterpolationColor(pt.interpolation, isCurrent);

            const float outerRadius =

                isHovered ? dotRadius * 3.4f : dotRadius * 2.8f;

            const float innerRadius =

                isHovered ? dotRadius * 2.0f : dotRadius * 1.65f;

            const FloatColor ringColor = isHovered

                                              ? FloatColor{1.0f, 1.0f, 1.0f, 0.95f}

                                              : FloatColor{0.96f, 0.96f, 1.0f, 0.92f};

            renderer_->drawPoint(pt.x, pt.y, outerRadius, ringColor);

            renderer_->drawPoint(pt.x, pt.y, innerRadius, keyColor);

          }

        }

      }

      {

        ArtifactCore::ProfileScope _profBBox(

            "BoundingBox", ArtifactCore::ProfileCategory::Render);

        // Selection frames are layer controls, not composition guides. Keep
        // them visible whenever gizmos are enabled even if guides are hidden.
        if (showGizmoOverlay_) {

          const FloatColor primaryColor{1.0f, 0.72f, 0.22f, 1.0f};

          const FloatColor secondaryColor{0.28f, 0.74f, 1.0f, 0.85f};

          for (const auto &layer : layersForOverlay) {

            if (!isLayerEffectivelyVisible(layer) ||

                !layer->isActiveAt(currentFrame)) {

              continue;

            }

            if (dynamic_cast<ArtifactVideoLayer *>(layer.get())) {

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

            const bool showBounds = hostSnapshot.showBoundsLines;

            drawTaggedSolidLine(renderer_.get(), tl, tr, color, thickness,

                                showBounds);

            drawTaggedSolidLine(renderer_.get(), tr, br, color, thickness,

                                showBounds);

            drawTaggedSolidLine(renderer_.get(), br, bl, color, thickness,

                                showBounds);

            drawTaggedSolidLine(renderer_.get(), bl, tl, color, thickness,

                                showBounds);

          }

        }

      } // BoundingBox scope

    } */



    markVideoPreviewSetupStage(12);
    RenderPassContext overlayContext{renderer_.get(), renderFrameCounter_};

    RenderPassResources overlayResources;

    FunctionalRenderPass overlayPass(

        FrameRenderPassKind::Overlay, QStringLiteral("Overlay"),

        [](RenderPassResources&) { return true; },

        [&](RenderPassContext&, RenderPassResources&) {

          if (!compositeFinalizedThisFrame_ &&
              compositionViewLog().isDebugEnabled()) {
            qCDebug(compositionViewLog)
                << "[CompositionView] overlay ran before composite finalize";
          }

          drawViewportCanvasOverlay(hostSnapshot, cw, ch);

          drawViewportInteractionOverlay(hostSnapshot, currentFrame, cw, ch);

          drawViewportGuideOverlay(comp, cw, ch);

          drawViewportOverlayPass(hostSnapshot, comp, activeCamera, layers,

                                  selectedIds, currentFrame, cw, ch,

                                  has3DCamera, cameraViewMatrix,

                                  cameraProjMatrix);

          return true;

        });

    framePassesSucceeded = RenderPassExecutor::runWithRenderGraph(
        overlayPass, overlayContext, overlayResources) && framePassesSucceeded;

    markVideoPreviewSetupStage(13);
    overlayMs = markPhaseMs();
    addProfileBoundary(profileOverlayBoundaryNs);



    flushCountThisFrame = renderer_->flushCount() - flushCountAtFrameStart;
    const qint64 flushContextTimeUsThisFrame =
        renderer_->flushContextTimeUs() - flushContextTimeUsAtFrameStart;
    flushMs = std::max<qint64>(
        0, static_cast<qint64>(std::llround(
               static_cast<double>(flushContextTimeUsThisFrame) / 1000.0)));

  renderCrashTrace("render-cost-end-begin", renderFrameCounter_);

  renderCostGuard.finish();

  renderCrashTrace("render-cost-end", renderFrameCounter_);



    if (!lastVideoDebug_.isEmpty() &&

        lastVideoDebug_ != lastEmittedVideoDebug_) {

      lastEmittedVideoDebug_ = lastVideoDebug_;

      qDebug() << lastVideoDebug_;

      owner->videoDebugMessage(lastVideoDebug_);

    }



    // renderer_->flushAndWait(); // 毎フレーム同期を削除し、性能改善を試む

    bool presentationSucceeded = false;
    {

      ArtifactCore::ProfileScope _profPresent(

          "Present", ArtifactCore::ProfileCategory::Render);

      renderCrashTrace("render-present-begin", renderFrameCounter_);

      PresentStageResult presentResult;

      RenderPassContext presentContext{renderer_.get(), renderFrameCounter_};

      RenderPassResources presentResources;

      FunctionalRenderPass presentPass(

          FrameRenderPassKind::Present, QStringLiteral("Present"),

          [&](RenderPassResources&) { return !partialGpuRecomposeFailed; },

          [&](RenderPassContext&, RenderPassResources&) {

            presentResult = presentAndUpdateVideoLayers(

                layers, currentFrame, useRamPreviewFallback,

                playbackPreviewState, ramPreviewFallbackReason);

            return !presentResult.presentedStatus.contains(

                QStringLiteral("fail"), Qt::CaseInsensitive);

          });

      presentationSucceeded = RenderPassExecutor::runWithRenderGraph(
          presentPass, presentContext, presentResources) &&
          presentResult.presentedStatus == QStringLiteral("ok");

      if (!presentResult.presentedVideoDebug.isEmpty() &&

          presentResult.presentedVideoDebug != lastEmittedVideoDebug_) {

        lastVideoDebug_ = presentResult.presentedVideoDebug;

        lastEmittedVideoDebug_ = presentResult.presentedVideoDebug;

        qDebug() << presentResult.presentedVideoDebug;

        owner->videoDebugMessage(presentResult.presentedVideoDebug);

      }

      if (pipelineEnabled) {

        previewRenderSlot.state = PreviewRenderPipelineSlot::State::Retirable;

      }

      if (showOnionSkin_ && !hasCalculatedOnionSkinSource(comp)) {
         queueOnionSkinCapture(owner, comp, currentFrame,
                               lastCanvasWidth_, lastCanvasHeight_);
      }

      renderCrashTrace("render-present-end", renderFrameCounter_);

    }

  // Restore the full-frame viewport after the Interactive Render Region pass.
  if (irrScissorApplied) {
    renderer_->setViewportRect(hostWidth_, hostHeight_);
  }

    if (auto *playback = ArtifactPlaybackService::instance();
        playback && snapshotMatchesComposition) {

      const auto& previewSummary = hostSnapshot.ramPreviewSummary;

      const bool shouldCaptureRamPreview =
          hostSnapshot.ramPreviewEnabled && playbackPreviewStateValid &&

          playbackSameComposition &&

          !hostSnapshot.viewportInteracting && !frameOutOfRange &&

          !playbackPreviewState.ready && playbackPreviewPendingBuild;

      if (shouldCaptureRamPreview && !videoFramesReadyForRamPreview) {
        playback->deferRamPreviewBuildFrame(
            framePos, QStringLiteral("video-frame-pending"));
      }
      if (shouldCaptureRamPreview && videoFramesReadyForRamPreview) {

        renderCrashTrace("render-async-readback-begin", renderFrameCounter_);

        // Asynchronous readback: GPU copy + fence wait runs on a worker thread,

        // avoiding a full GPU pipeline stall on the render thread.

        const auto weakPlayback = QPointer<ArtifactPlaybackService>(playback);

        const PreviewFrameRequest previewRequest{

            framePos,

            // Paused warmup follows the queue head. During playback a
            // deferred earlier frame must not block capturing later exact
            // frames that are still pending in this same build generation.
            playbackPlaying ? framePos : previewSummary.buildQueueNextFrame,

            comp->id().toString(),

            previewDownsample_,

            effectivePreviewDownsample,

            pipelineEnabled,

            previewSummary.currentPriorityReason,

            previewSummary.buildQueueGeneration, playbackPlaying};

        const bool previewRequestMatchesRenderedFrame =

            previewRequest.buildTargetFrame == previewRequest.framePos;

        if (!previewRequestMatchesRenderedFrame) {

          renderCrashTrace("render-async-readback-skip-frame-mismatch",

                           renderFrameCounter_);

        } else if (previewRequest.pipelineEnabled && ramPreviewReadbackSRV) {

          const bool accepted = renderer_->readbackTextureViewToBufferAsync(

              ramPreviewReadbackSRV,
              lastPresentedReadbackChannelOrder_,

              [weakPlayback, previewRequest, weakOwner = QPointer<CompositionRenderController>(owner)](ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer> capturedFrame) {

                publishPreviewFrameRequestResult(weakPlayback, previewRequest, weakOwner,

                                                 std::move(capturedFrame));

              }, !playbackPlaying);

          if (!accepted) {
            playback->deferRamPreviewBuildFrame(
                previewRequest.framePos,
                QStringLiteral("readback-ring-busy"));
          }

        } else {

          const bool accepted = renderer_->readbackTextureViewToBufferAsync(
              nullptr, ArtifactCore::SurfaceChannelOrder::RGBA,

              [weakPlayback, previewRequest, weakOwner = QPointer<CompositionRenderController>(owner)](ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer> capturedFrame) {

                publishPreviewFrameRequestResult(weakPlayback, previewRequest, weakOwner,

                                                 std::move(capturedFrame));

              }, !playbackPlaying);

          if (!accepted) {
            playback->deferRamPreviewBuildFrame(
                previewRequest.framePos,
                QStringLiteral("readback-ring-busy"));
          }

        }

        renderCrashTrace("render-async-readback-end", renderFrameCounter_);

      }

    }

    presentMs = markPhaseMs();
    addProfileBoundary(profilePresentBoundaryNs);
    markVideoPreviewSetupStage(9);

    if (profileThisVideoPreviewSetup) {
      const auto stageMs = [&](const std::size_t begin,
                               const std::size_t end) -> double {
        return static_cast<double>(videoPreviewSetupMarksNs[end] -
                                   videoPreviewSetupMarksNs[begin]) /
               1000000.0;
      };
      const qint64 profileBoundaryTotalNs =
          profileBaseBoundaryNs + profileSurfaceBoundaryNs +
          profileMaskBoundaryNs + profileCompositeBoundaryNs +
          profileDirectLayerBoundaryNs + profilePostBoundaryNs +
          profileOverlayBoundaryNs + profilePresentBoundaryNs;
      std::fprintf(
          stderr,
          "[VideoPreviewSetupProfile] frame=%llu layers=%zu "
          "preSceneMs=%.2f sceneLightsMs=%.2f envShadowMs=%.2f "
          "cameraRangeMs=%.2f playbackStateMs=%.2f damageMs=%.2f "
          "pipelineMs=%.2f graphSetupMs=%.2f renderPresentMs=%.2f\n",
          static_cast<unsigned long long>(renderFrameCounter_), layers.size(),
          stageMs(0, 1), stageMs(1, 2), stageMs(2, 3), stageMs(3, 4),
          stageMs(4, 5), stageMs(5, 6), stageMs(6, 7), stageMs(7, 8),
          stageMs(8, 9));
      std::fprintf(
          stderr,
          "[VideoPreviewRenderPassProfile] frame=%llu "
          "baseMs=%lld baseSetupMs=%.3f backgroundSeedMs=%.3f "
          "depthClearMs=%.3f accumulationClearMs=%.3f "
          "partialRegionClearMs=%.3f fullTargetClearMs=%.3f "
          "computeUnbindMs=%.3f "
          "baseBoundaryMs=%.3f surfaceBoundaryMs=%.3f "
          "maskBoundaryMs=%.3f compositeBoundaryMs=%.3f "
          "directLayerBoundaryMs=%.3f postBoundaryMs=%.3f "
          "giBlockMs=%.3f giDispatchMs=%.3f giApplyMs=%.3f fxaaMs=%.3f "
          "resolveExecuteMs=%.3f finalizeMs=%.3f "
          "overlayBoundaryMs=%.3f presentBoundaryMs=%.3f "
          "boundarySumMs=%.3f flushMs=%lld\n",
          static_cast<unsigned long long>(renderFrameCounter_),
          static_cast<long long>(basePassMs),
          static_cast<double>(baseSetupNs) / 1000000.0,
          static_cast<double>(backgroundSeedNs) / 1000000.0,
          static_cast<double>(depthClearNs) / 1000000.0,
          static_cast<double>(accumulationClearNs) / 1000000.0,
          static_cast<double>(partialRegionClearNs) / 1000000.0,
          static_cast<double>(fullTargetClearNs) / 1000000.0,
          static_cast<double>(computeUnbindNs) / 1000000.0,
          static_cast<double>(profileBaseBoundaryNs) / 1000000.0,
          static_cast<double>(profileSurfaceBoundaryNs) / 1000000.0,
          static_cast<double>(profileMaskBoundaryNs) / 1000000.0,
          static_cast<double>(profileCompositeBoundaryNs) / 1000000.0,
          static_cast<double>(profileDirectLayerBoundaryNs) / 1000000.0,
          static_cast<double>(profilePostBoundaryNs) / 1000000.0,
          static_cast<double>(profileGiBlockNs) / 1000000.0,
          static_cast<double>(profileGiDispatchNs) / 1000000.0,
          static_cast<double>(profileGiApplyNs) / 1000000.0,
          static_cast<double>(profileFxaaNs) / 1000000.0,
          static_cast<double>(profileResolveExecuteNs) / 1000000.0,
          static_cast<double>(profileFinalizeNs) / 1000000.0,
          static_cast<double>(profileOverlayBoundaryNs) / 1000000.0,
          static_cast<double>(profilePresentBoundaryNs) / 1000000.0,
          static_cast<double>(profileBoundaryTotalNs) / 1000000.0,
          static_cast<long long>(flushMs));
      if (useRamPreviewFallback) {
        std::fprintf(stderr,
            "[VideoPreviewRamPassProfile] frame=%llu ramPassMs=%.3f betweenMs=%.3f overlayMs=%.3f tailPresentMs=%.3f\n",
            static_cast<unsigned long long>(renderFrameCounter_),
            stageMs(10, 11), stageMs(11, 12), stageMs(12, 13), stageMs(13, 9));
        std::fprintf(stderr,
            "[VideoPreviewRamFinalizeProfile] frame=%llu beforeFinalizeMs=%.3f finalizeMs=%.3f selectionMs=%.3f afterSelectionMs=%.3f\n",
            static_cast<unsigned long long>(renderFrameCounter_),
            stageMs(11, 14), stageMs(14, 15), stageMs(15, 16), stageMs(16, 12));
      }
    }

    lastSubmit2DMs_ = latestTimerMs("Submit2D");

    // Consume damage only after every frame pass and presentation succeed.
    // A committed partial pass consumes the requested canvas region; a full
    // redraw consumes all damage. Failed and skipped presentations retry it.
    auto& presentedRenderSlot =
        previewRenderPipelineSlots_[activePreviewRenderPipelineSlot_];
    auto& presentedSlotDamage = presentedRenderSlot.damageTracker;
    if (framePassesSucceeded && presentationSucceeded &&
        presentedSlotDamage.hasDirtyRegions()) {

      lastConsumedDamageLayerCount_ = presentedSlotDamage.dirtyLayerCount();

      lastConsumedDamageWasFullRedraw_ =
          presentedSlotDamage.combinedNeedsFullRedraw();

      const RenderROI damageRoi = presentedSlotDamage.combinedDirtyROI();

      lastConsumedDamageRect_ = damageRoi.rect;

      lastConsumedDamageTilePlan_ = {};

      if (!lastConsumedDamageWasFullRedraw_ && !damageRoi.isEmpty()) {

        const TileGrid grid(std::max(1, static_cast<int>(std::ceil(cw))),
                            std::max(1, static_cast<int>(std::ceil(ch))), 256);

        lastConsumedDamageTileCount_ = presentedSlotDamage.dirtyTileCount(grid);

        if (gpuPartialRecomposeStarted &&
            !partialGpuRecomposeFailed) {
          lastConsumedDamageTilePlan_ = gpuPartialDamageTilePlan;
        } else {
          lastConsumedDamageTilePlan_ =
              presentedSlotDamage.makeBoundedDirtyTilePlan(
                  grid, RenderROI(visibleCanvasRect), 8,
                  QString(), presentedRenderSlot.damageTileCursor);
        }

      } else {

        lastConsumedDamageTileCount_ = 0;

        lastConsumedDamageTilePlan_.requiresFullRedraw =
            lastConsumedDamageWasFullRedraw_;

      }

    }

    if (framePassesSucceeded && presentationSucceeded) {
      if (pipelineEnabled) {
        presentedRenderSlot.retainedCompositeValid = true;
        presentedRenderSlot.retainedAccumTexture =
            presentedRenderSlot.pipeline.accumSRV()
                ? presentedRenderSlot.pipeline.accumSRV()->GetTexture()
                : nullptr;
        if (gpuPartialRecomposeStarted && !partialGpuRecomposeFailed &&
            !gpuPartialRecomposeRequestedCanvasRect.isEmpty()) {
          presentedSlotDamage.consumeRegion(
              gpuPartialRecomposeRequestedCanvasRect);
          presentedRenderSlot.damageTileCursor =
              gpuPartialDamageTilePlan.nextTileOrdinal;
          if (!presentedSlotDamage.hasDirtyRegions()) {
            presentedRenderSlot.damageTileCursor = 0;
          }
        } else {
          presentedSlotDamage.clearAll();
          presentedRenderSlot.damageTileCursor = 0;
        }
        const bool anySlotDamagePending = std::any_of(
            previewRenderPipelineSlots_.cbegin(),
            previewRenderPipelineSlots_.cend(),
            [](const PreviewRenderPipelineSlot& slot) {
              return slot.damageTracker.hasDirtyRegions();
            });
        if (!anySlotDamagePending) {
          damageTracker_.clearAll();
        } else if (tgfxPartialRecomposeEnabled_ && owner) {
          const TileGrid remainingDamageGrid(
              std::max(1, static_cast<int>(std::ceil(cw))),
              std::max(1, static_cast<int>(std::ceil(ch))), 256);
          const bool anyVisibleDamagePending = std::any_of(
              previewRenderPipelineSlots_.cbegin(),
              previewRenderPipelineSlots_.cend(),
              [&](const PreviewRenderPipelineSlot& slot) {
                return !slot.damageTracker
                            .makeBoundedDirtyTilePlan(
                                remainingDamageGrid,
                                RenderROI(visibleCanvasRect), 8, QString(),
                                slot.damageTileCursor)
                            .empty();
              });
          if (anyVisibleDamagePending) {
            // Continue bounded refinement while visible damage remains in
            // either retained preview slot.
            damageContinuationPending_.store(true,
                                             std::memory_order_release);
            owner->markRenderDirty();
          }
        }
      } else {
        for (auto& slot : previewRenderPipelineSlots_) {
          slot.retainedCompositeValid = false;
          slot.damageTracker.clearAll();
          slot.damageTileCursor = 0;
        }
        damageTracker_.clearAll();
      }
      fullRedrawPending_ = false;
      failedFrameRetryIssued_.store(false, std::memory_order_release);
    } else {
      markAllPreviewSlotsFullRedraw();
      damageTracker_.markFullRedraw();
      fullRedrawPending_ = true;
      lastRenderKeyState_ = {};
      scheduleFailedFrameRetry();
    }

    // P0-3d: when an Interactive Render Region is active, drive one
    // extra partial-region pass at the requested RenderQuality. The pass
    // re-uses the IRR scissor applied earlier in this frame; this keeps
    // the partial render inside the rectangle without disturbing the
    // surrounding framebuffer contents.
    if (hostSnapshot.interactiveRenderRegionActive && !hostSnapshot.interactiveRenderRegionRect.isEmpty()) {
      const float scale = hostSnapshot.interactiveRenderRegionResolutionScale;
      const RenderQuality quality = scale <= 0.34f
          ? RenderQuality::Draft
          : (scale <= 0.67f ? RenderQuality::Preview : RenderQuality::Final);
      renderPartialRegion(owner, quality,
                          RenderROI(hostSnapshot.interactiveRenderRegionRect));
    }

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

    lastSurfacePassMs_ = std::max<qint64>(0, surfacePassMs - basePassMs);

    lastMaskPassMs_ = std::max<qint64>(0, maskPassMs - surfacePassMs);

    lastCompositePassMs_ = std::max<qint64>(0, compositePassMs - maskPassMs);

    lastPostPassMs_ = std::max<qint64>(0, postPassMs - compositePassMs);

    lastOverlayMs_ = overlayMs;

    lastFlushMs_ = flushMs;

    lastSubmit2DMs_ = std::max<qint64>(lastSubmit2DMs_, 0);

    lastPresentMs_ = presentMs;

    const qint64 layerPassMs = std::max<qint64>(0, postPassMs - basePassMs);

    if (interactionPerfEnabled_ && hostSnapshot.viewportInteracting && interactionPerfAction_ != 0 &&
        interactionPerfFiles_ < 64) {
      if (!interactionPerfTimer_.isValid()) interactionPerfTimer_.start();
      if (!interactionPerfWrite_.isRunning() && interactionPerf_.count < interactionPerf_.samples.size()) {
        interactionPerf_.samples[interactionPerf_.count++] = {
            interactionPerfTimer_.nsecsElapsed() / 1000, frameTimer.nsecsElapsed() / 1000,
            setupMs, basePassMs, layerPassMs, overlayMs, flushMs, presentMs,
            static_cast<quint64>(renderer_->frameCostStats().drawCalls),
            hostWidth_, hostHeight_, zoom, panX, panY,
            effectivePreviewDownsample, interactionPerfAction_};
      } else {
        ++interactionPerf_.dropped;
      }
    }

    if (renderFrameCounter_ <= 5 ||

        renderer_->lastPresentStatus() != QStringLiteral("ok")) {

      quintptr hostWinId = 0;

      QSize hostSize;

      qreal hostDpr = 1.0;

      if (hostSnapshot.windowIdCaptured) {
        hostWinId = hostSnapshot.windowId;
      }
      hostSize = hostSnapshot.logicalSize;
      hostDpr = hostSnapshot.devicePixelRatio;
      const bool hostVisible = hostSnapshot.hasHost && hostSnapshot.visible;

      qInfo() << "[CompositionView][PresentProbe]"

              << "frame=" << renderFrameCounter_

              << "status=" << renderer_->lastPresentStatus()

              << "attempts=" << renderer_->presentAttemptCount()

              << "ok=" << renderer_->presentSuccessCount()

              << "fail=" << renderer_->presentFailureCount()

              << "skip=" << renderer_->presentSkippedCount()

              << "hostVisible=" << hostVisible

              << "hostWinId=" << hostWinId

              << "hostSize=" << hostSize

              << "hostDpr=" << hostDpr

              << "hasSwapChain=" << renderer_->hasSwapChain();

    }

    // The three summary strings below are only consumed by the opt-in
    // diagnostics surfaces (frameDebugSnapshot, the debug render harness and
    // the FrameDebug view).  Rebuilding them unconditionally cost ~60 QString
    // allocations per frame, which is measurable in the viewport hot path, so
    // keep the whole block behind the same opt-in switch the trace and cost
    // capture already use.  The strings are cleared otherwise so a stale frame
    // can never be presented as if it described the current one.
    if (captureRenderDiagnostics) {

    const auto textureCacheStats = gpuTextureCacheManager_

                                       ? gpuTextureCacheManager_->stats()

                                       : GPUTextureCacheStats{};

    const QString mfrpSlotsSummary = previewRenderPipelineSlotsSummary();

    const QString mfrpPressureSummary = previewRenderPipelinePressureSummary();

    const QString mfrpPolicySummary = previewRenderPipelineAcquirePolicySummary();

    const qulonglong mfrpEstimatedBytes =

        static_cast<qulonglong>(previewRenderPipelineEstimatedBytes());

    lastRenderPathSummary_ =

        QStringLiteral(

            "path=%1 gpuBlendEnabled=%2 gpuBlendReady=%3 layersTotal=%4 "

             "layersDrawn=%5 surfaceUploadLayers=%6 cpuRasterLayers=%7 "

             "frameOutOfRange=%8 previewDownsample=%9 effectiveDownsample=%10 "

             "viewportInteracting=%11 cacheEntries=%12 cacheBytes=%13 "

             "presentStatus=%14 presentOk=%15 presentFail=%16 presentSkip=%17 "

             "ramPreviewFallback=%18 ramPreviewFallbackReason=%19 "

             "mfrpEligible=%20 mfrpReason=%21 mfrpSlot=%22 mfrpSlotState=%23 "

             "mfrpDepthReady=%24 mfrpLastSubmitFrame=%25 "

             "mfrpSlotAcquire=%26 mfrpEstimatedBytes=%27 mfrpSlots={%28} "

             "mfrpPressure={%29} mfrpPolicy={%30} rayTracing={%31}")

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

            .arg(hostSnapshot.viewportInteracting)

            .arg(textureCacheStats.entryCount)

            .arg(static_cast<qulonglong>(textureCacheStats.memoryBytes))

            .arg(renderer_->lastPresentStatus())

            .arg(static_cast<qulonglong>(renderer_->presentSuccessCount()))

            .arg(static_cast<qulonglong>(renderer_->presentFailureCount()))

            .arg(static_cast<qulonglong>(renderer_->presentSkippedCount()))

            .arg(useRamPreviewFallback ? 1 : 0)

            .arg(ramPreviewFallbackReason)

            .arg(multiFrameReason == QStringLiteral("eligible") ? 1 : 0)

            .arg(multiFrameReason)

            .arg(activePreviewRenderPipelineSlot_)

            .arg(previewRenderPipelineSlotStateText(previewRenderSlot))

            .arg(previewRenderSlot.depthTargetView ? 1 : 0)

            .arg(previewRenderSlot.lastSubmittedFrame)

            .arg(lastPreviewRenderPipelineSlotAcquireReason_)

            .arg(mfrpEstimatedBytes)

            .arg(mfrpSlotsSummary)

            .arg(mfrpPressureSummary)

            .arg(mfrpPolicySummary)

            .arg(renderer_->rayTracingDebugState());

    lastRenderPathSummary_ +=
        QStringLiteral(" gpuBlendInitState=%1 gpuBlendInitAttempts=%2 transparentOutput=%3")
            .arg(blendPipelineInitState_)
            .arg(blendPipelineInitAttemptCount_)
            .arg(currentBgColor.a() < 0.999f ? 1 : 0);

    const QString visibilityState =

        frameOutOfRange

            ? QStringLiteral("frameOutOfRange")

            : (compositedLayerCount > 0

                   ? QStringLiteral("drawn")

                   : (layers.isEmpty() ? QStringLiteral("noLayers")

                                       : QStringLiteral("allSkipped")));

    lastCompositionVisibilitySummary_ =

        QStringLiteral(

            "phase=composition-visibility-v1 state=%1 path=%2 layersTotal=%3 "

            "drawn=%4 composited=%5 skippedInvisible=%6 skippedSolo=%7 "

            "skippedInactive=%8 skippedRoi=%9 skippedLod=%10 opacityZero=%11 "

            "frameOutOfRange=%12 cpuRasterizerUpload=%13 "

            "forceContinuousRedraw=%14 viewportInteracting=%15 "

            "firstLayerSeed=%16 notes=%17")

            .arg(visibilityState,

                 pipelineEnabled ? QStringLiteral("gpu-blend")

                                 : QStringLiteral("fallback"))

            .arg(layers.size())

            .arg(drawnLayerCount)

            .arg(compositedLayerCount)

            .arg(skipInvisibleCount)

            .arg(skipSoloCount)

            .arg(skipInactiveCount)

            .arg(skipRoiCount)

            .arg(skipLodCount)

            .arg(opacityZeroCount)

            .arg(frameOutOfRange ? 1 : 0)

            .arg(hasCpuRasterizerUpload ? 1 : 0)

            .arg(forceContinuousRedraw ? 1 : 0)

            .arg(hostSnapshot.viewportInteracting ? 1 : 0)

            .arg(layerToFloatConvertCount)

            .arg(hasCpuRasterizerUpload

                     ? QStringLiteral("gpuBlendUsesCpuRasterizerUpload")

                     : QStringLiteral("none"));

    // The layer visibility report below is a qWarning() that reports a real
    // per-composition visibility problem.  It is already rate limited by
    // baseInvalidationSerial_, so it costs nothing on a steady frame, and it
    // must stay outside the diagnostics gate to keep reporting on a normal run.
    if (!layers.empty() &&
        lastLayerVisibilityReportSerial_ != baseInvalidationSerial_) {
      lastLayerVisibilityReportSerial_ = baseInvalidationSerial_;

      QStringList layerReports;
      layerReports.reserve(static_cast<qsizetype>(layers.size()));
      for (const auto &layer : layers) {
        if (!layer) {
          layerReports << QStringLiteral("<null-layer>: rejected=null");
          continue;
        }

        const QRectF localBounds = layer->localBounds();
        const QRectF transformedBounds = layer->transformedBoundingBox();
        const QRectF visibleBounds = transformedBounds.intersected(roiRect);
        QString decision = QStringLiteral("submitted");
        if (!isLayerEffectivelyVisible(layer)) {
          decision = QStringLiteral("rejected=invisible");
        } else if (hasSoloLayer && !layer->isSolo()) {
          decision = QStringLiteral("rejected=non-solo");
        } else if (!layer->isActiveAt(currentFrame)) {
          decision = QStringLiteral("rejected=inactive-at-frame");
        } else if (frameOutOfRange) {
          decision = QStringLiteral("rejected=composition-frame-out-of-range");
        } else if (visibleBounds.isEmpty()) {
          decision = QStringLiteral("rejected=outside-roi");
        } else if (layer->opacity() <= 0.0f) {
          decision = QStringLiteral("rejected=zero-opacity");
        }

        QString sourceState = QStringLiteral("source=n/a");
        if (const auto *image =
                dynamic_cast<const ArtifactImageLayer *>(layer.get())) {
          sourceState = QStringLiteral("imageBuffer=%1 sourcePath=%2 sequence=%3")
                            .arg(image->hasCurrentFrameBuffer() ? 1 : 0)
                            .arg(image->sourcePath().isEmpty() ? 0 : 1)
                            .arg(image->isImageSequence() ? 1 : 0);
        } else if (dynamic_cast<const ArtifactSolid2DLayer *>(layer.get()) ||
                   dynamic_cast<const ArtifactSolidImageLayer *>(layer.get())) {
          sourceState = QStringLiteral("source=direct-solid");
        }

        layerReports <<
            QStringLiteral(
                "name='%1' id=%2 class=%3 decision=%4 visible=%5 solo=%6 "
                "active=%7 opacity=%8 local=[%9,%10 %11x%12] "
                "world=[%13,%14 %15x%16] roiHit=%17 parentGroup=%18 %19")
                .arg(layer->layerName(), layer->id().toString(),
                     layer->className().toQString(), decision)
                .arg(isLayerEffectivelyVisible(layer) ? 1 : 0)
                .arg(layer->isSolo() ? 1 : 0)
                .arg(layer->isActiveAt(currentFrame) ? 1 : 0)
                .arg(layer->opacity(), 0, 'f', 3)
                .arg(localBounds.x(), 0, 'f', 2)
                .arg(localBounds.y(), 0, 'f', 2)
                .arg(localBounds.width(), 0, 'f', 2)
                .arg(localBounds.height(), 0, 'f', 2)
                .arg(transformedBounds.x(), 0, 'f', 2)
                .arg(transformedBounds.y(), 0, 'f', 2)
                .arg(transformedBounds.width(), 0, 'f', 2)
                .arg(transformedBounds.height(), 0, 'f', 2)
                .arg(visibleBounds.isEmpty() ? 0 : 1)
                .arg(layer->parentLayer() && layer->parentLayer()->isGroupLayer()
                         ? 1
                         : 0)
                .arg(sourceState);
      }

      qWarning().noquote()
          << QStringLiteral(
                 "[CompositionView][LayerVisibilityReport] comp='%1' id=%2 "
                 "frame=%3 effectiveEnd=%4 path=%5 layers=%6 drawn=%7 "
                 "composited=%8 frameOutOfRange=%9 preflightErrors=%10 "
                 "selectedLayer=%11\n  %12")
                 .arg(comp->settings().compositionName().toQString(),
                      comp->id().toString())
                 .arg(framePos)
                 .arg(effectiveEndFrame)
                 .arg(pipelineEnabled ? QStringLiteral("gpu-blend")
                                      : QStringLiteral("direct"))
                 .arg(layers.size())
                 .arg(drawnLayerCount)
                 .arg(compositedLayerCount)
                 .arg(frameOutOfRange ? 1 : 0)
                 .arg(selectedLayerId_.toString())
                 .arg(layerReports.join(QStringLiteral("\n  ")));
    }

    lastBlendMaskSummary_ =

        QStringLiteral(

            "phase=blend-mask-smoke-v1 path=%1 blendContract=straight-src_over-premul-accum "

            "maskContract=%2 pipelineFormat=RGBA32F layerFormat=RGBA8_sRGB layerComputeFormat=RGBA32F "

            "layersDrawn=%3 nonNormal=%4 maskedLayers=%5 masks=%6 "

            "dispatch=%7 retryNormal=%8 failed=%9 directFallback=%10 "

            "layerToFloatConvert=%11 layerPixels={%12} accumPixels={%13} "

            "notes=%14")

            .arg(pipelineEnabled ? QStringLiteral("gpu-blend")

                                 : QStringLiteral("fallback"))

            .arg(totalMaskCount > 0 ? QStringLiteral("pending")

                                    : QStringLiteral("none"))

            .arg(drawnLayerCount)

            .arg(nonNormalBlendLayerCount)

            .arg(maskedLayerCount)

            .arg(totalMaskCount)

            .arg(blendDispatchCount)

            .arg(blendRetryNormalCount)

            .arg(blendFailureCount)

            .arg(directBlendFallbackCount)

            .arg(layerToFloatConvertCount)

            .arg(lastLayerRtPixelStats_.isEmpty()

                     ? QStringLiteral("state=unavailable")

                     : lastLayerRtPixelStats_)

            .arg(lastAccumRtPixelStats_.isEmpty()

                     ? QStringLiteral("state=unavailable")

                     : lastAccumRtPixelStats_)

            .arg(blendMaskLayerNotes.isEmpty()

                     ? QStringLiteral("none")

                     : blendMaskLayerNotes.join(QStringLiteral("; ")));

    } else {

      lastRenderPathSummary_.clear();

      lastCompositionVisibilitySummary_.clear();

      lastBlendMaskSummary_.clear();

    }

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

            << "viewportInteracting=" << hostSnapshot.viewportInteracting

            << "setupMs=" << setupMs << "basePassMs=" << basePassMs

            << "layerPassMs=" << layerPassMs << "overlayMs=" << overlayMs

            << "flushCount=" << flushCountThisFrame

            << "flushMs=" << flushMs << "submit2DMs=" << lastSubmit2DMs_

            << "presentMs=" << presentMs;

      } else if ((renderFrameCounter_ % 120u) == 0u) {

        qCDebug(compositionViewLog)

            << "[CompositionView][Perf]"

            << "frameMs=" << frameMs << "pipelineEnabled=" << pipelineEnabled

            << "layersTotal=" << layers.size()

            << "layersDrawn=" << drawnLayerCount << "setupMs=" << setupMs

            << "basePassMs=" << basePassMs << "layerPassMs=" << layerPassMs

            << "overlayMs=" << overlayMs << "flushCount=" << flushCountThisFrame

            << "flushMs=" << flushMs

            << "submit2DMs=" << lastSubmit2DMs_ << "presentMs=" << presentMs;

      }

      if (tgfxPartialRecomposeEnabled_ && gpuPartialRecomposeCandidate) {
        qCDebug(compositionViewLog)
            << "[CompositionView][TGFXPartial]"
            << "candidate=" << gpuPartialRecomposeCandidate
            << "started=" << gpuPartialRecomposeStarted
            << "failed=" << partialGpuRecomposeFailed
            << "regionPixels=" << gpuPartialRegionPixels
            << "targetPixels=" << gpuPartialTargetPixels
            << "partialSkippedLayers=" << skipPartialDamageCount
            << "partialCommitted="
            << (gpuPartialRecomposeStarted && !partialGpuRecomposeFailed &&
                framePassesSucceeded && presentationSucceeded);
      }

    }

  } // _profSetup ("RenderFrame") destructs here — BEFORE endFrame() so its

    // duration is correctly recorded (popScope requires inFrame == true).

  ArtifactCore::Profiler::instance().endFrame();

} // end renderOneFrameImpl

QString CompositionRenderController::Impl::multiFramePreviewFallbackReason(

    const bool gpuBlendEnabled,

    const bool blendPipelineReady,

    const bool hasGpuBlendJustification,

    const bool hasCpuRasterizerUpload,

    const bool gpuBlendPathRequested,

    const bool renderPipelineReady,

    const bool hasDepthSlot,

    const bool acquireHazard) {

  (void)hasCpuRasterizerUpload;

  if (!gpuBlendEnabled) {

    return QStringLiteral("gpu-blend-disabled");

  }

  if (!blendPipelineReady) {

    return QStringLiteral("blend-pipeline-not-ready");

  }

  if (!hasGpuBlendJustification) {

    return QStringLiteral("no-multi-layer-blend-work");

  }

  if (!gpuBlendPathRequested) {

    return QStringLiteral("gpu-path-not-requested");

  }

  if (!renderPipelineReady) {

    return QStringLiteral("render-pipeline-not-ready");

  }

  if (!hasDepthSlot) {

    return QStringLiteral("depth-slot-unavailable");

  }

  if (acquireHazard) {

    return QStringLiteral("submitted-slot-hazard");

  }

  return QStringLiteral("eligible");

}

void CompositionRenderController::Impl::publishPreviewFrameRequestResult(

    const QPointer<ArtifactPlaybackService>& weakPlayback,

    const PreviewFrameRequest& request,
    const QPointer<CompositionRenderController>& weakOwner,

    ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer> capturedFrame) {

  if (!weakPlayback || !capturedFrame || !capturedFrame->isValid()) {

    return;

  }

  const auto frameForPublish = std::move(capturedFrame);

  QMetaObject::invokeMethod(

      weakPlayback.data(),

      [weakPlayback, weakOwner, request, frameForPublish]() {

        if (!weakPlayback) {

          return;

        }

        const auto currentComposition = weakPlayback->currentComposition();

        if (!currentComposition ||

            currentComposition->id().toString() != request.compositionId) {

          return;

        }

        const auto currentSummary = weakPlayback->ramPreviewSummary();

        if (currentSummary.buildQueueGeneration != request.buildGeneration ||

            (!request.capturedDuringPlayback &&
             currentSummary.buildQueueNextFrame != request.buildTargetFrame) ||

            request.buildTargetFrame != request.framePos ||

            !weakPlayback->isRamPreviewFramePendingBuild(request.framePos)) {

          return;

        }

        const QString renderPath = request.pipelineEnabled

            ? QStringLiteral("gpu-blend")

            : QStringLiteral("fallback");

        ArtifactCore::ImageF32x4_RGBA cpuFrame;
        cpuFrame.setFromRGBA8(frameForPublish->bytes.data(),
            static_cast<int>(frameForPublish->width),
            static_cast<int>(frameForPublish->height));
        weakPlayback->storeCompositionPreviewFrameImage(

            request.framePos, cpuFrame, request.compositionId,

            request.previewDownsample, request.effectiveDownsample,

            renderPath, request.priorityReason, true);

        // Warm only after authoritative RAM publication, on the GUI/render
        // owner lane while paused. Resource creation stays outside playback.
        // These owners share the renderer's existing 50-entry / 512 MiB LRU;
        // eviction is an ordinary resident miss, never a stale texture use.
        if (!request.capturedDuringPlayback && weakOwner &&
            !weakPlayback->isPlaying() && weakOwner->impl_->renderer_ &&
            !weakOwner->impl_->renderInProgress_) {
          auto* impl = weakOwner->impl_;
          std::uint64_t revision = 0;
          const auto* image = weakPlayback->borrowRamPreviewFrameImage(request.framePos);
          const auto* upload = weakPlayback->borrowRamPreviewFrameUpload(request.framePos, revision);
          if (image && upload) {
            const auto slot = static_cast<std::uint64_t>(request.framePos) %
                impl->ramPreviewResidentOwners_.size();
            impl->renderer_->textureForVideoFrame(*image,
                impl->ramPreviewResidentOwners_[slot], static_cast<qint64>(revision), upload);
            // Retain the original streaming slot as a no-resident fallback.
            impl->renderer_->textureForVideoFrame(*image,
                impl->ramPreviewTextureOwner_, static_cast<qint64>(revision), upload);
          }
        }

      },

      Qt::QueuedConnection);

}

bool CompositionRenderController::Impl::advanceViewportNavigation(const double deltaSeconds) {
  if (viewportNavigationMode_ ==
          CompositionRenderController::ViewportNavigationMode::Off ||
      deltaSeconds <= 0.0) {
    return false;
  }
  if (viewportNavigationForward_ == 0.0f &&
      viewportNavigationStrafe_ == 0.0f &&
      viewportNavigationLift_ == 0.0f) {
    return false;
  }
  const QVector3D forward = viewportNavigationForwardVector(
      viewportNavigationYawDeg_, viewportNavigationPitchDeg_);
  QVector3D right =
      QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f));
  if (right.lengthSquared() < 1.0e-6f) {
    right = QVector3D(1.0f, 0.0f, 0.0f);
  }
  right.normalize();
  QVector3D move;
  if (viewportNavigationMode_ ==
      CompositionRenderController::ViewportNavigationMode::Walk) {
    // Walk along the ground plane using the screen axes as the heading, so
    // W/S always mean "toward the top of the image" and strafing matches the
    // screen even when the view is pitched straight down onto the canvas.
    const QVector3D walkUp = viewportNavigationWalkUp_;
    const QMatrix4x4 view = viewportNavigationViewMatrix();
    QVector3D walkForward(view(0, 1), view(1, 1), view(2, 1));
    QVector3D walkRight(view(0, 0), view(1, 0), view(2, 0));
    walkForward -= walkUp * QVector3D::dotProduct(walkForward, walkUp);
    walkRight -= walkUp * QVector3D::dotProduct(walkRight, walkUp);
    if (walkForward.lengthSquared() > 1.0e-6f) {
      walkForward.normalize();
    }
    if (walkRight.lengthSquared() > 1.0e-6f) {
      walkRight.normalize();
    }
    if (walkForward.lengthSquared() <= 1.0e-6f &&
        walkRight.lengthSquared() <= 1.0e-6f) {
      return false;
    }
    move = walkForward * viewportNavigationForward_ +
           walkRight * viewportNavigationStrafe_;
  } else {
    move = forward * viewportNavigationForward_ +
           right * viewportNavigationStrafe_ +
           QVector3D(0.0f, 1.0f, 0.0f) * viewportNavigationLift_;
  }
  if (move.lengthSquared() < 1.0e-8f) {
    return false;
  }
  if (move.lengthSquared() > 1.0f) {
    move.normalize();
  }
  const float speed = viewportNavigationBaseSpeed_ *
      std::clamp(viewportNavigationSpeedScale_, 0.05f, 10.0f);
  viewportNavigationEye_ +=
      move * (speed * static_cast<float>(deltaSeconds));
  if (viewportNavigationMode_ ==
      CompositionRenderController::ViewportNavigationMode::Walk) {
    const float height = QVector3D::dotProduct(
        viewportNavigationEye_, viewportNavigationWalkUp_);
    viewportNavigationEye_ +=
        viewportNavigationWalkUp_ *
        (viewportNavigationWalkHeight_ - height);
  }
  return true;
}

namespace CompositionRenderControllerDetail {





void renderCrashTrace(const char* phase, quint64 frame, const QString& detail)

{

  if (!renderCrashTraceEnabled()) {

    return;

  }

  qInfo().noquote() << QStringLiteral("[CompositionView][CrashTrace] frame=%1 phase=%2 %3")

                           .arg(frame)

                           .arg(QString::fromLatin1(phase))

                           .arg(detail);

}



int compositionPreviewIntervalMs(const ArtifactCompositionPtr &comp) {

  const double fps = comp ? comp->frameRate().framerate() : 0.0;

  if (fps <= 0.0) {

    return kFallbackPreviewIntervalMs;

  }

  return std::max(1, static_cast<int>(std::lround(1000.0 / fps)));

}

int compositionRenderDispatchIntervalMs(const ArtifactCompositionPtr &comp) {

  const auto *playback = ArtifactPlaybackService::instance();

  return playback && playback->isPlaying()

             ? 1

             : compositionPreviewIntervalMs(comp);

}

} // namespace CompositionRenderControllerDetail



namespace CompositionRenderControllerDetail {





} // namespace CompositionRenderControllerDetail

ArtifactCompositionPtr

resolvePreferredComposition(ArtifactProjectService *service) {

  // A project with no current composition is authoritative.  The active and
  // playback contexts may still retain the composition from a project that
  // was just closed, and must not repopulate an intentionally empty viewport.
  ArtifactCompositionPtr projectComposition;
  if (service) {
    projectComposition = service->currentComposition().lock();
    if (!projectComposition) {
      return ArtifactCompositionPtr();
    }
  }

  // The editor is bound to ActiveContextService. Use the same authority here;
  // otherwise the render loop can replace the editor's composition every frame
  // with a stale ProjectService selection and render an unrelated empty comp.

  if (auto *app = ArtifactApplicationManager::instance()) {

    if (auto *active = app->activeContextService()) {

      if (auto comp = active->activeComposition()) {

        return comp;

      }

    }

  }



  // フォールバック: ProjectService

  if (projectComposition) {

    return projectComposition;

  }



  // フォールバック: PlaybackService

  if (auto *playback = ArtifactPlaybackService::instance()) {

    if (auto comp = playback->currentComposition()) {

      return comp;

    }

  }



  return ArtifactCompositionPtr();

}

} // namespace Artifact
