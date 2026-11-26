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

bool CompositionRenderController::beginModalGizmoInteraction(
    TransformGizmo::Mode mode,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->renderer_ || !impl_->gizmo3D_ ||
      impl_->gizmoSession_.owner != GizmoOwner::None ||
      impl_->gizmo3D_->isDragging() || impl_->constructionDragLayer_) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  if (!selectedLayer || selectedLayer->isLocked() ||
      selectedLayer->isSelectionLocked() || layerUsesTextGizmo(selectedLayer)) {
    return false;
  }

  // Capture the same transform state as pointer-started gizmo drags. Modal
  // transforms must remain one undo operation and preserve group roots.
  impl_->gizmoUndoLayer_ = selectedLayer;
  impl_->gizmoUndoFrame_ = comp->framePosition().framePosition();
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
   impl_->gizmoLayerTransformBefore_.scale = {
      layerTransform.scaleXAt(layerTime), layerTransform.scaleYAt(layerTime),
      selectedLayer->is3D() ? layerTransform.snapshotAt(layerTime).scaleZ
                             : 1.0f};
  impl_->gizmoLayerTransformBefore_.is3D = selectedLayer->is3D();
  captureGizmoKeyState(selectedLayer, impl_->gizmoUndoFrame_,
                       impl_->gizmoLayerTransformBefore_);
  const QMatrix4x4 startWorld = selectedLayer->getGlobalTransform4x4();
  const QVector3D startOrigin = startWorld.map(QVector3D());
  impl_->projectedFrameStartAxisX_ =
      ArtifactCore::Coordinates::worldVectorFromQVector3D(
          startWorld.map(QVector3D(1.0f, 0.0f, 0.0f)) - startOrigin);
  impl_->projectedFrameStartAxisY_ =
      ArtifactCore::Coordinates::worldVectorFromQVector3D(
          startWorld.map(QVector3D(0.0f, 1.0f, 0.0f)) - startOrigin);
  const QVector3D selectedAnchor(
      layerTransform.anchorXAt(layerTime),
      layerTransform.anchorYAt(layerTime),
      layerTransform.anchorZAt(layerTime));
  impl_->projectedFrameStartWorldAnchor_ =
      ArtifactCore::Coordinates::worldPoint3FromQVector3D(
          startWorld.map(selectedAnchor));
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
        const auto candidateTime = gizmoTransformTime(candidate, state.frame);
        state.before.position = {
            candidateTransform.snapshotAt(candidateTime).positionX,
            candidateTransform.snapshotAt(candidateTime).positionY,
            candidateTransform.snapshotAt(candidateTime).positionZ};
        state.before.rotation = candidate->is3D()
            ? ArtifactCore::Units::eulerDegreesFromQVector3D(
                  candidate->rotation3D())
            : ArtifactCore::Units::EulerDegrees3{
                  {}, {}, {candidateTransform.rotationAt(candidateTime)}};
        state.before.scale = {
            candidateTransform.scaleXAt(candidateTime),
            candidateTransform.scaleYAt(candidateTime),
            candidate->is3D()
                ? candidateTransform.snapshotAt(candidateTime).scaleZ
                : 1.0f};
        captureGizmoKeyState(candidate, state.frame, state.before);
        const QVector3D anchor(
            candidateTransform.anchorXAt(candidateTime),
            candidateTransform.anchorYAt(candidateTime),
            candidateTransform.anchorZAt(candidateTime));
        state.worldAnchor =
            ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                candidate->getGlobalTransform4x4().map(anchor));
        if (const auto parent = candidate->parentLayer()) {
          state.parentWorldInverse = parent->getGlobalTransform4x4().inverted(
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

  setGizmoMode(mode);
  impl_->gizmo3D_->setDepthEnabled(selectedLayer->is3D());
  impl_->gizmoModalPreviousSpace_ = impl_->gizmo3D_->space();
  if (layerUsesProjectedFrameGizmo(selectedLayer) ||
      impl_->viewportOrientationMatricesValid_) {
    impl_->gizmo3D_->setSpace(GizmoSpace::Local);
    if (impl_->gizmoGroupTransformActive_) {
      const QMatrix4x4 &modalView = impl_->viewportOrientationMatricesValid_
          ? impl_->viewportOrientationViewForOverlay_
          : impl_->gizmo3DCameraMatricesValid_
                ? impl_->gizmo3DViewMatrix_
                : impl_->renderer_->getViewMatrix();
      bool invertible = false;
      const QMatrix4x4 inverseView = modalView.inverted(&invertible);
      if (invertible) {
        QVector3D basisX = inverseView.mapVector(QVector3D(1.0f, 0.0f, 0.0f));
        QVector3D basisY = inverseView.mapVector(QVector3D(0.0f, -1.0f, 0.0f));
        if (basisX.lengthSquared() > 0.000001f &&
            basisY.lengthSquared() > 0.000001f) {
          basisX.normalize();
          basisY.normalize();
          const QVector3D basisZ =
              QVector3D::crossProduct(basisX, basisY).normalized();
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
        }
      }
    }
  }
  const GizmoAxis startAxis = mode == TransformGizmo::Mode::Rotate
      ? GizmoAxis::Z
      : GizmoAxis::Screen;
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  const WorldRay modalRay = createPickingRay(screenPhysicalPoint(physicalPos));
  if (mode == TransformGizmo::Mode::Scale) {
    impl_->gizmo3D_->beginDrag(
        startAxis, modalRay,
        QVector3D(1.0f, 1.0f, 1.0f));
  } else {
    impl_->gizmo3D_->beginDrag(startAxis, modalRay);
  }
  impl_->gizmoModalTransformActive_ = true;
  impl_->beginGizmoSession(GizmoOwner::Transform3D, selectedLayer);
  impl_->gizmoDragActive_ = true;
  impl_->projectedFrameHandle_ = TransformGizmo::HandleType::None;
  impl_->projectedFrameMove_ = mode == TransformGizmo::Mode::Move;
  impl_->projectedFrameScalePointerBasisValid_ = false;
  impl_->projectedFrameLastPointer_ = screenPhysicalPoint(physicalPos);
  impl_->projectedFrameSnapCache_.invalidate();
  impl_->projectedFrameLastPointerValid_ = true;
  notifyViewportInteractionActivity();
  impl_->gizmoDragRenderTimer_.restart();
  impl_->invalidateOverlayComposite();
  setInfoOverlayText(
      QStringLiteral("Transform"),
      mode == TransformGizmo::Mode::Move
          ? QStringLiteral("Move — X/Y/Z constrain, Shift precise, Ctrl snap, Enter or click confirm")
          : mode == TransformGizmo::Mode::Rotate
                ? QStringLiteral("Rotate — X/Y/Z constrain, Shift precise, Ctrl snap, Enter or click confirm")
                : QStringLiteral("Scale — X/Y/Z constrain, Shift precise, Ctrl snap, Enter or click confirm"));
  markRenderDirty();
  return true;
}

int CompositionRenderController::beginFrameSizeBadgeInput(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || impl_->gizmoModalTransformActive_ ||
      !impl_->projectedFrameSizeBadgesVisible_) {
    return -1;
  }
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  int dimension = -1;
  if (impl_->projectedFrameWidthBadgeRect_.contains(physicalPos)) {
    dimension = 0;
  } else if (impl_->projectedFrameHeightBadgeRect_.contains(physicalPos)) {
    dimension = 1;
  }
  if (dimension < 0 ||
      !beginModalGizmoInteraction(TransformGizmo::Mode::Scale, viewportPos)) {
    return -1;
  }
  setInfoOverlayText(
      QStringLiteral("Frame Size"),
      QStringLiteral("Enter %1 in pixels, then Enter to confirm")
          .arg(dimension == 0 ? QStringLiteral("width")
                              : QStringLiteral("height")));
  return dimension;
}

