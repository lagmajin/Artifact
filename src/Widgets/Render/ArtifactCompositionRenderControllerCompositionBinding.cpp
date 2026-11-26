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

void CompositionRenderController::setComposition(

    ArtifactCompositionPtr composition) {

  if (impl_->gizmoSession_.owner != GizmoOwner::None &&
      composition != impl_->previewPipeline_.composition()) {
    cancelGizmoInteraction();
  }

  if (trackerJobRunning() &&
      composition != impl_->previewPipeline_.composition()) {
    trackerStop();
    impl_->trackerSolveFuture_.waitForFinished();
    impl_->trackerSolveWorkingCopy_.reset();
    impl_->trackerPreSolveJson_.clear();
    ++impl_->trackerJobGeneration_;
    impl_->trackerCaptureActive_ = false;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
  }

  qCDebug(compositionViewLog) << "[CompositionView] setComposition"

                              << "isNull=" << (composition == nullptr) << "id="

                              << (composition ? composition->id().toString()

                                              : QStringLiteral("<null>"));

  // P0-4: invalidate the per-layer category cache so the next frame
  // re-classifies layers belonging to the new composition. Safe even
  // when the composition pointer is unchanged (LayerID set is the same).
  impl_->layerCategoryCache_.clear();



  auto currentComposition = impl_->previewPipeline_.composition();

  const bool samePointer = (currentComposition == composition);

  const bool sameId = !samePointer && composition && currentComposition &&

                      composition->id() == currentComposition->id();

  // TrackerManager sessions are project-owned. Register the active session
  // against the composition before detaching the controller, so switching
  // views does not delete a saved analysis or leave its pointer bound to the
  // wrong composition.
  if (!samePointer && impl_->trackerMotionTracker_) {
    const bool trackerStillRegistered =
        ArtifactCore::TrackerManager::instance().tracker(
            impl_->trackerMotionTrackerId_) == impl_->trackerMotionTracker_;
    if (currentComposition && trackerStillRegistered) {
      registerProjectTrackerSession(currentComposition->id(),
                                    impl_->trackerMotionTrackerId_);
    }
    trackerDetachSession();
  }

  if (!samePointer) {
    impl_->trackerSourceLayerId_ = LayerID();
    impl_->trackerCaptureSourceLayerId_ = LayerID();
    impl_->trackerMaskIndex_ = 0;
    impl_->trackerMaskPathIndex_ = 0;
  }

  if (!samePointer && !sameId) {
    if (const auto layer = impl_->physicsDragLayer_.lock()) {
      layer->endRigidBodyMouseDrag();
    }
    impl_->physicsDragLayer_.reset();
  }



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

      impl_->sync2DGizmosForLayer(

          composition->layerById(impl_->selectedLayerId_));

    } else if (!composition) {

      impl_->sync2DGizmosForLayer(nullptr);

    }

    if (sameId) {

      // Pointer changed but same composition — re-bind changed signals

      // The render key only stores the composition ID, so a replacement
      // instance with the same ID would otherwise be treated as an unchanged
      // frame. Its layer objects and cached surfaces belong to the old
      // instance and must not be reused.

      impl_->surfaceCache_.clear();

      impl_->solidPointwiseCache_.clear(impl_->renderer_.get());

      impl_->surfaceGenerations_.clear();

      if (impl_->gpuTextureCacheManager_) {

        impl_->gpuTextureCacheManager_->clear();

      }

      impl_->invalidateBaseComposite();

      impl_->resetOnionSkinHistory();

      impl_->previewPipeline_.setComposition(composition);

      impl_->resetLayerDamageBounds(composition);

      impl_->bindCompositionChanged(this, composition);

    }

    if (impl_->renderTickDriver_) {

      impl_->renderTickDriver_->setInterval(std::chrono::milliseconds(

          compositionRenderDispatchIntervalMs(impl_->previewPipeline_.composition())));

    }

    if (!impl_->trackerMotionTracker_) {
      trackerRestoreCompositionSession();
    }

    markRenderDirty();

    return;

  }

  if (impl_->stateCompareSessionActive_ && currentComposition) {
    currentComposition->setActiveStateVariantId(
        impl_->compareRestoreStateId_);
  }
  impl_->stateCompareSessionActive_ = false;
  impl_->compareRestoreStateId_.clear();
  impl_->compareMode_ = CompositionCompareMode::Off;
  impl_->layerRenderFilter_ = CompositionLayerRenderFilter::All;



  for (auto &connection : impl_->layerChangedConnections_) {

    disconnect(connection);

  }

  impl_->layerChangedConnections_.clear();

  impl_->compositionChangedSubscription_.disconnect();

  impl_->surfaceCache_.clear();

  impl_->solidPointwiseCache_.clear(impl_->renderer_.get());

  impl_->clearPrecompGpuOutputs();

  impl_->clearMatteGpuOutputs();

  impl_->surfaceGenerations_.clear();

  if (impl_->gpuTextureCacheManager_) {

    impl_->gpuTextureCacheManager_->clear();

  }

  impl_->invalidateBaseComposite();

  impl_->resetOnionSkinHistory();



  impl_->previewPipeline_.setComposition(composition);

  trackerRestoreCompositionSession();

  impl_->resetLayerDamageBounds(composition);

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

    if (impl_->renderTickDriver_) {

      impl_->renderTickDriver_->setInterval(

          std::chrono::milliseconds(compositionRenderDispatchIntervalMs(composition)));

    }

    // コンポジションがセットされた瞬間に1フレーム描画

    markRenderDirty();

  } else if (!composition) {

    impl_->syncSelectedLayerOverlayState(composition);

    if (impl_->renderTickDriver_) {

      impl_->renderTickDriver_->setInterval(

          std::chrono::milliseconds(compositionRenderDispatchIntervalMs(nullptr)));

    }

    markRenderDirty();

  }

}



