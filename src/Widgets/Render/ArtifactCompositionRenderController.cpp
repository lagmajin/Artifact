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


#include <wobjectimpl.h>



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



W_OBJECT_IMPL(CompositionRenderController)



using namespace CompositionRenderControllerDetail;

CompositionRenderController::CompositionRenderController(QObject *parent)

    : QObject(parent), impl_(new Impl()) {

  auto& config = ArtifactCore::LayeredConfigStore::instance();
  impl_->presentationLayout_ = config.valueBool(
      QStringLiteral("Viewport/PresentationLayout/Quad"), false)
      ? CompositionViewportPresentationLayout::Quad
      : CompositionViewportPresentationLayout::Single;
  impl_->showAudioWaveformOverlay_ = config.valueBool(
      QStringLiteral("Viewport/AudioWaveformOverlay"), true);
  impl_->showAudioSpectrumOverlay_ = config.valueBool(
      QStringLiteral("Viewport/AudioSpectrumOverlay"), true);
  // P1-5 display-only exposure. Defaults are the identity transform, so a
  // project without these keys presents exactly as before.
  impl_->viewportExposureEnabled_ = config.valueBool(
      QStringLiteral("Viewport/Exposure/Enabled"), true);
  impl_->viewportExposureGain_ = std::clamp(
      static_cast<float>(config.valueDouble(
          std::string_view("Viewport/Exposure/Gain"), 0.0)),
      -6.0f, 6.0f);
  impl_->viewportExposureGamma_ = std::clamp(
      static_cast<float>(config.valueDouble(
          std::string_view("Viewport/Exposure/Gamma"), 1.0)),
      0.0f, 5.0f);
  impl_->viewportExposureSaturation_ = std::clamp(
      static_cast<float>(config.valueDouble(
          std::string_view("Viewport/Exposure/Saturation"), 1.0)),
      0.0f, 4.0f);
  impl_->viewportClippingWarningsEnabled_ = config.valueBool(
      QStringLiteral("Viewport/ClippingWarnings/Enabled"), false);
  const float clippingUnderThreshold = static_cast<float>(config.valueDouble(
      std::string_view("Viewport/ClippingWarnings/UnderThreshold"), 0.01));
  impl_->viewportClippingUnderThreshold_ =
      std::isfinite(clippingUnderThreshold)
          ? std::clamp(clippingUnderThreshold, 0.0f, 1.0f)
          : 0.01f;
  const float clippingOverThreshold = static_cast<float>(config.valueDouble(
      std::string_view("Viewport/ClippingWarnings/OverThreshold"), 1.0));
  impl_->viewportClippingOverThreshold_ =
      std::isfinite(clippingOverThreshold)
          ? std::clamp(clippingOverThreshold, 0.0f, 4.0f)
          : 1.0f;
  impl_->showRigOverlay_ = config.valueBool(
      QStringLiteral("Viewport/RigOverlayVisible"), false);
  // P1-4: X-Ray toggle and its attenuation. Default opacity keeps the
  // previous hardcoded 0.35 behavior.
  impl_->showXRayOverlay_ = config.valueBool(
      QStringLiteral("Viewport/XRay/Enabled"), false);
  {
    const float xRayOpacity = static_cast<float>(config.valueDouble(
        std::string_view("Viewport/XRay/Opacity"), 0.35));
    impl_->xRayOverlayOpacity_ = std::isfinite(xRayOpacity)
        ? std::clamp(xRayOpacity, 0.0f, 1.0f)
        : 0.35f;
  }
  impl_->rigWeightRadius_ = static_cast<float>(config.valueDouble(
      std::string_view("Viewport/RigWeight/Radius"), 36.0));
  impl_->rigWeightOpacity_ = static_cast<float>(config.valueDouble(
      std::string_view("Viewport/RigWeight/Opacity"), 0.35));
  impl_->rigWeightFlow_ = static_cast<float>(config.valueDouble(
      std::string_view("Viewport/RigWeight/Flow"), 1.0));
  impl_->rigWeightRadius_ = std::clamp(impl_->rigWeightRadius_, 2.0f, 500.0f);
  impl_->rigWeightOpacity_ = std::clamp(impl_->rigWeightOpacity_, 0.01f, 1.0f);
  impl_->rigWeightFlow_ = std::clamp(impl_->rigWeightFlow_, 0.01f, 1.0f);
  {
    auto& config = ArtifactCore::LayeredConfigStore::instance();
    for (const auto kind : {LineDebugKind::RigBone,
                            LineDebugKind::RigControl,
                            LineDebugKind::RigSkin}) {
      const QString key = lineDebugVisibilitySettingKey(kind);
      const size_t index = lineDebugKindIndex(kind);
      if (!key.isEmpty() && index < impl_->lineDebugVisibility_.size()) {
        impl_->lineDebugVisibility_[index] = config.valueBool(
            key, impl_->lineDebugVisibility_[index]);
      }
    }
  }

  impl_->viewportClearColor_ =

      FloatColor{QColor(28, 40, 56).redF(), QColor(28, 40, 56).greenF(),

                 QColor(28, 40, 56).blueF(), 1.0f};

  impl_->gizmo_ = std::make_unique<TransformGizmo>();
  impl_->gizmo_->setAutoKeyPredicate(
      [](const ArtifactAbstractLayerPtr& layer, const QString& propertyPrefix) {
        return gizmoAutoKeyApplies(layer, propertyPrefix);
      });

  impl_->textGizmo_ = std::make_unique<TextGizmo>();

  impl_->contentGizmo_ = std::make_unique<ContentGizmo>();

  impl_->gizmo3D_ = std::make_unique<Artifact3DGizmo>(this);
  impl_->viewportOrientationNavigator_.snapTo(
      ArtifactCore::ViewOrientationHotspot::Front, true);

  impl_->trackerGizmo_ = std::make_unique<ArtifactPointTrackerGizmo>();
  {
    auto& config = ArtifactCore::LayeredConfigStore::instance();
    auto trackerState = impl_->trackerGizmo_->state();
    trackerState.innerHalfW = std::clamp(
        static_cast<float>(config.valueDouble(std::string_view("Viewport/TrackPoint/FeatureWidth"),
                                              trackerState.innerHalfW * 2.0) * 0.5),
        2.0f, 4096.0f);
    trackerState.innerHalfH = std::clamp(
        static_cast<float>(config.valueDouble(std::string_view("Viewport/TrackPoint/FeatureHeight"),
                                              trackerState.innerHalfH * 2.0) * 0.5),
        2.0f, 4096.0f);
    trackerState.outerHalfW = std::max(
        trackerState.innerHalfW + 2.0f,
        static_cast<float>(config.valueDouble(std::string_view("Viewport/TrackPoint/SearchWidth"),
                                              trackerState.outerHalfW * 2.0) * 0.5));
    trackerState.outerHalfH = std::max(
        trackerState.innerHalfH + 2.0f,
        static_cast<float>(config.valueDouble(std::string_view("Viewport/TrackPoint/SearchHeight"),
                                              trackerState.outerHalfH * 2.0) * 0.5));
    impl_->trackerGizmo_->setState(trackerState);
  }



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

              if (!impl_->gizmoSessionEnding_ &&
                  impl_->gizmoSession_.owner != GizmoOwner::None) {
                cancelGizmoInteraction();
              }

              if (impl_->selectedLayerId_ != incomingId) {

                impl_->clearPendingMaskCreation();

                // Most viewport selections arrive through this EventBus path
                // rather than setSelectedLayerId().  Keep the universal gizmo
                // mode in sync here as well, otherwise planes/images retain
                // the previous tool's None/partial mode and nothing is drawn.
                if (comp && !incomingId.isNil()) {
                  const auto incomingLayer = comp->layerById(incomingId);
                  if (incomingLayer &&
                      (incomingLayer->is3D() ||
                       layerUsesProjectedFrameGizmo(incomingLayer))) {
                    setGizmoMode(TransformGizmo::Mode::All);
                    // Match setSelectedLayerId: creation normally selects via
                    // EventBus, so the first frame needs a fresh composite too.
                    impl_->invalidateBaseComposite();
                  }
                }

                impl_->selectedLayerId_ = incomingId;

                impl_->projectedFrameHoverHandle_ =
                    TransformGizmo::HandleType::None;

                impl_->invalidateOverlayComposite();

              }

              impl_->syncSelectedLayerOverlayState(comp);

              markRenderDirty();

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

              // FrameReady changes decoded-resource availability, not the
              // authored scene snapshot. Keep that invalidation on its own
              // path so playback does not force full-scene packet recapture.
              if (event.changeType != LayerChangedEvent::ChangeType::FrameReady) {
                impl_->sceneMutationGeneration_.fetch_add(
                    1, std::memory_order_acq_rel);
              }

              if (event.changeType == LayerChangedEvent::ChangeType::FrameReady) {
                // Decode/upload completion changes availability, not authored
                // content. Refresh this frame while preserving the RAM build
                // and persistent preview entries for other timeline frames.
                const auto layer = comp->layerById(LayerID(event.layerId));
                if (layer && !impl_->renderQueueActive_) {
                  impl_->invalidateLayerSurfaceCache(layer);
                  impl_->changeDetector_.markLayerChanged(event.layerId);
                  impl_->invalidateBaseComposite(true, true);
                  // Decode availability cannot change a final RAM image
                  // already presented for this frame. Keep surface damage for
                  // the next live draw without another same-frame Present.
                  const auto* playback = ArtifactPlaybackService::instance();
                  const auto playbackComp = playback
                      ? playback->currentComposition() : nullptr;
                  const bool alreadyPresentedRamFrame =
                      playback && playback->isPlaying() && playbackComp &&
                      playbackComp->id() == comp->id() &&
                      impl_->lastPresentedRamPreviewComposition_ == comp->id() &&
                      !impl_->interactionBusy() &&
                      impl_->lastPresentedRamPreviewFrame_ ==
                          playback->currentFrame().framePosition() &&
                      playback->borrowRamPreviewFrameImage(
                          playback->currentFrame().framePosition());
                  if (!alreadyPresentedRamFrame) markRenderDirty();
                }
                return;
              }

              impl_->resetOnionSkinHistory();

              if (impl_->renderQueueActive_) {

                return;

              }

              // Bulk removal deliberately has no individual layer ID.
              // Do not send that sentinel through the UUID parser.
              const auto layerId = event.layerId.isEmpty()
                  ? LayerID(ArtifactCore::Id::Nil()) : LayerID(event.layerId);

              if (event.changeType == LayerChangedEvent::ChangeType::Created) {
                impl_->pendingFullGizmoLayerId_ = layerId;
                if (layerId == impl_->selectedLayerId_) {
                  impl_->pendingFullGizmoLayerId_ = LayerID();
                  setGizmoMode(TransformGizmo::Mode::All);
                }
              }

              bool invalidatesFinalPreview = false;
              bool hasScopedLayerDamage = false;
              if (event.changeType == LayerChangedEvent::ChangeType::Created ||

                  event.changeType == LayerChangedEvent::ChangeType::Removed) {

                // Structural change: full cache clear + composition state sync

                if (event.changeType ==

                    LayerChangedEvent::ChangeType::Removed) {

                  if (event.layerId.isEmpty() ||
                      layerId == impl_->selectedLayerId_) {
                    if (impl_->gizmoSession_.owner != GizmoOwner::None)
                      cancelGizmoInteraction();

                    impl_->selectedLayerId_ = LayerID();

                    impl_->projectedFrameHoverHandle_ =
                        TransformGizmo::HandleType::None;

                  }

                }

                impl_->surfaceCache_.clear();

                impl_->solidPointwiseCache_.clear(impl_->renderer_.get());

                impl_->surfaceGenerations_.clear();

                impl_->resetLayerDamageBounds(comp);

                if (impl_->gpuTextureCacheManager_) {

                  impl_->gpuTextureCacheManager_->clear();

               }

                impl_->applyCompositionState(comp);
                invalidatesFinalPreview = true;

              } else {

                // Property/transform modification: invalidate only this layer

                // ギズモドラッグ中は同じレイヤーのピクセルは変わらないので、

                // transform だけの更新では重いサーフェス再生成を避ける。

                if (auto layer = comp->layerById(layerId)) {

                  hasScopedLayerDamage = true;

                  // Property drags arrive through LayerChanged rather than a
                  // viewport mouse event. Treat effect edits as a short
                  // interaction window so large Blur radii use preview LOD,
                  // then let the existing idle transition request Full again.
                  if (layerId == impl_->selectedLayerId_ &&
                      layer->isDirty(LayerDirtyFlag::Effect)) {
                    notifyViewportInteractionActivity();
                  }

                  const bool directDragTarget =

                      impl_->isDraggingLayer_ &&

                      (layerId == impl_->selectedLayerId_ ||

                       std::any_of(

                           impl_->dragGroupLayers_.cbegin(),

                           impl_->dragGroupLayers_.cend(),

                           [&layerId](const ArtifactAbstractLayerPtr &candidate) {

                             return candidate && candidate->id() == layerId;

                           }));

                  // Transform, opacity, blend and visibility edits only change
                  // composition placement/state. Reuse the rendered layer
                  // surface and re-composite it instead of regenerating source
                  // pixels for every interactive property update.
                  const bool lightweightCompositeChange =

                      !(layer->isDirty(LayerDirtyFlag::Property) &&
                        dynamic_cast<ArtifactTextLayer *>(layer.get())) &&

                      !layer->isDirty(LayerDirtyFlag::Effect) &&

                      !layer->isDirty(LayerDirtyFlag::Mask) &&

                      !layer->isDirty(LayerDirtyFlag::Source) &&

                      (layer->isDirty(LayerDirtyFlag::Transform) ||

                       layer->isDirty(LayerDirtyFlag::Property) ||

                       layer->isDirty(LayerDirtyFlag::Visibility));

                  const bool skipCacheInvalidation =

                      lightweightCompositeChange || directDragTarget ||

                      (impl_->gizmoDragActive_ &&

                       layerId == impl_->selectedLayerId_);

                  // Source pixels can be reused for transform edits, but the
                  // cached final frames still contain the previous placement.
                  if (skipCacheInvalidation) {
                    const auto damageSource =
                        layer->isDirty(LayerDirtyFlag::Transform)
                            ? LayerInvalidationRegion::Source::Transform
                            : layer->isDirty(LayerDirtyFlag::Visibility)
                                  ? LayerInvalidationRegion::Source::Visibility
                                  : LayerInvalidationRegion::Source::Property;
                    impl_->recordLayerDamage(layer, damageSource);
                    invalidatesFinalPreview = true;
                  }

                  if (!skipCacheInvalidation) {

                    impl_->invalidateLayerSurfaceCache(layer);
                    invalidatesFinalPreview = true;
                    if (auto *playback = ArtifactPlaybackService::instance()) {
                      playback->invalidateRamPreviewRange(
                          ArtifactCore::FrameRange(layer->inPoint(),
                                                   layer->outPoint()),
                          QStringLiteral("layer-content-changed"));
                      // The RAM preview entry was invalidated precisely above;
                      // retain the conservative full-cache fallback only for
                      // structural or otherwise unscoped changes.
                      invalidatesFinalPreview = false;
                    }

                  }

                  impl_->changeDetector_.markLayerChanged(

                      layer->id().toString());

               }

              }

              impl_->invalidateBaseComposite(hasScopedLayerDamage);

              if (invalidatesFinalPreview) {
                if (auto *playback = ArtifactPlaybackService::instance()) {
                  playback->invalidateRamPreviewCache(
                      QStringLiteral("composition-content-changed"));
                }
              }

              impl_->invalidateOverlayComposite();

              // ギズモドラッグ中はオーバーレイ同期コストを省く（ドラッグ終了時に一括同期）

              if (!impl_->gizmoDragActive_) {

                impl_->syncSelectedLayerOverlayState(

                    impl_->previewPipeline_.composition());

              }

              // Coalesce rapid property changes on the fixed render tick.

              markRenderDirty();

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

  if (impl_) {
    impl_->trackerCancelRequested_.store(true, std::memory_order_release);
    impl_->trackerSolveFuture_.waitForFinished();
    if (const auto layer = impl_->physicsDragLayer_.lock()) {
      layer->endRigidBodyMouseDrag();
    }
  }

  destroy();

  delete impl_;

}



