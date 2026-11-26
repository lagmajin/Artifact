module;

#define NOMINMAX
#define QT_NO_KEYWORDS

#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ratio>

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QString>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Video;
import Artifact.Preview.Pipeline;
import Artifact.Render.CompositionPassState;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.IRenderer;
import Artifact.Render.Pipeline;
import Artifact.Service.Playback;
import Artifact.Widgets.ViewportColorPipeline;
import Color.Float;
import Frame.Position;
import Graphics.SurfaceColorContract;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

using namespace CompositionRenderControllerDetail;

Diligent::ITextureView *CompositionRenderController::Impl::applyDisplayColorTransform(
    Diligent::ITextureView *source,
    Diligent::ITextureView *destination,
    int width,
    int height) {
  if (!viewportColorPipeline_ || !renderer_) return source;
  const auto lutUpdate = viewportColorPipeline_->prepare();
  if (lutUpdate.clear) renderer_->clearViewportDisplayLut();
  if (lutUpdate.update) {
    renderer_->updateViewportDisplayLut(
        lutUpdate.data, lutUpdate.edge, lutUpdate.inputMin,
        lutUpdate.inputMax);
  }
  return lutUpdate.active
      ? renderer_->applyViewportDisplayTransform(
            source, destination, width, height)
      : source;
}