ArtifactCompositionPtr CompositionRenderController::composition() const {

  return impl_->previewPipeline_.composition();

}

void CompositionRenderController::setSelectedLayerId(const LayerID &id) {
  if (impl_->selectedLayerId_ != id &&
      impl_->gizmoSession_.owner != GizmoOwner::None) {
    cancelGizmoInteraction();
  }

  // A newly selected spatial layer must always enter the viewport in the
  // complete transform mode.  Otherwise a mode left by the previous tool can
  // leave volume models with no visible universal gizmo at all.
  if (const auto composition = impl_->previewPipeline_.composition();
      composition && !id.isNil()) {
    const auto selectedLayer = composition->layerById(id);
    if (selectedLayer &&
        (selectedLayer->is3D() || layerUsesProjectedFrameGizmo(selectedLayer))) {
      setGizmoMode(TransformGizmo::Mode::All);
    }
  }

  if (!impl_->pendingFullGizmoLayerId_.isNil()) {
    if (id == impl_->pendingFullGizmoLayerId_) {
      setGizmoMode(TransformGizmo::Mode::All);
      impl_->pendingFullGizmoLayerId_ = LayerID();
    }
  }

  if (impl_->selectedLayerId_ == id) {
    impl_->invalidateOverlayComposite();
    impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());
    markRenderDirty();
    return;

  }

  impl_->clearPendingMaskCreation();

  impl_->selectedLayerId_ = id;
  impl_->trackerMaskIndex_ = 0;
  impl_->trackerMaskPathIndex_ = 0;
  impl_->selectedMotionPathFrames_.clear();
  impl_->draggingMotionPathGroupBefore_.clear();

  impl_->invalidateOverlayComposite();

  impl_->syncSelectedLayerOverlayState(impl_->previewPipeline_.composition());

  // A freshly inserted plane can acquire its final local bounds while the
  // first base composite is prepared.  Rebuild that base together with the
  // selection overlay so its projected frame is valid on the first draw,
  // without requiring a later camera/view-cube change to wake it up.
  if (const auto composition = impl_->previewPipeline_.composition();
      composition && !id.isNil()) {
    const auto selectedLayer = composition->layerById(id);
    if (layerUsesProjectedFrameGizmo(selectedLayer)) {
      impl_->invalidateBaseComposite();
    }
  }



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



  markRenderDirty();

}

void CompositionRenderController::Impl::applyCompositionState(const ArtifactCompositionPtr &composition) {

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



  // NOTE: previewRenderPipelineSlots_ は renderOneFrameImpl() で

  // ビューポートの実サイズ (rcw, rch) を使って初期化される。ここで

  // コンポジションサイズ (cw, ch) で初期化すると、サイズが異なる場合に

  // 未初期化の D3D12 テクスチャが生成され、次フレームで再び不一致が起き、

  // ゴミデータが画面に出ることがある。サイズ管理は renderOneFrameImpl() に

  // 一元化する。

}