void CompositionRenderController::videoDebugMessage(const QString &msg) {
  ArtifactCore::globalEventBus().publish<TimelineDebugMessageEvent>(
      TimelineDebugMessageEvent{msg});
}

void CompositionRenderController::initialize(QWidget *hostWidget) {

  if (impl_->initialized_ || hostWidget == nullptr) {

    return;

  }



  impl_->hostWidget_ = hostWidget;

  impl_->startupTimer_.start();

  impl_->renderer_ = std::make_unique<ArtifactIRenderer>();

  impl_->renderer_->initialize(hostWidget);

  // P0-3b.0: initialize the owned RenderContext with the default Editor
  // preview mode. P0-3b.1 will start consuming setROI() in the render path.
  impl_->renderContext_.setMode(RenderMode::Editor);



  if (!impl_->renderer_->isInitialized()) {

    qWarning() << "[CompositionRenderController] renderer initialize failed for"

               << hostWidget << "size=" << hostWidget->size()

               << "DPR=" << hostWidget->devicePixelRatio();

    impl_->renderer_.reset();

    return;

  }

  impl_->compositionRenderer_ =

      std::make_unique<CompositionRenderer>(*impl_->renderer_);

  impl_->viewportColorPipeline_ = std::make_unique<ViewportColorPipeline>();

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



  // Precise render tick: clocked from ArtifactCore and marshalled back onto the

  // UI thread to keep swapchain / QWidget access thread-safe.

  impl_->renderTickDriver_ = std::make_unique<ArtifactCore::PreciseTicker>();

  impl_->renderTickDriver_->setInterval(std::chrono::milliseconds(

      compositionRenderDispatchIntervalMs(impl_->previewPipeline_.composition())));

  const bool profileRenderDispatch =
      qEnvironmentVariableIsSet("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") &&
      qEnvironmentVariable("ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP") != QStringLiteral("0");
  impl_->profilePresentStages_ = profileRenderDispatch;
  impl_->renderTickDriver_->setCallback([this, profileRenderDispatch]() {

    if (!impl_) return;
    if (profileRenderDispatch) {
      const auto requestedInterval = impl_->renderTickDriver_->interval();
      if (requestedInterval > std::chrono::milliseconds(2)) {
        impl_->renderDispatchProfileCount_ = 0;
        impl_->renderDispatchPreviousTick_ = {};
      } else if (impl_->renderDispatchProfileCount_ < 64) {
        const auto tickNow = std::chrono::steady_clock::now();
        if (impl_->renderDispatchPreviousTick_.time_since_epoch().count() != 0) {
          const double elapsedMs = std::chrono::duration<double, std::milli>(
              tickNow - impl_->renderDispatchPreviousTick_).count();
          std::fprintf(stderr, "[VideoPreviewDispatchProfile] requestedMs=%.3f actualMs=%.3f\n",
                       std::chrono::duration<double, std::milli>(requestedInterval).count(),
                       elapsedMs);
        }
        impl_->renderDispatchPreviousTick_ = tickNow;
        ++impl_->renderDispatchProfileCount_;
      }
    }

    if (!impl_ || impl_->renderTickPosted_.exchange(true, std::memory_order_acq_rel)) {

      return;

    }

    const bool profileActiveDispatch = profileRenderDispatch &&
        impl_->renderTickDriver_->interval() <= std::chrono::milliseconds(2);
    const auto dispatchPostedAt = profileActiveDispatch
        ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    QMetaObject::invokeMethod(

        this,

        [this, profileActiveDispatch, dispatchPostedAt]() {

          if (!impl_) {

            return;

          }

          const bool inspectDispatch = profileActiveDispatch &&
              (impl_->renderGuiDispatchProfileCount_ < 64 ||
               impl_->renderSlowGuiDispatchProfileCount_ < 32);
          const double callbackQueuedMs = inspectDispatch
              ? std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - dispatchPostedAt).count() : 0.0;
          const bool slowDispatch = inspectDispatch && callbackQueuedMs > 20.0 &&
              impl_->renderSlowGuiDispatchProfileCount_ < 32;
          const bool sampleDispatch = inspectDispatch &&
              (impl_->renderGuiDispatchProfileCount_ < 64 || slowDispatch);
          qint64 frameAtCallback = -1;
          if (sampleDispatch) {
            if (const auto dispatchComposition = impl_->previewPipeline_.composition()) {
              frameAtCallback = dispatchComposition->framePosition().framePosition();
            }
          }
          impl_->renderTickPosted_.store(false, std::memory_order_release);

          renderCrashTrace("tick-enter", impl_->renderFrameCounter_);

          if (impl_->viewportInteracting_ &&

              impl_->viewportInteractionElapsedTimer_.isValid() &&

              impl_->viewportInteractionElapsedTimer_.elapsed() >=

                  impl_->viewportInteractionIdleMs_) {

            renderCrashTrace("tick-finish-viewport-interaction",

                             impl_->renderFrameCounter_);

            finishViewportInteraction();

          }

          if (!impl_->initialized_ || !impl_->renderer_ || !impl_->running_) {

            renderCrashTrace("tick-skip-not-ready", impl_->renderFrameCounter_);

            return;

          }

          const bool animateResizeGhost =
              impl_->gizmo_ && impl_->gizmo_->isDragging() &&
              impl_->gizmo_->activeHandle() >=
                  TransformGizmo::HandleType::Scale_TL &&
              impl_->gizmo_->activeHandle() <=
                  TransformGizmo::HandleType::Scale_Center;
          if (animateResizeGhost) {
            impl_->renderDirty_.store(true, std::memory_order_release);
          }

          const auto hostSnapshot = impl_->captureHostFrameSnapshot(this);
          if (hostSnapshot.hasHost && !hostSnapshot.visible) {

            renderCrashTrace("tick-skip-host-hidden",

                             impl_->renderFrameCounter_);

            return;

          }

          // Experimental Fly / Walk navigation advances on the GUI tick so
          // movement speed is frame-rate independent. A moving camera keeps
          // the frame dirty, which keeps the tick alive until it stops.
          if (impl_->viewportNavigationMode_ !=
              CompositionRenderController::ViewportNavigationMode::Off) {
            const auto navigationNow = std::chrono::steady_clock::now();
            double navigationDelta = 0.0;
            if (impl_->viewportNavigationLastTick_.time_since_epoch().count() !=
                0) {
              navigationDelta = std::chrono::duration<double>(
                  navigationNow - impl_->viewportNavigationLastTick_)
                  .count();
            }
            impl_->viewportNavigationLastTick_ = navigationNow;
            if (impl_->advanceViewportNavigation(
                    std::clamp(navigationDelta, 0.0, 0.1))) {
              impl_->renderDirty_.store(true, std::memory_order_release);
            }
          }

          if (!impl_->renderDirty_.exchange(false, std::memory_order_acq_rel)) {

            const auto* idlePlayback = ArtifactPlaybackService::instance();
            // Keep the existing worker alive across playback frames. Restarting
            // it for each dirty frame creates/joins a thread on the GUI path.
            if (!impl_->viewportInteracting_ && impl_->renderTickDriver_ &&
                !(idlePlayback && idlePlayback->isPlaying())) {

              if (impl_->profilePresentStages_ && impl_->renderIdleStopProfileCount_ < 64) {
                const auto* playback = ArtifactPlaybackService::instance();
                std::fprintf(stderr, "[VideoPreviewIdleStopProfile] playing=%d frameSerial=%llu\n",
                    playback && playback->isPlaying(),
                    static_cast<unsigned long long>(impl_->renderFrameCounter_));
                ++impl_->renderIdleStopProfileCount_;
              }
              impl_->renderTickDriver_->stop();

            }

            return;

          }

          if (impl_->renderInProgress_) {

            impl_->renderDirty_.store(true, std::memory_order_release);

            renderCrashTrace("tick-skip-in-progress",

                             impl_->renderFrameCounter_);

            return;

          }

          struct RenderInProgressGuard {

            bool& flag;

            explicit RenderInProgressGuard(bool& f) : flag(f) { flag = true; }

            ~RenderInProgressGuard() { flag = false; }

          } renderGuard(impl_->renderInProgress_);

          // The engine can advance its clock before the service's GUI-thread
          // composition update is dispatched. Do not present a new cache frame
          // with old layer state or acknowledge it ahead of that update.
          // FrameChanged marks the renderer dirty after service synchronization.
          if (const auto* playback = ArtifactPlaybackService::instance();
              playback && playback->isPlaying() && !impl_->renderQueueActive_) {
            const auto composition = impl_->previewPipeline_.composition();
            const auto playbackComposition = playback->currentComposition();
            if (composition && playbackComposition &&
                composition->id() == playbackComposition->id() &&
                composition->framePosition().framePosition() !=
                    playback->currentFrame().framePosition()) {
              return;
            }
          }

          // A redraw of the same frame retains its temporal history. History
          // is otherwise only meaningful across a contiguous step. A
          // seek, a reverse step, or any other discontinuity makes the retained
          // frames the wrong neighbours, so drop them rather than letting a
          // temporal effect blend against a distant frame.  Forward playback
          // leaves history intact.
          //
          // The GPU-side slot pool lives in the per-frame RenderPipeline, whose
          // identity follows the composition/view being rendered; a composition
          // or view change therefore gets a fresh pool rather than needing an
          // explicit invalidate here.  Only the process-wide CPU sampler needs
          // to be told.
          std::int64_t temporalFrame = -1;
          bool temporalDiscontinuity = false;
          if (const auto* playback = ArtifactPlaybackService::instance()) {
            temporalFrame = playback->currentFrame().framePosition();
            temporalDiscontinuity =
                temporalFrame >= 0 && impl_->lastEvaluatedTemporalFrame_ >= 0 &&
                temporalFrame != impl_->lastEvaluatedTemporalFrame_ &&
                temporalFrame != impl_->lastEvaluatedTemporalFrame_ + 1;
            if (temporalDiscontinuity) {
              ArtifactEffectFrameSampler::instance().invalidateAll();
            }
            impl_->lastEvaluatedTemporalFrame_ = temporalFrame;
          }

          if (!impl_->prepareHostFrame(this, hostSnapshot)) {
            return;
          }

          renderCrashTrace("tick-render-begin", impl_->renderFrameCounter_);

          // Pre-roll: on a discontinuity the retained history was just dropped,
          // so a temporal effect would have no neighbour for this very frame.
          // Render exactly one frame of history first, then restore the
          // playhead, so frame N can sample N-1.  Deliberately a single frame
          // and not a range: a deeper lookback should be amortised by the
          // caller (RenderFarm / final render), not paid on every scrub.
          if (temporalDiscontinuity && temporalFrame > 0) {
            if (auto composition = impl_->previewPipeline_.composition()) {
              const auto savedFrame =
                  composition->framePosition().framePosition();
              composition->goToFrame(temporalFrame - 1);
              impl_->renderOneFrameImpl(this, hostSnapshot, false);
              composition->goToFrame(savedFrame);
            }
          }

          if (sampleDispatch) {
            std::fprintf(stderr,
                "[VideoPreviewGuiDispatchProfile] queuedMs=%.3f frameAtCallback=%lld frameSerialBefore=%llu\n",
                callbackQueuedMs, static_cast<long long>(frameAtCallback),
                static_cast<unsigned long long>(impl_->renderFrameCounter_));
            if (impl_->renderGuiDispatchProfileCount_ < 64)
              ++impl_->renderGuiDispatchProfileCount_;
            if (slowDispatch) ++impl_->renderSlowGuiDispatchProfileCount_;
          }
          const bool profileRenderDuration = impl_->profilePresentStages_ &&
              impl_->renderDurationProfileCount_ < 64 &&
              ArtifactPlaybackService::instance() && ArtifactPlaybackService::instance()->isPlaying();
          const auto renderStarted = profileRenderDuration
              ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
          qint64 frameAtRenderStart = -1;
          if (profileRenderDuration) {
            if (const auto renderComposition = impl_->previewPipeline_.composition()) {
              frameAtRenderStart = renderComposition->framePosition().framePosition();
            }
          }
          impl_->renderOneFrameImpl(this, hostSnapshot, false);
          if (profileRenderDuration) {
            std::fprintf(stderr, "[VideoPreviewRenderDurationProfile] frameSerial=%llu frameAtRenderStart=%lld totalMs=%.3f\n",
                static_cast<unsigned long long>(impl_->renderFrameCounter_),
                static_cast<long long>(frameAtRenderStart),
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - renderStarted).count());
            ++impl_->renderDurationProfileCount_;
          }

          renderCrashTrace("tick-render-end", impl_->renderFrameCounter_);

        },

        Qt::QueuedConnection);

  });

  impl_->renderTickDriver_->start();



  // PlaybackService のフレーム変更に合わせて再描画

  impl_->eventBusSubscriptions_.push_back(

      impl_->eventBus_.subscribe<FrameChangedEvent>(

          [this](const FrameChangedEvent &) {

            // 可視性チェック: 非表示（他タブの裏など）なら描画しない

            if (auto *owner = qobject_cast<QWidget *>(parent())) {

              if (!owner->isVisible())

                return;

            }

            // comp->goToFrame() は ArtifactPlaybackService::syncCurrentCompositionFrame()

            // が publishFrame より先に投入済みのため、ここで重複呼び出しは不要。

            const auto *playback = ArtifactPlaybackService::instance();

            if (!playback || !playback->isPlaying()) {

              impl_->invalidateOverlayComposite();

            }

            markRenderDirty();

          }));

   // Timeline scrub (any gesture: scrub bar, playhead overlay, track view)
   // publishes seeks. Route them into the existing interacting/draft path
   // so scrub renders use reduced effect resolution and skip the CPU
   // readback shape. Quiet-timeout restore is automatic: the render tick
   // finishes the interaction after viewportInteractionIdleMs_ without seeks.
   impl_->eventBusSubscriptions_.push_back(

       impl_->eventBus_.subscribe<TimelineSeekRequestedEvent>(

           [this](const TimelineSeekRequestedEvent &) {

             notifyViewportInteractionActivity();

           }));

   // PlaybackEngine is the authoritative frame clock. While it is running,
  // use the render ticker only as a short UI-thread dispatch/coalescing step
  // instead of introducing a second composition-FPS clock with an arbitrary
  // phase offset. When playback stops, restore the composition-rate interval
  // used for ordinary dirty and viewport-interaction rendering.
  impl_->eventBusSubscriptions_.push_back(

      impl_->eventBus_.subscribe<PlaybackStateChangedEvent>(

          [this](const PlaybackStateChangedEvent &event) {

            if (!impl_ || !impl_->renderTickDriver_) {

              return;

            }

            const bool playing =

                event.state == ::Artifact::PlaybackState::Playing;

            const int intervalMs =

                playing

                    ? 1

                    : compositionPreviewIntervalMs(

                          impl_->previewPipeline_.composition());

            impl_->renderTickDriver_->setInterval(

                std::chrono::milliseconds(intervalMs));

            markRenderDirty();

          }));

  if (!impl_->gpuTextureCacheManager_) {

    impl_->gpuTextureCacheManager_ = std::make_unique<GPUTextureCacheManager>();

  }

  if (auto device = impl_->renderer_->device()) {

    impl_->gpuTextureCacheManager_->setDevice(
        device, impl_->renderer_->immediateContext());

    impl_->gpuTextureCacheManager_->setBudgetBytes(512ull * 1024ull * 1024ull);

    impl_->gpuTextureCacheManager_->setMaxEntries(256);

    // TGFX-inspired frame-age retirement complements the byte/entry LRU.
    // Visible resources refresh their age through bindingRecord(); hidden
    // resources retire gradually without a one-frame destruction spike.
    impl_->gpuTextureCacheManager_->setResourceExpirationFrames(600);

    impl_->gpuTextureCacheManager_->setMaxExpiredEvictionsPerFrame(8);

  }



  impl_->initialized_ = true;



  // Schedule blend pipeline initialization off the hot render path.

  // blendPipeline_->initialize() compiles GPU shaders (slow); running it

  // inside renderOneFrameImpl would stall the render timer for hundreds of ms.

  if (impl_->gpuBlendEnabled_) {
    impl_->scheduleBlendPipelineInitialization(
        this, 1500, QStringLiteral("startup"));
  }

  // Mask cutout is an independent capability. Its initialization must not
  // decide whether standard Add/Multiply composition is available.
  QTimer::singleShot(1500, this, [this]() {
    if (!impl_ || !impl_->initialized_ || !impl_->renderer_ ||
        !impl_->renderer_->device() || !impl_->renderer_->immediateContext()) {
      return;
    }
    if (!impl_->maskCutoutPipeline_) {
      auto ctx = std::make_unique<ArtifactCore::GpuContext>(
          impl_->renderer_->device(), impl_->renderer_->immediateContext());
      impl_->maskCutoutPipeline_ =
          std::make_unique<ArtifactCore::MaskCutoutPipeline>(*ctx);
      ctx.release();
      if (impl_->maskCutoutPipeline_->initialize()) {
        qInfo() << "[CompositionView][Startup] mask cutout pipeline init OK";
      } else {
        qWarning() << "[CompositionView] MaskCutoutPipeline FAILED to init.";
      }
    }
  });

}