bool CompositionRenderController::constrainModalGizmoInteraction(
    int axis, ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->gizmoModalTransformActive_ || !impl_->gizmo3D_ ||
      !impl_->gizmo3D_->isDragging()) {
    return false;
  }
  GizmoAxis target = GizmoAxis::None;
  const bool rotateMode = impl_->gizmo3D_->mode() == GizmoMode::Rotate;
  if (axis == 0 && (impl_->gizmo3D_->depthEnabled() || !rotateMode)) {
    target = GizmoAxis::X;
  }
  if (axis == 1 && (impl_->gizmo3D_->depthEnabled() || !rotateMode)) {
    target = GizmoAxis::Y;
  }
  if (axis == 2 && (impl_->gizmo3D_->depthEnabled() || rotateMode)) {
    target = GizmoAxis::Z;
  }
  if (target == GizmoAxis::None) return false;
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  impl_->gizmo3D_->constrainDrag(
      target, createPickingRay(screenPhysicalPoint(physicalPos)));
  handleMouseMove(viewportPos);
  setInfoOverlayText(
      QStringLiteral("Transform"),
      axis == 0 ? QStringLiteral("Constrained to X")
                : axis == 1 ? QStringLiteral("Constrained to Y")
                            : QStringLiteral("Constrained to Z"));
  return true;
}

bool CompositionRenderController::setModalGizmoNumericInput(
    ArtifactCore::Units::WorldLength value,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->gizmoModalTransformActive_ || !impl_->gizmo3D_ ||
      !impl_->gizmo3D_->isDragging() ||
      impl_->gizmo3D_->mode() != GizmoMode::Move ||
      !std::isfinite(value.value)) {
    return false;
  }
  impl_->gizmo3D_->setNumericInput(value);
  handleMouseMove(viewportPos);
  setInfoOverlayText(
      QStringLiteral("Transform Numeric"),
      QStringLiteral("%1").arg(QString::number(value.value, 'g', 7)));
  return true;
}

bool CompositionRenderController::setModalGizmoNumericInput(
    ArtifactCore::Units::Degrees value,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->gizmoModalTransformActive_ || !impl_->gizmo3D_ ||
      !impl_->gizmo3D_->isDragging() ||
      impl_->gizmo3D_->mode() != GizmoMode::Rotate ||
      !std::isfinite(value.value)) {
    return false;
  }
  impl_->gizmo3D_->setNumericInput(value);
  handleMouseMove(viewportPos);
  setInfoOverlayText(
      QStringLiteral("Transform Numeric"),
      QStringLiteral("%1").arg(QString::number(value.value, 'g', 7)));
  return true;
}

bool CompositionRenderController::setModalGizmoNumericInput(
    ArtifactCore::Units::ScaleFactor factor,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->gizmoModalTransformActive_ || !impl_->gizmo3D_ ||
      !impl_->gizmo3D_->isDragging() ||
      impl_->gizmo3D_->mode() != GizmoMode::Scale ||
      !std::isfinite(factor.value)) {
    return false;
  }
  impl_->gizmo3D_->setNumericInput(factor);
  handleMouseMove(viewportPos);
  setInfoOverlayText(
      QStringLiteral("Transform Numeric"),
      QStringLiteral("%1").arg(QString::number(factor.value, 'g', 7)));
  return true;
}

bool CompositionRenderController::setModalGizmoFrameDimension(
    int dimension, ArtifactCore::Units::Pixels pixels,
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->gizmoModalTransformActive_ || !impl_->gizmo3D_ ||
      impl_->gizmo3D_->mode() != GizmoMode::Scale ||
      !(pixels.value > 0.0f)) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  if (!layer) return false;
  const QRectF bounds = layer->localBounds();
  const float sourceExtent = dimension == 0
      ? static_cast<float>(bounds.width())
      : static_cast<float>(bounds.height());
  const float startScale = dimension == 0
      ? std::abs(impl_->gizmoUndoBefore_.scale.x)
      : std::abs(impl_->gizmoUndoBefore_.scale.y);
  const float startPixels = sourceExtent * startScale;
  if (!(startPixels > 0.0001f)) return false;
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  impl_->gizmo3D_->constrainDrag(
      GizmoAxis::Screen,
      createPickingRay(screenPhysicalPoint(physicalPos)));
  impl_->gizmo3D_->setNumericPlanarScaleInput(
      ArtifactCore::Units::ScaleFactor{pixels.value / startPixels});
  handleMouseMove(viewportPos);
  setInfoOverlayText(
      QStringLiteral("Frame Size"),
      QStringLiteral("%1 %2 px")
          .arg(dimension == 0 ? QStringLiteral("W") : QStringLiteral("H"))
          .arg(QString::number(pixels.value, 'f', 1)));
  return true;
}

void CompositionRenderController::clearModalGizmoNumericInput() {
  if (impl_ && impl_->gizmo3D_) {
    impl_->gizmo3D_->clearNumericInput();
  }
}

bool CompositionRenderController::commitModalGizmoInteraction() {
  if (!impl_ || !impl_->gizmoModalTransformActive_) return false;
  impl_->gizmoModalTransformActive_ = false;
  if (impl_->gizmo3D_) {
    impl_->gizmo3D_->setSpace(impl_->gizmoModalPreviousSpace_);
  }
  handleMouseRelease();
  return true;
}

bool CompositionRenderController::isModalGizmoInteractionActive() const {
  return impl_ && impl_->gizmoModalTransformActive_;
}

bool CompositionRenderController::isInteractionBusy() const {
  return impl_ && impl_->interactionBusy();
}

