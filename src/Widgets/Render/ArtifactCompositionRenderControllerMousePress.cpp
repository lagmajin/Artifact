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

static ArtifactCore::Id hitTestRigBone(ArtifactCore::Bone2D *bone, const QPointF &localPoint,
                                float threshold) {
    if (!bone) return {};
    const QMatrix4x4 &boneMatrix = bone->globalMatrix();
    const QVector3D origin3 = boneMatrix * QVector3D(0.0f, 0.0f, 0.0f);
    const QVector3D tip3 = boneMatrix * QVector3D(0.0f, -bone->length(), 0.0f);
    const QPointF origin(origin3.x(), origin3.y());
    const QPointF tip(tip3.x(), tip3.y());
    const QPointF segment = tip - origin;
    const QPointF fromOrigin = localPoint - origin;
    const double segmentLengthSquared =
        segment.x() * segment.x() + segment.y() * segment.y();
    const double projection = segmentLengthSquared > 1.0e-6
        ? std::clamp((fromOrigin.x() * segment.x() +
                      fromOrigin.y() * segment.y()) / segmentLengthSquared,
                     0.0, 1.0)
        : 0.0;
    const QPointF nearest = origin + segment * projection;
    const QPointF distance = localPoint - nearest;
    if (std::hypot(distance.x(), distance.y()) <= threshold) {
      return bone->id();
    }
    for (auto *child : bone->children()) {
      if (const auto hit = hitTestRigBone(child, localPoint, threshold);
          !hit.isNil()) {
        return hit;
      }
    }
    return {};
}

static ArtifactCore::Id hitTestRigControl(const ArtifactCore::Rig2D &rig, const QPointF &localPoint,
                                   float threshold) {
  const double thresholdSquared = threshold * threshold;
  for (auto *control : rig.controls()) {
    if (!control || !control->enabled()) continue;
    QPointF controlPoint;
    if (control->kind() == ArtifactCore::RigControlKind::Point) {
      const QVector2D value = control->value().value<QVector2D>();
      controlPoint = QPointF(value.x(), value.y());
    } else {
      controlPoint = QPointF(0.0, 0.0);
    }
    const QPointF delta = localPoint - controlPoint;
    if (delta.x() * delta.x() + delta.y() * delta.y() <= thresholdSquared) {
      return control->id();
    }
  }
  return {};
}

} // namespace CompositionRenderControllerDetail

static SelectionMode selectionModeFromModifiers(Qt::KeyboardModifiers modifiers) {

  if (modifiers.testFlag(Qt::ControlModifier)) {

    return SelectionMode::Toggle;

  }

  if (modifiers.testFlag(Qt::ShiftModifier)) {

    return SelectionMode::Add;

  }

  return SelectionMode::Replace;

}

static LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr &layer,

                                   const QPointF &viewportPos,

                                   ArtifactIRenderer *renderer) {

  if (!layer || !renderer) {

    return LayerDragMode::None;

  }



  const QRectF bbox = layer->transformedBoundingBox();

  if (!bbox.isValid() || bbox.width() <= 0.0 || bbox.height() <= 0.0) {

    return LayerDragMode::None;

  }



  constexpr float kHandleHitSize = 16.0f;

  const auto containsHandle = [&](float x, float y) {

    const auto p = renderer->canvasToViewport({x, y});

    const QRectF rect(p.x - kHandleHitSize * 0.5f, p.y - kHandleHitSize * 0.5f,

                      kHandleHitSize, kHandleHitSize);

    return rect.contains(viewportPos);

  };



  if (containsHandle(static_cast<float>(bbox.left()),

                     static_cast<float>(bbox.top()))) {

    return LayerDragMode::ScaleTL;

  }

  if (containsHandle(static_cast<float>(bbox.right()),

                     static_cast<float>(bbox.top()))) {

    return LayerDragMode::ScaleTR;

  }

  if (containsHandle(static_cast<float>(bbox.left()),

                     static_cast<float>(bbox.bottom()))) {

    return LayerDragMode::ScaleBL;

  }

  if (containsHandle(static_cast<float>(bbox.right()),

                     static_cast<float>(bbox.bottom()))) {

    return LayerDragMode::ScaleBR;

  }



  return LayerDragMode::Move;

}

namespace {

// Shared POI-handle hit geometry for press/cursor paths. Returns the screen
// position of the selected camera's POI, or a null point when not hoverable.
QPointF cameraPoiHandleScreenPos(
    const ArtifactAbstractLayerPtr &layer,
    const QMatrix4x4 &view, const QMatrix4x4 &projection,
    const QRect &viewport) {
  auto *camera =
      layer ? dynamic_cast<ArtifactCameraLayer *>(layer.get()) : nullptr;
  if (!camera || !camera->pointOfInterestEnabled()) {
    return {};
  }
  const QVector3D screen = ViewportMath::projectToTopDown(
      ArtifactCore::Coordinates::toQVector3D(
          camera->pointOfInterestWorld()),
      view, projection, viewport);
  if (!std::isfinite(screen.x()) || !std::isfinite(screen.y()) ||
      screen.z() < 0.0f || screen.z() > 1.0f) {
    return {};
  }
  return QPointF(static_cast<double>(screen.x()),
                 static_cast<double>(screen.y()));
}

bool selectedCameraPoiHoverable(const ArtifactAbstractLayerPtr &layer) {
  auto *camera =
      layer ? dynamic_cast<ArtifactCameraLayer *>(layer.get()) : nullptr;
  return camera && camera->pointOfInterestEnabled() && !layer->isLocked() &&
         !layer->isSelectionLocked();
}

} // namespace