void CompositionRenderController::destroy() {

  if (impl_) {
    impl_->trackerCancelRequested_.store(true, std::memory_order_release);
    impl_->trackerSolveFuture_.waitForFinished();
    impl_->trackerSolveWorkingCopy_.reset();
    impl_->trackerPreSolveJson_.clear();
    ++impl_->trackerJobGeneration_;
    impl_->trackerCaptureActive_ = false;
    impl_->trackerJobDirection_ = Impl::TrackerJobDirection::None;
    if (impl_->trackerMotionTracker_) {
      trackerDetachSession();
    }
  }

  if (impl_->renderTickDriver_) {

    impl_->renderTickDriver_->stop();

  }

  stop();

  if (impl_->renderer_) {

    for (auto &slot : impl_->previewRenderPipelineSlots_) {

      if (slot.depthTargetView) {

        impl_->renderer_->destroyOffscreenTexture(slot.depthTargetView);

        slot.depthTargetView = nullptr;

        slot.depthTargetSize = QSize();

      }

      if (slot.msaaColorTargetView) {
        impl_->renderer_->destroyOffscreenTexture(slot.msaaColorTargetView);
        slot.msaaColorTargetView = nullptr;
      }

      if (slot.msaaDepthTargetView) {
        impl_->renderer_->destroyOffscreenTexture(slot.msaaDepthTargetView);
        slot.msaaDepthTargetView = nullptr;
      }

      slot.msaaTargetSize = QSize();

    }

  }

  impl_->compositionRenderer_.reset();

  impl_->viewportColorPipeline_.reset();

  impl_->clearPrecompGpuOutputs();

  impl_->clearMatteGpuOutputs();

  impl_->clearCompositionSpaceGpuCache();

  impl_->surfaceCache_.clear();

  impl_->solidPointwiseCache_.clear(impl_->renderer_.get());

  impl_->surfaceGenerations_.clear();

  if (impl_->gpuTextureCacheManager_) {

    impl_->gpuTextureCacheManager_->clearDevice();

    impl_->gpuTextureCacheManager_.reset();

  }

  impl_->blendPipeline_.reset();

  impl_->blendPipelineReady_ = false;

  impl_->blendPipelineInitScheduled_ = false;

  impl_->blendPipelineInitInProgress_ = false;

  impl_->blendPipelineInitAttemptCount_ = 0;

  ++impl_->blendPipelineInitGeneration_;

  impl_->blendPipelineInitState_ = QStringLiteral("not-scheduled");

  impl_->maskCutoutPipeline_.reset();

  impl_->lastPresentedReadbackSRV_ = nullptr;
  impl_->compareMode_ = CompositionCompareMode::Off;
  impl_->layerRenderFilter_ = CompositionLayerRenderFilter::All;

  if (impl_->renderer_) {

    impl_->renderer_->destroy();

    impl_->renderer_.reset();

  }

  // P0-3b.0: reset the owned RenderContext so its ROI / mode state does
  // not survive across destroy()/initialize() cycles.
  impl_->renderContext_.reset();

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

  if (impl_->renderTickDriver_ && !impl_->renderTickDriver_->isRunning()) {

    impl_->renderTickDriver_->start();

  }

  impl_->invalidateBaseComposite();

  // Continuous timer removed for performance.

  // Rendering is now event-driven (frameChanged, propertyChanged, etc.)

  markRenderDirty();

}



