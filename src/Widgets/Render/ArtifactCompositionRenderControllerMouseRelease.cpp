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
#include <QMouseEvent>
#include <QVariant>
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

static bool isMeaningfulDragRect(const QRectF &rect)

{

  return rect.width() >= 1.0 && rect.height() >= 1.0;

}

} // namespace CompositionRenderControllerDetail

static bool layerIntersectsCanvasRect(const ArtifactAbstractLayerPtr &layer,

                               const QRectF &rect,

                               const FramePosition &currentFrame) {

  if (!layer || !rect.isValid() || !isLayerEffectivelyVisible(layer) ||

      !layer->isActiveAt(currentFrame) || layer->isLocked() ||
      layer->isSelectionLocked()) {

    return false;

  }



  const QRectF bbox = layer->transformedBoundingBox();

  if (!bbox.isValid()) {

    return false;

  }



  return bbox.intersects(rect);

}

void CompositionRenderController::handleMouseRelease() {
  if (!impl_->gizmoSessionValid()) {
    cancelGizmoInteraction();
    return;
  }
  if (impl_->gizmoSessionEnding_) return;
  Impl::GizmoSessionEndGuard endSession(*impl_);
  const auto releasingOwner = impl_->gizmoSession_.owner;
  // P0-3a IRR handle drag finalization (mouse-up path).
  if (impl_ && impl_->interactiveRenderRegionHandleDrag_ != 0) {
    endInteractiveRenderRegionDrag();
    return;
  }
  // P0-1 Box zoom owns the release event when its marquee is active. The
  // UI drives begin/update via beginBoxZoomInteraction / updateBoxZoomInteraction;
  // finalize here so any release path (UI shortcut, mouse button up, ESC)
  // produces a single canonical end.
  if (impl_->isBoxZooming_) {
    endBoxZoomInteraction();
    return;
  }
  if (impl_->historicalScaleDragging_) {
    if (auto layer = impl_->historicalScaleLayer_.lock(); layer && impl_->historicalScaleChanged_) {
      const auto time = impl_->historicalScaleTime_;
      MotionPathScaleState after;
      auto *manager = UndoManager::instance();
      const bool captured = captureMotionPathScale(layer, time, after);
      const bool unchanged = captured &&
          std::abs(after.values[0].toDouble() - impl_->historicalScaleBefore_.values[0].toDouble()) < 0.000001 &&
          std::abs(after.values[1].toDouble() - impl_->historicalScaleBefore_.values[1].toDouble()) < 0.000001;
      if (!captured || unchanged || !manager ||
          !manager->push(std::make_unique<MotionPathScaleUndoCommand>(
              layer, time, impl_->historicalScaleBefore_, after))) {
        applyMotionPathScale(layer, time, impl_->historicalScaleBefore_);
      }
    }
    impl_->historicalScaleDragging_ = false;
    impl_->historicalScaleChanged_ = false;
    impl_->historicalScaleLayer_.reset();
    impl_->motionPathCache_.valid = false;
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    setInfoOverlayText(QString(), QString());
    finishViewportInteraction();
    markRenderDirty();
    return;
  }
  if (const auto layer = impl_->physicsDragLayer_.lock()) {
    layer->endRigidBodyMouseDrag();
    impl_->physicsDragLayer_.reset();
    impl_->invalidateOverlayComposite();
    finishViewportInteraction();
    markRenderDirty();
    return;
  }
  if (const auto layer = impl_->constructionDragLayer_) {
    const auto index = impl_->constructionDragHandle_.index;
    const auto before = impl_->constructionDragBefore_;
    impl_->constructionDragLayer_.reset();
    if (index < layer->constructionItems().size()) {
      const auto after = layer->constructionItems()[index];
      if (before.toJson() != after.toJson()) {
        if (auto *manager = UndoManager::instance()) {
          if (!manager->push(std::make_unique<ConstructionItemUndoCommand>(layer, index, before, after)))
            layer->setConstructionItem(index, before);
        }
      }
    }
    impl_->invalidateLayerSurfaceCache(layer);
    impl_->invalidateBaseComposite(); impl_->invalidateOverlayComposite(); markRenderDirty();
    return;
  }

  qCDebug(compositionViewLog) << "[MouseRelease] ENTER";
  if (impl_->isDraggingLineEndpoint_) {
    auto layer = impl_->draggingLineLayer_.lock();
    auto *line = layer
        ? dynamic_cast<ArtifactShapeLayer *>(layer.get())
        : nullptr;
    if (line) {
      const auto time = gizmoTransformTime(layer, layer->currentFrame());
      const auto &transform = line->transform3D();
      const auto afterPosition =
          QPointF(transform.positionXAt(time), transform.positionYAt(time));
      const bool pushed =
          !UndoManager::instance() || UndoManager::instance()->push(
              std::make_unique<LineEndpointUndoCommand>(
                  layer, impl_->draggingLineBeforeWidth_,
                  impl_->draggingLineBeforeHeight_,
                  impl_->draggingLineBeforePosition_,
                  impl_->draggingLineBeforeRotation_, line->shapeWidth(),
                  line->shapeHeight(), afterPosition,
                  transform.rotationAt(time)));
      if (!pushed) {
        line->setSize(impl_->draggingLineBeforeWidth_,
                      impl_->draggingLineBeforeHeight_);
        line->transform3D().setPosition(
            time,
            static_cast<float>(impl_->draggingLineBeforePosition_.x()),
            static_cast<float>(impl_->draggingLineBeforePosition_.y()));
        line->transform3D().setRotation(
            time, impl_->draggingLineBeforeRotation_);
        line->setDirty(LayerDirtyFlag::Transform);
        line->changed();
      }
    }
    impl_->isDraggingLineEndpoint_ = false; impl_->draggingLineEndpoint_ = -1; impl_->draggingLineLayer_.reset(); finishViewportInteraction(); markRenderDirty(); return;
  }


  if (impl_->rigWeightPainting_) {
    if (auto layer = impl_->rigWeightLayer_.lock()) {
      if (auto *rigLayer = dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())) {
        if (auto *mesh = rigLayer->rig2D().skinMesh()) {
          const auto after = mesh->vertices();
          bool changed = after.size() != impl_->rigWeightBeforeVertices_.size();
          if (!changed) {
            for (size_t i = 0; i < after.size() && !changed; ++i) {
              for (int slot = 0; slot < 4; ++slot) {
                if (after[i].boneIndices[slot] !=
                        impl_->rigWeightBeforeVertices_[i].boneIndices[slot] ||
                    !qFuzzyCompare(after[i].weights[slot] + 1.0f,
                                   impl_->rigWeightBeforeVertices_[i].weights[slot] + 1.0f)) {
                  changed = true;
                  break;
                }
              }
            }
          }
          if (changed) {
            auto *manager = UndoManager::instance();
            const bool pushed =
                !manager || manager->push(std::make_unique<RigSkinWeightsUndoCommand>(
                    layer, impl_->rigWeightBeforeVertices_, after));
            if (!pushed) {
              mesh->setVertices(impl_->rigWeightBeforeVertices_);
              rigLayer->setDirty(LayerDirtyFlag::Transform);
              rigLayer->changed();
            }
          }
        }
      }
    }
    impl_->rigWeightPainting_ = false;
    impl_->rigWeightLayer_.reset();
    impl_->rigWeightBeforeVertices_.clear();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->rigSelectionDragging_) {
    if (!impl_->selectedRigBoneId_.isNil()) {
      if (auto layer = impl_->rigDragLayer_.lock()) {
        if (auto *rigLayer = dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())) {
          if (auto *bone = rigLayer->rig2D().findBone(impl_->selectedRigBoneId_)) {
            const auto after = bone->localTransform();
            if (!qFuzzyCompare(after.rotation,
                               impl_->rigDragStartTransform_.rotation)) {
              auto *manager = UndoManager::instance();
              const bool pushed =
                  !manager || manager->push(std::make_unique<RigBoneTransformUndoCommand>(
                      layer, impl_->selectedRigBoneId_,
                      impl_->rigDragStartTransform_, after));
              if (!pushed) {
                rigLayer->setRigBoneLocalTransform(
                    impl_->selectedRigBoneId_, impl_->rigDragStartTransform_);
                if (rigLayer->rig2D().rootBone()) {
                  rigLayer->rig2D().rootBone()->updateHierarchy();
                }
                rigLayer->setDirty(LayerDirtyFlag::Transform);
                rigLayer->changed();
              }
            }
          }
        }
      }
    } else if (!impl_->selectedRigControlId_.isNil()) {
      if (auto layer = impl_->rigDragLayer_.lock()) {
        if (auto *rigLayer = dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())) {
          if (auto *control = rigLayer->rig2D().findControl(
                  impl_->selectedRigControlId_)) {
            const QVariant after = control->value();
            if (after != impl_->rigDragStartControlValue_) {
              auto *manager = UndoManager::instance();
              const bool pushed =
                  !manager || manager->push(std::make_unique<RigControlValueUndoCommand>(
                      layer, impl_->selectedRigControlId_,
                      impl_->rigDragStartControlValue_, after));
              if (!pushed) {
                control->setValue(impl_->rigDragStartControlValue_);
                rigLayer->goToFrame(rigLayer->currentFrame());
                rigLayer->setDirty(LayerDirtyFlag::Transform);
                rigLayer->changed();
              }
            }
          }
        }
      }
    }
    impl_->rigSelectionDragging_ = false;
    impl_->rigDragLayer_.reset();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (!impl_->puppetPinUndoId_.isEmpty()) {
    if (auto *app = ArtifactApplicationManager::instance();
        app && app->puppetTool()) {
      const QPointF afterPosition =
          app->puppetTool()->pinPosition(impl_->puppetPinUndoId_);
      const float afterRotation =
          app->puppetTool()->pinRotation(impl_->puppetPinUndoId_);
      const LayerID puppetLayerId =
          app->puppetTool()->pinLayerId(impl_->puppetPinUndoId_);
      const auto composition = impl_->previewPipeline_.composition();
      const auto layer = composition
          ? composition->layerById(puppetLayerId) : ArtifactAbstractLayerPtr{};
      if (layer && impl_->puppetLayerUndoSnapshotValid_) {
        app->puppetTool()->persistLayerData(puppetLayerId);
      }
      const QJsonObject afterAnimation =
          app->puppetTool()->pinPositionAnimationSnapshot(
              puppetLayerId, impl_->puppetPinUndoId_);
      if (layer && afterAnimation == impl_->puppetPinAnimationBefore_) {
        app->puppetTool()->commitPinPositionAtFrame(
            impl_->puppetPinUndoId_, afterPosition);
      }
      const QJsonObject committedAnimation =
          app->puppetTool()->pinPositionAnimationSnapshot(
              puppetLayerId, impl_->puppetPinUndoId_);
      const QJsonObject afterState = layer ? layer->deformation2DData()
                                           : QJsonObject{};
      if (layer && committedAnimation != impl_->puppetPinAnimationBefore_) {
        auto *manager = UndoManager::instance();
        const bool hasStateSnapshot = impl_->puppetLayerUndoSnapshotValid_ &&
            afterState != impl_->puppetLayerUndoSnapshot_;
        const bool pushed = !manager || (hasStateSnapshot
            ? manager->push(std::make_unique<Deformation2DStateUndoCommand>(
                  layer, impl_->puppetLayerUndoSnapshot_, afterState,
                  QStringLiteral("Set Deformer Keyframe"), app->puppetTool()))
            : manager->push(std::make_unique<DeformerControlKeyframeUndoCommand>(
                  app->puppetTool(), puppetLayerId, impl_->puppetPinUndoId_,
                  impl_->puppetPinAnimationBefore_, committedAnimation,
                  impl_->puppetPinUndoBeforePosition_, afterPosition)));
        if (!pushed) {
          if (hasStateSnapshot) {
            app->puppetTool()->restoreLayerData(
                puppetLayerId, impl_->puppetLayerUndoSnapshot_, layer.get());
          } else {
            app->puppetTool()->restorePinPositionAnimation(
                puppetLayerId, impl_->puppetPinUndoId_,
                impl_->puppetPinAnimationBefore_,
                impl_->puppetPinUndoBeforePosition_);
            app->puppetTool()->movePin(
                impl_->puppetPinUndoId_, impl_->puppetPinUndoBeforePosition_);
          }
        }
      } else if (layer && impl_->puppetLayerUndoSnapshotValid_ &&
          afterState != impl_->puppetLayerUndoSnapshot_) {
        auto *manager = UndoManager::instance();
        const bool pushed = !manager || manager->push(
            std::make_unique<Deformation2DStateUndoCommand>(
                layer, impl_->puppetLayerUndoSnapshot_, afterState,
                QStringLiteral("Move Deformer Control"), app->puppetTool()));
        if (!pushed) {
          app->puppetTool()->restoreLayerData(
              puppetLayerId, impl_->puppetLayerUndoSnapshot_, layer.get());
        }
      } else if (!impl_->puppetLayerUndoSnapshotValid_ &&
                 (afterPosition != impl_->puppetPinUndoBeforePosition_ ||
                 std::abs(afterRotation - impl_->puppetPinUndoBeforeRotation_) >
                     0.001f)) {
        if (!puppetLayerId.isNil()) {
          auto *manager = UndoManager::instance();
          const bool pushed =
              !manager || manager->push(std::make_unique<PuppetPinUndoCommand>(
                  app->puppetTool(), impl_->renderer_.get(),
                  puppetLayerId, impl_->puppetPinUndoId_,
                  impl_->puppetPinUndoBeforePosition_, afterPosition,
                  impl_->puppetPinUndoBeforeRotation_, afterRotation));
          if (!pushed) {
            app->puppetTool()->movePin(impl_->puppetPinUndoId_,
                                       impl_->puppetPinUndoBeforePosition_);
            app->puppetTool()->setPinRotation(
                impl_->puppetPinUndoId_, impl_->puppetPinUndoBeforeRotation_);
            app->puppetTool()->deformLayer(
                puppetLayerId, impl_->renderer_.get());
          }
        }
      }
    }
  }
  if (!impl_->puppetPinUndoId_.isEmpty()) {
    if (auto *app = ArtifactApplicationManager::instance();
        app && app->puppetTool()) {
      const LayerID puppetLayerId =
          app->puppetTool()->pinLayerId(impl_->puppetPinUndoId_);
      const QJsonObject finalAnimation =
          app->puppetTool()->pinPositionAnimationSnapshot(
              puppetLayerId, impl_->puppetPinUndoId_);
      if (!puppetLayerId.isNil() &&
          finalAnimation == impl_->puppetPinAnimationBefore_) {
        app->puppetTool()->persistLayerData(puppetLayerId);
      }
    }
  }
  impl_->puppetPinUndoId_.clear();
  impl_->puppetLayerUndoSnapshot_ = {};
  impl_->puppetPinAnimationBefore_ = {};
  impl_->puppetLayerUndoSnapshotValid_ = false;
  impl_->puppetPinUndoBeforePosition_ = {};
  impl_->puppetPinUndoBeforeRotation_ = 0.0f;
  impl_->puppetRotationDragging_ = false;
  impl_->puppetRotationPinId_.clear();
  impl_->puppetRotationStartAngle_ = 0.0f;
  impl_->puppetPinDragging_ = false;
  impl_->puppetPinDragStartCanvas_ = {};
  impl_->puppetPinDragStartPosition_ = {};

  auto toolManager = ArtifactApplicationManager::instance()
                         ? ArtifactApplicationManager::instance()->toolManager()
                         : nullptr;
  const auto activeTool =
      toolManager ? toolManager->activeTool() : ToolType::Selection;
  if (activeTool == ToolType::Clone && impl_->cloneStampDragging_) {
    impl_->cloneStampDragging_ = false;
    impl_->brushCursorVisible_ = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }
  if (activeTool == ToolType::Brush || activeTool == ToolType::RotoBrush ||
      activeTool == ToolType::Eraser) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                            : ArtifactAbstractLayerPtr{};
    if (layer && impl_->renderer_) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {impl_->brushLastViewportPos_.x,
           impl_->brushLastViewportPos_.y});
      if (auto *brushTool = ArtifactApplicationManager::instance()->brushTool()) {
        brushTool->mouseReleaseEvent(layer, {canvas.x, canvas.y});
        if (activeTool == ToolType::RotoBrush && impl_->rotoBrushEngine_) {
          if (auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer.get())) {
            const QImage source = imageLayer->toQImage();
            if (!source.isNull()) {
              cv::Mat sourceMat = compositionQImageToCvMat(source, true);
              const QRectF bounds = imageLayer->localBounds();
              const auto &points = brushTool->lastStrokePoints();
              ArtifactCore::RotoBrushStroke stroke;
              stroke.type = impl_->rotoBrushStrokeType_;
              stroke.thickness = std::max(1, static_cast<int>(brushTool->radius()));
              for (const QPointF &point : points) {
                if (!std::isfinite(point.x()) || !std::isfinite(point.y()) ||
                    bounds.width() <= 0.0 || bounds.height() <= 0.0) {
                  continue;
                }
                stroke.points.push_back({
                    static_cast<int>((point.x() - bounds.left()) /
                                     bounds.width() * sourceMat.cols),
                    static_cast<int>((point.y() - bounds.top()) /
                                     bounds.height() * sourceMat.rows)});
              }
              if (!stroke.points.empty()) {
                const int64_t frame = static_cast<int64_t>(layer->currentFrame());
                if (impl_->rotoBrushLayerId_ != layer->id() ||
                    impl_->rotoBrushFrame_ != frame) {
                  if (impl_->rotoBrushLayerId_ == layer->id() &&
                      impl_->rotoBrushMaskIndex_ >= 0 &&
                      impl_->rotoBrushMaskIndex_ < layer->maskCount()) {
                    impl_->beginMaskEditTransaction(layer);
                    layer->removeMask(impl_->rotoBrushMaskIndex_);
                    layer->changed();
                  }
                  impl_->rotoBrushStrokes_.clear();
                  impl_->rotoBrushMaskIndex_ = -1;
                }
                impl_->rotoBrushStrokes_.push_back(std::move(stroke));
                impl_->rotoBrushSource_ = sourceMat.clone();
                impl_->rotoBrushEngine_->updateBaseFrame(
                    &sourceMat, impl_->rotoBrushStrokes_);
                impl_->rotoBrushEngine_->refineCurrentMask(1);
                impl_->rotoBrushMask_ = static_cast<const cv::Mat*>(
                    impl_->rotoBrushEngine_->currentMask())->clone();
                impl_->rotoBrushLayerId_ = layer->id();
                impl_->rotoBrushFrame_ = frame;
                MaskConversionParams conversion;
                conversion.simplificationTolerance = 1.5f;
                conversion.minPathVertices = 3;
                auto paths = MaskPath::fromAlphaMask(&impl_->rotoBrushMask_, conversion);
                if (!paths.empty() && bounds.width() > 0.0 && bounds.height() > 0.0) {
                  LayerMask generatedMask;
                  for (auto &path : paths) {
                    for (int i = 0; i < path.vertexCount(); ++i) {
                      MaskVertex vertex = path.vertex(i);
                      vertex.position = QPointF(
                          bounds.left() + vertex.position.x() * bounds.width(),
                          bounds.top() + vertex.position.y() * bounds.height());
                      vertex.inTangent = QPointF(
                          vertex.inTangent.x() * bounds.width(),
                          vertex.inTangent.y() * bounds.height());
                      vertex.outTangent = QPointF(
                          vertex.outTangent.x() * bounds.width(),
                          vertex.outTangent.y() * bounds.height());
                      path.setVertex(i, vertex);
                    }
                    generatedMask.addMaskPath(path);
                  }
                  impl_->beginMaskEditTransaction(layer);
                  if (impl_->rotoBrushLayerId_ == layer->id() &&
                      impl_->rotoBrushMaskIndex_ >= 0 &&
                      impl_->rotoBrushMaskIndex_ < layer->maskCount()) {
                    layer->setMask(impl_->rotoBrushMaskIndex_, generatedMask);
                  } else {
                    layer->addMask(generatedMask);
                    impl_->rotoBrushMaskIndex_ = layer->maskCount() - 1;
                  }
                  layer->changed();
                  setInfoOverlayText(
                      QStringLiteral("Roto Brush"),
                      QStringLiteral("Mask generated • %1 contour(s)")
                          .arg(generatedMask.maskPathCount()));
                } else if (impl_->rotoBrushMaskIndex_ >= 0 &&
                           impl_->rotoBrushMaskIndex_ < layer->maskCount()) {
                  // A later prompt may eliminate all foreground pixels. Do
                  // not leave the previous generated contour visible.
                  impl_->beginMaskEditTransaction(layer);
                  layer->removeMask(impl_->rotoBrushMaskIndex_);
                  impl_->rotoBrushMaskIndex_ = -1;
                  layer->changed();
                  setInfoOverlayText(
                      QStringLiteral("Roto Brush"),
                      QStringLiteral("No foreground contour detected"));
                }
              }
            }
          }
        }
      }
    }
    impl_->brushCursorVisible_ = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->textToolCandidate_) {
    const QRectF rect =
        QRectF(QPointF(impl_->textToolStartCanvas_.x,
                       impl_->textToolStartCanvas_.y),
               QPointF(impl_->textToolCurrentCanvas_.x,
                       impl_->textToolCurrentCanvas_.y)).normalized();
    const bool boxText = impl_->textToolDragging_ && rect.width() >= 2.0 &&
                         rect.height() >= 2.0;
    createTextLayerAtCanvas(
        impl_->textToolStartCanvas_,
        boxText ? QSizeF(rect.width(), rect.height()) : QSizeF());
    impl_->textToolCandidate_ = false;
    impl_->textToolDragging_ = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->isMaskRubberBandSelecting_) {
    const auto comp = impl_->previewPipeline_.composition();
    auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
    if (layer) {
      const QRectF rect = QRectF(
          ArtifactCore::Coordinates::toQPointF(
              impl_->maskRubberBandStartCanvas_),
          ArtifactCore::Coordinates::toQPointF(
              impl_->maskRubberBandCurrentCanvas_)).normalized();
      const QTransform globalTransform = layer->getGlobalTransform();
      for (int maskIndex = 0; maskIndex < layer->maskCount(); ++maskIndex) {
        const LayerMask mask = layer->mask(maskIndex);
        for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
          const MaskPath path = mask.maskPath(pathIndex);
          for (int vertexIndex = 0; vertexIndex < path.vertexCount(); ++vertexIndex) {
            const QPointF canvasPos =
                globalTransform.map(path.vertex(vertexIndex).position);
            if (rect.contains(canvasPos)) {
              impl_->selectedMaskVertices_.emplace_back(
                  maskIndex, pathIndex, vertexIndex);
            }
          }
        }
      }
    }
    impl_->isMaskRubberBandSelecting_ = false;
    impl_->maskRubberBandCandidate_ = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }
  impl_->maskRubberBandCandidate_ = false;

  if (impl_->designReorderActive_) {
    const LayerID layerId = impl_->designReorderLayerId_;
    const int targetIndex = impl_->designReorderTargetIndex_;
    impl_->designReorderActive_ = false;
    impl_->designReorderLayerId_ = LayerID::Nil();
    impl_->designReorderParentId_ = LayerID::Nil();
    impl_->designReorderTargetIndex_ = -1;
    bool reordered = false;
    if (targetIndex >= 0) {
      if (auto *service = ArtifactProjectService::instance()) {
        reordered = service->moveLayerInCurrentComposition(layerId, targetIndex);
      }
    }
    setInfoOverlayText(QStringLiteral("Design"),
                       reordered ? QStringLiteral("Auto Layout order updated")
                                 : QStringLiteral("Auto Layout order unchanged"));
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }



  // Finish MotionSketch on mouse release

  {

    auto* app = ArtifactApplicationManager::instance();

    if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {

      app->motionSketchTool()->addSample(
          ArtifactCore::Coordinates::toQPointF(
              impl_->motionSketchLastCanvasPos_));
      app->motionSketchTool()->finishSketch();

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

  if (impl_->isDraggingMotionPathTangent_) {
    auto layer = impl_->draggingMotionPathTangentLayer_.lock();
    if (layer) {
      const auto time = gizmoTransformTime(layer, impl_->draggingMotionPathTangentFrame_);
      MotionPathTangentSnapshot after;
      after.present = layer->transform3D().positionKeyFrameSpatialTangentsAt(
          time, after.tangents);
      const auto &before = impl_->draggingMotionPathTangentBefore_;
      const bool changed = before.present != after.present ||
          std::abs(before.tangents.inTangent.x - after.tangents.inTangent.x) > 0.0001f ||
          std::abs(before.tangents.inTangent.y - after.tangents.inTangent.y) > 0.0001f ||
          std::abs(before.tangents.outTangent.x - after.tangents.outTangent.x) > 0.0001f ||
          std::abs(before.tangents.outTangent.y - after.tangents.outTangent.y) > 0.0001f;
      if (changed) {
        auto *mgr = UndoManager::instance();
        if (mgr && !mgr->push(std::make_unique<MotionPathTangentUndoCommand>(
                                  layer, time,
                                  before, after))) {
          const auto restoreTime = time;
          auto &transform = layer->transform3D();
          if (before.present) {
            transform.setPositionKeyFrameSpatialTangentsAt(
                restoreTime, before.tangents);
          } else {
            transform.removePositionKeyFrameSpatialTangentsAt(restoreTime);
          }
          layer->setDirty(LayerDirtyFlag::Transform);
          layer->changed();
        }
      }
    }
    impl_->clearMotionPathTangentDragState();
    impl_->motionPathCache_.valid = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }

  if (impl_->isDraggingShapePathVertex_) {
    endShapePathVertexDrag();
    return;
  }
  // F2: parameter and polygon drag sessions end with their own Undo commit.
  if (impl_->shapeParamDragMode_ != 0) {
    endShapeParamDrag();
    return;
  }
  // F5: operator drag session ends with its own Undo commit.
  if (impl_->shapeOpDragOp_ >= 0) {
    endShapeOperatorDrag();
    return;
  }
  if (impl_->isDraggingShapePolygon_) {
    endShapePolygonDrag();
    return;
  }
  if (impl_->isDraggingCameraPoi_) {
    auto poiLayer = impl_->draggingCameraPoiLayer_.lock();
    if (poiLayer) {
      if (auto *camera =
              dynamic_cast<ArtifactCameraLayer *>(poiLayer.get())) {
        const auto after = camera->pointOfInterest();
        const auto poiDelta = after - impl_->cameraPoiBefore_;
        if (poiDelta.x * poiDelta.x + poiDelta.y * poiDelta.y +
                poiDelta.z * poiDelta.z > 0.000001f) {
          auto *mgr = UndoManager::instance();
          if (mgr && !mgr->push(std::make_unique<CameraPoiUndoCommand>(
                                    poiLayer, impl_->cameraPoiBefore_, after))) {
            camera->setPointOfInterest(impl_->cameraPoiBefore_);
            impl_->publishLayerModified(poiLayer, true);
          }
        }
      }
    }
    impl_->clearCameraPoiDrag();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->isDraggingPastPlaneFrame_) {
    auto layer = impl_->draggingPastPlaneLayer_.lock();
    if (layer) {
      const auto time = gizmoTransformTime(layer, impl_->draggingPastPlaneFrame_);
      const auto &transform = layer->transform3D();
      MotionPathPositionSnapshot after;
      after.hasPositionKey = transform.hasPositionKeyFrameAt(time);
      after.position.x = transform.positionXAt(time);
      after.position.y = transform.positionYAt(time);
      auto *mgr = UndoManager::instance();
      if (mgr && !mgr->push(std::make_unique<MotionPathUndoCommand>(
                                layer, time,
                                impl_->draggingPastPlaneBefore_, after))) {
        const auto &before = impl_->draggingPastPlaneBefore_;
        auto &mutableTransform = layer->transform3D();
        if (before.hasPositionKey) {
          mutableTransform.setPositionKeyFrameValueAt(time, before.position.x, before.position.y);
        } else {
          mutableTransform.removePositionKeyFrameAt(time);
        }
        layer->setDirty(LayerDirtyFlag::Transform);
        layer->changed();
      }
    }
    impl_->clearPastPlaneFrameDrag();
    impl_->motionPathCache_.valid = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->isDraggingMotionPathKeyframe_) {

    auto layer = impl_->draggingMotionPathLayer_.lock();

    if (layer) {

      const auto time = gizmoTransformTime(layer, impl_->draggingMotionPathFrame_);

      const auto &t3d = layer->transform3D();

      MotionPathPositionSnapshot after;

      after.hasPositionKey = t3d.hasPositionKeyFrameAt(time);

      after.position.x = t3d.positionXAt(time);

      after.position.y = t3d.positionYAt(time);



      const bool changed = impl_->draggingMotionPathBefore_.hasPositionKey != after.hasPositionKey ||

                           std::abs(impl_->draggingMotionPathBefore_.position.x - after.position.x) > 0.0001f ||

                           std::abs(impl_->draggingMotionPathBefore_.position.y - after.position.y) > 0.0001f;

      if (changed) {
        auto *mgr = UndoManager::instance();
        bool pushed = mgr == nullptr;
        if (mgr) {
          if (impl_->draggingMotionPathGroupBefore_.size() > 1) {
            QVector<MotionPathKeySnapshot> groupAfter;
            for (const auto &beforeKey :
                 impl_->draggingMotionPathGroupBefore_) {
              const auto keyTime = gizmoTransformTime(layer, beforeKey.frame);
              MotionPathKeySnapshot current;
              current.frame = beforeKey.frame;
              current.value.hasPositionKey = t3d.hasPositionKeyFrameAt(keyTime);
              current.value.position.x = t3d.positionXAt(keyTime);
              current.value.position.y = t3d.positionYAt(keyTime);
               current.hasTangents =
                   t3d.positionKeyFrameSpatialTangentsAt(keyTime,
                                                         current.tangents);
               groupAfter.push_back(current);
             }
            pushed = mgr->push(std::make_unique<MotionPathGroupUndoCommand>(
                layer, time.scale(), impl_->draggingMotionPathGroupBefore_, groupAfter));
          } else {
            pushed = mgr->push(std::make_unique<MotionPathUndoCommand>(
                layer, time,
                impl_->draggingMotionPathBefore_, after));
          }
        }
        if (!pushed) {
          auto &mutableTransform = layer->transform3D();
          if (impl_->draggingMotionPathGroupBefore_.size() > 1) {
            for (const auto &beforeKey : impl_->draggingMotionPathGroupBefore_) {
              const auto keyTime = gizmoTransformTime(layer, beforeKey.frame);
              if (beforeKey.value.hasPositionKey) {
                mutableTransform.setPositionKeyFrameValueAt(
                    keyTime, beforeKey.value.position.x, beforeKey.value.position.y);
                if (beforeKey.hasTangents) {
                  mutableTransform.setPositionKeyFrameSpatialTangentsAt(
                      keyTime, beforeKey.tangents);
                }
              } else {
                mutableTransform.removePositionKeyFrameAt(keyTime);
              }
            }
          } else {
            const auto &before = impl_->draggingMotionPathBefore_;
            if (before.hasPositionKey) {
              mutableTransform.setPositionKeyFrameValueAt(
                  time, before.position.x, before.position.y);
            } else {
              mutableTransform.removePositionKeyFrameAt(time);
            }
          }
          layer->setDirty(LayerDirtyFlag::Transform);
          layer->changed();
        } else if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
          ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
              LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                                LayerChangedEvent::ChangeType::Modified});
        }
      }

    }

    impl_->clearMotionPathDragState();

    impl_->motionPathCache_.valid = false;

    impl_->invalidateOverlayComposite();

    markRenderDirty();

  }

  if (impl_->isDraggingTransformField_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp && !impl_->draggingTransformFieldId_.isEmpty()) {
      for (const auto &field : comp->transformFields()) {
        if (field.fieldId != impl_->draggingTransformFieldId_) {
          continue;
        }
        const bool changed =
            field.center != impl_->draggingTransformFieldBefore_.center ||
            std::abs(field.radius - impl_->draggingTransformFieldBefore_.radius) >
                0.0001 ||
            std::abs(field.secondaryRadius -
                     impl_->draggingTransformFieldBefore_.secondaryRadius) >
                0.0001 ||
            std::abs(field.rotationDegrees -
                     impl_->draggingTransformFieldBefore_.rotationDegrees) >
                0.0001 ||
            std::abs(field.timeOffsetSeconds -
                     impl_->draggingTransformFieldBefore_.timeOffsetSeconds) >
                0.0001 ||
            std::abs(field.strength - impl_->draggingTransformFieldBefore_.strength) >
                0.0001 ||
            std::abs(field.expansion -
                     impl_->draggingTransformFieldBefore_.expansion) >
                0.0001 ||
            std::abs(field.edgeScale -
                     impl_->draggingTransformFieldBefore_.edgeScale) >
                0.0001 ||
            field.blendMode != impl_->draggingTransformFieldBefore_.blendMode ||
            field.invert != impl_->draggingTransformFieldBefore_.invert ||
            field.enabled != impl_->draggingTransformFieldBefore_.enabled ||
            field.coordinateParentLayerId !=
                impl_->draggingTransformFieldBefore_.coordinateParentLayerId ||
            field.targetLayerIds !=
                 impl_->draggingTransformFieldBefore_.targetLayerIds ||
            field.shape != impl_->draggingTransformFieldBefore_.shape ||
            field.displayName != impl_->draggingTransformFieldBefore_.displayName;
        if (changed) {
          auto *mgr = UndoManager::instance();
          if (mgr && !mgr->push(std::make_unique<TransformFieldUndoCommand>(
                                    ArtifactCompositionWeakPtr(comp),
                                    impl_->draggingTransformFieldBefore_, field))) {
            comp->addTransformField(impl_->draggingTransformFieldBefore_);
            comp->changed();
          }
        }
        break;
      }
    }

    impl_->clearTransformFieldDragState();
    impl_->invalidateOverlayComposite();
    finishViewportInteraction();
    markRenderDirty();
    return;
  }



  impl_->isDraggingLayer_ = false;

  auto comp = impl_->previewPipeline_.composition();



  auto syncPrimarySelectionLayer = [this, comp](const ArtifactAbstractLayerPtr &primaryLayer) {

    const LayerID primaryId = primaryLayer ? primaryLayer->id() : LayerID::Nil();

    if (impl_->selectedLayerId_ != primaryId) {

      impl_->clearPendingMaskCreation();

      if (primaryLayer &&
          (primaryLayer->is3D() ||
           layerUsesProjectedFrameGizmo(primaryLayer))) {
        setGizmoMode(TransformGizmo::Mode::All);
      }

      impl_->selectedLayerId_ = primaryId;

      impl_->invalidateOverlayComposite();

    }

    impl_->syncSelectedLayerOverlayState(comp);

    markRenderDirty();

  };



  if (impl_->isShapeVertexMarqueeSelecting_) {
    const QRectF rect = impl_->shapeVertexMarqueeCanvasRect().normalized();
    if (comp && impl_->renderer_ && rect.width() >= 2.0 &&
        rect.height() >= 2.0) {
      auto selectedLayer = !impl_->selectedLayerId_.isNil()
          ? comp->layerById(impl_->selectedLayerId_)
          : ArtifactAbstractLayerPtr{};
      auto *shape = selectedLayer
          ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
          : nullptr;
      if (shape && !selectedLayer->isLocked() &&
          !selectedLayer->isSelectionLocked()) {
        const QTransform globalTransform = selectedLayer->getGlobalTransform();
        auto toggleIndex = [&](int index) {
          const auto it = std::find(impl_->selectedShapePathVertices_.begin(),
                                    impl_->selectedShapePathVertices_.end(),
                                    index);
          if (impl_->shapeVertexMarqueeSelectionMode_ == SelectionMode::Toggle) {
            if (it != impl_->selectedShapePathVertices_.end()) {
              impl_->selectedShapePathVertices_.erase(it);
            } else {
              impl_->selectedShapePathVertices_.push_back(index);
            }
          } else if (it == impl_->selectedShapePathVertices_.end()) {
            impl_->selectedShapePathVertices_.push_back(index);
          }
        };
        if (impl_->shapeVertexMarqueeSelectionMode_ == SelectionMode::Replace) {
          impl_->selectedShapePathVertices_.clear();
        }
        if (shape->hasCustomPath()) {
          const auto vertices = shape->customPathVertices();
          for (int index = 0; index < static_cast<int>(vertices.size()); ++index) {
            if (rect.contains(globalTransform.map(vertices[static_cast<size_t>(index)].pos))) {
              toggleIndex(index);
            }
          }
        } else if (shape->hasCustomPolygon()) {
          const auto points = shape->customPolygonPoints();
          for (int index = 0; index < static_cast<int>(points.size()); ++index) {
            if (rect.contains(globalTransform.map(points[static_cast<size_t>(index)]))) {
              toggleIndex(index);
            }
          }
        }
      }
    }
    impl_->clearSelectionGestureState();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->isRubberBandSelecting_) {

    auto *selection =

        ArtifactApplicationManager::instance()

            ? ArtifactApplicationManager::instance()->layerSelectionManager()

            : nullptr;

    if (comp && selection && impl_->renderer_) {

      const QRectF rect = impl_->rubberBandCanvasRect().normalized();
      const QPolygonF lasso = impl_->lassoCanvasPolygon();

      const auto currentFrame = currentFrameForComposition(comp);

      const auto &layers = comp->allLayerRef();

      QVector<ArtifactAbstractLayerPtr> hits;

      hits.reserve(layers.size());

      for (const auto &layer : layers) {

        if (!layerIntersectsCanvasRect(layer, rect, currentFrame)) {

          continue;

        }
        if (impl_->isLassoSelecting_ &&
            (lasso.size() < 3 ||
             !lasso.containsPoint(layer->transformedBoundingBox().center(),
                                  Qt::OddEvenFill))) {
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

        const ArtifactAbstractLayerPtr layerRef = layer;

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

      syncPrimarySelectionLayer(primaryLayer);

      impl_->sync2DGizmosForLayer(primaryLayer);

      if (impl_->gizmo3D_ && primaryLayer) {

        impl_->syncGizmo3DFromLayer(primaryLayer);

      }

    }

    impl_->clearSelectionGestureState();

    markRenderDirty();

    return;

  }



  if (impl_->rectangleToolDragging_) {

    auto comp = impl_->previewPipeline_.composition();

    auto *selectionManager = ArtifactApplicationManager::instance()

                                 ? ArtifactApplicationManager::instance()

                                       ->layerSelectionManager()

                                 : nullptr;

    const QRectF rect = dragRectFromPoints(impl_->rectangleToolStartCanvasPos_,

                                           impl_->rectangleToolCurrentCanvasPos_,
                                           impl_->rectangleToolFromCenter_);

    const bool meaningfulRect = isMeaningfulDragRect(rect);


    ArtifactAbstractLayerPtr committedMaskLayer;

    if (impl_->rectangleToolMode_ == RectangleToolMode::Mask ||
        impl_->rectangleToolMode_ == RectangleToolMode::EllipseMask) {

      auto layer = impl_->rectangleToolTargetLayer_.lock();

      if (layer && meaningfulRect) {

        const QTransform globalTransform = layer->getGlobalTransform();

        bool invertible = false;

        const QTransform invTransform = globalTransform.inverted(&invertible);

        MaskPath path;

        if (impl_->rectangleToolMode_ == RectangleToolMode::EllipseMask) {
          QPainterPath ellipsePath;
          ellipsePath.addEllipse(rect.normalized());
          const auto ellipsePaths = MaskPath::fromQPainterPath(ellipsePath);
          if (!ellipsePaths.empty()) {
            path = ellipsePaths.front();
            for (int i = 0; i < path.vertexCount(); ++i) {
              MaskVertex vertex = path.vertex(i);
              vertex.position = invertible ? invTransform.map(vertex.position)
                                           : vertex.position;
              path.setVertex(i, vertex);
            }
          }
        } else {

        for (const auto &corner : rectCorners(rect)) {

          const QPointF localCorner =

              invertible ? invTransform.map(corner) : corner;

          MaskVertex vertex;

          vertex.position = localCorner;

          vertex.inTangent = QPointF(0.0, 0.0);

          vertex.outTangent = QPointF(0.0, 0.0);

          path.addVertex(vertex);

        }
        }

        path.setClosed(true);



        LayerMask mask;

        mask.addMaskPath(path);

        layer->addMask(mask);

        impl_->markMaskEditDirty();
        committedMaskLayer = layer;

      }

      const bool committed = impl_->commitMaskEditTransaction();
      if (committed && committedMaskLayer) {
        impl_->publishLayerModified(committedMaskLayer, true);
      }

    } else if ((impl_->rectangleToolMode_ == RectangleToolMode::Shape ||
                impl_->rectangleToolMode_ == RectangleToolMode::EllipseShape ||
                impl_->rectangleToolMode_ == RectangleToolMode::StarShape ||
                impl_->rectangleToolMode_ == RectangleToolMode::PolygonShape ||
                impl_->rectangleToolMode_ == RectangleToolMode::TriangleShape) &&

               meaningfulRect && comp) {

      QString baseName = QStringLiteral("Rectangle");
      switch (impl_->rectangleToolShapeType_) {
      case ShapeType::Rect: baseName = QStringLiteral("Rectangle"); break;
      case ShapeType::Ellipse: baseName = QStringLiteral("Ellipse"); break;
      case ShapeType::Star: baseName = QStringLiteral("Star"); break;
      case ShapeType::Polygon: baseName = QStringLiteral("Polygon"); break;
      case ShapeType::Line: baseName = QStringLiteral("Line"); break;
      case ShapeType::Triangle: baseName = QStringLiteral("Triangle"); break;
      case ShapeType::Square: baseName = QStringLiteral("Square"); break;
      case ShapeType::Arrow: baseName = QStringLiteral("Arrow"); break;
      case ShapeType::Heart: baseName = QStringLiteral("Heart"); break;
      case ShapeType::Diamond: baseName = QStringLiteral("Diamond"); break;
      case ShapeType::Gear: baseName = QStringLiteral("Gear"); break;
      case ShapeType::Cross: baseName = QStringLiteral("Cross"); break;
      }
      const QString layerName = uniqueLayerNameForCurrentComposition(baseName);

      if (auto *service = ArtifactProjectService::instance()) {

        ArtifactLayerInitParams params(layerName, LayerType::Shape);

        service->addLayerToCurrentComposition(params, true);



        ArtifactAbstractLayerPtr createdLayer =

            selectionManager ? selectionManager->currentLayer()

                             : ArtifactAbstractLayerPtr{};

        if (!createdLayer && !comp->allLayer().isEmpty()) {

          createdLayer = comp->allLayer().back();

        }



        if (createdLayer) {

          const QRectF normalizedRect = rect.normalized();

          const auto currentPos = createdLayer->position3D();

          createdLayer->setPosition3D({
              static_cast<float>(normalizedRect.left()),
              static_cast<float>(normalizedRect.top()), currentPos.z});

          if (auto shapeLayer =

                  ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(createdLayer)) {

            shapeLayer->setShapeType(impl_->rectangleToolShapeType_);

            if (impl_->rectangleToolShapeType_ == ShapeType::Arrow) {
              shapeLayer->setStarInnerRadius(0.30f);
            } else if (impl_->rectangleToolShapeType_ == ShapeType::Heart) {
              shapeLayer->setStarInnerRadius(0.50f);
            } else if (impl_->rectangleToolShapeType_ == ShapeType::Gear) {
              shapeLayer->setPolygonSides(8);
              shapeLayer->setStarInnerRadius(0.70f);
            } else if (impl_->rectangleToolShapeType_ == ShapeType::Cross) {
              shapeLayer->setStarInnerRadius(0.32f);
            }

            shapeLayer->setSize(

                std::max(1, static_cast<int>(std::lround(normalizedRect.width()))),

                std::max(1, static_cast<int>(std::lround(normalizedRect.height()))));
            shapeLayer->setCornerRadius(std::clamp(
                impl_->rectangleToolRoundness_, 0.0f,
                0.5f * static_cast<float>(std::min(normalizedRect.width(),
                                                    normalizedRect.height()))));

          }

          setSelectedLayerId(createdLayer->id());

          impl_->sync2DGizmosForLayer(createdLayer);

          if (impl_->gizmo3D_) {

            impl_->syncGizmo3DFromLayer(createdLayer);

          }

        }

      }

    }



    impl_->clearRectangleToolSession();

    markRenderDirty();

    return;

  }



  const bool publishFinalMaskEditChange = impl_->maskEditDirty_;

  const auto maskEditLayer = impl_->maskEditLayer_.lock();

  impl_->isDraggingVertex_ = false;

  impl_->isDraggingMaskHandle_ = false;

  impl_->isDraggingMaskGeometry_ = false;

  impl_->draggingMaskIndex_ = -1;

  impl_->draggingPathIndex_ = -1;

  impl_->draggingVertexIndex_ = -1;

  impl_->draggingMaskHandleType_ = -1;

  impl_->draggingMaskHandleStartFeather_ = {};

  impl_->commitMaskEditTransaction();

  if (publishFinalMaskEditChange) {

    impl_->publishLayerModified(maskEditLayer, true);

  }

  impl_->resetLayerMutationNotify();



  if (impl_->gizmo3D_ && releasingOwner == GizmoOwner::Transform3D) {

    const bool wasDragging = impl_->gizmoDragActive_;

    impl_->gizmoDragActive_ = false;

    if (impl_->gizmoModalTransformActive_) {
      impl_->gizmo3D_->setSpace(impl_->gizmoModalPreviousSpace_);
      impl_->gizmoModalTransformActive_ = false;
    }

    impl_->gizmo3D_->endDrag();
    impl_->gizmo3D_->setInteractionModifiers(false, false);
    if (impl_->projectedFrameForcedLocal_) {
      impl_->gizmo3D_->setSpace(impl_->projectedFramePreviousSpace_);
      impl_->projectedFrameForcedLocal_ = false;
    }
    if (impl_->projectedFrameForcedMode_) {
      impl_->gizmo3D_->setMode(impl_->projectedFramePreviousMode_);
      impl_->projectedFrameForcedMode_ = false;
    }
    impl_->projectedFrameHandle_ = TransformGizmo::HandleType::None;
    impl_->projectedFrameMove_ = false;
    impl_->projectedFrameCorrectedLocalPositionValid_ = false;
    impl_->projectedFrameLastPointerValid_ = false;
    impl_->projectedFrameSnapCache_.invalidate();
    impl_->projectedFrameScalePointerBasisValid_ = false;
    impl_->projectedFrameSnapVerticalValid_ = false;
    impl_->projectedFrameSnapHorizontalValid_ = false;

    // One Undo entry per anchor drag, pushed on release.  It must run before
    // the gizmo-layer bookkeeping below clears the drag state.
    const bool wasAnchorDrag = wasDragging && impl_->gizmoAnchorDragHasResult_;
    if (wasAnchorDrag) {
      commitProjectedFrameAnchorDrag();
    }
    impl_->gizmoAnchorDragStartLayer_.reset();

    if (wasDragging && impl_->gizmoUndoSnapshotValid_ &&
        impl_->gizmoGroupTransformActive_ && !wasAnchorDrag) {
      std::vector<GizmoGroupUndoEntry> entries;
      entries.reserve(impl_->gizmoGroupLayers_.size());
      for (const auto &state : impl_->gizmoGroupLayers_) {
        if (!state.layer) continue;
        GizmoTransformSnapshot after;
        after.is3D = state.before.is3D;
        const auto &transform = state.layer->transform3D();
        const auto time = gizmoTransformTime(state.layer, state.frame);
        const auto evaluated = transform.snapshotAt(time);
        after.position = {evaluated.positionX, evaluated.positionY,
                          evaluated.positionZ};
        after.rotation = after.is3D
             ? ArtifactCore::Units::eulerDegreesFromQVector3D(
                   state.layer->rotation3D())
            : ArtifactCore::Units::EulerDegrees3{
                  {}, {}, {transform.rotationAt(time)}};
        after.scale = {transform.scaleXAt(time), transform.scaleYAt(time),
                       after.is3D ? transform.snapshotAt(time).scaleZ : 1.0f};
        captureGizmoKeyState(state.layer, state.frame, after);
        const bool changed =
            layerLocalPositionDeltaLengthSquared(state.before.position,
                                                 after.position) >
                0.000001f ||
            eulerDegreesDeltaLengthSquared(state.before.rotation,
                                           after.rotation) > 0.000001f ||
            scale3DeltaLengthSquared(state.before.scale, after.scale) >
                0.000001f ||
            state.before.hasPositionKey != after.hasPositionKey ||
            state.before.hasRotationKey != after.hasRotationKey ||
            state.before.hasScaleKey != after.hasScaleKey;
        if (changed) {
          entries.push_back(GizmoGroupUndoEntry{
              state.layer, state.frame, state.before, after});
        }
      }
      if (!entries.empty()) {
        const auto rollbackEntries = entries;
        bool pushed = UndoManager::instance() == nullptr;
        if (auto *mgr = UndoManager::instance()) {
          pushed = mgr->push(std::make_unique<GizmoGroupTransformUndoCommand>(
              std::move(entries)));
        }
        if (!pushed) {
          for (const auto &entry : rollbackEntries) {
            if (const auto target = entry.layer.lock()) {
              GizmoTransformUndoCommand rollback(
                  target, entry.frame, entry.before, entry.after);
              rollback.undo();
            }
          }
        }
      }
      impl_->gizmoGroupTransformActive_ = false;
      if (impl_->gizmoGroupProjectedBasisValid_) {
        impl_->gizmo3D_->setLocalBasis(
            ArtifactCore::Coordinates::WorldVector3{},
            ArtifactCore::Coordinates::WorldVector3{},
            ArtifactCore::Coordinates::WorldVector3{});
      }
      impl_->gizmoGroupProjectedBasisValid_ = false;
      impl_->gizmoGroupLayers_.clear();
      impl_->gizmoUndoLayer_.reset();
      impl_->gizmoUndoSnapshotValid_ = false;
    } else if (wasDragging && impl_->gizmoUndoSnapshotValid_ &&
               !wasAnchorDrag) {
      // Anchor drags are excluded: they already pushed a single
      // AnchorPointUndoCommand above, and they deliberately also move position
      // (the compensation term), so a GizmoTransformUndoCommand here would
      // produce a second, overlapping undo entry for the same gesture.
      GizmoTransformSnapshot after;
      after.is3D = impl_->gizmoUndoBefore_.is3D;
      GizmoTransformSnapshot before = impl_->gizmoLayerTransformBefore_;
      if (const auto layer = impl_->gizmoUndoLayer_.lock()) {
        const auto &transform = layer->transform3D();
        const auto time = gizmoTransformTime(layer, impl_->gizmoUndoFrame_);
        const auto evaluated = transform.snapshotAt(time);
        after.position = {evaluated.positionX, evaluated.positionY,
                          evaluated.positionZ};
        after.rotation = layer->is3D()
             ? ArtifactCore::Units::eulerDegreesFromQVector3D(
                   layer->rotation3D())
            : ArtifactCore::Units::EulerDegrees3{
                  {}, {}, {transform.rotationAt(time)}};
        after.scale = {transform.scaleXAt(time), transform.scaleYAt(time),
                       layer->is3D() ? transform.snapshotAt(time).scaleZ
                                     : 1.0f};
        captureGizmoKeyState(layer, impl_->gizmoUndoFrame_, after);
      }
      const auto changed = [](const GizmoTransformSnapshot& lhs,
                              const GizmoTransformSnapshot& rhs) {
        return lhs.is3D != rhs.is3D ||
               layerLocalPositionDeltaLengthSquared(lhs.position,
                                                    rhs.position) >
                   0.000001f ||
               eulerDegreesDeltaLengthSquared(lhs.rotation, rhs.rotation) >
                   0.000001f ||
               scale3DeltaLengthSquared(lhs.scale, rhs.scale) > 0.000001f ||
               lhs.hasPositionKey != rhs.hasPositionKey ||
               lhs.hasRotationKey != rhs.hasRotationKey ||
               lhs.hasScaleKey != rhs.hasScaleKey;
      };
      if (changed(before, after)) {
        auto* mgr = UndoManager::instance();
        if (mgr && !mgr->push(std::make_unique<GizmoTransformUndoCommand>(
                                  impl_->gizmoUndoLayer_.lock(),
                                  impl_->gizmoUndoFrame_, before, after))) {
          if (const auto layer = impl_->gizmoUndoLayer_.lock()) {
            GizmoTransformUndoCommand rollback(
                layer, impl_->gizmoUndoFrame_, before, after);
            rollback.undo();
          }
        }
      }
      // Deliver the final transform even when the last pointer notification
      // was coalesced. Inspector/timeline observers must see the final value.
      impl_->publishLayerModified(impl_->gizmoUndoLayer_.lock(), true);
      impl_->gizmoUndoLayer_.reset();
      impl_->gizmoUndoSnapshotValid_ = false;
    }
    if (!wasDragging) {
      impl_->gizmoUndoLayer_.reset();
      impl_->gizmoUndoSnapshotValid_ = false;
      impl_->gizmoGroupTransformActive_ = false;
      if (impl_->gizmoGroupProjectedBasisValid_) {
        impl_->gizmo3D_->setLocalBasis(
            ArtifactCore::Coordinates::WorldVector3{},
            ArtifactCore::Coordinates::WorldVector3{},
            ArtifactCore::Coordinates::WorldVector3{});
      }
      impl_->gizmoGroupProjectedBasisValid_ = false;
      impl_->gizmoGroupLayers_.clear();
    }

    impl_->invalidateOverlayComposite();

    if (wasDragging) {

      finishViewportInteraction();

    }

    markRenderDirty();

  }



  if (impl_->contentGizmo_ && releasingOwner == GizmoOwner::Content) {

    impl_->contentGizmo_->handleMouseRelease();

    impl_->invalidateOverlayComposite();

    finishViewportInteraction();

    markRenderDirty();

  }



  if (impl_->textGizmo_ && releasingOwner == GizmoOwner::Text) {

    impl_->textGizmo_->handleMouseRelease();

    impl_->invalidateOverlayComposite();

    finishViewportInteraction();

    markRenderDirty();

  }



  if (impl_->gizmo_ && releasingOwner == GizmoOwner::Transform2D) {

    const bool wasDragging = impl_->gizmoDragActive_;

    impl_->gizmoDragActive_ = false;

    impl_->gizmo_->handleMouseRelease();

    impl_->invalidateOverlayComposite();

    if (wasDragging) {

      finishViewportInteraction();

    }

    markRenderDirty();

  }



  if (impl_->trackerGizmo_) {

    const bool wasTrackerDragging = impl_->trackerGizmoDragActive_;
    const bool wasPathPointDrag =
        impl_->trackerGizmo_->activeHandle() ==
        ArtifactPointTrackerGizmo::HandleType::PathPoint;

    impl_->trackerGizmoDragActive_ = false;

    impl_->trackerGizmo_->handleMouseRelease();

    if (wasTrackerDragging && wasPathPointDrag && impl_->trackerMotionTracker_ &&
        !impl_->trackerPreDragJson_.isEmpty()) {
      pushTrackerUndo(impl_->trackerPreDragJson_,
                      impl_->trackerMotionTracker_->toJson());
    }
    if (wasTrackerDragging && !wasPathPointDrag &&
        impl_->trackerMotionTracker_ && impl_->trackerGizmo_) {
      if (const auto comp = impl_->previewPipeline_.composition()) {
        persistProjectTrackerGeometry(
            comp->id(), impl_->trackerMotionTracker_->id(),
            impl_->trackerGizmo_->state());
      }
    }
    impl_->trackerPreDragJson_.clear();

    impl_->invalidateOverlayComposite();

    if (wasTrackerDragging) {

      finishViewportInteraction();

    }

    markRenderDirty();

  }

}

} // namespace Artifact