void CompositionRenderController::handleMousePress(QMouseEvent *event) {
  impl_->projectedFrameSnapCache_.invalidate();
  if (event && impl_->gizmoSession_.owner != GizmoOwner::None) {
    if (event->button() == Qt::RightButton) cancelGizmoInteraction();
    event->accept();
    return;
  }
  if (event && impl_->historicalScaleDragging_) {
    if (event->button() == Qt::RightButton) cancelGizmoInteraction();
    event->accept();
    return;
  }
  if (event && impl_->constructionDragLayer_) {
    if (event->button() == Qt::RightButton) cancelGizmoInteraction();
    event->accept();
    return;
  }

  if (!event || !impl_->renderer_)

    return;



  qCDebug(compositionViewLog)

      << "[MousePress] ENTER pos:" << event->position()

      << "button:" << event->button() << "modifiers:" << event->modifiers()

      << "devicePixelRatio:" << impl_->devicePixelRatio_;



  // event->position() is in logical pixels; convert to physical for rendering

  // pipeline

  const QPointF viewportPos = event->position() * impl_->devicePixelRatio_;

  auto toolManager = ArtifactApplicationManager::instance()

                         ? ArtifactApplicationManager::instance()->toolManager()

                         : nullptr;

  auto activeTool = toolManager ? toolManager->activeTool() : ToolType::Selection;



  auto comp = impl_->previewPipeline_.composition();

  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)

                           ? comp->layerById(impl_->selectedLayerId_)

                           : ArtifactAbstractLayerPtr{};

  // Playback-only physics manipulation. It intentionally precedes the
  // transform gizmo so a selected Dynamic body is never turned into an
  // authored transform drag while its simulation is running.
  if (event->button() == Qt::LeftButton &&
      activeTool == ToolType::Selection && selectedLayer &&
      selectedLayer->isVisible() && !selectedLayer->isLocked() &&
      !selectedLayer->isSelectionLocked() &&
      !event->modifiers().testFlag(Qt::AltModifier)) {
    const auto* playback = ArtifactPlaybackService::instance();
    if (playback && playback->isPlaying()) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      if (selectedLayer->beginRigidBodyMouseDrag(
              QPointF(canvas.x, canvas.y))) {
        impl_->physicsDragLayer_ = selectedLayer;
        notifyViewportInteractionActivity();
        impl_->invalidateOverlayComposite();
        markRenderDirty();
        event->accept();
        return;
      }
    }
  }

  if (event->button() == Qt::LeftButton &&
      (activeTool == ToolType::RigSelect || activeTool == ToolType::RigWeight) &&
      selectedLayer) {
    if (auto *rigLayer = dynamic_cast<ArtifactAbstract2DLayer *>(selectedLayer.get())) {
      const auto viewportCanvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      const QPointF canvasPoint(viewportCanvas.x, viewportCanvas.y);
      bool invertible = false;
      const QTransform inverse = rigLayer->getGlobalTransform().inverted(&invertible);
      if (invertible) {
        const QPointF localPoint = inverse.map(canvasPoint);
        const float threshold = 14.0f /
            std::max(0.001f, impl_->renderer_->getZoom());
        auto &rig = rigLayer->rig2D();
        if (activeTool == ToolType::RigWeight && rig.skinMesh()) {
          impl_->rigWeightBeforeVertices_ = rig.skinMesh()->vertices();
          impl_->rigWeightLayer_ = selectedLayer;
          const ArtifactCore::Id hitControlId =
              hitTestRigControl(rig, localPoint, threshold);
          const ArtifactCore::Id hitBoneId =
              hitTestRigBone(rig.rootBone(), localPoint, threshold);
          if (!hitBoneId.isNil()) {
            impl_->selectedRigBoneId_ = hitBoneId;
            if (auto *hitBone = rig.findBone(hitBoneId)) {
              const auto bones = rig.bones();
              for (int i = 0; i < bones.size(); ++i) {
                if (bones[i] == hitBone) {
                  impl_->rigWeightBoneIndex_ = i;
                  break;
                }
              }
            }
          }
          // Clicking a bone selects the paint target; painting starts only
          // when the initial press lands on the mesh rather than the rig.
          impl_->rigWeightPainting_ = hitBoneId.isNil() && hitControlId.isNil();
          impl_->rigWeightLastLocalPoint_ =
              ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPoint);
          if (impl_->rigWeightPainting_) {
            // Apply an initial dab immediately so a click without movement
            // still paints the vertex neighborhood under the brush.
            handleMouseMove(
                ArtifactCore::Coordinates::screenLogicalPointFromQPointF(
                    event->position()));
          }
          impl_->invalidateOverlayComposite();
          markRenderDirty();
          event->accept();
          return;
        }
        const ArtifactCore::Id controlId =
            hitTestRigControl(rig, localPoint, threshold);
        const ArtifactCore::Id boneId = controlId.isNil()
            ? hitTestRigBone(rig.rootBone(), localPoint, threshold)
            : ArtifactCore::Id{};
        impl_->selectedRigControlId_ = controlId;
        impl_->selectedRigBoneId_ = boneId;
        impl_->rigSelectionDragging_ = activeTool == ToolType::RigSelect &&
                                       (!controlId.isNil() || !boneId.isNil());
        if (!controlId.isNil()) {
          if (auto *control = rig.findControl(controlId)) {
            impl_->rigDragStartControlValue_ = control->value();
            impl_->rigDragStartLocalPoint_ =
                ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPoint);
            impl_->rigDragLayer_ = selectedLayer;
          }
        }
        if (!boneId.isNil()) {
          if (auto *bone = rig.findBone(boneId)) {
            impl_->rigDragStartLocalPoint_ =
                ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPoint);
            impl_->rigDragStartRotation_ = bone->localTransform().rotation;
            impl_->rigDragStartTransform_ = bone->localTransform();
            impl_->rigDragLayer_ = selectedLayer;
          }
        }
        impl_->invalidateOverlayComposite();
        markRenderDirty();
        event->accept();
        return;
      }
    }
  }

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Selection &&
      selectedLayer && impl_->showGizmoOverlay_ && selectedLayer->isVisible() &&
      !selectedLayer->isLocked() && !selectedLayer->isSelectionLocked() &&
      !event->modifiers().testFlag(Qt::AltModifier)) {
    const auto construction = ArtifactCore::dynamicPointerCast<ArtifactConstructionLayer>(selectedLayer);
    if (construction) {
      const auto view = impl_->gizmo3DCameraMatricesValid_ ? impl_->gizmo3DViewMatrix_ : impl_->renderer_->getViewMatrix();
      const auto projection = impl_->gizmo3DCameraMatricesValid_ ? impl_->gizmo3DProjectionMatrix_ : impl_->renderer_->getProjectionMatrix();
      const QRect viewport(0, 0, int(impl_->hostWidth_), int(impl_->hostHeight_));
      const auto world = construction->getGlobalTransform4x4();
      bool invertible = false;
      const auto inverse = world.inverted(&invertible);
      QPointF local;
      double nearest = 10.0 * impl_->devicePixelRatio_;
      bool found = false;
      ConstructionHandle hit;
      if (invertible && constructionLocalPoint(viewportPos, view, projection, viewport, inverse, local)) {
        for (const auto &handle : constructionHandles(*construction)) {
          const auto p = ViewportMath::projectToTopDown(world.map(QVector3D(float(handle.point.x()), float(handle.point.y()), 0)), view, projection, viewport);
          if (!std::isfinite(p.z()) || p.z() < 0 || p.z() > 1) continue;
          const double distance = QLineF(viewportPos, QPointF(p.x(), p.y())).length();
          if (distance < nearest) { nearest = distance; hit = handle; found = true; }
        }
      }
      if (found) {
        impl_->constructionDragLayer_ = construction;
        impl_->constructionDragHandle_ = hit;
        impl_->constructionDragBefore_ = construction->constructionItems()[hit.index];
        impl_->constructionDragStart_ = local;
        impl_->constructionDragInverse_ = inverse;
        impl_->constructionDragView_ = view;
        impl_->constructionDragProjection_ = projection;
        impl_->constructionDragViewport_ = viewport;
        impl_->constructionDragFrame_ = construction->currentFrame();
        notifyViewportInteractionActivity();
        impl_->invalidateOverlayComposite(); markRenderDirty();
        event->accept(); return;
      }
    }
  }

  const auto beginGizmoUndoSnapshot =
      [&]() {
        if (!selectedLayer || !impl_->gizmo3D_ ||
            impl_->gizmoUndoSnapshotValid_) {
          return;
        }
        impl_->gizmoUndoLayer_ = selectedLayer;
        impl_->gizmoUndoFrame_ = comp
            ? comp->framePosition().framePosition()
            : 0;
        impl_->gizmoUndoWorldPivotBefore_ = impl_->gizmo3D_->position();
        impl_->gizmoUndoBefore_.rotation = impl_->gizmo3D_->rotation();
        impl_->gizmoUndoBefore_.scale = impl_->gizmo3D_->scale();
        impl_->gizmoUndoBefore_.is3D = selectedLayer->is3D();
        const auto &layerTransform = selectedLayer->transform3D();
        const auto layerTime =
            gizmoTransformTime(selectedLayer, impl_->gizmoUndoFrame_);
        impl_->gizmoLayerTransformBefore_.position = {
            layerTransform.snapshotAt(layerTime).positionX,
            layerTransform.snapshotAt(layerTime).positionY,
            layerTransform.snapshotAt(layerTime).positionZ};
        impl_->gizmoLayerTransformBefore_.rotation = selectedLayer->is3D()
             ? ArtifactCore::Units::eulerDegreesFromQVector3D(
                   selectedLayer->rotation3D())
             : ArtifactCore::Units::EulerDegrees3{
                   {}, {}, {layerTransform.rotationAt(layerTime)}};
        const float layerScaleZ = selectedLayer->is3D()
            ? layerTransform.snapshotAt(layerTime).scaleZ
            : 1.0f;
         impl_->gizmoLayerTransformBefore_.scale = {
            layerTransform.scaleXAt(layerTime),
             layerTransform.scaleYAt(layerTime), layerScaleZ};
        impl_->gizmoLayerTransformBefore_.is3D = selectedLayer->is3D();
        captureGizmoKeyState(selectedLayer, impl_->gizmoUndoFrame_,
                             impl_->gizmoLayerTransformBefore_);
        const QMatrix4x4 startWorld = selectedLayer->getGlobalTransform4x4();
        const QVector3D startOrigin =
            startWorld.map(QVector3D(0.0f, 0.0f, 0.0f));
        impl_->projectedFrameStartAxisX_ =
            ArtifactCore::Coordinates::worldVectorFromQVector3D(
                startWorld.map(QVector3D(1.0f, 0.0f, 0.0f)) - startOrigin);
        impl_->projectedFrameStartAxisY_ =
            ArtifactCore::Coordinates::worldVectorFromQVector3D(
                startWorld.map(QVector3D(0.0f, 1.0f, 0.0f)) - startOrigin);
        const QVector3D anchor(
            layerTransform.anchorXAt(layerTime),
            layerTransform.anchorYAt(layerTime),
            layerTransform.anchorZAt(layerTime));
        impl_->projectedFrameStartWorldAnchor_ =
            ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                startWorld.map(anchor));
        impl_->projectedFrameParentWorld_.setToIdentity();
        impl_->projectedFrameParentWorldInverse_.setToIdentity();
        impl_->projectedFrameParentWorldInvertible_ = true;
        impl_->projectedFrameCorrectedLocalPosition_ =
            impl_->gizmoLayerTransformBefore_.position;
        impl_->projectedFrameCorrectedLocalPositionValid_ = false;
        if (const auto parent = selectedLayer->parentLayer()) {
          impl_->projectedFrameParentWorld_ = parent->getGlobalTransform4x4();
          impl_->projectedFrameParentWorldInverse_ =
              impl_->projectedFrameParentWorld_.inverted(
                  &impl_->projectedFrameParentWorldInvertible_);
        }
        impl_->gizmoGroupTransformActive_ = false;
        impl_->gizmoGroupProjectedBasisValid_ = false;
        impl_->gizmoGroupLayers_.clear();
        if (auto *selection = ArtifactLayerSelectionManager::instance()) {
          QVector<ArtifactAbstractLayerPtr> transformable;
          for (const auto &candidate : selection->selectedLayers()) {
            if (candidate && !candidate->isLocked() &&
                !candidate->isSelectionLocked()) {
              transformable.push_back(candidate);
            }
          }
          if (transformable.size() > 1) {
            const auto selectedContains = [&transformable](const LayerID &id) {
              return std::any_of(
                  transformable.cbegin(), transformable.cend(),
                  [&id](const ArtifactAbstractLayerPtr &candidate) {
                    return candidate && candidate->id() == id;
                  });
            };
            for (const auto &candidate : transformable) {
              bool rootSelection = true;
              for (auto parent = candidate->parentLayer(); parent;
                   parent = parent->parentLayer()) {
                if (selectedContains(parent->id())) {
                  rootSelection = false;
                  break;
                }
              }
              if (!rootSelection) continue;

              GizmoGroupLayerState state;
              state.layer = candidate;
              state.frame = impl_->gizmoUndoFrame_;
              state.before.is3D = candidate->is3D();
              const auto &candidateTransform = candidate->transform3D();
              const auto candidateTime =
                  gizmoTransformTime(candidate, state.frame);
              state.before.position = {
                  candidateTransform.snapshotAt(candidateTime).positionX,
                  candidateTransform.snapshotAt(candidateTime).positionY,
                  candidateTransform.snapshotAt(candidateTime).positionZ};
              state.before.rotation = candidate->is3D()
                  ? ArtifactCore::Units::eulerDegreesFromQVector3D(
                        candidate->rotation3D())
                  : ArtifactCore::Units::EulerDegrees3{
                        {}, {},
                        {candidateTransform.rotationAt(candidateTime)}};
              state.before.scale = {
                  candidateTransform.scaleXAt(candidateTime),
                  candidateTransform.scaleYAt(candidateTime),
                  candidate->is3D()
                      ? candidateTransform.snapshotAt(candidateTime).scaleZ
                      : 1.0f};
              captureGizmoKeyState(candidate, state.frame, state.before);
              const QVector3D anchor(candidateTransform.anchorXAt(candidateTime),
                                     candidateTransform.anchorYAt(candidateTime),
                                     candidateTransform.anchorZAt(candidateTime));
              state.worldAnchor =
                  ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                      candidate->getGlobalTransform4x4().map(anchor));
              if (const auto parent = candidate->parentLayer()) {
                state.parentWorldInverse =
                    parent->getGlobalTransform4x4().inverted(
                        &state.parentWorldInvertible);
              }
              impl_->gizmoGroupLayers_.push_back(std::move(state));
            }
            impl_->gizmoGroupTransformActive_ =
                !impl_->gizmoGroupLayers_.empty();
            impl_->gizmoGroupPivotBefore_ = impl_->gizmo3D_->position();
            impl_->gizmoGroupRotationBefore_ = impl_->gizmo3D_->rotation();
            impl_->gizmoGroupScaleBefore_ = impl_->gizmo3D_->scale();
          }
        }
        impl_->gizmoDragViewMatrix_ = impl_->gizmo3DViewMatrix_;
        impl_->gizmoDragProjectionMatrix_ = impl_->gizmo3DProjectionMatrix_;
        impl_->gizmoUndoSnapshotValid_ = true;
      };

  if (event->button() == Qt::LeftButton &&
      activeTool == ToolType::Clone && selectedLayer && impl_->renderer_) {
    auto *paintLayer = dynamic_cast<ArtifactPaintLayer *>(selectedLayer.get());
    if (!paintLayer) {
      setInfoOverlayText(QStringLiteral("Clone Stamp"),
                         QStringLiteral("Select a Paint layer"));
      event->accept();
      return;
    }
  const auto canvasPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()) * impl_->devicePixelRatio_,
       static_cast<float>(viewportPos.y()) * impl_->devicePixelRatio_});
    if (event->modifiers().testFlag(Qt::AltModifier) ||
        !impl_->cloneStampSourceSet_) {
      auto sourceLayer = selectedLayer;
      const LayerID hitLayerId = layerAtViewportPos(
          ArtifactCore::Coordinates::screenLogicalPointFromQPointF(
              viewportPos));
      if (comp && !hitLayerId.isNil()) {
        if (auto hitLayer = comp->layerById(hitLayerId);
            hitLayer && dynamic_cast<ArtifactPaintLayer *>(hitLayer.get())) {
          sourceLayer = hitLayer;
        }
      }
      if (!sourceLayer || !dynamic_cast<ArtifactPaintLayer *>(sourceLayer.get())) {
        setInfoOverlayText(QStringLiteral("Clone Stamp"),
                           QStringLiteral("Alt-click a Paint layer"));
        event->accept();
        return;
      }
      impl_->cloneStampSourceLayer_ = sourceLayer;
      impl_->cloneStampSourceCanvas_ = {canvasPos.x, canvasPos.y};
      impl_->cloneStampSourceSet_ = true;
      impl_->cloneStampDragging_ = false;
      setInfoOverlayText(QStringLiteral("Clone Source"),
                         QStringLiteral("Alt-click source set"));
      event->accept();
      return;
    }
    impl_->cloneStampStartCanvas_ = {canvasPos.x, canvasPos.y};
    impl_->cloneStampLastCanvas_ = impl_->cloneStampStartCanvas_;
    auto *brushTool = ArtifactApplicationManager::instance()->brushTool();
    const float radius = brushTool ? brushTool->radius() : 20.0f;
    if (auto sourceLayer = impl_->cloneStampSourceLayer_.lock()) {
      auto *sourcePaintLayer = dynamic_cast<ArtifactPaintLayer *>(sourceLayer.get());
      QPointF sourcePaintPosition;
      QPointF destinationPaintPosition;
      if (!compositionToPaintPosition(
              sourcePaintLayer,
              ArtifactCore::Coordinates::toQPointF(
                  impl_->cloneStampSourceCanvas_),
              &sourcePaintPosition) ||
          !compositionToPaintPosition(
              paintLayer,
              ArtifactCore::Coordinates::toQPointF(
                  impl_->cloneStampStartCanvas_),
              &destinationPaintPosition)) {
        setInfoOverlayText(QStringLiteral("Clone Stamp"),
                           QStringLiteral("Paint layer transform is not invertible"));
        event->accept();
        return;
      }
      const FramePosition sourceFrame(
          (sourcePaintLayer ? sourcePaintLayer->currentFrame() : 0) +
          (brushTool ? brushTool->cloneTimeOffset() : 0));
      paintLayer->applyCloneStampFromLayerAtFrame(
          sourcePaintLayer, sourcePaintPosition, destinationPaintPosition, radius,
          brushTool ? brushTool->opacity() : 1.0f,
          brushTool ? brushTool->hardness() : 1.0f, true, sourceFrame);
    } else {
      setInfoOverlayText(QStringLiteral("Clone Stamp"),
                         QStringLiteral("Alt-click a Paint layer"));
      event->accept();
      return;
    }
    impl_->cloneStampDragging_ = true;
    impl_->brushCursorCanvasPos_ = impl_->cloneStampStartCanvas_;
    impl_->brushLastViewportPos_ = screenPhysicalPoint(viewportPos);
    impl_->brushCursorVisible_ = true;
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton &&
      (activeTool == ToolType::Brush || activeTool == ToolType::Fill ||
       activeTool == ToolType::Eraser) &&
      (!selectedLayer ||
       dynamic_cast<ArtifactPaintLayer *>(selectedLayer.get()) == nullptr)) {
    if (auto *service = ArtifactProjectService::instance()) {
      const QString layerName =
          uniqueLayerNameForCurrentComposition(QStringLiteral("Paint"));
      ArtifactLayerInitParams params(layerName, LayerType::Paint);
      service->addLayerToCurrentComposition(params, true);
      selectedLayer = comp ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
      if (selectedLayer) {
        setInfoOverlayText(
            activeTool == ToolType::Eraser ? QStringLiteral("Eraser")
            : activeTool == ToolType::Fill ? QStringLiteral("Fill")
                                           : QStringLiteral("Brush"),
            QStringLiteral("Paint layer created for this tool"));
      }
    }
  }

  if (event->button() == Qt::LeftButton &&
      activeTool == ToolType::Fill && selectedLayer && impl_->renderer_) {
    const auto canvasPos = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()),
         static_cast<float>(viewportPos.y())});
    auto* paintLayer = dynamic_cast<ArtifactPaintLayer*>(selectedLayer.get());
    auto* brushTool = ArtifactApplicationManager::instance()->brushTool();
    if (paintLayer && brushTool && paintLayer->floodFillAtFrame(
            QPointF(canvasPos.x, canvasPos.y), brushTool->color(),
            brushTool->opacity(), 0.08f,
            FramePosition(paintLayer->currentFrame()))) {
      impl_->publishLayerModified(selectedLayer);
      impl_->invalidateBaseComposite();
      impl_->invalidateOverlayComposite();
      markRenderDirty();
    }
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton &&
      (activeTool == ToolType::Brush || activeTool == ToolType::RotoBrush ||
       activeTool == ToolType::Eraser) &&
      selectedLayer) {
    auto *brushTool = ArtifactApplicationManager::instance()->brushTool();
    if (activeTool == ToolType::Eraser &&
        (event->modifiers().testFlag(Qt::AltModifier) ||
         (brushTool && (brushTool->lastStrokeOnly() ||
                        brushTool->eraserModeKind() == 2)))) {
      if (auto *paintLayer = dynamic_cast<ArtifactPaintLayer *>(
              selectedLayer.get())) {
        if (paintLayer->canUndo()) {
          paintLayer->undoLastStroke();
          paintLayer->changed();
          impl_->publishLayerModified(selectedLayer);
          impl_->invalidateBaseComposite();
          impl_->invalidateOverlayComposite();
          markRenderDirty();
        }
      }
      event->accept();
      return;
    }
    if (activeTool == ToolType::Eraser && brushTool &&
        brushTool->eraserModeKind() == 1) {
      if (auto *paintLayer = dynamic_cast<ArtifactPaintLayer *>(
              selectedLayer.get())) {
        paintLayer->clearAllFrames();
        impl_->publishLayerModified(selectedLayer);
        setInfoOverlayText(QStringLiteral("Layer Eraser"),
                           QStringLiteral("All frames cleared • Undo to restore"));
        impl_->invalidateBaseComposite();
        impl_->invalidateOverlayComposite();
        markRenderDirty();
      }
      event->accept();
      return;
    }
      const auto canvasPos = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()),
         static_cast<float>(viewportPos.y())});
    if (activeTool == ToolType::RotoBrush) {
      impl_->rotoBrushStrokeType_ =
          event->modifiers().testFlag(Qt::ShiftModifier)
              ? ArtifactCore::RotoBrushStrokeType::Background
              : ArtifactCore::RotoBrushStrokeType::Foreground;
    }
    if (auto *brushTool = ArtifactApplicationManager::instance()->brushTool()) {
      brushTool->setRotoInputMode(activeTool == ToolType::RotoBrush);
      brushTool->setEraserMode(activeTool == ToolType::Eraser);
      brushTool->mousePressEvent(selectedLayer,
                                 {canvasPos.x, canvasPos.y});
    }
    impl_->brushCursorCanvasPos_ = {canvasPos.x, canvasPos.y};
    impl_->brushLastViewportPos_ = screenPhysicalPoint(viewportPos);
    impl_->brushCursorVisible_ = true;
    markRenderDirty();
    return;
  }

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Text &&
      comp && ArtifactProjectService::instance()) {
    // A press that lands on the selected text layer's gizmo is a transform
    // gesture, not the seed of a new text layer.  Hand it to the 2D TextGizmo
    // so the handles stay interactive while the Text tool is active; only a
    // press off the gizmo starts the new-text candidate below.
    if (impl_->textGizmo_ && selectedLayer &&
        impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Text &&
        !selectedLayer->isLocked() && !selectedLayer->isSelectionLocked()) {
      impl_->textGizmo_->setLayer(selectedLayer);
      const bool textHandleHit = impl_->textGizmo_->hitTest(
          viewportPos, impl_->renderer_.get()) != TextGizmo::HandleType::None;
      impl_->textGizmo_->handleMousePress(viewportPos,
                                          impl_->renderer_.get());
      if (impl_->textGizmo_->isDragging()) {
        impl_->beginGizmoSession(GizmoOwner::Text, selectedLayer);
        notifyViewportInteractionActivity();
        impl_->invalidateOverlayComposite();
        markRenderDirty();
        impl_->gizmoDragRenderTimer_.restart();
        return;
      }
      // An expression-owned handle is disabled, not empty canvas on which
      // the Text tool should create a new layer.
      if (textHandleHit) {
        event->accept();
        return;
      }
    }
    const auto canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    impl_->textToolCandidate_ = true;
    impl_->textToolDragging_ = false;
    impl_->textToolStartCanvas_ = {canvasPos.x, canvasPos.y};
    impl_->textToolCurrentCanvas_ = impl_->textToolStartCanvas_;
    event->accept();
    return;
  }

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





  // MotionSketch tool

  if (event->button() == Qt::LeftButton && activeTool == ToolType::MotionSketch && selectedLayer && impl_->renderer_) {

    const auto cPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    auto* app = ArtifactApplicationManager::instance();

    if (app && app->motionSketchTool()) {

      app->motionSketchTool()->beginSketch(QPointF(cPos.x, cPos.y), selectedLayer);

    }
    impl_->motionSketchLastCanvasPos_ = {cPos.x, cPos.y};

    if (auto *playback = ArtifactPlaybackService::instance()) {
      impl_->motionSketchWasPlaying_ = playback->isPlaying();
      if (!impl_->motionSketchWasPlaying_) {
        playback->play();
      }
    }

    markRenderDirty();

    event->accept();

    return;

  }



  // Puppet tool

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Puppet && impl_->renderer_) {

    const auto cPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    auto* app = ArtifactApplicationManager::instance();

    if (app && app->puppetTool()) {

      const QPointF canvasPt(cPos.x, cPos.y);

      QString hitId = app->puppetTool()->hitTestPin(canvasPt);

      if (!hitId.isEmpty()) {

        app->puppetTool()->setSelectedPinId(hitId);

        const LayerID controlLayerId = app->puppetTool()->pinLayerId(hitId);
        const auto controlLayer = comp
            ? comp->layerById(controlLayerId) : ArtifactAbstractLayerPtr{};
        // Same image-layer-only limitation as the add-pin path below: the
        // move/rotation drag ends in Deformation2DStateUndoCommand, which
        // cannot restore a text layer's pin state.
        if (layerUsesTextGizmo(controlLayer)) {
          app->puppetTool()->setSelectedPinId(QString());
          markRenderDirty();
          event->accept();
          return;
        }
        app->puppetTool()->evaluatePinPositionsAtCurrentFrame(controlLayerId);
        const QPointF pinPos = app->puppetTool()->pinPosition(hitId);
        impl_->puppetLayerUndoSnapshot_ = controlLayer
            ? controlLayer->deformation2DData() : QJsonObject{};
        impl_->puppetLayerUndoSnapshotValid_ = static_cast<bool>(controlLayer);
        impl_->puppetPinDragging_ = true;
        impl_->puppetPinDragStartCanvas_ =
            ArtifactCore::Coordinates::compositionPointFromQPointF(canvasPt);
        impl_->puppetPinDragStartPosition_ = pinPos;
        impl_->puppetPinUndoId_ = hitId;
        impl_->puppetPinUndoBeforePosition_ = pinPos;
        impl_->puppetPinUndoBeforeRotation_ =
            app->puppetTool()->pinRotation(hitId);
        impl_->puppetPinAnimationBefore_ =
            app->puppetTool()->pinPositionAnimationSnapshot(controlLayerId,
                                                            hitId);

        if (event->modifiers().testFlag(Qt::AltModifier)) {
          impl_->puppetRotationDragging_ = true;
          impl_->puppetRotationPinId_ = hitId;
          impl_->puppetRotationStartAngle_ =
              static_cast<float>(std::atan2(canvasPt.y() - pinPos.y(),
                                             canvasPt.x() - pinPos.x()) *
                                 180.0 / 3.14159265358979323846) -
              app->puppetTool()->pinRotation(hitId);
        }

      } else if (selectedLayer) {
        // The 2D deformer drives the deformation through
        // ArtifactPuppetTool::renderDeformedLayer and
        // Deformation2DStateUndoCommand::apply, both of which require an
        // ArtifactImageLayer.  Accepting a pin on a text layer would let the
        // overlay draw while both the deform pass and the undo restore
        // silently fail, so reject the edit instead of half-applying it.
        if (!layerUsesTextGizmo(selectedLayer)) {
        const QJsonObject before = selectedLayer->deformation2DData();
        if (app->puppetTool()->addPin(selectedLayer->id(), canvasPt)) {
          const auto modifiers = event->modifiers();
          const int pinType = modifiers.testFlag(Qt::ControlModifier)
                                  ? 1  // Starch takes precedence.
                                  : modifiers.testFlag(Qt::ShiftModifier)
                                        ? 2  // Bend.
                                        : modifiers.testFlag(Qt::AltModifier)
                                              ? 3  // Overlap.
                                              : 0; // Position.
          app->puppetTool()->setPinTypeFor(
              app->puppetTool()->selectedPinId(), pinType);
          const QJsonObject after = selectedLayer->deformation2DData();
          if (auto* undo = UndoManager::instance()) {
            if (!undo->push(std::make_unique<Deformation2DStateUndoCommand>(
                    selectedLayer, before, after,
                    QStringLiteral("Add Deformer Pin"), app->puppetTool()))) {
              app->puppetTool()->restoreLayerData(selectedLayer->id(), before);
            }
          }
        }

        if (app && app->puppetTool()) {

          app->puppetTool()->deformLayer(selectedLayer->id(), impl_->renderer_.get());

        }
        }

      }

    }

    markRenderDirty();

    event->accept();

    return;

  }

  if (event->button() == Qt::LeftButton && activeTool == ToolType::Selection &&
      event->modifiers().testFlag(Qt::AltModifier) && comp && impl_->renderer_) {
    QSet<LayerID> selectedLayerIds;
    if (auto *selection = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->layerSelectionManager()
                              : nullptr) {
      for (const auto &layer : selection->selectedLayers()) {
        if (layer) {
          selectedLayerIds.insert(layer->id());
        }
      }
    }
    if (selectedLayerIds.isEmpty() && !impl_->selectedLayerId_.isNil()) {
      selectedLayerIds.insert(impl_->selectedLayerId_);
    }

    if (!selectedLayerIds.isEmpty()) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      const float hitThreshold =
          14.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom();
      QString fieldId;
      TransformFieldDragMode dragMode = TransformFieldDragMode::None;
      if (hitTestTransformFieldHandle(comp, QPointF(cPos.x, cPos.y),
                                      selectedLayerIds, hitThreshold, fieldId,
                                      dragMode) &&
          impl_->beginTransformFieldDrag(comp, fieldId, dragMode)) {
        notifyViewportInteractionActivity();
        impl_->applyTransformFieldDrag(comp, QPointF(cPos.x, cPos.y));
        markRenderDirty();
        event->accept();
        return;
      }
    }
  }

  if (event->button() == Qt::LeftButton &&
      (activeTool == ToolType::Rectangle || activeTool == ToolType::Ellipse ||
       activeTool == ToolType::Shape)) {

    auto *selectionManager = ArtifactApplicationManager::instance()

                                 ? ArtifactApplicationManager::instance()

                                       ->layerSelectionManager()

                                 : nullptr;

    ArtifactAbstractLayerPtr effectiveSelectedLayer =

        selectionManager ? selectionManager->currentLayer()

                         : ArtifactAbstractLayerPtr{};

    if (!effectiveSelectedLayer && !selectionManager && selectedLayer) {

      effectiveSelectedLayer = selectedLayer;

    }



    const auto canvasPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    impl_->clearPendingMaskCreation();

    if (effectiveSelectedLayer) {

      setViewportOrientation(ArtifactCore::ViewOrientationHotspot::Front);

      RectangleToolMode maskMode = RectangleToolMode::Mask;
      if (activeTool == ToolType::Ellipse) {
        maskMode = RectangleToolMode::EllipseMask;
      }
      impl_->beginRectangleToolSession(
          maskMode, effectiveSelectedLayer,
          ArtifactCore::Coordinates::CompositionPoint2{canvasPos.x,
                                                        canvasPos.y});

      impl_->beginMaskEditTransaction(effectiveSelectedLayer);

    } else {

      ShapeType createShapeType = ShapeType::Rect;
      if (activeTool == ToolType::Ellipse) {
        createShapeType = ShapeType::Ellipse;
      } else if (activeTool == ToolType::Shape) {
        const int rawShapeType = QSettings().value(
            QStringLiteral("shape/createType"),
            static_cast<int>(ShapeType::Rect)).toInt();
        if (rawShapeType >= static_cast<int>(ShapeType::Rect) &&
            rawShapeType <= static_cast<int>(ShapeType::Cross)) {
          createShapeType = static_cast<ShapeType>(rawShapeType);
        }
      }

      RectangleToolMode shapeMode = RectangleToolMode::Shape;
      switch (createShapeType) {
      case ShapeType::Ellipse:
        shapeMode = RectangleToolMode::EllipseShape;
        break;
      case ShapeType::Star:
        shapeMode = RectangleToolMode::StarShape;
        break;
      case ShapeType::Polygon:
        shapeMode = RectangleToolMode::PolygonShape;
        break;
      case ShapeType::Triangle:
        shapeMode = RectangleToolMode::TriangleShape;
        break;
      case ShapeType::Rect:
      case ShapeType::Square:
      case ShapeType::Line:
      case ShapeType::Arrow:
      case ShapeType::Heart:
      case ShapeType::Diamond:
      case ShapeType::Gear:
      case ShapeType::Cross:
        break;
      }
      impl_->rectangleToolShapeType_ = createShapeType;
      impl_->beginRectangleToolSession(
          shapeMode, ArtifactAbstractLayerPtr{},
          ArtifactCore::Coordinates::CompositionPoint2{canvasPos.x,
                                                        canvasPos.y});

    }

    impl_->rectangleToolFromCenter_ =
        event->modifiers().testFlag(Qt::AltModifier);

    markRenderDirty();

    event->accept();

    return;

  }



  if (event->button() == Qt::LeftButton && activeTool == ToolType::Pen) {
    if (!selectedLayer || !comp || !impl_->renderer_) {
      setInfoOverlayText(
          QStringLiteral("Mask editing"),
          QStringLiteral(
              "Select a layer first, then click in the viewport to add mask "
              "vertices."));
      markRenderDirty();
      event->accept();
      return;
    }

    // Shape layer + Pen tool creates a custom shape path instead of a mask.
    if (dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())) {
      const auto cShapePos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      const QTransform shapeGlobalTransform =
          selectedLayer->getGlobalTransform();
      bool shapeInvertible = false;
      const QTransform shapeInv =
          shapeGlobalTransform.inverted(&shapeInvertible);
      if (!shapeInvertible) {
        event->accept();
        return;
      }
      const QPointF localShapePos =
          shapeInv.map(QPointF(cShapePos.x, cShapePos.y));
      impl_->penMaskPreviewCanvasPos_ = {cShapePos.x, cShapePos.y};
      impl_->penMaskPreviewValid_ = true;

      // Clicking near the first vertex closes and finalizes the path.
      if (impl_->pendingShapePathCreation_ &&
          impl_->pendingShapePathLayerId_ == selectedLayer->id() &&
          impl_->pendingShapePathVertices_.size() >= 2) {
        const float closeThreshold =
            12.0f / impl_->renderer_->getZoom();
        const QPointF &first =
            impl_->pendingShapePathVertices_.front().pos;
        const float dx =
            static_cast<float>(first.x() - localShapePos.x());
        const float dy =
            static_cast<float>(first.y() - localShapePos.y());
        if (dx * dx + dy * dy <= closeThreshold * closeThreshold) {
          if (impl_->finalizePendingShapePathCreation(selectedLayer)) {
            setInfoOverlayText(
                QStringLiteral("Shape Path"),
                QStringLiteral("Custom path created"));
          }
          event->accept();
          return;
        }
      }

      impl_->beginPendingShapePathCreation(selectedLayer,
                                           localShapePos);
      setInfoOverlayText(
          QStringLiteral("Shape Path"),
          QStringLiteral(
              "%1 vertices - click first point or press Enter to close")
              .arg(impl_->pendingShapePathVertices_.size()));
      markRenderDirty();
      event->accept();
      return;
    }

    if (event->modifiers().testFlag(Qt::ControlModifier) &&
        resetHoveredMaskTangent()) {
      event->accept();
      return;
    }

    const auto cPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    const QTransform globalTransform = selectedLayer->getGlobalTransform();

    bool invertible = false;

    const QTransform invTransform = globalTransform.inverted(&invertible);



    if (invertible) {

      impl_->beginMaskEditTransaction(selectedLayer);

      const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));

      impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};

      impl_->penMaskPreviewValid_ = true;



      if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
        impl_->selectedMaskVertices_.clear();
        impl_->hoveredVertexIndex_ = -1;
      }

      if (impl_->pendingMaskCreation_ &&

          impl_->pendingMaskLayerId_ != selectedLayer->id()) {

        impl_->clearPendingMaskCreation();

      }

      if (impl_->pendingShapePathCreation_ &&

          impl_->pendingShapePathLayerId_ != selectedLayer->id()) {

        impl_->clearPendingShapePathCreation();

      }



      const float handleThreshold =
          14.0f * Accessibility::targetScale();

      int handleMaskIndex = -1;

      int handlePathIndex = -1;

      int handleVertexIndex = -1;

      MaskEditHandleType handleType = MaskEditHandleType::None;

      if (hitTestMaskHandle(selectedLayer, impl_->renderer_.get(), viewportPos, handleThreshold,

                            handleMaskIndex, handlePathIndex, handleVertexIndex,

                            handleType)) {

        if (handleMaskIndex >= 0 &&
            selectedLayer->mask(handleMaskIndex).isLocked()) {
          event->accept();
          return;
        }

        impl_->resetLayerMutationNotify();

        impl_->isDraggingVertex_ = false;

        impl_->isDraggingMaskHandle_ = true;

        impl_->draggingMaskIndex_ = handleMaskIndex;

        impl_->draggingPathIndex_ = handlePathIndex;

        impl_->draggingVertexIndex_ = handleVertexIndex;

        impl_->draggingMaskHandleType_ = static_cast<int>(handleType);

        impl_->draggingMaskHandleStartLocal_ =
            ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPos);

        impl_->draggingMaskHandleStartFeather_ =
            handleType == MaskEditHandleType::FeatherHandle
                ? selectedLayer->mask(handleMaskIndex)
                      .maskPath(handlePathIndex)
                      .feather()
                : ArtifactCore::Units::LayerLocalLength{};

        impl_->hoveredMaskIndex_ = handleMaskIndex;

        impl_->hoveredPathIndex_ = handlePathIndex;

        impl_->hoveredVertexIndex_ = handleVertexIndex;

        impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);

        event->accept();

        return;

      }



      // 1. Hit test existing vertices for dragging or closing path

      const float hitThreshold =

          12.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom(); // widened for direct mask edits

      for (int m = 0; m < selectedLayer->maskCount(); ++m) {

        LayerMask mask = selectedLayer->mask(m);

        for (int p = 0; p < mask.maskPathCount(); ++p) {

          MaskPath path = mask.maskPath(p);

          for (int v = 0; v < path.vertexCount(); ++v) {

            MaskVertex vertex = path.vertex(v);

            if (QVector2D(vertex.position - localPos).length() <

                hitThreshold) {

              if (v == 0 && !path.isClosed() && path.vertexCount() > 2) {

                path.setClosed(true);

                mask.setMaskPath(p, path);

                mask.addMaskPath(MaskPath());

                selectedLayer->setMask(m, mask);

                impl_->markMaskEditDirty();

                qDebug() << "[PenTool] Closed path" << p;

                const bool committed = impl_->commitMaskEditTransaction();
                if (committed) {
                  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
                      comp->id().toString(), selectedLayer->id().toString(),
                      LayerChangedEvent::ChangeType::Modified});
                }

                impl_->isDraggingVertex_ = false;

                impl_->draggingMaskIndex_ = m;

                impl_->draggingPathIndex_ = p;

                impl_->draggingVertexIndex_ = -1;

                impl_->hoveredMaskIndex_ = m;

                impl_->hoveredPathIndex_ = p;

                impl_->hoveredVertexIndex_ = -1;

                event->accept();

                return;

              }



              if (event->modifiers().testFlag(Qt::ControlModifier)) {

                impl_->isDraggingVertex_ = false;

                impl_->resetLayerMutationNotify();

                impl_->isDraggingMaskHandle_ = true;

                impl_->draggingMaskIndex_ = m;

                impl_->draggingPathIndex_ = p;

                impl_->draggingVertexIndex_ = v;

                impl_->draggingMaskHandleType_ =

                    static_cast<int>(MaskEditHandleType::OutTangent);

                impl_->hoveredMaskIndex_ = m;

                impl_->hoveredPathIndex_ = p;

                impl_->hoveredVertexIndex_ = v;

                impl_->hoveredMaskHandleType_ =

                    static_cast<int>(MaskEditHandleType::OutTangent);

                if (resetHoveredMaskVertexTangents()) {
                  event->accept();
                  return;
                }

                qDebug() << "[PenTool] Started dragging bezier handle" << v;

                event->accept();

                return;

              }



              impl_->isDraggingVertex_ = true;

              impl_->dragStartVertexLocal_ =
                  ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(
                      vertex.position);

              const auto selectedVertex = ArtifactCore::artifactMakeTuple(m, p, v);
              if (!event->modifiers().testFlag(Qt::ShiftModifier)) {
                impl_->selectedMaskVertices_.clear();
              }
              const auto selectedIt = std::find(
                  impl_->selectedMaskVertices_.begin(),
                  impl_->selectedMaskVertices_.end(), selectedVertex);
              if (event->modifiers().testFlag(Qt::ShiftModifier) &&
                  selectedIt != impl_->selectedMaskVertices_.end()) {
                impl_->selectedMaskVertices_.erase(selectedIt);
                impl_->isDraggingVertex_ = false;
                impl_->draggingVertexIndex_ = -1;
                impl_->commitMaskEditTransaction();
                event->accept();
                return;
              } else if (selectedIt == impl_->selectedMaskVertices_.end()) {
                impl_->selectedMaskVertices_.push_back(selectedVertex);
              }

              impl_->resetLayerMutationNotify();

              impl_->draggingMaskIndex_ = m;

              impl_->draggingPathIndex_ = p;

              impl_->draggingVertexIndex_ = v;

              qDebug() << "[PenTool] Started dragging vertex" << v;

              event->accept();

              return;

            }

          }

        }

      }



      if (impl_->pendingMaskCreation_ &&

          impl_->pendingMaskLayerId_ == selectedLayer->id() &&

          impl_->pendingMaskPath_.vertexCount() >= 3) {

        const float closeThreshold = 12.0f / impl_->renderer_->getZoom();

        const MaskVertex firstVertex = impl_->pendingMaskPath_.vertex(0);

        if (QVector2D(firstVertex.position - localPos).length() <

            closeThreshold) {

          if (impl_->finalizePendingMaskCreation(selectedLayer)) {

            qDebug() << "[PenTool] Finalized pending mask path"

                     << "layer:" << selectedLayer->id().toString();

            ArtifactCore::globalEventBus().publish(LayerChangedEvent{

                comp->id().toString(), selectedLayer->id().toString(),

                LayerChangedEvent::ChangeType::Modified});

          }

          event->accept();

          return;

        }

      }



      int segmentMaskIndex = -1;

      int segmentPathIndex = -1;

      int segmentIndex = -1;

      const float segmentThreshold = 12.0f / impl_->renderer_->getZoom();

      if (hitTestMaskSegment(selectedLayer, QPointF(cPos.x, cPos.y),

                             segmentThreshold, segmentMaskIndex,

                             segmentPathIndex, segmentIndex)) {

        if (segmentMaskIndex >= 0 &&
            selectedLayer->mask(segmentMaskIndex).isLocked()) {
          event->accept();
          return;
        }

        // Modifier-dragging a path edits its geometry parameters instead of
        // inserting a vertex. Shift controls feather; Alt controls expansion.
        // Handle hits are resolved earlier, so Alt remains available for
        // breaking tangents when the pointer is over a handle.
        if (event->modifiers().testFlag(Qt::ShiftModifier) ||
            event->modifiers().testFlag(Qt::AltModifier)) {
          const LayerMask geometryMask = selectedLayer->mask(segmentMaskIndex);
          const MaskPath geometryPath =
              geometryMask.maskPath(segmentPathIndex);
          impl_->beginMaskEditTransaction(selectedLayer);
          impl_->isDraggingMaskGeometry_ = true;
          impl_->draggingMaskIndex_ = segmentMaskIndex;
          impl_->draggingPathIndex_ = segmentPathIndex;
          impl_->draggingMaskGeometryStartLocal_ =
              ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPos);
          impl_->draggingMaskGeometryStartFeather_ = geometryPath.feather();
          impl_->draggingMaskGeometryStartExpansion_ =
              geometryPath.expansion();
          impl_->draggingMaskGeometryExpansion_ =
              event->modifiers().testFlag(Qt::AltModifier);
          event->accept();
          return;
        }

        if (insertVertexOnMaskSegment(selectedLayer, segmentMaskIndex,

                                      segmentPathIndex, segmentIndex,

                                      localPos)) {

          const int insertedVertexIndex = segmentIndex + 1;

          impl_->markMaskEditDirty();

          impl_->isDraggingVertex_ = true;

          impl_->dragStartVertexLocal_ =
              ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPos);

          impl_->selectedMaskVertices_.clear();
          impl_->selectedMaskVertices_.emplace_back(
              segmentMaskIndex, segmentPathIndex, insertedVertexIndex);

          impl_->resetLayerMutationNotify();

          impl_->draggingMaskIndex_ = segmentMaskIndex;

          impl_->draggingPathIndex_ = segmentPathIndex;

          impl_->draggingVertexIndex_ = insertedVertexIndex;

          impl_->hoveredMaskIndex_ = segmentMaskIndex;

          impl_->hoveredPathIndex_ = segmentPathIndex;

          impl_->hoveredVertexIndex_ = insertedVertexIndex;

          qDebug() << "[PenTool] Inserted mask vertex on segment"

                   << "mask:" << segmentMaskIndex

                   << "path:" << segmentPathIndex

                   << "vertex:" << insertedVertexIndex;

        }

        event->accept();

        return;

      }



      impl_->selectedMaskVertices_.clear();
      impl_->maskSnapPreviewValid_ = false;
      QPointF maskCreationCanvasPos(cPos.x, cPos.y);
      if (event->modifiers().testFlag(Qt::ControlModifier) &&
          !event->modifiers().testFlag(Qt::AltModifier) && comp) {
        const QSize compositionSize = comp->settings().compositionSize();
        const float snapDistance = 10.0f /
                                    std::max(0.001f,
                                             impl_->renderer_->getZoom());
        const auto snap = [snapDistance](qreal value,
                                         qreal target) {
          return std::abs(value - target) <= snapDistance ? target : value;
        };
        maskCreationCanvasPos.setX(
            snap(maskCreationCanvasPos.x(), 0.0));
        maskCreationCanvasPos.setX(
            snap(maskCreationCanvasPos.x(), compositionSize.width()));
        maskCreationCanvasPos.setY(
            snap(maskCreationCanvasPos.y(), 0.0));
        maskCreationCanvasPos.setY(
            snap(maskCreationCanvasPos.y(), compositionSize.height()));
        QPointF nearestVertex;
        float nearestDistance = snapDistance;
        for (const auto &candidateLayer : comp->allLayer()) {
          if (!candidateLayer || !candidateLayer->isVisible()) {
            continue;
          }
          const QTransform candidateTransform =
              candidateLayer->getGlobalTransform();
          for (int maskIndex = 0; maskIndex < candidateLayer->maskCount();
               ++maskIndex) {
            const LayerMask candidateMask = candidateLayer->mask(maskIndex);
            for (int pathIndex = 0;
                 pathIndex < candidateMask.maskPathCount(); ++pathIndex) {
              const MaskPath candidatePath = candidateMask.maskPath(pathIndex);
              for (int vertexIndex = 0;
                   vertexIndex < candidatePath.vertexCount(); ++vertexIndex) {
                const QPointF candidateCanvas = candidateTransform.map(
                    candidatePath.vertex(vertexIndex).position);
                const float distance = static_cast<float>(
                    QLineF(maskCreationCanvasPos, candidateCanvas).length());
                if (distance < nearestDistance) {
                  nearestDistance = distance;
                  nearestVertex = candidateCanvas;
                }
              }
            }
          }
        }
        if (nearestDistance < snapDistance) {
          maskCreationCanvasPos = nearestVertex;
        }
        if (maskCreationCanvasPos != QPointF(cPos.x, cPos.y)) {
          impl_->maskSnapPreviewValid_ = true;
          impl_->maskSnapPreviewCanvasPos_ =
              ArtifactCore::Coordinates::compositionPointFromQPointF(
                  maskCreationCanvasPos);
        }
      }
      const QPointF maskCreationLocalPos =
          invTransform.map(maskCreationCanvasPos);
      QPointF maskCreationPos = maskCreationLocalPos;
      if (event->modifiers().testFlag(Qt::ShiftModifier) &&
          impl_->pendingMaskCreation_ &&
          impl_->pendingMaskLayerId_ == selectedLayer->id() &&
          impl_->pendingMaskPath_.vertexCount() > 0) {
        const QPointF previous =
            impl_->pendingMaskPath_
                .vertex(impl_->pendingMaskPath_.vertexCount() - 1)
                .position;
        const QPointF delta = maskCreationLocalPos - previous;
        const double distance = std::hypot(delta.x(), delta.y());
        if (distance > 0.0001) {
          constexpr double kPi = 3.14159265358979323846;
          const double angle = std::atan2(delta.y(), delta.x());
          const double snapped =
              std::round(angle / (kPi / 4.0)) * (kPi / 4.0);
          maskCreationPos = previous + QPointF(
              std::cos(snapped) * distance, std::sin(snapped) * distance);
        }
      }
      impl_->beginPendingMaskCreation(selectedLayer, maskCreationPos);

      impl_->maskRubberBandCandidate_ = true;
      impl_->isMaskRubberBandSelecting_ = false;
      impl_->maskRubberBandStartCanvas_ = {cPos.x, cPos.y};
      impl_->maskRubberBandCurrentCanvas_ =
          impl_->maskRubberBandStartCanvas_;

      impl_->isDraggingVertex_ = false;

      impl_->isDraggingMaskHandle_ = true;

      impl_->draggingMaskIndex_ = -1;

      impl_->draggingPathIndex_ = -1;

      impl_->draggingVertexIndex_ =

          impl_->pendingMaskPath_.vertexCount() - 1;

      impl_->draggingMaskHandleType_ =

          static_cast<int>(MaskEditHandleType::OutTangent);



      qDebug() << "[PenTool] Added pending mask vertex at local:" << localPos

               << "layer:" << selectedLayer->id().toString()

               << "pendingVertices:" << impl_->pendingMaskPath_.vertexCount();

      event->accept();

      return;

    }

  }



  // Straight lines expose endpoint handles. Converted Line paths use the
  // normal Bézier vertex/tangent interaction below instead.
  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen && selectedLayer && impl_->renderer_) {
    if (auto *line = dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get()); line && line->shapeType() == ShapeType::Line && !line->hasCustomPath()) {
      const QTransform global = line->getGlobalTransform();
      const QPointF a = global.map(QPointF(0.0, line->shapeHeight() * 0.5));
      const QPointF b = global.map(QPointF(line->shapeWidth(), line->shapeHeight() * 0.5));
  const Detail::float2 canvas = impl_->renderer_->viewportToCanvas({static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      const float threshold = 14.0f / std::max(0.001f, impl_->renderer_->getZoom());
      const float da = static_cast<float>(QLineF(QPointF(canvas.x, canvas.y), a).length());
      const float db = static_cast<float>(QLineF(QPointF(canvas.x, canvas.y), b).length());
      if (std::min(da, db) <= threshold) {
        impl_->isDraggingLineEndpoint_ = true; impl_->draggingLineEndpoint_ = da <= db ? 0 : 1;
        impl_->draggingLineLayer_ = selectedLayer; impl_->draggingLineBeforeWidth_ = line->shapeWidth(); impl_->draggingLineBeforeHeight_ = line->shapeHeight();
        const auto time = gizmoTransformTime(selectedLayer, selectedLayer->currentFrame()); const auto &t = line->transform3D();
        impl_->draggingLineBeforePosition_ = QPointF(t.positionXAt(time), t.positionYAt(time)); impl_->draggingLineBeforeRotation_ = t.rotationAt(time);
        handleMouseMove(
            ArtifactCore::Coordinates::screenLogicalPointFromQPointF(
                event->position())); event->accept(); return;
      }
    }
  }

  // Main-VP custom path vertex editing (Selection tool on shape layers).
  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen &&
      activeTool != ToolType::Rectangle && impl_->renderer_) {
    // F2: parameter handles first (Rect/Square/Star), then F5 operator
    // handles (offset markers keep vertex handles grabbable), then polygon
    // vertices/insert, then custom path vertices. Each owns its press.
    const auto physicalPoint = screenPhysicalPoint(viewportPos);
    if (beginShapeParamDrag(physicalPoint)) {
      notifyViewportInteractionActivity();
      event->accept();
      return;
    }
    if (beginShapeOperatorDrag(physicalPoint)) {
      notifyViewportInteractionActivity();
      event->accept();
      return;
    }
    if (beginShapePolygonDrag(physicalPoint)) {
      notifyViewportInteractionActivity();
      event->accept();
      return;
    }
    if (beginShapePathVertexDrag(physicalPoint)) {
      notifyViewportInteractionActivity();
      event->accept();
      return;
    }
    // F4: a press on the selected shape body without a vertex/param handle
    // clears the vertex selection. The event keeps flowing so gizmo and
    // transform paths still own the press.
    if (!impl_->selectedShapePathVertices_.empty() && selectedLayer &&
        impl_->renderer_) {
      if (dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())) {
        const auto missCanvas = impl_->renderer_->viewportToCanvas(
            {static_cast<float>(viewportPos.x()),
             static_cast<float>(viewportPos.y())});
        QRectF missBounds = selectedLayer->transformedBoundingBox();
        const float pad =
            4.0f / std::max(0.001f, impl_->renderer_->getZoom());
        missBounds.adjust(-pad, -pad, pad, pad);
        if (missBounds.contains(QPointF(missCanvas.x, missCanvas.y))) {
          clearShapePathSelection();
        }
      }
    }
  }

  // F12: a modifier drag over the editable shape, or a plain drag from
  // empty canvas, selects custom-path/polygon vertices without stealing the
  // ordinary layer move gesture from a click inside the selected layer.
  if (event->button() == Qt::LeftButton && activeTool == ToolType::Selection &&
      selectedLayer && comp && impl_->renderer_ &&
      !selectedLayer->isLocked() && !selectedLayer->isSelectionLocked()) {
    const auto *selectedShape =
        dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get());
    if (selectedShape &&
        (selectedShape->hasCustomPath() || selectedShape->hasCustomPolygon())) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()),
           static_cast<float>(viewportPos.y())});
      const QPointF canvasPoint(canvas.x, canvas.y);
      const auto frame = currentFrameForComposition(comp);
      const bool ignoreLocked =
          event->modifiers().testFlag(Qt::AltModifier);
      bool hitAnyLayer = false;
      bool hitSelectedLayer = false;
      for (const auto &candidate : comp->allLayerRef()) {
        if (!isLayerEffectivelyVisible(candidate) ||
            !candidate->isActiveAt(frame) ||
            ((candidate->isLocked() || candidate->isSelectionLocked()) &&
             !ignoreLocked)) {
          continue;
        }
        bool hit = false;
        bool invertible = false;
        const QTransform inverse = candidate->getGlobalTransform().inverted(
            &invertible);
        if (invertible) {
          hit = candidate->localBounds().contains(inverse.map(canvasPoint));
        } else {
          hit = candidate->transformedBoundingBox().contains(canvasPoint);
        }
        if (hit) {
          hitAnyLayer = true;
          hitSelectedLayer |= candidate->id() == selectedLayer->id();
        }
      }
      const bool hasSelectionModifier =
          event->modifiers().testFlag(Qt::ShiftModifier) ||
          event->modifiers().testFlag(Qt::ControlModifier);
      if ((!hasSelectionModifier && !hitAnyLayer) ||
          (hasSelectionModifier && (hitSelectedLayer || !hitAnyLayer))) {
        impl_->isShapeVertexMarqueeSelecting_ = true;
        impl_->shapeVertexMarqueeStartViewportPos_ =
            screenPhysicalPoint(viewportPos);
        impl_->shapeVertexMarqueeCurrentViewportPos_ =
            screenPhysicalPoint(viewportPos);
        impl_->shapeVertexMarqueeSelectionMode_ =
            selectionModeFromModifiers(event->modifiers());
        impl_->invalidateOverlayComposite();
        markRenderDirty();
        event->accept();
        return;
      }
    }
  }

  // Two-node camera POI handle drag. Tested before the motion-path block so
  // the POI cross owns clicks near it when the camera layer is selected.
  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen &&
      selectedLayer && comp && impl_->renderer_ &&
      selectedCameraPoiHoverable(selectedLayer)) {
    const QMatrix4x4 &poiView =
        impl_->viewportOrientationMatricesValid_
            ? impl_->viewportOrientationViewForOverlay_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DViewMatrix_
                  : impl_->renderer_->getViewMatrix();
    const QMatrix4x4 &poiProj =
        impl_->viewportOrientationMatricesValid_
            ? impl_->viewportOrientationProjectionForOverlay_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DProjectionMatrix_
                  : impl_->renderer_->getProjectionMatrix();
    const QRect poiViewport(
        0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
        std::max(1, static_cast<int>(impl_->hostHeight_)));
    auto *camera =
        dynamic_cast<ArtifactCameraLayer *>(selectedLayer.get());
    const auto poiWorld = camera->pointOfInterestWorld();
    const QPointF poiScreen =
        cameraPoiHandleScreenPos(selectedLayer, poiView, poiProj, poiViewport);
    if (!poiScreen.isNull()) {
      const QPointF physicalPos = viewportPos * impl_->devicePixelRatio_;
      const float hitRadius =
          18.0f * std::max(1.0f, impl_->devicePixelRatio_);
      const float dx = static_cast<float>(physicalPos.x()) -
                       static_cast<float>(poiScreen.x());
      const float dy = static_cast<float>(physicalPos.y()) -
                       static_cast<float>(poiScreen.y());
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        // Drag plane faces the viewing camera through the POI.
        const QMatrix4x4 viewInverse = poiView.inverted();
        const QVector3D planeNormal =
            viewInverse.mapVector(QVector3D(0.0f, 0.0f, 1.0f)).normalized();
        impl_->beginCameraPoiDrag(
            selectedLayer, camera->pointOfInterest(), poiWorld,
            ArtifactCore::Coordinates::worldVectorFromQVector3D(planeNormal));
        setInfoOverlayText(QStringLiteral("Camera POI"),
                           QStringLiteral("Drag to move Point of Interest"));
        notifyViewportInteractionActivity();
        markRenderDirty();
        event->accept();
        return;
      }
    }
  }

  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen &&

      impl_->showMotionPathOverlay_ && selectedLayer && comp &&

      impl_->renderer_) {

    // Historical bounds own corner hits before Shift's legacy key insertion.
    if ((activeTool == ToolType::Selection || activeTool == ToolType::Move ||
         activeTool == ToolType::Scale) && historicalBoundsSupported(selectedLayer) &&
        !event->modifiers().testFlag(Qt::AltModifier) &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
      const auto cursor = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      const QPointF canvas(cursor.x, cursor.y);
      const int fps = std::max(1, static_cast<int>(std::round(comp->frameRate().framerate())));
      const auto frames = historicalBoundsFrames(selectedLayer,
          static_cast<int>(selectedLayer->currentFrame()), fps);
      const double radius = 8.0 * impl_->devicePixelRatio_ /
          std::max(0.001f, impl_->renderer_->getZoom());
      for (auto it = frames.crbegin(); it != frames.crend(); ++it) {
        const auto matrix = selectedLayer->getGlobalTransformAt(*it);
        bool invertible = false;
        const auto inverse = matrix.inverted(&invertible);
        if (!invertible) continue;
        const auto bounds = selectedLayer->localBounds();
        const QPointF corners[4] = {bounds.topLeft(), bounds.topRight(),
                                    bounds.bottomRight(), bounds.bottomLeft()};
        for (const auto &corner : corners) {
          const QPointF delta = matrix.map(corner) - canvas;
          if (QPointF::dotProduct(delta, delta) > radius * radius) continue;
          const auto time = gizmoTransformTime(selectedLayer, *it);
          MotionPathScaleState before;
          if (!UndoManager::instance() || !captureMotionPathScale(selectedLayer, time, before)) continue;
          const auto &transform = selectedLayer->transform3D();
          const QPointF anchor(transform.anchorXAt(time), transform.anchorYAt(time));
          const QPointF start = inverse.map(canvas) - anchor;
          if (std::abs(start.x()) < 0.001 || std::abs(start.y()) < 0.001) continue;
          impl_->historicalScaleLayer_ = selectedLayer;
          impl_->historicalScaleFrame_ = *it;
          impl_->historicalScaleTime_ = time;
          impl_->historicalScaleBefore_ = before;
          impl_->historicalScaleInverse_ = inverse;
          impl_->historicalScaleAnchor_ = anchor;
          impl_->historicalScaleStart_ = start;
          impl_->historicalScaleDragging_ = true;
          impl_->historicalScaleChanged_ = false;
          impl_->selectedMotionPathFrames_.clear();
          impl_->selectedMotionPathFrames_.push_back(*it);
          setInfoOverlayText(QStringLiteral("Motion Path"),
              QStringLiteral("Frame %1: scale around anchor; Shift: uniform; Esc: cancel").arg(*it));
          markRenderDirty();
          event->accept();
          return;
        }
      }
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {

      if (setSelectedLayerMotionPathKeyframeAtCurrentFrame()) {

        event->accept();

        return;

      }

    }



    const QPointF physicalViewportPos =
        viewportPos * impl_->devicePixelRatio_;
    const QMatrix4x4 &motionFrameView = impl_->viewportOrientationMatricesValid_
        ? impl_->viewportOrientationViewForOverlay_
        : impl_->gizmo3DCameraMatricesValid_ ? impl_->gizmo3DViewMatrix_
                                              : impl_->renderer_->getViewMatrix();
    const QMatrix4x4 &motionFrameProjection =
        impl_->viewportOrientationMatricesValid_
            ? impl_->viewportOrientationProjectionForOverlay_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DProjectionMatrix_
                  : impl_->renderer_->getProjectionMatrix();
    const QRect motionFrameViewport(
        0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
        std::max(1, static_cast<int>(impl_->hostHeight_)));
    int pastPlaneFrame = 0;
    const int motionFrameFps = std::max(
        1, static_cast<int>(std::round(comp->frameRate().framerate())));
    if (hitTestPastFixedPlaneFrameCenter(
            selectedLayer, static_cast<int>(selectedLayer->currentFrame()),
            motionFrameFps,
            motionFrameView, motionFrameProjection, motionFrameViewport,
            screenPhysicalPoint(physicalViewportPos),
            14.0f * std::max(1.0f, impl_->devicePixelRatio_),
            pastPlaneFrame)) {
      auto *plane = dynamic_cast<Artifact3DLayer *>(selectedLayer.get());
      QVector3D startHit;
      const Ray ray = pickingLegacyRayFromWorldRay(
          createPickingRay(screenPhysicalPoint(physicalViewportPos)));
      if (plane && intersectPickingRayFixedPlaneAt(*plane, pastPlaneFrame, ray,
                                                   startHit)) {
        const auto time = gizmoTransformTime(selectedLayer, pastPlaneFrame);
        const auto &transform = selectedLayer->transform3D();
        MotionPathPositionSnapshot before;
        before.hasPositionKey = transform.hasPositionKeyFrameAt(time);
        before.position.x = transform.positionXAt(time);
        before.position.y = transform.positionYAt(time);
        if (before.hasPositionKey) {
          impl_->beginPastPlaneFrameDrag(selectedLayer, pastPlaneFrame,
                                         startHit, before);
          event->accept();
          return;
        }
      }
    }

    const auto cPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    const auto motionPathSamples = buildMotionPathSamples(selectedLayer, comp);

    MotionPathSample hitSample;

    const float hitThreshold = std::max(
        8.0f * Accessibility::targetScale(),
        12.0f * Accessibility::targetScale() /
            impl_->renderer_->getZoom());

    int64_t tangentFrame = 0;
    MotionPathTangentHandle tangentHandle = MotionPathTangentHandle::None;
    if (impl_->hitTestMotionPathTangent(QPointF(cPos.x, cPos.y),
                                        hitThreshold, tangentFrame,
                                        tangentHandle)) {
      impl_->beginMotionPathTangentDrag(selectedLayer, tangentFrame,
                                        tangentHandle, event->modifiers());
      setInfoOverlayText(QStringLiteral("Motion Path"),
                         event->modifiers().testFlag(Qt::AltModifier)
                             ? QStringLiteral("Broken tangent — drag independently")
                             : QStringLiteral("Drag tangent to shape the path"));
      event->accept();
      return;
    }

    if (hitTestMotionPathSample(motionPathSamples, QPointF(cPos.x, cPos.y),

                                hitThreshold, hitSample)) {

      const auto selectionMode = selectionModeFromModifiers(event->modifiers());
      if (selectionMode == SelectionMode::Replace) {
        impl_->selectedMotionPathFrames_.clear();
        impl_->selectedMotionPathFrames_.push_back(hitSample.framePosition);
      } else if (selectionMode == SelectionMode::Add) {
        if (!impl_->selectedMotionPathFrames_.contains(hitSample.framePosition))
          impl_->selectedMotionPathFrames_.push_back(hitSample.framePosition);
      } else {
        const int index = impl_->selectedMotionPathFrames_.indexOf(
            hitSample.framePosition);
        if (index >= 0)
          impl_->selectedMotionPathFrames_.removeAt(index);
        else
          impl_->selectedMotionPathFrames_.push_back(hitSample.framePosition);
      }

      const auto &t3d = selectedLayer->transform3D();

      const auto time = gizmoTransformTime(selectedLayer, hitSample.framePosition);

      if (event->modifiers().testFlag(Qt::ControlModifier) &&
          !event->modifiers().testFlag(Qt::ShiftModifier)) {
        impl_->beginMotionPathTangentDrag(selectedLayer,
                                          hitSample.framePosition,
                                          MotionPathTangentHandle::Out,
                                          event->modifiers());
        setInfoOverlayText(QStringLiteral("Motion Path"),
                           QStringLiteral("Drag to create linked tangents"));
        event->accept();
        return;
      }

      if (event->modifiers().testFlag(Qt::AltModifier) &&
          !event->modifiers().testFlag(Qt::ShiftModifier)) {

        MotionPathPositionSnapshot before;

        before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);

        before.position.x = t3d.positionXAt(time);

        before.position.y = t3d.positionYAt(time);

        if (before.hasPositionKey) {

          auto &mutableT3d = selectedLayer->transform3D();

          mutableT3d.removePositionKeyFrameAt(time);

          MotionPathPositionSnapshot after;

          after.hasPositionKey = false;

          after.position.x = mutableT3d.positionXAt(time);

          after.position.y = mutableT3d.positionYAt(time);

          auto *mgr = UndoManager::instance();
          if (mgr && !mgr->push(std::make_unique<MotionPathUndoCommand>(
                                    selectedLayer, time,
                                    before, after))) {
            mutableT3d.setPositionKeyFrameValueAt(
                time, before.position.x, before.position.y);
            if (!before.hasPositionKey) {
              mutableT3d.removePositionKeyFrameAt(time);
            }
            selectedLayer->setDirty(LayerDirtyFlag::Transform);
            selectedLayer->changed();
            impl_->motionPathCache_.valid = false;
            impl_->invalidateOverlayComposite();
            markRenderDirty();
            event->accept();
            return;
          }

          selectedLayer->setDirty(LayerDirtyFlag::Transform);

          selectedLayer->changed();

          if (auto *compPtr =

                  static_cast<ArtifactAbstractComposition *>(selectedLayer->composition())) {

            ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

                LayerChangedEvent{compPtr->id().toString(),

                                  selectedLayer->id().toString(),

                                  LayerChangedEvent::ChangeType::Modified});

          }

          impl_->motionPathCache_.valid = false;

          impl_->invalidateOverlayComposite();

          markRenderDirty();

          event->accept();

          return;

        }

      }

      MotionPathPositionSnapshot before;

      before.hasPositionKey = t3d.hasPositionKeyFrameAt(time);

      before.position.x = t3d.positionXAt(time);

      before.position.y = t3d.positionYAt(time);

      impl_->beginMotionPathDrag(selectedLayer, hitSample.framePosition,

                                 ArtifactCore::Coordinates::CompositionPoint2{
                                     cPos.x, cPos.y}, before,
                                 event->modifiers());
      if (impl_->selectedMotionPathFrames_.size() > 1) {
        const QString mode =
            event->modifiers().testFlag(Qt::ControlModifier) &&
                    event->modifiers().testFlag(Qt::ShiftModifier)
                ? QStringLiteral("Scale")
            : event->modifiers().testFlag(Qt::AltModifier) &&
                    event->modifiers().testFlag(Qt::ShiftModifier)
                ? QStringLiteral("Rotate")
                : QStringLiteral("Move");
        setInfoOverlayText(
            QStringLiteral("Motion Path (%1 keys)").arg(
                impl_->selectedMotionPathFrames_.size()),
            QStringLiteral("%1 selected keys — Undo available").arg(mode));
      }

      impl_->motionPathCache_.valid = false;

      impl_->invalidateOverlayComposite();

      event->accept();

      return;

    }

  }


  // Clicking outside the motion path ends path-key selection before the
  // ordinary layer gizmo receives the same pointer event.
  if (event->button() == Qt::LeftButton &&
      event->modifiers() == Qt::NoModifier) {
    impl_->selectedMotionPathFrames_.clear();
  }


  // The camera-projected frame is outside the ordinary 2D transform space.
  // Keep the hit test in viewport space, but route the drag through the 3D
  // gizmo so the pointer delta is resolved with the active camera ray.
  bool selectedGroupUsesProjectedFrame =
      selectedLayer && layerUsesProjectedFrameGizmo(selectedLayer);
  if (!selectedGroupUsesProjectedFrame) {
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      for (const auto &candidate : selection->selectedLayers()) {
        if (candidate && layerUsesProjectedFrameGizmo(candidate)) {
          selectedGroupUsesProjectedFrame = true;
          break;
        }
      }
    }
  }
  if (event->button() == Qt::LeftButton && selectedLayer &&
      impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Transform3D &&
      !selectedLayer->isLocked() && !selectedLayer->isSelectionLocked() &&
      (selectedGroupUsesProjectedFrame ||
       impl_->viewportOrientationMatricesValid_) &&
      impl_->gizmo3D_ &&
      activeTool != ToolType::Pen && activeTool != ToolType::AnchorPoint) {

    // mousePressEvent already converted the event to physical pixels.
    const QPointF physicalViewportPos = viewportPos;

    const QMatrix4x4 &frameView = impl_->viewportOrientationMatricesValid_
                                      ? impl_->viewportOrientationViewForOverlay_
                                      : impl_->gizmo3DCameraMatricesValid_
                                            ? impl_->gizmo3DViewMatrix_
                                            : impl_->renderer_->getViewMatrix();
    const QMatrix4x4 &frameProjection =
        impl_->viewportOrientationMatricesValid_
            ? impl_->viewportOrientationProjectionForOverlay_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DProjectionMatrix_
                  : impl_->renderer_->getProjectionMatrix();
    const QRect frameViewport(
        0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
        std::max(1, static_cast<int>(impl_->hostHeight_)));

    QVector<ArtifactAbstractLayerPtr> projectedSelection;
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      for (const auto &candidate : selection->selectedLayers()) {
        if (candidate && !candidate->isLocked() &&
            !candidate->isSelectionLocked()) {
          projectedSelection.push_back(candidate);
        }
      }
    }
    const bool combinedProjectedFrame = projectedSelection.size() > 1 &&
        (selectedGroupUsesProjectedFrame ||
         impl_->viewportOrientationMatricesValid_);
    const ScreenPhysicalBounds2 combinedFramePhysicalBounds =
        combinedProjectedFrame
        ? projectedSelectionFrameBounds(projectedSelection, frameView,
                                        frameProjection, frameViewport)
        : ScreenPhysicalBounds2{};
    const QRectF combinedFrameBounds =
        qRectFromScreenPhysicalBounds(combinedFramePhysicalBounds);
    auto frameHandle = combinedProjectedFrame
        ? hitTestProjectedSelectionFrame(
              combinedFramePhysicalBounds,
              screenPhysicalPoint(physicalViewportPos),
              32.0f * std::max(1.0f, impl_->devicePixelRatio_))
        : hitTestProjectedFrameCorner(
              selectedLayer, screenPhysicalPoint(physicalViewportPos),
              frameView, frameProjection,
              frameViewport,
              32.0f * std::max(1.0f, impl_->devicePixelRatio_));
    if (!projectedFrameHandleEnabled(impl_->gizmoMode_, frameHandle)) {
      frameHandle = TransformGizmo::HandleType::None;
    }
    const auto combinedFramePivotWorld = [&](TransformGizmo::HandleType handle) {
      auto fallback = impl_->projectedFrameStartWorldAnchor_;
      if (!combinedProjectedFrame || !isValidScreenPhysicalBounds(combinedFramePhysicalBounds)) {
        return fallback;
      }
      QPointF pivot = combinedFrameBounds.center();
      switch (handle) {
      case TransformGizmo::HandleType::Scale_TL:
        pivot = combinedFrameBounds.bottomRight();
        break;
      case TransformGizmo::HandleType::Scale_TR:
        pivot = combinedFrameBounds.bottomLeft();
        break;
      case TransformGizmo::HandleType::Scale_BL:
        pivot = combinedFrameBounds.topRight();
        break;
      case TransformGizmo::HandleType::Scale_BR:
        pivot = combinedFrameBounds.topLeft();
        break;
      case TransformGizmo::HandleType::Scale_T:
        pivot = QPointF(combinedFrameBounds.center().x(),
                        combinedFrameBounds.bottom());
        break;
      case TransformGizmo::HandleType::Scale_B:
        pivot = QPointF(combinedFrameBounds.center().x(),
                        combinedFrameBounds.top());
        break;
      case TransformGizmo::HandleType::Scale_L:
        pivot = QPointF(combinedFrameBounds.right(),
                        combinedFrameBounds.center().y());
        break;
      case TransformGizmo::HandleType::Scale_R:
        pivot = QPointF(combinedFrameBounds.left(),
                        combinedFrameBounds.center().y());
        break;
      default:
        break;
      }
      const auto &transform = selectedLayer->transform3D();
      const auto time =
          gizmoTransformTime(selectedLayer, selectedLayer->currentFrame());
      const QVector3D anchorWorld = selectedLayer->getGlobalTransform4x4().map(
          QVector3D(transform.anchorXAt(time), transform.anchorYAt(time),
                    transform.anchorZAt(time)));
      const QVector3D anchorProjected = ViewportMath::projectToTopDown(
          anchorWorld, frameView, frameProjection, frameViewport);
      if (!std::isfinite(anchorProjected.z())) {
        return fallback;
      }
      return ArtifactCore::Coordinates::worldPoint3FromQVector3D(
          ViewportMath::unprojectFromTopDown(
              QVector3D(static_cast<float>(pivot.x()),
                        static_cast<float>(pivot.y()), anchorProjected.z()),
              frameView, frameProjection, frameViewport));
    };
    const auto configureCombinedGroupBasis = [&]() {
      if (!combinedProjectedFrame || !impl_->gizmoGroupTransformActive_) {
        return;
      }
      bool invertible = false;
      const QMatrix4x4 inverseView = frameView.inverted(&invertible);
      if (!invertible) return;
      QVector3D basisX = inverseView.mapVector(QVector3D(1.0f, 0.0f, 0.0f));
      QVector3D basisY = inverseView.mapVector(QVector3D(0.0f, -1.0f, 0.0f));
      if (basisX.lengthSquared() <= 0.000001f ||
          basisY.lengthSquared() <= 0.000001f) return;
      basisX.normalize();
      basisY.normalize();
      QVector3D basisZ = QVector3D::crossProduct(basisX, basisY).normalized();
      impl_->gizmoGroupBasisX_ =
          ArtifactCore::Coordinates::worldVectorFromQVector3D(basisX);
      impl_->gizmoGroupBasisY_ =
          ArtifactCore::Coordinates::worldVectorFromQVector3D(basisY);
      impl_->gizmoGroupBasisZ_ =
          ArtifactCore::Coordinates::worldVectorFromQVector3D(basisZ);
      impl_->gizmoGroupProjectedBasisValid_ = true;
      impl_->gizmo3D_->setLocalBasis(
          impl_->gizmoGroupBasisX_, impl_->gizmoGroupBasisY_,
          impl_->gizmoGroupBasisZ_);
    };

    // Projected-frame geometry is drawn in physical pixels. Keep the hit test
    // in that same space so an interior drag cannot fall through to a 3D axis
    // or rotation ring on HiDPI displays.
    const bool frameInteriorHit = combinedProjectedFrame
        ? combinedFrameBounds.contains(physicalViewportPos)
        : hitTestProjectedFrameInterior(
              selectedLayer, screenPhysicalPoint(physicalViewportPos),
              frameView, frameProjection, frameViewport);

    // Explicit frame handles own the drag before axes; axes still take
    // priority over the frame interior's move fallback.
    const WorldRay priorityRay =
        createPickingRay(screenPhysicalPoint(physicalViewportPos));
    const GizmoAxis priorityAxis =
        impl_->gizmo3D_->hitTest(priorityRay,
                                 frameView, frameProjection);
    if (frameHandle == TransformGizmo::HandleType::None &&
        priorityAxis != GizmoAxis::None) {
      beginGizmoUndoSnapshot();
      configureCombinedGroupBasis();
      impl_->gizmo3D_->beginDrag(
          priorityAxis, priorityRay,
          impl_->gizmo3D_->hoverScaleSigns());
      impl_->projectedFrameHandle_ = TransformGizmo::HandleType::None;
      impl_->projectedFrameMove_ = false;
      impl_->projectedFrameScalePointerBasisValid_ = false;
      impl_->projectedFrameLastPointer_ =
          screenPhysicalPoint(physicalViewportPos);
      impl_->projectedFrameLastPointerValid_ = true;
      impl_->beginGizmoSession(GizmoOwner::Transform3D, selectedLayer);
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->gizmoDragRenderTimer_.restart();
      impl_->invalidateOverlayComposite();
      markRenderDirty();
      event->accept();
      return;
    }

    if (frameHandle != TransformGizmo::HandleType::None) {
      impl_->projectedFrameScalePointerBasisValid_ = false;
      if (frameHandle != TransformGizmo::HandleType::Rotate) {
        const bool scaleFromCenter =
            event->modifiers().testFlag(Qt::ControlModifier);
        ArtifactCore::Coordinates::ScreenPhysicalPoint2 fixedPoint{};
        if (combinedProjectedFrame) {
          if (projectedFrameScaleFixedPoint(combinedFramePhysicalBounds,
                                            frameHandle,
                                            scaleFromCenter, fixedPoint)) {
            impl_->projectedFrameScaleStartPointer_ =
                screenPhysicalPoint(viewportPos);
            impl_->projectedFrameScaleFixedPointer_ = fixedPoint;
            const auto startVector =
                impl_->projectedFrameScaleStartPointer_ -
                impl_->projectedFrameScaleFixedPointer_;
            impl_->projectedFrameScalePointerBasisValid_ =
                startVector.x * startVector.x +
                    startVector.y * startVector.y > 0.25f;
          }
        } else {
          const auto& transform = selectedLayer->transform3D();
          const auto time = gizmoTransformTime(selectedLayer,
                                               selectedLayer->currentFrame());
          {
            const QVector3D fixedWorld = selectedLayer->getGlobalTransform4x4().map(
                QVector3D(transform.anchorXAt(time), transform.anchorYAt(time),
                          transform.anchorZAt(time)));
            const QVector3D fixedProjected = ViewportMath::projectToTopDown(
                fixedWorld, frameView, frameProjection, frameViewport);
            if (std::isfinite(fixedProjected.x()) &&
                std::isfinite(fixedProjected.y()) && fixedProjected.z() >= 0.0f &&
                fixedProjected.z() <= 1.0f) {
              impl_->projectedFrameScaleStartPointer_ =
                  screenPhysicalPoint(viewportPos);
              impl_->projectedFrameScaleFixedPointer_ =
                  screenPhysicalPoint(
                      QPointF(fixedProjected.x(), fixedProjected.y()));
              const auto startVector =
                  impl_->projectedFrameScaleStartPointer_ -
                  impl_->projectedFrameScaleFixedPointer_;
              impl_->projectedFrameScalePointerBasisValid_ =
                  startVector.x * startVector.x +
                      startVector.y * startVector.y > 0.25f;
            }
          }
        }
        // SPEC 13.1: remember the projected handle position at drag start so
        // the resize guide can leave a reference mark behind. The frame
        // projection and the hit test use the same matrices, so the mark lands
        // on the handle the user grabbed.
        impl_->projectedFrameScaleStartHandlePointValid_ = false;
        if (combinedProjectedFrame) {
          const std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
              startCorners{screenPhysicalPoint(combinedFrameBounds.topLeft()),
                           screenPhysicalPoint(combinedFrameBounds.topRight()),
                           screenPhysicalPoint(combinedFrameBounds.bottomRight()),
                           screenPhysicalPoint(combinedFrameBounds.bottomLeft())};
          ArtifactCore::Coordinates::ScreenPhysicalPoint2 startMoving{};
          ArtifactCore::Coordinates::ScreenPhysicalPoint2 startFixed{};
          if (projectedFrameGuidePoints(startCorners, frameHandle, startMoving,
                                        startFixed)) {
            impl_->projectedFrameScaleStartHandlePoint_ = startMoving;
            impl_->projectedFrameScaleStartHandlePointValid_ = true;
          }
        } else {
          std::array<ArtifactCore::Coordinates::ScreenPhysicalPoint2, 4>
              startCorners;
          if (projectedLayerFrameCorners(selectedLayer, frameView,
                                         frameProjection, frameViewport,
                                         startCorners)) {
            ArtifactCore::Coordinates::ScreenPhysicalPoint2 startMoving{};
            ArtifactCore::Coordinates::ScreenPhysicalPoint2 startFixed{};
            if (projectedFrameGuidePoints(startCorners, frameHandle,
                                          startMoving, startFixed)) {
              impl_->projectedFrameScaleStartHandlePoint_ = startMoving;
              impl_->projectedFrameScaleStartHandlePointValid_ = true;
            }
          }
        }
      }
      beginGizmoUndoSnapshot();
      configureCombinedGroupBasis();
      if (combinedProjectedFrame && impl_->gizmoGroupTransformActive_) {
        const auto pivotHandle = event->modifiers().testFlag(Qt::ControlModifier)
            ? TransformGizmo::HandleType::None
            : frameHandle;
        impl_->projectedFrameStartWorldAnchor_ =
            combinedFramePivotWorld(pivotHandle);
        impl_->gizmoGroupPivotBefore_ =
            impl_->projectedFrameStartWorldAnchor_;
      }
      impl_->gizmo3D_->setTransform(
          impl_->projectedFrameStartWorldAnchor_,
          impl_->gizmo3D_->rotation());
      impl_->projectedFramePreviousSpace_ = impl_->gizmo3D_->space();
      impl_->projectedFrameForcedLocal_ = true;
      impl_->projectedFramePreviousMode_ = impl_->gizmo3D_->mode();
      impl_->projectedFrameForcedMode_ = true;
      impl_->gizmo3D_->setSpace(GizmoSpace::Local);
      impl_->gizmo3D_->setMode(GizmoMode::Scale);
      impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
      // Projected frame edges represent one local axis, while corners keep
      // the screen-plane/uniform behavior of the frame resize path.  Routing
      // the edge handles to the matching 3D axis prevents a vertical edge
      // drag from changing both dimensions.
      GizmoAxis frameAxis = GizmoAxis::Screen;
      float frameAxisDirectionSign = 1.0f;
      const bool isFrameRotation =
          frameHandle == TransformGizmo::HandleType::Rotate;
      if (isFrameRotation) {
        impl_->gizmo3D_->setMode(GizmoMode::Rotate);
        frameAxis = GizmoAxis::Z;
      } else {
        switch (frameHandle) {
        case TransformGizmo::HandleType::Scale_T:
          frameAxis = GizmoAxis::Y;
          break;
        case TransformGizmo::HandleType::Scale_B:
          frameAxis = GizmoAxis::Y;
          frameAxisDirectionSign = -1.0f;
          break;
        case TransformGizmo::HandleType::Scale_L:
          frameAxis = GizmoAxis::X;
          frameAxisDirectionSign = -1.0f;
          break;
        case TransformGizmo::HandleType::Scale_R:
          frameAxis = GizmoAxis::X;
          break;
        default:
          break;
        }
      }
      impl_->gizmo3D_->beginDrag(frameAxis,
                                 createPickingRay(
                                     screenPhysicalPoint(physicalViewportPos)),
                                 frameAxisDirectionSign);

      impl_->projectedFrameHandle_ = frameHandle;
      impl_->projectedFrameHoverHandle_ =
          TransformGizmo::HandleType::None;
      impl_->projectedFrameMove_ = false;
      impl_->projectedFrameLastPointer_ = screenPhysicalPoint(viewportPos);
      impl_->projectedFrameLastPointerValid_ = true;
      impl_->beginGizmoSession(GizmoOwner::Transform3D, selectedLayer);
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->gizmoDragRenderTimer_.restart();
      impl_->invalidateOverlayComposite();
      markRenderDirty();
      event->accept();
      return;

    }

    if (frameInteriorHit) {
      beginGizmoUndoSnapshot();
      if (combinedProjectedFrame && impl_->gizmoGroupTransformActive_) {
        impl_->projectedFrameStartWorldAnchor_ =
            combinedFramePivotWorld(TransformGizmo::HandleType::None);
        impl_->gizmoGroupPivotBefore_ =
            impl_->projectedFrameStartWorldAnchor_;
      }
      impl_->gizmo3D_->setTransform(
          impl_->projectedFrameStartWorldAnchor_,
          impl_->gizmo3D_->rotation());
      impl_->projectedFramePreviousMode_ = impl_->gizmo3D_->mode();
      impl_->projectedFrameForcedMode_ = true;
      impl_->gizmo3D_->setMode(GizmoMode::Move);
      impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
      impl_->gizmo3D_->beginDrag(GizmoAxis::Screen,
                                 createPickingRay(
                                     screenPhysicalPoint(physicalViewportPos)));
      impl_->projectedFrameHandle_ = TransformGizmo::HandleType::None;
      impl_->projectedFrameMove_ = true;
      impl_->projectedFrameScalePointerBasisValid_ = false;
      impl_->projectedFrameLastPointer_ = screenPhysicalPoint(viewportPos);
      impl_->projectedFrameLastPointerValid_ = true;
      impl_->beginGizmoSession(GizmoOwner::Transform3D, selectedLayer);
      impl_->gizmoDragActive_ = true;
      notifyViewportInteractionActivity();
      impl_->gizmoDragRenderTimer_.restart();
      impl_->invalidateOverlayComposite();
      markRenderDirty();
      event->accept();
      return;
    }

  }

  // Unified frame gizmo hit test (GIZ-2) — also used for 2D layers.

  if (event->button() == Qt::LeftButton && selectedLayer && impl_->gizmo3D_ &&
      impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Transform3D &&
      !selectedLayer->isLocked() && !selectedLayer->isSelectionLocked() &&
      activeTool != ToolType::Pen) {

    impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());

    WorldRay ray = createPickingRay(screenPhysicalPoint(viewportPos));

    const QMatrix4x4& gizmoView = impl_->gizmo3DCameraMatricesValid_
                                      ? impl_->gizmo3DViewMatrix_
                                      : impl_->renderer_->getViewMatrix();
    const QMatrix4x4& gizmoProj = impl_->gizmo3DCameraMatricesValid_
                                      ? impl_->gizmo3DProjectionMatrix_
                                      : impl_->renderer_->getProjectionMatrix();
    GizmoAxis axis = impl_->gizmo3D_->hitTest(
        ray, gizmoView, gizmoProj);

    if (axis != GizmoAxis::None) {

      beginGizmoUndoSnapshot();
      // Anchor drags are relative to the anchor captured at press time: the
      // gizmo reports a world delta from its own start hit, and the layer is
      // rewritten every frame, so both sides need a fixed reference instead of
      // reading the already-moved value.
      if (axis == GizmoAxis::Anchor) {
        const auto anchorLayer = comp ? comp->layerById(impl_->selectedLayerId_)
                                      : ArtifactAbstractLayerPtr{};
        if (anchorLayer) {
          const auto anchorTime =
              gizmoTransformTime(anchorLayer, anchorLayer->currentFrame());
          const auto &anchorT3 = anchorLayer->transform3D();
          impl_->gizmoAnchorDragStartLayer_ = anchorLayer;
          impl_->gizmoAnchorDragStart_ =
              ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                  QVector3D(anchorT3.anchorXAt(anchorTime),
                            anchorT3.anchorYAt(anchorTime),
                            anchorT3.anchorZAt(anchorTime)));
          impl_->gizmoAnchorDragStartPosition_ = anchorLayer->position3D();
        }
      }
      impl_->gizmo3D_->beginDrag(
          axis, ray,
          impl_->gizmo3D_->hoverScaleSigns());

      impl_->beginGizmoSession(GizmoOwner::Transform3D, selectedLayer);
      impl_->gizmoDragActive_ = true;

      notifyViewportInteractionActivity();

      impl_->invalidateOverlayComposite();

      markRenderDirty();

      return;

    }

  }



  // Gizmo hit test first (2D)

  if (event->button() == Qt::LeftButton && activeTool != ToolType::Pen) {

    ArtifactAbstractLayerPtr gizmoLayer;

    if (comp && !impl_->selectedLayerId_.isNil()) {

      gizmoLayer = comp->layerById(impl_->selectedLayerId_);

    }

    if (impl_->contentGizmo_ &&
        impl_->gizmoOwnerForLayer(gizmoLayer) == GizmoOwner::Content &&
        !gizmoLayer->isLocked() && !gizmoLayer->isSelectionLocked()) {

      impl_->contentGizmo_->setLayer(gizmoLayer);

      impl_->contentGizmo_->handleMousePress(viewportPos,
                                             impl_->renderer_.get());

      if (impl_->contentGizmo_->isDragging()) {

        impl_->beginGizmoSession(GizmoOwner::Content, gizmoLayer);

        notifyViewportInteractionActivity();

        impl_->gizmoDragRenderTimer_.restart();

        return;

      }

    } else if (impl_->textGizmo_ &&
               impl_->gizmoOwnerForLayer(gizmoLayer) == GizmoOwner::Text &&
               !gizmoLayer->isLocked() && !gizmoLayer->isSelectionLocked()) {

      impl_->textGizmo_->setLayer(gizmoLayer);

      impl_->textGizmo_->handleMousePress(viewportPos, impl_->renderer_.get());

      if (impl_->textGizmo_->isDragging()) {

        impl_->beginGizmoSession(GizmoOwner::Text, gizmoLayer);

        notifyViewportInteractionActivity();

        impl_->gizmoDragRenderTimer_.restart();

        return;

      }

    } else if (impl_->gizmo_ &&
               isTransformGizmoOwner(impl_->gizmoOwnerForLayer(gizmoLayer)) &&
               !gizmoLayer->isLocked() && !gizmoLayer->isSelectionLocked() &&
               legacy2DGizmoShouldOwnPress(
                   this, impl_->gizmoOwnerForLayer(gizmoLayer))) {

      impl_->gizmo_->handleMousePress(viewportPos, impl_->renderer_.get());

      if (impl_->gizmo_->isDragging()) {

        if (isDesignWorkspace(this) && !isAbsoluteLayoutLayer(gizmoLayer) &&
            impl_->gizmo_->activeHandle() == TransformGizmo::HandleType::Move) {

          impl_->gizmo_->handleMouseRelease();
          impl_->gizmoDragActive_ = false;
          impl_->designReorderActive_ = false;
          impl_->designReorderTargetIndex_ = -1;
          if (gizmoLayer && comp && !gizmoLayer->parentLayerId().isNil()) {
            const auto layoutParent = comp->layerById(gizmoLayer->parentLayerId());
            const auto enabledProperty = layoutParent
                                             ? layoutParent->getProperty(
                                                   QStringLiteral("component.layout.enabled"))
                                             : nullptr;
            if (enabledProperty && enabledProperty->getValue().toBool()) {
              const auto layers = comp->allLayer();
              for (int index = 0; index < layers.size(); ++index) {
                if (layers.at(index) && layers.at(index)->id() == gizmoLayer->id()) {
                  impl_->designReorderTargetIndex_ = index;
                  break;
                }
              }
              impl_->designReorderActive_ = impl_->designReorderTargetIndex_ >= 0;
              impl_->designReorderLayerId_ = gizmoLayer->id();
              impl_->designReorderParentId_ = gizmoLayer->parentLayerId();
            }
          }
          setInfoOverlayText(
              QStringLiteral("Design"),
              impl_->designReorderActive_
                  ? QStringLiteral("Drag across siblings to reorder")
                  : QStringLiteral("Position is layout-owned; enable Auto Layout on the parent"));
          impl_->invalidateOverlayComposite();
          markRenderDirty();
          return;

        }

        impl_->beginGizmoSession(GizmoOwner::Transform2D, gizmoLayer);
        impl_->gizmoDragActive_ = true;

        notifyViewportInteractionActivity();

        impl_->gizmoDragRenderTimer_.restart();

        return;

      }

    }

  }



  // Tracker gizmo hit test (TrackPoint tool)

  if (activeTool == ToolType::TrackPoint && impl_->trackerGizmo_) {

    if (!impl_->trackerMotionTracker_) {

      trackerInitialize();

    }

    if (impl_->trackerPointPlacementArmed_) {
      impl_->trackerPointPlacementArmed_ = false;
      auto *tracker = impl_->trackerMotionTracker_;
      if (tracker && !tracker->hasResult() &&
          tracker->trackerType() == ArtifactCore::TrackerType::Point &&
          impl_->renderer_) {
        const auto canvas = impl_->renderer_->viewportToCanvas(
            {static_cast<float>(viewportPos.x()),
             static_cast<float>(viewportPos.y())});
        const QString previousJson = tracker->toJson();
        if (tracker->addTrackPoint(QPointF(canvas.x, canvas.y)) >= 0) {
          pushTrackerUndo(previousJson, tracker->toJson());
          setInfoOverlayText(
              trackerModeTitle(tracker),
              QStringLiteral("Added tracking point %1")
                  .arg(tracker->trackPointCount()));
        }
      }
      markRenderDirty();
      return;
    }

    impl_->trackerGizmo_->handleMousePress(viewportPos, impl_->renderer_.get());

    if (impl_->trackerGizmo_->isDragging()) {

      impl_->trackerPreDragJson_ =
          impl_->trackerGizmo_->activeHandle() ==
                  ArtifactPointTrackerGizmo::HandleType::PathPoint &&
                  impl_->trackerMotionTracker_
              ? impl_->trackerMotionTracker_->toJson()
              : QString();

      impl_->trackerGizmoDragActive_ = true;

      notifyViewportInteractionActivity();

      impl_->gizmoDragRenderTimer_.restart();

      return;

    }

  }



  if (event->button() == Qt::LeftButton) {

    if (comp && impl_->renderer_) {

      // Cryptomatte ID picking is deliberately available only while
      // inspecting an Object or Material ID AOV. The readback is synchronous
      // and can allocate, so it must stay a click-only, cold interaction
      // rather than becoming part of the pointer-move or ordinary geometry
      // hit-test path.
      const bool objectIdPicking = impl_->viewportChannelDisplayMode_ ==
          ViewportChannelDisplayMode::ObjectId;
      const bool materialIdPicking = impl_->viewportChannelDisplayMode_ ==
          ViewportChannelDisplayMode::MaterialId;
      if ((objectIdPicking || materialIdPicking) &&
          event->modifiers() == Qt::NoModifier) {
        const auto cryptomatteAov =
            impl_->renderer_->readbackToMultiChannelImage();
        const auto cryptomatteIds = cryptomatteAov.getChannel(
            materialIdPicking ? ArtifactCore::ChannelType::MaterialId
                              : ArtifactCore::ChannelType::ObjectId);
        if (cryptomatteIds && cryptomatteAov.width() > 0 &&
            cryptomatteAov.height() > 0 && impl_->hostWidth_ > 0.0f &&
            impl_->hostHeight_ > 0.0f) {
          // Object/Material ID use the same aspect-fitted CPU overlay as the
          // visible channel image. Sampling against the whole host incorrectly
          // maps clicks whenever the composition is letterboxed.
          const float displayScale = std::min(
              impl_->hostWidth_ / static_cast<float>(cryptomatteAov.width()),
              impl_->hostHeight_ /
                  static_cast<float>(cryptomatteAov.height()));
          const float displayWidth =
              static_cast<float>(cryptomatteAov.width()) * displayScale;
          const float displayHeight =
              static_cast<float>(cryptomatteAov.height()) * displayScale;
          const float displayX = (impl_->hostWidth_ - displayWidth) * 0.5f;
          const float displayY = (impl_->hostHeight_ - displayHeight) * 0.5f;
          const bool isInsideDisplay =
              viewportPos.x() >= displayX && viewportPos.x() < displayX + displayWidth &&
              viewportPos.y() >= displayY && viewportPos.y() < displayY + displayHeight;
          if (isInsideDisplay) {
          const int sampleX = std::clamp(
              static_cast<int>((viewportPos.x() - displayX) *
                               cryptomatteAov.width() / displayWidth),
              0, cryptomatteAov.width() - 1);
          const int sampleY = std::clamp(
              static_cast<int>((viewportPos.y() - displayY) *
                               cryptomatteAov.height() / displayHeight),
              0, cryptomatteAov.height() - 1);
          const std::size_t sampleIndex =
              static_cast<std::size_t>(sampleY) * cryptomatteAov.width() +
              static_cast<std::size_t>(sampleX);
          if (sampleIndex < cryptomatteIds->size()) {
            const std::uint32_t pickedId =
                ArtifactCore::Image::Cryptomatte::CryptoSample::floatToId(
                    cryptomatteIds->data()[sampleIndex]);
            const auto &layers = comp->allLayerRef();
            ArtifactAbstractLayerPtr pickedLayer;
            // The ID target stores the front-most visible object. Iterate in
            // that same top-down order, while still requiring an exact
            // uint32 payload match rather than a lossy float comparison.
            for (int index = static_cast<int>(layers.size()) - 1;
                 index >= 0; --index) {
              const auto &candidate = layers[index];
              if (!candidate || !isLayerEffectivelyVisible(candidate) ||
                  !candidate->isActiveAt(currentFrameForComposition(comp))) {
                continue;
              }
              QString candidateKey = candidate->id().toString();
              if (materialIdPicking) {
                candidateKey = candidate->layerName() +
                    QStringLiteral("|material");
                if (const auto *modelLayer =
                        dynamic_cast<const Artifact3DLayer *>(candidate.get())) {
                  candidateKey = modelLayer->materialSignature();
                }
              }
              if (ArtifactCore::Image::Cryptomatte::CryptoSample::nameToId(
                      candidateKey) == pickedId) {
                pickedLayer = candidate;
                break;
              }
            }
            if (pickedLayer) {
              if (auto *selection = ArtifactApplicationManager::instance()
                                        ? ArtifactApplicationManager::instance()
                                              ->layerSelectionManager()
                                        : nullptr) {
                if (auto *svc = ArtifactProjectService::instance()) {
                  svc->selectLayer(pickedLayer->id());
                } else {
                  selection->selectLayer(pickedLayer);
                }
                if (!selection->currentLayer()) {
                  selection->selectLayer(pickedLayer);
                }
              }
              syncPrimarySelectionLayer(pickedLayer);
              impl_->sync2DGizmosForLayer(pickedLayer);
              if (impl_->gizmo3D_) {
                impl_->syncGizmo3DFromLayer(pickedLayer);
              }
              setInfoOverlayText(QStringLiteral("Cryptomatte"),
                                 QStringLiteral("Selected %1")
                                     .arg(pickedLayer->layerName()));
              impl_->invalidateOverlayComposite();
              markRenderDirty();
              event->accept();
              return;
            }
          }
          }
        }
      }

      const auto cPos = impl_->renderer_->viewportToCanvas(

          {(float)viewportPos.x(), (float)viewportPos.y()});

      auto *selection =

          ArtifactApplicationManager::instance()

              ? ArtifactApplicationManager::instance()->layerSelectionManager()

              : nullptr;

      const auto currentFrame = currentFrameForComposition(comp);



      const auto &layers = comp->allLayerRef();



      ArtifactAbstractLayerPtr hitLayer = nullptr;

      QVector<ArtifactAbstractLayerPtr> hitLayers;

      const bool ignoreLocked = event->modifiers().testFlag(Qt::AltModifier);

      const bool backPick = event->modifiers().testFlag(Qt::ControlModifier);



      // Collect all layers at this position

      for (int i = (int)layers.size() - 1; i >= 0; --i) {

        auto &layer = layers[i];

        if (!isLayerEffectivelyVisible(layer))

          continue;

        if ((layer->isLocked() || layer->isSelectionLocked()) && !ignoreLocked)

          continue;

        if (!layer->isActiveAt(currentFrame))

          continue;



        const QTransform globalTransform = layer->getGlobalTransform();

        bool invertible = false;

        const QTransform invTransform = globalTransform.inverted(&invertible);



        bool isHit = false;

        if (invertible) {

          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));

          if (layer->localBounds().contains(localPos)) {

            isHit = true;

          }

        } else {

          auto bbox = layer->transformedBoundingBox();

          if (bbox.contains(cPos.x, cPos.y)) {

            isHit = true;

          }

        }



        if (isHit) {

          hitLayers.push_back(layer);

        }

      }



      // Cyclic selection logic

      if (!hitLayers.isEmpty()) {

        if (backPick && hitLayers.size() > 1) {

          hitLayer = hitLayers[1];

        } else {

          const float posThreshold = 3.0f; // px

          const bool sameSpot =

              QVector2D(viewportPos - impl_->lastHitPosition_).length() <

              posThreshold;



          if (sameSpot && !impl_->lastHitLayerId_.isNil()) {

            int currentHitIdx = -1;

            for (int i = 0; i < hitLayers.size(); ++i) {

              if (hitLayers[i]->id() == impl_->lastHitLayerId_) {

                currentHitIdx = i;

                break;

              }

            }



            if (currentHitIdx != -1) {

              int nextIdx = (currentHitIdx + 1) % hitLayers.size();

              hitLayer = hitLayers[nextIdx];

            } else {

              hitLayer = hitLayers[0];

            }

          } else {

            hitLayer = hitLayers[0];

          }

        }



        impl_->lastHitPosition_ = viewportPos;

        impl_->lastHitLayerId_ = hitLayer->id();

      } else {

        impl_->lastHitLayerId_ = LayerID::Nil();

      }



      if (hitLayer) {

        const ArtifactAbstractLayerPtr hitLayerRef = hitLayer;

        if (selection) {

          const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);

          const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

          if (ctrl) {

            if (selection->isSelected(hitLayerRef)) {

              selection->removeFromSelection(hitLayerRef);

            } else {

              selection->addToSelection(hitLayerRef);

            }

          } else if (shift) {

            selection->addToSelection(hitLayerRef);

          } else {

            if (auto *svc = ArtifactProjectService::instance()) {

              svc->selectLayer(hitLayer->id());

            } else {

              selection->selectLayer(hitLayerRef);

            }

            // If svc failed to update the selection manager (e.g., its

            // currentComposition doesn't match the render controller's

            // composition), it may have called clearSelection(). Fix this

            // directly so the deferred syncSelectionState() won't nullify the

            // gizmo on the next tick.

            if (selection && !selection->currentLayer()) {

              selection->selectLayer(hitLayerRef);

            }

          }

        }



        // Use hitLayer as the fallback: if the selection manager still has no

        // current layer (composition mismatch, etc.), don't override with null.

        ArtifactAbstractLayerPtr primaryLayer = hitLayer;

        if (selection) {

          if (auto current = selection->currentLayer()) {

            primaryLayer = current;

          }

        }

        syncPrimarySelectionLayer(primaryLayer);

        impl_->sync2DGizmosForLayer(primaryLayer);

        if (impl_->gizmo3D_ && primaryLayer) {

          impl_->syncGizmo3DFromLayer(primaryLayer);

        }



        if (activeTool == ToolType::Selection) {

          impl_->clearSelectionGestureState();

          impl_->isRubberBandSelecting_ = false;

          impl_->dragGroupMove_ = selection &&

                                  selection->selectedLayers().size() > 1 &&

                                  selection->isSelected(hitLayerRef);

          impl_->dragGroupLayers_.clear();

          impl_->dragGroupStartPositions_.clear();

          if (impl_->dragGroupMove_ && selection) {

            const auto selected = selection->selectedLayers();

            impl_->dragGroupLayers_.reserve(selected.size());

            for (const auto &layer : selected) {

              if (!layer) {

                continue;

              }

              const QString id = layer->id().toString();

              impl_->dragGroupLayers_.push_back(layer);

              impl_->dragGroupStartPositions_.insert(

                  id, QPointF(layer->transform3D().positionX(),

                              layer->transform3D().positionY()));

            }

            impl_->dragMode_ = LayerDragMode::Move;

          } else {

            impl_->dragMode_ = hitTestLayerDragMode(hitLayer, event->position(),

                                                    impl_->renderer_.get());

            if (impl_->dragMode_ == LayerDragMode::None) {

              impl_->dragMode_ = LayerDragMode::Move;

            }

          }



          impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);

          impl_->dragStartLayerPos_ =

              QPointF(hitLayer->transform3D().positionX(),

                      hitLayer->transform3D().positionY());

          impl_->dragStartScaleX_ = hitLayer->transform3D().scaleX();

          impl_->dragStartScaleY_ = hitLayer->transform3D().scaleY();

          impl_->dragStartBoundingBox_ = hitLayer->transformedBoundingBox();

          impl_->dragFrame_ = comp->framePosition().framePosition();

          impl_->dragAppliedDelta_ = QPointF(0.0, 0.0);

          impl_->isDraggingLayer_ = true;

        } else {

          impl_->clearSelectionGestureState();

          impl_->isDraggingLayer_ = false;

          impl_->dragMode_ = LayerDragMode::None;

        }

      } else {

        const bool selectionTool = activeTool == ToolType::Selection;

        if (selectionTool) {

          impl_->clearSelectionGestureState();

          impl_->isDraggingLayer_ = false;

          impl_->dragMode_ = LayerDragMode::None;

          impl_->isRubberBandSelecting_ = true;
          impl_->isLassoSelecting_ =
              event->modifiers().testFlag(Qt::AltModifier);
          impl_->lassoViewportPoints_.clear();
          impl_->lassoViewportPoints_.push_back(screenPhysicalPoint(viewportPos));

          impl_->rubberBandStartViewportPos_ = screenPhysicalPoint(viewportPos);

          impl_->rubberBandCurrentViewportPos_ = screenPhysicalPoint(viewportPos);

          impl_->selectionMode_ =

              selectionModeFromModifiers(event->modifiers());

        } else {

          if (!(event->modifiers() & Qt::ShiftModifier)) {

            if (selection) {

              selection->clearSelection();

            }

            setSelectedLayerId(LayerID::Nil());

            impl_->sync2DGizmosForLayer(nullptr);

          }

          impl_->clearSelectionGestureState();

          impl_->isDraggingLayer_ = false;

          impl_->dragMode_ = LayerDragMode::None;

        }

      }

    }

  }

}