bool CompositionRenderController::cancelGizmoInteraction() {
  if (impl_->gizmoSessionEnding_) return false;
  Impl::GizmoSessionEndGuard endSession(*impl_);
  if (impl_->historicalScaleDragging_) {
    if (auto layer = impl_->historicalScaleLayer_.lock(); layer && impl_->historicalScaleChanged_) {
      applyMotionPathScale(layer, impl_->historicalScaleTime_,
                            impl_->historicalScaleBefore_);
    }
    impl_->historicalScaleDragging_ = false;
    impl_->historicalScaleLayer_.reset();
    impl_->historicalScaleChanged_ = false;
    impl_->motionPathCache_.valid = false;
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    setInfoOverlayText(QString(), QString());
    finishViewportInteraction();
    markRenderDirty();
    return true;
  }
  if (const auto layer = impl_->constructionDragLayer_) {
    layer->setConstructionItem(impl_->constructionDragHandle_.index, impl_->constructionDragBefore_);
    impl_->constructionDragLayer_.reset();
    impl_->invalidateLayerSurfaceCache(layer);
    impl_->invalidateBaseComposite(); impl_->invalidateOverlayComposite(); markRenderDirty();
    return true;
  }
  if (impl_->gizmoSession_.owner == GizmoOwner::Content) {
    if (impl_->contentGizmo_->cancelInteraction()) {
      impl_->invalidateBaseComposite();
      impl_->invalidateOverlayComposite();
      finishViewportInteraction();
      markRenderDirty();
      return true;
    }
  }
  if (impl_->gizmoSession_.owner == GizmoOwner::Text) {
    if (impl_->textGizmo_->cancelInteraction()) {
      impl_->invalidateBaseComposite();
      impl_->invalidateOverlayComposite();
      finishViewportInteraction();
      markRenderDirty();
      return true;
    }
  }
  if (impl_->isDraggingLineEndpoint_) {
    if (auto layer = impl_->draggingLineLayer_.lock()) if (auto *line = dynamic_cast<ArtifactShapeLayer *>(layer.get())) { const auto time = gizmoTransformTime(layer, layer->currentFrame()); line->setSize(impl_->draggingLineBeforeWidth_, impl_->draggingLineBeforeHeight_); line->transform3D().setPosition(time, static_cast<float>(impl_->draggingLineBeforePosition_.x()), static_cast<float>(impl_->draggingLineBeforePosition_.y())); line->transform3D().setRotation(time, impl_->draggingLineBeforeRotation_); line->changed(); }
    impl_->isDraggingLineEndpoint_ = false; impl_->draggingLineEndpoint_ = -1; impl_->draggingLineLayer_.reset(); impl_->invalidateBaseComposite(); impl_->invalidateOverlayComposite(); finishViewportInteraction(); markRenderDirty(); return true;
  }

  // Cancel an in-progress POI drag, restoring the pre-drag value.
  if (impl_->isDraggingCameraPoi_) {
    if (auto layer = impl_->draggingCameraPoiLayer_.lock()) {
      if (auto *camera =
              dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
        camera->setPointOfInterest(impl_->cameraPoiBefore_);
      }
    }
    impl_->clearCameraPoiDrag();
    impl_->invalidateOverlayComposite();
    finishViewportInteraction();
    markRenderDirty();
    return true;
  }

  if (impl_->gizmoSession_.owner == GizmoOwner::Transform2D &&
      impl_->gizmo_->cancelInteraction()) {
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    finishViewportInteraction();
    markRenderDirty();
    return true;
  }
  if (impl_->gizmoSession_.owner != GizmoOwner::Transform3D ||
      !impl_->gizmo3D_ || !impl_->gizmo3D_->isDragging()) {
    return false;
  }

  const auto restoreSnapshot = [](const ArtifactAbstractLayerPtr &layer,
                                  int64_t frame,
                                  const GizmoTransformSnapshot &snapshot) {
    if (!layer) {
      return;
    }
    if (snapshot.is3D) {
      auto &transform = layer->transform3D();
      const auto time = gizmoTransformTime(layer, frame);
      if (snapshot.hasPositionKey) {
        const float initialX = transform.snapshotAt(time).positionX - transform.positionXAt(time);
        const float initialY = transform.snapshotAt(time).positionY - transform.positionYAt(time);
        transform.setPosition(time, snapshot.position.x - initialX,
                              snapshot.position.y - initialY);
      } else {
        transform.removePositionKeyFrameAt(time);
        if (!snapshot.positionAnimated) {
          transform.setInitialPosition(time, snapshot.position.x,
                                       snapshot.position.y);
        }
      }
      transform.setCurrentPositionZ(snapshot.position.z);
      if (snapshot.hasRotationKey) {
        transform.setRotationX(time, snapshot.rotation.x.value);
        transform.setRotationY(time, snapshot.rotation.y.value);
        transform.setRotationZ(time, snapshot.rotation.z.value);
      } else {
        transform.removeRotationKeyFrameAt(time);
        if (!snapshot.rotationAnimated) {
          transform.setCurrentRotationX(snapshot.rotation.x.value);
          transform.setCurrentRotationY(snapshot.rotation.y.value);
          transform.setInitialRotation(time, snapshot.rotation.z.value);
        }
      }
      if (snapshot.hasScaleKey) {
        transform.setScale(time, snapshot.scale.x, snapshot.scale.y);
      } else {
        transform.removeScaleKeyFrameAt(time);
        if (!snapshot.scaleAnimated) {
          transform.setInitialScale(time, snapshot.scale.x,
                                    snapshot.scale.y);
        }
      }
      if (std::abs(transform.snapshotAt(time).scaleZ - snapshot.scale.z) >
          0.000001f) {
        transform.setScale(time, snapshot.scale.x, snapshot.scale.y,
                           snapshot.scale.z);
      }
    } else {
      applyPlanarGizmoTransform(layer, frame, snapshot);
    }
    restoreGizmoPropertyKeyState(layer, frame, snapshot);
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto *composition =
            static_cast<ArtifactAbstractComposition *>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(),
                            layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  };

  if (impl_->gizmoGroupTransformActive_) {
    for (const auto& state : impl_->gizmoGroupLayers_) {
      restoreSnapshot(state.layer, state.frame, state.before);
    }
  } else if (impl_->gizmoUndoSnapshotValid_) {
    restoreSnapshot(impl_->gizmoUndoLayer_.lock(), impl_->gizmoUndoFrame_,
                    impl_->gizmoLayerTransformBefore_);
  }

  // Cancelling rolls the layer back to the pre-drag snapshot, so any pending
  // anchor result is dropped rather than committed.
  impl_->gizmoAnchorDragHasResult_ = false;
  impl_->gizmoAnchorDragStartLayer_.reset();

  impl_->gizmo3D_->endDrag();
  impl_->gizmo3D_->setInteractionModifiers(false, false);
  if (impl_->gizmoModalTransformActive_) {
    impl_->gizmo3D_->setSpace(impl_->gizmoModalPreviousSpace_);
    impl_->gizmoModalTransformActive_ = false;
  }
  if (impl_->projectedFrameForcedLocal_) {
    impl_->gizmo3D_->setSpace(impl_->projectedFramePreviousSpace_);
  }
  if (impl_->projectedFrameForcedMode_) {
    impl_->gizmo3D_->setMode(impl_->projectedFramePreviousMode_);
  }
  impl_->projectedFrameForcedLocal_ = false;
  impl_->projectedFrameForcedMode_ = false;
  impl_->projectedFrameHandle_ = TransformGizmo::HandleType::None;
  impl_->projectedFrameMove_ = false;
  impl_->projectedFrameCorrectedLocalPositionValid_ = false;
  impl_->projectedFrameLastPointerValid_ = false;
  impl_->projectedFrameSnapCache_.invalidate();
  impl_->projectedFrameScalePointerBasisValid_ = false;
  impl_->projectedFrameSnapVerticalValid_ = false;
  impl_->projectedFrameSnapHorizontalValid_ = false;
  impl_->gizmoDragActive_ = false;
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
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  finishViewportInteraction();
  markRenderDirty();
  return true;
}

void CompositionRenderController::Impl::beginMotionPathDrag(const ArtifactAbstractLayerPtr &layer,

                         int64_t frame,
                         ArtifactCore::Coordinates::CompositionPoint2 canvasPos,
                         const MotionPathPositionSnapshot &before,
                         Qt::KeyboardModifiers modifiers) {

  draggingMotionPathLayer_ = layer;

  draggingMotionPathFrame_ = frame;

  draggingMotionPathBefore_ = before;
  draggingMotionPathStartLocalPos_ =
      ArtifactCore::Coordinates::layerParentPoint2FromComposition(canvasPos);
  if (const auto parent = layer->parentLayer()) {
    bool invertible = false;
    const QTransform inverseParent =
        parent->getGlobalTransformAt(frame).inverted(&invertible);
    if (invertible) {
      draggingMotionPathStartLocalPos_ =
          ArtifactCore::Coordinates::layerParentPoint2FromQPointF(
              inverseParent.map(
                  ArtifactCore::Coordinates::toQPointF(canvasPos)));
    }
  }
  draggingMotionPathModifiers_ = modifiers;
  draggingMotionPathGroupTransform_ =
      modifiers.testFlag(Qt::ControlModifier) &&
              modifiers.testFlag(Qt::ShiftModifier)
          ? MotionPathGroupTransform::Scale
      : modifiers.testFlag(Qt::AltModifier) &&
              modifiers.testFlag(Qt::ShiftModifier)
          ? MotionPathGroupTransform::Rotate
          : MotionPathGroupTransform::Translate;
  draggingMotionPathGroupBefore_.clear();
  draggingMotionPathGroupPivot_ = {};
  const auto &transform = layer->transform3D();
  const auto &frames = selectedMotionPathFrames_.isEmpty()
                           ? QVector<int64_t>{frame}
                           : selectedMotionPathFrames_;
  for (const auto selectedFrame : frames) {
    const auto time = gizmoTransformTime(layer, selectedFrame);
    MotionPathKeySnapshot snapshot;
    snapshot.frame = selectedFrame;
    snapshot.value.hasPositionKey = transform.hasPositionKeyFrameAt(time);
    snapshot.value.position.x = transform.positionXAt(time);
    snapshot.value.position.y = transform.positionYAt(time);
    if (snapshot.value.hasPositionKey) {
      snapshot.hasTangents =
          transform.positionKeyFrameSpatialTangentsAt(time,
                                                      snapshot.tangents);
      draggingMotionPathGroupBefore_.push_back(snapshot);
    }
  }
  for (const auto &snapshot : draggingMotionPathGroupBefore_) {
    draggingMotionPathGroupPivot_.x += snapshot.value.position.x;
    draggingMotionPathGroupPivot_.y += snapshot.value.position.y;
  }
  if (!draggingMotionPathGroupBefore_.isEmpty()) {
    const double divisor = static_cast<double>(
        draggingMotionPathGroupBefore_.size());
    draggingMotionPathGroupPivot_.x /= divisor;
    draggingMotionPathGroupPivot_.y /= divisor;
  }
  const QPointF startVector =
      ArtifactCore::Coordinates::toQPointF(
          draggingMotionPathStartLocalPos_) -
      ArtifactCore::Coordinates::toQPointF(
          draggingMotionPathGroupPivot_);
  draggingMotionPathGroupStartAngle_ =
      std::atan2(static_cast<float>(startVector.y()),
                 static_cast<float>(startVector.x()));
  draggingMotionPathGroupStartRadius_ =
      std::max(1.0f, static_cast<float>(std::hypot(startVector.x(),
                                                   startVector.y())));

  isDraggingMotionPathKeyframe_ = true;

}

