module;

#define NOMINMAX
#define QT_NO_KEYWORDS

#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import :LayerDrawing;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Composition;
import Artifact.Layer.ParametricComposition;
import Artifact.Render.Config;
import Artifact.Render.IRenderer;
import Artifact.Render.Pipeline;
import Color.Float;
import Frame.Position;
import Graphics.LayerBlendPipeline;
import Layer.Blend;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

using namespace CompositionRenderControllerDetail;

void CompositionRenderController::Impl::clearPrecompGpuOutputs() {
  if (renderer_) {
    for (auto& entry : precompGpuOutputs_) {
      renderer_->destroyOffscreenTexture(entry.colorTargetView);
      renderer_->destroyOffscreenTexture(entry.depthTargetView);
    }
  }
  precompGpuOutputs_.clear();
}

void CompositionRenderController::Impl::clearMatteGpuOutputs() {
  if (renderer_) {
    for (auto& entry : matteGpuOutputs_) {
      renderer_->destroyOffscreenTexture(entry.colorTargetView);
      renderer_->destroyOffscreenTexture(entry.depthTargetView);
    }
  }
  matteGpuOutputs_.clear();
}

Diligent::ITextureView* CompositionRenderController::Impl::renderMatteGpuOutput(
    ArtifactAbstractLayer* sourceLayer, qint64 frame, const QSize& size,
    float restoreViewportW, float restoreViewportH, float canvasW,
    float canvasH, Diligent::ITextureView* accumSRV,
    const std::function<QImage(const ArtifactCore::Id&)>& matteResolver,
    const std::vector<SceneLightEntry>& sceneLights, bool has3DCamera,
    const QMatrix4x4& cameraViewMatrix, const QMatrix4x4& cameraProjMatrix,
    RenderPipeline* renderPipeline) {
  if (!renderer_ || !sourceLayer || size.width() <= 0 ||
      size.height() <= 0) {
    return nullptr;
  }

  const quint64 generation = surfaceGeneration(sourceLayer);
  auto& entry = matteGpuOutputs_[sourceLayer->id()];
  if (entry.ready && entry.size == size && entry.frame == frame &&
      entry.generation == generation && entry.colorShaderResourceView) {
    return entry.colorShaderResourceView;
  }

  if (entry.size != size || !entry.colorTargetView ||
      !entry.depthTargetView || !entry.colorShaderResourceView) {
    renderer_->destroyOffscreenTexture(entry.colorTargetView);
    renderer_->destroyOffscreenTexture(entry.depthTargetView);
    entry = {};
    entry.colorTargetView =
        renderer_->createOffscreenTexture(size.width(), size.height());
    entry.depthTargetView =
        renderer_->createOffscreenDepthTexture(size.width(), size.height());
    entry.colorShaderResourceView = renderer_->offscreenTextureShaderResourceView(
        entry.colorTargetView);
    entry.size = size;
  }
  if (!entry.colorTargetView || !entry.depthTargetView ||
      !entry.colorShaderResourceView) {
    renderer_->destroyOffscreenTexture(entry.colorTargetView);
    renderer_->destroyOffscreenTexture(entry.depthTargetView);
    entry = {};
    return nullptr;
  }

  const float savedZoom = renderer_->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  renderer_->getPan(savedPanX, savedPanY);
  renderer_->setViewportRect(static_cast<float>(size.width()),
                             static_cast<float>(size.height()));
  renderer_->setCanvasSize(canvasW, canvasH);
  renderer_->setPan(0.0f, 0.0f);
  renderer_->setZoom(1.0f);
  sourceLayer->goToFrame(frame);
  entry.ready = false;

  drawGpuLayerToIntermediate(
      sourceLayer,
      static_cast<Diligent::ITextureView*>(entry.colorTargetView),
      static_cast<Diligent::ITextureView*>(entry.depthTargetView), accumSRV,
      restoreViewportW, restoreViewportH, canvasW, canvasH,
      DetailLevel::High, FramePosition(frame), matteResolver, sceneLights,
      has3DCamera, cameraViewMatrix, cameraProjMatrix, false, renderPipeline);

  renderer_->setViewportRect(restoreViewportW, restoreViewportH);
  renderer_->setCanvasSize(canvasW, canvasH);
  renderer_->setPan(savedPanX, savedPanY);
  renderer_->setZoom(savedZoom);
  entry.frame = frame;
  entry.generation = generation;
  entry.ready = true;
  return entry.colorShaderResourceView;
}