void CompositionRenderController::stop() {

  if (impl_->renderTickDriver_) {

    impl_->renderTickDriver_->stop();

  }

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

  const float savedZoom = impl_->renderer_->getZoom();
  float savedPanX = 0.0f;
  float savedPanY = 0.0f;
  impl_->renderer_->getPan(savedPanX, savedPanY);

  impl_->renderer_->flushAndWait(false);

  // If swapchain was never created (e.g., widget was 0×0 at init time),

  // use createSwapChain which handles fresh creation + shader/PSO setup.

  if (!impl_->renderer_->hasSwapChain()) {

    qDebug() << "[CompositionRenderController] recreateSwapChain: no swapchain "

                "— calling createSwapChain";

    impl_->renderer_->createSwapChain(hostWidget);

  } else {

    impl_->renderer_->recreateSwapChain(hostWidget);

  }

  impl_->renderer_->setZoom(savedZoom);
  impl_->renderer_->setPan(savedPanX, savedPanY);

  impl_->invalidateBaseComposite();

}



void CompositionRenderController::setViewportSize(
    ArtifactCore::Coordinates::ScreenLogicalExtent2 logicalSize) {

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

  const auto physicalSize = ArtifactCore::Coordinates::toScreenPhysical(
      logicalSize, impl_->devicePixelRatio_);
  const float newHostWidth = physicalSize.width;
  const float newHostHeight = physicalSize.height;

  if (std::abs(newHostWidth - impl_->hostWidth_) < 0.5f &&

      std::abs(newHostHeight - impl_->hostHeight_) < 0.5f) {

    return;

  }

  impl_->hostWidth_ = newHostWidth;

  impl_->hostHeight_ = newHostHeight;

  impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);

  impl_->invalidateBaseComposite();

}

