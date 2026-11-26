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

void CompositionRenderController::handleMouseMove(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPosLogical) {

  if (!impl_->gizmoSessionValid()) {
    cancelGizmoInteraction();
    return;
  }
  impl_->assertGizmoSession();

  // P0-1 Box zoom owns the move event while the marquee is active.
  // updateBoxZoomInteraction handles the logical->physical conversion.
  if (impl_ && impl_->isBoxZooming_) {
    updateBoxZoomInteraction(viewportPosLogical);
    return;
  }

  // P0-3a IRR handle drag. Move/edge/corner updates feed the current
  // viewport position; the controller converts to canvas space.
  if (impl_ && impl_->interactiveRenderRegionHandleDrag_ != 0) {
    updateInteractiveRenderRegionDrag(viewportPosLogical);
    return;
  }


  qCDebug(compositionViewLog)

      << "[MouseMove] ENTER logicalPos:"
      << ArtifactCore::Coordinates::toQPointF(viewportPosLogical)

      << "devicePixelRatio:" << impl_->devicePixelRatio_;



  // Convert logical pixels (from Qt event) to physical pixels for the rendering

  // pipeline

  const QPointF viewportPos =
      ArtifactCore::Coordinates::toQPointF(viewportPosLogical) *
      impl_->devicePixelRatio_;
  impl_->magnifierCursorViewportPos_ = screenPhysicalPoint(viewportPos);
  if (impl_->magnifierEnabled_) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  if (const auto layer = impl_->physicsDragLayer_.lock()) {
    const auto* playback = ArtifactPlaybackService::instance();
    if (!impl_->renderer_ || !playback || !playback->isPlaying() ||
        !layer->isVisible() || layer->isLocked()) {
      layer->endRigidBodyMouseDrag();
      impl_->physicsDragLayer_.reset();
      return;
    }
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    if (!layer->updateRigidBodyMouseDrag(QPointF(canvas.x, canvas.y))) {
      layer->endRigidBodyMouseDrag();
      impl_->physicsDragLayer_.reset();
      return;
    }
    notifyViewportInteractionActivity();
    markRenderDirty();
    return;
  }
  if (const auto layer = impl_->constructionDragLayer_) {
    if (layer->isLocked() || layer->isSelectionLocked() || !layer->isVisible() ||
        layer->id() != impl_->selectedLayerId_ || layer->currentFrame() != impl_->constructionDragFrame_) {
      cancelGizmoInteraction(); return;
    }
    QPointF local;
    if (constructionLocalPoint(viewportPos, impl_->constructionDragView_,
        impl_->constructionDragProjection_, impl_->constructionDragViewport_,
        impl_->constructionDragInverse_, local)) {
      auto item = impl_->constructionDragBefore_;
      QPointF delta = local - impl_->constructionDragStart_;
      if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
        if (std::abs(delta.x()) >= std::abs(delta.y())) delta.setY(0); else delta.setX(0);
      }
      const int kind = impl_->constructionDragHandle_.kind;
      if (kind == 1) item.start += delta;
      else if (kind == 2) item.end += delta;
      else if (kind == 3) item.radius = std::max(0.0,
          item.radius + QLineF(item.center, local).length() - QLineF(item.center, impl_->constructionDragStart_).length());
      else if (item.type == ConstructionItemType::Line) { item.start += delta; item.end += delta; }
      else item.center += delta;
      if (layer->setConstructionItem(impl_->constructionDragHandle_.index, item)) {
        notifyViewportInteractionActivity();
        impl_->invalidateLayerSurfaceCache(layer);
        impl_->invalidateBaseComposite(); impl_->invalidateOverlayComposite(); markRenderDirty();
      }
    }
    return;
  }
  if (impl_->isDraggingLineEndpoint_ && impl_->renderer_) {
    auto layer = impl_->draggingLineLayer_.lock(); auto *line = layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
    if (line && line->shapeType() == ShapeType::Line) {
      const Detail::float2 canvas = impl_->renderer_->viewportToCanvas({static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())}); bool ok = false; const QTransform inv = line->getGlobalTransform().inverted(&ok);
      if (ok) { const QPointF p = inv.map(QPointF(canvas.x, canvas.y)); const QPointF fixed = impl_->draggingLineEndpoint_ == 0 ? QPointF(line->shapeWidth(), line->shapeHeight() * 0.5) : QPointF(0.0, line->shapeHeight() * 0.5); QPointF d = p - fixed; if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) { constexpr double kPi = 3.14159265358979323846; const double length = std::hypot(d.x(), d.y()); if (length > 0.0001) { const double angle = std::atan2(d.y(), d.x()); const double snappedAngle = std::round(angle / (kPi / 4.0)) * (kPi / 4.0); d = QPointF(std::cos(snappedAngle) * length, std::sin(snappedAngle) * length); } } const int w = std::max(1, static_cast<int>(std::round(std::hypot(d.x(), d.y())))); line->setSize(w, line->shapeHeight()); const auto time = gizmoTransformTime(layer, layer->currentFrame()); line->transform3D().setRotation(time, static_cast<float>(std::atan2(-d.y(), d.x()) * 180.0 / 3.14159265358979323846)); line->setDirty(LayerDirtyFlag::Transform); layer->changed(); impl_->invalidateBaseComposite(); impl_->invalidateOverlayComposite(); markRenderDirty(); }
    } return;
  }


  auto toolManager = ArtifactApplicationManager::instance()->toolManager();

  auto activeTool =

      toolManager ? toolManager->activeTool() : ToolType::Selection;

  if (impl_->rigWeightPainting_ && activeTool == ToolType::RigWeight &&
      impl_->renderer_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto layer = comp && !impl_->selectedLayerId_.isNil()
        ? comp->layerById(impl_->selectedLayerId_)
        : ArtifactAbstractLayerPtr{};
    if (auto *rigLayer = layer
            ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
            : nullptr) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      bool invertible = false;
      const QTransform inverse = rigLayer->getGlobalTransform().inverted(&invertible);
      if (invertible && rigLayer->rig2D().skinMesh()) {
        const QPointF localPoint = inverse.map(QPointF(canvas.x, canvas.y));
        auto *mesh = rigLayer->rig2D().skinMesh();
        auto vertices = mesh->vertices();
        const float radius = std::max(1.0f, impl_->rigWeightRadius_);
        const QPointF strokeStart = ArtifactCore::Coordinates::toQPointF(
            impl_->rigWeightLastLocalPoint_);
        const float strokeLength = static_cast<float>(QLineF(
            strokeStart, localPoint).length());
        const int sampleCount = std::clamp(
            static_cast<int>(std::ceil(strokeLength / std::max(1.0f, radius * 0.35f))),
            1, 64);
        const bool subtract = QGuiApplication::keyboardModifiers().testFlag(
            Qt::ControlModifier);
        for (auto &vertex : vertices) {
          const QPointF vertexPoint(vertex.position.x(), vertex.position.y());
          float distance = std::numeric_limits<float>::max();
          for (int sample = 0; sample <= sampleCount; ++sample) {
            const float t = static_cast<float>(sample) /
                static_cast<float>(sampleCount);
            const QPointF strokePoint = strokeStart +
                (localPoint - strokeStart) * t;
            distance = std::min(distance, static_cast<float>(QLineF(
                strokePoint, vertexPoint).length()));
          }
          if (distance > radius) continue;
          const float falloff = 1.0f - distance / radius;
          const float delta = impl_->rigWeightOpacity_ *
              impl_->rigWeightFlow_ * falloff;
          int slot = -1;
          for (int i = 0; i < 4; ++i) {
            if (vertex.boneIndices[i] == impl_->rigWeightBoneIndex_) {
              slot = i;
              break;
            }
          }
          if (slot < 0) {
            slot = 0;
            for (int i = 1; i < 4; ++i) {
              if (vertex.weights[i] < vertex.weights[slot]) slot = i;
            }
            vertex.boneIndices[slot] = static_cast<int16_t>(
                impl_->rigWeightBoneIndex_);
            vertex.weights[slot] = 0.0f;
          }
          vertex.weights[slot] = std::clamp(
              vertex.weights[slot] + (subtract ? -delta : delta), 0.0f, 1.0f);
          float total = 0.0f;
          for (float weight : vertex.weights) total += weight;
          if (total > 0.0001f) {
            for (float &weight : vertex.weights) weight /= total;
          }
        }
        mesh->setVertices(vertices);
        rigLayer->setDirty(LayerDirtyFlag::Transform);
        rigLayer->changed();
        impl_->invalidateBaseComposite();
        impl_->invalidateOverlayComposite();
        impl_->publishLayerModified(layer, true);
        markRenderDirty();
        impl_->rigWeightLastLocalPoint_ =
            ArtifactCore::Coordinates::layerLocalPoint2FromQPointF(localPoint);
      }
    }
    return;
  }

  if (impl_->rigSelectionDragging_ && impl_->renderer_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto layer = comp && !impl_->selectedLayerId_.isNil()
        ? comp->layerById(impl_->selectedLayerId_)
        : ArtifactAbstractLayerPtr{};
    if (auto *rigLayer = layer
            ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
            : nullptr) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      bool invertible = false;
      const QTransform inverse = rigLayer->getGlobalTransform().inverted(&invertible);
      if (invertible) {
        const QPointF localPoint = inverse.map(QPointF(canvas.x, canvas.y));
        const QPointF dragStartLocalPoint =
            ArtifactCore::Coordinates::toQPointF(
                impl_->rigDragStartLocalPoint_);
        if (!impl_->selectedRigBoneId_.isNil()) {
          if (auto *bone = rigLayer->rig2D().findBone(impl_->selectedRigBoneId_)) {
          const QPointF origin(bone->localTransform().position.x(),
                               bone->localTransform().position.y());
          const auto angleForPoint = [&origin](const QPointF &point) {
            return static_cast<float>(std::atan2(point.x() - origin.x(),
                                                 -(point.y() - origin.y())) *
                                      180.0 / 3.14159265358979323846);
          };
          float rotation = impl_->rigDragStartRotation_ +
              angleForPoint(localPoint) -
              angleForPoint(dragStartLocalPoint);
          if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            rotation = std::round(rotation / 15.0f) * 15.0f;
          }
          auto transform = bone->localTransform();
          transform.rotation = rotation;
          if (rigLayer->setRigBoneLocalTransform(bone->id(), transform)) {
            if (rigLayer->rig2D().rootBone()) {
              rigLayer->rig2D().rootBone()->updateHierarchy();
            }
            rigLayer->setDirty(LayerDirtyFlag::Transform);
            rigLayer->changed();
            impl_->invalidateBaseComposite();
            impl_->invalidateOverlayComposite();
            impl_->publishLayerModified(layer, true);
            markRenderDirty();
          }
          }
        } else if (!impl_->selectedRigControlId_.isNil()) {
          if (auto *control = rigLayer->rig2D().findControl(
                  impl_->selectedRigControlId_)) {
            QVariant nextValue = impl_->rigDragStartControlValue_;
            if (control->kind() == RigControlKind::Point) {
              QPointF point = localPoint;
              if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
                const QPointF delta = localPoint - dragStartLocalPoint;
                if (std::abs(delta.x()) >= std::abs(delta.y())) {
                  point.setY(dragStartLocalPoint.y());
                } else {
                  point.setX(dragStartLocalPoint.x());
                }
              }
              nextValue = QVariant::fromValue(QVector2D(
                  static_cast<float>(point.x()),
                  static_cast<float>(point.y())));
            } else if (control->kind() == RigControlKind::Slider) {
              const float range = std::max(1.0f, 100.0f);
              const float delta = static_cast<float>(
                  localPoint.x() - dragStartLocalPoint.x()) / range;
              const double start = impl_->rigDragStartControlValue_.toDouble();
              const double minValue = control->minValue().toDouble();
              const double maxValue = control->maxValue().toDouble();
              nextValue = std::clamp(start + delta * (maxValue - minValue),
                                     minValue, maxValue);
            } else if (control->kind() == RigControlKind::Angle) {
              const QPointF delta = localPoint - QPointF(0.0, 0.0);
              const float angle = static_cast<float>(std::atan2(
                  delta.y(), delta.x()) * 180.0 / 3.14159265358979323846);
              const float snappedAngle =
                  QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)
                      ? std::round(angle / 15.0f) * 15.0f
                      : angle;
              const double minValue = control->minValue().toDouble();
              const double maxValue = control->maxValue().toDouble();
              nextValue = std::clamp(static_cast<double>(snappedAngle), minValue,
                                     maxValue);
            }
            control->setValue(nextValue);
            // Re-evaluate bindings and constraints through the layer's normal
            // frame path so control edits reach their bound properties.
            rigLayer->goToFrame(rigLayer->currentFrame());
            rigLayer->setDirty(LayerDirtyFlag::Transform);
            rigLayer->changed();
            impl_->invalidateBaseComposite();
            impl_->invalidateOverlayComposite();
            impl_->publishLayerModified(layer, true);
            markRenderDirty();
          }
        }
      }
    }
    return;
  }

  if (activeTool == ToolType::Clone && impl_->cloneStampDragging_ &&
      impl_->renderer_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                            : ArtifactAbstractLayerPtr{};
    auto *paintLayer = layer ? dynamic_cast<ArtifactPaintLayer *>(layer.get()) : nullptr;
    if (paintLayer) {
      const auto canvas = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      const ArtifactCore::Coordinates::CompositionPoint2 destination{
          canvas.x, canvas.y};
      const QPointF destinationQt =
          ArtifactCore::Coordinates::toQPointF(destination);
      const QPointF delta = destinationQt -
          ArtifactCore::Coordinates::toQPointF(
              impl_->cloneStampStartCanvas_);
      auto *brushTool = ArtifactApplicationManager::instance()->brushTool();
      const QPointF source = brushTool && brushTool->cloneAligned()
          ? ArtifactCore::Coordinates::toQPointF(
                impl_->cloneStampSourceCanvas_) + delta
          : ArtifactCore::Coordinates::toQPointF(
                impl_->cloneStampSourceCanvas_);
      if (QLineF(destinationQt, ArtifactCore::Coordinates::toQPointF(
                                    impl_->cloneStampLastCanvas_)).length() >=
          1.0) {
        if (auto sourceLayer = impl_->cloneStampSourceLayer_.lock()) {
          auto *sourcePaintLayer =
              dynamic_cast<ArtifactPaintLayer *>(sourceLayer.get());
          QPointF sourcePaintPosition;
          QPointF destinationPaintPosition;
          if (compositionToPaintPosition(sourcePaintLayer, source,
                                         &sourcePaintPosition) &&
              compositionToPaintPosition(paintLayer, destinationQt,
                                         &destinationPaintPosition)) {
            const FramePosition sourceFrame(
                (sourcePaintLayer ? sourcePaintLayer->currentFrame() : 0) +
                (brushTool ? brushTool->cloneTimeOffset() : 0));
            paintLayer->applyCloneStampFromLayerAtFrame(
                sourcePaintLayer, sourcePaintPosition,
                destinationPaintPosition,
                brushTool ? brushTool->radius() : 20.0f,
                brushTool ? brushTool->opacity() : 1.0f,
                brushTool ? brushTool->hardness() : 1.0f, false, sourceFrame);
          }
        }
        impl_->cloneStampLastCanvas_ = destination;
        impl_->invalidateBaseComposite();
        markRenderDirty();
      }
    }
    impl_->brushCursorCanvasPos_ = impl_->cloneStampLastCanvas_;
    impl_->brushCursorVisible_ = true;
    impl_->invalidateOverlayComposite();
    return;
  }

  if ((activeTool == ToolType::Brush || activeTool == ToolType::RotoBrush ||
       activeTool == ToolType::Eraser) &&
      impl_->renderer_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto layer = comp ? comp->layerById(impl_->selectedLayerId_)
                            : ArtifactAbstractLayerPtr{};
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()),
         static_cast<float>(viewportPos.y())});
    impl_->brushCursorCanvasPos_ = {canvas.x, canvas.y};
    impl_->brushLastViewportPos_ = screenPhysicalPoint(viewportPos);
    impl_->brushCursorVisible_ = true;
    if (layer) {
      if (auto *brushTool = ArtifactApplicationManager::instance()->brushTool()) {
        brushTool->mouseMoveEvent(layer, {canvas.x, canvas.y});
      }
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (activeTool == ToolType::Text && impl_->textToolCandidate_ &&
      impl_->renderer_) {
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    impl_->textToolCurrentCanvas_ = {canvas.x, canvas.y};
    const QPointF delta(
        impl_->textToolCurrentCanvas_.x - impl_->textToolStartCanvas_.x,
        impl_->textToolCurrentCanvas_.y - impl_->textToolStartCanvas_.y);
    if (delta.manhattanLength() >= 6.0) {
      impl_->textToolDragging_ = true;
    }
    if (impl_->textToolDragging_) {
      impl_->invalidateOverlayComposite();
      markRenderDirty();
      return;
    }
  }

  bool needsRender = impl_->updateColorSamplerOverlay(this, viewportPos);

  if (impl_->designReorderActive_) {
    const auto comp = impl_->previewPipeline_.composition();
    const auto parent = comp ? comp->layerById(impl_->designReorderParentId_)
                             : ArtifactAbstractLayerPtr{};
    const auto directionProperty = parent
                                       ? parent->getProperty(
                                             QStringLiteral("component.layout.stackDirection"))
                                       : nullptr;
    const bool vertical = directionProperty &&
                          directionProperty->getValue().toInt() != 0;
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    const qreal pointerAxis = vertical ? static_cast<qreal>(canvas.y)
                                       : static_cast<qreal>(canvas.x);
    if (comp) {
      const auto layers = comp->allLayer();
      qreal closestDistance = std::numeric_limits<qreal>::max();
      int closestIndex = impl_->designReorderTargetIndex_;
      for (int index = 0; index < layers.size(); ++index) {
        const auto &sibling = layers.at(index);
        if (!sibling || sibling->parentLayerId() != impl_->designReorderParentId_) {
          continue;
        }
        if (isAbsoluteLayoutLayer(sibling)) {
          continue;
        }
        const QRectF bounds = sibling->getGlobalTransform().mapRect(
            sibling->localBounds());
        const qreal siblingAxis = vertical ? bounds.center().y()
                                           : bounds.center().x();
        const qreal distance = std::abs(pointerAxis - siblingAxis);
        if (distance < closestDistance) {
          closestDistance = distance;
          closestIndex = index;
        }
      }
      if (closestIndex != impl_->designReorderTargetIndex_) {
        impl_->designReorderTargetIndex_ = closestIndex;
        setInfoOverlayText(QStringLiteral("Design"),
                           QStringLiteral("Reorder target %1")
                               .arg(closestIndex + 1));
        impl_->invalidateOverlayComposite();
        markRenderDirty();
      }
    }
    return;
  }



  if (impl_->isShapeVertexMarqueeSelecting_) {
    impl_->shapeVertexMarqueeCurrentViewportPos_ =
        screenPhysicalPoint(viewportPos);
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }

  if (impl_->isRubberBandSelecting_) {

    impl_->rubberBandCurrentViewportPos_ = screenPhysicalPoint(viewportPos);
    if (impl_->isLassoSelecting_ &&
        (impl_->lassoViewportPoints_.isEmpty() ||
         QLineF(ArtifactCore::Coordinates::toQPointF(
                    impl_->lassoViewportPoints_.back()),
                viewportPos).length() >= 3.0)) {
      impl_->lassoViewportPoints_.push_back(screenPhysicalPoint(viewportPos));
    }

    if (needsRender || impl_->isRubberBandSelecting_) {

      markRenderDirty();

    }

    return;

  }



  if (impl_->isDraggingShapePathVertex_) {
    updateShapePathVertexDrag(screenPhysicalPoint(viewportPos));
    notifyViewportInteractionActivity();
    return;
  }
  // F2: parameter and polygon drags own their sessions.
  if (impl_->shapeParamDragMode_ != 0) {
    updateShapeParamDrag(screenPhysicalPoint(viewportPos));
    notifyViewportInteractionActivity();
    return;
  }
  // F5: operator handle drag owns its session.
  if (impl_->shapeOpDragOp_ >= 0) {
    updateShapeOperatorDrag(screenPhysicalPoint(viewportPos));
    notifyViewportInteractionActivity();
    return;
  }
  if (impl_->isDraggingShapePolygon_) {
    updateShapePolygonDrag(screenPhysicalPoint(viewportPos));
    notifyViewportInteractionActivity();
    return;
  }
  // F1: shape vertex/tangent/segment hover for the overlay emphasis.
  // Read-only; drag ownership and mask/pen paths are untouched.
  if (!impl_->isDraggingLineEndpoint_ && activeTool != ToolType::Pen) {
    updateShapePathHover(screenPhysicalPoint(viewportPos));
  }  if (impl_->isDraggingCameraPoi_) {
    if (impl_->renderer_) {
      const WorldRay poiRay = createPickingRay(
          screenPhysicalPoint(viewportPos * impl_->devicePixelRatio_));
      ArtifactCore::Coordinates::WorldPoint3 nextPoi{};
      if (impl_->applyCameraPoiDrag(poiRay, nextPoi)) {
        if (auto layer = impl_->draggingCameraPoiLayer_.lock()) {
          if (auto *camera =
                  dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
            if (!camera->setPointOfInterestWorld(nextPoi)) return;
            impl_->publishLayerModified(layer, true);
            notifyViewportInteractionActivity();
            markRenderDirty();
            return;
          }
        }
      }
    }
  }

  if (impl_->isDraggingPastPlaneFrame_) {
    const Ray ray = pickingLegacyRayFromWorldRay(createPickingRay(
        screenPhysicalPoint(viewportPos * impl_->devicePixelRatio_)));
    if (impl_->applyPastPlaneFrameDrag(ray)) {
      markRenderDirty();
      return;
    }
  }

  if (impl_->historicalScaleDragging_) {
    auto layer = impl_->historicalScaleLayer_.lock();
    if (!impl_->renderer_ || !historicalBoundsSupported(layer) || !impl_->showMotionPathOverlay_ ||
        layer->id() != impl_->selectedLayerId_) {
      cancelGizmoInteraction();
      return;
    }
    const auto cursor = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
    const auto local = impl_->historicalScaleInverse_.map(QPointF(cursor.x, cursor.y)) -
                       impl_->historicalScaleAnchor_;
    double x = local.x() / impl_->historicalScaleStart_.x();
    double y = local.y() / impl_->historicalScaleStart_.y();
    if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
      x = y = QPointF::dotProduct(local, impl_->historicalScaleStart_) /
              QPointF::dotProduct(impl_->historicalScaleStart_, impl_->historicalScaleStart_);
    }
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    // Keep the original reflection and avoid collapsing the transform to zero.
    x = std::clamp(x, 0.001, 1000.0);
    y = std::clamp(y, 0.001, 1000.0);
    if (!impl_->historicalScaleChanged_ && std::abs(x - 1.0) < 0.00001 &&
        std::abs(y - 1.0) < 0.00001) return;
    auto after = impl_->historicalScaleBefore_;
    after.present[0] = after.present[1] = true;
    after.values[0] = after.values[0].toDouble() * x;
    after.values[1] = after.values[1].toDouble() * y;
    if (applyMotionPathScale(layer, impl_->historicalScaleTime_, after)) {
      impl_->historicalScaleChanged_ = true;
      impl_->motionPathCache_.valid = false;
      impl_->invalidateBaseComposite();
      impl_->invalidateOverlayComposite();
      setInfoOverlayText(QStringLiteral("Motion Path"),
          QStringLiteral("Frame %1 · Scale %2% / %3%")
              .arg(impl_->historicalScaleFrame_)
              .arg(after.values[0].toDouble() * 100.0, 0, 'f', 1)
              .arg(after.values[1].toDouble() * 100.0, 0, 'f', 1));
      markRenderDirty();
    }
    return;
  }

  if (impl_->isDraggingMotionPathTangent_) {
    if (impl_->renderer_) {
      const auto canvasPos = impl_->renderer_->viewportToCanvas(
          {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
      if (impl_->applyMotionPathTangentDrag(QPointF(canvasPos.x, canvasPos.y))) {
        markRenderDirty();
        return;
      }
    }
  }

  if (impl_->isDraggingMotionPathKeyframe_) {

    auto comp = impl_->previewPipeline_.composition();

    if (comp && impl_->renderer_) {

      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);

      if (selectedLayer) {

        const auto canvasPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        if (impl_->applyMotionPathDrag(

                selectedLayer,
                ArtifactCore::Coordinates::CompositionPoint2{
                    canvasPos.x, canvasPos.y})) {

          impl_->invalidateMotionPathCache();

          markRenderDirty();

          return;

        }

      }

    }

  }



  if (impl_->showMotionPathOverlay_ && !impl_->isDraggingMotionPathKeyframe_) {

    auto comp = impl_->previewPipeline_.composition();

    if (comp && impl_->renderer_) {

      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);

      if (selectedLayer) {

        const auto cPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        const auto motionPathSamples = buildMotionPathSamples(selectedLayer, comp);

        MotionPathSample hoverSample;

        const float hoverThreshold =

            std::max(8.0f, 12.0f / impl_->renderer_->getZoom());

        bool hoverChanged = false;

        int64_t tangentHoverFrame = 0;
        MotionPathTangentHandle tangentHoverHandle =
            MotionPathTangentHandle::None;
        const bool tangentHovered = impl_->hitTestMotionPathTangent(
            QPointF(cPos.x, cPos.y), hoverThreshold, tangentHoverFrame,
            tangentHoverHandle);

        if (tangentHovered) {
          impl_->hoveredMotionPathTangentHandle_ = tangentHoverHandle;
          hoverChanged = impl_->setHoveredMotionPathFrame(
              static_cast<int>(tangentHoverFrame));
          if (hoverChanged) {
            ArtifactCore::PositionSpatialTangents hoverTangents;
            const bool hasHoverTangents =
                selectedLayer->transform3D().positionKeyFrameSpatialTangentsAt(
                    gizmoTransformTime(selectedLayer, tangentHoverFrame),
                    hoverTangents);
            setInfoOverlayText(
                QStringLiteral("Motion Path • %1 Tangent • Frame %2")
                    .arg(tangentHoverHandle == MotionPathTangentHandle::In
                             ? QStringLiteral("In")
                             : QStringLiteral("Out"))
                    .arg(tangentHoverFrame),
                QStringLiteral("%1 • Alt+drag to %2")
                    .arg(hasHoverTangents && hoverTangents.linked
                             ? QStringLiteral("Linked")
                             : QStringLiteral("Broken"))
                    .arg(hasHoverTangents && hoverTangents.linked
                             ? QStringLiteral("break link")
                             : QStringLiteral("keep independent")));
          }
        } else if (hitTestMotionPathSample(motionPathSamples,
                                           QPointF(cPos.x, cPos.y),

                                    hoverThreshold, hoverSample)) {
          impl_->hoveredMotionPathTangentHandle_ =
              MotionPathTangentHandle::None;

          hoverChanged = impl_->setHoveredMotionPathFrame(

              static_cast<int>(hoverSample.framePosition));
          if (hoverChanged) {
            const auto interpolation = static_cast<ArtifactCore::InterpolationType>(
                motionPathPositionInterpolation(
                    selectedLayer,
                    gizmoTransformTime(selectedLayer, hoverSample.framePosition)));
            QString interpolationName = QStringLiteral("Linear");
            switch (interpolation) {
            case ArtifactCore::InterpolationType::Constant:
              interpolationName = QStringLiteral("Hold");
              break;
            case ArtifactCore::InterpolationType::Bezier:
              interpolationName = QStringLiteral("Bezier");
              break;
            case ArtifactCore::InterpolationType::EaseIn:
              interpolationName = QStringLiteral("Ease In");
              break;
            case ArtifactCore::InterpolationType::EaseOut:
              interpolationName = QStringLiteral("Ease Out");
              break;
            case ArtifactCore::InterpolationType::EaseInOut:
              interpolationName = QStringLiteral("Ease In-Out");
              break;
            default:
              break;
            }
            setInfoOverlayText(
                QStringLiteral("Motion Path • Frame %1")
                    .arg(hoverSample.framePosition),
                QStringLiteral("Position %1, %2 • %3")
                    .arg(hoverSample.position.x(), 0, 'f', 1)
                    .arg(hoverSample.position.y(), 0, 'f', 1)
                    .arg(interpolationName));
          }

        } else {
          impl_->hoveredMotionPathTangentHandle_ =
              MotionPathTangentHandle::None;

          hoverChanged = impl_->setHoveredMotionPathFrame(-1);
          if (hoverChanged) {
            clearInfoOverlayText();
          }

        }

        if (hoverChanged) {

          needsRender = true;

        }

      } else {

        if (impl_->setHoveredMotionPathFrame(-1)) {

          needsRender = true;

        }

      }

    }

  } else if (impl_->hoveredMotionPathFrame_ != -1) {

    if (impl_->setHoveredMotionPathFrame(-1)) {

      needsRender = true;

    }

  }

  if (activeTool == ToolType::Selection &&
      QApplication::keyboardModifiers().testFlag(Qt::AltModifier) &&
      impl_->renderer_ &&
      !impl_->isDraggingTransformField_) {
    auto comp = impl_->previewPipeline_.composition();
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
    if (!selectedLayerIds.isEmpty() && comp) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      const float hitThreshold =
          14.0f * Accessibility::targetScale() /
          impl_->renderer_->getZoom();
      if (impl_->updateTransformFieldHover(comp, QPointF(cPos.x, cPos.y),
                                          selectedLayerIds, hitThreshold)) {
        needsRender = true;
      }
    } else if (!impl_->hoveredTransformFieldId_.isEmpty() ||
               impl_->hoveredTransformFieldMode_ != TransformFieldDragMode::None) {
      impl_->clearTransformFieldHoverState();
      needsRender = true;
    }
  } else if (!impl_->hoveredTransformFieldId_.isEmpty() ||
             impl_->hoveredTransformFieldMode_ != TransformFieldDragMode::None) {
    impl_->clearTransformFieldHoverState();
    needsRender = true;
  }


  // MotionSketch tool: capture samples during drag

  if (activeTool == ToolType::MotionSketch && impl_->renderer_) {

    auto* app = ArtifactApplicationManager::instance();

    if (app && app->motionSketchTool() && app->motionSketchTool()->isSketching()) {

      const auto cPos = impl_->renderer_->viewportToCanvas(

          {(float)viewportPos.x(), (float)viewportPos.y()});

      app->motionSketchTool()->addSample(QPointF(cPos.x, cPos.y));
      impl_->motionSketchLastCanvasPos_ = {cPos.x, cPos.y};

      markRenderDirty();

      return;

    }

  }



  // Puppet tool: drag pin

  if (activeTool == ToolType::Puppet && impl_->renderer_) {

    auto* app = ArtifactApplicationManager::instance();

    if (app && app->puppetTool()) {

      QString selId = app->puppetTool()->selectedPinId();

      if (!selId.isEmpty()) {

        const auto cPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        const QPointF canvasPos(cPos.x, cPos.y);
        if (impl_->puppetRotationDragging_ &&
            impl_->puppetRotationPinId_ == selId) {
          const QPointF pinPos = app->puppetTool()->pinPosition(selId);
          const float angle = static_cast<float>(
              std::atan2(canvasPos.y() - pinPos.y(),
                         canvasPos.x() - pinPos.x()) *
              180.0 / 3.14159265358979323846);
          float targetRotation = angle - impl_->puppetRotationStartAngle_;
          if (QGuiApplication::keyboardModifiers().testFlag(
                  Qt::ShiftModifier)) {
            targetRotation = std::round(targetRotation / 15.0f) * 15.0f;
          }
          app->puppetTool()->setPinRotation(
              selId, targetRotation);
        } else {
          QPointF constrainedPos = canvasPos;
          if (impl_->puppetPinDragging_ &&
              QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            const QPointF delta = canvasPos -
                ArtifactCore::Coordinates::toQPointF(
                    impl_->puppetPinDragStartCanvas_);
            if (std::abs(delta.x()) >= std::abs(delta.y())) {
              constrainedPos.setX(impl_->puppetPinDragStartPosition_.x() + delta.x());
              constrainedPos.setY(impl_->puppetPinDragStartPosition_.y());
            } else {
              constrainedPos.setX(impl_->puppetPinDragStartPosition_.x());
              constrainedPos.setY(impl_->puppetPinDragStartPosition_.y() + delta.y());
            }
          }
          app->puppetTool()->movePinAtFrame(selId, constrainedPos);
        }

        auto comp = impl_->previewPipeline_.composition();

        if (comp && !impl_->selectedLayerId_.isNil()) {

          app->puppetTool()->deformLayer(impl_->selectedLayerId_, impl_->renderer_.get());

        }

        markRenderDirty();

        return;

      }

    }

  }

  if (impl_->isDraggingTransformField_ && impl_->renderer_) {
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)viewportPos.x(), (float)viewportPos.y()});
      if (impl_->applyTransformFieldDrag(comp, QPointF(cPos.x, cPos.y))) {
        notifyViewportInteractionActivity();
        markRenderDirty();
        return;
      }
    }
  }