void CompositionRenderController::Impl::clearCompositionSpaceGpuCache() {
  if (renderer_ && compositionSpaceGpuCache_.colorTargetView) {
    renderer_->destroyOffscreenTexture(
        compositionSpaceGpuCache_.colorTargetView);
  }
  if (renderer_ && compositionSpaceGpuCache_.postProcessTargetView) {
    renderer_->destroyOffscreenTexture(
        compositionSpaceGpuCache_.postProcessTargetView);
  }
  compositionSpaceGpuCache_ = {};
}

bool CompositionRenderController::Impl::ensureCompositionSpaceGpuCache(const QSize& size) {
  if (!renderer_ || size.width() <= 0 || size.height() <= 0) {
    return false;
  }
  if (compositionSpaceGpuCache_.colorTargetView &&
      compositionSpaceGpuCache_.size == size &&
      compositionSpaceGpuCache_.colorShaderResourceView &&
      compositionSpaceGpuCache_.postProcessTargetView &&
      compositionSpaceGpuCache_.postProcessShaderResourceView &&
      compositionSpaceGpuCache_.postProcessUnorderedAccessView) {
    return true;
  }
  clearCompositionSpaceGpuCache();
  compositionSpaceGpuCache_.colorTargetView =
      renderer_->createOffscreenTexture(size.width(), size.height());
  compositionSpaceGpuCache_.colorShaderResourceView =
      renderer_->offscreenTextureShaderResourceView(
          compositionSpaceGpuCache_.colorTargetView);
  compositionSpaceGpuCache_.postProcessTargetView =
      renderer_->createOffscreenComputeTexture(size.width(), size.height());
  compositionSpaceGpuCache_.postProcessShaderResourceView =
      renderer_->offscreenTextureShaderResourceView(
          compositionSpaceGpuCache_.postProcessTargetView);
  compositionSpaceGpuCache_.postProcessUnorderedAccessView =
      renderer_->offscreenTextureUnorderedAccessView(
          compositionSpaceGpuCache_.postProcessTargetView);
  compositionSpaceGpuCache_.size = size;
  if (!compositionSpaceGpuCache_.colorTargetView ||
      !compositionSpaceGpuCache_.colorShaderResourceView ||
      !compositionSpaceGpuCache_.postProcessTargetView ||
      !compositionSpaceGpuCache_.postProcessShaderResourceView ||
      !compositionSpaceGpuCache_.postProcessUnorderedAccessView) {
    clearCompositionSpaceGpuCache();
    return false;
  }
  return true;
}

CompositionRenderController::Impl::PrecompGpuOutputEntry* CompositionRenderController::Impl::preparePrecompGpuOutput(
    const QString& compositionId, qint64 frame, const QSize& size,
    quint64 generation) {
  if (!renderer_ || compositionId.isEmpty() || size.width() <= 0 ||
      size.height() <= 0) {
    return nullptr;
  }
  auto& entry = precompGpuOutputs_[compositionId];
  if (entry.size != size || !entry.colorTargetView ||
      !entry.depthTargetView || !entry.colorShaderResourceView) {
    renderer_->destroyOffscreenTexture(entry.colorTargetView);
    renderer_->destroyOffscreenTexture(entry.depthTargetView);
    entry = {};
    entry.colorTargetView =
        renderer_->createOffscreenTexture(size.width(), size.height());
    entry.depthTargetView =
        renderer_->createOffscreenDepthTexture(size.width(), size.height());
    entry.colorShaderResourceView =
        renderer_->offscreenTextureShaderResourceView(entry.colorTargetView);
    entry.size = size;
  }
  entry.frame = frame;
  entry.generation = generation;
  entry.ready = false;
  return entry.colorTargetView && entry.depthTargetView &&
                 entry.colorShaderResourceView
             ? &entry
             : nullptr;
}