void CompositionRenderController::Impl::clearMotionPathDragState() {

  isDraggingMotionPathKeyframe_ = false;

  draggingMotionPathLayer_.reset();

  draggingMotionPathFrame_ = 0;

  draggingMotionPathBefore_ = {};
  draggingMotionPathGroupBefore_.clear();
  draggingMotionPathModifiers_ = Qt::NoModifier;
  draggingMotionPathGroupTransform_ = MotionPathGroupTransform::Translate;
  draggingMotionPathGroupPivot_ = {};

  draggingMotionPathStartLocalPos_ = {};

  hoveredMotionPathFrame_ = -1;
  hoveredMotionPathTangentHandle_ = MotionPathTangentHandle::None;

}

bool CompositionRenderController::Impl::setHoveredMotionPathFrame(int frame) {

  if (hoveredMotionPathFrame_ == frame) {

    return false;

  }

  hoveredMotionPathFrame_ = frame;

  invalidateOverlayComposite();

  return true;

}

bool CompositionRenderController::Impl::hitTestMotionPathTangent(const QPointF &canvasPos, float threshold,
                              int64_t &outFrame,
                              MotionPathTangentHandle &outHandle) const {
  const float thresholdSq = threshold * threshold;
  float bestDistanceSq = thresholdSq;
  bool found = false;
  for (const auto &point : motionPathCache_.keyPoints) {
    if (!point.hasSpatialTangents) continue;
    const auto testHandle = [&](float x, float y,
                                MotionPathTangentHandle handle) {
      const QPointF delta = QPointF(x, y) - canvasPos;
      const float distanceSq =
          static_cast<float>(QPointF::dotProduct(delta, delta));
      if (distanceSq <= bestDistanceSq) {
        bestDistanceSq = distanceSq;
        outFrame = point.frame;
        outHandle = handle;
        found = true;
      }
    };
    testHandle(point.inTangentX, point.inTangentY,
               MotionPathTangentHandle::In);
    testHandle(point.outTangentX, point.outTangentY,
               MotionPathTangentHandle::Out);
  }
  return found;
}

void CompositionRenderController::Impl::beginMotionPathTangentDrag(const ArtifactAbstractLayerPtr &layer,
                                int64_t frame,
                                MotionPathTangentHandle handle,
                                Qt::KeyboardModifiers modifiers) {
  draggingMotionPathTangentLayer_ = layer;
  draggingMotionPathTangentFrame_ = frame;
  draggingMotionPathTangentHandle_ = handle;
  draggingMotionPathTangentBroken_ = modifiers.testFlag(Qt::AltModifier);
  draggingMotionPathTangentBefore_ = {};
  draggingMotionPathTangentBroken_ = false;
  if (layer) {
    const auto time = gizmoTransformTime(layer, frame);
    draggingMotionPathTangentBefore_.present =
        layer->transform3D().positionKeyFrameSpatialTangentsAt(
            time, draggingMotionPathTangentBefore_.tangents);
  }
  isDraggingMotionPathTangent_ = true;
}

void CompositionRenderController::Impl::clearMotionPathTangentDragState() {
  isDraggingMotionPathTangent_ = false;
  draggingMotionPathTangentLayer_.reset();
  draggingMotionPathTangentFrame_ = 0;
  draggingMotionPathTangentHandle_ = MotionPathTangentHandle::None;
  draggingMotionPathTangentBefore_ = {};
}

bool CompositionRenderController::Impl::applyMotionPathTangentDrag(const QPointF &canvasPos) {
  auto layer = draggingMotionPathTangentLayer_.lock();
  if (!layer || !isDraggingMotionPathTangent_ ||
      draggingMotionPathTangentHandle_ == MotionPathTangentHandle::None) {
    return false;
  }

  QPointF localHandlePos = canvasPos;
  if (const auto parent = layer->parentLayer()) {
    const QTransform parentGlobal =
        parent->getGlobalTransformAt(draggingMotionPathTangentFrame_);
    bool invertible = false;
    const QTransform inverseParent = parentGlobal.inverted(&invertible);
    if (!invertible) return false;
    localHandlePos = inverseParent.map(canvasPos);
  }

  auto &t3d = layer->transform3D();
  const auto time = gizmoTransformTime(layer, draggingMotionPathTangentFrame_);
  const QPointF keyPosition(t3d.positionXAt(time), t3d.positionYAt(time));
  const QPointF delta = localHandlePos - keyPosition;
  ArtifactCore::PositionSpatialTangents tangents;
  if (!t3d.positionKeyFrameSpatialTangentsAt(time, tangents)) {
    tangents.linked = true;
  }
  if (draggingMotionPathTangentBroken_) {
    tangents.linked = false;
  }
  if (draggingMotionPathTangentHandle_ == MotionPathTangentHandle::In) {
    tangents.inTangent = {static_cast<float>(delta.x()),
                          static_cast<float>(delta.y())};
    if (tangents.linked) {
      tangents.outTangent = {-tangents.inTangent.x, -tangents.inTangent.y};
    }
  } else {
    tangents.outTangent = {static_cast<float>(delta.x()),
                           static_cast<float>(delta.y())};
    if (tangents.linked) {
      tangents.inTangent = {-tangents.outTangent.x, -tangents.outTangent.y};
    }
  }
  if (!t3d.setPositionKeyFrameSpatialTangentsAt(time, tangents)) return false;
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto *comp = static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  motionPathCache_.valid = false;
  invalidateOverlayComposite();
  return true;
}

void CompositionRenderController::Impl::beginPastPlaneFrameDrag(const ArtifactAbstractLayerPtr &layer,
                             int64_t frame, const QVector3D &startHit,
                             const MotionPathPositionSnapshot &before) {
  draggingPastPlaneLayer_ = layer;
  draggingPastPlaneFrame_ = frame;
  draggingPastPlaneStartHit_ = startHit;
  draggingPastPlaneBefore_ = before;
  isDraggingPastPlaneFrame_ = true;
}

void CompositionRenderController::Impl::clearPastPlaneFrameDrag() {
  isDraggingPastPlaneFrame_ = false;
  draggingPastPlaneLayer_.reset();
  draggingPastPlaneFrame_ = 0;
  draggingPastPlaneBefore_ = {};
  draggingPastPlaneStartHit_ = {};
}