void CompositionRenderController::setPresentationLayout(
    CompositionViewportPresentationLayout layout) {
  if (impl_->presentationLayout_ == layout) {
    return;
  }
  impl_->presentationLayout_ = layout;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/PresentationLayout/Quad"),
      layout == CompositionViewportPresentationLayout::Quad);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

CompositionViewportPresentationLayout
CompositionRenderController::presentationLayout() const {
  return impl_->presentationLayout_;
}



void CompositionRenderController::setPreviewQualityPreset(

    PreviewQualityPreset preset) {

  impl_->previewQualityPreset_ = preset;

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

    if (auto *playback = ArtifactPlaybackService::instance()) {

      playback->invalidateRamPreviewCache(

          QStringLiteral("preview-quality-changed"));

    }

    if (impl_->hostWidth_ > 0 && impl_->hostHeight_ > 0) {

      // hostWidth_/hostHeight_ are already physical pixels; call renderer

      // directly

      impl_->renderer_->setViewportSize(impl_->hostWidth_, impl_->hostHeight_);

    }

    impl_->invalidateBaseComposite();

    markRenderDirty();

  }

}



void CompositionRenderController::panBy(
    ArtifactCore::Coordinates::ScreenLogicalVector2 viewportDelta) {

  if (impl_->interactionPerfEnabled_) impl_->interactionPerfAction_ |= 1;
  impl_->recordInteractionPerfEvent(1); // pan entry (controller boundary)

  if (!impl_->renderer_) {

    return;

  }

  // Fly / Walk owns a viewport-only camera; panning would only mutate the
  // state the mode restores on exit.
  if (impl_->viewportNavigationMode_ != ViewportNavigationMode::Off) {
    return;
  }

  if (!std::isfinite(viewportDelta.x) ||
      !std::isfinite(viewportDelta.y)) {
    return;
  }

  ++impl_->smoothZoomGeneration_;
  impl_->smoothZoomActive_ = false;

  if (!impl_->restoringViewState_) {
    const auto now = std::chrono::steady_clock::now();
    const bool startOfPanHistory =
        impl_->lastPanHistoryAt_.time_since_epoch().count() == 0 ||
        now - impl_->lastPanHistoryAt_ > std::chrono::milliseconds(250);
    if (startOfPanHistory) {
      impl_->pushViewHistory();
    }
    impl_->lastPanHistoryAt_ = now;
  }
  impl_->recordInteractionPerfEvent(2);

  const auto physicalDelta = ArtifactCore::Coordinates::toScreenPhysical(
      viewportDelta, impl_->devicePixelRatio_);
  impl_->renderer_->panBy(physicalDelta.x, physicalDelta.y);
  impl_->recordInteractionPerfEvent(3); // renderer pan applied
  notifyViewportInteractionActivity();
  impl_->recordInteractionPerfEvent(4); // interaction notification finished

  // Phase 2: Use fixed-rate render tick instead of renderOneFrame().
  if (impl_->interactionPerfEnabled_) impl_->interactionPerfAction_ |= 1;

  // No invalidateBaseComposite() needed — panX/panY in RenderKeyState

  // already detects the change.

  markRenderDirty();
  impl_->recordInteractionPerfEvent(5); // pan returned from scheduling

}