Diligent::ITextureView* CompositionRenderController::Impl::renderPrecomp2DGpuOutput(
    ArtifactAbstractLayer* containerLayer, qint64 childFrame,
    const QSize& childSize, float restoreViewportW, float restoreViewportH,
    float restoreCanvasW, float restoreCanvasH) {
  if (!containerLayer || !renderer_ || !gpuTextureCacheManager_) return nullptr;
  auto* precompLayer = dynamic_cast<ArtifactCompositionLayer*>(containerLayer);
  auto* parametricLayer = dynamic_cast<ArtifactParametricCompositionLayer*>(containerLayer);
  const auto child = precompLayer ? precompLayer->sourceComposition()
                                  : (parametricLayer ? parametricLayer->sourceComposition() : nullptr);
  if (!child || childSize.isEmpty()) return nullptr;
  const auto layers = child->allLayerRef();
  for (const auto& layer : layers) {
    if (!layer || layer->is3D() || layer->isAdjustmentLayer() ||
        layer->maskCount() != 0 || layer->layerBlendType() !=
            ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL ||
        layerHasRasterizerEffectsOrMasks(layer.get()) ||
        dynamic_cast<ArtifactCompositionLayer*>(layer.get()) != nullptr) {
      ++precompFastPathSkipCount_;
      return nullptr;
    }
  }
  const QString outputOwnerId = parametricLayer
      ? child->id().toString() + QStringLiteral("|parametric|") +
            containerLayer->id().toString()
      : child->id().toString();
  auto* output = preparePrecompGpuOutput(
      outputOwnerId, childFrame, childSize, renderFrameCounter_);
  if (!output) return nullptr;
  const auto savedFrame = child->framePosition();
  const float savedZoom = renderer_->getZoom();
  float savedPanX = 0.0f, savedPanY = 0.0f;
  renderer_->getPan(savedPanX, savedPanY);
  child->goToFrame(childFrame);
  const bool scopeActive = precompLayer
      ? precompLayer->beginExposedPropertyOverrideScope()
      : parametricLayer->beginInputBindingScope();
  renderer_->pushRenderTarget(output->colorTargetView, output->depthTargetView);
  renderer_->setViewportRect(childSize.width(), childSize.height());
  renderer_->setCanvasSize(childSize.width(), childSize.height());
  renderer_->setPan(0.0f, 0.0f);
  renderer_->setZoom(1.0f);
  renderer_->clearRenderTarget(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
  renderer_->clearDepthRenderTarget(output->depthTargetView);
  for (const auto& layer : layers) {
    if (layer && child->shouldEvaluateLayer(layer->id()) &&
        layer->isActiveAt(FramePosition(childFrame)) && layer->isVisible() &&
        layer->shouldIncludeInFinalRender()) {
      drawLayerForCompositionView(layer.get(), renderer_.get(), 1.0f,
                                  &lastVideoDebug_, &surfaceCache_,
                                  &layerSurfaceCacheDiagnosticsEnabled_,
                                  &layerSurfaceCacheHitCounts_,
                                  &layerSurfaceCacheMissCounts_,
                                  &layerSurfaceCacheBuildCounts_,
                                  gpuTextureCacheManager_.get(), childFrame,
                                  true, DetailLevel::High,
                                  static_cast<const QMatrix4x4*>(nullptr));
    }
  }
  renderer_->flush();
  renderer_->popRenderTarget();
  renderer_->setViewportRect(restoreViewportW, restoreViewportH);
  renderer_->setCanvasSize(restoreCanvasW, restoreCanvasH);
  renderer_->setPan(savedPanX, savedPanY);
  renderer_->setZoom(savedZoom);
  if (scopeActive) {
    if (precompLayer) precompLayer->endExposedPropertyOverrideScope();
    else parametricLayer->endInputBindingScope();
  }
  child->goToFrame(savedFrame.framePosition());
  output->ready = true;
  return output->colorShaderResourceView;
}

void CompositionRenderController::Impl::scheduleBlendPipelineInitialization(
    CompositionRenderController* owner, int delayMs, const QString& reason) {
  if (!owner || !initialized_ || !gpuBlendEnabled_ || blendPipelineReady_ ||
      blendPipelineInitScheduled_ || blendPipelineInitInProgress_ ||
      blendPipelineInitAttemptCount_ >= kBlendPipelineMaxInitAttempts) {
    return;
  }

  blendPipelineInitScheduled_ = true;
  blendPipelineInitState_ = QStringLiteral("scheduled:%1").arg(reason);
  QTimer::singleShot(std::max(0, delayMs), owner,
                     [this, owner, reason,
                      generation = blendPipelineInitGeneration_]() {
    if (generation != blendPipelineInitGeneration_) {
      return;
    }
    blendPipelineInitScheduled_ = false;
    if (!initialized_ || !gpuBlendEnabled_ || blendPipelineReady_) {
      return;
    }

    auto* renderer = renderer_.get();
    if (!renderer) {
      blendPipelineInitState_ = QStringLiteral("waiting-for-device");
      scheduleBlendPipelineInitialization(
          owner, 250, QStringLiteral("device-not-ready"));
      return;
    }

    blendPipelineInitInProgress_ = true;
    if (!blendPipeline_) {
      blendPipeline_ = renderer->createLayerBlendPipeline();
    }
    if (!blendPipeline_) {
      blendPipelineInitInProgress_ = false;
      blendPipelineInitState_ = QStringLiteral("waiting-for-device");
      scheduleBlendPipelineInitialization(
          owner, 250, QStringLiteral("device-not-ready"));
      return;
    }

    ++blendPipelineInitAttemptCount_;
    blendPipelineInitState_ = QStringLiteral("initializing:%1").arg(reason);

    QElapsedTimer timer;
    timer.start();
    blendPipelineReady_ = blendPipeline_ && blendPipeline_->initialize() &&
                          blendPipeline_->ready();
    blendPipelineInitInProgress_ = false;

    if (blendPipelineReady_) {
      blendPipelineInitState_ = QStringLiteral("ready");
      qInfo() << "[CompositionView][Startup] blend pipeline init"
              << "attempt=" << blendPipelineInitAttemptCount_
              << "reason=" << reason << "ms=" << timer.elapsed();
      invalidateBaseComposite();
      owner->markRenderDirty();
      return;
    }

    blendPipeline_.reset();
    blendPipelineInitState_ = QStringLiteral("failed:%1/%2")
                                  .arg(blendPipelineInitAttemptCount_)
                                  .arg(kBlendPipelineMaxInitAttempts);
    qWarning() << "[CompositionView] LayerBlendPipeline initialization failed"
               << "attempt=" << blendPipelineInitAttemptCount_
               << "reason=" << reason;

    if (blendPipelineInitAttemptCount_ < kBlendPipelineMaxInitAttempts) {
      scheduleBlendPipelineInitialization(
          owner, 500 * blendPipelineInitAttemptCount_,
          QStringLiteral("retry-after-failure"));
    }
  });
}

CompositionRenderController::Impl::PreviewRenderPipelineSlot &CompositionRenderController::Impl::acquirePreviewRenderPipelineSlot() {

  ++previewRenderPipelineAcquireCount_;

  lastPreviewRenderPipelineAcquireHazard_ = false;

  int selectedIndex = -1;

  for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

    if (previewRenderPipelineSlots_[i].state ==

        PreviewRenderPipelineSlot::State::Free) {

      selectedIndex = i;

      lastPreviewRenderPipelineSlotAcquireReason_ =

          QStringLiteral("free-slot");

      ++previewRenderPipelineFreeAcquireCount_;

      previewRenderPipelineConsecutiveReuseCount_ = 0;

      break;

    }

  }



  if (selectedIndex < 0) {

    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

      const auto& slot = previewRenderPipelineSlots_[i];

      if (slot.state == PreviewRenderPipelineSlot::State::Retirable) {

        if (selectedIndex < 0 ||

            slot.lastSubmittedFrame <

                previewRenderPipelineSlots_[selectedIndex].lastSubmittedFrame) {

          selectedIndex = i;

        }

      }

    }

  }



  if (selectedIndex < 0) {

    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

      const auto& slot = previewRenderPipelineSlots_[i];

      if (slot.state == PreviewRenderPipelineSlot::State::Ready) {

        if (selectedIndex < 0 ||

            slot.lastSubmittedFrame <

                previewRenderPipelineSlots_[selectedIndex].lastSubmittedFrame) {

          selectedIndex = i;

        }

      }

    }

  }



  if (selectedIndex < 0) {

    quint64 oldestFrame = std::numeric_limits<quint64>::max();

    for (int i = 0; i < kPreviewRenderPipelineSlotCount; ++i) {

      const auto& slot = previewRenderPipelineSlots_[i];

      if (slot.lastSubmittedFrame < oldestFrame) {

        oldestFrame = slot.lastSubmittedFrame;

        selectedIndex = i;

      }

    }

  }



  if (selectedIndex < 0) {

    selectedIndex = 0;

  }



  if (lastPreviewRenderPipelineSlotAcquireReason_ !=

      QStringLiteral("free-slot")) {

    const auto reusedState =

        previewRenderPipelineSlots_[selectedIndex].state;

    lastPreviewRenderPipelineSlotAcquireReason_ =

        reusedState == PreviewRenderPipelineSlot::State::Retirable

            ? QStringLiteral("reuse-retirable-slot")

        : reusedState == PreviewRenderPipelineSlot::State::Ready

            ? QStringLiteral("reuse-ready-slot")

            : QStringLiteral("reuse-submitted-slot");

    if (reusedState == PreviewRenderPipelineSlot::State::Retirable) {

      ++previewRenderPipelineReuseRetirableCount_;

    } else if (reusedState == PreviewRenderPipelineSlot::State::Ready) {

      ++previewRenderPipelineReuseReadyCount_;

    } else {

      ++previewRenderPipelineReuseSubmittedCount_;

      lastPreviewRenderPipelineAcquireHazard_ = true;

    }

    ++previewRenderPipelineConsecutiveReuseCount_;

    previewRenderPipelineMaxConsecutiveReuseCount_ =

        std::max(previewRenderPipelineMaxConsecutiveReuseCount_,

                 previewRenderPipelineConsecutiveReuseCount_);

  }



  activePreviewRenderPipelineSlot_ = selectedIndex;

  auto &slot = previewRenderPipelineSlots_[activePreviewRenderPipelineSlot_];

  slot.lastSubmittedFrame = renderFrameCounter_;

  slot.state = PreviewRenderPipelineSlot::State::Submitted;

  return slot;

}