if (activeTool == ToolType::Pen && impl_->maskRubberBandCandidate_ &&
    impl_->renderer_) {
    const auto current = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    impl_->maskRubberBandCurrentCanvas_ = {current.x, current.y};
    const QPointF delta =
        ArtifactCore::Coordinates::toQPointF(
            impl_->maskRubberBandCurrentCanvas_) -
        ArtifactCore::Coordinates::toQPointF(
            impl_->maskRubberBandStartCanvas_);
    // delta is in canvas coordinates. Keep the physical drag threshold stable,
    // otherwise a small click jitter cancels a point while zoomed out.
    const float rubberBandThreshold = 6.0f /
        std::max(0.001f, impl_->renderer_->getZoom());
    if (delta.manhattanLength() >= rubberBandThreshold) {
      impl_->clearPendingMaskCreation();
      impl_->isDraggingMaskHandle_ = false;
      impl_->maskRubberBandCandidate_ = false;
      impl_->isMaskRubberBandSelecting_ = true;
      impl_->selectedMaskVertices_.clear();
      markRenderDirty();
    }
    if (impl_->isMaskRubberBandSelecting_) {
      markRenderDirty();
      return;
    }
}

if (activeTool == ToolType::Pen && impl_->isDraggingMaskGeometry_ &&
    impl_->renderer_) {
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (selectedLayer && impl_->draggingMaskIndex_ >= 0 &&
      impl_->draggingMaskIndex_ < selectedLayer->maskCount() &&
      impl_->draggingPathIndex_ >= 0) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {(float)viewportPos.x(), (float)viewportPos.y()});
    const QTransform globalTransform = selectedLayer->getGlobalTransform();
    bool invertible = false;
    const QPointF localPos = globalTransform.inverted(&invertible).map(
        QPointF(cPos.x, cPos.y));
    if (invertible) {
      LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);
      if (impl_->draggingPathIndex_ < mask.maskPathCount()) {
        MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
        const QPointF geometryStartLocal =
            ArtifactCore::Coordinates::toQPointF(
                impl_->draggingMaskGeometryStartLocal_);
        const ArtifactCore::Units::LayerLocalLength delta{
            static_cast<float>(-(localPos.y() - geometryStartLocal.y()))};
        if (impl_->draggingMaskGeometryExpansion_) {
              path.setExpansion({
                  impl_->draggingMaskGeometryStartExpansion_.value + delta.value});
        } else {
          path.setFeather({std::max(
              0.0f,
              impl_->draggingMaskGeometryStartFeather_.value + delta.value)});
        }
        mask.setMaskPath(impl_->draggingPathIndex_, path);
        selectedLayer->setMask(impl_->draggingMaskIndex_, mask);
        impl_->markMaskEditDirty();
        impl_->publishLayerModified(selectedLayer);
        impl_->invalidateOverlayComposite();
        markRenderDirty();
        return;
      }
    }
  }
}