void CompositionRenderController::Impl::beginCameraPoiDrag(const ArtifactAbstractLayerPtr &layer,
                        ArtifactCore::Coordinates::LayerParentPoint3 before,
                        ArtifactCore::Coordinates::WorldPoint3 planePoint,
                        ArtifactCore::Coordinates::WorldVector3 planeNormal) {
  draggingCameraPoiLayer_ = layer;
  cameraPoiBefore_ = before;
  cameraPoiDragPlanePoint_ = planePoint;
  cameraPoiDragPlaneNormal_ = planeNormal;
  isDraggingCameraPoi_ = true;
}

void CompositionRenderController::Impl::clearCameraPoiDrag() {
  isDraggingCameraPoi_ = false;
  draggingCameraPoiLayer_.reset();
  cameraPoiBefore_ = {};
  cameraPoiDragPlanePoint_ = {};
  cameraPoiDragPlaneNormal_ = {};
}

bool CompositionRenderController::Impl::applyCameraPoiDrag(
    const WorldRay &ray,
    ArtifactCore::Coordinates::WorldPoint3 &outPoi) const {
  auto layer = draggingCameraPoiLayer_.lock();
  if (!isDraggingCameraPoi_ || !layer) return false;
  const QVector3D rayDirection =
      ArtifactCore::Coordinates::toQVector3D(ray.direction);
  const QVector3D planeNormal =
      ArtifactCore::Coordinates::toQVector3D(cameraPoiDragPlaneNormal_);
  const auto planeToRay = cameraPoiDragPlanePoint_ - ray.origin;
  const float denominator =
      QVector3D::dotProduct(rayDirection, planeNormal);
  if (std::abs(denominator) <= 0.000001f) return false;
  const ArtifactCore::Units::WorldLength distance{
      QVector3D::dotProduct(
          ArtifactCore::Coordinates::toQVector3D(planeToRay), planeNormal) /
      denominator};
  if (distance.value <= 0.0f) return false;
  outPoi = ray.origin + ray.direction * distance.value;
  return true;
}

bool CompositionRenderController::Impl::applyPastPlaneFrameDrag(const Ray &ray) {
  auto layer = draggingPastPlaneLayer_.lock();
  auto *plane = layer ? dynamic_cast<Artifact3DLayer *>(layer.get()) : nullptr;
  if (!isDraggingPastPlaneFrame_ || !plane) return false;
  QVector3D hit;
  if (!intersectPickingRayFixedPlaneAt(*plane, draggingPastPlaneFrame_, ray,
                                       hit)) return false;
  const QVector3D delta = hit - draggingPastPlaneStartHit_;
  const auto time = gizmoTransformTime(layer, draggingPastPlaneFrame_);
  auto &transform = layer->transform3D();
  transform.setPositionKeyFrameValueAt(
      time, draggingPastPlaneBefore_.position.x + delta.x(),
      draggingPastPlaneBefore_.position.y + delta.y());
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  publishLayerModified(layer, true);
  motionPathCache_.valid = false;
  invalidateOverlayComposite();
  return true;
}

bool CompositionRenderController::Impl::applyMotionPathDrag(
    const ArtifactAbstractLayerPtr &layer,
    ArtifactCore::Coordinates::CompositionPoint2 canvasPos) {

  auto draggingLayer = draggingMotionPathLayer_.lock();

  if (!layer || !renderer_ || !isDraggingMotionPathKeyframe_ ||

      !draggingLayer || draggingLayer->id() != layer->id()) {

    return false;

  }



  ArtifactCore::Coordinates::LayerParentPoint2 localParentPos =
      ArtifactCore::Coordinates::layerParentPoint2FromComposition(canvasPos);

  if (const auto parent = layer->parentLayer()) {

    const QTransform parentGlobal =

        parent->getGlobalTransformAt(draggingMotionPathFrame_);

    bool invertible = false;

    const QTransform invParent = parentGlobal.inverted(&invertible);

    if (!invertible) {

      return false;

    }

    localParentPos =
        ArtifactCore::Coordinates::layerParentPoint2FromQPointF(
            invParent.map(
                ArtifactCore::Coordinates::toQPointF(canvasPos)));

  }



  auto &t3d = layer->transform3D();

  const auto time = gizmoTransformTime(layer, draggingMotionPathFrame_);

  const ArtifactCore::Coordinates::LayerParentVector2 typedDelta =
      localParentPos - draggingMotionPathStartLocalPos_;
  const QPointF localPos =
      ArtifactCore::Coordinates::toQPointF(localParentPos);
  const QPointF delta =
      ArtifactCore::Coordinates::toQPointF(typedDelta);
  const QPointF groupPivot = ArtifactCore::Coordinates::toQPointF(
      draggingMotionPathGroupPivot_);
  if (draggingMotionPathGroupBefore_.size() > 1) {
    const QPointF currentVector = localPos - groupPivot;
    const float currentAngle = std::atan2(
        static_cast<float>(currentVector.y()),
        static_cast<float>(currentVector.x()));
    const float angleDelta = currentAngle -
                             draggingMotionPathGroupStartAngle_;
    const float scale = std::clamp(
        static_cast<float>(std::hypot(currentVector.x(), currentVector.y())) /
            draggingMotionPathGroupStartRadius_,
        0.05f, 20.0f);
    for (const auto &snapshot : draggingMotionPathGroupBefore_) {
      const auto keyTime = gizmoTransformTime(layer, snapshot.frame);
      QPointF point = ArtifactCore::Coordinates::toQPointF(
          snapshot.value.position);
      if (draggingMotionPathGroupTransform_ ==
          MotionPathGroupTransform::Translate) {
        point += delta;
      } else {
        point -= groupPivot;
        if (draggingMotionPathGroupTransform_ ==
            MotionPathGroupTransform::Scale) {
          point *= scale;
        } else {
          const float c = std::cos(angleDelta);
          const float s = std::sin(angleDelta);
          point = QPointF(point.x() * c - point.y() * s,
                          point.x() * s + point.y() * c);
        }
        point += groupPivot;
      }
      t3d.setPositionKeyFrameValueAt(keyTime, static_cast<float>(point.x()),
                                     static_cast<float>(point.y()));
      if (snapshot.hasTangents) {
        auto tangents = snapshot.tangents;
        if (draggingMotionPathGroupTransform_ ==
            MotionPathGroupTransform::Scale) {
          tangents.inTangent.x *= scale;
          tangents.inTangent.y *= scale;
          tangents.outTangent.x *= scale;
          tangents.outTangent.y *= scale;
        } else if (draggingMotionPathGroupTransform_ ==
                   MotionPathGroupTransform::Rotate) {
          const float c = std::cos(angleDelta);
          const float s = std::sin(angleDelta);
          const auto inX = snapshot.tangents.inTangent.x;
          const auto inY = snapshot.tangents.inTangent.y;
          const auto outX = snapshot.tangents.outTangent.x;
          const auto outY = snapshot.tangents.outTangent.y;
          tangents.inTangent.x = inX * c - inY * s;
          tangents.inTangent.y = inX * s + inY * c;
          tangents.outTangent.x = outX * c - outY * s;
          tangents.outTangent.y = outX * s + outY * c;
        }
        t3d.setPositionKeyFrameSpatialTangentsAt(keyTime, tangents);
      }
    }
  } else {
    t3d.setPositionKeyFrameValueAt(time, static_cast<float>(localPos.x()),
                                   static_cast<float>(localPos.y()));
  }

  layer->setDirty(LayerDirtyFlag::Transform);

  layer->changed();

  if (auto *comp =

          static_cast<ArtifactAbstractComposition *>(layer->composition())) {

    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

        LayerChangedEvent{comp->id().toString(), layer->id().toString(),

                          LayerChangedEvent::ChangeType::Modified});

  }

  invalidateOverlayComposite();

  return true;

}