bool CompositionRenderController::Impl::ensurePreviewRenderPipelineDepthSlot(

    PreviewRenderPipelineSlot &slot,

    const int width,

    const int height) {

  if (!renderer_ || width <= 0 || height <= 0) {

    return false;

  }

  const QSize targetSize(width, height);

  if (slot.depthTargetView && slot.depthTargetSize == targetSize) {

    return true;

  }

  if (slot.depthTargetView) {

    renderer_->destroyOffscreenTexture(slot.depthTargetView);

    slot.depthTargetView = nullptr;

    slot.depthTargetSize = QSize();

  }

  slot.depthTargetView =

      renderer_->createOffscreenDepthTexture(width, height);

  if (!slot.depthTargetView) {

    return false;

  }

  slot.depthTargetSize = targetSize;

  return true;

}

bool CompositionRenderController::Impl::ensurePreviewRenderPipelineMsaaSlot(
    PreviewRenderPipelineSlot& slot, const int width, const int height) {
  if (!renderer_ || width <= 0 || height <= 0) return false;
  const QSize targetSize(width, height);
  if (slot.msaaColorTargetView && slot.msaaDepthTargetView &&
      slot.msaaTargetSize == targetSize) return true;
  if (slot.msaaColorTargetView) renderer_->destroyOffscreenTexture(slot.msaaColorTargetView);
  if (slot.msaaDepthTargetView) renderer_->destroyOffscreenTexture(slot.msaaDepthTargetView);
  slot.msaaColorTargetView = renderer_->createOffscreenMultisampleTexture(
      width, height, RenderConfig::MainRTVFormat, 4);
  slot.msaaDepthTargetView = renderer_->createOffscreenMultisampleDepthTexture(
      width, height, 4);
  slot.msaaTargetSize = slot.msaaColorTargetView && slot.msaaDepthTargetView
      ? targetSize : QSize();
  return !slot.msaaTargetSize.isEmpty();
}

} // namespace Artifact