if (activeTool == ToolType::Pen && impl_->isDraggingVertex_) {

    auto comp = impl_->previewPipeline_.composition();

    if (comp && impl_->renderer_) {

      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);

      if (selectedLayer) {

        const auto cPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        ArtifactCore::Coordinates::CompositionPoint2 snappedCanvasPoint{
            cPos.x, cPos.y};
        if (impl_->gridSettings_.snapToGrid) {
          snappedCanvasPoint = snapCanvasToGrid(snappedCanvasPoint);
        }
        QPointF snappedCanvasPos =
            ArtifactCore::Coordinates::toQPointF(snappedCanvasPoint);
        const Qt::KeyboardModifiers vertexModifiers =
            QGuiApplication::keyboardModifiers();
        if (impl_->snapToGuides_ &&
            vertexModifiers.testFlag(Qt::ControlModifier) &&
            !vertexModifiers.testFlag(Qt::AltModifier)) {
          const float threshold =
              8.0f / std::max(0.001f, impl_->renderer_->getZoom());
          const std::array<float, 3> xGuides{
              0.0f, impl_->lastCanvasWidth_ * 0.5f, impl_->lastCanvasWidth_};
          const std::array<float, 3> yGuides{
              0.0f, impl_->lastCanvasHeight_ * 0.5f, impl_->lastCanvasHeight_};
          for (const float guide : xGuides) {
            if (std::abs(snappedCanvasPos.x() - guide) <= threshold) {
              snappedCanvasPos.setX(guide);
              break;
            }
          }
          for (const float guide : yGuides) {
            if (std::abs(snappedCanvasPos.y() - guide) <= threshold) {
              snappedCanvasPos.setY(guide);
              break;
            }
          }
          float nearestDistance = threshold;
          QPointF nearestVertex;
          const QTransform snapTransform = selectedLayer->getGlobalTransform();
          for (int maskIndex = 0; maskIndex < selectedLayer->maskCount();
               ++maskIndex) {
            const LayerMask candidateMask = selectedLayer->mask(maskIndex);
            for (int pathIndex = 0;
                 pathIndex < candidateMask.maskPathCount(); ++pathIndex) {
              const MaskPath candidatePath = candidateMask.maskPath(pathIndex);
              for (int vertexIndex = 0;
                   vertexIndex < candidatePath.vertexCount(); ++vertexIndex) {
                if (maskIndex == impl_->draggingMaskIndex_ &&
                    pathIndex == impl_->draggingPathIndex_ &&
                    vertexIndex == impl_->draggingVertexIndex_) {
                  continue;
                }
                const auto selected = ArtifactCore::artifactMakeTuple(maskIndex, pathIndex,
                                                       vertexIndex);
                if (std::find(impl_->selectedMaskVertices_.begin(),
                              impl_->selectedMaskVertices_.end(),
                              selected) !=
                    impl_->selectedMaskVertices_.end()) {
                  continue;
                }
                const QPointF candidateCanvas = snapTransform.map(
                    candidatePath.vertex(vertexIndex).position);
                const float distance = static_cast<float>(
                    QLineF(snappedCanvasPos, candidateCanvas).length());
                if (distance < nearestDistance) {
                  nearestDistance = distance;
                  nearestVertex = candidateCanvas;
                }
              }
            }
          }
          if (nearestDistance < threshold) {
            snappedCanvasPos = nearestVertex;
          }
        }

        const QTransform globalTransform = selectedLayer->getGlobalTransform();

        bool invertible = false;

        const QTransform invTransform = globalTransform.inverted(&invertible);



        if (invertible) {

          const QPointF localPos = invTransform.map(snappedCanvasPos);

          LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);

          MaskPath path = mask.maskPath(impl_->draggingPathIndex_);

          MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);



          const QPointF previousPosition = vertex.position;
          QPointF constrainedLocalPos = localPos;
          if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            const QPointF dragStartVertexLocal =
                ArtifactCore::Coordinates::toQPointF(
                    impl_->dragStartVertexLocal_);
            const QPointF delta = localPos - dragStartVertexLocal;
            const double distance = std::hypot(delta.x(), delta.y());
            if (distance > 0.0001) {
              constexpr double kPi = 3.14159265358979323846;
              const double angle = std::atan2(delta.y(), delta.x());
              const double snapped =
                  std::round(angle / (kPi / 4.0)) * (kPi / 4.0);
              constrainedLocalPos =
              dragStartVertexLocal + QPointF(
                      std::cos(snapped) * distance,
                      std::sin(snapped) * distance);
            }
          }
          vertex.position = constrainedLocalPos;

          path.setVertex(impl_->draggingVertexIndex_, vertex);

          mask.setMaskPath(impl_->draggingPathIndex_, path);

          selectedLayer->setMask(impl_->draggingMaskIndex_, mask);

          const QPointF previewCanvasPos =
              globalTransform.map(constrainedLocalPos);
          impl_->penMaskPreviewCanvasPos_ = {
              static_cast<float>(previewCanvasPos.x()),
              static_cast<float>(previewCanvasPos.y())};
          impl_->penMaskPreviewValid_ = true;

          const QPointF delta = constrainedLocalPos - previousPosition;
          for (const auto &selectedAddress :
               impl_->selectedMaskVertices_) {
            const int maskIndex = ArtifactCore::artifactGet<0>(selectedAddress);
            const int pathIndex = ArtifactCore::artifactGet<1>(selectedAddress);
            const int vertexIndex = ArtifactCore::artifactGet<2>(selectedAddress);
            if (maskIndex == impl_->draggingMaskIndex_ &&
                pathIndex == impl_->draggingPathIndex_ &&
                vertexIndex == impl_->draggingVertexIndex_) {
              continue;
            }
            if (maskIndex < 0 || maskIndex >= selectedLayer->maskCount()) {
              continue;
            }
            LayerMask otherMask = selectedLayer->mask(maskIndex);
            if (pathIndex < 0 || pathIndex >= otherMask.maskPathCount()) {
              continue;
            }
            MaskPath otherPath = otherMask.maskPath(pathIndex);
            if (vertexIndex < 0 || vertexIndex >= otherPath.vertexCount()) {
              continue;
            }
            MaskVertex otherVertex = otherPath.vertex(vertexIndex);
            otherVertex.position += delta;
            otherPath.setVertex(vertexIndex, otherVertex);
            otherMask.setMaskPath(pathIndex, otherPath);
            selectedLayer->setMask(maskIndex, otherMask);
          }

          if (impl_->maskProportionalEditingEnabled_ &&
              impl_->maskProportionalEditRadius_ > 0.0f) {
            const QPointF pivotCanvas = globalTransform.map(previousPosition);
            const float radius = impl_->maskProportionalEditRadius_;
            for (int maskIndex = 0; maskIndex < selectedLayer->maskCount(); ++maskIndex) {
              LayerMask otherMask = selectedLayer->mask(maskIndex);
              for (int pathIndex = 0; pathIndex < otherMask.maskPathCount(); ++pathIndex) {
                MaskPath otherPath = otherMask.maskPath(pathIndex);
                for (int vertexIndex = 0; vertexIndex < otherPath.vertexCount(); ++vertexIndex) {
                  if (maskIndex == impl_->draggingMaskIndex_ &&
                      pathIndex == impl_->draggingPathIndex_ &&
                      vertexIndex == impl_->draggingVertexIndex_) continue;
                  const auto selected = ArtifactCore::artifactMakeTuple(maskIndex, pathIndex, vertexIndex);
                  if (std::find(impl_->selectedMaskVertices_.begin(),
                                impl_->selectedMaskVertices_.end(), selected) !=
                      impl_->selectedMaskVertices_.end()) continue;
                  MaskVertex otherVertex = otherPath.vertex(vertexIndex);
                  const QPointF otherCanvas = globalTransform.map(otherVertex.position);
                  const float distance = static_cast<float>(
                      QLineF(pivotCanvas, otherCanvas).length());
                  if (distance >= radius) continue;
                  const float t = std::clamp(distance / radius, 0.0f, 1.0f);
                  const QPointF weightedDelta = delta * ((1.0f - t) * (1.0f - t));
                  otherVertex.position += weightedDelta;
                  otherPath.setVertex(vertexIndex, otherVertex);
                }
                otherMask.setMaskPath(pathIndex, otherPath);
              }
              selectedLayer->setMask(maskIndex, otherMask);
            }
          }

          impl_->markMaskEditDirty();

          impl_->publishLayerModified(selectedLayer);

          return;

        }

      }

    }

  }



  if (activeTool == ToolType::Pen && impl_->isDraggingMaskHandle_) {

    auto comp = impl_->previewPipeline_.composition();

    if (comp && impl_->renderer_) {

      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);

      if (selectedLayer) {

        const auto cPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        const QTransform globalTransform = selectedLayer->getGlobalTransform();

        bool invertible = false;

        const QTransform invTransform = globalTransform.inverted(&invertible);



        if (invertible) {

          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));

          const auto handleType =

              static_cast<MaskEditHandleType>(impl_->draggingMaskHandleType_);

          const bool breakTangents =

              QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);



          if (impl_->draggingMaskIndex_ == -1 &&

              impl_->pendingMaskCreation_ &&

              impl_->draggingVertexIndex_ >= 0 &&

              impl_->draggingVertexIndex_ <

                  impl_->pendingMaskPath_.vertexCount()) {

            MaskVertex vertex =

                impl_->pendingMaskPath_.vertex(impl_->draggingVertexIndex_);

            setMaskVertexHandle(vertex, handleType, localPos - vertex.position,

                                breakTangents);

            impl_->pendingMaskPath_.setVertex(impl_->draggingVertexIndex_,

                                              vertex);

            impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};

            impl_->penMaskPreviewValid_ = true;

            impl_->invalidateOverlayComposite();

            markRenderDirty();

            return;

          }



          if (impl_->draggingMaskIndex_ >= 0 &&

              impl_->draggingPathIndex_ >= 0 &&

              impl_->draggingVertexIndex_ >= 0) {

            LayerMask mask = selectedLayer->mask(impl_->draggingMaskIndex_);

            MaskPath path = mask.maskPath(impl_->draggingPathIndex_);

            if (handleType == MaskEditHandleType::FeatherHandle) {

              const MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);

              const QPointF delta = localPos - vertex.position;
              ArtifactCore::Units::LayerLocalLength feather{
                  static_cast<float>(std::hypot(delta.x(), delta.y()))};
              if (impl_->draggingMaskHandleStartFeather_.value <= 0.01f) {
                const int vertexCount = path.vertexCount();
                const int vertexIndex = impl_->draggingVertexIndex_;
                const int previous = vertexIndex > 0 ? vertexIndex - 1
                    : (path.isClosed() ? vertexCount - 1 : vertexIndex);
                const int next = vertexIndex + 1 < vertexCount ? vertexIndex + 1
                    : (path.isClosed() ? 0 : vertexIndex);
                QPointF tangent = path.vertex(next).position -
                    path.vertex(previous).position;
                const qreal length = std::hypot(tangent.x(), tangent.y());
                if (length > 1e-6) {
                  tangent /= length;
                  const QPointF normal(-tangent.y(), tangent.x());
                  feather.value = std::max(0.0f, static_cast<float>(
                      QPointF::dotProduct(
                          localPos - ArtifactCore::Coordinates::toQPointF(
                              impl_->draggingMaskHandleStartLocal_),
                          normal)));
                }
              }
              path.setFeather(feather);

              mask.setMaskPath(impl_->draggingPathIndex_, path);

              selectedLayer->setMask(impl_->draggingMaskIndex_, mask);

              impl_->markMaskEditDirty();

              impl_->publishLayerModified(selectedLayer);

              return;

            }

            MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);

            setMaskVertexHandle(vertex, handleType, localPos - vertex.position,

                                breakTangents);

            path.setVertex(impl_->draggingVertexIndex_, vertex);

            mask.setMaskPath(impl_->draggingPathIndex_, path);

            selectedLayer->setMask(impl_->draggingMaskIndex_, mask);

            impl_->markMaskEditDirty();

            impl_->publishLayerModified(selectedLayer);

            return;

          }

        }

      }

    }

  }



  if (impl_->rectangleToolDragging_) {

    const auto cPos = impl_->renderer_->viewportToCanvas(

        {(float)viewportPos.x(), (float)viewportPos.y()});

    QPointF current(cPos.x, cPos.y);
    const QPointF start = ArtifactCore::Coordinates::toQPointF(
        impl_->rectangleToolStartCanvasPos_);
    const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    if (modifiers.testFlag(Qt::AltModifier)) {
      current = start + (current - start) * 2.0;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
      const QPointF delta = current - start;
      const double side = std::max(std::abs(delta.x()), std::abs(delta.y()));
      current = start + QPointF(delta.x() < 0.0 ? -side : side,
                                delta.y() < 0.0 ? -side : side);
    }
    impl_->rectangleToolCurrentCanvasPos_ =
        ArtifactCore::Coordinates::compositionPointFromQPointF(current);

    markRenderDirty();

    return;

  }



  // Hover detection for Pen tool

  if (activeTool == ToolType::Pen) {

    impl_->penToolPreviewVisible_ = true;

    const int prevHoveredMaskIndex = impl_->hoveredMaskIndex_;

    const int prevHoveredPathIndex = impl_->hoveredPathIndex_;

    const int prevHoveredVertexIndex = impl_->hoveredVertexIndex_;

    const int prevHoveredMaskHandleType = impl_->hoveredMaskHandleType_;

    impl_->hoveredMaskIndex_ = -1;

    impl_->hoveredPathIndex_ = -1;

    impl_->hoveredVertexIndex_ = -1;

    impl_->hoveredMaskHandleType_ = -1;



    auto comp = impl_->previewPipeline_.composition();

    if (comp && impl_->renderer_) {

      auto selectedLayer = comp->layerById(impl_->selectedLayerId_);

      if (selectedLayer) {

        const auto cPos = impl_->renderer_->viewportToCanvas(

            {(float)viewportPos.x(), (float)viewportPos.y()});

        const QTransform globalTransform = selectedLayer->getGlobalTransform();

        bool invertible = false;

        const QTransform invTransform = globalTransform.inverted(&invertible);



        if (invertible) {

          const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));

          const float hitThreshold =
              14.0f * Accessibility::targetScale();

          impl_->penMaskPreviewCanvasPos_ = {cPos.x, cPos.y};

          impl_->penMaskPreviewValid_ = true;



          int handleMaskIndex = -1;

          int handlePathIndex = -1;

          int handleVertexIndex = -1;

          MaskEditHandleType handleType = MaskEditHandleType::None;

          if (hitTestMaskHandle(selectedLayer, impl_->renderer_.get(), viewportPos,

                                hitThreshold, handleMaskIndex, handlePathIndex,

                                handleVertexIndex, handleType)) {

            impl_->hoveredMaskIndex_ = handleMaskIndex;

            impl_->hoveredPathIndex_ = handlePathIndex;

            impl_->hoveredVertexIndex_ = handleVertexIndex;

            impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);

          } else {

            for (int m = 0; m < selectedLayer->maskCount(); ++m) {

              LayerMask mask = selectedLayer->mask(m);

              if (mask.isLocked())

                continue;

              for (int p = 0; p < mask.maskPathCount(); ++p) {

                MaskPath path = mask.maskPath(p);

                for (int v = 0; v < path.vertexCount(); ++v) {

                  MaskVertex vertex = path.vertex(v);

                  if (QVector2D(vertex.position - localPos).length() <

                      hitThreshold) {

                    impl_->hoveredMaskIndex_ = m;

                    impl_->hoveredPathIndex_ = p;

                    impl_->hoveredVertexIndex_ = v;

                    break;

                  }

               }

                if (impl_->hoveredVertexIndex_ != -1)

                  break;

              }

              if (impl_->hoveredVertexIndex_ != -1)

                break;

            }

          }

        }

      }

    }

    if (prevHoveredMaskIndex != impl_->hoveredMaskIndex_ ||

        prevHoveredPathIndex != impl_->hoveredPathIndex_ ||

        prevHoveredVertexIndex != impl_->hoveredVertexIndex_ ||

        prevHoveredMaskHandleType != impl_->hoveredMaskHandleType_) {

      impl_->invalidateOverlayComposite();

      needsRender = true;

    }

  } else {

    impl_->penToolPreviewVisible_ = false;

    impl_->penMaskPreviewValid_ = false;

  }



  // Unified frame gizmo interaction (GIZ-2, GIZ-3).

  if (impl_->gizmo3D_) {

    auto comp3D = impl_->previewPipeline_.composition();

    auto sel3DLayer = (!impl_->selectedLayerId_.isNil() && comp3D)

                          ? comp3D->layerById(impl_->selectedLayerId_)

                          : ArtifactAbstractLayerPtr{};

    if (sel3DLayer &&
        impl_->gizmoOwnerForLayer(sel3DLayer) == GizmoOwner::Transform3D) {

      QPointF dragViewportPos = viewportPos;
      const auto dragModifiers = QGuiApplication::keyboardModifiers();
      const bool projectedFrameDrag = impl_->projectedFrameMove_ ||
          impl_->projectedFrameHandle_ != TransformGizmo::HandleType::None;
      const auto moveAxis = impl_->gizmo3D_->activeAxis();
      const bool axisMoveSnap = impl_->gizmo3D_->isDragging() &&
          impl_->gizmo3D_->mode() == GizmoMode::Move &&
          (moveAxis == GizmoAxis::Screen ||
           moveAxis == GizmoAxis::X || moveAxis == GizmoAxis::Y || moveAxis == GizmoAxis::Z);
      impl_->projectedFrameSnapVerticalValid_ = false;
      impl_->projectedFrameSnapHorizontalValid_ = false;
      if ((projectedFrameDrag || axisMoveSnap) && impl_->projectedFrameLastPointerValid_ &&
          impl_->projectedFrameHandle_ != TransformGizmo::HandleType::Rotate &&
          impl_->snapToGuides_ &&
          !impl_->viewportOrientationNavigator_.isAnimating() &&
          !dragModifiers.testFlag(Qt::AltModifier)) {
        const QMatrix4x4 &snapView = impl_->gizmoUndoSnapshotValid_
            ? impl_->gizmoDragViewMatrix_
            : impl_->gizmo3DCameraMatricesValid_
                  ? impl_->gizmo3DViewMatrix_
                  : impl_->renderer_->getViewMatrix();
        const QMatrix4x4 &snapProjection =
            impl_->gizmoUndoSnapshotValid_
                ? impl_->gizmoDragProjectionMatrix_
                : impl_->gizmo3DCameraMatricesValid_
                      ? impl_->gizmo3DProjectionMatrix_
                      : impl_->renderer_->getProjectionMatrix();
        const QRect snapViewport(
            0, 0, std::max(1, static_cast<int>(impl_->hostWidth_)),
            std::max(1, static_cast<int>(impl_->hostHeight_)));
        const bool constrained = axisMoveSnap && moveAxis != GizmoAxis::Screen;
        QPointF direction;
        if (constrained) {
          const QVector3D origin = ArtifactCore::Coordinates::toQVector3D(
              impl_->gizmo3D_->position());
          const QVector3D p = ViewportMath::projectToTopDown(origin, snapView, snapProjection, snapViewport);
          const QVector3D q = ViewportMath::projectToTopDown(
              origin + ArtifactCore::Coordinates::toQVector3D(
                           impl_->gizmo3D_->dragAxisDirection()),
              snapView, snapProjection, snapViewport);
          if (std::isfinite(p.x()) && std::isfinite(p.y()) &&
              std::isfinite(q.x()) && std::isfinite(q.y())) {
            direction = QPointF(q.x() - p.x(), q.y() - p.y());
          }
        }
        ProjectedFrameSnapResult snapResult = snapProjectedFramePointer(
            comp3D, sel3DLayer, screenPhysicalPoint(viewportPos),
            impl_->projectedFrameLastPointer_,
            impl_->projectedFrameMove_ || axisMoveSnap,
            impl_->projectedFrameHandle_,
            snapView, snapProjection, snapViewport, impl_->renderer_.get(),
            impl_->devicePixelRatio_, impl_->projectedFrameSnapCache_, direction, constrained);
        dragViewportPos = qPointFromScreenPhysicalPoint(snapResult.pointer);
        impl_->projectedFrameSnapVerticalValid_ =
            snapResult.verticalGuideValid;
        impl_->projectedFrameSnapHorizontalValid_ =
            snapResult.horizontalGuideValid;
        impl_->projectedFrameSnapVertical_ = snapResult.verticalGuide;
        impl_->projectedFrameSnapHorizontal_ = snapResult.horizontalGuide;
        impl_->invalidateOverlayComposite();
      } else {
        impl_->projectedFrameSnapCache_.release();
      }
      // mouseMoveEvent already converted this pointer to physical pixels.
      WorldRay ray = createPickingRay(screenPhysicalPoint(dragViewportPos));

      if (impl_->gizmo3D_->isDragging()) {

        notifyViewportInteractionActivity();

        const auto modifiers = QGuiApplication::keyboardModifiers();
        impl_->gizmo3D_->setInteractionModifiers(
            modifiers.testFlag(Qt::ControlModifier) &&
                !modifiers.testFlag(Qt::AltModifier),
            modifiers.testFlag(Qt::ShiftModifier));

        impl_->gizmo3D_->updateDrag(ray);

        // AnchorPoint owns its own result channel: the gizmo reports a world
        // offset instead of moving position, so it is applied here and the
        // generic position/scale/rotation write-back further down is gated off
        // for this frame.  Pointer bookkeeping is shared with the other drags.
        const bool anchorDrag = impl_->gizmo3D_->activeAxis() == GizmoAxis::Anchor;
        if (anchorDrag) {
          applyProjectedFrameAnchorDelta(impl_->gizmo3D_->anchorDragDelta());
        }

        if (anchorDrag || projectedFrameDrag || axisMoveSnap ||
            impl_->gizmoModalTransformActive_) {
          impl_->projectedFrameLastPointer_ =
              screenPhysicalPoint(dragViewportPos);
          impl_->projectedFrameLastPointerValid_ = true;
        }

        if (impl_->projectedFrameMove_ &&
            !impl_->gizmoModalTransformActive_ &&
            QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
          const QVector3D beforePosition =
              ArtifactCore::Coordinates::toQVector3D(
                  impl_->gizmoUndoWorldPivotBefore_);
          const QVector3D currentPosition =
              ArtifactCore::Coordinates::toQVector3D(
                  impl_->gizmo3D_->position());
          const QVector3D delta = currentPosition - beforePosition;
          QVector3D axisX = ArtifactCore::Coordinates::toQVector3D(
              impl_->projectedFrameStartAxisX_);
          QVector3D axisY = ArtifactCore::Coordinates::toQVector3D(
              impl_->projectedFrameStartAxisY_);
          if (axisX.lengthSquared() > 0.000001f &&
              axisY.lengthSquared() > 0.000001f) {
            axisX.normalize();
            axisY.normalize();
            const float alongX = QVector3D::dotProduct(delta, axisX);
            const float alongY = QVector3D::dotProduct(delta, axisY);
            const QVector3D constrained =
                std::abs(alongX) >= std::abs(alongY) ? axisX * alongX
                                                     : axisY * alongY;
            impl_->gizmo3D_->setTransform(
                ArtifactCore::Coordinates::worldPoint3FromQVector3D(
                    beforePosition + constrained),
                                          impl_->gizmo3D_->rotation());
          }
        }

        // Shift constrains projected-frame edge resizing to the initial
        // aspect ratio.  The 3D gizmo still resolves the pointer against the
        // camera plane; this post-step only mirrors the driven axis onto the
        // other local scale axis.
        const auto projectedHandle = impl_->projectedFrameHandle_;
        const bool projectedCorner =
            projectedHandle == TransformGizmo::HandleType::Scale_TL ||
            projectedHandle == TransformGizmo::HandleType::Scale_TR ||
            projectedHandle == TransformGizmo::HandleType::Scale_BL ||
            projectedHandle == TransformGizmo::HandleType::Scale_BR;
        const bool projectedEdge =
            projectedHandle == TransformGizmo::HandleType::Scale_T ||
            projectedHandle == TransformGizmo::HandleType::Scale_B ||
            projectedHandle == TransformGizmo::HandleType::Scale_L ||
            projectedHandle == TransformGizmo::HandleType::Scale_R;

        // Resolve projected-frame resizing in viewport space. The generic 3D
        // axis solver is correct for axis handles, but its ray/axis sign can
        // reverse at oblique views. Measuring the pointer from the fixed
        // opposite handle keeps top/bottom and left/right drags visually
        // consistent with the projected frame.
        if ((projectedCorner || projectedEdge) &&
            impl_->projectedFrameScalePointerBasisValid_) {
          const auto startVector =
              impl_->projectedFrameScaleStartPointer_ -
              impl_->projectedFrameScaleFixedPointer_;
          const auto currentVector =
              screenPhysicalPoint(dragViewportPos) -
              impl_->projectedFrameScaleFixedPointer_;
          const double startLengthSquared =
              startVector.x * startVector.x +
              startVector.y * startVector.y;
          if (startLengthSquared > 0.25) {
            const double rawFactor =
                (currentVector.x * startVector.x +
                 currentVector.y * startVector.y) /
                startLengthSquared;
            const ArtifactCore::Units::ScaleFactor factor{static_cast<float>(
                std::max(0.001, std::isfinite(rawFactor) ? rawFactor : 1.0))};
            auto resolvedScale = impl_->gizmoUndoBefore_.scale;
            if (projectedCorner) {
              resolvedScale.x = impl_->gizmoUndoBefore_.scale.x * factor.value;
              resolvedScale.y = impl_->gizmoUndoBefore_.scale.y * factor.value;
            } else if (projectedHandle ==
                           TransformGizmo::HandleType::Scale_T ||
                       projectedHandle ==
                           TransformGizmo::HandleType::Scale_B) {
              resolvedScale.y = impl_->gizmoUndoBefore_.scale.y * factor.value;
            } else {
              resolvedScale.x = impl_->gizmoUndoBefore_.scale.x * factor.value;
            }
            impl_->gizmo3D_->setScale(resolvedScale);
          }
        }

        if (projectedEdge && modifiers.testFlag(Qt::ShiftModifier) &&
            !modifiers.testFlag(Qt::ControlModifier)) {
          const auto beforeScale = impl_->gizmoUndoBefore_.scale;
          const auto currentScale = impl_->gizmo3D_->scale();
          auto constrainedScale = currentScale;
          if (projectedHandle == TransformGizmo::HandleType::Scale_T ||
              projectedHandle == TransformGizmo::HandleType::Scale_B) {
            const float denominator =
                std::abs(beforeScale.y) > 0.001f ? beforeScale.y : 0.001f;
            const ArtifactCore::Units::ScaleFactor ratio{
                currentScale.y / denominator};
            constrainedScale.x = beforeScale.x * ratio.value;
          } else {
            const float denominator =
                std::abs(beforeScale.x) > 0.001f ? beforeScale.x : 0.001f;
            const ArtifactCore::Units::ScaleFactor ratio{
                currentScale.x / denominator};
            constrainedScale.y = beforeScale.y * ratio.value;
          }
          if (std::isfinite(constrainedScale.x) &&
              std::isfinite(constrainedScale.y)) {
            impl_->gizmo3D_->setScale(constrainedScale);
          }
        }

        if (projectedCorner && modifiers.testFlag(Qt::ShiftModifier) &&
            !modifiers.testFlag(Qt::ControlModifier)) {
          const auto beforeScale = impl_->gizmoUndoBefore_.scale;
          const auto currentScale = impl_->gizmo3D_->scale();
          const float baseX = std::abs(beforeScale.x) > 0.001f
              ? beforeScale.x : 0.001f;
          const float baseY = std::abs(beforeScale.y) > 0.001f
              ? beforeScale.y : 0.001f;
          const ArtifactCore::Units::ScaleFactor ratioX{
              currentScale.x / baseX};
          const ArtifactCore::Units::ScaleFactor ratioY{
              currentScale.y / baseY};
          const auto ratio = std::abs(ratioX.value - 1.0f) >=
                                     std::abs(ratioY.value - 1.0f)
                                 ? ratioX
                                 : ratioY;
          if (std::isfinite(ratio.value)) {
            impl_->gizmo3D_->setScale({beforeScale.x * ratio.value,
                                       beforeScale.y * ratio.value,
                                       currentScale.z});
          }
        }

        // Keep projected-frame resizing from collapsing a dimension below one
        // local pixel.  The clamp is applied after the modifier-specific
        // scale calculation so Shift/Control anchoring remains authoritative.
        if (projectedCorner || projectedEdge) {
          const QRectF localBounds = sel3DLayer->localBounds();
          const auto currentScale = impl_->gizmo3D_->scale();
          const float minScaleX = localBounds.width() > 0.001
              ? 1.0f / static_cast<float>(localBounds.width())
              : 0.001f;
          const float minScaleY = localBounds.height() > 0.001
              ? 1.0f / static_cast<float>(localBounds.height())
              : 0.001f;
          const auto clampDimension = [](float value, float minimum) {
            const float sign = value < 0.0f ? -1.0f : 1.0f;
            return sign * std::max(minimum, std::abs(value));
          };
          const ArtifactCore::Units::Scale3 clampedScale{
              clampDimension(currentScale.x, minScaleX),
              clampDimension(currentScale.y, minScaleY), currentScale.z};
          if (clampedScale.x != currentScale.x ||
              clampedScale.y != currentScale.y ||
              clampedScale.z != currentScale.z) {
            impl_->gizmo3D_->setScale(clampedScale);
          }
        }

        // Single-layer frame scaling keeps the authored anchor fixed (AE).
        // Position must not be adjusted or keyed as a side effect of Scale.
        impl_->projectedFrameCorrectedLocalPositionValid_ = false;
        if ((projectedCorner || projectedEdge) &&
            !impl_->gizmoGroupTransformActive_) {
          impl_->projectedFrameCorrectedLocalPosition_ =
              impl_->gizmoLayerTransformBefore_.position;
          impl_->projectedFrameCorrectedLocalPositionValid_ = true;
          impl_->gizmo3D_->setTransform(
              impl_->projectedFrameStartWorldAnchor_,
              impl_->gizmo3D_->rotation());
        }

        // AE-style rotation snap for the 3D gizmo. Ctrl explicitly disables
        // the snap so a user can make a free rotation while keeping Shift
        // pressed for a constrained workflow.
        const Qt::KeyboardModifiers gizmoModifiers =
            QGuiApplication::keyboardModifiers();
        if (impl_->gizmo3D_->mode() == GizmoMode::Rotate &&
            gizmoModifiers.testFlag(Qt::ShiftModifier) &&
            !gizmoModifiers.testFlag(Qt::ControlModifier)) {
          const ArtifactCore::Units::EulerDegrees3 currentRotation =
              impl_->gizmo3D_->rotation();
          const auto snapDegrees = [](ArtifactCore::Units::Degrees degrees) {
            return ArtifactCore::Units::Degrees{
                std::round(degrees.value / 15.0f) * 15.0f};
          };
          impl_->gizmo3D_->setTransform(
              impl_->gizmo3D_->position(),
              {snapDegrees(currentRotation.x),
               snapDegrees(currentRotation.y),
               snapDegrees(currentRotation.z)});
        }



        // Update layer transform from gizmo

        auto comp = impl_->previewPipeline_.composition();

        if (comp && impl_->gizmoGroupTransformActive_ &&
            !impl_->gizmoGroupLayers_.empty()) {
          const auto gizmoWorldPosition = impl_->gizmo3D_->position();
          const QVector3D translation = ArtifactCore::Coordinates::toQVector3D(
              gizmoWorldPosition - impl_->gizmoGroupPivotBefore_);
          const auto currentGroupRotation = impl_->gizmo3D_->rotation();
          const ArtifactCore::Units::EulerDegrees3 rotationDeltaDegrees{
              currentGroupRotation.x - impl_->gizmoGroupRotationBefore_.x,
              currentGroupRotation.y - impl_->gizmoGroupRotationBefore_.y,
              currentGroupRotation.z - impl_->gizmoGroupRotationBefore_.z};
          const QVector3D rotationDelta =
              ArtifactCore::Units::toQVector3D(rotationDeltaDegrees);
          const auto gizmoScale = impl_->gizmo3D_->scale();
          const QVector3D groupBasisX =
              ArtifactCore::Coordinates::toQVector3D(
                  impl_->gizmoGroupBasisX_);
          const QVector3D groupBasisY =
              ArtifactCore::Coordinates::toQVector3D(
                  impl_->gizmoGroupBasisY_);
          const QVector3D groupBasisZ =
              ArtifactCore::Coordinates::toQVector3D(
                  impl_->gizmoGroupBasisZ_);
          const auto signedDenominator = [](float value) {
            return std::abs(value) > 0.001f ? value : 0.001f;
          };
          const ArtifactCore::Units::Scale3 groupScale{
              gizmoScale.x /
                  signedDenominator(impl_->gizmoGroupScaleBefore_.x),
              gizmoScale.y /
                  signedDenominator(impl_->gizmoGroupScaleBefore_.y),
              gizmoScale.z /
                  signedDenominator(impl_->gizmoGroupScaleBefore_.z)};
          const QQuaternion groupRotation =
              impl_->gizmoGroupProjectedBasisValid_
                  ? QQuaternion::fromAxisAndAngle(groupBasisZ,
                                                  rotationDelta.z())
                  : QQuaternion::fromEulerAngles(rotationDelta);

          for (const auto &state : impl_->gizmoGroupLayers_) {
            if (!state.layer) continue;
            const QVector3D startOffset = ArtifactCore::Coordinates::toQVector3D(
                state.worldAnchor - impl_->gizmoGroupPivotBefore_);
            const QVector3D scaledOffset =
                impl_->gizmoGroupProjectedBasisValid_
                    ? groupBasisX *
                          (QVector3D::dotProduct(
                               startOffset, groupBasisX) *
                           groupScale.x) +
                          groupBasisY *
                          (QVector3D::dotProduct(
                               startOffset, groupBasisY) *
                           groupScale.y) +
                          groupBasisZ *
                          (QVector3D::dotProduct(
                               startOffset, groupBasisZ) *
                           groupScale.z)
                    : QVector3D(startOffset.x() * groupScale.x,
                                startOffset.y() * groupScale.y,
                                startOffset.z() * groupScale.z);
            const auto newWorldAnchor =
                impl_->gizmoGroupPivotBefore_ +
                ArtifactCore::Coordinates::worldVectorFromQVector3D(
                    translation + groupRotation.rotatedVector(scaledOffset));
            GizmoTransformSnapshot current = state.before;
            if (state.parentWorldInvertible) {
              current.position =
                  ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                      state.parentWorldInverse.map(
                          ArtifactCore::Coordinates::toQVector3D(
                              newWorldAnchor)));
            }
            current.scale = {
                state.before.scale.x * groupScale.x,
                state.before.scale.y * groupScale.y,
                state.before.is3D
                    ? state.before.scale.z * groupScale.z
                    : state.before.scale.z};
            if (impl_->gizmo3D_->mode() == GizmoMode::Scale) {
              const QRectF memberBounds = state.layer->localBounds();
              const auto clampFrameScale = [](float value, qreal extent) {
                const float sign = value < 0.0f ? -1.0f : 1.0f;
                const float minimum = extent > 0.001
                    ? 1.0f / static_cast<float>(extent)
                    : 0.001f;
                return sign * std::max(minimum, std::abs(value));
              };
              current.scale.x =
                  clampFrameScale(current.scale.x, memberBounds.width());
              current.scale.y =
                  clampFrameScale(current.scale.y, memberBounds.height());
            }
            if (state.before.is3D) {
              current.rotation = {
                  state.before.rotation.x +
                      ArtifactCore::Units::Degrees{rotationDelta.x()},
                  state.before.rotation.y +
                      ArtifactCore::Units::Degrees{rotationDelta.y()},
                  state.before.rotation.z +
                      ArtifactCore::Units::Degrees{rotationDelta.z()}};
              if (std::abs(rotationDelta.z()) > 0.0001f &&
                  std::abs(rotationDelta.x()) <= 0.0001f &&
                  std::abs(rotationDelta.y()) <= 0.0001f) {
                current.rotation.x = state.before.rotation.x +
                    ArtifactCore::Units::Degrees{rotationDelta.z()};
              }
            } else {
              current.rotation.z = state.before.rotation.z +
                  ArtifactCore::Units::Degrees{rotationDelta.z()};
            }
            applyLiveGizmoTransform(state.layer, state.frame, state.before,
                                    current);
            ArtifactCore::globalEventBus().publish(
                LayerChangedEvent{comp->id().toString(),
                                  state.layer->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
          }
          impl_->invalidateBaseComposite();
          impl_->invalidateOverlayComposite();
          markRenderDirty();
          return;
        }

        if (comp && !impl_->selectedLayerId_.isNil() && !anchorDrag) {

          if (auto layer = comp->layerById(impl_->selectedLayerId_)) {

            if (!layer->is3D()) {
              const auto gizmoScale = impl_->gizmo3D_->scale();
              const auto gizmoRotation = impl_->gizmo3D_->rotation();
              const QVector3D gizmoPosition =
                  ArtifactCore::Coordinates::toQVector3D(
                      impl_->gizmo3D_->position());
              const auto clampScale = [](float value) {
                const float sign = value < 0.0f ? -1.0f : 1.0f;
                return sign * std::max(0.001f, std::abs(value));
              };
              const auto visualBefore = impl_->gizmoUndoBefore_.scale;
              const auto layerBefore =
                  impl_->gizmoLayerTransformBefore_.scale;
              GizmoTransformSnapshot current =
                  impl_->gizmoLayerTransformBefore_;
              if (impl_->projectedFrameHandle_ ==
                  TransformGizmo::HandleType::Rotate) {
                current.position =
                    impl_->gizmoLayerTransformBefore_.position;
              } else if (impl_->projectedFrameMove_) {
                if (impl_->projectedFrameParentWorldInvertible_) {
                  current.position =
                      ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                          impl_->projectedFrameParentWorldInverse_.map(
                              gizmoPosition));
                }
              } else if (impl_->projectedFrameCorrectedLocalPositionValid_) {
                current.position =
                    impl_->projectedFrameCorrectedLocalPosition_;
              } else {
                const auto worldDelta =
                    impl_->gizmo3D_->position() -
                    impl_->gizmoUndoWorldPivotBefore_;
                QVector3D localDelta =
                    ArtifactCore::Coordinates::toQVector3D(worldDelta);
                if (impl_->projectedFrameParentWorldInvertible_) {
                  localDelta = impl_->projectedFrameParentWorldInverse_.mapVector(
                      localDelta);
                  current.position = current.position +
                      ArtifactCore::Coordinates::layerLocalVector3FromQVector3D(
                          localDelta);
                }
              }
              current.rotation.z = gizmoRotation.z;
              current.scale.x = clampScale(
                  layerBefore.x * gizmoScale.x /
                  (std::abs(visualBefore.x) > 0.001f
                       ? visualBefore.x : 0.001f));
              current.scale.y = clampScale(
                  layerBefore.y * gizmoScale.y /
                  (std::abs(visualBefore.y) > 0.001f
                       ? visualBefore.y : 0.001f));
              applyLiveGizmoTransform(layer, impl_->gizmoUndoFrame_,
                                      impl_->gizmoLayerTransformBefore_,
                                      current);
            } else if (impl_->gizmo3D_->mode() == GizmoMode::Scale) {
              const auto scale = impl_->gizmo3D_->scale();
              const auto clampScale = [](float value) {
                const float sign = value < 0.0f ? -1.0f : 1.0f;
                return sign * std::max(0.001f, std::abs(value));
              };
              GizmoTransformSnapshot current =
                  impl_->gizmoLayerTransformBefore_;
              current.scale = {clampScale(scale.x), clampScale(scale.y),
                               clampScale(scale.z)};

              if (impl_->projectedFrameHandle_ !=
                  TransformGizmo::HandleType::None) {
                const ArtifactCore::Coordinates::LayerLocalPoint3 localPosition =
                    impl_->projectedFrameCorrectedLocalPositionValid_
                        ? impl_->projectedFrameCorrectedLocalPosition_
                        : ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                              impl_->projectedFrameParentWorldInvertible_
                                  ? impl_->projectedFrameParentWorldInverse_.map(
                                        ArtifactCore::Coordinates::toQVector3D(
                                            impl_->gizmo3D_->position()))
                                  : ArtifactCore::Coordinates::toQVector3D(
                                        impl_->gizmo3D_->position()));
                current.position = localPosition;
              }
              applyLiveGizmoTransform(layer, impl_->gizmoUndoFrame_,
                                      impl_->gizmoLayerTransformBefore_,
                                      current);
            } else {
              const QVector3D gizmoWorldPosition =
                  ArtifactCore::Coordinates::toQVector3D(
                      impl_->gizmo3D_->position());
              GizmoTransformSnapshot current =
                  impl_->gizmoLayerTransformBefore_;

              if (impl_->projectedFrameHandle_ ==
                  TransformGizmo::HandleType::Rotate) {
                const auto rotation = impl_->gizmo3D_->rotation();
                const ArtifactCore::Units::Degrees rotationDelta =
                    rotation.z - impl_->gizmoUndoBefore_.rotation.z;
                const auto beforeRotation =
                    impl_->gizmoLayerTransformBefore_.rotation;
                current.position = impl_->gizmoLayerTransformBefore_.position;
                current.rotation = {
                    beforeRotation.x + rotationDelta,
                    beforeRotation.y, beforeRotation.z};
              } else if (impl_->projectedFrameMove_) {
                if (impl_->projectedFrameParentWorldInvertible_) {
                  const QVector3D localPosition =
                      impl_->projectedFrameParentWorldInverse_.map(
                          gizmoWorldPosition);
                  current.position =
                      ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                          localPosition);
                }
              } else if (impl_->gizmo3D_->depthEnabled()) {
                if (const auto parent = layer->parentLayer()) {
                  bool parentWorldInvertible = false;
                  const QMatrix4x4 parentWorldInverse =
                      parent->getGlobalTransform4x4().inverted(
                          &parentWorldInvertible);
                  if (parentWorldInvertible) {
                    current.position =
                        ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                            parentWorldInverse.map(gizmoWorldPosition));
                  }
                } else {
                  current.position =
                      ArtifactCore::Coordinates::layerLocalPoint3FromQVector3D(
                          gizmoWorldPosition);
                }
              } else {
                current.position = {gizmoWorldPosition.x(),
                                    gizmoWorldPosition.y(),
                                    current.position.z};
              }

              if (impl_->projectedFrameHandle_ !=
                  TransformGizmo::HandleType::Rotate) {
                current.rotation = impl_->gizmo3D_->rotation();
              }
              applyLiveGizmoTransform(layer, impl_->gizmoUndoFrame_,
                                      impl_->gizmoLayerTransformBefore_,
                                      current);
            }
            impl_->invalidateBaseComposite();
            impl_->invalidateOverlayComposite();
            markRenderDirty();

            impl_->publishLayerModified(layer);

          }

        }

        return;

      } else {

        // Hover highlighting

        const auto previousFrameHover = impl_->projectedFrameHoverHandle_;
        impl_->projectedFrameHoverHandle_ =
            TransformGizmo::HandleType::None;
        if (activeTool != ToolType::AnchorPoint &&
            activeTool != ToolType::Pen &&
            (layerUsesProjectedFrameGizmo(sel3DLayer) ||
             impl_->viewportOrientationMatricesValid_) &&
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
          const bool combinedProjectedFrame = projectedSelection.size() > 1;
          if (combinedProjectedFrame) {
            const ScreenPhysicalBounds2 bounds = projectedSelectionFrameBounds(
                projectedSelection, frameView, frameProjection, frameViewport);
            impl_->projectedFrameHoverHandle_ = hitTestProjectedSelectionFrame(
                bounds, screenPhysicalPoint(viewportPos),
                32.0f * std::max(1.0f, impl_->devicePixelRatio_));
          } else {
            impl_->projectedFrameHoverHandle_ = hitTestProjectedFrameCorner(
                sel3DLayer, screenPhysicalPoint(viewportPos), frameView,
                frameProjection,
                frameViewport,
                32.0f * std::max(1.0f, impl_->devicePixelRatio_));
          }
          if (!projectedFrameHandleEnabled(
                  impl_->gizmoMode_, impl_->projectedFrameHoverHandle_)) {
            impl_->projectedFrameHoverHandle_ =
                TransformGizmo::HandleType::None;
          }
        }

        if (previousFrameHover != impl_->projectedFrameHoverHandle_) {
          impl_->invalidateOverlayComposite();
          needsRender = true;
        }

        const auto prevHoverAxis = impl_->gizmo3D_->hoverAxis();

        const QMatrix4x4& gizmoView = impl_->gizmo3DCameraMatricesValid_
                                          ? impl_->gizmo3DViewMatrix_
                                          : impl_->renderer_->getViewMatrix();
        const QMatrix4x4& gizmoProj = impl_->gizmo3DCameraMatricesValid_
                                          ? impl_->gizmo3DProjectionMatrix_
                                          : impl_->renderer_->getProjectionMatrix();
        impl_->gizmo3D_->hitTest(ray, gizmoView,
                                 gizmoProj);

        if (prevHoverAxis != impl_->gizmo3D_->hoverAxis()) {

          impl_->invalidateOverlayComposite();

          needsRender = true;

        }

      }

    } else if (impl_->projectedFrameHoverHandle_ !=
               TransformGizmo::HandleType::None) {
      impl_->projectedFrameHoverHandle_ = TransformGizmo::HandleType::None;
      impl_->invalidateOverlayComposite();
      needsRender = true;

    }

  }



  if (impl_->gizmoSession_.owner == GizmoOwner::Content) {

    impl_->contentGizmo_->handleMouseMove(
        impl_->planarGizmoPointer(viewportPos), impl_->renderer_.get());

    notifyViewportInteractionActivity();

    impl_->invalidateBaseComposite();

    markRenderDirty();

    return;

  }



  if (impl_->gizmoSession_.owner == GizmoOwner::Text) {

    impl_->textGizmo_->handleMouseMove(
        impl_->planarGizmoPointer(viewportPos), impl_->renderer_.get());

    notifyViewportInteractionActivity();

    impl_->invalidateBaseComposite();

    markRenderDirty();

    return;

  }



  if (impl_->gizmo_ &&
      (impl_->gizmoSession_.owner == GizmoOwner::None ||
       impl_->gizmoSession_.owner == GizmoOwner::Transform2D)) {

    const QPointF gizmoPointer =
        impl_->gizmoSession_.owner == GizmoOwner::Transform2D
            ? impl_->planarGizmoPointer(viewportPos) : viewportPos;
    impl_->gizmo_->handleMouseMove(gizmoPointer, impl_->renderer_.get());

    if (impl_->gizmo_->isDragging()) {

      notifyViewportInteractionActivity();

      impl_->invalidateBaseComposite();

      markRenderDirty();

      return;

    }

    const auto hovered = impl_->gizmo_->handleAtViewportPos(
        gizmoPointer, impl_->renderer_.get());
    const bool scaleHandle =
        hovered == TransformGizmo::HandleType::Scale_TL ||
        hovered == TransformGizmo::HandleType::Scale_TR ||
        hovered == TransformGizmo::HandleType::Scale_BL ||
        hovered == TransformGizmo::HandleType::Scale_BR ||
        hovered == TransformGizmo::HandleType::Scale_T ||
        hovered == TransformGizmo::HandleType::Scale_B ||
        hovered == TransformGizmo::HandleType::Scale_L ||
        hovered == TransformGizmo::HandleType::Scale_R;
    if (impl_->gizmo_->setHoverHandle(
            scaleHandle ? hovered : TransformGizmo::HandleType::None)) {
      impl_->invalidateOverlayComposite();
      needsRender = true;
    }

  }



  if (impl_->trackerGizmo_ && impl_->trackerGizmo_->isDragging()) {

    impl_->trackerGizmo_->handleMouseMove(viewportPos, impl_->renderer_.get());
    const auto &trackerState = impl_->trackerGizmo_->state();
    auto& config = ArtifactCore::LayeredConfigStore::instance();
    config.setValue(QStringLiteral("Viewport/TrackPoint/FeatureWidth"),
                    static_cast<double>(trackerState.innerHalfW * 2.0f));
    config.setValue(QStringLiteral("Viewport/TrackPoint/FeatureHeight"),
                    static_cast<double>(trackerState.innerHalfH * 2.0f));
    config.setValue(QStringLiteral("Viewport/TrackPoint/SearchWidth"),
                    static_cast<double>(trackerState.outerHalfW * 2.0f));
    config.setValue(QStringLiteral("Viewport/TrackPoint/SearchHeight"),
                    static_cast<double>(trackerState.outerHalfH * 2.0f));

    notifyViewportInteractionActivity();

    impl_->invalidateBaseComposite();

    markRenderDirty();

    return;

  }



  if (needsRender) {

    markRenderDirty();

  }

}

} // namespace Artifact