void CompositionRenderController::setGizmoMode(

    const TransformGizmo::Mode mode) {

  if (!impl_) {

    return;

  }

  if (!impl_->gizmoSessionEnding_ && impl_->gizmoMode_ != mode &&
      impl_->gizmoSession_.owner != GizmoOwner::None) {
    cancelGizmoInteraction();
  }
  impl_->gizmoMode_ = mode;

  if (impl_->gizmo_) {

    impl_->gizmo_->setMode(mode == TransformGizmo::Mode::Full ? TransformGizmo::Mode::None : mode);

  }

  if (impl_->gizmo3D_) {
    // Selection keeps move axes and the separate frame resize handles.
    // Rotation rings and axis scale handles belong to their dedicated modes.
    GizmoMode gizmo3DMode = GizmoMode::Move;
    if (mode == TransformGizmo::Mode::Full) {
      gizmo3DMode = GizmoMode::Full;
    } else if (mode == TransformGizmo::Mode::Translation) {
      gizmo3DMode = GizmoMode::Move;
    } else if (mode == TransformGizmo::Mode::Rotate) {
      gizmo3DMode = GizmoMode::Rotate;
    } else if (mode == TransformGizmo::Mode::Scale) {
      gizmo3DMode = GizmoMode::Scale;
    } else if (mode == TransformGizmo::Mode::AnchorPoint) {
      // Projected-frame layers (image / solid / 3D plane) never reach the
      // legacy 2D gizmo's press path, so the anchor handle is owned by the 3D
      // gizmo for every layer type.  The 2D gizmo keeps its own AnchorPoint
      // mode for the layers that still reach it (text-free 2D paths).
      gizmo3DMode = GizmoMode::AnchorPoint;
    }
    impl_->gizmo3D_->setMode(gizmo3DMode);
  }

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



TransformGizmo::Mode CompositionRenderController::gizmoMode() const {

  return impl_ ? impl_->gizmoMode_ : TransformGizmo::Mode::All;

}



void CompositionRenderController::notifyViewportInteractionActivity() {

  const bool wasInteracting = impl_->viewportInteracting_;

  impl_->viewportInteracting_ = true;

  impl_->viewportInteractionElapsedTimer_.restart();

  if (impl_->running_ && impl_->renderTickDriver_ &&

      !impl_->renderTickDriver_->isRunning()) {

    impl_->renderTickDriver_->start();

  }

  if (!wasInteracting) {

    // damage 保持型で再合成する。無引数で呼ぶと damageTracker_ を clearAll し、
    // 全 previewRenderPipelineSlots_ を full-redraw に戻すため、OnOff のような
    // 1 操作でも画面全体が作り直される。到達する経路（Effect パラメータ編集・
    // ギズモドラッグ）はいずれも recordLayerDamage 済みなので、保持型でも
    // 必要な領域は再合成される。preview LOD は viewportInteracting_ 側で
    // 制御されるため、引数とは独立に維持される。
    impl_->invalidateBaseComposite(/*preserveLayerDamage=*/true);

  }

}



void CompositionRenderController::finishViewportInteraction() {

  if (!impl_->viewportInteracting_) {

    return;

  }

  impl_->viewportInteracting_ = false;

  impl_->recordInteractionPerfEvent(9); // gesture end
  if (impl_->interactionPerfEnabled_ &&
      (impl_->interactionPerf_.count != 0 || impl_->interactionPerf_.eventCount != 0) &&
      !impl_->interactionPerfWrite_.isRunning() && impl_->interactionPerfFiles_ < 64) {
    // Copy once at gesture end; worker owns all data and never accesses Impl.
    const auto batch = impl_->interactionPerf_;
    ++impl_->interactionPerfFiles_;
    impl_->interactionPerfWrite_ = QtConcurrent::run(&sharedBackgroundThreadPool(), [batch]() {
      const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                              .filePath(QStringLiteral("Diagnostics/ViewportPerf"));
      if (!QDir().mkpath(dir)) {
        qWarning() << "[ViewportPerf] Cannot create output directory" << dir;
        return;
      }
      const QString path = QDir(dir).filePath(
          QStringLiteral("interaction-%1.csv").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
      QByteArray csv;
      csv.reserve(static_cast<qsizetype>(batch.count * 220 + 512));
      qint64 totalUs = 0, maxUs = 0;
      for (std::size_t i = 0; i < batch.count; ++i) {
        totalUs += batch.samples[i].frameUs;
        maxUs = std::max(maxUs, batch.samples[i].frameUs);
      }
      csv += "# CPU wall times, not GPU timestamps; phases may overlap; action:1=pan,2=zoom,3=mixed\n";
      csv += "# samples=" + QByteArray::number(static_cast<qulonglong>(batch.count)) +
             ",dropped=" + QByteArray::number(batch.dropped) +
             ",mean_frame_us=" + QByteArray::number(batch.count ? totalUs / static_cast<qint64>(batch.count) : 0) +
             ",max_frame_us=" + QByteArray::number(maxUs) + "\n";
      csv += "elapsed_us,frame_us,setup_ms,base_ms,layer_ms,overlay_ms,flush_ms,present_phase_ms,draws,width,height,zoom,pan_x,pan_y,downsample,action\n";
      for (std::size_t i = 0; i < batch.count; ++i) {
        const auto &s = batch.samples[i];
        csv += QByteArray::number(s.elapsedUs) + ',' + QByteArray::number(s.frameUs) + ',' +
               QByteArray::number(s.setupMs) + ',' + QByteArray::number(s.baseMs) + ',' +
               QByteArray::number(s.layerMs) + ',' + QByteArray::number(s.overlayMs) + ',' +
               QByteArray::number(s.flushMs) + ',' + QByteArray::number(s.presentMs) + ',' +
               QByteArray::number(s.draws) + ',' + QByteArray::number(s.width) + ',' +
               QByteArray::number(s.height) + ',' + QByteArray::number(s.zoom) + ',' +
               QByteArray::number(s.panX) + ',' + QByteArray::number(s.panY) + ',' +
               QByteArray::number(s.downsample) + ',' + QByteArray::number(s.action) + '\n';
      }
      QSaveFile file(path);
      if (!file.open(QIODevice::WriteOnly) || file.write(csv) != csv.size() || !file.commit()) {
        qWarning() << "[ViewportPerf] Save failed" << path << file.errorString();
        return;
      }
      QByteArray events;
      events.reserve(static_cast<qsizetype>(batch.eventCount * 32 + 512));
      events += "# controller timestamps; same clock as frame CSV; not OS input timestamps\n";
      events += "# kind:1=pan_entry,2=history_done,3=renderer_pan_done,4=notify_done,5=pan_exit,6=render_request,7=frame_body_start,8=render_return,9=gesture_end\n";
      events += "# dropped=" + QByteArray::number(batch.eventsDropped) + "\n";
      events += "elapsed_us,kind\n";
      for (std::size_t i = 0; i < batch.eventCount; ++i) {
        events += QByteArray::number(batch.events[i].elapsedUs) + ',' +
                  QByteArray::number(batch.events[i].kind) + '\n';
      }
      const QString eventPath = path.chopped(4) + QStringLiteral(".events.csv");
      QSaveFile eventFile(eventPath);
      if (!eventFile.open(QIODevice::WriteOnly) || eventFile.write(events) != events.size() ||
          !eventFile.commit()) {
        qWarning() << "[ViewportPerf] Event save failed" << eventPath << eventFile.errorString();
      }
      qInfo() << "[ViewportPerf] Saved" << path;
    });
  }
  impl_->interactionPerf_.count = 0;
  impl_->interactionPerf_.dropped = 0;
  impl_->interactionPerf_.eventCount = 0;
  impl_->interactionPerf_.eventsDropped = 0;
  impl_->interactionPerfAction_ = 0;
  impl_->interactionPerfTimer_.invalidate();

  impl_->invalidateBaseComposite();

  markRenderDirty();

}



void CompositionRenderController::setLayerSurfaceCacheDiagnosticsEnabled(
    bool enabled) {
  impl_->layerSurfaceCacheDiagnosticsEnabled_ = enabled;
  impl_->layerSurfaceCacheHitCounts_.clear();
  impl_->layerSurfaceCacheMissCounts_.clear();
  impl_->layerSurfaceCacheInvalidationCounts_.clear();
  impl_->layerSurfaceCacheBuildCounts_.clear();
}

QJsonObject CompositionRenderController::layerSurfaceCacheDiagnostics() const {
  QJsonArray entries;
  QSet<QString> keys;
  for (auto it = impl_->layerSurfaceCacheHitCounts_.cbegin();
       it != impl_->layerSurfaceCacheHitCounts_.cend(); ++it) keys.insert(it.key());
  for (auto it = impl_->layerSurfaceCacheMissCounts_.cbegin();
       it != impl_->layerSurfaceCacheMissCounts_.cend(); ++it) keys.insert(it.key());
  for (auto it = impl_->layerSurfaceCacheInvalidationCounts_.cbegin();
       it != impl_->layerSurfaceCacheInvalidationCounts_.cend(); ++it) keys.insert(it.key());
  for (auto it = impl_->layerSurfaceCacheBuildCounts_.cbegin();
       it != impl_->layerSurfaceCacheBuildCounts_.cend(); ++it) keys.insert(it.key());
  for (const QString& key : keys) {
    entries.append(QJsonObject{
        {QStringLiteral("slot"), key.endsWith(QStringLiteral("|draft"))
             ? QStringLiteral("draft") : QStringLiteral("full")},
        {QStringLiteral("layerId"), key.left(key.lastIndexOf(QLatin1Char('|')))},
        {QStringLiteral("hits"), static_cast<qint64>(impl_->layerSurfaceCacheHitCounts_.value(key))},
        {QStringLiteral("misses"), static_cast<qint64>(impl_->layerSurfaceCacheMissCounts_.value(key))},
        {QStringLiteral("invalidations"), static_cast<qint64>(impl_->layerSurfaceCacheInvalidationCounts_.value(key))},
        {QStringLiteral("builds"), static_cast<qint64>(impl_->layerSurfaceCacheBuildCounts_.value(key))}});
  }
  return {{QStringLiteral("enabled"), impl_->layerSurfaceCacheDiagnosticsEnabled_},
          {QStringLiteral("entries"), entries}};
}



LayerID CompositionRenderController::selectedLayerId() const {

  return impl_->selectedLayerId_;

}

void CompositionRenderController::clearMotionPathSelection() {
  if (impl_->selectedMotionPathFrames_.isEmpty()) return;
  impl_->selectedMotionPathFrames_.clear();
  impl_->hoveredMotionPathFrame_ = -1;
  impl_->hoveredMotionPathTangentHandle_ = MotionPathTangentHandle::None;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::setLayerRenderFilter(
    CompositionLayerRenderFilter filter) {
  if (impl_->layerRenderFilter_ == filter) {
    return;
  }
  impl_->layerRenderFilter_ = filter;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

CompositionLayerRenderFilter
CompositionRenderController::layerRenderFilter() const {
  return impl_->layerRenderFilter_;
}

// P0-4: per-viewport category mask. The mask only affects viewport
// rendering; render queue output is unaffected.
void CompositionRenderController::setViewportLayerCategoryMask(
    CompositionViewportLayerCategoryMask mask) {
  if (!impl_) {
    return;
  }
  if (impl_->viewportLayerCategoryMask_ == mask) {
    return;
  }
  impl_->viewportLayerCategoryMask_ = mask;
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

CompositionViewportLayerCategoryMask
CompositionRenderController::viewportLayerCategoryMask() const {
  return impl_
      ? impl_->viewportLayerCategoryMask_
      : static_cast<CompositionViewportLayerCategoryMask>(
            CompositionViewportLayerCategory::All);
}

void CompositionRenderController::toggleViewportLayerCategory(
    CompositionViewportLayerCategory category) {
  if (!impl_) {
    return;
  }
  const auto bit = static_cast<CompositionViewportLayerCategoryMask>(category);
  impl_->viewportLayerCategoryMask_ =
      (impl_->viewportLayerCategoryMask_ & bit) != 0
          ? (impl_->viewportLayerCategoryMask_ & ~bit)
          : (impl_->viewportLayerCategoryMask_ | bit);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

bool CompositionRenderController::isViewportLayerCategoryVisible(
    CompositionViewportLayerCategory category) const {
  if (!impl_) {
    return true;
  }
  const auto bit = static_cast<CompositionViewportLayerCategoryMask>(category);
  return (impl_->viewportLayerCategoryMask_ & bit) != 0;
}



void CompositionRenderController::setCompareMode(CompositionCompareMode mode) {
  const auto composition = impl_->previewPipeline_.composition();
  if (composition) {
    if (mode == CompositionCompareMode::Off) {
      if (impl_->stateCompareSessionActive_) {
        composition->setActiveStateVariantId(
            impl_->compareRestoreStateId_);
      }
      impl_->stateCompareSessionActive_ = false;
      impl_->compareRestoreStateId_.clear();
    } else if (mode == CompositionCompareMode::A ||
               mode == CompositionCompareMode::B) {
      const QString targetStateId = mode == CompositionCompareMode::A
          ? composition->stateComparisonAId()
          : composition->stateComparisonBId();
      const bool hasConfiguredPair =
          !composition->stateComparisonAId().isEmpty() ||
          !composition->stateComparisonBId().isEmpty();
      if (hasConfiguredPair) {
        if (!impl_->stateCompareSessionActive_) {
          impl_->compareRestoreStateId_ =
              composition->activeStateVariantId();
          impl_->stateCompareSessionActive_ = true;
        }
        if (!composition->setActiveStateVariantId(targetStateId)) {
          return;
        }
      }
    }
  }

  impl_->compareMode_ = mode;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



CompositionCompareMode CompositionRenderController::compareMode() const {

  return impl_->compareMode_;

}



void CompositionRenderController::setReferencePinned(bool pinned) {

  impl_->referencePinned_ = pinned;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



bool CompositionRenderController::isReferencePinned() const {

  return impl_->referencePinned_;

}



void CompositionRenderController::setReferenceFrame(int frame) {

  impl_->referenceFrame_ = frame;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



int CompositionRenderController::referenceFrame() const {

  return impl_->referenceFrame_;

}



void CompositionRenderController::setGpuBlendEnabled(bool enabled) {

  if (impl_->gpuBlendEnabled_ == enabled) {

    return;

  }

  impl_->gpuBlendEnabled_ = enabled;

  if (enabled && !impl_->blendPipelineReady_) {
    impl_->blendPipelineInitAttemptCount_ = 0;
    impl_->blendPipelineInitState_ = QStringLiteral("re-enabled");
    impl_->scheduleBlendPipelineInitialization(
        this, 0, QStringLiteral("user-enabled"));
  } else if (!enabled) {
    ++impl_->blendPipelineInitGeneration_;
    impl_->blendPipelineInitScheduled_ = false;
    impl_->blendPipelineInitState_ = QStringLiteral("disabled");
  }

  if (auto *playback = ArtifactPlaybackService::instance()) {

    playback->invalidateRamPreviewCache(

        QStringLiteral("render-policy-changed"));

  }

  impl_->invalidateBaseComposite();

  qWarning() << "[CompositionView] GPU blend user toggle changed"

             << "enabled=" << impl_->gpuBlendEnabled_ << "envDisable="

             << qEnvironmentVariableIsSet(

                    "ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");

  markRenderDirty();

}



bool CompositionRenderController::isGpuBlendEnabled() const {

  return impl_->gpuBlendEnabled_;

}

ArtifactCore::Coordinates::ScreenPhysicalVector2
CompositionRenderController::viewportPan() const {

  if (!impl_ || !impl_->renderer_) {

    return {};

  }

  float panX = 0.0f;
  float panY = 0.0f;
  impl_->renderer_->getPan(panX, panY);
  return {panX, panY};

}

ArtifactIRenderer *CompositionRenderController::renderer() const {

  return impl_->renderer_.get();

}

// P0-3b.0: read-only access to the owned RenderContext. The returned
// reference reflects the current render mode and is updated by the
// controller's render pipeline. Callers must not call setMode() / setROI()
// on it; doing so would desynchronize the controller's pipeline state.
const RenderContext &CompositionRenderController::renderContext() const {
  return impl_->renderContext_;
}



LayerID CompositionRenderController::layerAtViewportPos(

    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {

  auto comp = impl_->previewPipeline_.composition();

  // viewportPos is in logical pixels; convert to physical for hit testing

  const QPointF physPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;

  if (const auto modelLayer = hitNearest3DModelLayerAtPickingRay(
          comp, createPickingRay(screenPhysicalPoint(physPos)))) {
    return modelLayer->id();
  }

  const auto layer =
      hitTopmostLayerAtViewportPos(comp, impl_->renderer_.get(), physPos);
  return layer ? layer->id() : LayerID::Nil();

}

WorldRay CompositionRenderController::createPickingRay(

    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos) const {

  if (!impl_->renderer_)

    return {};



  const QMatrix4x4 view = impl_->gizmoUndoSnapshotValid_
                              ? impl_->gizmoDragViewMatrix_
                              : impl_->gizmo3DCameraMatricesValid_
                              ? impl_->gizmo3DViewMatrix_
                              : impl_->renderer_->getViewMatrix();

  const QMatrix4x4 proj = impl_->gizmoUndoSnapshotValid_
                              ? impl_->gizmoDragProjectionMatrix_
                              : impl_->gizmo3DCameraMatricesValid_
                              ? impl_->gizmo3DProjectionMatrix_
                              : impl_->renderer_->getProjectionMatrix();

  QRect viewport(0, 0, (int)impl_->hostWidth_, (int)impl_->hostHeight_);



  const QVector3D nearPos = ViewportMath::unprojectFromTopDown(
      QVector3D(viewportPos.x, viewportPos.y, 0.0f),
      view, proj, viewport);

  const QVector3D farPos = ViewportMath::unprojectFromTopDown(
      QVector3D(viewportPos.x, viewportPos.y, 1.0f),
      view, proj, viewport);



  const QVector3D direction = (farPos - nearPos).normalized();
  return {{nearPos.x(), nearPos.y(), nearPos.z()},
          {direction.x(), direction.y(), direction.z()}};

}

bool CompositionRenderController::isPhysicsDragActive() const {
  const auto layer = impl_ ? impl_->physicsDragLayer_.lock()
                           : ArtifactAbstractLayerPtr{};
  return layer && layer->hasRigidBodyMouseDrag();
}





TransformGizmo *CompositionRenderController::gizmo() const {

  return impl_->gizmo_.get();

}



Artifact3DGizmo *CompositionRenderController::gizmo3D() const {

  return impl_->gizmo3D_.get();

}



bool CompositionRenderController::isGizmoDragActive() const {

  if (!impl_) {

    return false;

  }

  impl_->assertGizmoSession();
  return impl_->gizmoSession_.owner != GizmoOwner::None;

}

bool CompositionRenderController::isTransformGizmoHovered(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {
  if (!impl_) {
    return false;
  }
  if (impl_->constructionDragLayer_) return true;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? ArtifactCore::dynamicPointerCast<ArtifactConstructionLayer>(
      comp->layerById(impl_->selectedLayerId_)) : nullptr;
  const QPointF logical =
      ArtifactCore::Coordinates::toQPointF(viewportPos);
  if (impl_->showGizmoOverlay_ && impl_->renderer_ && layer && layer->isVisible() &&
      !layer->isLocked() && !layer->isSelectionLocked()) {
    const auto view = impl_->gizmo3DCameraMatricesValid_ ? impl_->gizmo3DViewMatrix_ : impl_->renderer_->getViewMatrix();
    const auto projection = impl_->gizmo3DCameraMatricesValid_ ? impl_->gizmo3DProjectionMatrix_ : impl_->renderer_->getProjectionMatrix();
    const QRect viewport(0, 0, int(impl_->hostWidth_), int(impl_->hostHeight_));
    const auto world = layer->getGlobalTransform4x4();
    const QPointF physical = logical * impl_->devicePixelRatio_;
    for (const auto &handle : constructionHandles(*layer)) {
      const auto p = ViewportMath::projectToTopDown(world.map(QVector3D(float(handle.point.x()), float(handle.point.y()), 0)), view, projection, viewport);
      if (std::isfinite(p.z()) && p.z() >= 0 && p.z() <= 1 &&
          QLineF(physical, QPointF(p.x(), p.y())).length() <= 10 * impl_->devicePixelRatio_) return true;
    }
  }
  const auto selected = comp ? comp->layerById(impl_->selectedLayerId_)
                             : ArtifactAbstractLayerPtr{};
  const auto owner = impl_->gizmoOwnerForLayer(selected);
  const QPointF physical = logical * impl_->devicePixelRatio_;
  if (owner == GizmoOwner::Text) {
    return impl_->textGizmo_ && impl_->renderer_ &&
        impl_->textGizmo_->hitTest(physical, impl_->renderer_.get()) !=
            TextGizmo::HandleType::None;
  }
  if (owner == GizmoOwner::Content) {
    return impl_->contentGizmo_ && impl_->renderer_ &&
        impl_->contentGizmo_->hitTest(physical, impl_->renderer_.get()) !=
            ContentGizmo::HandleType::None;
  }
  if (owner == GizmoOwner::Transform3D &&
      (impl_->projectedFrameHoverHandle_ != TransformGizmo::HandleType::None ||
       (impl_->gizmo3D_ && impl_->gizmo3D_->hoverAxis() != GizmoAxis::None))) {
    return true;
  }
  return isTransformGizmoOwner(owner) && impl_->gizmo_ && impl_->renderer_ &&
      legacy2DGizmoShouldOwnPress(this, owner) &&
      impl_->gizmo_->handleAtViewportPos(physical, impl_->renderer_.get()) !=
          TransformGizmo::HandleType::None;

}



ArtifactPointTrackerGizmo *CompositionRenderController::trackerGizmo() const {

  return impl_->trackerGizmo_.get();

}

void CompositionRenderController::cancelMotionSketch() {
  auto *app = ArtifactApplicationManager::instance();
  if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {
    app->motionSketchTool()->cancelSketch();
    if (auto *playback = ArtifactPlaybackService::instance()) {
      if (!impl_->motionSketchWasPlaying_) {
        playback->pause();
      }
    }
    impl_->motionSketchWasPlaying_ = false;
    impl_->motionSketchLastCanvasPos_ = {};
    markRenderDirty();
  }
}

// ROI Debug

void CompositionRenderController::setDebugMode(bool enabled) {

  impl_->debugMode_ = enabled;

}



bool CompositionRenderController::isDebugMode() const {

  return impl_->debugMode_;

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
