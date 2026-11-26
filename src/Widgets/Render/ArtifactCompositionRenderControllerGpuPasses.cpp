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

std::vector<FrameRenderPass> CompositionRenderController::Impl::buildFrameRenderPassPlan(

    bool useRamPreviewFallback, bool pipelineEnabled,

    bool hasComposition) const {

  std::vector<FrameRenderPass> plan;

  plan.reserve(8);

  plan.push_back(

      {FrameRenderPassKind::Setup, QStringLiteral("setup"),

       useRamPreviewFallback ? QStringLiteral("ram-preview fallback")

                             : QStringLiteral("frame bootstrap")});

  plan.push_back(

      {FrameRenderPassKind::Base, QStringLiteral("background"),

       pipelineEnabled ? QStringLiteral("seed background and intermediates")

                       : QStringLiteral("viewport background")});

  if (pipelineEnabled) {

    plan.push_back(

        {FrameRenderPassKind::Surface, QStringLiteral("surface"),

         QStringLiteral("layer surface capture")});

    plan.push_back(

        {FrameRenderPassKind::Mask, QStringLiteral("mask"),

         QStringLiteral("mask and track matte resolve")});

    plan.push_back(

        {FrameRenderPassKind::Composite, QStringLiteral("composite"),

         QStringLiteral("blend into accum target")});

    plan.push_back(

        {FrameRenderPassKind::Post, QStringLiteral("resolve"),

         QStringLiteral("resolve accumulation for viewport")});

  } else {

    plan.push_back(

        {FrameRenderPassKind::Composite,

         useRamPreviewFallback ? QStringLiteral("ram-preview-composite")

                               : QStringLiteral("direct-composite"),

         useRamPreviewFallback ? QStringLiteral("cached frame composite")

                               : QStringLiteral("direct layer composite")});

  }

  plan.push_back(

      {FrameRenderPassKind::Overlay, QStringLiteral("overlay"),

       hasComposition ? QStringLiteral("controller overlays")

                      : QStringLiteral("no composition")});

  plan.push_back(

      {FrameRenderPassKind::Flush, QStringLiteral("flush"),

       QStringLiteral("submit queued draw packets")});

  plan.push_back(

      {FrameRenderPassKind::Present, QStringLiteral("present"),

       QStringLiteral("swapchain present")});

  return plan;

}

QString CompositionRenderController::Impl::summarizeFrameRenderPassPlan(

    const std::vector<FrameRenderPass>& plan) const {

  QStringList items;

  items.reserve(static_cast<int>(plan.size()));

  for (const auto& pass : plan) {

    items.push_back(pass.name);

  }

  return items.join(QStringLiteral(" -> "));

}

ArtifactCore::ComputeRegion CompositionRenderController::Impl::mapDamageRectToRenderTarget(
    const QRectF& damageRect, Diligent::Uint32 targetWidth,
    Diligent::Uint32 targetHeight) const {
  ArtifactCore::ComputeRegion region;
  if (!renderer_ || damageRect.isEmpty() || targetWidth == 0 ||
      targetHeight == 0) {
    return region;
  }

  const std::array<QPointF, 4> canvasCorners{
      damageRect.topLeft(), damageRect.topRight(),
      damageRect.bottomLeft(), damageRect.bottomRight()};
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  for (const QPointF& corner : canvasCorners) {
    const auto mapped = renderer_->canvasToViewport(
        {static_cast<float>(corner.x()), static_cast<float>(corner.y())});
    if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y)) {
      return {};
    }
    minX = std::min(minX, mapped.x);
    minY = std::min(minY, mapped.y);
    maxX = std::max(maxX, mapped.x);
    maxY = std::max(maxY, mapped.y);
  }

  constexpr float kRasterCoveragePad = 1.0f;
  const double targetWidthD = static_cast<double>(targetWidth);
  const double targetHeightD = static_cast<double>(targetHeight);
  const auto left = static_cast<Diligent::Uint32>(std::floor(std::clamp(
      static_cast<double>(minX) - kRasterCoveragePad, 0.0, targetWidthD)));
  const auto top = static_cast<Diligent::Uint32>(std::floor(std::clamp(
      static_cast<double>(minY) - kRasterCoveragePad, 0.0, targetHeightD)));
  const auto right = static_cast<Diligent::Uint32>(std::ceil(std::clamp(
      static_cast<double>(maxX) + kRasterCoveragePad, 0.0, targetWidthD)));
  const auto bottom = static_cast<Diligent::Uint32>(std::ceil(std::clamp(
      static_cast<double>(maxY) + kRasterCoveragePad, 0.0, targetHeightD)));
  if (right <= left || bottom <= top) {
    return region;
  }

  region = {left, top, right - left, bottom - top};
  const std::uint64_t dirtyPixels =
      static_cast<std::uint64_t>(region.width) * region.height;
  const std::uint64_t targetPixels =
      static_cast<std::uint64_t>(targetWidth) * targetHeight;
  if (!region.validFor(targetWidth, targetHeight) ||
      dirtyPixels >= targetPixels - targetPixels / 4) {
    return {};
  }
  return region;
}

QRectF CompositionRenderController::Impl::mapRenderTargetRegionToCanvas(
    const ArtifactCore::ComputeRegion& region) const {
  if (!renderer_ || region.width == 0 || region.height == 0) {
    return {};
  }
  const float left = static_cast<float>(region.x);
  const float top = static_cast<float>(region.y);
  const float right = static_cast<float>(region.x + region.width);
  const float bottom = static_cast<float>(region.y + region.height);
  const std::array<QPointF, 4> targetCorners{
      QPointF(left, top), QPointF(right, top),
      QPointF(left, bottom), QPointF(right, bottom)};
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  for (const QPointF& corner : targetCorners) {
    const auto mapped = renderer_->viewportToCanvas(
        {static_cast<float>(corner.x()), static_cast<float>(corner.y())});
    if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y)) {
      return {};
    }
    minX = std::min(minX, mapped.x);
    minY = std::min(minY, mapped.y);
    maxX = std::max(maxX, mapped.x);
    maxY = std::max(maxY, mapped.y);
  }
  return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