Diligent::ITextureView* CompositionRenderController::Impl::finalizeGpuRenderToViewport(

    RenderPipeline& renderPipeline, Diligent::ITextureView* accumSRV,

    Diligent::ITextureView* layerFloatSRV,

    Diligent::ITextureView* layerFloatUAV, float rcw, float rch, float cw,

    float ch, float origViewW, float origViewH, float origZoom,

    float origPanX, float origPanY, FloatColor bgColor,

    FloatColor layerBgColor, FloatColor origClearColor,

    CompositionBackgroundMode backgroundMode,

    int& layerToFloatConvertCount) {

  // The preview pass may leave its offscreen depth view active. The final
  // viewport (including the 3D ground grid) renders to the swap-chain RTV,
  // whose dimensions can differ by one pixel after resize/DPI rounding.
  // Restore the matching swap-chain DSV before any viewport draw is queued.
  renderer_->setOverrideDSV(nullptr);

  renderer_->setViewportRect(origViewW, origViewH);

  renderer_->setUseExternalMatrices(false);

  renderer_->resetGizmoCameraMatrices();

  renderer_->reset3DCameraMatrices();



  const bool transparentCompositionBackground = layerBgColor.a() < 0.999f;

  // The single-pane path keeps its existing command ordering. Quad mode
  // presents the already-resolved texture below, once per Diligent viewport.
  if (presentationLayout_ == CompositionViewportPresentationLayout::Single) {

  if (backgroundMode == CompositionBackgroundMode::MayaGradient) {

    drawViewportMayaGradientBackground(renderer_.get(), origViewW, origViewH,

                                       bgColor, cachedMayaGradientSprite_);

  } else if (backgroundMode == CompositionBackgroundMode::Checkerboard ||

             transparentCompositionBackground) {

    drawViewportCheckerboardBackground(renderer_.get(), origViewW, origViewH,

                                       checkerboardTileSize_);

  }



  renderer_->setCanvasSize(cw, ch);

  renderer_->setZoom(origZoom);

  renderer_->setPan(origPanX, origPanY);

  // The ground grid belongs behind the composition surface, not in the
  // post-composite gizmo overlay pass.
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
  }



  Diligent::ITextureView* finalPresentSRV = accumSRV;

  ++layerToFloatConvertCount;

  const bool convertedAccumToStraight = blendPipeline_ && renderer_ &&
      renderer_->convertBgraAccumulatorToStraightRgba(
          blendPipeline_.get(), accumSRV, layerFloatUAV,
          static_cast<Diligent::Uint32>(renderPipeline.width()),
          static_cast<Diligent::Uint32>(renderPipeline.height()));

  if (convertedAccumToStraight) {

    finalPresentSRV = layerFloatSRV;

  } else {

    qWarning() << "[CompositionView] accum-to-straight conversion failed; "

                  "presenting premultiplied accum directly";

  }



  lastPresentedReadbackSRV_ = finalPresentSRV;
  lastPresentedReadbackChannelOrder_ = convertedAccumToStraight
      ? ArtifactCore::SurfaceChannelOrder::RGBA
      : ArtifactCore::SurfaceChannelOrder::BGRA;
  compositeFinalizedThisFrame_ = true;

  // P1-5/P1-10 display-only exposure and clipping warnings. Written to a
  // scratch surface and applied only
  // at the presentation selection below; finalPresentSRV and
  // lastPresentedReadbackSRV_ are intentionally left untouched so the color
  // sampler, Color Science scopes, magnifier and RAM preview readback keep
  // seeing the unmodified composite. Restricted to Color mode so AOV and
  // single-channel inspection stay neutral.
  Diligent::ITextureView* exposedPresentSRV = nullptr;
  const bool exposureTransformActive = viewportExposureEnabled_ &&
      (viewportExposureGain_ != 0.0f || viewportExposureGamma_ != 1.0f ||
       viewportExposureSaturation_ != 1.0f);
  if ((exposureTransformActive || viewportClippingWarningsEnabled_) &&
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Color) {
    const bool displayAdjustmentApplied = renderer_->applyViewportExposure(
        finalPresentSRV, renderPipeline.tempUAV(),
        static_cast<Diligent::Uint32>(renderPipeline.width()),
        static_cast<Diligent::Uint32>(renderPipeline.height()),
        exposureTransformActive ? viewportExposureGain_ : 0.0f,
        exposureTransformActive ? viewportExposureGamma_ : 1.0f,
        exposureTransformActive ? viewportExposureSaturation_ : 1.0f,
        viewportClippingWarningsEnabled_,
        viewportClippingUnderThreshold_, viewportClippingOverThreshold_);
    if (displayAdjustmentApplied) {
      exposedPresentSRV = renderPipeline.tempSRV();
    }
  }

  Diligent::ITextureView* channelComponentSource = nullptr;
  Diligent::Uint32 channelComponent = 0;
  // P1-6: Straight is produced directly from the beauty SRV below; the
  // other two variants go through displayComposite channels 7 / 8.
  bool straightDisplayReady = false;
  switch (viewportChannelDisplayMode_) {
  case ViewportChannelDisplayMode::Red:
    channelComponentSource = finalPresentSRV;
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::Green:
    channelComponentSource = finalPresentSRV;
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::Blue:
    channelComponentSource = finalPresentSRV;
    channelComponent = 2;
    break;
  case ViewportChannelDisplayMode::Alpha:
    channelComponentSource = finalPresentSRV;
    channelComponent = 3;
    break;
  case ViewportChannelDisplayMode::AlbedoR:
    channelComponentSource = renderPipeline.albedoSRV();
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::AlbedoG:
    channelComponentSource = renderPipeline.albedoSRV();
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::AlbedoB:
    channelComponentSource = renderPipeline.albedoSRV();
    channelComponent = 2;
    break;
  case ViewportChannelDisplayMode::NormalX:
    channelComponentSource = renderPipeline.normalSRV();
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::NormalY:
    channelComponentSource = renderPipeline.normalSRV();
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::NormalZ:
    channelComponentSource = renderPipeline.normalSRV();
    channelComponent = 2;
    break;
  case ViewportChannelDisplayMode::VelocityX:
    channelComponentSource = renderPipeline.velocitySRV();
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::VelocityY:
    channelComponentSource = renderPipeline.velocitySRV();
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::PositionX:
    channelComponentSource = renderPipeline.positionSRV();
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::PositionY:
    channelComponentSource = renderPipeline.positionSRV();
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::PositionZ:
    channelComponentSource = renderPipeline.positionSRV();
    channelComponent = 2;
    break;
  case ViewportChannelDisplayMode::U:
    channelComponentSource = renderPipeline.uvSRV();
    channelComponent = 0;
    break;
  case ViewportChannelDisplayMode::V:
    channelComponentSource = renderPipeline.uvSRV();
    channelComponent = 1;
    break;
  case ViewportChannelDisplayMode::Albedo:
    channelComponentSource = renderPipeline.albedoSRV();
    channelComponent = 4;
    break;
  case ViewportChannelDisplayMode::Normal:
    channelComponentSource = renderPipeline.normalSRV();
    channelComponent = 5;
    break;
  case ViewportChannelDisplayMode::Velocity:
    channelComponentSource = renderPipeline.velocitySRV();
    channelComponent = 6;
    break;
  case ViewportChannelDisplayMode::Position:
    // Raw world coordinates; shown unencoded like Albedo.
    channelComponentSource = renderPipeline.positionSRV();
    channelComponent = 4;
    break;
  case ViewportChannelDisplayMode::UV:
    channelComponentSource = renderPipeline.uvSRV();
    channelComponent = 4;
    break;
  // P1-6: Autograph-style variants. Straight reuses the existing
  // BGRA-accumulator conversion (no new shader); Luminance / Matte use
  // displayComposite modes 4 / 5 from LayerBlendComputeShader.
  case ViewportChannelDisplayMode::Unpremultiplied: {
    straightDisplayReady = renderer_->convertBgraAccumulatorToStraightRgba(
        blendPipeline_.get(), finalPresentSRV, renderPipeline.tempUAV(),
        renderPipeline.width(), renderPipeline.height());
    if (straightDisplayReady) {
      viewportChannelDisplaySRV_ = renderPipeline.tempSRV();
    }
    break;
  }
  case ViewportChannelDisplayMode::Luminance:
    channelComponentSource = finalPresentSRV;
    channelComponent = 7;
    break;
  case ViewportChannelDisplayMode::Matte:
    channelComponentSource = finalPresentSRV;
    channelComponent = 8;
    break;
  default:
    break;
  }
  const bool presentComponentAsPrimary =
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Red ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Green ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Blue ||
      viewportChannelDisplayMode_ == ViewportChannelDisplayMode::Alpha;
  bool channelDisplayReady = straightDisplayReady;
  if (channelComponentSource && blendPipeline_) {
    if (channelComponent <= 3) {
      channelDisplayReady = renderer_->displayTextureComponent(
          blendPipeline_.get(), channelComponentSource,
          renderPipeline.tempUAV(), channelComponent,
          renderPipeline.width(), renderPipeline.height());
    } else {
      const Diligent::Uint32 mode = channelComponent == 4 ? 1u :
          channelComponent == 5 ? 2u : channelComponent == 6 ? 3u :
          channelComponent == 7 ? 4u : 5u;
      channelDisplayReady = renderer_->displayCompositeChannel(
          blendPipeline_.get(), channelComponentSource,
          renderPipeline.tempUAV(), mode, 0u, 1u,
          renderPipeline.width(), renderPipeline.height());
    }
    if (channelDisplayReady) {
      viewportChannelDisplaySRV_ = renderPipeline.tempSRV();
    }
  }
  // P1-5: exposure takes precedence over the channel display surface. Both
  // write to tempUAV(), so the exposed SRV is only valid when the channel
  // display stage did not overwrite tempSRV() in the block above; Color mode
  // (the only mode exposure runs in) never reaches that stage, so the two
  // cannot collide.
  Diligent::ITextureView* presentationSRV =
      exposedPresentSRV
          ? exposedPresentSRV
          : (channelDisplayReady && viewportChannelDisplaySRV_
                 ? viewportChannelDisplaySRV_.RawPtr()
                 : finalPresentSRV);

  if (presentationLayout_ == CompositionViewportPresentationLayout::Quad) {
    const float leftW = std::floor(origViewW * 0.5f);
    const float topH = std::floor(origViewH * 0.5f);
    const std::array<QRectF, 4> panes{
        QRectF(0.0, 0.0, leftW, topH),
        QRectF(leftW, 0.0, origViewW - leftW, topH),
        QRectF(0.0, topH, leftW, origViewH - topH),
        QRectF(leftW, topH, origViewW - leftW, origViewH - topH)};
    for (const QRectF& pane : panes) {
      if (pane.width() <= 0.0 || pane.height() <= 0.0) continue;
      renderer_->setViewportRect(
          static_cast<float>(pane.x()), static_cast<float>(pane.y()),
          static_cast<float>(pane.width()), static_cast<float>(pane.height()),
          origViewW, origViewH);
      renderer_->setCanvasSize(static_cast<float>(pane.width()),
                               static_cast<float>(pane.height()));
      renderer_->setZoom(1.0f);
      renderer_->setPan(0.0f, 0.0f);
      renderer_->drawSprite(0.0f, 0.0f, static_cast<float>(pane.width()),
                            static_cast<float>(pane.height()),
                            presentationSRV,
                            1.0f);
      // The hardware viewport/scissor is immediate state, while the sprite
      // commands are buffered. Drain each pane before switching state.
      renderer_->flush();
    }
    renderer_->setViewportRect(origViewW, origViewH);
  } else {
    renderer_->setCanvasSize(origViewW, origViewH);
    renderer_->setZoom(1.0f);
    renderer_->setPan(0.0f, 0.0f);
    renderer_->drawSprite(0.0f, 0.0f, origViewW, origViewH,
                          presentationSRV,
                          1.0f);
  }



  if (compositionRenderer_) {

    compositionRenderer_->SetCompositionSize(cw, ch);

    compositionRenderer_->ApplyCompositionSpace();

  } else {

    renderer_->setCanvasSize(cw, ch);

  }

  renderer_->setZoom(origZoom);

  renderer_->setPan(origPanX, origPanY);

  renderer_->setClearColor(origClearColor);

  return finalPresentSRV;

}

