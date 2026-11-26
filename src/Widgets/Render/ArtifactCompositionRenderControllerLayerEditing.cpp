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

bool CompositionRenderController::resetProjectedFrameHandleAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->renderer_ || impl_->selectedLayerId_.isNil()) {
    return false;
  }

  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
  if (!layer || !layer->is3D() || layerUsesTextGizmo(layer)) {
    return false;
  }

  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  const QMatrix4x4 &view = impl_->gizmo3DCameraMatricesValid_
                               ? impl_->gizmo3DViewMatrix_
                               : impl_->renderer_->getViewMatrix();
  const QMatrix4x4 &projection = impl_->gizmo3DCameraMatricesValid_
                                     ? impl_->gizmo3DProjectionMatrix_
                                     : impl_->renderer_->getProjectionMatrix();
  const QRect viewport(
      0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
      std::max(1, static_cast<int>(impl_->hostHeight_)));
  auto handle = hitTestProjectedFrameCorner(
      layer, screenPhysicalPoint(physicalPos), view, projection, viewport,
      32.0f * std::max(1.0f, impl_->devicePixelRatio_));
  if (!projectedFrameHandleEnabled(impl_->gizmoMode_, handle)) {
    handle = TransformGizmo::HandleType::None;
  }
  const bool resetPosition =
      handle == TransformGizmo::HandleType::None &&
      hitTestProjectedFrameInterior(layer, screenPhysicalPoint(physicalPos),
                                    view, projection, viewport);
  if (handle == TransformGizmo::HandleType::None && !resetPosition) {
    return false;
  }

  const auto &transform = layer->transform3D();
  const auto time = gizmoTransformTime(layer, layer->currentFrame());
  GizmoTransformSnapshot before;
  before.position = layer->position3D();
  before.rotation = ArtifactCore::Units::eulerDegreesFromQVector3D(
      layer->rotation3D());
  const float scaleZ = transform.snapshotAt(time).scaleZ;
  before.scale = {transform.scaleXAt(time), transform.scaleYAt(time), scaleZ};
  before.is3D = true;
  captureGizmoKeyState(layer, layer->currentFrame(), before);
  GizmoTransformSnapshot after = before;

  if (handle == TransformGizmo::HandleType::Rotate) {
    // Artifact's current 3D rotation bridge stores the editable scalar
    // layer-plane rotation in X. Keep this consistent with the live frame
    // rotation and Undo paths rather than resetting an unused Z component.
    after.rotation.x = {};
  } else if (resetPosition) {
    const QSize compositionSize = comp->settings().compositionSize();
    const float width = static_cast<float>(
        compositionSize.width() > 0 ? compositionSize.width() : 1920);
    const float height = static_cast<float>(
        compositionSize.height() > 0 ? compositionSize.height() : 1080);
    after.position.x = width * 0.5f;
    after.position.y = height * 0.5f;
  } else {
    after.scale.x = 1.0f;
    after.scale.y = 1.0f;
  }

  const auto changed = [](const GizmoTransformSnapshot &lhs,
                          const GizmoTransformSnapshot &rhs) {
    return layerLocalPositionDeltaLengthSquared(lhs.position, rhs.position) >
               0.000001f ||
           eulerDegreesDeltaLengthSquared(lhs.rotation, rhs.rotation) >
               0.000001f ||
           scale3DeltaLengthSquared(lhs.scale, rhs.scale) > 0.000001f;
  };
  if (!changed(before, after)) {
    return true;
  }

  applyLiveGizmoTransform(layer, layer->currentFrame(), before, after);
  captureGizmoKeyState(layer, layer->currentFrame(), after);
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<GizmoTransformUndoCommand>(
                            layer, layer->currentFrame(), before, after))) {
    GizmoTransformUndoCommand rollback(layer, layer->currentFrame(), before, after);
    rollback.undo();
    return false;
  }
  if (auto *composition =
          static_cast<ArtifactAbstractComposition *>(layer->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{composition->id().toString(), layer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::applyProjectedFrameAnchorDelta(
    ArtifactCore::Coordinates::WorldVector3 worldAnchorDelta) {
  if (!impl_ || impl_->selectedLayerId_.isNil()) return false;
  const QVector3D worldDelta =
      ArtifactCore::Coordinates::toQVector3D(worldAnchorDelta);
  if (worldDelta.lengthSquared() < 0.000001f) return true;
  const auto layer = impl_->gizmoAnchorDragStartLayer_.lock();
  if (!layer) return false;

  // The 3D gizmo reports the anchor offset in world space, but the transform
  // stores it in layer-local coordinates.  Convert through the inverse of the
  // layer's world matrix so rotation and scale are undone exactly, and keep the
  // on-screen content anchored: moving the anchor without compensating the
  // position would shift the artwork, which is not what dragging an anchor does.
  //
  // Both the anchor and the position are written from the press-time reference,
  // not from the current values, because this runs every frame of the drag and
  // an incremental read-modify-write would compound.
  const QTransform worldToLocal = layer->getGlobalTransform().inverted();
  const QPointF localDelta = worldToLocal.map(worldDelta.toPointF());
  if (!std::isfinite(localDelta.x()) || !std::isfinite(localDelta.y()) ||
      localDelta.isNull()) return true;

  auto &transform = layer->transform3D();
  const auto time = gizmoTransformTime(layer, layer->currentFrame());
  const QVector3D beforeAnchor =
      ArtifactCore::Coordinates::toQVector3D(impl_->gizmoAnchorDragStart_);
  const QVector3D beforePosition = ArtifactCore::Coordinates::toQVector3D(
      impl_->gizmoAnchorDragStartPosition_);

  const double radians = transform.rotationAt(time) *
                         3.14159265358979323846 / 180.0;
  const double cosAngle = std::cos(radians);
  const double sinAngle = std::sin(radians);
  const QPointF compensation(
      localDelta.x() * transform.scaleXAt(time) * cosAngle -
          localDelta.y() * transform.scaleYAt(time) * sinAngle,
      localDelta.x() * transform.scaleXAt(time) * sinAngle +
          localDelta.y() * transform.scaleYAt(time) * cosAngle);

  const QVector3D afterAnchorValue(
      beforeAnchor.x() + static_cast<float>(localDelta.x()),
      beforeAnchor.y() + static_cast<float>(localDelta.y()),
      beforeAnchor.z());
  const QVector3D afterPositionValue(
      beforePosition.x() + static_cast<float>(compensation.x()),
      beforePosition.y() + static_cast<float>(compensation.y()),
      beforePosition.z());
  const auto afterAnchor =
      ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterAnchorValue);
  const auto afterPosition =
      ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterPositionValue);
  if (layerLocalPositionDeltaLengthSquared(afterAnchor,
                                           impl_->gizmoAnchorDragStart_) <
          0.000001f &&
      layerLocalPositionDeltaLengthSquared(
          afterPosition, impl_->gizmoAnchorDragStartPosition_) < 0.000001f) {
    return true;
  }
  transform.setAnchor(time, afterAnchor.x, afterAnchor.y, afterAnchor.z);
  transform.setPosition(time, afterPosition.x, afterPosition.y);
  transform.setPositionZ(time, afterPosition.z);
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  // The Undo command is pushed once on release, not per frame: this runs on
  // every mouse-move, and pushing here would flood the undo stack with one
  // entry per sample.  gizmoAnchorDragCurrent_ carries the live result.
  impl_->gizmoAnchorDragCurrent_ = afterAnchor;
  impl_->gizmoAnchorDragCurrentPosition_ = afterPosition;
  impl_->gizmoAnchorDragHasResult_ = true;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::commitProjectedFrameAnchorDrag() {
  if (!impl_ || !impl_->gizmoAnchorDragHasResult_) return false;
  const auto layer = impl_->gizmoAnchorDragStartLayer_.lock();
  impl_->gizmoAnchorDragHasResult_ = false;
  if (!layer) return false;
  const auto beforeAnchor = impl_->gizmoAnchorDragStart_;
  const auto beforePosition = impl_->gizmoAnchorDragStartPosition_;
  const auto afterAnchor = impl_->gizmoAnchorDragCurrent_;
  const auto afterPosition = impl_->gizmoAnchorDragCurrentPosition_;
  if (layerLocalPositionDeltaLengthSquared(afterAnchor, beforeAnchor) <
          0.000001f &&
      layerLocalPositionDeltaLengthSquared(afterPosition, beforePosition) <
          0.000001f) {
    return true;
  }
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<AnchorPointUndoCommand>(
                            layer, layer->currentFrame(),
                            beforeAnchor, beforePosition, afterAnchor,
                            afterPosition))) {
    // Roll the layer back when the undo stack refuses the command, matching the
    // fail-closed behavior the other gizmo write-back paths use.
    auto &transform = layer->transform3D();
    const auto time = gizmoTransformTime(layer, layer->currentFrame());
    transform.setAnchor(time, beforeAnchor.x, beforeAnchor.y, beforeAnchor.z);
    transform.setPosition(time, beforePosition.x, beforePosition.y);
    transform.setPositionZ(time, beforePosition.z);
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  return true;
}

bool CompositionRenderController::resetSelected3DAnchorToCenter() {
  if (!impl_ || impl_->selectedLayerId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
  if (!layer || !layer->is3D()) return false;

  const QRectF bounds = layer->localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return false;
  }
  auto &transform = layer->transform3D();
  const auto time = gizmoTransformTime(layer, layer->currentFrame());
  const QVector3D beforeAnchor(transform.anchorXAt(time),
                               transform.anchorYAt(time),
                               transform.anchorZAt(time));
  const QVector3D beforePosition =
      ArtifactCore::Coordinates::toQVector3D(layer->position3D());
  const QPointF target = bounds.center();
  const QPointF delta(target.x() - beforeAnchor.x(),
                      target.y() - beforeAnchor.y());
  const double radians = transform.rotationAt(time) *
                         3.14159265358979323846 / 180.0;
  const double cosAngle = std::cos(radians);
  const double sinAngle = std::sin(radians);
  const QPointF compensation(
      delta.x() * transform.scaleXAt(time) * cosAngle -
          delta.y() * transform.scaleYAt(time) * sinAngle,
      delta.x() * transform.scaleXAt(time) * sinAngle +
          delta.y() * transform.scaleYAt(time) * cosAngle);
  const QVector3D afterAnchor(static_cast<float>(target.x()),
                              static_cast<float>(target.y()),
                              beforeAnchor.z());
  const QVector3D afterPosition(
      beforePosition.x() + static_cast<float>(compensation.x()),
      beforePosition.y() + static_cast<float>(compensation.y()),
      beforePosition.z());
  if ((afterAnchor - beforeAnchor).lengthSquared() < 0.000001f &&
      (afterPosition - beforePosition).lengthSquared() < 0.000001f) {
    return true;
  }
  transform.setAnchor(time, afterAnchor.x(), afterAnchor.y(), afterAnchor.z());
  transform.setPosition(time, afterPosition.x(), afterPosition.y());
  transform.setPositionZ(time, afterPosition.z());
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<AnchorPointUndoCommand>(
                            layer, layer->currentFrame(),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(beforeAnchor),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(beforePosition),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterAnchor),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterPosition)))) {
    transform.setAnchor(time, beforeAnchor.x(), beforeAnchor.y(), beforeAnchor.z());
    transform.setPosition(time, beforePosition.x(), beforePosition.y());
    transform.setPositionZ(time, beforePosition.z());
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return false;
  }
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setSelected3DTransform(
    ArtifactCore::Coordinates::LayerLocalPoint3 position,
    ArtifactCore::Units::EulerDegrees3 rotation,
    ArtifactCore::Units::Scale3 scale) {
  if (!impl_ || impl_->selectedLayerId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
  if (!layer || !layer->is3D() || layer->isLocked() || layer->isSelectionLocked()) {
    return false;
  }
  GizmoTransformSnapshot before;
  before.position = layer->position3D();
  before.rotation = ArtifactCore::Units::eulerDegreesFromQVector3D(
      layer->rotation3D());
  before.scale = {layer->transform3D().scaleX(),
                  layer->transform3D().scaleY(),
                  layer->transform3D().scaleZ()};
  before.is3D = true;
  captureGizmoKeyState(layer, layer->currentFrame(), before);
  GizmoTransformSnapshot after;
  after.position = position;
  after.rotation = rotation;
  after.scale = {std::max(0.001f, scale.x), std::max(0.001f, scale.y),
                 std::max(0.001f, scale.z)};
  after.is3D = true;
  if (layerLocalPositionsEqual(before.position, after.position) &&
      eulerDegreesEqual(before.rotation, after.rotation) &&
      scale3Equal(before.scale, after.scale)) {
    return true;
  }
  applyLiveGizmoTransform(layer, layer->currentFrame(), before, after);
  captureGizmoKeyState(layer, layer->currentFrame(), after);
  auto* manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<GizmoTransformUndoCommand>(
                           layer, layer->currentFrame(), before, after))) {
    GizmoTransformUndoCommand rollback(layer, layer->currentFrame(), before, after);
    rollback.undo();
    return false;
  }
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetSelected3DTransform() {
  if (!impl_ || impl_->selectedLayerId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
  if (!layer || !layer->is3D() || layer->isLocked() || layer->isSelectionLocked()) {
    return false;
  }
  GizmoTransformSnapshot before;
  before.position = layer->position3D();
  before.rotation = ArtifactCore::Units::eulerDegreesFromQVector3D(
      layer->rotation3D());
  before.scale = {layer->transform3D().scaleX(),
                  layer->transform3D().scaleY(),
                  layer->transform3D().scaleZ()};
  before.is3D = true;
  captureGizmoKeyState(layer, layer->currentFrame(), before);
  GizmoTransformSnapshot after;
  after.position = {0.0f, 0.0f, before.position.z};
  after.rotation = {};
  after.scale = {1.0f, 1.0f, 1.0f};
  after.is3D = true;
  if (layerLocalPositionsEqual(before.position, after.position) &&
      eulerDegreesEqual(before.rotation, after.rotation) &&
      scale3Equal(before.scale, after.scale)) {
    return true;
  }
  applyLiveGizmoTransform(layer, layer->currentFrame(), before, after);
  captureGizmoKeyState(layer, layer->currentFrame(), after);
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<GizmoTransformUndoCommand>(
                            layer, layer->currentFrame(), before, after))) {
    GizmoTransformUndoCommand rollback(layer, layer->currentFrame(), before, after);
    rollback.undo();
    return false;
  }
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetSelectedTransformComponent(
    int component) {
  if (component < 0 || component > 2 || !impl_ ||
      impl_->selectedLayerId_.isNil()) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  if (!comp) {
    return false;
  }
  QVector<ArtifactAbstractLayerPtr> targets;
  if (auto *selection = ArtifactLayerSelectionManager::instance()) {
    for (const auto &candidate : selection->selectedLayers()) {
      if (candidate && !candidate->isLocked() &&
          !candidate->isSelectionLocked()) {
        targets.push_back(candidate);
      }
    }
  }
  if (targets.isEmpty()) {
    if (const auto layer = comp->layerById(impl_->selectedLayerId_);
        layer && !layer->isLocked() && !layer->isSelectionLocked()) {
      targets.push_back(layer);
    }
  }
  std::vector<GizmoGroupUndoEntry> undoEntries;
  for (const auto &layer : targets) {
    const auto &transform = layer->transform3D();
    const int64_t frame = layer->currentFrame();
    const auto time = gizmoTransformTime(layer, frame);
    GizmoTransformSnapshot before;
    before.position = {transform.positionXAt(time),
                       transform.positionYAt(time),
                       transform.positionZAt(time)};
    before.rotation = layer->is3D()
        ? ArtifactCore::Units::eulerDegreesFromQVector3D(
              layer->rotation3D())
        : ArtifactCore::Units::EulerDegrees3{
              {}, {}, {transform.rotationAt(time)}};
    before.scale = {transform.scaleXAt(time), transform.scaleYAt(time),
                    layer->is3D() ? transform.snapshotAt(time).scaleZ : 1.0f};
    before.is3D = layer->is3D();
    captureGizmoKeyState(layer, frame, before);
    GizmoTransformSnapshot after = before;
    if (component == 0) {
      after.position = {};
    } else if (component == 1) {
      after.rotation = {};
    } else {
      after.scale = {1.0f, 1.0f, 1.0f};
    }
    if (layerLocalPositionsEqual(before.position, after.position) &&
        eulerDegreesEqual(before.rotation, after.rotation) &&
        scale3Equal(before.scale, after.scale)) {
      continue;
    }
    applyLiveGizmoTransform(layer, frame, before, after);
    captureGizmoKeyState(layer, frame, after);
    undoEntries.push_back(GizmoGroupUndoEntry{layer, frame, before, after});
  }
  if (undoEntries.empty()) {
    return !targets.isEmpty();
  }
  const auto rollbackEntries = undoEntries;
  auto *manager = UndoManager::instance();
  bool pushed = manager == nullptr;
  if (manager) {
    if (undoEntries.size() == 1) {
      const auto &entry = undoEntries.front();
      pushed = manager->push(std::make_unique<GizmoTransformUndoCommand>(
          entry.layer.lock(), entry.frame, entry.before, entry.after));
    } else {
      pushed = manager->push(std::make_unique<GizmoGroupTransformUndoCommand>(
          std::move(undoEntries)));
    }
  }
  if (!pushed) {
    for (const auto &entry : rollbackEntries) {
      if (const auto target = entry.layer.lock()) {
        GizmoTransformUndoCommand rollback(target, entry.frame, entry.before, entry.after);
        rollback.undo();
      }
    }
    return false;
  }
  for (const auto &entry : rollbackEntries) {
    if (const auto target = entry.layer.lock()) {
      impl_->publishLayerModified(target);
    }
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

namespace {
QRectF arrangeSelectionUnion(
    const std::vector<ArtifactAbstractLayerPtr> &targets) {
  QRectF out;
  bool hasRect = false;
  for (const auto &target : targets) {
    if (!target) {
      continue;
    }
    const QRectF bounds = target->transformedBoundingBox();
    if (!bounds.isValid() || bounds.width() <= 0.0 ||
        bounds.height() <= 0.0) {
      continue;
    }
    if (!hasRect) {
      out = bounds;
      hasRect = true;
    } else {
      out = out.united(bounds);
    }
  }
  return hasRect ? out : QRectF();
}

QRectF arrangeCompRect(const ArtifactCompositionPtr &comp) {
  QSize size;
  if (comp) {
    size = comp->settings().compositionSize();
  }
  const double width = size.width() > 0 ? size.width() : 1920.0;
  const double height = size.height() > 0 ? size.height() : 1080.0;
  return QRectF(0.0, 0.0, width, height);
}

std::vector<ArtifactAbstractLayerPtr> arrangeGizmoTargets(
    TransformGizmo *gizmo) {
  std::vector<ArtifactAbstractLayerPtr> targets;
  if (!gizmo) {
    return targets;
  }
  for (const auto &candidate : gizmo->targetLayers()) {
    if (candidate && !candidate->isLocked() &&
        !candidate->isSelectionLocked()) {
      targets.push_back(candidate);
    }
  }
  return targets;
}
}  // namespace

bool CompositionRenderController::fitSelectedToComp(ArrangeFitMode mode) {
  if (!impl_ || !impl_->gizmo_) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto changed = impl_->gizmo_->fitTargetsToRect(arrangeCompRect(comp), mode);
  if (changed.empty()) {
    return false;
  }
  for (const auto &target : changed) {
    target->changed();
    impl_->publishLayerModified(target);
  }
  impl_->fitGuideCompRect_ = arrangeCompRect(comp);
  impl_->fitGuideResultRect_ = arrangeSelectionUnion(changed);
  impl_->fitGuideVisible_ = impl_->fitGuideCompRect_.isValid() &&
                            impl_->fitGuideResultRect_.isValid();
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::alignSelectedLayers(ArrangeAlignMode mode) {
  if (!impl_ || !impl_->gizmo_) {
    return false;
  }
  const auto targets = arrangeGizmoTargets(impl_->gizmo_.get());
  if (targets.empty()) {
    return false;
  }
  // Multi-selection aligns within itself; a single layer aligns to the comp.
  const QRectF reference = targets.size() > 1
                               ? arrangeSelectionUnion(targets)
                               : arrangeCompRect(impl_->previewPipeline_.composition());
  const auto changed = impl_->gizmo_->alignTargets(mode, reference);
  if (changed.empty()) {
    return false;
  }
  for (const auto &target : changed) {
    target->changed();
    impl_->publishLayerModified(target);
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::distributeSelectedLayers(
    ArrangeDistributeAxis axis) {
  if (!impl_ || !impl_->gizmo_) {
    return false;
  }
  const auto changed = impl_->gizmo_->distributeTargets(axis);
  if (changed.empty()) {
    return false;
  }
  for (const auto &target : changed) {
    target->changed();
    impl_->publishLayerModified(target);
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

ArtifactCore::SharedPtr<ArtifactImageLayer>
CompositionRenderController::selectedCropLayer() const {
  if (!impl_ || impl_->selectedLayerId_.isNil()) {
    return {};
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer =
      comp ? comp->layerById(impl_->selectedLayerId_) : ArtifactAbstractLayerPtr{};
  if (!layer || layer->isLocked() || layer->isSelectionLocked()) {
    return {};
  }
  return ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
}

bool CompositionRenderController::selectedSupportsCrop() const {
  return static_cast<bool>(selectedCropLayer());
}

bool CompositionRenderController::setSelectedCropEnabled(bool enabled) {
  const auto imageLayer = selectedCropLayer();
  if (!imageLayer) {
    return false;
  }
  const auto before = imageLayer->sourceCrop();
  if (before.enabled() == enabled) {
    return true;
  }
  auto after = before;
  after.setEnabled(enabled);
  if (enabled && !after.cropRect().isValid()) {
    after.reset();
    after.setEnabled(true);
  }
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                     after.enabled());
  auto *manager = UndoManager::instance();
  bool pushed = manager == nullptr;
  if (manager) {
    pushed = manager->push(std::make_unique<SourceCropRectUndoCommand>(
        imageLayer, before, imageLayer->sourceCrop()));
  }
  if (!pushed) {
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                       before.enabled());
    return false;
  }
  impl_->publishLayerModified(imageLayer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setSelectedCropRect(const QRectF &sourceRect) {
  return setSelectedCropRect(toSourcePixelBounds2(sourceRect));
}

bool CompositionRenderController::setSelectedCropRect(
    SourcePixelBounds2 typedSourceRect) {
  const QRectF sourceRect = toQRectF(typedSourceRect);
  const auto imageLayer = selectedCropLayer();
  if (!imageLayer || !sourceRect.isValid() || sourceRect.width() <= 0.0 ||
      sourceRect.height() <= 0.0) {
    return false;
  }
  const auto before = imageLayer->sourceCrop();
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), true);
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"),
                                     sourceRect.width());
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"),
                                     sourceRect.height());
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                     sourceRect.x());
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                     sourceRect.y());
  const auto after = imageLayer->sourceCrop();
  if (before.enabled() == after.enabled() &&
      before.cropRect() == after.cropRect()) {
    return true;
  }
  auto *manager = UndoManager::instance();
  bool pushed = manager == nullptr;
  if (manager) {
    pushed = manager->push(
        std::make_unique<SourceCropRectUndoCommand>(imageLayer, before, after));
  }
  if (!pushed) {
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                       before.enabled());
    const QRectF rect = before.cropRect();
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"),
                                       rect.width());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"),
                                       rect.height());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"),
                                       rect.x());
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"),
                                       rect.y());
    return false;
  }
  impl_->publishLayerModified(imageLayer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetSelectedCrop() {
  const auto imageLayer = selectedCropLayer();
  if (!imageLayer || !imageLayer->sourceCropEnabled()) {
    return false;
  }
  const auto before = imageLayer->sourceCrop();
  imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), false);
  const auto after = imageLayer->sourceCrop();
  auto *manager = UndoManager::instance();
  bool pushed = manager == nullptr;
  if (manager) {
    pushed = manager->push(
        std::make_unique<SourceCropRectUndoCommand>(imageLayer, before, after));
  }
  if (!pushed) {
    imageLayer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"),
                                       before.enabled());
    return false;
  }
  impl_->publishLayerModified(imageLayer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::setContentEditMode(bool enabled) {
  if (!impl_) {
    return;
  }
  if (impl_->contentEditMode_ != enabled &&
      impl_->gizmoSession_.owner != GizmoOwner::None) {
    cancelGizmoInteraction();
  }
  impl_->contentEditMode_ = enabled;
  markRenderDirty();
}

bool CompositionRenderController::contentEditMode() const {
  return impl_ && impl_->contentEditMode_;
}

bool CompositionRenderController::resetSelected2DAnchorToCenter() {
  return setSelected2DAnchorPreset(4);
}

bool CompositionRenderController::setSelected2DAnchorPreset(int preset) {
  if (!impl_ || impl_->selectedLayerId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                          : ArtifactAbstractLayerPtr{};
  if (!layer || layer->is3D() || layer->isLocked() ||
      layer->isSelectionLocked()) {
    return false;
  }

  const QRectF bounds = layer->localBounds();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return false;
  }

  auto &transform = layer->transform3D();
  const auto time = gizmoTransformTime(layer, layer->currentFrame());
  const QVector3D beforeAnchor(transform.anchorXAt(time),
                               transform.anchorYAt(time),
                               transform.anchorZAt(time));
  const QVector3D beforePosition(transform.positionXAt(time),
                                 transform.positionYAt(time),
                                 transform.positionZAt(time));
  const int normalizedPreset = std::clamp(preset, 0, 8);
  const int column = normalizedPreset % 3;
  const int row = normalizedPreset / 3;
  const double targetX = column == 0
                             ? bounds.left()
                             : (column == 1 ? bounds.center().x()
                                            : bounds.right());
  const double targetY = row == 0
                             ? bounds.top()
                             : (row == 1 ? bounds.center().y()
                                         : bounds.bottom());
  const QPointF target(targetX, targetY);
  const QPointF delta(target.x() - beforeAnchor.x(),
                      target.y() - beforeAnchor.y());
  const double radians = transform.rotationAt(time) *
                         3.14159265358979323846 / 180.0;
  const double cosAngle = std::cos(radians);
  const double sinAngle = std::sin(radians);
  const QPointF compensation(
      delta.x() * transform.scaleXAt(time) * cosAngle -
          delta.y() * transform.scaleYAt(time) * sinAngle,
      delta.x() * transform.scaleXAt(time) * sinAngle +
          delta.y() * transform.scaleYAt(time) * cosAngle);
  const QVector3D afterAnchor(static_cast<float>(target.x()),
                              static_cast<float>(target.y()),
                              beforeAnchor.z());
  const QVector3D afterPosition(
      beforePosition.x() + static_cast<float>(compensation.x()),
      beforePosition.y() + static_cast<float>(compensation.y()),
      beforePosition.z());
  if ((afterAnchor - beforeAnchor).lengthSquared() < 0.000001f &&
      (afterPosition - beforePosition).lengthSquared() < 0.000001f) {
    return true;
  }

  transform.setAnchor(time, afterAnchor.x(), afterAnchor.y(), afterAnchor.z());
  transform.setPosition(time, afterPosition.x(), afterPosition.y());
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<AnchorPointUndoCommand>(
                            layer, layer->currentFrame(),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(beforeAnchor),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(beforePosition),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterAnchor),
                            ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(afterPosition)))) {
    transform.setAnchor(time, beforeAnchor.x(), beforeAnchor.y(), beforeAnchor.z());
    transform.setPosition(time, beforePosition.x(), beforePosition.y());
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    return false;
  }
  impl_->publishLayerModified(layer, true);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setMaterialGraphJsonOnSelectedLayer(
    const std::string& json) {
  if (!impl_) return false;
  const auto composition = impl_->previewPipeline_.composition();
  auto layer = composition
      ? composition->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto* modelLayer = layer ? dynamic_cast<Artifact3DLayer*>(layer.get()) : nullptr;
  if (!modelLayer) return false;
  modelLayer->setMaterialGraphJson(
      QString::fromStdString(json));
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::clearMaterialGraphOnSelectedLayer() {
  if (!impl_) return false;
  const auto composition = impl_->previewPipeline_.composition();
  auto layer = composition
      ? composition->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto* modelLayer = layer ? dynamic_cast<Artifact3DLayer*>(layer.get()) : nullptr;
  if (!modelLayer) return false;
  if (modelLayer->materialGraphJson().trimmed().isEmpty()) return false;
  modelLayer->clearMaterialGraph();
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

std::string CompositionRenderController::materialGraphJsonOfSelectedLayer() const {
  if (!impl_) return {};
  const auto composition = impl_->previewPipeline_.composition();
  auto layer = composition
      ? composition->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  const auto* modelLayer = layer ? dynamic_cast<const Artifact3DLayer*>(layer.get()) : nullptr;
  if (!modelLayer) return {};
  return modelLayer->materialGraphJson().toStdString();
}

bool CompositionRenderController::deleteSelectedPuppetPin() {
  auto *app = ArtifactApplicationManager::instance();
  if (!impl_ || !app || !app->puppetTool()) {
    return false;
  }
  const QString pinId = app->puppetTool()->selectedPinId();
  const LayerID layerId = app->puppetTool()->pinLayerId(pinId);
  auto layer = impl_->previewPipeline_.composition()
      ? impl_->previewPipeline_.composition()->layerById(layerId)
      : ArtifactAbstractLayerPtr{};
  if (pinId.isEmpty() || !layer) {
    return false;
  }
  // Deformation2DStateUndoCommand::apply restores the pin state through an
  // ArtifactImageLayer cast.  Removing a text-layer pin would succeed here
  // and then fail to undo, so refuse the edit.
  if (layerUsesTextGizmo(layer)) {
    return false;
  }
  const QJsonObject before = layer->deformation2DData();
  if (!app->puppetTool()->removePin(pinId)) {
    return false;
  }
  const QJsonObject after = layer->deformation2DData();
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<Deformation2DStateUndoCommand>(
            layer, before, after, QStringLiteral("Delete Deformer Control"),
            app->puppetTool()))) {
      app->puppetTool()->restoreLayerData(layerId, before);
      return false;
    }
  }
  app->puppetTool()->setSelectedPinId(QString());
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setSelectedPuppetPinType(int type) {
  auto *app = ArtifactApplicationManager::instance();
  if (!impl_ || !app || !app->puppetTool()) {
    return false;
  }
  // Grid controls carry type 4 and are identified by the grid mode itself;
  // only the four pin kinds the user can pick are reachable from the menu.
  if (type < 0 || type > 3) {
    return false;
  }
  const QString pinId = app->puppetTool()->selectedPinId();
  if (pinId.isEmpty() || app->puppetTool()->pinTypeFor(pinId) == type) {
    return false;
  }
  const LayerID layerId = app->puppetTool()->pinLayerId(pinId);
  auto layer = impl_->previewPipeline_.composition()
      ? impl_->previewPipeline_.composition()->layerById(layerId)
      : ArtifactAbstractLayerPtr{};
  // Same reason as deleteSelectedPuppetPin: the undo restore path casts to
  // ArtifactImageLayer, so a text-layer pin would change type without being
  // undoable.
  if (layerUsesTextGizmo(layer)) {
    return false;
  }
  const QJsonObject before = layer ? layer->deformation2DData() : QJsonObject{};
  app->puppetTool()->setPinTypeFor(pinId, type);
  if (!layer) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return true;
  }
  const QJsonObject after = layer->deformation2DData();
  if (auto* undo = UndoManager::instance()) {
    if (!undo->push(std::make_unique<Deformation2DStateUndoCommand>(
            layer, before, after, QStringLiteral("Change Deformer Control Type"),
            app->puppetTool()))) {
      app->puppetTool()->restoreLayerData(layerId, before);
      return false;
    }
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetSelectedPuppetPinRotation() {
  if (!impl_) return false;
  auto *app = ArtifactApplicationManager::instance();
  if (!app || !app->puppetTool()) return false;
  const QString pinId = app->puppetTool()->selectedPinId();
  if (pinId.isEmpty()) return false;
  const int pinType = app->puppetTool()->pinTypeFor(pinId);
  if (pinType == 0) return false;
  if (pinType == 1) {
    const float before = app->puppetTool()->pinWeight(pinId);
    if (std::abs(before - 1.0f) < 0.001f) {
      return true;
    }
    auto *manager = UndoManager::instance();
    const LayerID layerId = app->puppetTool()->pinLayerId(pinId);
    const auto composition = impl_->previewPipeline_.composition();
    const auto layer = composition ? composition->layerById(layerId)
                                  : ArtifactAbstractLayerPtr{};
    if (!layer) return false;
    if (layerUsesTextGizmo(layer)) return false;
    const QJsonObject beforeState = layer->deformation2DData();
    app->puppetTool()->setPinWeight(pinId, 1.0f);
    if (manager && !manager->push(std::make_unique<Deformation2DStateUndoCommand>(
            layer, beforeState, layer->deformation2DData(),
            QStringLiteral("Reset Deformer Control"), app->puppetTool()))) {
      app->puppetTool()->restoreLayerData(layerId, beforeState, layer.get());
      return false;
    }
  } else if (pinType == 2) {
    const float before = app->puppetTool()->pinRotation(pinId);
      if (std::abs(before) < 0.001f) {
        return true;
      }
      const LayerID layerId = app->puppetTool()->pinLayerId(pinId);
      const auto layer = impl_->previewPipeline_.composition()
          ? impl_->previewPipeline_.composition()->layerById(layerId)
          : ArtifactAbstractLayerPtr{};
      if (!layer) return false;
      if (layerUsesTextGizmo(layer)) return false;
      const QJsonObject beforeState = layer->deformation2DData();
      app->puppetTool()->setPinRotation(pinId, 0.0f);
      app->puppetTool()->persistLayerData(layerId);
      auto *manager = UndoManager::instance();
      if (manager) {
        const QJsonObject afterState = layer->deformation2DData();
        if (!manager->push(std::make_unique<Deformation2DStateUndoCommand>(
                layer, beforeState, afterState,
                QStringLiteral("Reset Deformer Control"), app->puppetTool()))) {
          app->puppetTool()->restoreLayerData(layerId, beforeState, layer.get());
          return false;
        }
      }
  } else if (pinType == 3) {
    const float before = app->puppetTool()->pinDepth(pinId);
    if (std::abs(before) < 0.001f) {
      return true;
    }
    auto *manager = UndoManager::instance();
    const LayerID layerId = app->puppetTool()->pinLayerId(pinId);
    const auto composition = impl_->previewPipeline_.composition();
    const auto layer = composition ? composition->layerById(layerId)
                                  : ArtifactAbstractLayerPtr{};
    if (!layer) return false;
    if (layerUsesTextGizmo(layer)) return false;
    const QJsonObject beforeState = layer->deformation2DData();
    app->puppetTool()->setPinDepth(pinId, 0.0f);
    if (manager && !manager->push(std::make_unique<Deformation2DStateUndoCommand>(
            layer, beforeState, layer->deformation2DData(),
            QStringLiteral("Reset Deformer Control"), app->puppetTool()))) {
      app->puppetTool()->restoreLayerData(layerId, beforeState, layer.get());
      return false;
    }
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::adjustSelectedPuppetPinWeightAt(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint,
    float delta) {
  if (!impl_ || !impl_->renderer_) return false;
  auto* app = ArtifactApplicationManager::instance();
  if (!app || !app->puppetTool()) return false;
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  const auto canvas = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
  const QString hitId = app->puppetTool()->hitTestPin(
      QPointF(canvas.x, canvas.y), 14.0f / std::max(0.001f, impl_->renderer_->getZoom()));
  if (hitId.isEmpty() || app->puppetTool()->pinTypeFor(hitId) != 1) {
    return false;
  }
  app->puppetTool()->setSelectedPinId(hitId);
  const LayerID layerId = app->puppetTool()->pinLayerId(hitId);
  const auto composition = impl_->previewPipeline_.composition();
  const auto layer = composition ? composition->layerById(layerId)
                                : ArtifactAbstractLayerPtr{};
  if (!layer) return false;
  if (layerUsesTextGizmo(layer)) return false;
  const QJsonObject beforeState = layer->deformation2DData();
  const float before = app->puppetTool()->pinWeight(hitId);
  const float after = before + delta;
  app->puppetTool()->setPinWeight(hitId, after);
  const QJsonObject afterState = layer->deformation2DData();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<Deformation2DStateUndoCommand>(
          layer, beforeState, afterState,
          QStringLiteral("Adjust Deformer Control"), app->puppetTool()))) {
    app->puppetTool()->restoreLayerData(layerId, beforeState, layer.get());
    return false;
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::adjustSelectedPuppetPinDepthAt(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint,
    float delta) {
  if (!impl_ || !impl_->renderer_) return false;
  auto* app = ArtifactApplicationManager::instance();
  if (!app || !app->puppetTool()) return false;
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  const auto canvas = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
  const QString hitId = app->puppetTool()->hitTestPin(
      QPointF(canvas.x, canvas.y), 14.0f / std::max(0.001f, impl_->renderer_->getZoom()));
  if (hitId.isEmpty() || app->puppetTool()->pinTypeFor(hitId) != 3) {
    return false;
  }
  app->puppetTool()->setSelectedPinId(hitId);
  const LayerID layerId = app->puppetTool()->pinLayerId(hitId);
  const auto composition = impl_->previewPipeline_.composition();
  const auto layer = composition ? composition->layerById(layerId)
                                : ArtifactAbstractLayerPtr{};
  if (!layer) return false;
  if (layerUsesTextGizmo(layer)) return false;
  const QJsonObject beforeState = layer->deformation2DData();
  const float before = app->puppetTool()->pinDepth(hitId);
  const float after = before + delta;
  app->puppetTool()->setPinDepth(hitId, after);
  const QJsonObject afterState = layer->deformation2DData();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<Deformation2DStateUndoCommand>(
          layer, beforeState, afterState,
          QStringLiteral("Adjust Deformer Control"), app->puppetTool()))) {
    app->puppetTool()->restoreLayerData(layerId, beforeState, layer.get());
    return false;
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

} // namespace Artifact