void CompositionRenderController::Impl::sync2DGizmosForLayer(const ArtifactAbstractLayerPtr &layer) {
  // Redraws must not rebind the owner of an active gesture.
  if (gizmoSession_.owner != GizmoOwner::None) return;

  std::vector<ArtifactAbstractLayerPtr> selectedTargets;

  if (auto *app = ArtifactApplicationManager::instance()) {

    if (auto *selection = app->layerSelectionManager()) {

      for (const auto &candidate : selection->selectedLayers()) {

        if (candidate && !candidate->isLocked() &&
            !candidate->isSelectionLocked()) {

          selectedTargets.push_back(candidate);

        }

      }

    }

  }

  if (selectedTargets.empty() && layer) {

    selectedTargets.push_back(layer);

  }

  const auto owner = gizmoOwnerForLayer(layer);
  const bool useTextGizmo = owner == GizmoOwner::Text;
  const bool useContentGizmo = owner == GizmoOwner::Content;
  // A fixed 3D Plane is manipulated by Artifact3DGizmo even in the
  // default composition view.  Do not leave the legacy 2D gizmo bound as a
  // second owner for the same selection.
  const bool use3DGizmoOnly = layer && layer->is3D() &&
      layerUsesProjectedFrameGizmo(layer);

  if (textGizmo_) {

    textGizmo_->setLayer(useTextGizmo ? layer : nullptr);

  }

  if (contentGizmo_) {

    contentGizmo_->setLayer(useContentGizmo ? layer : nullptr);

  }

  if (gizmo_) {

    if (useTextGizmo || useContentGizmo || use3DGizmoOnly) {

      gizmo_->setLayer(nullptr);

    } else if (selectedTargets.size() > 1) {

      gizmo_->setTargetLayers(std::move(selectedTargets));

    } else {

      gizmo_->setLayer(layer);

    }

    // A 2D image/plane/solid layer is never drawn by this gizmo: the 3D
    // manipulator and the projected frame own the visible handles.  Without
    // this, the undrawn 2D gizmo still hit-tests its rotate ring and scale
    // handles, so a press just outside the projected frame would start an
    // invisible resize/rotate and commit it through a different transform
    // and undo path than the drawn one.  Move stays live because the Design
    // workspace's sibling-reorder gesture is driven from the Move handle.
    gizmo_->setInvisibleHandlesSuppressed(
        !useTextGizmo && layerUsesProjectedFrameGizmo(layer));

  }

}

void CompositionRenderController::Impl::syncGizmo3DFromLayer(const ArtifactAbstractLayerPtr &layer) {

  if (!gizmo3D_ || !layer) {

    return;

  }

  // The drag owns its initial basis and scale until release. Resyncing a
  // single layer here also overwrites the in-flight resize calculation.
  if (gizmo3D_->isDragging()) {
    return;
  }

  QVector<ArtifactAbstractLayerPtr> groupLayers;
  if (auto *selection = ArtifactLayerSelectionManager::instance()) {
    const auto selected = selection->selectedLayers();
    if (selected.size() > 1 && selection->isSelected(layer)) {
      groupLayers.reserve(selected.size());
      for (const auto &candidate : selected) {
        if (candidate && !candidate->isLocked() &&
            !candidate->isSelectionLocked()) {
          groupLayers.push_back(candidate);
        }
      }
    }
  }

  if (groupLayers.size() > 1) {
    QRectF groupBounds;
    bool hasBounds = false;
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    bool any3D = false;
    for (const auto &candidate : groupLayers) {
      const QRectF bounds = candidate->transformedBoundingBox().normalized();
      if (bounds.isValid() && bounds.width() >= 0.0 &&
          bounds.height() >= 0.0) {
        groupBounds = hasBounds ? groupBounds.united(bounds) : bounds;
        hasBounds = true;
      }
      const QVector3D candidateWorldOrigin =
          candidate->getGlobalTransform4x4().map(QVector3D());
      const float z = candidateWorldOrigin.z();
      minZ = std::min(minZ, z);
      maxZ = std::max(maxZ, z);
      any3D = any3D || candidate->is3D();
    }
    if (hasBounds) {
      const QPointF center = groupBounds.center();
      const float centerZ = minZ <= maxZ ? (minZ + maxZ) * 0.5f : 0.0f;
      gizmo3D_->setDepthEnabled(any3D);
      gizmo3D_->setLocalBasis({1.0f, 0.0f, 0.0f},
                              {0.0f, -1.0f, 0.0f},
                              {0.0f, 0.0f, 1.0f});
    gizmo3D_->setTransform(
        ArtifactCore::Coordinates::worldPoint3FromQVector3D(
            QVector3D(static_cast<float>(center.x()),
                      static_cast<float>(center.y()), centerZ)),
          ArtifactCore::Units::eulerDegreesFromQVector3D(
              QVector3D(0.0f, 0.0f, 0.0f)));
      gizmo3D_->setScale({1.0f, 1.0f, 1.0f});
      gizmo3D_->clearBoundingBox();
      return;
    }
  }

  const QMatrix4x4 worldTransform = layer->getGlobalTransform4x4();
  const QVector3D worldOrigin =
      worldTransform.map(QVector3D(0.0f, 0.0f, 0.0f));
  gizmo3D_->setLocalBasis(
      ArtifactCore::Coordinates::worldVectorFromQVector3D(
          worldTransform.map(QVector3D(1.0f, 0.0f, 0.0f)) - worldOrigin),
      ArtifactCore::Coordinates::worldVectorFromQVector3D(
          worldTransform.map(QVector3D(0.0f, -1.0f, 0.0f)) - worldOrigin),
      ArtifactCore::Coordinates::worldVectorFromQVector3D(
          worldTransform.map(QVector3D(0.0f, 0.0f, 1.0f)) - worldOrigin));



  if (layer->is3D()) {

    const auto &t3 = layer->transform3D();

    // AnchorPoint re-points the gizmo at the anchor's world position so the
    // single handle lands on the anchor rather than on the transform origin.
    if (gizmoMode_ == TransformGizmo::Mode::AnchorPoint) {
      const auto anchorTime = gizmoTransformTime(layer, layer->currentFrame());
      const QPointF worldAnchor = layer->getGlobalTransform().map(
          QPointF(t3.anchorXAt(anchorTime), t3.anchorYAt(anchorTime)));
      gizmo3D_->setDepthEnabled(false);
      gizmo3D_->setTransform(
          ArtifactCore::Coordinates::worldPoint3FromQVector3D(
              QVector3D(static_cast<float>(worldAnchor.x()),
                        static_cast<float>(worldAnchor.y()), 0.0f)),
          ArtifactCore::Units::eulerDegreesFromQVector3D(QVector3D()));
      gizmo3D_->setScale({1.0f, 1.0f, 1.0f});
      gizmo3D_->clearBoundingBox();
      return;
    }

    gizmo3D_->setDepthEnabled(true);

    gizmo3D_->setTransform(
        ArtifactCore::Coordinates::worldPoint3FromQVector3D(
            worldTransform.map(QVector3D(0.0f, 0.0f, 0.0f))),
        ArtifactCore::Units::eulerDegreesFromQVector3D(layer->rotation3D()));

    gizmo3D_->setScale({t3.scaleX(), t3.scaleY(), t3.scaleZ()});

    const auto *modelLayer = dynamic_cast<const Artifact3DLayer *>(layer.get());
    if (modelLayer &&
        modelLayer->fixedGeometry() != FixedGeometry3D::Plane) {
      const auto &mesh = modelLayer->mesh();
      QVector3D minBounds = mesh.boundingBoxMin();
      QVector3D maxBounds = mesh.boundingBoxMax();
      const auto finite = [](float value) { return std::isfinite(value); };
      const bool meshBoundsValid = finite(minBounds.x()) &&
          finite(minBounds.y()) && finite(minBounds.z()) &&
          finite(maxBounds.x()) && finite(maxBounds.y()) &&
          finite(maxBounds.z()) && maxBounds.x() > minBounds.x() &&
          maxBounds.y() > minBounds.y() && maxBounds.z() > minBounds.z();
      if (!meshBoundsValid) {
        // Procedural volumes may not populate Mesh::boundingBox* until
        // their first render.  Fall back to the layer extent so selection
        // still immediately receives a usable enclosing box.
        const QRectF localBounds = layer->localBounds().normalized();
        const float width = std::max(
            1.0f, static_cast<float>(localBounds.width()));
        const float height = std::max(
            1.0f, static_cast<float>(localBounds.height()));
        const float halfDepth = std::max(1.0f, std::min(width, height) * 0.1f);
        minBounds = QVector3D(static_cast<float>(localBounds.left()),
                              static_cast<float>(localBounds.top()),
                              -halfDepth);
        maxBounds = QVector3D(static_cast<float>(localBounds.left()) + width,
                              static_cast<float>(localBounds.top()) + height,
                              halfDepth);
      }
      gizmo3D_->setBoundingBox(
          ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(minBounds),
          ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(maxBounds));
    } else {
      // A plane uses the camera-projected 2D frame; do not give it an
      // artificial thickness merely to satisfy the box gizmo.
      gizmo3D_->clearBoundingBox();
    }

    return;

  }



  const QRectF localRect = layer->localBounds();

  const QTransform globalTransform = layer->getGlobalTransform();

  const QPointF center =

      localRect.isValid()

          ? globalTransform.map(localRect.center())

          : QPointF(globalTransform.dx(), globalTransform.dy());

  const float scaleX = std::max<float>(

      0.01f, static_cast<float>(

                 std::hypot(globalTransform.m11(), globalTransform.m12())));

  const float scaleY = std::max<float>(

      0.01f, static_cast<float>(

                 std::hypot(globalTransform.m21(), globalTransform.m22())));

  const float rotationZ =

      std::atan2(globalTransform.m12(), globalTransform.m11()) *

      (180.0f / 3.14159265358979323846f);



  gizmo3D_->setDepthEnabled(false);

  // In AnchorPoint mode the single handle must sit on the layer anchor, not
  // on the bounding-box center, so the 3D gizmo is re-pointed at the anchor's
  // world position.  Every other mode keeps the center as the pivot.
  if (gizmoMode_ == TransformGizmo::Mode::AnchorPoint) {
    const auto &anchorT3 = layer->transform3D();
    const QPointF worldAnchor = globalTransform.map(
        QPointF(anchorT3.anchorX(), anchorT3.anchorY()));
    gizmo3D_->setTransform(
        ArtifactCore::Coordinates::worldPoint3FromQVector3D(
            QVector3D(static_cast<float>(worldAnchor.x()),
                      static_cast<float>(worldAnchor.y()), 0.0f)),
        ArtifactCore::Units::eulerDegreesFromQVector3D(
            QVector3D(0.0f, 0.0f, rotationZ)));
    gizmo3D_->setScale({1.0f, 1.0f, 1.0f});
    gizmo3D_->clearBoundingBox();
    return;
  }

  gizmo3D_->setTransform(
                         ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                             QVector3D(static_cast<float>(center.x()),
                                       static_cast<float>(center.y()), 0.0f)),

                         ArtifactCore::Units::eulerDegreesFromQVector3D(
                             QVector3D(0.0f, 0.0f, rotationZ)));

  gizmo3D_->setScale({scaleX, scaleY, 1.0f});
  gizmo3D_->clearBoundingBox();

}