PresentStageResult CompositionRenderController::Impl::presentAndUpdateVideoLayers(

    const QList<ArtifactAbstractLayerPtr>& layers,

    const FramePosition& currentFrame, bool useRamPreviewFallback,

    const ArtifactRamPreviewFrameCacheState& playbackPreviewState,

    const QString& ramPreviewFallbackReason) {

  PresentStageResult result;

  const bool samplePresentStages = profilePresentStages_ &&
      presentStageProfileCount_ < 64 &&
      ArtifactPlaybackService::instance() && ArtifactPlaybackService::instance()->isPlaying();
  const auto presentStarted = samplePresentStages
      ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  static const bool captureRamPreviewBeforePresent = [] {
    return qEnvironmentVariableIsSet(
               "ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") &&
           qEnvironmentVariable(
               "ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") !=
               QStringLiteral("0");
  }();
  if (captureRamPreviewBeforePresent && useRamPreviewFallback) {
    renderer_->flushAndWait();
    const QImage diagnosticFrame = renderer_->readbackToImage();
    const QString captureDirectory = qEnvironmentVariable(
        "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
    if (!diagnosticFrame.isNull() && !captureDirectory.isEmpty()) {
      QDir().mkpath(captureDirectory);
      const QString capturePath = QDir(captureDirectory).filePath(
          QStringLiteral("composition_controller_ram_preview_before_present.png"));
      if (!diagnosticFrame.save(capturePath, "PNG")) {
        std::fprintf(stderr,
            "[VideoPreviewFallbackCapture] save-failed path=%s\n",
            capturePath.toUtf8().constData());
      }
    } else {
      std::fprintf(stderr,
          "[VideoPreviewFallbackCapture] readback-failed size=%dx%d\n",
          diagnosticFrame.width(), diagnosticFrame.height());
    }
  }
  renderer_->present();
  const auto presentFinished = samplePresentStages
      ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  if (gpuTextureCacheManager_) gpuTextureCacheManager_->endFrame();

  result.presentedGpuFrameMs = renderer_->lastFrameGpuTimeMs();

  result.presentedStatus = renderer_->lastPresentStatus();
  lastPresentedRamPreviewFrame_ =
      useRamPreviewFallback && result.presentedStatus == QStringLiteral("ok")
          ? currentFrame.framePosition() : -1;
  if (lastPresentedRamPreviewFrame_ >= 0) {
    if (const auto comp = previewPipeline_.composition())
      lastPresentedRamPreviewComposition_ = comp->id();
    else
      lastPresentedRamPreviewFrame_ = -1;
  }
  if (auto *playback = ArtifactPlaybackService::instance();
      playback && playback->isPlaying() &&
      result.presentedStatus == QStringLiteral("ok")) {
    playback->recordPresentedFrame(currentFrame.framePosition());
  }

  for (const auto& layer : layers) {

    if (auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer.get())) {

      videoLayer->markFramePresented(layer->currentFrame(),

                                     result.presentedGpuFrameMs,

                                     result.presentedStatus);

      if (result.presentedVideoDebug.isEmpty() &&

          isLayerEffectivelyVisible(layer) &&

          layer->isActiveAt(currentFrame)) {

        result.presentedVideoDebug =

            QStringLiteral("[Video] phase=present compositionCache=%1 "

                           "compositionCacheReason=%2 ")

                .arg(useRamPreviewFallback

                         ? (playbackPreviewState.onDisk

                                ? QStringLiteral("disk-hydrated-ram")

                                : QStringLiteral("ram"))

                         : QStringLiteral("live"))

                .arg(ramPreviewFallbackReason) +

            videoLayer->decodeState();

      }

    }

  }

  if (samplePresentStages) {
    const auto bookkeepingFinished = std::chrono::steady_clock::now();
    std::fprintf(stderr,
        "[VideoPreviewPresentStageProfile] frame=%lld presentMs=%.3f bookkeepingMs=%.3f\n",
        static_cast<long long>(currentFrame.framePosition()),
        std::chrono::duration<double, std::milli>(presentFinished - presentStarted).count(),
        std::chrono::duration<double, std::milli>(bookkeepingFinished - presentFinished).count());
    ++presentStageProfileCount_;
  }
  return result;

}

} // namespace Artifact