GpuBasePassState CompositionRenderController::Impl::beginGpuBasePass(

    RenderPipeline& renderPipeline, PreviewRenderPipelineSlot& previewRenderSlot,

    float rcw, float rch, float cw, float ch, FloatColor layerBgColor,

    CompositionBackgroundMode backgroundMode,
    const QRectF* damageRect,
    ArtifactCore::ComputeRegion* outDamageRegion,
    bool* outPartialRecompose,
    QElapsedTimer* profileTimer,
    qint64* depthClearNs,
    qint64* accumulationClearNs,
    qint64* partialRegionClearNs,
    qint64* fullTargetClearNs,
    qint64* computeUnbindNs) {

  GpuBasePassState state;

  state.origZoom = renderer_->getZoom();

  state.origClearColor = renderer_->getClearColor();

  renderer_->getPan(state.origPanX, state.origPanY);

  state.origViewW = hostWidth_;

  state.origViewH = hostHeight_;



  auto* previewDepthDSV =

      static_cast<Diligent::ITextureView*>(previewRenderSlot.depthTargetView);

  renderer_->setOverrideDSV(previewDepthDSV);



  renderer_->setViewportSize(rcw, rch);

  renderer_->setCanvasSize(cw, ch);

  const float previewScale =

      std::min(rcw / std::max(1.0f, state.origViewW),

               rch / std::max(1.0f, state.origViewH));

  renderer_->setZoom(state.origZoom * previewScale);

  renderer_->setPan(state.origPanX * previewScale,

                    state.origPanY * previewScale);

  renderer_->setViewportRect(rcw, rch);

  if (outDamageRegion) {
    *outDamageRegion = {};
  }
  if (outPartialRecompose) {
    *outPartialRecompose = false;
  }



  if (compositionViewLog().isDebugEnabled()) {

    qCDebug(compositionViewLog)

        << "[CompositionView] background pass (gpu)"

        << "compSize=" << QSize(static_cast<int>(cw), static_cast<int>(ch))

        << "rtSize=" << QSize(static_cast<int>(rcw), static_cast<int>(rch))

        << "viewport="

        << QSize(static_cast<int>(state.origViewW),

                 static_cast<int>(state.origViewH))

        << "zoom=" << state.origZoom << "pan=(" << state.origPanX << ","

        << state.origPanY << ")"

        << "bg="

        << QColor::fromRgbF(layerBgColor.r(), layerBgColor.g(),

                            layerBgColor.b(), layerBgColor.a())

        << "bgMode=" << static_cast<int>(backgroundMode)

        << "compositionSpaceApplied=" << true;

  }



  ArtifactCore::ComputeRegion damageRegion;
  bool canPreserveOutside = damageRect && outDamageRegion &&
                            outPartialRecompose;
  if (canPreserveOutside) {
    damageRegion = mapDamageRectToRenderTarget(
        *damageRect, static_cast<Diligent::Uint32>(renderPipeline.width()),
        static_cast<Diligent::Uint32>(renderPipeline.height()));
    canPreserveOutside = damageRegion.validFor(
        static_cast<Diligent::Uint32>(renderPipeline.width()),
        static_cast<Diligent::Uint32>(renderPipeline.height()));
  }

  if (previewRenderSlot.depthTargetView) {
    const qint64 clearStartedNs = profileTimer
        ? profileTimer->nsecsElapsed()
        : 0;
    renderer_->clearDepthRenderTargetForColor(
        renderPipeline.accumRTV(), previewRenderSlot.depthTargetView);
    if (profileTimer && depthClearNs) {
      *depthClearNs +=
          profileTimer->nsecsElapsed() - clearStartedNs;
    }
  }

  const qint64 accumulationClearStartedNs = profileTimer
      ? profileTimer->nsecsElapsed()
      : 0;
  if (canPreserveOutside) {
    const qint64 unbindStartedNs = profileTimer
        ? profileTimer->nsecsElapsed()
        : 0;
    renderer_->unbindColorTargetsForCompute();
    if (profileTimer && computeUnbindNs) {
      *computeUnbindNs +=
          profileTimer->nsecsElapsed() - unbindStartedNs;
    }
    const qint64 partialClearStartedNs = profileTimer
        ? profileTimer->nsecsElapsed()
        : 0;
    canPreserveOutside = renderer_->clearTextureRegion(
        blendPipeline_.get(), renderPipeline.accumUAV(), damageRegion);
    if (profileTimer && partialRegionClearNs) {
      *partialRegionClearNs +=
          profileTimer->nsecsElapsed() - partialClearStartedNs;
    }
  }

  if (!canPreserveOutside) {
    const qint64 fullClearStartedNs = profileTimer
        ? profileTimer->nsecsElapsed()
        : 0;
    renderer_->clearRenderTarget(renderPipeline.accumRTV(),
                                 FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
    if (profileTimer && fullTargetClearNs) {
      *fullTargetClearNs +=
          profileTimer->nsecsElapsed() - fullClearStartedNs;
    }
  } else {
    *outDamageRegion = damageRegion;
    *outPartialRecompose = true;
  }
  if (profileTimer && accumulationClearNs) {
    *accumulationClearNs +=
        profileTimer->nsecsElapsed() - accumulationClearStartedNs;
  }



  return state;

}

bool CompositionRenderController::Impl::seedGpuBasePassBackground(

    RenderPipeline& renderPipeline, Diligent::ITextureView* layerRTV,

    Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

    Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView*& accumSRV,

    Diligent::ITextureView*& tempUAV, int& layerToFloatConvertCount,

    int& blendDispatchCount, int& blendFailureCount, float cw, float ch,

    FloatColor layerBgColor, CompositionBackgroundMode backgroundMode,
    const ArtifactCompositionPtr& composition,
    const ArtifactCore::ComputeRegion* damageRegion) {

  renderer_->pushRenderTarget(layerRTV);

  renderer_->clearRenderTarget(layerRTV, FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

  drawCompositionBackgroundDirect(renderer_.get(), cw, ch, layerBgColor,

                                  backgroundMode, checkerboardTileSize_,

                                  cachedMayaGradientSprite_,
                                  usesSpatialViewportOrientation()
                                      ? &viewportOrientationViewForOverlay_
                                      : nullptr,
                                  usesSpatialViewportOrientation()
                                      ? &viewportOrientationProjectionForOverlay_
                                      : nullptr);

  if (showCompositionRegionOverlay_ && composition &&
      !usesSpatialViewportOrientation()) {
    ::Artifact::drawCompositionRegionOverlay(renderer_.get(), composition);
  }
  if (previewOrbitActive_ && composition) {
    ::Artifact::drawNavigationCrossOverlay(renderer_.get(), composition);
  }

  renderer_->flush();

  renderer_->popRenderTarget();

  renderer_->unbindColorTargetsForCompute();



  if (layerBgColor.a() <= 0.0f) {

    return true;

  }



  ++layerToFloatConvertCount;

  const bool convertedBackgroundToFloat = damageRegion
      ? renderer_->convertLayerToFloat(
            blendPipeline_.get(), layerSRV, layerFloatUAV,
            static_cast<Diligent::Uint32>(renderPipeline.width()),
            static_cast<Diligent::Uint32>(renderPipeline.height()),
            *damageRegion)
      : renderer_->convertLayerToFloat(
            blendPipeline_.get(), layerSRV, layerFloatUAV,
            static_cast<Diligent::Uint32>(renderPipeline.width()),
            static_cast<Diligent::Uint32>(renderPipeline.height()));

  Diligent::ITextureView* backgroundBlendSrc =

      convertedBackgroundToFloat ? layerFloatSRV : layerSRV;

  if (!convertedBackgroundToFloat) {

    if (damageRegion) {
      return false;
    }

    qWarning() << "[CompositionView] background-to-float conversion failed; "

                  "falling back to legacy background SRV";

  }

  ++blendDispatchCount;

  if (damageRegion &&
      !renderer_->copyTextureOutsideRegion(accumSRV, tempUAV,
                                           *damageRegion)) {
    return false;
  }

  const bool blended = damageRegion
      ? renderer_->blendLayers(blendPipeline_.get(), backgroundBlendSrc,
                               accumSRV, tempUAV,
                               ArtifactCore::BlendMode::Normal, 1.0f,
                               *damageRegion)
      : renderer_->blendLayers(blendPipeline_.get(), backgroundBlendSrc,
                               accumSRV, tempUAV,
                               ArtifactCore::BlendMode::Normal, 1.0f);
  if (blended) {

    renderPipeline.swapAccumAndTemp();

    accumSRV = renderPipeline.accumSRV();

    tempUAV = renderPipeline.tempUAV();
    return true;

  } else {

    ++blendFailureCount;

    qWarning() << "[CompositionView] background seed blend failed";
    return false;

  }

}

void CompositionRenderController::Impl::drawGpuLayerToIntermediate(

    ArtifactAbstractLayer* layer, Diligent::ITextureView* layerRTV,

    Diligent::ITextureView* layerDepthDSV,

    Diligent::ITextureView* accumSRV, float rcw, float rch, float cw,

    float ch, DetailLevel lod, const FramePosition& currentFrame,

    const std::function<QImage(const ArtifactCore::Id&)>& matteResolver,

    const std::vector<SceneLightEntry>& sceneLights, bool has3DCamera,

    const QMatrix4x4& cameraViewMatrix, const QMatrix4x4& cameraProjMatrix,

    bool preserveSceneDepth, RenderPipeline* renderPipeline,
    Diligent::ITextureView* msaaColorRTV,
    Diligent::ITextureView* msaaDepthDSV,
    GpuRasterEffectPlan* outRasterEffectPlan) {

  // Mesh draws require color/depth attachments with the same dimensions.
  // Without this explicit depth target, the layer RTV is paired with the
  // swap-chain depth texture and the 3D pass corrupts after a transform.
  auto* activeColorRTV = msaaColorRTV ? msaaColorRTV : layerRTV;
  auto* activeDepthDSV = msaaDepthDSV ? msaaDepthDSV : layerDepthDSV;
  renderer_->setOverrideRTV(activeColorRTV);
  renderer_->setOverrideDSV(activeDepthDSV);

  renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

  if (layer->isAdjustmentLayer()) {

    renderer_->clear();

    const float savedZoom = renderer_->getZoom();

    float savedPanX = 0.0f;

    float savedPanY = 0.0f;

    renderer_->setCanvasSize(rcw, rch);

    renderer_->setZoom(1.0f);

    renderer_->setPan(0.0f, 0.0f);

    // Pointwise adjustment effects can stay on the existing accumulation
    // texture.  Unsupported effects deliberately fall through to the
    // established full-quality path below.
    ArtifactCore::PointwiseEffectStack pointwiseStack;
    bool canApplyPointwise = true;
    bool pointwiseApplied = false;

    const auto drawBgraAccumulator = [&](Diligent::ITextureView* source) {
      if (renderPipeline && blendPipeline_ && source &&
          renderer_->convertBgraAccumulatorToStraightRgba(
              blendPipeline_.get(), source, renderPipeline->tempUAV(),
              static_cast<Diligent::Uint32>(renderPipeline->width()),
              static_cast<Diligent::Uint32>(renderPipeline->height()))) {
        renderer_->drawSprite(0.0f, 0.0f, rcw, rch,
                              renderPipeline->tempSRV(), 1.0f);
        return;
      }
      qWarning() << "[CompositionView] adjustment BGRA presentation failed;"
                    " drawing source texture directly";
      renderer_->drawSprite(0.0f, 0.0f, rcw, rch, source, 1.0f);
    };
    bool spatialAdjustmentApplied = false;
    // Pointwise processing operates on the full accumulated surface, so the
    // adjustment's mask and opacity are applied *inside* the shader as a lerp
    // against the untouched accumulation (mask*opacity as the factor).  This
    // is the AE semantic: a mask restricts where the effect applies, it must
    // never scale or grade the result.  A transform still disqualifies the
    // path because the pointwise pass has no geometry stage.
    const QMatrix4x4 adjustmentTransform = layer->getGlobalTransform4x4();
    bool hasNonIdentityTransform = false;
    for (int row = 0; row < 4 && !hasNonIdentityTransform; ++row) {
      for (int column = 0; column < 4; ++column) {
        const float expected = row == column ? 1.0f : 0.0f;
        if (std::abs(adjustmentTransform(row, column) - expected) > 1.0e-5f) {
          hasNonIdentityTransform = true;
          break;
        }
      }
    }
    if (hasNonIdentityTransform) {
      canApplyPointwise = false;
    }

    // Rasterize the layer's masks into the pipeline's matte slot so the
    // pointwise shader can sample them.  Slot 0 is free on the adjustment
    // path: track mattes are a different feature and are never evaluated for
    // a layer while it is being composited as an adjustment.
    Diligent::ITextureView* adjustmentMaskSRV = nullptr;
    if (layer->hasMasks() && canApplyPointwise && renderPipeline) {
      auto devCtx = renderer_->immediateContext();
      // Sample animated mask paths at the frame being composited so a
      // mask-tracked or keyframed mask lines up with the adjustment.
      int64_t maskFrame = layer->currentFrame();
      if (auto* maskComposition = static_cast<ArtifactAbstractComposition *>(
              layer->composition())) {
        maskFrame = maskComposition->framePosition().framePosition();
      }
      const QImage maskImage = [&]() -> QImage {
        // Rasterizing the mask is full-frame CPU work, so reuse the previous
        // result whenever the layer, frame, size and mask geometry are
        // unchanged.  Panning, gizmo hovering and idle redraws all hit this
        // path, which is exactly where the cost used to be paid every frame.
        AdjustmentMaskCacheKey key;
        key.layerId = layer->id().toString();
        key.frame = maskFrame;
        key.width = static_cast<int>(renderPipeline->width());
        key.height = static_cast<int>(renderPipeline->height());
        key.contentHash = adjustmentMaskContentHash(layer, maskFrame);
        if (!adjustmentMaskCache_.isNull() &&
            adjustmentMaskCacheKey_.matches(key)) {
          // Upload only reads constBits; retain the implicitly shared cache
          // instead of cloning the full mask on every unchanged redraw.
          return adjustmentMaskCache_;
        }
        QImage rasterized = renderAdjustmentMaskToImage(
            layer, key.width, key.height, maskFrame);
        if (!rasterized.isNull()) {
          adjustmentMaskCacheKey_ = key;
          adjustmentMaskCache_ = rasterized;
        }
        return rasterized;
      }();
      if (devCtx && !maskImage.isNull() &&
          renderPipeline->updateMatteSourceFromData(
              devCtx.RawPtr(), 0, maskImage.constBits(),
              static_cast<Diligent::Uint32>(maskImage.width()),
              static_cast<Diligent::Uint32>(maskImage.height()),
              static_cast<Diligent::Uint32>(maskImage.bytesPerLine()))) {
        adjustmentMaskSRV = renderPipeline->matteSourceSRV(0);
      } else {
        // A mask that cannot be rasterized or uploaded must not silently
        // widen the effect to the whole frame: drop the pointwise path.
        canApplyPointwise = false;
      }
    }
    // Opacity is carried in the reserved mask-mix parameter slot rather than
    // in a mask texture, so a layer with only a non-unity opacity still needs
    // a mix but needs no upload.
    const bool needsMaskMix = canApplyPointwise &&
        (adjustmentMaskSRV != nullptr ||
         std::abs(layer->opacity() - 1.0f) > 1.0e-6f);
    // Effect nodes must stay below the reserved slot, and the slot itself is
    // written before dispatch so the shader can read mask*opacity.
    std::uint32_t parameterSlot = 0;
    // The reserved mask-mix slot carries layer opacity. It is written even
    // when the layer has no mask, because the shader multiplies it into the
    // mask factor.
    pointwiseStack.setParameter(
        ArtifactCore::PointwiseEffectStack::kMaskMixParameterSlot,
        std::clamp(layer->opacity(), 0.0f, 1.0f));
    for (const auto& effect : layer->getEffects()) {
      const auto exposure = ArtifactCore::dynamicPointerCast<ExposureEffect>(effect);
      const auto hueAndSaturation =
          ArtifactCore::dynamicPointerCast<HueAndSaturation>(effect);
      const auto levels =
          ArtifactCore::dynamicPointerCast<LevelsEffect>(effect);
      const auto brightness =
          ArtifactCore::dynamicPointerCast<BrightnessEffect>(effect);
      const auto whiteBalance =
          ArtifactCore::dynamicPointerCast<WhiteBalanceEffect>(effect);
      const auto invert = ArtifactCore::dynamicPointerCast<InvertEffect>(effect);
      const auto grayscale = ArtifactCore::dynamicPointerCast<GrayscaleEffect>(effect);
      if (!effect || !effect->isEnabled()) {
        continue;
      }
      if (effect->pipelineStage() != EffectPipelineStage::Rasterizer) {
        canApplyPointwise = false;
        break;
      }
      if (!exposure && !hueAndSaturation && !levels && !brightness &&
          !whiteBalance && !invert && !grayscale) {
        // Reuse the effect-owned pointwise contract (currently ChannelMixer)
        // rather than duplicating its matrix and unsupported-mode checks.
        // Per-effect scope is distinct from the adjustment layer's mask, so
        // keep those variants on the established path. New contributors are
        // limited to Normal blend until the adjustment fold is generalized.
        if (msaaColorRTV || effect->computeMode() == ComputeMode::CPU ||
            effect->gpuRasterEffectDomain() != GpuRasterEffectDomain::Pointwise ||
            effect->hasEffectRegion() || effect->maskEnabled() ||
            effect->effectMaskImageCount() > 0 || effect->mix() < 1.0f ||
            ArtifactCore::toBlendMode(layer->layerBlendType()) !=
                ArtifactCore::BlendMode::Normal ||
            !effect->appendGpuPointwiseNodes(pointwiseStack, parameterSlot) ||
            parameterSlot > ArtifactCore::PointwiseEffectStack::kMaxNodeParameterSlot) {
          canApplyPointwise = false;
          break;
        }
        continue;
      }
      if (exposure) {
        pointwiseStack.addNode(
            ArtifactCore::PointwiseNodeKind::Exposure, parameterSlot);
        pointwiseStack.setParameter(
            parameterSlot, {exposure->exposure(), 0.0f, 0.0f, 0.0f});
        ++parameterSlot;
        if (std::abs(exposure->offset()) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Offset, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {exposure->offset(), 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
        if (std::abs(exposure->gammaCorrection() - 1.0f) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Gamma, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {exposure->gammaCorrection(), 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
      } else if (hueAndSaturation) {
        // The fusion contract covers saturation and hue rotation, but not
        // HueAndSaturation's lightness or colorize branches. Preserve the
        // established fallback whenever either unsupported branch is active.
        if (std::abs(hueAndSaturation->lightness()) > 1.0e-6f ||
            hueAndSaturation->isColorize()) {
          canApplyPointwise = false;
          break;
        }
        if (std::abs(hueAndSaturation->saturation() - 1.0f) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Saturation, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {hueAndSaturation->saturation(), 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
        if (std::abs(hueAndSaturation->hue()) > 1.0e-6f) {
          constexpr float kPi = 3.14159265358979323846f;
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::HueRotate, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot,
              {hueAndSaturation->hue() * kPi / 180.0f, 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
      } else if (levels) {
        // Pointwise Levels currently models only the master input range.
        // Keep output range, gamma, and per-channel variants on fallback so
        // the GPU shortcut cannot silently change the effect semantics.
        const auto& settings = levels->settings();
        constexpr double kEpsilon = 1.0e-6;
        if (settings.perChannel ||
            std::abs(settings.inputGamma - 1.0) > kEpsilon ||
            std::abs(settings.outputBlack) > kEpsilon ||
            std::abs(settings.outputWhite - 255.0) > kEpsilon ||
            settings.inputWhite <= settings.inputBlack + kEpsilon) {
          canApplyPointwise = false;
          break;
        }
        const float inputBlack = static_cast<float>(
            std::clamp(settings.inputBlack / 255.0, 0.0, 1.0));
        const float inputWhite = static_cast<float>(
            std::clamp(settings.inputWhite / 255.0, 0.0, 1.0));
        pointwiseStack.addNode(
            ArtifactCore::PointwiseNodeKind::Levels, parameterSlot);
        pointwiseStack.setParameter(
            parameterSlot, {inputBlack, 0.0f, 0.0f, 0.0f});
        ++parameterSlot;
        pointwiseStack.setParameter(
            parameterSlot, {inputWhite, 0.0f, 0.0f, 0.0f});
        ++parameterSlot;
      } else if (brightness) {
        // BrightnessEffect's CPU path is equivalent to Offset followed by
        // Contrast only while Highlights and Shadows are at their neutral
        // values. Preserve its richer tonal branches on fallback.
        if (std::abs(brightness->highlights()) > 1.0e-6f ||
            std::abs(brightness->shadows()) > 1.0e-6f) {
          canApplyPointwise = false;
          break;
        }
        if (std::abs(brightness->brightness()) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Offset, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {brightness->brightness(), 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
        if (std::abs(brightness->contrast()) > 1.0e-6f) {
          const float c = std::clamp(brightness->contrast(), -0.999f, 0.999f);
          const float factor = (1.0f + c) / (1.0f - c);
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Contrast, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {factor, 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
      } else if (invert) {
        // The pointwise contrast primitive can express a full RGB invert or
        // a partial-strength RGB invert. Channel-specific and alpha-only
        // variants remain on the established fallback path.
        if (invert->channel() != 0) {
          canApplyPointwise = false;
          break;
        }
        const float strength = std::clamp(invert->strength(), 0.0f, 1.0f);
        const float factor = 1.0f - 2.0f * strength;
        if (std::abs(factor - 1.0f) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Contrast, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {factor, 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
      } else if (grayscale) {
        // Pointwise Saturation uses the same Rec.709 luma coefficients as
        // Grayscale mode 0. Linear-light and min/max modes stay on fallback.
        if (grayscale->mode() != 0) {
          canApplyPointwise = false;
          break;
        }
        const float strength = std::clamp(grayscale->strength(), 0.0f, 1.0f);
        if (std::abs(strength - 1.0f) > 1.0e-6f) {
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Saturation, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {1.0f - strength, 0.0f, 0.0f, 0.0f});
          ++parameterSlot;
        }
      } else {
        // WhiteBalance's Kelvin conversion is richer than the generic
        // ColorTemperature node. Only its neutral-temperature tint branch
        // is representable by the existing RGB multiplier contract.
        if (std::abs(whiteBalance->temperature() - 6500.0f) > 1.0e-4f ||
            std::abs(whiteBalance->brightness()) > 1.0e-6f) {
          canApplyPointwise = false;
          break;
        }
        if (std::abs(whiteBalance->tint()) > 1.0e-6f) {
          const float tint = whiteBalance->tint();
          pointwiseStack.addNode(
              ArtifactCore::PointwiseNodeKind::Tint, parameterSlot);
          pointwiseStack.setParameter(
              parameterSlot, {1.0f - tint * 0.5f, 1.0f + tint * 0.5f,
                              1.0f - tint * 0.5f, 0.0f});
          ++parameterSlot;
        }
      }
    }
    // The whitelist above can emit up to a few nodes per effect; refuse the
    // stack if it would run into the reserved mask-mix slot rather than
    // silently overwriting the opacity parameter.
    if (parameterSlot > ArtifactCore::PointwiseEffectStack::kMaxNodeParameterSlot) {
      canApplyPointwise = false;
    }
    if (canApplyPointwise && !pointwiseStack.nodes().empty()) {
      const auto context = renderer_->immediateContext();
      Diligent::ITextureView* mixView = nullptr;
      if (needsMaskMix && renderPipeline) {
        // The shader mixes against MaskTexture, so an opacity-only adjustment
        // still needs a view.  Upload an opaque white mask in that case: the
        // lerp then degenerates to a plain opacity fade.
        mixView = adjustmentMaskSRV
            ? adjustmentMaskSRV
            : uploadOpaqueAdjustmentMask(*renderPipeline,
                                         context ? context.RawPtr() : nullptr);
      }
      if (renderPipeline && renderPipeline->applyPointwise(
              context, pointwiseStack, nullptr, nullptr, nullptr,
              mixView)) {
        // A non-Normal blend mode must be honoured.  The pointwise pass
        // rewrote the accumulation in place, so the blend decision is applied
        // as a second pass that folds the *adjusted* result back over the
        // *original* accumulation.  Screen/Multiply/etc. exist only in that
        // fold, so skipping it is what made them silently no-op here.
        const ArtifactCore::BlendMode adjustmentBlend =
            ArtifactCore::toBlendMode(layer->layerBlendType());
        if (adjustmentBlend != ArtifactCore::BlendMode::Normal) {
          renderPipeline->foldAdjustmentBlend(
              context ? context.RawPtr() : nullptr,
              blendPipeline_.get(), adjustmentBlend);
        }
        drawBgraAccumulator(renderPipeline->accumSRV());
        pointwiseApplied = true;
      }
    }
    if (!pointwiseApplied) {
      // Start with full-quality, unscoped Blur stacks. Spatial
      // processing must preserve the accumulated backdrop across failure;
      // preflight the complete stack before touching it, then keep a GPU-only
      // snapshot for rollback. Draft scaling, layer/effect masks, temporal
      // nodes and non-Normal blend retain the existing fallback semantics.
      const auto trySpatialAdjustment = [&]() {
        if (!renderPipeline || msaaColorRTV || hasNonIdentityTransform || layer->hasMasks() ||
            std::abs(layer->opacity() - 1.0f) > 1.0e-6f ||
            lod != DetailLevel::High || viewportInteracting_ ||
            previewDownsample_ >= interactivePreviewDownsampleFloor_ ||
            std::abs(static_cast<float>(renderPipeline->width()) - cw) > 0.5f ||
            std::abs(static_cast<float>(renderPipeline->height()) - ch) > 0.5f ||
            ArtifactCore::toBlendMode(layer->layerBlendType()) !=
                ArtifactCore::BlendMode::Normal) {
          return false;
        }
        GpuSpatialEffectStack spatial;
        for (const auto& effect : layer->getEffects()) {
          if (!effect || !effect->isEnabled()) continue;
          const std::size_t firstNode = spatial.count;
          if (effect->pipelineStage() != EffectPipelineStage::Rasterizer ||
              effect->computeMode() == ComputeMode::CPU ||
              effect->gpuRasterEffectDomain() != GpuRasterEffectDomain::Spatial ||
              effect->hasEffectRegion() || effect->maskEnabled() ||
              effect->effectMaskImageCount() > 0 || effect->mix() < 1.0f ||
              effect->allowOverscan() || !effect->appendGpuSpatialNodes(spatial)) {
            return false;
          }
          if (spatial.count == firstNode) return false;
          // The CPU Blur uses the primary kernel size for every iteration,
          // including its secondary EdgePreserving pass. Do not widen the
          // path when the resident ceil(3*sigma) support would differ.
          const float primarySigma = spatial.nodes[firstNode].parameters[0];
          if (!std::isfinite(primarySigma) || primarySigma <= 0.0f ||
              primarySigma >= 3.0f) return false;
          const int cpuRadius =
              (std::max(3, static_cast<int>(primarySigma * 6.0f) | 1) - 1) / 2;
          for (std::size_t index = firstNode; index < spatial.count; ++index) {
            const float sigma = spatial.nodes[index].parameters[0];
            if (!std::isfinite(sigma) || sigma <= 0.0f || sigma >= 3.0f ||
                static_cast<int>(std::ceil(std::max(0.1f, sigma) * 3.0f)) !=
                    cpuRadius) return false;
          }
        }
        if (spatial.count == 0) return false;
        for (std::size_t index = 0; index < spatial.count; ++index) {
          const auto& node = spatial.nodes[index];
          // Sharpen's straight-alpha/threshold contract differs from the
          // resident shader; retain its established fallback here.
          if (node.historyFrameOffset != 0 ||
              node.kind != GpuSpatialEffectKind::SeparableGaussianBlur) {
            return false;
          }
        }
        const auto context = renderer_->immediateContext();
        if (!context || !renderPipeline->accumSRV() ||
            !renderPipeline->accumUAV() || !renderPipeline->tempUAV()) {
          return false;
        }
        renderer_->flush();
        renderer_->unbindColorTargetsForCompute();
        auto* original = renderPipeline->snapshotForEffectMask(
            context.RawPtr(), renderPipeline->accumSRV());
        if (!original) return false;
        for (std::size_t index = 0; index < spatial.count; ++index) {
          if (!renderPipeline->applySpatialEffect(
                  context.RawPtr(), renderPipeline->accumSRV(),
                  renderPipeline->tempUAV(), renderPipeline->accumUAV(),
                  spatial.nodes[index])) {
            Diligent::CopyTextureAttribs rollback{};
            rollback.pSrcTexture = original->GetTexture();
            rollback.SrcTextureTransitionMode =
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            rollback.pDstTexture = renderPipeline->accumUAV()->GetTexture();
            rollback.DstTextureTransitionMode =
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            context->CopyTexture(rollback);
            return false;
          }
        }
        return true;
      };
      spatialAdjustmentApplied = trySpatialAdjustment();
      if (spatialAdjustmentApplied) {
        drawBgraAccumulator(renderPipeline->accumSRV());
      } else {
        if (!pointwiseStack.nodes().empty()) {
          ++adjustmentFallbackCount_;
        }
        drawBgraAccumulator(accumSRV);
      }
    }

    renderer_->setCanvasSize(cw, ch);
    renderer_->setZoom(savedZoom);
    renderer_->setPan(savedPanX, savedPanY);

    if (pointwiseApplied || spatialAdjustmentApplied) {
      if (pointwiseApplied) ++pointwiseAppliedCount_;
      renderer_->flush();
      renderer_->setOverrideDSV(nullptr);
      renderer_->setOverrideRTV(nullptr);
      renderer_->unbindColorTargetsForCompute();
      return;
    }

    // Interactive/draft mode: skip only the remaining CPU effect processing
    // for adjustment layers to avoid GPU->CPU readback. Pointwise effects
    // above have already stayed entirely on the GPU.

    if (viewportInteracting_ || previewDownsample_ >= interactivePreviewDownsampleFloor_) {

      renderer_->flush();

      renderer_->setOverrideDSV(nullptr);
      renderer_->setOverrideRTV(nullptr);

      renderer_->unbindColorTargetsForCompute();

      return;

    }

  } else if (preserveSceneDepth) {

    // A contiguous 3D scene bin keeps the shared depth attachment alive while
    // each layer continues through the existing per-layer color compositor.
    // Clearing only color lets later geometry depth-test against earlier
    // geometry without carrying the previous layer color into layerRTV.
    renderer_->clearRenderTarget(
        layerRTV, FloatColor{0.0f, 0.0f, 0.0f, 0.0f});

  } else {

    renderer_->clear();

  }



  GpuRasterEffectPlan rasterEffectPlan;
  bool deferRasterizerEffectsToGpu =
      blendPipeline_ && renderer_->immediateContext() &&
      buildGpuRasterEffectPlan(layer, &rasterEffectPlan);
  if (deferRasterizerEffectsToGpu) {
    constexpr std::uint32_t kStillLightLeakGenericKey =
        gpuGenericKeyFromString("still_light_leak");
    bool hasStillLightLeak = false;
    for (std::size_t index = 0; index < rasterEffectPlan.count; ++index) {
      const auto& pass = rasterEffectPlan.passes[index];
      hasStillLightLeak = hasStillLightLeak ||
          (pass.kind == GpuRasterPassKind::Spatial &&
           pass.spatial.kind == GpuSpatialEffectKind::Generic &&
           pass.spatial.genericKey == kStillLightLeakGenericKey);
    }
    if (hasStillLightLeak) {
      const auto sourceSize = layer->sourceSize();
      bool invertible = false;
      const QTransform viewportToLayer =
          layer->getGlobalTransform().inverted(&invertible);
      const float targetWidth = static_cast<float>(renderPipeline->width());
      const float targetHeight = static_cast<float>(renderPipeline->height());
      if (layer->is3D() || !invertible || sourceSize.width <= 0 ||
          sourceSize.height <= 0 || targetWidth <= 1.0f ||
          targetHeight <= 1.0f) {
        // The 2D affine mapping below cannot reproduce a perspective-projected
        // 3D layer. Keep that case on its established CPU effect path.
        rasterEffectPlan.count = 0;
        deferRasterizerEffectsToGpu = false;
      } else {
        const auto viewportToLayerUv = [&](float viewportX, float viewportY) {
          const auto canvasPoint = renderer_->viewportToCanvas(
              {viewportX, viewportY});
          const QPointF layerPoint = viewportToLayer.map(
              QPointF(canvasPoint.x, canvasPoint.y));
          return QPointF(layerPoint.x() / sourceSize.width,
                          layerPoint.y() / sourceSize.height);
        };
        const QPointF origin = viewportToLayerUv(0.0f, 0.0f);
        const QPointF across = viewportToLayerUv(targetWidth - 1.0f, 0.0f);
        const QPointF down = viewportToLayerUv(0.0f, targetHeight - 1.0f);
        const std::array<float, 6> uvTransform{
            static_cast<float>(origin.x()),
            static_cast<float>(origin.y()),
            static_cast<float>(across.x() - origin.x()),
            static_cast<float>(across.y() - origin.y()),
            static_cast<float>(down.x() - origin.x()),
            static_cast<float>(down.y() - origin.y())};
        for (std::size_t index = 0; index < rasterEffectPlan.count; ++index) {
          auto& pass = rasterEffectPlan.passes[index];
          if (pass.kind != GpuRasterPassKind::Spatial ||
              pass.spatial.kind != GpuSpatialEffectKind::Generic ||
              pass.spatial.genericKey != kStillLightLeakGenericKey) {
            continue;
          }
          for (std::size_t parameterIndex = 0;
               parameterIndex < uvTransform.size(); ++parameterIndex) {
            pass.spatial.parameters[6 + parameterIndex] =
                uvTransform[parameterIndex];
          }
          pass.spatial.parameters[12] = 1.0f;
        }
      }
    }
    const bool interactiveDraft =
        viewportInteracting_ ||
        previewDownsample_ >= interactivePreviewDownsampleFloor_;
    const float effectResolutionScale = interactiveDraft
        ? 0.25f
        : lod == DetailLevel::Low
              ? 0.25f
              : lod == DetailLevel::Medium ? 0.5f : 1.0f;
    for (std::size_t index = 0; index < rasterEffectPlan.count; ++index) {
      auto& pass = rasterEffectPlan.passes[index];
      if (pass.kind != GpuRasterPassKind::Spatial) continue;
      auto& node = pass.spatial;
      for (std::size_t parameterIndex = 0;
           parameterIndex < node.parameters.size(); ++parameterIndex) {
        if ((node.resolutionScaledParameterMask &
             (1u << parameterIndex)) != 0) {
          node.parameters[parameterIndex] *= effectResolutionScale;
        }
      }
    }
  }
  if (outRasterEffectPlan) *outRasterEffectPlan = std::move(rasterEffectPlan);

  QString* dbgOut = &lastVideoDebug_;

  const std::function<Diligent::ITextureView*(ArtifactAbstractLayer*,
                                              int64_t, const QSize&)>
      precompGpuResolver =
          [this, rcw, rch, cw, ch](ArtifactAbstractLayer* precomp,
                                   int64_t childFrame,
                                   const QSize& childSize) {
            return renderPrecomp2DGpuOutput(precomp, childFrame, childSize,
                                             rcw, rch, cw, ch);
          };

  drawLayerForCompositionView(

      layer, renderer_.get(), 1.0f, dbgOut, &surfaceCache_,
      &layerSurfaceCacheDiagnosticsEnabled_, &layerSurfaceCacheHitCounts_,
      &layerSurfaceCacheMissCounts_, &layerSurfaceCacheBuildCounts_,

      gpuTextureCacheManager_.get(), currentFrame.framePosition(), true, lod,

      has3DCamera ? &cameraViewMatrix : nullptr,

                  has3DCamera ? &cameraProjMatrix : nullptr,
                  nullptr, nullptr, &matteResolver, &sceneLights,
                  viewportInteracting_ ||

          previewDownsample_ >= interactivePreviewDownsampleFloor_,

                  usesSpatialViewportOrientation(), surfaceGeneration(layer),
      &precompGpuResolver, deferRasterizerEffectsToGpu,
      &solidPointwiseCache_, &viewportCullingStats_);

  renderer_->flush();

  if (msaaColorRTV &&
      !renderer_->resolveOffscreenMultisampleTexture(msaaColorRTV, layerRTV)) {
    qWarning() << "[CompositionView] MSAA resolve failed; using unresolved layer target";
  }

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

  renderer_->unbindColorTargetsForCompute();

}

void CompositionRenderController::Impl::drawGpuLayerEmissionToTarget(

    ArtifactAbstractLayer* layer, Diligent::ITextureView* emissionRTV,
    Diligent::ITextureView* depthDSV) {

  if (!layer || !emissionRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(emissionRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshEmissionOnlyPass(true);

  layer->draw(renderer_.get());

  // setOverrideDSV/RTV submits queued draws before changing the target.
  // Keep this AOV in the immediate-context command stream; the frame-level
  // flush submits it together with the remaining AOVs and presentation.

  renderer_->setMeshEmissionOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerNormalToTarget(

    ArtifactAbstractLayer* layer, Diligent::ITextureView* normalRTV,
    Diligent::ITextureView* depthDSV) {

  if (!layer || !normalRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(normalRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshNormalOnlyPass(true);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshNormalOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerVelocityToTarget(

    ArtifactAbstractLayer* layer, Diligent::ITextureView* velocityRTV,
    Diligent::ITextureView* depthDSV) {

  if (!layer || !velocityRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(velocityRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshVelocityOnlyPass(true);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshVelocityOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerIdToTarget(ArtifactAbstractLayer* layer,

                            Diligent::ITextureView* idRTV,

                            Diligent::ITextureView* depthDSV,

                            ArtifactIRenderer::ChannelType channel,

                            float encodedId) {

  if (!layer || !idRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(idRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshIdPass(channel, encodedId);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshIdPass(ArtifactIRenderer::ChannelType::Custom, 0.0f);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerAlbedoToTarget(ArtifactAbstractLayer* layer,

                                Diligent::ITextureView* albedoRTV,
                                Diligent::ITextureView* depthDSV) {

  if (!layer || !albedoRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(albedoRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshAlbedoOnlyPass(true);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshAlbedoOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerPositionToTarget(ArtifactAbstractLayer* layer,

                                  Diligent::ITextureView* positionRTV,
                                  Diligent::ITextureView* depthDSV) {

  if (!layer || !positionRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(positionRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshPositionOnlyPass(true);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshPositionOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

void CompositionRenderController::Impl::drawGpuLayerUvToTarget(ArtifactAbstractLayer* layer,

                            Diligent::ITextureView* uvRTV,
                            Diligent::ITextureView* depthDSV) {

  if (!layer || !uvRTV || !layer->is3D()) {

    return;

  }

  renderer_->setOverrideRTV(uvRTV);
  renderer_->setOverrideDSV(depthDSV);

  renderer_->setMeshUvOnlyPass(true);

  layer->draw(renderer_.get());

  // Target reset submits the queued draw; do not force a per-AOV flush.

  renderer_->setMeshUvOnlyPass(false);

  renderer_->setOverrideDSV(nullptr);
  renderer_->setOverrideRTV(nullptr);

}

bool CompositionRenderController::Impl::applyGpuPointwiseToLayer(
    RenderPipeline& renderPipeline, Diligent::ITextureView* inputSRV,
    Diligent::ITextureView* outputUAV, Diligent::ITextureView* scratchUAV,
    const ArtifactCore::PointwiseEffectStack& stack) {
  if (stack.nodes().empty()) {
    return true;
  }
  if (!blendPipeline_ || !inputSRV || !outputUAV || !scratchUAV) {
    return false;
  }
  auto* outputTexture = outputUAV->GetTexture();
  auto* scratchTexture = scratchUAV->GetTexture();
  if (!outputTexture || !scratchTexture || outputTexture == scratchTexture) {
    return false;
  }
  auto* outputSRV = outputTexture->GetDefaultView(
      Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  auto* scratchSRV = scratchTexture->GetDefaultView(
      Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  if (!outputSRV || !scratchSRV) return false;
  const auto validation = stack.validate();
  const auto segments = stack.segments();
  auto context = renderer_->immediateContext();
  if (!validation.valid || segments.empty() || !context ||
      !blendPipeline_->updatePointwiseParameters(context.RawPtr(), stack)) {
    return false;
  }

  Diligent::ITextureView* sourceSRV = inputSRV;
  auto* parameterBuffer = blendPipeline_->createPointwiseParameterBuffer();
  if (!parameterBuffer) {
    return false;
  }
  for (const auto& segment : segments) {
    if (!ArtifactCore::PointwiseEffectFusion::validateSegment(
            stack.nodes(), segment).valid) {
      return false;
    }
    const auto plan = ArtifactCore::PointwiseEffectFusion::makeComputePlan(
        "diligent", "rgba16f", stack.nodes(), segment,
        renderPipeline.width(), renderPipeline.height());
    // Every segment needs distinct read/write textures. Alternate the two
    // existing targets instead of copying scratch back after every dispatch.
    const bool writeScratch = sourceSRV->GetTexture() == outputTexture;
    auto* destinationUAV = writeScratch ? scratchUAV : outputUAV;
    if (!plan.valid() || !blendPipeline_->applyPointwise(
            context.RawPtr(), sourceSRV, destinationUAV, parameterBuffer, plan)) {
      return false;
    }
    sourceSRV = writeScratch ? scratchSRV : outputSRV;
  }
  // Keep the caller's fixed output contract. Only an odd number of passes
  // starting on output leaves the final result in scratch and needs a copy.
  if (sourceSRV->GetTexture() != outputTexture) {
    Diligent::CopyTextureAttribs copyAttrs = {};
    copyAttrs.pSrcTexture = sourceSRV->GetTexture();
    copyAttrs.SrcTextureTransitionMode =
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copyAttrs.pDstTexture = outputTexture;
    copyAttrs.DstTextureTransitionMode =
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    context->CopyTexture(copyAttrs);
  }
  return true;
}

Diligent::ITextureView* CompositionRenderController::Impl::prepareGpuLayerForBlend(

    ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

    Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

    Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView* tempUAV,

    const GpuRasterEffectPlan* rasterEffectPlan,

    const QHash<ArtifactCore::Id, QImage>& matteSourceImages,

    const QHash<ArtifactCore::Id, Diligent::ITextureView*>& matteSourceGpuViews,

    int& layerToFloatConvertCount, bool& convertedLayerToFloat,
    const ArtifactCore::ComputeRegion* damageRegion) {

  convertedLayerToFloat = damageRegion
      ? renderer_->convertLayerToFloat(
            blendPipeline_.get(), layerSRV, layerFloatUAV,
            static_cast<Diligent::Uint32>(renderPipeline.width()),
            static_cast<Diligent::Uint32>(renderPipeline.height()),
            *damageRegion)
      : renderer_->convertLayerToFloat(
            blendPipeline_.get(), layerSRV, layerFloatUAV,
            static_cast<Diligent::Uint32>(renderPipeline.width()),
            static_cast<Diligent::Uint32>(renderPipeline.height()));

  if (!convertedLayerToFloat) {

    qCritical() << "[CompositionView] layer-to-float conversion failed; "

                   "rejecting incompatible blend input"

               << "layer=" << layer->id().toString()

               << "layerName=" << layer->layerName();

    return nullptr;

  }



  ++layerToFloatConvertCount;

  // The frame this layer is being rendered for.  Resolved once here because
  // both the temporal history lookup and the history record below need it.
  const std::int64_t effectFrame = [layer]() -> std::int64_t {
    if (auto* composition =
            static_cast<ArtifactAbstractComposition*>(layer->composition())) {
      return composition->framePosition().framePosition();
    }
    return layer->currentFrame();
  }();

  if (rasterEffectPlan && rasterEffectPlan->count > 0) {
    const auto context = renderer_->immediateContext();
    if (!context) return nullptr;
    // The mask composite blends the finished effect against the UNPROCESSED
    // layer, so the original is snapshotted into accumulationTexture before
    // any pass overwrites layerFloat_. The snapshot is a GPU-to-GPU copy and
    // is only taken when a pass actually carries a mask.
    ITextureView* effectMaskSourceSRV = nullptr;
    bool anyPassMasked = false;
    for (std::size_t index = 0; index < rasterEffectPlan->count; ++index) {
      if (rasterEffectPlan->passes[index].mask.isValid()) {
        anyPassMasked = true;
        break;
      }
    }
    if (anyPassMasked) {
      effectMaskSourceSRV = renderPipeline.snapshotForEffectMask(
          context.RawPtr(), layerFloatSRV);
      if (!effectMaskSourceSRV) {
        qWarning() << "[CompositionView] effect mask source snapshot failed"
                   << "layer=" << layer->id().toString();
        return nullptr;
      }
    }
    for (std::size_t index = 0; index < rasterEffectPlan->count; ++index) {
      const auto& pass = rasterEffectPlan->passes[index];
      if (pass.kind == GpuRasterPassKind::Pointwise) {
        if (!applyGpuPointwiseToLayer(renderPipeline, layerFloatSRV,
                                      layerFloatUAV, tempUAV,
                                      pass.pointwise)) {
          qWarning() << "[CompositionView] layer pointwise GPU pass failed"
                     << "layer=" << layer->id().toString();
          return nullptr;
        }
      } else {
        // A temporal node needs the previous frame as a second input.
        // layerHistoryView only reports a frame when it is genuinely the
        // immediately preceding one, so a seek or a cold start degrades
        // instead of sampling a distant frame.
        ITextureView* historySRV = nullptr;
        const bool historyValid =
            pass.spatial.historyFrameOffset != 0 &&
            renderPipeline.layerHistoryView(
                RenderPipeline::temporalLayerKey(layer->id().toString()),
                effectFrame, &historySRV);
        if (!renderPipeline.applySpatialEffect(
                context.RawPtr(), layerFloatSRV, tempUAV, layerFloatUAV,
                pass.spatial, historySRV, historyValid)) {
          qWarning() << "[CompositionView] layer spatial GPU pass failed; "
                        "rejecting the layer rather than dropping an effect"
                     << "layer=" << layer->id().toString()
                     << "node=" << static_cast<int>(pass.spatial.kind);
          return nullptr;
        }
        if (pass.mask.isValid()) {
          // temp_ holds the snapshot taken above and is free again now that
          // the node has consumed it as its scratch target, so it doubles as
          // this composite's output before being copied back.
          RenderPipeline::EffectMaskCompositeParams compositeParams;
          compositeParams.primaryMaskData = pass.mask.primaryData;
          compositeParams.primaryMaskWidth =
              static_cast<Diligent::Uint32>(pass.mask.primaryWidth);
          compositeParams.primaryMaskHeight =
              static_cast<Diligent::Uint32>(pass.mask.primaryHeight);
          compositeParams.maskOpacity = pass.mask.opacity;
          compositeParams.maskInverted = pass.mask.inverted;
          compositeParams.mix = pass.mask.mix;
          compositeParams.regionEnabled = pass.mask.regionEnabled;
          compositeParams.regionX = pass.mask.regionX;
          compositeParams.regionY = pass.mask.regionY;
          compositeParams.regionWidth = pass.mask.regionWidth;
          compositeParams.regionHeight = pass.mask.regionHeight;
          compositeParams.additionalMaskData = pass.mask.additionalData.data();
          compositeParams.additionalMaskCount =
              static_cast<Diligent::Uint32>(pass.mask.additionalCount);
          if (!renderPipeline.applyEffectMaskComposite(
                  context.RawPtr(), layerFloatSRV, effectMaskSourceSRV,
                  tempUAV, layerFloatUAV, compositeParams)) {
            qWarning() << "[CompositionView] effect mask composite failed; "
                          "rejecting the layer rather than dropping the mask"
                       << "layer=" << layer->id().toString();
            return nullptr;
          }
        }
      }
    }
  }

  // Record the layer's post-effect result so a temporal effect on the NEXT
  // frame has a real neighbour to sample.  This is a GPU-to-GPU copy into
  // the pipeline's pre-allocated slot; no readback and no allocation, so it
  // is safe on the hot path.  It happens before every early return below so a
  // matted layer also gets a history entry.  A bounded pool running out simply
  // reports false and the effect falls back to the CPU sampler.
  if (rasterEffectPlan) {
    bool anyTemporal = false;
    for (std::size_t i = 0; i < rasterEffectPlan->count; ++i) {
      const auto& pass = rasterEffectPlan->passes[i];
      if (pass.kind == GpuRasterPassKind::Spatial &&
          pass.spatial.historyFrameOffset != 0) {
        anyTemporal = true;
        break;
      }
    }
    if (anyTemporal && layerFloatSRV) {
      const auto historyContext = renderer_->immediateContext();
      renderPipeline.recordLayerFrame(
          historyContext.RawPtr(),
          RenderPipeline::temporalLayerKey(layer->id().toString()),
          effectFrame, layerFloatSRV);
    }
  }

  const auto reveal = layer->revealSettings();
  if (reveal.enabled && !layer->revealMapView().failed) {
    const auto context = renderer_->immediateContext();
    const QRectF bounds = layer->localBounds();
    bool invertible = false;
    const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
    if (!context || !invertible || bounds.width() <= 0.0 || bounds.height() <= 0.0) return nullptr;
    const auto normalizedLocal = [&](float x, float y) {
      const auto canvas = renderer_->viewportToCanvas({x, y});
      const QPointF local = inverse.map(QPointF(canvas.x, canvas.y));
      return QPointF((local.x() - bounds.left()) / bounds.width(),
                     (local.y() - bounds.top()) / bounds.height());
    };
    const QPointF origin = normalizedLocal(0.0f, 0.0f);
    const QPointF axisX = normalizedLocal(1.0f, 0.0f) - origin;
    const QPointF axisY = normalizedLocal(0.0f, 1.0f) - origin;
    GpuSpatialEffectNode node;
    node.kind = GpuSpatialEffectKind::Reveal;
    node.parameters = {
        static_cast<float>(axisX.x()), static_cast<float>(axisY.x()),
        static_cast<float>(origin.x()), static_cast<float>(bounds.width() / bounds.height()),
        static_cast<float>(axisX.y()), static_cast<float>(axisY.y()),
        static_cast<float>(origin.y()), 0.0f,
        reveal.progress, reveal.softness, static_cast<float>(reveal.pattern),
        reveal.angle * 0.017453292519943295f,
        reveal.centerX, reveal.centerY, static_cast<float>(reveal.seed), reveal.reverse ? 1.0f : 0.0f};
    for (const float parameter : node.parameters) if (!std::isfinite(parameter)) return nullptr;
    auto* mapSRV = reveal.pattern >= 3 ? renderPipeline.revealMapSRV(layer) : nullptr;
    if (reveal.pattern >= 3 && !mapSRV) return nullptr;
    if (!renderPipeline.applySpatialEffect(context.RawPtr(), layerFloatSRV,
                                           tempUAV, layerFloatUAV, node,
                                           nullptr, false, mapSRV)) {
      return nullptr;
    }
  }

  if (!layer->hasEnabledExternalMatteReference()) {

    return layerFloatSRV;

  }


  const auto mattes = layer->matteReferences();


  auto devCtx = renderer_->immediateContext();

  bool allMatted = true;
  bool missingMatteSource = false;

  std::vector<LayerMatteReference> batchRefs;
  batchRefs.reserve(mattes.size());
  for (const auto& matteRef : mattes) {
    if (!matteRef.enabled || matteRef.sourceLayerId.isNil() ||
        matteRef.sourceLayerId == layer->id()) {
      continue;
    }
    const auto gpuIt = matteSourceGpuViews.constFind(matteRef.sourceLayerId);
    const auto matteIt = matteSourceImages.constFind(matteRef.sourceLayerId);
    const bool hasGpuSource = gpuIt != matteSourceGpuViews.constEnd() &&
        gpuIt.value();
    const bool hasImageSource = matteIt != matteSourceImages.constEnd() &&
        !matteIt.value().isNull();
    if (!hasGpuSource && !hasImageSource) {
      allMatted = false;
      missingMatteSource = true;
      continue;
    }
    batchRefs.push_back(matteRef);
  }

  const auto shaderModeFor = [](const LayerMatteReference& matteRef) {
    return matteRef.toGpuModeIndex();
  };

  const auto shaderBlendModeFor = [](const LayerMatteReference& matteRef) {
    return matteRef.toGpuBlendIndex();
  };

  const bool batchSizeSupported = !batchRefs.empty() && batchRefs.size() <= 3;
  const bool canBatch = batchSizeSupported && allMatted;
  if (batchRefs.size() > 3) {
    if (!allMatted) {
      qWarning() << "[CompositionView] GPU track matte batch source unavailable"
                 << "layer=" << layer->id().toString()
                 << "layerName=" << layer->layerName();
      return nullptr;
    }
    std::array<Diligent::ITextureView*, 2> maskViews = {
        renderPipeline.tempSRV(), renderPipeline.matteAccumSRV()};
    std::array<Diligent::ITextureView*, 2> maskOutputs = {
        tempUAV, renderPipeline.matteAccumUAV()};
    if (!maskViews[0] || !maskViews[1] ||
        !maskOutputs[0] || !maskOutputs[1]) {
      return nullptr;
    }

    Diligent::ITextureView* currentMask = nullptr;
    size_t outputIndex = 0;
    for (size_t first = 0; first < batchRefs.size(); first += 3) {
      const size_t count = std::min<size_t>(3, batchRefs.size() - first);
      std::array<Diligent::ITextureView*, 3> matteViews{};
      bool uploadedAll = true;
      for (size_t slot = 0; slot < count; ++slot) {
        const auto& matteRef = batchRefs[first + slot];
        const auto gpuIt = matteSourceGpuViews.constFind(
            matteRef.sourceLayerId);
        if (gpuIt != matteSourceGpuViews.constEnd() && gpuIt.value()) {
          matteViews[slot] = gpuIt.value();
          continue;
        }
        const QImage matteSrc = fitMatteSourceToTarget(
            matteSourceImages.value(matteRef.sourceLayerId),
            QSize(static_cast<int>(renderPipeline.width()),
                  static_cast<int>(renderPipeline.height())),
            matteRef.fitMode);
        uploadedAll = uploadedAll && !matteSrc.isNull() &&
            renderPipeline.updateMatteSourceFromData(
                devCtx.RawPtr(), static_cast<int>(slot),
                matteSrc.constBits(),
                static_cast<Diligent::Uint32>(matteSrc.width()),
                static_cast<Diligent::Uint32>(matteSrc.height()),
                static_cast<Diligent::Uint32>(matteSrc.bytesPerLine()));
        matteViews[slot] = renderPipeline.matteSourceSRV(
            static_cast<int>(slot));
      }
      if (!uploadedAll) {
        return nullptr;
      }

      ArtifactCore::MatteTrackParams params;
      params.matteCount = static_cast<Diligent::Uint32>(count);
      for (size_t slot = 0; slot < count; ++slot) {
        const auto& matteRef = batchRefs[first + slot];
        if (slot == 0) {
          params.matteMode0 = shaderModeFor(matteRef);
          params.matteBlendMode0 = shaderBlendModeFor(matteRef);
          params.matteOpacity0 = std::clamp(matteRef.opacity, 0.0f, 1.0f);
        } else if (slot == 1) {
          params.matteMode1 = shaderModeFor(matteRef);
          params.matteBlendMode1 = shaderBlendModeFor(matteRef);
          params.matteOpacity1 = std::clamp(matteRef.opacity, 0.0f, 1.0f);
        } else {
          params.matteMode2 = shaderModeFor(matteRef);
          params.matteBlendMode2 = shaderBlendModeFor(matteRef);
          params.matteOpacity2 = std::clamp(matteRef.opacity, 0.0f, 1.0f);
        }
      }
      params.lumaMode = 0;
      if (!renderer_->applyTrackMatte(
              blendPipeline_.get(), layerFloatSRV,
              matteViews[0], count > 1 ? matteViews[1] : nullptr,
              count > 2 ? matteViews[2] : nullptr,
              maskOutputs[outputIndex], params,
              renderPipeline.width(), renderPipeline.height(),
              currentMask, true)) {
        return nullptr;
      }
      currentMask = maskViews[outputIndex];
      outputIndex = 1u - outputIndex;
    }

    ArtifactCore::MatteTrackParams applyParams;
    applyParams.matteCount = 0;
    applyParams.lumaMode = 0;
    if (!renderer_->applyTrackMatte(
            blendPipeline_.get(), layerFloatSRV,
            nullptr, nullptr, nullptr, maskOutputs[outputIndex],
            applyParams, renderPipeline.width(), renderPipeline.height(),
            currentMask, false)) {
      return nullptr;
    }
    Diligent::CopyTextureAttribs copyAttrs = {};
    copyAttrs.pSrcTexture = maskOutputs[outputIndex]->GetTexture();
    copyAttrs.SrcTextureTransitionMode =
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copyAttrs.pDstTexture = layerFloatUAV->GetTexture();
    copyAttrs.DstTextureTransitionMode =
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    devCtx->CopyTexture(copyAttrs);
    return layerFloatSRV;
  }

  if (canBatch) {
    bool uploadedAll = true;
    std::array<Diligent::ITextureView*, 3> matteViews{};
    for (size_t index = 0; index < batchRefs.size(); ++index) {
      const auto& matteRef = batchRefs[index];
      const auto gpuIt = matteSourceGpuViews.constFind(
          matteRef.sourceLayerId);
      if (gpuIt != matteSourceGpuViews.constEnd() && gpuIt.value()) {
        matteViews[index] = gpuIt.value();
        continue;
      }
      const QImage matteSrc = fitMatteSourceToTarget(
          matteSourceImages.value(matteRef.sourceLayerId),
          QSize(static_cast<int>(renderPipeline.width()),
                static_cast<int>(renderPipeline.height())),
          matteRef.fitMode);
      uploadedAll = uploadedAll && !matteSrc.isNull() &&
          renderPipeline.updateMatteSourceFromData(
              devCtx.RawPtr(), static_cast<int>(index), matteSrc.constBits(),
              static_cast<Diligent::Uint32>(matteSrc.width()),
              static_cast<Diligent::Uint32>(matteSrc.height()),
              static_cast<Diligent::Uint32>(matteSrc.bytesPerLine()));
      matteViews[index] = renderPipeline.matteSourceSRV(
          static_cast<int>(index));
    }
    if (uploadedAll) {
      ArtifactCore::MatteTrackParams params;
      params.matteCount = static_cast<Diligent::Uint32>(batchRefs.size());
      params.matteMode0 = shaderModeFor(batchRefs[0]);
      if (batchRefs.size() > 1) {
        params.matteMode1 = shaderModeFor(batchRefs[1]);
      }
      if (batchRefs.size() > 2) {
        params.matteMode2 = shaderModeFor(batchRefs[2]);
      }
      params.matteBlendMode0 = shaderBlendModeFor(batchRefs[0]);
      params.matteOpacity0 = std::clamp(batchRefs[0].opacity, 0.0f, 1.0f);
      if (batchRefs.size() > 1) {
        params.matteBlendMode1 = shaderBlendModeFor(batchRefs[1]);
        params.matteOpacity1 =
            std::clamp(batchRefs[1].opacity, 0.0f, 1.0f);
      }
      if (batchRefs.size() > 2) {
        params.matteBlendMode2 = shaderBlendModeFor(batchRefs[2]);
        params.matteOpacity2 =
            std::clamp(batchRefs[2].opacity, 0.0f, 1.0f);
      }
      params.lumaMode = 0;
      if (renderer_->applyTrackMatte(
              blendPipeline_.get(), layerFloatSRV,
              matteViews[0],
              batchRefs.size() > 1 ? matteViews[1] : nullptr,
              batchRefs.size() > 2 ? matteViews[2] : nullptr,
              tempUAV, params, renderPipeline.width(), renderPipeline.height())) {
        Diligent::CopyTextureAttribs copyAttrs = {};
        copyAttrs.pSrcTexture = tempUAV->GetTexture();
        copyAttrs.SrcTextureTransitionMode =
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copyAttrs.pDstTexture = layerFloatUAV->GetTexture();
        copyAttrs.DstTextureTransitionMode =
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        devCtx->CopyTexture(copyAttrs);
        return layerFloatSRV;
      }
    }
  }

  for (const auto& matteRef : mattes) {

    if (!matteRef.enabled || matteRef.sourceLayerId.isNil() ||
        matteRef.sourceLayerId == layer->id()) {

      continue;

    }

    const auto matteIt = matteSourceImages.constFind(matteRef.sourceLayerId);
    const auto gpuIt = matteSourceGpuViews.constFind(matteRef.sourceLayerId);
    const bool hasGpuSource = gpuIt != matteSourceGpuViews.constEnd() &&
        gpuIt.value();

    if (!hasGpuSource &&
        (matteIt == matteSourceImages.constEnd() || matteIt.value().isNull())) {

      allMatted = false;

      missingMatteSource = true;

      continue;

    }

    bool uploaded = hasGpuSource;
    QImage matteSrc;
    if (!hasGpuSource) {
      matteSrc = fitMatteSourceToTarget(
          matteIt.value(),
          QSize(static_cast<int>(renderPipeline.width()),
                static_cast<int>(renderPipeline.height())),
          matteRef.fitMode);
      if (matteSrc.isNull()) {
        allMatted = false;
        continue;
      }

      uploaded = renderPipeline.updateMatteSourceFromData(
          devCtx.RawPtr(), matteSrc.constBits(),
          static_cast<Diligent::Uint32>(matteSrc.width()),
          static_cast<Diligent::Uint32>(matteSrc.height()),
          static_cast<Diligent::Uint32>(matteSrc.bytesPerLine()));
    }

    if (!uploaded) {

      allMatted = false;

      continue;

    }



    const Diligent::Uint32 shaderMode = shaderModeFor(matteRef);



    ArtifactCore::MatteTrackParams params;

    params.matteCount = 1;

    params.matteMode0 = shaderMode;

    params.matteBlendMode0 = shaderBlendModeFor(matteRef);

    params.lumaMode = 0;

    params.matteOpacity0 = std::clamp(matteRef.opacity, 0.0f, 1.0f);

    if (!renderer_->applyTrackMatte(

            blendPipeline_.get(), layerFloatSRV,

            hasGpuSource ? gpuIt.value() : renderPipeline.matteSourceSRV(),
            nullptr, nullptr, tempUAV,

            params, renderPipeline.width(), renderPipeline.height())) {

      allMatted = false;

      continue;

    }



    Diligent::CopyTextureAttribs copyAttrs = {};

    copyAttrs.pSrcTexture = tempUAV->GetTexture();

    copyAttrs.SrcTextureTransitionMode =

        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    copyAttrs.pDstTexture = layerFloatUAV->GetTexture();

    copyAttrs.DstTextureTransitionMode =

        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    devCtx->CopyTexture(copyAttrs);

  }

  if (!allMatted) {

    if (missingMatteSource) {

      qWarning() << "[CompositionView] GPU track matte source unavailable; using unmasked layer"
                 << "layer=" << layer->id().toString()
                 << "layerName=" << layer->layerName();

      return layerFloatSRV;

    }

    qCritical() << "[CompositionView] GPU track matte failed; layer blend skipped for"

               << layer->layerName();

    return nullptr;

  }

  return layerFloatSRV;

}

GpuLayerBlendResult CompositionRenderController::Impl::blendGpuLayerIntoAccum(

    ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

    Diligent::ITextureView* layerSRV,

    Diligent::ITextureView* preparedBlendSRV,

    Diligent::ITextureView*& accumSRV, Diligent::ITextureView*& tempUAV,

    ArtifactCore::BlendMode blendMode, float opacity, float cw, float ch,

    int& blendDispatchCount, int& blendRetryNormalCount,

    int& blendFailureCount, int& directBlendFallbackCount,

    bool convertedLayerToFloat,
    const ArtifactCore::ComputeRegion* damageRegion) {

  GpuLayerBlendResult result;

  result.convertedLayerToFloat = convertedLayerToFloat;

  (void)renderPipeline;
  (void)layerSRV;
  (void)cw;
  (void)ch;
  (void)blendRetryNormalCount;
  (void)directBlendFallbackCount;

  if (!convertedLayerToFloat || !preparedBlendSRV) {

    ++blendFailureCount;

    qCritical() << "[CompositionView] blend input does not satisfy the "
                   "canonical float/straight-alpha contract"
                << "layer=" << layer->id().toString()
                << "layerName=" << layer->layerName()
                << "mode=" << static_cast<int>(blendMode);

    return result;

  }

  ++blendDispatchCount;

  if (damageRegion &&
      !renderer_->copyTextureOutsideRegion(accumSRV, tempUAV,
                                           *damageRegion)) {
    ++blendFailureCount;
    return result;
  }

  result.blended = damageRegion
      ? renderer_->blendLayers(blendPipeline_.get(), preparedBlendSRV,
                               accumSRV, tempUAV, blendMode, opacity,
                               *damageRegion)
      : renderer_->blendLayers(blendPipeline_.get(), preparedBlendSRV,
                               accumSRV, tempUAV, blendMode, opacity);

  if (!result.blended) {

    ++blendFailureCount;

    qCritical() << "[CompositionView] requested blend failed; accumulator "
                   "left unchanged"

               << "layer=" << layer->id().toString()

               << "layerName=" << layer->layerName()

               << "mode=" << static_cast<int>(blendMode)

               << "opacity=" << opacity;

    return result;

  }



  renderPipeline.swapAccumAndTemp();

  accumSRV = renderPipeline.accumSRV();

  tempUAV = renderPipeline.tempUAV();

  return result;

}

namespace CompositionRenderControllerDetail {

QImage resolveProjectRenderInputImage(const ArtifactCore::Id& projectItemId,
                                      const std::int64_t frameNumber) {
  if (projectItemId.isNil()) return {};
  auto* service = ArtifactProjectService::instance();
  auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return {};
  FootageItem* matched = nullptr;
  std::function<void(ProjectItem*)> find = [&](ProjectItem* item) {
    if (!item || matched) return;
    if (item->id == projectItemId && item->type() == eProjectItemType::Footage) {
      auto* footage = static_cast<FootageItem*>(item);
      if (footage->assetUsage == ProjectAssetUsage::RenderInput) matched = footage;
      return;
    }
    for (auto* child : item->children) find(child);
  };
  for (auto* root : project->projectItems()) find(root);
  if (!matched) return {};
  QString path = matched->filePath;
  if (matched->isSequence && !matched->sequencePaths.isEmpty()) {
    const auto count = static_cast<std::int64_t>(matched->sequencePaths.size());
    const auto index = static_cast<int>(((frameNumber % count) + count) % count);
    path = matched->sequencePaths.at(index);
  }
  ArtifactCore::ImageF32x4_RGBA decoded;
  if (path.trimmed().isEmpty() || !decoded.load(path)) return {};
  // Explicit compatibility boundary: the established matte resolver uploads
  // RGBA8 QImage data to Diligent. Decoding remains ImageF32x4_RGBA/OIIO.
  return decoded.toQImage();
}

bool gpuBlendForNormalCompositionEnabled()

{

  static const bool enabled =

      qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL") &&

      qEnvironmentVariable("ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL") !=

          QStringLiteral("0");

  return enabled;

}

QRectF effectExpandedLayerBounds(const ArtifactAbstractLayer *layer) {
  if (!layer) {
    return {};
  }
  const QRectF bounds = layer->transformedBoundingBox();
  const QRectF localBounds = layer->localBounds();
  const float overscan = layerOverscanPixels(layer);
  if (overscan <= 0.0f || !bounds.isValid() || !localBounds.isValid() ||
      localBounds.width() <= 0.0 || localBounds.height() <= 0.0) {
    return bounds;
  }
  const qreal scale = std::max(std::abs(bounds.width() / localBounds.width()),
                               std::abs(bounds.height() / localBounds.height()));
  const qreal expansion = static_cast<qreal>(overscan) * scale;
  return bounds.adjusted(-expansion, -expansion, expansion, expansion);
}

} // namespace CompositionRenderControllerDetail

DetailLevel detailLevelFromZoom(float zoom) {

  if (zoom < 0.50f) {

    return DetailLevel::Low;

  }

  if (zoom < 0.90f) {

    return DetailLevel::Medium;

  }

  return DetailLevel::High;

}

bool passesLayerRenderFilter(CompositionLayerRenderFilter filter,
                             const QStringList &selectedIds,
                             const LayerID &selectedLayerId,
                             const ArtifactAbstractLayerPtr &layer) {
  if (filter == CompositionLayerRenderFilter::All) {
    return true;
  }
  if (!layer) {
    return false;
  }
  if (!selectedIds.isEmpty()) {
    return isLayerSelected(selectedIds, layer);
  }
  return !selectedLayerId.isNil() && layer->id() == selectedLayerId;
}

CompositionViewportLayerCategory classifyLayerCategory(
    const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return CompositionViewportLayerCategory::Null;
  }
  const QString name = layer->className().toQString();
  if (name == QStringLiteral("ArtifactLightLayer")) {
    return CompositionViewportLayerCategory::Light3D;
  }
  if (name == QStringLiteral("ArtifactCameraLayer") ||
      name == QStringLiteral("ArtifactCompositionBackgroundLayer") ||
      name == QStringLiteral("ArtifactEnvironmentMapLayer")) {
    return CompositionViewportLayerCategory::Camera3D;
  }
  if (name == QStringLiteral("Artifact3DModelLayer")) {
    return CompositionViewportLayerCategory::Model3D;
  }
  if (name == QStringLiteral("ArtifactAdjustableLayer") ||
      name == QStringLiteral("ArtifactSandSim2DLayer")) {
    return CompositionViewportLayerCategory::Adjustment;
  }
  if (name == QStringLiteral("ArtifactNullLayer")) {
    return CompositionViewportLayerCategory::Null;
  }
  if (name == QStringLiteral("ArtifactSpatialAudioLayer")) {
    return CompositionViewportLayerCategory::Audio;
  }
  if (name == QStringLiteral("ArtifactParticleLayer")) {
    return CompositionViewportLayerCategory::Particle;
  }
  if (name == QStringLiteral("ArtifactCloneLayer")) {
    return CompositionViewportLayerCategory::Clone;
  }
  if (name == QStringLiteral("ArtifactTextLayer")) {
    return CompositionViewportLayerCategory::Text;
  }
  if (name == QStringLiteral("ArtifactSolid2DLayer") ||
      name == QStringLiteral("ArtifactNoiseLayer")) {
    return CompositionViewportLayerCategory::Solid2D;
  }
  if (name == QStringLiteral("ArtifactImageLayer")) {
    return CompositionViewportLayerCategory::Image;
  }
  if (name == QStringLiteral("ArtifactShapeLayer")) {
    return CompositionViewportLayerCategory::Shape;
  }
  if (name == QStringLiteral("ArtifactMaskLayer")) {
    return CompositionViewportLayerCategory::Mask;
  }
  // 2D cards (and unknown subclasses) are treated as 2D content so the
  // viewport still has a sensible category for legacy layers.
  if (layer->is3D()) {
    return CompositionViewportLayerCategory::Model3D;
  }
  return CompositionViewportLayerCategory::Image;
}

bool passesLayerCategoryMask(CompositionViewportLayerCategoryMask mask,
                             const ArtifactAbstractLayerPtr &layer,
                             CompositionViewportLayerCategory category) {
  if (!layer) {
    return true;
  }
  if (mask == static_cast<CompositionViewportLayerCategoryMask>(
                 CompositionViewportLayerCategory::All)) {
    return true;
  }
  return (static_cast<CompositionViewportLayerCategoryMask>(category) &
          mask) != 0;
}

namespace CompositionRenderControllerDetail {

QImage makeSolidColorSprite(const FloatColor &color) {

  QImage image(1, 1, QImage::Format_RGBA8888);

  image.fill(QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));

  return image;

}

[[maybe_unused]] bool buildGpuPointwiseColorStack(
    ArtifactAbstractLayer *layer,
    ArtifactCore::PointwiseEffectStack *outStack) {
  if (!layer || !outStack || layer->isAdjustmentLayer()) {
    return false;
  }

  ArtifactCore::PointwiseEffectStack stack;
  std::uint32_t parameterSlot = 0;
  for (const auto &effect : layer->getEffects()) {
    if (!effect || !effect->isEnabled()) {
      continue;
    }
    if (effect->pipelineStage() != EffectPipelineStage::Rasterizer ||
        effect->computeMode() == ComputeMode::CPU ||
        effect->hasEffectRegion() || effect->hasMask() ||
        effect->effectMaskImageCount() > 0 ||
        std::abs(effect->mix() - 1.0f) > 1.0e-6f) {
      return false;
    }

    // The renderer only owns common execution constraints. Concrete effects
    // contribute their GPU representation through ArtifactAbstractEffect.
    if (!effect->appendGpuPointwiseNodes(stack, parameterSlot)) {
      return false;
    }

#if 0 // Replaced by ArtifactAbstractEffect::appendGpuPointwiseNodes().
    const auto exposure = ArtifactCore::dynamicPointerCast<ExposureEffect>(effect);
    const auto hueAndSaturation =
        ArtifactCore::dynamicPointerCast<HueAndSaturation>(effect);
    const auto levels = ArtifactCore::dynamicPointerCast<LevelsEffect>(effect);
    const auto brightness = ArtifactCore::dynamicPointerCast<BrightnessEffect>(effect);
    const auto whiteBalance = ArtifactCore::dynamicPointerCast<WhiteBalanceEffect>(effect);
    const auto invert = ArtifactCore::dynamicPointerCast<InvertEffect>(effect);
    const auto grayscale = ArtifactCore::dynamicPointerCast<GrayscaleEffect>(effect);

    if (exposure) {
      stack.addNode(ArtifactCore::PointwiseNodeKind::Exposure, parameterSlot);
      stack.setParameter(parameterSlot++, {exposure->exposure(), 0.0f, 0.0f, 0.0f});
      if (std::abs(exposure->offset()) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Offset, parameterSlot);
        stack.setParameter(parameterSlot++, {exposure->offset(), 0.0f, 0.0f, 0.0f});
      }
      if (std::abs(exposure->gammaCorrection() - 1.0f) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Gamma, parameterSlot);
        stack.setParameter(parameterSlot++,
                           {exposure->gammaCorrection(), 0.0f, 0.0f, 0.0f});
      }
    } else if (hueAndSaturation) {
      if (std::abs(hueAndSaturation->lightness()) > 1.0e-6f ||
          hueAndSaturation->isColorize()) {
        return false;
      }
      if (std::abs(hueAndSaturation->saturation() - 1.0f) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Saturation, parameterSlot);
        stack.setParameter(parameterSlot++,
                           {hueAndSaturation->saturation(), 0.0f, 0.0f, 0.0f});
      }
      if (std::abs(hueAndSaturation->hue()) > 1.0e-6f) {
        constexpr float kPi = 3.14159265358979323846f;
        stack.addNode(ArtifactCore::PointwiseNodeKind::HueRotate, parameterSlot);
        stack.setParameter(parameterSlot++,
                           {hueAndSaturation->hue() * kPi / 180.0f,
                            0.0f, 0.0f, 0.0f});
      }
    } else if (levels) {
      const auto &settings = levels->settings();
      constexpr double kEpsilon = 1.0e-6;
      if (settings.perChannel || std::abs(settings.inputGamma - 1.0) > kEpsilon ||
          std::abs(settings.outputBlack) > kEpsilon ||
          std::abs(settings.outputWhite - 255.0) > kEpsilon ||
          settings.inputWhite <= settings.inputBlack + kEpsilon) {
        return false;
      }
      stack.addNode(ArtifactCore::PointwiseNodeKind::Levels, parameterSlot);
      stack.setParameter(parameterSlot++,
                         {static_cast<float>(std::clamp(settings.inputBlack / 255.0,
                                                        0.0, 1.0)),
                          0.0f, 0.0f, 0.0f});
      stack.setParameter(parameterSlot++,
                         {static_cast<float>(std::clamp(settings.inputWhite / 255.0,
                                                        0.0, 1.0)),
                          0.0f, 0.0f, 0.0f});
    } else if (brightness) {
      if (std::abs(brightness->highlights()) > 1.0e-6f ||
          std::abs(brightness->shadows()) > 1.0e-6f) {
        return false;
      }
      if (std::abs(brightness->brightness()) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Offset, parameterSlot);
        stack.setParameter(parameterSlot++,
                           {brightness->brightness(), 0.0f, 0.0f, 0.0f});
      }
      if (std::abs(brightness->contrast()) > 1.0e-6f) {
        const float c = std::clamp(brightness->contrast(), -0.999f, 0.999f);
        stack.addNode(ArtifactCore::PointwiseNodeKind::Contrast, parameterSlot);
        stack.setParameter(parameterSlot++, {(1.0f + c) / (1.0f - c),
                                             0.0f, 0.0f, 0.0f});
      }
    } else if (invert) {
      if (invert->channel() != 0) {
        return false;
      }
      const float factor = 1.0f - 2.0f * std::clamp(invert->strength(), 0.0f, 1.0f);
      if (std::abs(factor - 1.0f) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Contrast, parameterSlot);
        stack.setParameter(parameterSlot++, {factor, 0.0f, 0.0f, 0.0f});
      }
    } else if (grayscale) {
      if (grayscale->mode() != 0) {
        return false;
      }
      const float strength = std::clamp(grayscale->strength(), 0.0f, 1.0f);
      if (std::abs(strength - 1.0f) > 1.0e-6f) {
        stack.addNode(ArtifactCore::PointwiseNodeKind::Saturation, parameterSlot);
        stack.setParameter(parameterSlot++, {1.0f - strength, 0.0f, 0.0f, 0.0f});
      }
    } else if (whiteBalance) {
      if (std::abs(whiteBalance->temperature() - 6500.0f) > 1.0e-4f ||
          std::abs(whiteBalance->brightness()) > 1.0e-6f) {
        return false;
      }
      if (std::abs(whiteBalance->tint()) > 1.0e-6f) {
        const float tint = whiteBalance->tint();
        stack.addNode(ArtifactCore::PointwiseNodeKind::Tint, parameterSlot);
        stack.setParameter(parameterSlot++, {1.0f - tint * 0.5f,
                                             1.0f + tint * 0.5f,
                                             1.0f - tint * 0.5f, 0.0f});
      }
    } else {
      return false;
    }
#endif

    if (parameterSlot >= ArtifactCore::PointwiseEffectStack::kParameterSlotCount) {
      return false;
    }
  }

  if (stack.nodes().empty()) {
    return false;
  }
  *outStack = std::move(stack);
  return true;
}

} // namespace CompositionRenderControllerDetail

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

} // namespace Artifact