Qt::CursorShape CompositionRenderController::cursorShapeForViewportPos(

    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {

  // Two-node camera POI handle cursor (checked first so it wins over gizmos).
  {
    auto *appManager = ArtifactApplicationManager::instance();
    auto *toolManager = appManager ? appManager->toolManager() : nullptr;
    const ToolType currentTool =
        toolManager ? toolManager->activeTool() : ToolType::Selection;
    auto compCursor = impl_->previewPipeline_.composition();
    auto layerCursor =
        (compCursor && !impl_->selectedLayerId_.isNil())
            ? compCursor->layerById(impl_->selectedLayerId_)
            : ArtifactAbstractLayerPtr{};
    if (currentTool != ToolType::Pen && selectedCameraPoiHoverable(layerCursor) &&
        impl_->renderer_) {
      const QMatrix4x4 &poiView =
          impl_->viewportOrientationMatricesValid_
              ? impl_->viewportOrientationViewForOverlay_
              : impl_->gizmo3DCameraMatricesValid_
                    ? impl_->gizmo3DViewMatrix_
                    : impl_->renderer_->getViewMatrix();
      const QMatrix4x4 &poiProj =
          impl_->viewportOrientationMatricesValid_
              ? impl_->viewportOrientationProjectionForOverlay_
              : impl_->gizmo3DCameraMatricesValid_
                    ? impl_->gizmo3DProjectionMatrix_
                    : impl_->renderer_->getProjectionMatrix();
      const QRect poiViewport(
          0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
          std::max(1, static_cast<int>(impl_->hostHeight_)));
      const QPointF hit = cameraPoiHandleScreenPos(
          layerCursor, poiView, poiProj, poiViewport);
      if (!hit.isNull()) {
        const auto phys = ArtifactCore::Coordinates::toScreenPhysical(
            viewportPos, impl_->devicePixelRatio_);
        const float hitRadius =
            18.0f * std::max(1.0f, impl_->devicePixelRatio_);
        const float dx = phys.x -
                         static_cast<float>(hit.x());
        const float dy = phys.y -
                         static_cast<float>(hit.y());
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
          return Qt::SizeAllCursor;
        }
      }
    }
  }


  const auto *app = ArtifactApplicationManager::instance();

  const auto *toolManager = app ? app->toolManager() : nullptr;

  const ToolType activeTool =

      toolManager != nullptr ? toolManager->activeTool() : ToolType::Selection;



  if (activeTool == ToolType::Pen) {

    if (impl_->isDraggingMaskHandle_ || impl_->isDraggingVertex_) {

      return Qt::ClosedHandCursor;

    }

    if (impl_->hoveredMaskHandleType_ !=

        static_cast<int>(MaskEditHandleType::None)) {
      const auto handleType = static_cast<MaskEditHandleType>(
          impl_->hoveredMaskHandleType_);
      if (handleType == MaskEditHandleType::FeatherHandle) {
        return Qt::SizeVerCursor;
      }
      return Qt::CrossCursor;

    }

    if (impl_->hoveredVertexIndex_ >= 0) {

      return Qt::OpenHandCursor;

    }

    // A segment click inserts an anchor. Keep this affordance distinct from
    // the arrow cursor used to begin a new path.
    if (impl_->renderer_) {
      const auto comp = impl_->previewPipeline_.composition();
      const auto layer = (comp && !impl_->selectedLayerId_.isNil())
          ? comp->layerById(impl_->selectedLayerId_)
          : ArtifactAbstractLayerPtr{};
      if (layer) {
        const auto canvas = impl_->renderer_->viewportToCanvas(
            {viewportPos.x * impl_->devicePixelRatio_,
             viewportPos.y * impl_->devicePixelRatio_});
        int maskIndex = -1;
        int pathIndex = -1;
        int segmentIndex = -1;
        if (hitTestMaskSegment(layer, QPointF(canvas.x, canvas.y),
                               12.0f / impl_->renderer_->getZoom(),
                               maskIndex, pathIndex, segmentIndex)) {
          return Qt::CrossCursor;
        }
      }
    }

    return Qt::ArrowCursor;

  }



  if (activeTool == ToolType::Fill) {
    return Qt::CrossCursor;
  }

  if (activeTool == ToolType::Rectangle || activeTool == ToolType::Ellipse) {

    return Qt::CrossCursor;

  }

  if (activeTool == ToolType::Brush || activeTool == ToolType::RotoBrush ||
      activeTool == ToolType::Eraser) {

    return Qt::BlankCursor;

  }



  if (activeTool == ToolType::MotionSketch) {

    return Qt::CrossCursor;

  }



  if (activeTool == ToolType::Puppet) {

    return Qt::PointingHandCursor;

  }



  if (activeTool == ToolType::TrackPoint) {

    if (impl_->renderer_ && impl_->trackerGizmo_) {

      const QPointF physPos =
          ArtifactCore::Coordinates::toQPointF(viewportPos) *
          impl_->devicePixelRatio_;

      return impl_->trackerGizmo_->cursorShapeForViewportPos(

          physPos, impl_->renderer_.get());

    }

    return Qt::CrossCursor;

  }



  if (!impl_->renderer_) {

    return Qt::ArrowCursor;

  }

  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  if (!impl_->gizmoModalTransformActive_ &&
      impl_->projectedFrameSizeBadgesVisible_ &&
      (impl_->projectedFrameWidthBadgeRect_.contains(physicalPos) ||
       impl_->projectedFrameHeightBadgeRect_.contains(physicalPos))) {
    return Qt::PointingHandCursor;
  }

  if (activeTool == ToolType::Selection &&
      QApplication::keyboardModifiers().testFlag(Qt::AltModifier)) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
      QSet<LayerID> selectedLayerIds;
      if (auto* selection = ArtifactApplicationManager::instance()
                                ? ArtifactApplicationManager::instance()->layerSelectionManager()
                                : nullptr) {
        for (const auto& layer : selection->selectedLayers()) {
          if (layer) {
            selectedLayerIds.insert(layer->id());
          }
        }
      }
      if (selectedLayerIds.isEmpty() && !impl_->selectedLayerId_.isNil()) {
        selectedLayerIds.insert(impl_->selectedLayerId_);
      }
      if (!selectedLayerIds.isEmpty()) {
        const auto canvasPos = impl_->renderer_->viewportToCanvas(
            {viewportPos.x * impl_->devicePixelRatio_,
             viewportPos.y * impl_->devicePixelRatio_});
        QString fieldId;
        TransformFieldDragMode mode = TransformFieldDragMode::None;
        if (hitTestTransformFieldHandle(
                comp, QPointF(canvasPos.x, canvasPos.y), selectedLayerIds,
                14.0f * Accessibility::targetScale() /
                    std::max(0.001f, impl_->renderer_->getZoom()),
                fieldId, mode)) {
          if (mode == TransformFieldDragMode::Center) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::Center
                       ? Qt::ClosedHandCursor
                       : Qt::SizeAllCursor;
          }
          if (mode == TransformFieldDragMode::Radius) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::Radius
                       ? Qt::ClosedHandCursor
                       : Qt::SizeHorCursor;
          }
          if (mode == TransformFieldDragMode::SecondaryRadius) {
            return impl_->isDraggingTransformField_ &&
                           impl_->draggingTransformFieldMode_ ==
                               TransformFieldDragMode::SecondaryRadius
                       ? Qt::ClosedHandCursor
                       : Qt::SizeVerCursor;
          }
        }
      }
    }
  }



  auto comp = impl_->previewPipeline_.composition();

  ArtifactAbstractLayerPtr selectedLayer;

  if (comp && !impl_->selectedLayerId_.isNil()) {

    selectedLayer = comp->layerById(impl_->selectedLayerId_);

  }



  // viewportPos is in logical pixels; convert to physical for gizmo hit testing

   const QPointF physPos =
       ArtifactCore::Coordinates::toQPointF(viewportPos) *
       impl_->devicePixelRatio_;

  bool selectedGroupUsesProjectedFrame =
      selectedLayer && layerUsesProjectedFrameGizmo(selectedLayer);
  if (!selectedGroupUsesProjectedFrame) {
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      for (const auto &candidate : selection->selectedLayers()) {
        if (candidate && layerUsesProjectedFrameGizmo(candidate)) {
          selectedGroupUsesProjectedFrame = true;
          break;
        }
      }
    }
  }
  if (selectedLayer &&
      (selectedGroupUsesProjectedFrame ||
       impl_->viewportOrientationMatricesValid_) &&
      impl_->gizmo3D_ &&
      impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Transform3D &&
      activeTool != ToolType::AnchorPoint &&
      (impl_->gizmoMode_ == TransformGizmo::Mode::All ||
       impl_->gizmoMode_ == TransformGizmo::Mode::Move ||
       impl_->gizmoMode_ == TransformGizmo::Mode::Scale ||
       impl_->gizmoMode_ == TransformGizmo::Mode::Rotate)) {

    const QMatrix4x4 &frameView = impl_->viewportOrientationMatricesValid_
                                      ? impl_->viewportOrientationViewForOverlay_
                                      : impl_->gizmo3DCameraMatricesValid_
                                            ? impl_->gizmo3DViewMatrix_
                                            : impl_->renderer_->getViewMatrix();
    const QMatrix4x4 &frameProjection =
        impl_->viewportOrientationMatricesValid_
            ? impl_->viewportOrientationProjectionForOverlay_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DProjectionMatrix_
                  : impl_->renderer_->getProjectionMatrix();
    const QRect frameViewport(
        0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
        std::max(1, static_cast<int>(impl_->hostHeight_)));
    QVector<ArtifactAbstractLayerPtr> projectedSelection;
    if (auto *selection = ArtifactLayerSelectionManager::instance()) {
      for (const auto &candidate : selection->selectedLayers()) {
        if (candidate && !candidate->isLocked() &&
            !candidate->isSelectionLocked()) {
          projectedSelection.push_back(candidate);
        }
      }
    }
    const bool combinedProjectedFrame = projectedSelection.size() > 1 &&
        (selectedGroupUsesProjectedFrame ||
         impl_->viewportOrientationMatricesValid_);
    const ScreenPhysicalBounds2 combinedFramePhysicalBounds =
        combinedProjectedFrame
        ? projectedSelectionFrameBounds(projectedSelection, frameView,
                                        frameProjection, frameViewport)
        : ScreenPhysicalBounds2{};
    const QRectF combinedFrameBounds =
        qRectFromScreenPhysicalBounds(combinedFramePhysicalBounds);
    auto frameHandle = combinedProjectedFrame
        ? hitTestProjectedSelectionFrame(
              combinedFramePhysicalBounds, screenPhysicalPoint(physPos),
              32.0f * std::max(1.0f, impl_->devicePixelRatio_))
        : hitTestProjectedFrameCorner(
              selectedLayer, screenPhysicalPoint(physPos), frameView,
              frameProjection,
              frameViewport,
              32.0f * std::max(1.0f, impl_->devicePixelRatio_));
    if (!projectedFrameHandleEnabled(impl_->gizmoMode_, frameHandle)) {
      frameHandle = TransformGizmo::HandleType::None;
    }
    // Match press priority: frame handles, axes, then frame interior.
    const GizmoAxis axis = impl_->gizmo3D_->hitTest(
        createPickingRay(screenPhysicalPoint(physPos)), frameView,
        frameProjection);
    if (frameHandle == TransformGizmo::HandleType::None &&
        axis != GizmoAxis::None) {
      return impl_->gizmoDragActive_ ? Qt::ClosedHandCursor
                                    : Qt::OpenHandCursor;
    }
    if (frameHandle != TransformGizmo::HandleType::None) {
      if (combinedProjectedFrame) {
        return cursorForAxisAlignedFrameHandle(frameHandle);
      }
      return cursorForProjectedFrameCorner(frameHandle, selectedLayer,
                                           frameView, frameProjection,
                                           frameViewport);
    }
    const bool frameInteriorHit = combinedProjectedFrame
        ? combinedFrameBounds.contains(physPos)
        : hitTestProjectedFrameInterior(
              selectedLayer, screenPhysicalPoint(physPos), frameView,
              frameProjection, frameViewport);
    if (frameInteriorHit) {
      return impl_->gizmoDragActive_ ? Qt::ClosedHandCursor
                                     : Qt::SizeAllCursor;
    }

  }

  if (impl_->contentGizmo_ &&
      impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Content) {

    return impl_->contentGizmo_->cursorShapeForViewportPos(physPos,

                                                          impl_->renderer_.get());

  }

  if (impl_->textGizmo_ &&
      impl_->gizmoOwnerForLayer(selectedLayer) == GizmoOwner::Text) {

    return impl_->textGizmo_->cursorShapeForViewportPos(physPos,

                                                        impl_->renderer_.get());

  }

  if (!impl_->gizmo_ || !legacy2DGizmoShouldOwnPress(
          this, impl_->gizmoOwnerForLayer(selectedLayer))) {

    return Qt::ArrowCursor;

  }

  return impl_->gizmo_->cursorShapeForViewportPos(physPos,

                                                  impl_->renderer_.get());

}

} // namespace Artifact