void CompositionRenderController::Impl::bindCompositionChanged(CompositionRenderController *owner,

                            const ArtifactCompositionPtr &composition) {

  // Layer change notifications are now handled exclusively via

  // CompositionChangedEvent.

  compositionChangedSubscription_.disconnect();
  trackerUndoSubscription_.disconnect();

  if (!owner || !composition) {

    return;

  }

  const auto reserveTransitionWorkspace = [this](
      const ArtifactCompositionPtr& target) {
    if (!target) return;
    const auto required = static_cast<std::size_t>(
        target->timelineTransitionCount());
    if (transitionEvaluationWorkspace_.capacity() < required) {
      transitionEvaluationWorkspace_.reserve(required);
    }
    const auto requiredSteps = required +
        static_cast<std::size_t>(target->allLayerRef().size());
    if (transitionDrawStepWorkspace_.capacity() < requiredSteps) {
      transitionDrawStepWorkspace_.reserve(requiredSteps);
    }
  };
  reserveTransitionWorkspace(composition);

  {

    const QString compositionId = composition->id().toString();

    trackerUndoSubscription_ = eventBus_.subscribe<UndoManagerChangedEvent>(
        [owner](const UndoManagerChangedEvent&) {
          if (!owner || owner->trackerJobRunning()) return;
          if (owner->trackerSessionSelectionOutOfDate()) {
            owner->trackerRestoreCompositionSession();
          }
        });

    compositionChangedSubscription_ =

        eventBus_.subscribe<CompositionChangedEvent>(

            [this, owner, composition, compositionId](const CompositionChangedEvent &event) {

              if (event.compositionId != compositionId) {

                return;

              }

              const auto required = static_cast<std::size_t>(
                  composition->timelineTransitionCount());
              if (transitionEvaluationWorkspace_.capacity() < required) {
                transitionEvaluationWorkspace_.reserve(required);
              }
              const auto requiredSteps = required +
                  static_cast<std::size_t>(composition->allLayerRef().size());
              if (transitionDrawStepWorkspace_.capacity() < requiredSteps) {
                transitionDrawStepWorkspace_.reserve(requiredSteps);
              }

              sceneMutationGeneration_.fetch_add(
                  1, std::memory_order_acq_rel);

              resetOnionSkinHistory();

              applyCompositionState(composition);

              invalidateBaseComposite();

              invalidateOverlayComposite();

              // Composition-level effects do not belong to a single layer
              // range. Keep their invalidation conservative until an
              // effect-specific temporal range is part of the model.
              if (auto *playback = ArtifactPlaybackService::instance()) {
                playback->invalidateRamPreviewCache(
                    QStringLiteral("composition-effect-or-state-changed"));
              }

              owner->markRenderDirty();

            });

  }

}

void CompositionRenderController::Impl::invalidateLayerSurfaceCache(const ArtifactAbstractLayerPtr &layer) {

  if (!layer) {

    return;

  }

  const QString ownerId = layer->id().toString();

  if (layerSurfaceCacheDiagnosticsEnabled_) {
    const QString fullKey = ownerId + QStringLiteral("|full");
    const QString draftKey = ownerId + QStringLiteral("|draft");
    if (surfaceCache_.contains(fullKey)) {
      ++layerSurfaceCacheInvalidationCounts_[fullKey];
    }
    if (surfaceCache_.contains(draftKey)) {
      ++layerSurfaceCacheInvalidationCounts_[draftKey];
    }
  }

  surfaceGenerations_[ownerId] =

      surfaceGenerations_.value(ownerId, 1) + 1;

  surfaceCache_.remove(ownerId + QStringLiteral("|full"));

  surfaceCache_.remove(ownerId + QStringLiteral("|draft"));

  if (gpuTextureCacheManager_) {

    gpuTextureCacheManager_->invalidateOwner(ownerId);

  }

  recordLayerDamage(layer, LayerInvalidationRegion::Source::Content);

}