void CompositionRenderController::Impl::clearSelectionGestureState() {

  isDraggingLayer_ = false;

  isRubberBandSelecting_ = false;
  isLassoSelecting_ = false;
  lassoViewportPoints_.clear();

  isShapeVertexMarqueeSelecting_ = false;
  shapeVertexMarqueeStartViewportPos_ = {};
  shapeVertexMarqueeCurrentViewportPos_ = {};
  shapeVertexMarqueeSelectionMode_ = SelectionMode::Replace;

  dragGroupMove_ = false;

  dragGroupLayers_.clear();

  dragGroupStartPositions_.clear();

  clearTransformFieldDragState();

  clearTransformFieldHoverState();

}

void CompositionRenderController::Impl::clearTransformFieldDragState() {
  isDraggingTransformField_ = false;
  draggingTransformFieldId_.clear();
  draggingTransformFieldBefore_ = {};
  draggingTransformFieldMode_ = TransformFieldDragMode::None;
}

void CompositionRenderController::Impl::clearTransformFieldHoverState() {
  hoveredTransformFieldId_.clear();
  hoveredTransformFieldMode_ = TransformFieldDragMode::None;
}

bool CompositionRenderController::Impl::beginTransformFieldDrag(const ArtifactCompositionPtr &comp,
                             const QString &fieldId,
                             TransformFieldDragMode mode) {
  if (!comp || fieldId.isEmpty() || mode == TransformFieldDragMode::None) {
    return false;
  }

  for (const auto &field : comp->transformFields()) {
    if (field.fieldId != fieldId) {
      continue;
    }
    draggingTransformFieldBefore_ = field;
    draggingTransformFieldId_ = fieldId;
    draggingTransformFieldMode_ = mode;
    hoveredTransformFieldId_ = fieldId;
    hoveredTransformFieldMode_ = mode;
    if (comp->activeTransformFieldId() != fieldId) {
      comp->setActiveTransformFieldId(fieldId);
    }
    isDraggingTransformField_ = true;
    return true;
  }

  return false;
}

bool CompositionRenderController::Impl::applyTransformFieldDrag(const ArtifactCompositionPtr &comp,
                             const QPointF &canvasPos) {
  if (!isDraggingTransformField_ || draggingTransformFieldId_.isEmpty() ||
      !comp) {
    return false;
  }

  for (auto field : comp->transformFields()) {
    if (field.fieldId != draggingTransformFieldId_) {
      continue;
    }

    QPointF localPoint;
    if (!transformFieldLocalPointFromCanvas(comp, field, canvasPos,
                                            localPoint)) {
      return false;
    }

    if (draggingTransformFieldMode_ == TransformFieldDragMode::Center) {
      field.center = localPoint;
    } else if (draggingTransformFieldMode_ == TransformFieldDragMode::Radius) {
      field.radius = field.shape == QStringLiteral("box")
                         ? std::max<qreal>(
                               0.01, std::abs(localPoint.x() - field.center.x()))
                         : std::max<qreal>(
                               0.01, std::hypot(localPoint.x() - field.center.x(),
                                                localPoint.y() - field.center.y()));
      if (field.shape == QStringLiteral("linear")) {
        field.rotationDegrees = std::atan2(
                                    localPoint.y() - field.center.y(),
                                    localPoint.x() - field.center.x()) *
                                (180.0 / std::acos(-1.0));
      }
    } else if (draggingTransformFieldMode_ ==
               TransformFieldDragMode::SecondaryRadius) {
      if (field.shape != QStringLiteral("box")) {
        return false;
      }
      field.secondaryRadius = std::max<qreal>(
          0.01, std::abs(localPoint.y() - field.center.y()));
    } else {
      return false;
    }

    comp->addTransformField(field);
    comp->changed();
    invalidateOverlayComposite();
    return true;
  }

  return false;
}

bool CompositionRenderController::Impl::updateTransformFieldHover(const ArtifactCompositionPtr &comp,
                               const QPointF &canvasPos,
                               const QSet<LayerID> &selectedLayerIds,
                               qreal hitThreshold) {
  QString fieldId;
  TransformFieldDragMode mode = TransformFieldDragMode::None;
  if (!hitTestTransformFieldHandle(comp, canvasPos, selectedLayerIds,
                                   hitThreshold, fieldId, mode)) {
    const bool changed = !hoveredTransformFieldId_.isEmpty() ||
                         hoveredTransformFieldMode_ != TransformFieldDragMode::None;
    clearTransformFieldHoverState();
    return changed;
  }

  if (hoveredTransformFieldId_ == fieldId &&
      hoveredTransformFieldMode_ == mode) {
    return false;
  }
  hoveredTransformFieldId_ = std::move(fieldId);
  hoveredTransformFieldMode_ = mode;
  return true;
}

namespace CompositionRenderControllerDetail {







bool isScaleHandle(TransformGizmo::HandleType handle) {

  switch (handle) {

  case TransformGizmo::HandleType::Scale_TL:

  case TransformGizmo::HandleType::Scale_TR:

  case TransformGizmo::HandleType::Scale_BL:

  case TransformGizmo::HandleType::Scale_BR:

  case TransformGizmo::HandleType::Scale_T:

  case TransformGizmo::HandleType::Scale_B:

  case TransformGizmo::HandleType::Scale_L:

  case TransformGizmo::HandleType::Scale_R:

    return true;

  default:

    return false;

  }

}

} // namespace CompositionRenderControllerDetail







ArtifactAbstractLayerPtr hitNearest3DModelLayerAtPickingRay(
    const ArtifactCompositionPtr &comp, const WorldRay &ray) {
  if (!comp) {
    return {};
  }
  const auto currentFrame = currentFrameForComposition(comp);
  ArtifactAbstractLayerPtr nearestLayer;
  ArtifactCore::Units::WorldLength nearestDistance{
      std::numeric_limits<float>::max()};
  for (const auto &layer : comp->allLayerRef()) {
    if (!isLayerEffectivelyVisible(layer) || !layer->isActiveAt(currentFrame)) {
      continue;
    }
    const auto modelLayer = ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer);
    ArtifactCore::Units::WorldLength distance{};
    if (modelLayer && intersectModelLayerPickingRay(*modelLayer, ray, distance) &&
        distance.value < nearestDistance.value) {
      nearestDistance = distance;
      nearestLayer = layer;
    }
  }
  return nearestLayer;
}

namespace CompositionRenderControllerDetail {



QString uniqueLayerNameForCurrentComposition(const QString &baseName)

{

  const QString trimmedBase = baseName.trimmed().isEmpty()

                                  ? QStringLiteral("Layer 1")

                                  : baseName.trimmed();



  QSet<QString> occupied;

  if (auto *service = ArtifactProjectService::instance()) {

    if (auto comp = service->currentComposition().lock()) {

      for (const auto &layer : comp->allLayer()) {

        if (!layer) {

          continue;

        }

        const QString name = layer->layerName().trimmed();

        if (!name.isEmpty()) {

          occupied.insert(name);

        }

      }

    }

  }



  if (!occupied.contains(trimmedBase)) {

    return trimmedBase;

  }



  QString prefix = trimmedBase;

  int startNumber = 2;

  int end = trimmedBase.size();

  while (end > 0 && trimmedBase.at(end - 1).isDigit()) {

    --end;

  }

  if (end < trimmedBase.size()) {

    int start = end;

    while (start > 0 && trimmedBase.at(start - 1).isSpace()) {

      --start;

    }

    bool ok = false;

    const int current = trimmedBase.mid(end).toInt(&ok);

    if (ok) {

      prefix = trimmedBase.left(start);

      startNumber = current + 1;

    }

  }

  if (prefix == trimmedBase && !prefix.endsWith(QLatin1Char(' '))) {

    prefix += QLatin1Char(' ');

  }

  for (int index = startNumber; index < 10000; ++index) {

    const QString candidate = prefix + QString::number(index);

    if (!occupied.contains(candidate)) {

      return candidate;

    }

  }

  return trimmedBase;

}

bool layerUsesTextGizmo(const ArtifactAbstractLayerPtr &layer) {

  return layer && dynamic_cast<ArtifactTextLayer *>(layer.get()) != nullptr;

}

bool layerUsesContentGizmo(const ArtifactAbstractLayerPtr &layer) {

  if (!layer) {
    return false;
  }
  return dynamic_cast<ArtifactImageLayer *>(layer.get()) != nullptr ||
         dynamic_cast<ArtifactSolid2DLayer *>(layer.get()) != nullptr ||
         dynamic_cast<ArtifactSolidImageLayer *>(layer.get()) != nullptr;

}

bool layerUsesProjectedFrameGizmo(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return false;
  }
  if (dynamic_cast<ArtifactImageLayer *>(layer.get()) != nullptr ||
      dynamic_cast<ArtifactSolidImageLayer *>(layer.get()) != nullptr ||
      dynamic_cast<ArtifactSolid2DLayer *>(layer.get()) != nullptr) {
    return true;
  }
  if (const auto *model = dynamic_cast<const Artifact3DLayer *>(layer.get())) {
    return model->fixedGeometry() == FixedGeometry3D::Plane;
  }
  return false;
}

GizmoOwner resolveGizmoOwner(const ArtifactAbstractLayerPtr &layer,
                            bool contentEditMode, bool cameraAvailable) {
  if (!layer) return GizmoOwner::None;
  if (layerUsesTextGizmo(layer)) return GizmoOwner::Text;
  if (contentEditMode && layerUsesContentGizmo(layer)) return GizmoOwner::Content;
  return cameraAvailable || layer->is3D() || layerUsesProjectedFrameGizmo(layer)
      ? GizmoOwner::Transform3D : GizmoOwner::Transform2D;
}

bool isTransformGizmoOwner(GizmoOwner owner) {
  return owner == GizmoOwner::Transform2D || owner == GizmoOwner::Transform3D;
}

} // namespace CompositionRenderControllerDetail

QRectF viewportRectToCanvasRect(ArtifactIRenderer *renderer,

                                const QPointF &startViewportPos,

                                const QPointF &endViewportPos) {

  if (!renderer) {

    return QRectF();

  }



  const auto a =

      renderer->viewportToCanvas({static_cast<float>(startViewportPos.x()),

                                  static_cast<float>(startViewportPos.y())});

  const auto b =

      renderer->viewportToCanvas({static_cast<float>(endViewportPos.x()),

                                  static_cast<float>(endViewportPos.y())});

  return QRectF(QPointF(std::min(a.x, b.x), std::min(a.y, b.y)),

                QPointF(std::max(a.x, b.x), std::max(a.y, b.y)));

}

QStringList selectedLayerIdList() {

  QStringList ids;

  auto *app = ArtifactApplicationManager::instance();

  auto *selection = app ? app->layerSelectionManager() : nullptr;

  if (!selection) {

    return ids;

  }



  for (const auto &layer : selection->selectedLayersInOrder()) {

    if (layer) {

      ids.push_back(layer->id().toString());

    }

  }

  return ids;

}

ArtifactAbstractLayerPtr

hitTopmostLayerAtViewportPos(const ArtifactCompositionPtr &comp,

                             ArtifactIRenderer *renderer,

                             const QPointF &viewportPos) {

  if (!comp || !renderer) {

    return ArtifactAbstractLayerPtr();

  }



  const auto currentFrame = currentFrameForComposition(comp);

  const auto canvasPos =

      renderer->viewportToCanvas({static_cast<float>(viewportPos.x()),

                                  static_cast<float>(viewportPos.y())});

  const auto &layers = comp->allLayerRef();

  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {

    const auto &layer = layers[static_cast<size_t>(i)];

    if (!isLayerEffectivelyVisible(layer) || !layer->isActiveAt(currentFrame)) {

      continue;

    }

    // 3D model layers are picked by their mesh and camera ray below. Their
    // projected 2D bounds cannot represent depth or the actual silhouette.
    if (ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer)) {
      continue;
    }

    const QTransform globalTransform = layer->getGlobalTransform();

    bool invertible = false;

    const QTransform invTransform = globalTransform.inverted(&invertible);

    if (invertible) {

      const QPointF localPos =

          invTransform.map(QPointF(canvasPos.x, canvasPos.y));

      if (layer->localBounds().contains(localPos)) {

        return layer;

      }

      continue;

    }



    const QRectF bbox = layer->transformedBoundingBox();

    if (bbox.contains(canvasPos.x, canvasPos.y)) {

      return layer;

    }

  }



  return ArtifactAbstractLayerPtr();

}

} // namespace Artifact