void CompositionRenderController::Impl::recordLayerDamage(

    const ArtifactAbstractLayerPtr &layer,

    LayerInvalidationRegion::Source source) {

  if (!layer) {

    return;

  }

  const QString ownerId = layer->id().toString();

  const QRectF newBounds = effectExpandedLayerBounds(layer.get());

  const QRectF oldBounds = lastLayerDamageBounds_.value(ownerId);

  const auto hasFiniteBounds = [](const QRectF& bounds) {
    return std::isfinite(bounds.x()) && std::isfinite(bounds.y()) &&
           std::isfinite(bounds.width()) &&
           std::isfinite(bounds.height()) &&
           std::isfinite(bounds.right()) &&
           std::isfinite(bounds.bottom()) && bounds.width() >= 0.0 &&
           bounds.height() >= 0.0;
  };

  LayerInvalidationRegion region;

  region.source = source;

  region.layerId = ownerId;

  region.frameNumber = layer->currentFrame();

  // Include both placements so transform edits repaint the vacated area as
  // well as the new one. Effect bounds include shared Blur/Glow expansion.

  region.region = oldBounds.isEmpty() ? newBounds
                                      : oldBounds.united(newBounds);

  lastLayerDamageBounds_[ownerId] = newBounds;

  if (source == LayerInvalidationRegion::Source::Property &&
      layerHasEnabledMatteReferences(layer.get())) {
    compositionHasMatteDependencies_ = true;
  }

  // Until dependency-aware ROI calculation exists, these layer contracts
  // conservatively invalidate the whole composition.
  region.requiresFullRedraw =
      !hasFiniteBounds(oldBounds) || !hasFiniteBounds(newBounds) ||
      layer->is3D() || layer->isAdjustmentLayer() || layer->hasMasks() ||
      layer->hasModifiers() || compositionHasMatteDependencies_ ||
      layer->layerBlendType() !=
          ArtifactCore::LAYER_BLEND_TYPE::BLEND_NORMAL;

  // A full-frame effect remains conservative until the composition host can
  // supply its canvas extent to the layer-level bounds query.
  region.requiresFullRedraw = region.requiresFullRedraw ||
      layer->hasEnabledFullFrameEffect();

  damageTracker_.markDirty(ownerId, region);

  for (auto& slot : previewRenderPipelineSlots_) {
    slot.damageTracker.markDirty(ownerId, region);
  }

}

void CompositionRenderController::Impl::resetLayerDamageBounds(const ArtifactCompositionPtr& composition) {

  lastLayerDamageBounds_.clear();

  compositionHasMatteDependencies_ = false;

  layerDamageBoundsInitialized_ = true;

  if (!composition) {

    return;

  }

  for (const auto& layer : composition->allLayerRef()) {

    if (layer) {

      compositionHasMatteDependencies_ =
          compositionHasMatteDependencies_ ||
          layerHasEnabledMatteReferences(layer.get());

      lastLayerDamageBounds_.insert(
          layer->id().toString(), effectExpandedLayerBounds(layer.get()));

    }

  }

}

void CompositionRenderController::Impl::invalidateBaseComposite(bool preserveLayerDamage,
                             bool preserveViewportTransform) {

  ++baseInvalidationSerial_;

  const float previousZoom = lastRenderKeyState_.zoom;
  const float previousPanX = lastRenderKeyState_.panX;
  const float previousPanY = lastRenderKeyState_.panY;
  lastRenderKeyState_ = {};
  if (preserveViewportTransform) {
    // Frame readiness invalidates content, not the user's view. Losing
    // these values falsely disables RAM presentation as a changing view.
    lastRenderKeyState_.zoom = previousZoom;
    lastRenderKeyState_.panX = previousPanX;
    lastRenderKeyState_.panY = previousPanY;
  }

  if (!preserveLayerDamage) {

    damageTracker_.clearAll();

    markAllPreviewSlotsFullRedraw();

    fullRedrawPending_ = true;

  }

}

void CompositionRenderController::Impl::invalidateOverlayComposite() {

  ++overlayInvalidationSerial_;

  lastRenderKeyState_ = {};

  invalidateMotionPathCache();

}

void CompositionRenderController::Impl::invalidateMotionPathCache() {

  // Clear both snapshots so a composition-wide or in-progress edit cannot
  // retain stale vectors for layers that are not visited in the next pass.
  motionPathCache_.valid = false;
  motionPathCacheByLayer_.clear();

}

void CompositionRenderController::Impl::scheduleFailedFrameRetry() {
  if (failedFrameRetryIssued_.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  renderDirty_.store(true, std::memory_order_release);
  if (running_ && renderTickDriver_ && !renderTickDriver_->isRunning()) {
    renderTickDriver_->start();
  }
}

void CompositionRenderController::Impl::markAllPreviewSlotsFullRedraw() {
  for (auto& slot : previewRenderPipelineSlots_) {
    slot.damageTracker.clearAll();
    slot.damageTracker.markFullRedraw();
    slot.damageTileCursor = 0;
    slot.retainedCompositeValid = false;
  }
}

} // namespace Artifact
