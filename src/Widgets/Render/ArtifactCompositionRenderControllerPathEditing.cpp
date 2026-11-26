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
#include <QVariant>
#include <QWidget>
#include <QScreen>
#include <QtGlobal>

#include <QVector3D>
#include <QVector2D>

#include <QVector4D>

#include <QVector>
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

static void restoreLayerMasks(const ArtifactAbstractLayerPtr &layer,
                       const std::vector<LayerMask> &masks) {
  if (!layer) return;
  while (layer->maskCount() > 0) {
    layer->removeMask(layer->maskCount() - 1);
  }
  for (const auto &mask : masks) {
    layer->addMask(mask);
  }
  layer->changed();
}

static QRectF quickMaskPresetRect(QuickMaskPreset preset, const QRectF &bounds)

{

  const QRectF n = bounds.normalized();

  const qreal x = n.x();

  const qreal y = n.y();

  const qreal w = n.width();

  const qreal h = n.height();

  const qreal halfW = w * 0.5;

  const qreal halfH = h * 0.5;

  switch (preset) {

  case QuickMaskPreset::Full:

    return n;

  case QuickMaskPreset::LeftHalf:

    return QRectF(x, y, halfW, h);

  case QuickMaskPreset::RightHalf:

    return QRectF(x + halfW, y, halfW, h);

  case QuickMaskPreset::TopHalf:

    return QRectF(x, y, w, halfH);

  case QuickMaskPreset::BottomHalf:

    return QRectF(x, y + halfH, w, halfH);

  case QuickMaskPreset::CenterHalf:

    return QRectF(x + w * 0.25, y + h * 0.25, halfW, halfH);

  case QuickMaskPreset::QuarterTL:

    return QRectF(x, y, halfW, halfH);

  case QuickMaskPreset::QuarterTR:

    return QRectF(x + halfW, y, halfW, halfH);

  case QuickMaskPreset::Count:

    break;

  }

  return n;

}

} // namespace CompositionRenderControllerDetail

bool CompositionRenderController::hasPendingMaskEdit() const {

  return impl_ && (impl_->maskEditPending_ || impl_->pendingMaskCreation_);

}


bool CompositionRenderController::hasPendingShapePathCreation() const {

  return impl_ && impl_->pendingShapePathCreation_;

}

bool CompositionRenderController::finalizePendingShapePathCreation() {

  if (!impl_) {

    return false;

  }

  auto comp = impl_->previewPipeline_.composition();

  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};

  if (!impl_->finalizePendingShapePathCreation(selectedLayer)) {

    return false;

  }



  impl_->invalidateOverlayComposite();

  markRenderDirty();

  return true;

}

bool CompositionRenderController::removeLastPendingShapePathVertex() {

  if (!impl_) {

    return false;

  }

  const bool removed = impl_->removeLastPendingShapePathVertex();

  if (removed) {

    impl_->invalidateOverlayComposite();

    markRenderDirty();

  }

  return removed;

}

void CompositionRenderController::cancelPendingShapePathCreation() {

  if (!impl_) {

    return;

  }

  impl_->clearPendingShapePathCreation();

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

namespace {
// F4: Shift toggles, Ctrl adds, plain press replaces the vertex selection.
// Returns true when the selection changed. Stale indices are pruned first
// so vertex deletion cannot leave dangling selections behind.
bool applyShapeVertexSelectionGrammar(std::vector<int> &selected, int count,
                                       int hit) {
  selected.erase(std::remove_if(selected.begin(), selected.end(),
                                [count](int index) {
                                  return index < 0 || index >= count;
                                }),
                 selected.end());
  const auto modifiers = QGuiApplication::keyboardModifiers();
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    const auto it = std::find(selected.begin(), selected.end(), hit);
    if (it != selected.end()) {
      selected.erase(it);
    } else {
      selected.push_back(hit);
    }
    return true;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    if (std::find(selected.begin(), selected.end(), hit) == selected.end()) {
      selected.push_back(hit);
      return true;
    }
    return false;
  }
  if (selected.size() != 1 || selected.front() != hit) {
    selected.clear();
    selected.push_back(hit);
    return true;
  }
  return false;
}

// F2: parameter handle local positions mirror
// shapeCornerRadiusHandlePosition/shapeStarInnerRadiusHandlePosition so the
// main VP and Solo View agree without a new module dependency.
QPointF shapeParamHandleLocal(const ArtifactShapeLayer &shape, int mode) {  if (mode == 1) {
    const float corner = shape.cornerRadius();
    const float width = static_cast<float>(shape.shapeWidth());
    if (shape.shapeType() == ShapeType::Square) {
      const float height = static_cast<float>(shape.shapeHeight());
      const float side = std::min(width, height);
      return QPointF((width - side) * 0.5f + side - corner,
                     (height - side) * 0.5f);
    }
    return QPointF(width - corner, 0.0f);
  }
  const float outerRadius =
      std::min(shape.shapeWidth(), shape.shapeHeight()) * 0.5f;
  return QPointF(shape.shapeWidth() * 0.5f,
                 shape.shapeHeight() * 0.5f - outerRadius * shape.starInnerRadius());
}
} // namespace

bool CompositionRenderController::beginShapeParamDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape =
      selectedLayer ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || selectedLayer->isLocked() || selectedLayer->isSelectionLocked()) {
    return false;
  }
  int mode = 0;
  if (shape->shapeType() == ShapeType::Rect ||
      shape->shapeType() == ShapeType::Square) {
    mode = 1;
  } else if (shape->shapeType() == ShapeType::Star) {
    mode = 2;
  } else {
    return false;
  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QPointF canvasPoint(cPos.x, cPos.y);
  const QTransform globalTransform = selectedLayer->getGlobalTransform();
  const QPointF handleCanvas =
      globalTransform.map(shapeParamHandleLocal(*shape, mode));
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const float hitRadius = 12.0f / zoom;
  const float dx = static_cast<float>(handleCanvas.x() - canvasPoint.x());
  const float dy = static_cast<float>(handleCanvas.y() - canvasPoint.y());
  if (dx * dx + dy * dy > hitRadius * hitRadius) {
    return false;
  }
  impl_->shapeParamDragMode_ = mode;
  impl_->shapeParamDragLayer_ = selectedLayer;
  if (mode == 1) {
    impl_->shapeParamBefore_ = impl_->shapeParamStart_ = shape->cornerRadius();
    impl_->shapeParamMax_ =
        std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
  } else {
    impl_->shapeParamBefore_ = impl_->shapeParamStart_ = shape->starInnerRadius();
    impl_->shapeParamMax_ = 0.0f;
  }
  impl_->shapeParamAnchorX_ = static_cast<float>(viewportPos.x());
  markRenderDirty();
  return true;
}

void CompositionRenderController::updateShapeParamDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_ || impl_->shapeParamDragMode_ == 0) {
    return;
  }
  auto layer = impl_->shapeParamDragLayer_.lock();
  auto *shape =
      layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (!shape) {
    return;
  }
  if (impl_->shapeParamDragMode_ == 1) {
    const float delta =
        static_cast<float>(viewportPos.x()) - impl_->shapeParamAnchorX_;
    shape->setCornerRadius(std::clamp(impl_->shapeParamStart_ + delta * 0.5f,
                                      0.0f, impl_->shapeParamMax_));
  } else {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()),
         static_cast<float>(viewportPos.y())});
    bool invertible = false;
    const QTransform invTransform =
        layer->getGlobalTransform().inverted(&invertible);
    if (!invertible) {
      return;
    }
    const QPointF local = invTransform.map(QPointF(cPos.x, cPos.y));
    const float outerRadius =
        std::min(shape->shapeWidth(), shape->shapeHeight()) * 0.5f;
    const QPointF center(shape->shapeWidth() * 0.5, shape->shapeHeight() * 0.5);
    const float distance = static_cast<float>(QLineF(center, local).length());
    shape->setStarInnerRadius(outerRadius > 0.001f
                                  ? std::clamp(distance / outerRadius, 0.05f, 0.99f)
                                  : impl_->shapeParamStart_);
  }
  shape->setDirty(LayerDirtyFlag::Property);
  markRenderDirty();
}

void CompositionRenderController::endShapeParamDrag() {
  if (!impl_ || impl_->shapeParamDragMode_ == 0) {
    return;
  }
  const int mode = impl_->shapeParamDragMode_;
  impl_->shapeParamDragMode_ = 0;
  auto layer = impl_->shapeParamDragLayer_.lock();
  impl_->shapeParamDragLayer_.reset();
  auto *shape =
      layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (!shape) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }
  const float after =
      mode == 1 ? shape->cornerRadius() : shape->starInnerRadius();
  if (std::abs(after - impl_->shapeParamBefore_) > 0.001f) {
    auto *mgr = UndoManager::instance();
    bool pushed = false;
    if (mgr) {
      if (mode == 1) {
        pushed = mgr->push(std::make_unique<ShapeCornerRadiusUndoCommand>(
            layer, impl_->shapeParamBefore_, after));
      } else {
        pushed = mgr->push(std::make_unique<ShapeStarInnerRadiusUndoCommand>(
            layer, impl_->shapeParamBefore_, after));
      }
    }
    if (!pushed && mgr) {
      if (mode == 1) {
        shape->setCornerRadius(impl_->shapeParamBefore_);
      } else {
        shape->setStarInnerRadius(impl_->shapeParamBefore_);
      }
      shape->setDirty(LayerDirtyFlag::Property);
      shape->changed();
    } else {
      impl_->publishLayerModified(layer, true);
    }
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isEditingShapeParam() const {
  return impl_ && impl_->shapeParamDragMode_ != 0;
}

bool CompositionRenderController::beginShapePolygonDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape =
      selectedLayer ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || !shape->hasCustomPolygon() || selectedLayer->isLocked() ||
      selectedLayer->isSelectionLocked()) {
    return false;
  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QPointF canvasPoint(cPos.x, cPos.y);
  const QTransform globalTransform = selectedLayer->getGlobalTransform();
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const float hitRadius = 12.0f / zoom;
  auto points = shape->customPolygonPoints();
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    const QPointF worldPos = globalTransform.map(points[static_cast<size_t>(i)]);
    const float dx = static_cast<float>(worldPos.x() - canvasPoint.x());
    const float dy = static_cast<float>(worldPos.y() - canvasPoint.y());
    if (dx * dx + dy * dy <= hitRadius * hitRadius) {
      // F4: press updates the vertex selection grammar before the drag.
      applyShapeVertexSelectionGrammar(impl_->selectedShapePathVertices_,
                                       static_cast<int>(points.size()), i);
      impl_->isDraggingShapePolygon_ = true;
      impl_->draggingShapePolygonIndex_ = i;
      impl_->shapePolygonDragLayer_ = selectedLayer;
      impl_->shapePolygonBefore_ = points;
      impl_->shapePolygonBeforeClosed_ = shape->customPolygonClosed();
      impl_->shapePolygonDragDirty_ = false;
      markRenderDirty();
      return true;
    }
  }
  // Shift-click on a segment inserts a vertex and starts dragging it.
  if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) &&
      points.size() >= 2) {
    const float segRadius = 10.0f / zoom;
    const bool closed = shape->customPolygonClosed();
    const int segCount = closed ? static_cast<int>(points.size())
                                : static_cast<int>(points.size()) - 1;
    float best = segRadius * segRadius;
    int bestSeg = -1;
    double bestT = 0.0;
    for (int s = 0; s < segCount; ++s) {
      const int next = (s + 1) % static_cast<int>(points.size());
      const QPointF a = globalTransform.map(points[static_cast<size_t>(s)]);
      const QPointF b = globalTransform.map(points[static_cast<size_t>(next)]);
      const QPointF ab = b - a;
      const double len2 = QPointF::dotProduct(ab, ab);
      double t = 0.0;
      if (len2 > 1e-9) {
        t = QPointF::dotProduct(canvasPoint - a, ab) / len2;
        t = std::clamp(t, 0.0, 1.0);
      }
      const QPointF proj = a + ab * t;
      const float dx = static_cast<float>(proj.x() - canvasPoint.x());
      const float dy = static_cast<float>(proj.y() - canvasPoint.y());
      const float d2 = dx * dx + dy * dy;
      if (d2 <= best) {
        best = d2;
        bestSeg = s;
        bestT = t;
      }
    }
    if (bestSeg >= 0) {
      if (!globalTransform.isInvertible()) {
        return false;
      }
      const int next = (bestSeg + 1) % static_cast<int>(points.size());
      const QPointF localMid = (points[static_cast<size_t>(bestSeg)] +
                                points[static_cast<size_t>(next)]) *
                               0.5;
      (void)bestT;
      impl_->shapePolygonBefore_ = points;
      impl_->shapePolygonBeforeClosed_ = shape->customPolygonClosed();
      points.insert(points.begin() + bestSeg + 1, localMid);
      shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
      shape->setDirty(LayerDirtyFlag::Source);
      impl_->isDraggingShapePolygon_ = true;
      impl_->draggingShapePolygonIndex_ = bestSeg + 1;
      impl_->shapePolygonDragLayer_ = selectedLayer;
      impl_->shapePolygonDragDirty_ = true;
      // F4: the inserted vertex becomes the sole selection.
      impl_->selectedShapePathVertices_.clear();
      impl_->selectedShapePathVertices_.push_back(bestSeg + 1);
      markRenderDirty();
      return true;
    }
  }
  return false;
}

void CompositionRenderController::updateShapePolygonDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_ || !impl_->isDraggingShapePolygon_) {
    return;
  }
  auto layer = impl_->shapePolygonDragLayer_.lock();
  auto *shape =
      layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (!shape) {
    return;
  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  bool invertible = false;
  const QTransform invTransform =
      layer->getGlobalTransform().inverted(&invertible);
  if (!invertible) {
    return;
  }
  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
  auto points = shape->customPolygonPoints();
  const int index = impl_->draggingShapePolygonIndex_;
  if (index < 0 || index >= static_cast<int>(points.size())) {
    return;
  }
  const auto selected = impl_->selectedShapePathVertices_;
  const bool moveSelection = selected.size() > 1 &&
      std::find(selected.begin(), selected.end(), index) != selected.end() &&
      index < static_cast<int>(impl_->shapePolygonBefore_.size());
  if (moveSelection) {
    const QPointF delta =
        localPos - impl_->shapePolygonBefore_[static_cast<size_t>(index)];
    points = impl_->shapePolygonBefore_;
    for (const int selectedIndex : selected) {
      if (selectedIndex >= 0 &&
          selectedIndex < static_cast<int>(points.size())) {
        points[static_cast<size_t>(selectedIndex)] =
            impl_->shapePolygonBefore_[static_cast<size_t>(selectedIndex)] +
            delta;
      }
    }
  } else {
    points[static_cast<size_t>(index)] = localPos;
  }
  shape->setCustomPolygonPoints(points, shape->customPolygonClosed());
  shape->setDirty(LayerDirtyFlag::Source);
  impl_->shapePolygonDragDirty_ = true;
  markRenderDirty();
}

void CompositionRenderController::endShapePolygonDrag() {
  if (!impl_ || !impl_->isDraggingShapePolygon_) {
    return;
  }
  impl_->isDraggingShapePolygon_ = false;
  impl_->draggingShapePolygonIndex_ = -1;
  auto layer = impl_->shapePolygonDragLayer_.lock();
  impl_->shapePolygonDragLayer_.reset();
  auto *shape =
      layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (layer && shape && impl_->shapePolygonDragDirty_) {
    auto *mgr = UndoManager::instance();
    const auto afterPoints = shape->customPolygonPoints();
    const bool afterClosed = shape->customPolygonClosed();
    const bool pushed = !mgr || mgr->push(
        std::make_unique<ShapePolygonPointsUndoCommand>(
            layer, impl_->shapePolygonBefore_, afterPoints,
            impl_->shapePolygonBeforeClosed_, afterClosed));
    if (!pushed) {
      shape->setCustomPolygonPoints(impl_->shapePolygonBefore_,
                                    impl_->shapePolygonBeforeClosed_);
      shape->setDirty(LayerDirtyFlag::Source);
      shape->changed();
    } else {
      impl_->publishLayerModified(layer, true);
    }
  }
  impl_->shapePolygonBefore_.clear();
  impl_->shapePolygonDragDirty_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isEditingShapePolygon() const {
  return impl_ && impl_->isDraggingShapePolygon_;
}

bool CompositionRenderController::deleteSelectedShapePathVertices() {
  if (!impl_ || impl_->isDraggingShapePathVertex_ ||
      impl_->isDraggingShapePolygon_ || impl_->shapeParamDragMode_ != 0 ||
      impl_->shapeOpDragOp_ >= 0 ||
      impl_->selectedShapePathVertices_.empty()) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || selectedLayer->isLocked() || selectedLayer->isSelectionLocked()) {
    return false;
  }
  auto *mgr = UndoManager::instance();
  if (shape->hasCustomPath()) {
    const auto before = shape->customPathVertices();
    const bool beforeClosed = shape->customPathClosed();
    // Delete descending so earlier indices stay valid.
    auto selected = impl_->selectedShapePathVertices_;
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    auto after = before;
    for (const int index : selected) {
      if (index >= 0 && index < static_cast<int>(after.size())) {
        after.erase(after.begin() + index);
      }
    }
    if (after.size() == before.size()) {
      return false;
    }
    if (after.size() >= 2) {
      shape->setCustomPathVertices(after, beforeClosed);
    } else {
      shape->clearCustomPath();
    }
    shape->setDirty(LayerDirtyFlag::Source);
    const auto applied = shape->customPathVertices();
    const bool pushed = !mgr || mgr->push(
        std::make_unique<ShapePathVertexEditCommand>(
            selectedLayer, before, applied, beforeClosed,
            shape->customPathClosed()));
    if (!pushed) {
      if (before.size() >= 2) {
        shape->setCustomPathVertices(before, beforeClosed);
      } else {
        shape->clearCustomPath();
      }
      shape->setDirty(LayerDirtyFlag::Source);
      shape->changed();
      return false;
    }
  } else if (shape->hasCustomPolygon()) {
    const auto before = shape->customPolygonPoints();
    const bool beforeClosed = shape->customPolygonClosed();
    auto selected = impl_->selectedShapePathVertices_;
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    auto after = before;
    for (const int index : selected) {
      if (index >= 0 && index < static_cast<int>(after.size())) {
        after.erase(after.begin() + index);
      }
    }
    if (after.size() == before.size()) {
      return false;
    }
    if (after.size() >= 3) {
      shape->setCustomPolygonPoints(after, beforeClosed);
    } else {
      shape->clearCustomPolygonPoints();
    }
    shape->setDirty(LayerDirtyFlag::Source);
    const auto applied = shape->customPolygonPoints();
    const bool pushed = !mgr || mgr->push(
        std::make_unique<ShapePolygonPointsUndoCommand>(
            selectedLayer, before, applied, beforeClosed,
            shape->customPolygonClosed()));
    if (!pushed) {
      if (before.size() >= 3) {
        shape->setCustomPolygonPoints(before, beforeClosed);
      } else {
        shape->clearCustomPolygonPoints();
      }
      shape->setDirty(LayerDirtyFlag::Source);
      shape->changed();
      return false;
    }
  } else {
    return false;
  }
  impl_->selectedShapePathVertices_.clear();
  impl_->hoveredShapePathVertex_ = -1;
  impl_->hoveredShapePathTangent_ = 0;
  impl_->hoveredShapePathSegment_ = -1;
  impl_->publishLayerModified(selectedLayer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::selectAllShapePathVertices() {
  if (!impl_) {
    return;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape) {
    return;
  }
  impl_->selectedShapePathVertices_.clear();
  if (shape->hasCustomPath()) {
    const int count = static_cast<int>(shape->customPathVertices().size());
    for (int i = 0; i < count; ++i) {
      impl_->selectedShapePathVertices_.push_back(i);
    }
  } else if (shape->hasCustomPolygon()) {
    const int count = static_cast<int>(shape->customPolygonPoints().size());
    for (int i = 0; i < count; ++i) {
      impl_->selectedShapePathVertices_.push_back(i);
    }
  } else {
    return;
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::clearShapePathSelection() {
  if (!impl_ || impl_->selectedShapePathVertices_.empty()) {
    return false;
  }
  impl_->selectedShapePathVertices_.clear();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

int CompositionRenderController::selectedShapePathVertexCount() const {
  return impl_ ? static_cast<int>(impl_->selectedShapePathVertices_.size()) : 0;
}

// F6: hovered-vertex open/closed and smooth/corner toggles. Mirrors the
// deleteSelectedShapePathVertices guards and Undo tail; pending-path
// creation disables every branch here by design.
namespace {
ArtifactShapeLayer *hoveredShapePathLayer(
    const ArtifactCompositionPtr &comp, const LayerID &selectedLayerId,
    bool pendingShapePathCreation, int hoveredShapePathVertex,
    ArtifactAbstractLayerPtr &layerOut) {
  if (pendingShapePathCreation) {
    return nullptr;
  }
  auto selectedLayer = (!selectedLayerId.isNil() && comp)
                           ? comp->layerById(selectedLayerId)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || !shape->hasCustomPath() || selectedLayer->isLocked() ||
      selectedLayer->isSelectionLocked()) {
    return nullptr;
  }
  const auto vertices = shape->customPathVertices();
  if (hoveredShapePathVertex < 0 ||
      hoveredShapePathVertex >= static_cast<int>(vertices.size())) {
    return nullptr;
  }
  layerOut = selectedLayer;
  return shape;
}
}  // namespace

bool CompositionRenderController::hasHoveredShapePathVertex() const {
  if (!impl_) {
    return false;
  }
  ArtifactAbstractLayerPtr layer;
  return hoveredShapePathLayer(impl_->previewPipeline_.composition(),
                               impl_->selectedLayerId_,
                               impl_->pendingShapePathCreation_,
                               impl_->hoveredShapePathVertex_, layer) != nullptr;
}

bool CompositionRenderController::hoveredShapePathVertexSmooth() const {
  if (!impl_) {
    return false;
  }
  ArtifactAbstractLayerPtr layer;
  auto *shape = hoveredShapePathLayer(impl_->previewPipeline_.composition(),
                                       impl_->selectedLayerId_,
                                       impl_->pendingShapePathCreation_,
                                       impl_->hoveredShapePathVertex_, layer);
  if (!shape) {
    return false;
  }
  const auto vertices = shape->customPathVertices();
  return vertices[static_cast<size_t>(impl_->hoveredShapePathVertex_)].smooth;
}

bool CompositionRenderController::isSelectedShapePathClosed() const {
  if (!impl_ || impl_->pendingShapePathCreation_) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || !shape->hasCustomPath()) {
    return false;
  }
  return shape->customPathClosed();
}

bool CompositionRenderController::toggleHoveredShapePathClosed() {
  if (!impl_ || impl_->isDraggingShapePathVertex_ ||
      impl_->isDraggingShapePolygon_ || impl_->shapeParamDragMode_ != 0 ||
      impl_->shapeOpDragOp_ >= 0) {
    return false;
  }
  ArtifactAbstractLayerPtr selectedLayer;
  auto *shape = hoveredShapePathLayer(impl_->previewPipeline_.composition(),
                                       impl_->selectedLayerId_,
                                       impl_->pendingShapePathCreation_,
                                       impl_->hoveredShapePathVertex_,
                                       selectedLayer);
  if (!shape) {
    return false;
  }
  const auto before = shape->customPathVertices();
  const bool beforeClosed = shape->customPathClosed();
  if (!beforeClosed && before.size() < 3) {
    return false;
  }
  shape->setCustomPathVertices(before, !beforeClosed);
  shape->setDirty(LayerDirtyFlag::Source);
  auto *mgr = UndoManager::instance();
  const bool pushed = !mgr || mgr->push(std::make_unique<ShapePathVertexEditCommand>(
                                selectedLayer, before, before, beforeClosed,
                                !beforeClosed));
  if (!pushed) {
    shape->setCustomPathVertices(before, beforeClosed);
    shape->setDirty(LayerDirtyFlag::Source);
    shape->changed();
    return false;
  }
  impl_->publishLayerModified(selectedLayer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::toggleHoveredShapePathSmooth() {
  if (!impl_ || impl_->isDraggingShapePathVertex_ ||
      impl_->isDraggingShapePolygon_ || impl_->shapeParamDragMode_ != 0 ||
      impl_->shapeOpDragOp_ >= 0) {
    return false;
  }
  ArtifactAbstractLayerPtr selectedLayer;
  auto *shape = hoveredShapePathLayer(impl_->previewPipeline_.composition(),
                                       impl_->selectedLayerId_,
                                       impl_->pendingShapePathCreation_,
                                       impl_->hoveredShapePathVertex_,
                                       selectedLayer);
  if (!shape) {
    return false;
  }
  const auto before = shape->customPathVertices();
  const bool beforeClosed = shape->customPathClosed();
  auto after = before;
  togglePathVertexSmooth(after, impl_->hoveredShapePathVertex_, beforeClosed);
  shape->setCustomPathVertices(after, beforeClosed);
  shape->setDirty(LayerDirtyFlag::Source);
  auto *mgr = UndoManager::instance();
  const bool pushed = !mgr || mgr->push(std::make_unique<ShapePathVertexEditCommand>(
                                selectedLayer, before, after, beforeClosed,
                                beforeClosed));
  if (!pushed) {
    shape->setCustomPathVertices(before, beforeClosed);
    shape->setDirty(LayerDirtyFlag::Source);
    shape->changed();
    return false;
  }
  impl_->publishLayerModified(selectedLayer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

namespace {
// F5: editable polyline for operator handles: custom path positions first,
// custom polygon points second. Returns false when neither exists.
bool shapeEditablePolyline(ArtifactShapeLayer *shape,
                           std::vector<QPointF> &local, bool &closed) {
  if (!shape) {
    return false;
  }
  if (shape->hasCustomPath()) {
    const auto verts = shape->customPathVertices();
    if (verts.size() < 2) {
      return false;
    }
    local.clear();
    local.reserve(verts.size());
    for (const auto &v : verts) {
      local.push_back(v.pos);
    }
    closed = shape->customPathClosed();
    return true;
  }
  if (shape->hasCustomPolygon()) {
    const auto points = shape->customPolygonPoints();
    if (points.size() < 2) {
      return false;
    }
    local = points;
    closed = shape->customPolygonClosed();
    return true;
  }
  return false;
}

// Fraction [0,1] of the total polyline length nearest to the canvas point.
double shapePolylineFraction(const std::vector<QPointF> &local, bool closed,
                             const QTransform &global,
                             const QPointF &canvasPoint) {
  const int count = static_cast<int>(local.size());
  if (count < 2) {
    return 0.0;
  }
  const int segCount = closed ? count : count - 1;
  std::vector<double> lengths(static_cast<size_t>(segCount), 0.0);
  double total = 0.0;
  for (int s = 0; s < segCount; ++s) {
    const int next = (s + 1) % count;
    lengths[static_cast<size_t>(s)] =
        QLineF(global.map(local[static_cast<size_t>(s)]),
               global.map(local[static_cast<size_t>(next)])).length();
    total += lengths[static_cast<size_t>(s)];
  }
  if (!(total > 1e-9)) {
    return 0.0;
  }
  float best = std::numeric_limits<float>::max();
  double bestFraction = 0.0;
  double run = 0.0;
  for (int s = 0; s < segCount; ++s) {
    const int next = (s + 1) % count;
    const QPointF a = global.map(local[static_cast<size_t>(s)]);
    const QPointF b = global.map(local[static_cast<size_t>(next)]);
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    double t = 0.0;
    if (len2 > 1e-9) {
      t = QPointF::dotProduct(canvasPoint - a, ab) / len2;
      t = std::clamp(t, 0.0, 1.0);
    }
    const QPointF proj = a + ab * t;
    const float dx = static_cast<float>(proj.x() - canvasPoint.x());
    const float dy = static_cast<float>(proj.y() - canvasPoint.y());
    const float d2 = dx * dx + dy * dy;
    if (d2 < best) {
      best = d2;
      bestFraction = (run + lengths[static_cast<size_t>(s)] * t) / total;
    }
    run += lengths[static_cast<size_t>(s)];
  }
  return std::clamp(bestFraction, 0.0, 1.0);
}

// First draggable operators: a TrimPaths for start/end/offset triangles and
// one single-value op (offset/amount/radius) for the midpoint diamond.
void findDraggableShapeOps(ArtifactShapeLayer *shape, int &trimOp,
                           int &primaryOp, QString &primaryField) {
  trimOp = -1;
  primaryOp = -1;
  primaryField.clear();
  if (!shape) {
    return;
  }
  const int count = shape->shapeOperatorCount();
  for (int i = 0; i < count; ++i) {
    const auto type = shape->shapeOperatorTypeAt(i);
    if (trimOp < 0 &&
        type == ArtifactCore::ShapeOperatorType::TrimPaths) {
      trimOp = i;
    }
    if (primaryOp < 0) {
      if (type == ArtifactCore::ShapeOperatorType::OffsetPaths) {
        primaryOp = i;
        primaryField = QStringLiteral("offset");
      } else if (type == ArtifactCore::ShapeOperatorType::PuckerBloat) {
        primaryOp = i;
        primaryField = QStringLiteral("amount");
      } else if (type == ArtifactCore::ShapeOperatorType::RoundedCorners) {
        primaryOp = i;
        primaryField = QStringLiteral("radius");
      } else if (type == ArtifactCore::ShapeOperatorType::WavePaths) {
        primaryOp = i;
        primaryField = QStringLiteral("amount");
      }
    }
    if (trimOp >= 0 && primaryOp >= 0) {
      break;
    }
  }
}

QString shapeOpFieldName(int field, const QString &primaryField) {
  if (field == 1) return QStringLiteral("start");
  if (field == 2) return QStringLiteral("end");
  if (field == 3) return QStringLiteral("offset");
  return primaryField;
}
} // namespace

bool CompositionRenderController::beginShapeOperatorDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_) {
    return false;
  }
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (!shape || selectedLayer->isLocked() || selectedLayer->isSelectionLocked()) {
    return false;
  }
  std::vector<QPointF> local;
  bool closed = false;
  if (!shapeEditablePolyline(shape, local, closed)) {
    return false;
  }
  int trimOp = -1;
  int primaryOp = -1;
  QString primaryField;
  findDraggableShapeOps(shape, trimOp, primaryOp, primaryField);
  if (trimOp < 0 && primaryOp < 0) {
    return false;
  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QPointF canvasPoint(cPos.x, cPos.y);
  const QTransform globalTransform = selectedLayer->getGlobalTransform();
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const float hitRadius = 12.0f / zoom;
  const auto canvasOf = [&](const QPointF &p) {
    return globalTransform.map(p);
  };
  const float markerOff = 26.0f / zoom;
  const auto endpointNormal = [&](bool atStart) {
    // Unit normal of the endpoint segment for marker placement.
    QPointF dir(1.0, 0.0);
    if (local.size() >= 2) {
      if (atStart) {
        dir = local[1] - local.front();
      } else {
        dir = local.back() - local[local.size() - 2];
      }
      const QPointF a = canvasOf(atStart ? local.front() : local[local.size() - 2]);
      const QPointF b = canvasOf(atStart ? local[1] : local.back());
      dir = b - a;
    }
    const double len = std::hypot(dir.x(), dir.y());
    if (!(len > 1e-9)) {
      return QPointF(0.0, -1.0);
    }
    return QPointF(-dir.y() / len, dir.x() / len);
  };
  // Trim triangles at the path endpoints first, then the midpoint diamond.
  // Markers sit off the path along the endpoint normal so vertex handles
  // underneath stay grabbable. Shift prefers segment insert: the diamond
  // (which shares the chord midpoint) is skipped while Shift is held.
  if (trimOp >= 0 && !local.empty()) {
    const QPointF startCanvas = canvasOf(local.front());
    const QPointF endCanvas = canvasOf(local.back());
    const QPointF startMarker = startCanvas + endpointNormal(true) * markerOff;
    const QPointF endMarker = endCanvas + endpointNormal(false) * markerOff;
    const float sdx = static_cast<float>(startMarker.x() - canvasPoint.x());
    const float sdy = static_cast<float>(startMarker.y() - canvasPoint.y());
    const float edx = static_cast<float>(endMarker.x() - canvasPoint.x());
    const float edy = static_cast<float>(endMarker.y() - canvasPoint.y());
    const float s2 = sdx * sdx + sdy * sdy;
    const float e2 = edx * edx + edy * edy;
    int field = 0;
    if (s2 <= hitRadius * hitRadius &&
        (e2 > hitRadius * hitRadius || s2 <= e2)) {
      field = 1;
    } else if (e2 <= hitRadius * hitRadius) {
      field = 2;
    }
    if (field != 0) {
      const QString name = shapeOpFieldName(field, primaryField);
      const double before =
          shape->shapeOperatorValue(trimOp, name).toDouble();
      impl_->shapeOpDragOp_ = trimOp;
      impl_->shapeOpDragField_ = field;
      impl_->shapeOpDragLayer_ = selectedLayer;
      impl_->shapeOpBefore_ = before;
      markRenderDirty();
      return true;
    }
  }
  // Midpoint diamond: trim offset when a Trim exists, else the primary value.
  // Skipped while Shift is held so segment insert keeps the midpoint.
  int diamondOp = trimOp >= 0 ? trimOp : primaryOp;
  int diamondField = trimOp >= 0 ? 3 : 4;
  const bool shiftHeld =
      QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
  if (diamondOp >= 0 && local.size() >= 2 && !shiftHeld) {
    const QPointF midLocal =
        (local.front() + local.back()) * 0.5;
    const QPointF midCanvas = canvasOf(midLocal);
    const float mdx = static_cast<float>(midCanvas.x() - canvasPoint.x());
    const float mdy = static_cast<float>(midCanvas.y() - canvasPoint.y());
    if (mdx * mdx + mdy * mdy <= hitRadius * hitRadius) {
      const QString name = shapeOpFieldName(diamondField, primaryField);
      const double before =
          shape->shapeOperatorValue(diamondOp, name).toDouble();
      impl_->shapeOpDragOp_ = diamondOp;
      impl_->shapeOpDragField_ = diamondField;
      impl_->shapeOpDragLayer_ = selectedLayer;
      impl_->shapeOpBefore_ = before;
      impl_->shapeOpStartValue_ = before;
      impl_->shapeOpAnchorCanvasX_ = static_cast<float>(canvasPoint.x());
      std::vector<QPointF> measure = local;
      double total = 0.0;
      const int segCount = closed ? static_cast<int>(measure.size())
                                  : static_cast<int>(measure.size()) - 1;
      for (int s = 0; s < segCount; ++s) {
        const int next = (s + 1) % static_cast<int>(measure.size());
        total += QLineF(canvasOf(measure[static_cast<size_t>(s)]),
                        canvasOf(measure[static_cast<size_t>(next)])).length();
      }
      impl_->shapeOpPathLength_ = static_cast<float>(std::max(1.0, total));
      markRenderDirty();
      return true;
    }
  }
  return false;
}

void CompositionRenderController::updateShapeOperatorDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_ || impl_->shapeOpDragOp_ < 0 ||
      impl_->shapeOpDragField_ == 0) {
    return;
  }
  auto layer = impl_->shapeOpDragLayer_.lock();
  auto *shape = layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (!shape || impl_->shapeOpDragOp_ >= shape->shapeOperatorCount()) {
    return;
  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QPointF canvasPoint(cPos.x, cPos.y);
  int primaryOp = -1;
  int trimOp = -1;
  QString primaryField;
  findDraggableShapeOps(shape, trimOp, primaryOp, primaryField);
  const QString name =
      shapeOpFieldName(impl_->shapeOpDragField_, primaryField);
  if (impl_->shapeOpDragField_ == 1 || impl_->shapeOpDragField_ == 2) {
    std::vector<QPointF> local;
    bool closed = false;
    if (!shapeEditablePolyline(shape, local, closed)) {
      return;
    }
    const double fraction = shapePolylineFraction(
        local, closed, layer->getGlobalTransform(), canvasPoint);
    const double value = std::clamp(fraction * 100.0, 0.0, 100.0);
    const QString path = QStringLiteral("shape.operator.%1.%2")
                             .arg(impl_->shapeOpDragOp_)
                             .arg(name);
    shape->setLayerPropertyValue(path, QVariant(value));
    shape->setDirty(LayerDirtyFlag::Property);
  } else {
    // Delta-based fields ride on horizontal canvas movement.
    const double dxCanvas =
        static_cast<double>(canvasPoint.x()) - impl_->shapeOpAnchorCanvasX_;
    double value = impl_->shapeOpStartValue_;
    if (impl_->shapeOpDragField_ == 3) {
      value = impl_->shapeOpStartValue_ +
              dxCanvas / std::max(1.0f, impl_->shapeOpPathLength_) * 100.0;
      value = std::clamp(value, -100000.0, 100000.0);
    } else {
      value = impl_->shapeOpStartValue_ + dxCanvas;
      if (name == QStringLiteral("radius")) {
        value = std::clamp(value, 0.0, 100000.0);
      } else {
        value = std::clamp(value, -100000.0, 100000.0);
      }
    }
    const QString path = QStringLiteral("shape.operator.%1.%2")
                             .arg(impl_->shapeOpDragOp_)
                             .arg(name);
    shape->setLayerPropertyValue(path, QVariant(value));
    shape->setDirty(LayerDirtyFlag::Property);
  }
  markRenderDirty();
}

void CompositionRenderController::endShapeOperatorDrag() {
  if (!impl_ || impl_->shapeOpDragOp_ < 0 || impl_->shapeOpDragField_ == 0) {
    return;
  }
  const int op = impl_->shapeOpDragOp_;
  const int field = impl_->shapeOpDragField_;
  impl_->shapeOpDragOp_ = -1;
  impl_->shapeOpDragField_ = 0;
  auto layer = impl_->shapeOpDragLayer_.lock();
  impl_->shapeOpDragLayer_.reset();
  auto *shape = layer ? dynamic_cast<ArtifactShapeLayer *>(layer.get()) : nullptr;
  if (!shape || op >= shape->shapeOperatorCount()) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }
  int trimOp = -1;
  int primaryOp = -1;
  QString primaryField;
  findDraggableShapeOps(shape, trimOp, primaryOp, primaryField);
  const QString name = shapeOpFieldName(field, primaryField);
  const double after = shape->shapeOperatorValue(op, name).toDouble();
  if (std::abs(after - impl_->shapeOpBefore_) > 0.0001) {
    auto *mgr = UndoManager::instance();
    const bool pushed =
        !mgr || mgr->push(std::make_unique<ShapeOperatorValueUndoCommand>(
                   layer, op, name, impl_->shapeOpBefore_, after));
    if (!pushed) {
      const QString path =
          QStringLiteral("shape.operator.%1.%2").arg(op).arg(name);
      shape->setLayerPropertyValue(path, QVariant(impl_->shapeOpBefore_));
      shape->setDirty(LayerDirtyFlag::Property);
      shape->changed();
    } else {
      impl_->publishLayerModified(layer, true);
    }
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isEditingShapeOperator() const {
  return impl_ && impl_->shapeOpDragOp_ >= 0;
}

void CompositionRenderController::updateShapePathHover(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_ || impl_->isDraggingShapePathVertex_ ||
      impl_->shapeParamDragMode_ != 0 || impl_->isDraggingShapePolygon_ ||
      impl_->shapeOpDragOp_ >= 0) {
    return;
  }
  const int prevVertex = impl_->hoveredShapePathVertex_;
  const int prevTangent = impl_->hoveredShapePathTangent_;
  const int prevSegment = impl_->hoveredShapePathSegment_;
  const int prevParam = impl_->hoveredShapeParam_;
  const int prevOp = impl_->hoveredShapeOp_;
  const int prevOpField = impl_->hoveredShapeOpField_;
  impl_->hoveredShapePathVertex_ = -1;
  impl_->hoveredShapePathTangent_ = 0;
  impl_->hoveredShapePathSegment_ = -1;
  impl_->hoveredShapeParam_ = 0;
  impl_->hoveredShapeOp_ = -1;
  impl_->hoveredShapeOpField_ = 0;
  auto comp = impl_->previewPipeline_.composition();
  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};
  auto *shape = selectedLayer
                    ? dynamic_cast<ArtifactShapeLayer *>(selectedLayer.get())
                    : nullptr;
  if (shape && !selectedLayer->isLocked() &&
      !selectedLayer->isSelectionLocked()) {
    const auto cPos = impl_->renderer_->viewportToCanvas(
        {static_cast<float>(viewportPos.x()),
         static_cast<float>(viewportPos.y())});
    const QTransform globalTransform = selectedLayer->getGlobalTransform();
    const QPointF canvasPoint(cPos.x, cPos.y);
    const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
    const float hitRadius = 12.0f / zoom;
    // F2: parameter handles for Rect/Square/Star primitives.
    int paramMode = 0;
    if (shape->shapeType() == ShapeType::Rect ||
        shape->shapeType() == ShapeType::Square) {
      paramMode = 1;
    } else if (shape->shapeType() == ShapeType::Star) {
      paramMode = 2;
    }
    if (paramMode != 0) {
      const QPointF handleCanvas =
          globalTransform.map(shapeParamHandleLocal(*shape, paramMode));
      const float dx = static_cast<float>(handleCanvas.x() - canvasPoint.x());
      const float dy = static_cast<float>(handleCanvas.y() - canvasPoint.y());
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        impl_->hoveredShapeParam_ = paramMode;
      }
    }
    if (shape->hasCustomPath()) {
    const auto verts = shape->customPathVertices();
    for (int vI = 0; vI < static_cast<int>(verts.size()); ++vI) {
      const QPointF tangentPoints[2] = {
          verts[static_cast<size_t>(vI)].pos +
              verts[static_cast<size_t>(vI)].inTangent,
          verts[static_cast<size_t>(vI)].pos +
              verts[static_cast<size_t>(vI)].outTangent};
      for (int t = 0; t < 2; ++t) {
        if (verts[static_cast<size_t>(vI)].smooth ||
            verts[static_cast<size_t>(vI)].inTangent != QPointF(0, 0) ||
            verts[static_cast<size_t>(vI)].outTangent != QPointF(0, 0)) {
          const QPointF worldTangent = globalTransform.map(tangentPoints[t]);
          const float dx = static_cast<float>(worldTangent.x() - canvasPoint.x());
          const float dy = static_cast<float>(worldTangent.y() - canvasPoint.y());
          if (dx * dx + dy * dy <= hitRadius * hitRadius) {
            impl_->hoveredShapePathVertex_ = vI;
            impl_->hoveredShapePathTangent_ = t == 0 ? 1 : 2;
            break;
          }
        }
      }
      if (impl_->hoveredShapePathVertex_ >= 0) {
        break;
      }
      const QPointF worldPos =
          globalTransform.map(verts[static_cast<size_t>(vI)].pos);
      const float dx = static_cast<float>(worldPos.x() - canvasPoint.x());
      const float dy = static_cast<float>(worldPos.y() - canvasPoint.y());
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        impl_->hoveredShapePathVertex_ = vI;
        impl_->hoveredShapePathTangent_ = 0;
        break;
      }
    }
    if (impl_->hoveredShapePathVertex_ < 0 &&
        static_cast<int>(verts.size()) >= 2) {
      const float segRadius = 10.0f / zoom;
      const bool closed = shape->customPathClosed();
      const int segCount = closed ? static_cast<int>(verts.size())
                                  : static_cast<int>(verts.size()) - 1;
      float best = segRadius * segRadius;
      for (int s = 0; s < segCount; ++s) {
        const int next = (s + 1) % static_cast<int>(verts.size());
        const QPointF a = globalTransform.map(verts[static_cast<size_t>(s)].pos);
        const QPointF b = globalTransform.map(verts[static_cast<size_t>(next)].pos);
        const QPointF ab = b - a;
        const double len2 = QPointF::dotProduct(ab, ab);
        double t = 0.0;
        if (len2 > 1e-9) {
          t = QPointF::dotProduct(canvasPoint - a, ab) / len2;
          t = std::clamp(t, 0.0, 1.0);
        }
        const QPointF proj = a + ab * t;
        const float dx = static_cast<float>(proj.x() - canvasPoint.x());
        const float dy = static_cast<float>(proj.y() - canvasPoint.y());
        const float d2 = dx * dx + dy * dy;
        if (d2 <= best) {
          best = d2;
          impl_->hoveredShapePathSegment_ = s;
        }
      }
    }
    } // hasCustomPath
    // F2: custom polygon hover shares the vertex/segment fields; polygon and
    // custom path data are mutually exclusive so the indices never collide.
    if (!shape->hasCustomPath() && shape->hasCustomPolygon() &&
        impl_->hoveredShapePathVertex_ < 0) {
      const auto points = shape->customPolygonPoints();
      for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        const QPointF worldPos =
            globalTransform.map(points[static_cast<size_t>(i)]);
        const float dx = static_cast<float>(worldPos.x() - canvasPoint.x());
        const float dy = static_cast<float>(worldPos.y() - canvasPoint.y());
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
          impl_->hoveredShapePathVertex_ = i;
          impl_->hoveredShapePathTangent_ = 0;
          break;
        }
      }
      if (impl_->hoveredShapePathVertex_ < 0 && points.size() >= 2) {
        const float segRadius = 10.0f / zoom;
        const bool closed = shape->customPolygonClosed();
        const int segCount = closed ? static_cast<int>(points.size())
                                    : static_cast<int>(points.size()) - 1;
        float best = segRadius * segRadius;
        for (int s = 0; s < segCount; ++s) {
          const int next = (s + 1) % static_cast<int>(points.size());
          const QPointF a = globalTransform.map(points[static_cast<size_t>(s)]);
          const QPointF b = globalTransform.map(points[static_cast<size_t>(next)]);
          const QPointF ab = b - a;
          const double len2 = QPointF::dotProduct(ab, ab);
          double t = 0.0;
          if (len2 > 1e-9) {
            t = QPointF::dotProduct(canvasPoint - a, ab) / len2;
            t = std::clamp(t, 0.0, 1.0);
          }
          const QPointF proj = a + ab * t;
          const float dx = static_cast<float>(proj.x() - canvasPoint.x());
          const float dy = static_cast<float>(proj.y() - canvasPoint.y());
          const float d2 = dx * dx + dy * dy;
          if (d2 <= best) {
            best = d2;
            impl_->hoveredShapePathSegment_ = s;
          }
        }
      }
    }
    // F5: operator handle hover (trim triangles + midpoint diamond).
    // Marker geometry mirrors beginShapeOperatorDrag by design.
    {
      std::vector<QPointF> opLocal;
      bool opClosed = false;
      int trimOp = -1;
      int primaryOp = -1;
      QString primaryField;
      findDraggableShapeOps(shape, trimOp, primaryOp, primaryField);
      if ((trimOp >= 0 || primaryOp >= 0) &&
          shapeEditablePolyline(shape, opLocal, opClosed) &&
          opLocal.size() >= 2) {
        const float opHit = 12.0f / zoom;
        const float opOff = 26.0f / zoom;
        const auto opCanvas = [&](const QPointF &p) {
          return globalTransform.map(p);
        };
        const auto opNormal = [&](bool atStart) {
          QPointF a;
          QPointF b;
          if (atStart) {
            a = opCanvas(opLocal.front());
            b = opCanvas(opLocal[1]);
          } else {
            a = opCanvas(opLocal[opLocal.size() - 2]);
            b = opCanvas(opLocal.back());
          }
          QPointF dir = b - a;
          const double len = std::hypot(dir.x(), dir.y());
          if (!(len > 1e-9)) {
            return QPointF(0.0, -1.0);
          }
          return QPointF(-dir.y() / len, dir.x() / len);
        };
        const QPointF startCanvas = opCanvas(opLocal.front());
        const QPointF endCanvas = opCanvas(opLocal.back());
        if (trimOp >= 0) {
          const QPointF startMarker = startCanvas + opNormal(true) * opOff;
          const QPointF endMarker = endCanvas + opNormal(false) * opOff;
          const float sdx = static_cast<float>(startMarker.x() - canvasPoint.x());
          const float sdy = static_cast<float>(startMarker.y() - canvasPoint.y());
          const float edx = static_cast<float>(endMarker.x() - canvasPoint.x());
          const float edy = static_cast<float>(endMarker.y() - canvasPoint.y());
          const float s2 = sdx * sdx + sdy * sdy;
          const float e2 = edx * edx + edy * edy;
          if (s2 <= opHit * opHit && (e2 > opHit * opHit || s2 <= e2)) {
            impl_->hoveredShapeOp_ = trimOp;
            impl_->hoveredShapeOpField_ = 1;
          } else if (e2 <= opHit * opHit) {
            impl_->hoveredShapeOp_ = trimOp;
            impl_->hoveredShapeOpField_ = 2;
          }
        }
        const bool hoverShift =
            QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
        if (impl_->hoveredShapeOp_ < 0 && !hoverShift) {
          const int diamondOp = trimOp >= 0 ? trimOp : primaryOp;
          const int diamondField = trimOp >= 0 ? 3 : 4;
          const QPointF midCanvas = globalTransform.map(
              (opLocal.front() + opLocal.back()) * 0.5);
          const float mdx = static_cast<float>(midCanvas.x() - canvasPoint.x());
          const float mdy = static_cast<float>(midCanvas.y() - canvasPoint.y());
          if (mdx * mdx + mdy * mdy <= opHit * opHit) {
            impl_->hoveredShapeOp_ = diamondOp;
            impl_->hoveredShapeOpField_ = diamondField;
          }
        }
      }
    }
  }
  if (prevVertex != impl_->hoveredShapePathVertex_ ||
      prevTangent != impl_->hoveredShapePathTangent_ ||
      prevSegment != impl_->hoveredShapePathSegment_ ||
      prevParam != impl_->hoveredShapeParam_ ||
      prevOp != impl_->hoveredShapeOp_ ||
      prevOpField != impl_->hoveredShapeOpField_) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
}

bool CompositionRenderController::beginShapePathVertexDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_) {

    return false;

  }

  auto comp = impl_->previewPipeline_.composition();

  auto selectedLayer = (!impl_->selectedLayerId_.isNil() && comp)
                           ? comp->layerById(impl_->selectedLayerId_)
                           : ArtifactAbstractLayerPtr{};

  auto *shape =
      selectedLayer ? dynamic_cast<ArtifactShapeLayer *>(
                          selectedLayer.get())
                    : nullptr;
  if (!shape || !shape->hasCustomPath() || selectedLayer->isLocked() ||
      selectedLayer->isSelectionLocked()) {

    return false;

  }

  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QTransform globalTransform = selectedLayer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {

    return false;

  }
  const QPointF canvasPoint(cPos.x, cPos.y);
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const float hitRadius = 12.0f / zoom;
  const auto verts = shape->customPathVertices();
  int hitVertex = -1;
  int hitTangent = 0;
  for (int vI = 0; vI < static_cast<int>(verts.size()); ++vI) {
    const QPointF tangentPoints[2] = {
        verts[static_cast<size_t>(vI)].pos +
            verts[static_cast<size_t>(vI)].inTangent,
        verts[static_cast<size_t>(vI)].pos +
            verts[static_cast<size_t>(vI)].outTangent};
    for (int t = 0; t < 2; ++t) {
      if (verts[static_cast<size_t>(vI)].smooth ||
          verts[static_cast<size_t>(vI)].inTangent != QPointF(0, 0) ||
          verts[static_cast<size_t>(vI)].outTangent != QPointF(0, 0)) {
        const QPointF worldTangent = globalTransform.map(tangentPoints[t]);
        const float dx = static_cast<float>(
            worldTangent.x() - canvasPoint.x());
        const float dy = static_cast<float>(
            worldTangent.y() - canvasPoint.y());
        if (dx * dx + dy * dy <= hitRadius * hitRadius) {
          hitVertex = vI;
          hitTangent = t == 0 ? 1 : 2;
          break;
        }
      }
    }
    if (hitVertex >= 0) break;
    const QPointF worldPos =
        globalTransform.map(verts[static_cast<size_t>(vI)].pos);
    const float dx =
        static_cast<float>(worldPos.x() - canvasPoint.x());
    const float dy =
        static_cast<float>(worldPos.y() - canvasPoint.y());
    if (dx * dx + dy * dy <= hitRadius * hitRadius) {
      hitVertex = vI;
      hitTangent = 0;
      break;
    }
  }
  if (hitVertex < 0) {

    return false;

  }
  // F4: press updates the vertex selection grammar before the drag owns it.
  applyShapeVertexSelectionGrammar(impl_->selectedShapePathVertices_,
                                   static_cast<int>(verts.size()), hitVertex);
  impl_->shapePathEditPending_ = true;
  impl_->shapePathEditDirty_ = false;
  impl_->shapePathEditLayer_ = selectedLayer;
  impl_->shapePathEditBefore_ = verts;
  impl_->shapePathEditBeforeClosed_ = shape->customPathClosed();
  impl_->isDraggingShapePathVertex_ = true;
  impl_->draggingShapePathVertexIndex_ = hitVertex;
  impl_->draggingShapePathTangent_ = hitTangent;
  markRenderDirty();
  return true;
}

void CompositionRenderController::updateShapePathVertexDrag(
    ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPoint) {
  const QPointF viewportPos =
      qPointFromScreenPhysicalPoint(viewportPoint);
  if (!impl_ || !impl_->renderer_ || !impl_->isDraggingShapePathVertex_) {

    return;

  }
  auto layer = impl_->shapePathEditLayer_.lock();
  auto *shape = layer ? dynamic_cast<ArtifactShapeLayer *>(
                            layer.get())
                      : nullptr;
  if (!shape) {

    return;

  }
  const auto cPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(viewportPos.x()),
       static_cast<float>(viewportPos.y())});
  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {

    return;

  }
  const QPointF localPos = invTransform.map(QPointF(cPos.x, cPos.y));
  const int index = impl_->draggingShapePathVertexIndex_;
  const auto currentVerts = shape->customPathVertices();
  if (index < 0 || index >= static_cast<int>(currentVerts.size())) {

    return;

  }
  const int tangentKind = impl_->draggingShapePathTangent_;
  auto verts = currentVerts;
  if (tangentKind == 0) {
    // Tangents are relative to the vertex, so moving the vertex with
    // untouched tangents keeps the handles glued (rigid follow), matching
    // mask vertex dragging. A previous revision subtracted the delta once
    // the session got dirty, pinning handles in world space mid-drag.
    const auto selected = impl_->selectedShapePathVertices_;
    const bool moveSelection = selected.size() > 1 &&
        std::find(selected.begin(), selected.end(), index) != selected.end() &&
        index < static_cast<int>(impl_->shapePathEditBefore_.size());
    verts = moveSelection ? impl_->shapePathEditBefore_ : currentVerts;
    if (moveSelection) {
      const QPointF delta =
          localPos - impl_->shapePathEditBefore_[static_cast<size_t>(index)].pos;
      for (const int selectedIndex : selected) {
        if (selectedIndex >= 0 &&
            selectedIndex < static_cast<int>(verts.size())) {
          verts[static_cast<size_t>(selectedIndex)].pos =
              impl_->shapePathEditBefore_[static_cast<size_t>(selectedIndex)].pos +
              delta;
        }
      }
    } else {
      verts[static_cast<size_t>(index)].pos = localPos;
    }
    shape->setCustomPathVertices(verts, shape->customPathClosed());
    shape->setDirty(LayerDirtyFlag::Source);
    impl_->shapePathEditDirty_ = true;
    markRenderDirty();
    return;
  } else if (tangentKind == 1) {
    verts[static_cast<size_t>(index)].inTangent =
        localPos - verts[static_cast<size_t>(index)].pos;
    // Alt breaks the smooth mirror for this drag only (parity with mask and
    // motion-path tangents). The smooth flag itself is left intact so the
    // mirror resumes on the next non-Alt drag.
    const bool breakMirror = QGuiApplication::keyboardModifiers().testFlag(
        Qt::AltModifier);
    if (!breakMirror && verts[static_cast<size_t>(index)].smooth) {
      const float outLen = static_cast<float>(std::hypot(
          verts[static_cast<size_t>(index)].outTangent.x(),
          verts[static_cast<size_t>(index)].outTangent.y()));
      const float inLen = static_cast<float>(std::hypot(
          verts[static_cast<size_t>(index)].inTangent.x(),
          verts[static_cast<size_t>(index)].inTangent.y()));
      if (inLen > 0.0001f && outLen > 0.0001f) {
        verts[static_cast<size_t>(index)].outTangent = QPointF(
            -verts[static_cast<size_t>(index)].inTangent.x(),
            -verts[static_cast<size_t>(index)].inTangent.y()) *
            (outLen / inLen);
      }
    }
  } else {
    verts[static_cast<size_t>(index)].outTangent =
        localPos - verts[static_cast<size_t>(index)].pos;
    const bool breakMirror = QGuiApplication::keyboardModifiers().testFlag(
        Qt::AltModifier);
    if (!breakMirror && verts[static_cast<size_t>(index)].smooth) {
      const float inLen = static_cast<float>(std::hypot(
          verts[static_cast<size_t>(index)].inTangent.x(),
          verts[static_cast<size_t>(index)].inTangent.y()));
      const float outLen = static_cast<float>(std::hypot(
          verts[static_cast<size_t>(index)].outTangent.x(),
          verts[static_cast<size_t>(index)].outTangent.y()));
      if (inLen > 0.0001f && outLen > 0.0001f) {
        verts[static_cast<size_t>(index)].inTangent = QPointF(
            -verts[static_cast<size_t>(index)].outTangent.x(),
            -verts[static_cast<size_t>(index)].outTangent.y()) *
            (inLen / outLen);
      }
    }
  }
  shape->setCustomPathVertices(verts, shape->customPathClosed());
  shape->setDirty(LayerDirtyFlag::Source);
  impl_->shapePathEditDirty_ = true;
  markRenderDirty();
}

void CompositionRenderController::endShapePathVertexDrag() {
  if (!impl_ || !impl_->isDraggingShapePathVertex_) {

    return;

  }
  impl_->isDraggingShapePathVertex_ = false;
  impl_->draggingShapePathVertexIndex_ = -1;
  impl_->draggingShapePathTangent_ = 0;
  auto layer = impl_->shapePathEditLayer_.lock();
  if (layer && impl_->shapePathEditDirty_) {
    auto *shape = dynamic_cast<ArtifactShapeLayer *>(layer.get());
    if (shape) {
      auto *mgr = UndoManager::instance();
      const auto afterVertices = shape->customPathVertices();
      const bool afterClosed = shape->customPathClosed();
      const bool pushed = !mgr || mgr->push(
          std::make_unique<ShapePathVertexEditCommand>(
              layer, impl_->shapePathEditBefore_, afterVertices,
              impl_->shapePathEditBeforeClosed_, afterClosed));
      if (!pushed) {
        shape->setCustomPathVertices(impl_->shapePathEditBefore_,
                                     impl_->shapePathEditBeforeClosed_);
        shape->setDirty(LayerDirtyFlag::Source);
        shape->changed();
      } else {
        impl_->publishLayerModified(layer, true);
      }
    }
  }
  impl_->shapePathEditPending_ = false;
  impl_->shapePathEditDirty_ = false;
  impl_->shapePathEditLayer_.reset();
  impl_->shapePathEditBefore_.clear();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isEditingShapePathVertices() const {

  return impl_ && impl_->isDraggingShapePathVertex_;

}

void CompositionRenderController::setPointerPressure(float pressure) {
  auto *app = ArtifactApplicationManager::instance();
  if (!app) return;
  if (auto *brushTool = app->brushTool()) {
    brushTool->setPressure(pressure);
  }
  if (auto *motionSketchTool = app->motionSketchTool()) {
    motionSketchTool->setPressure(pressure);
  }
}

void CompositionRenderController::setPointerTilt(float tiltX, float tiltY) {
  auto *app = ArtifactApplicationManager::instance();
  if (!app) return;
  if (auto *brushTool = app->brushTool()) {
    brushTool->setTilt(tiltX, tiltY);
  }
  if (auto *motionSketchTool = app->motionSketchTool()) {
    motionSketchTool->setTilt(tiltX, tiltY);
  }
}


bool CompositionRenderController::cancelTextToolInteraction() {
  if (!impl_ || !impl_->textToolCandidate_) {
    return false;
  }
  impl_->textToolCandidate_ = false;
  impl_->textToolDragging_ = false;
  impl_->textToolStartCanvas_ = {};
  impl_->textToolCurrentCanvas_ = {};
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::cancelMaskInteraction() {
  if (!impl_) {
    return;
  }
  impl_->clearPendingMaskCreation();
  impl_->clearPendingShapePathCreation();
  impl_->clearRectangleToolSession();
  impl_->maskRubberBandCandidate_ = false;
  impl_->isMaskRubberBandSelecting_ = false;
  impl_->isDraggingVertex_ = false;
  impl_->isDraggingMaskHandle_ = false;
  impl_->isDraggingMaskGeometry_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;
  impl_->draggingMaskHandleType_ = -1;
  if (impl_->maskEditPending_) {
    if (auto layer = impl_->maskEditLayer_.lock()) {
      for (int i = 0; i < static_cast<int>(impl_->maskEditBefore_.size()); ++i) {
        layer->setMask(i, impl_->maskEditBefore_[static_cast<size_t>(i)]);
      }
      impl_->publishLayerModified(layer, true);
    }
    impl_->maskEditPending_ = false;
    impl_->maskEditLayer_.reset();
    impl_->maskEditBefore_.clear();
    impl_->maskEditDirty_ = false;
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::removeLastPendingMaskVertex() {
  if (!impl_ || !impl_->pendingMaskCreation_ ||
      impl_->pendingMaskPath_.vertexCount() <= 0) {
    return false;
  }
  impl_->pendingMaskPath_.removeVertex(
      impl_->pendingMaskPath_.vertexCount() - 1);
  impl_->isDraggingMaskHandle_ = false;
  impl_->draggingMaskIndex_ = -1;
  impl_->draggingPathIndex_ = -1;
  impl_->draggingVertexIndex_ = -1;
  impl_->draggingMaskHandleType_ = -1;
  impl_->penMaskPreviewValid_ = false;
  if (impl_->pendingMaskPath_.vertexCount() == 0) {
    impl_->clearPendingMaskCreation();
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::deleteHoveredMaskVertex() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 || impl_->hoveredVertexIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked()) {
    return false;
  }
  if (impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  if (impl_->hoveredVertexIndex_ >= path.vertexCount()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  path.removeVertex(impl_->hoveredVertexIndex_);
  if (path.vertexCount() < 2) {
    mask.removeMaskPath(impl_->hoveredPathIndex_);
  } else {
    mask.setMaskPath(impl_->hoveredPathIndex_, path);
  }
  if (mask.maskPathCount() <= 0) {
    layer->removeMask(impl_->hoveredMaskIndex_);
  } else {
    layer->setMask(impl_->hoveredMaskIndex_, mask);
  }
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->hoveredVertexIndex_ = -1;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetHoveredMaskTangent() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 || impl_->hoveredVertexIndex_ < 0 ||
      impl_->hoveredMaskHandleType_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  if (impl_->hoveredVertexIndex_ >= path.vertexCount()) {
    return false;
  }
  MaskVertex vertex = path.vertex(impl_->hoveredVertexIndex_);
  if (static_cast<MaskEditHandleType>(impl_->hoveredMaskHandleType_) ==
      MaskEditHandleType::InTangent) {
    vertex.inTangent = QPointF(0.0, 0.0);
  } else if (static_cast<MaskEditHandleType>(impl_->hoveredMaskHandleType_) ==
             MaskEditHandleType::OutTangent) {
    vertex.outTangent = QPointF(0.0, 0.0);
  } else {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  path.setVertex(impl_->hoveredVertexIndex_, vertex);
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetHoveredMaskVertexTangents() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 || impl_->hoveredVertexIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  if (impl_->hoveredVertexIndex_ >= path.vertexCount()) {
    return false;
  }
  MaskVertex vertex = path.vertex(impl_->hoveredVertexIndex_);
  if (vertex.inTangent == QPointF(0.0, 0.0) &&
      vertex.outTangent == QPointF(0.0, 0.0)) {
    return false;
  }
  vertex.inTangent = QPointF(0.0, 0.0);
  vertex.outTangent = QPointF(0.0, 0.0);
  impl_->beginMaskEditTransaction(layer);
  path.setVertex(impl_->hoveredVertexIndex_, vertex);
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::hasHoveredMaskVertex() const {
  return impl_ && impl_->hoveredMaskIndex_ >= 0 &&
         impl_->hoveredPathIndex_ >= 0 && impl_->hoveredVertexIndex_ >= 0;
}

bool CompositionRenderController::setHoveredMaskVertexType(int type) {
  if (!impl_ || type < 0 || type > 3 ||
      impl_->hoveredMaskIndex_ < 0 || impl_->hoveredPathIndex_ < 0 ||
      impl_->hoveredVertexIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  const int vertexIndex = impl_->hoveredVertexIndex_;
  if (vertexIndex >= path.vertexCount()) {
    return false;
  }

  MaskVertex vertex = path.vertex(vertexIndex);
  if (type == 0) { // Corner
    vertex.inTangent = QPointF(0.0, 0.0);
    vertex.outTangent = QPointF(0.0, 0.0);
  } else if (type == 1 || type == 2) { // Smooth / Continuous
    const int count = path.vertexCount();
    const bool closed = path.isClosed() && count > 2;
    const int prevIndex = vertexIndex > 0 ? vertexIndex - 1
                                          : (closed ? count - 1 : -1);
    const int nextIndex = vertexIndex + 1 < count ? vertexIndex + 1
                                                   : (closed ? 0 : -1);
    QPointF direction(1.0, 0.0);
    if (prevIndex >= 0 && nextIndex >= 0) {
      direction = path.vertex(nextIndex).position -
                  path.vertex(prevIndex).position;
    } else if (nextIndex >= 0) {
      direction = path.vertex(nextIndex).position - vertex.position;
    } else if (prevIndex >= 0) {
      direction = vertex.position - path.vertex(prevIndex).position;
    }
    const double length = std::hypot(direction.x(), direction.y());
    if (length > 0.0001) {
      direction /= length;
    }
    const double inLength = std::hypot(vertex.inTangent.x(),
                                       vertex.inTangent.y());
    const double outLength = std::hypot(vertex.outTangent.x(),
                                        vertex.outTangent.y());
    const double fallback =
        length > 0.0001 ? length / 3.0 : 24.0;
    const double useInLength = inLength > 0.0001 ? inLength : fallback;
    const double useOutLength = outLength > 0.0001 ? outLength : fallback;
    vertex.inTangent = -direction * useInLength;
    vertex.outTangent = direction * useOutLength;
    if (type == 2) { // Continuous keeps both handles equal in magnitude.
      const double continuousLength =
          std::max(useInLength, useOutLength);
      vertex.inTangent = -direction * continuousLength;
      vertex.outTangent = direction * continuousLength;
    }
  } else {
    // Broken preserves the two current handles.  Future drags will not mirror
    // them when Alt is held, so the existing tangent values remain independent.
  }

  impl_->beginMaskEditTransaction(layer);
  path.setVertex(vertexIndex, vertex);
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::deleteSelectedMaskVertices() {
  if (!impl_ || impl_->selectedMaskVertices_.empty()) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer) {
    return false;
  }

  auto selected = impl_->selectedMaskVertices_;
  std::sort(selected.begin(), selected.end(),
            [](const auto &lhs, const auto &rhs) {
              if (ArtifactCore::artifactGet<0>(lhs) != ArtifactCore::artifactGet<0>(rhs)) {
                return ArtifactCore::artifactGet<0>(lhs) > ArtifactCore::artifactGet<0>(rhs);
              }
              if (ArtifactCore::artifactGet<1>(lhs) != ArtifactCore::artifactGet<1>(rhs)) {
                return ArtifactCore::artifactGet<1>(lhs) > ArtifactCore::artifactGet<1>(rhs);
              }
              return ArtifactCore::artifactGet<2>(lhs) > ArtifactCore::artifactGet<2>(rhs);
            });

  bool changed = false;
  impl_->beginMaskEditTransaction(layer);
  for (const auto &selectedAddress : selected) {
    const int maskIndex = ArtifactCore::artifactGet<0>(selectedAddress);
    const int pathIndex = ArtifactCore::artifactGet<1>(selectedAddress);
    const int vertexIndex = ArtifactCore::artifactGet<2>(selectedAddress);
    if (maskIndex < 0 || maskIndex >= layer->maskCount()) {
      continue;
    }
    LayerMask mask = layer->mask(maskIndex);
    if (mask.isLocked() || pathIndex < 0 ||
        pathIndex >= mask.maskPathCount()) {
      continue;
    }
    MaskPath path = mask.maskPath(pathIndex);
    if (vertexIndex < 0 || vertexIndex >= path.vertexCount()) {
      continue;
    }
    path.removeVertex(vertexIndex);
    if (path.vertexCount() < 2) {
      mask.removeMaskPath(pathIndex);
    } else {
      mask.setMaskPath(pathIndex, path);
    }
    if (mask.maskPathCount() == 0) {
      layer->removeMask(maskIndex);
    } else {
      layer->setMask(maskIndex, mask);
    }
    changed = true;
  }
  if (!changed) {
    impl_->commitMaskEditTransaction();
    return false;
  }
  impl_->selectedMaskVertices_.clear();
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::rotateSelectedMaskVertices(
    ArtifactCore::Coordinates::LayerLocalPoint2 center,
    ArtifactCore::Units::Degrees angle) {
  if (!impl_ || impl_->selectedMaskVertices_.empty()) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer) {
    return false;
  }

  const double radians = angle.value * (M_PI / 180.0);
  const double cosA = std::cos(radians);
  const double sinA = std::sin(radians);

  impl_->beginMaskEditTransaction(layer);

  for (const auto& selectedVertex : impl_->selectedMaskVertices_) {
    const auto maskIndex = ArtifactCore::artifactGet<0>(selectedVertex);
    const auto pathIndex = ArtifactCore::artifactGet<1>(selectedVertex);
    const auto vertexIndex = ArtifactCore::artifactGet<2>(selectedVertex);
    if (maskIndex < 0 || maskIndex >= layer->maskCount()) continue;
    LayerMask mask = layer->mask(maskIndex);
    if (pathIndex < 0 || pathIndex >= mask.maskPathCount()) continue;
    MaskPath path = mask.maskPath(pathIndex);
    if (vertexIndex < 0 || vertexIndex >= path.vertexCount()) continue;

    MaskVertex v = path.vertex(vertexIndex);

    // Tangents are relative offsets from position; compute their absolute
    // coordinates first so all three points rotate consistently.
    const QPointF inAbs = v.position + v.inTangent;
    const QPointF outAbs = v.position + v.outTangent;

    auto rotateAroundCenter = [&](const QPointF& p) -> QPointF {
      const double dx = p.x() - center.x;
      const double dy = p.y() - center.y;
      return QPointF(center.x + dx * cosA - dy * sinA,
                     center.y + dx * sinA + dy * cosA);
    };

    const QPointF newPos = rotateAroundCenter(v.position);
    const QPointF newInAbs = rotateAroundCenter(inAbs);
    const QPointF newOutAbs = rotateAroundCenter(outAbs);

    v.position = newPos;
    v.inTangent = newInAbs - newPos;
    v.outTangent = newOutAbs - newPos;

    path.setVertex(vertexIndex, v);
    mask.setMaskPath(pathIndex, path);
    layer->setMask(maskIndex, mask);
  }

  impl_->markMaskEditDirty();
  impl_->invalidateOverlayComposite();
  markRenderDirty();

  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp->id().toString(), layer->id().toString(),
      LayerChangedEvent::ChangeType::Modified});
  return true;
}

bool CompositionRenderController::scaleSelectedMaskVertices(
    ArtifactCore::Coordinates::LayerLocalPoint2 center,
    ArtifactCore::Units::Scale2 scale) {
  const float centerX = center.x;
  const float centerY = center.y;
  const float scaleX = scale.x;
  const float scaleY = scale.y;
  if (!impl_ || impl_->selectedMaskVertices_.empty()) {
    return false;
  }
  if (!std::isfinite(scaleX) || !std::isfinite(scaleY) ||
      (std::abs(scaleX) < 1e-6f && std::abs(scaleY) < 1e-6f)) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer) {
    return false;
  }

  impl_->beginMaskEditTransaction(layer);

  for (const auto& selectedVertex : impl_->selectedMaskVertices_) {
    const auto maskIndex = ArtifactCore::artifactGet<0>(selectedVertex);
    const auto pathIndex = ArtifactCore::artifactGet<1>(selectedVertex);
    const auto vertexIndex = ArtifactCore::artifactGet<2>(selectedVertex);
    if (maskIndex < 0 || maskIndex >= layer->maskCount()) continue;
    LayerMask mask = layer->mask(maskIndex);
    if (pathIndex < 0 || pathIndex >= mask.maskPathCount()) continue;
    MaskPath path = mask.maskPath(pathIndex);
    if (vertexIndex < 0 || vertexIndex >= path.vertexCount()) continue;

    MaskVertex v = path.vertex(vertexIndex);
    v.position.setX(centerX + (v.position.x() - centerX) * scaleX);
    v.position.setY(centerY + (v.position.y() - centerY) * scaleY);

    QPointF inAbs(v.position.x() + v.inTangent.x(),
                  v.position.y() + v.inTangent.y());
    QPointF outAbs(v.position.x() + v.outTangent.x(),
                   v.position.y() + v.outTangent.y());
    inAbs.setX(centerX + (inAbs.x() - centerX) * scaleX);
    inAbs.setY(centerY + (inAbs.y() - centerY) * scaleY);
    outAbs.setX(centerX + (outAbs.x() - centerX) * scaleX);
    outAbs.setY(centerY + (outAbs.y() - centerY) * scaleY);
    v.inTangent = inAbs - v.position;
    v.outTangent = outAbs - v.position;

    path.setVertex(vertexIndex, v);
    mask.setMaskPath(pathIndex, path);
    layer->setMask(maskIndex, mask);
  }

  impl_->markMaskEditDirty();
  impl_->invalidateOverlayComposite();
  markRenderDirty();

  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      comp->id().toString(), layer->id().toString(),
      LayerChangedEvent::ChangeType::Modified});
  return true;
}

bool CompositionRenderController::deleteHoveredMask() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  if (layer->mask(impl_->hoveredMaskIndex_).isLocked()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  layer->removeMask(impl_->hoveredMaskIndex_);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->hoveredMaskIndex_ = -1;
  impl_->hoveredPathIndex_ = -1;
  impl_->hoveredVertexIndex_ = -1;
  impl_->hoveredMaskHandleType_ = -1;
  impl_->selectedMaskVertices_.clear();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::selectAllMaskVertices() {
  if (!impl_) {
    return;
  }
  impl_->selectedMaskVertices_.clear();
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer) {
    return;
  }
  for (int maskIndex = 0; maskIndex < layer->maskCount(); ++maskIndex) {
    const LayerMask mask = layer->mask(maskIndex);
    if (mask.isLocked()) {
      continue;
    }
    for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
      const MaskPath path = mask.maskPath(pathIndex);
      for (int vertexIndex = 0; vertexIndex < path.vertexCount(); ++vertexIndex) {
        impl_->selectedMaskVertices_.emplace_back(maskIndex, pathIndex,
                                                  vertexIndex);
      }
    }
  }
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::setHoveredMaskMode(int modeValue) {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked()) {
    return false;
  }
  if (impl_->hoveredPathIndex_ >= mask.maskPathCount() ||
      modeValue < 0 || modeValue > 3) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  impl_->beginMaskEditTransaction(layer);
  path.setMode(static_cast<MaskMode>(modeValue));
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::hasHoveredMaskPath() const {
  return impl_ && impl_->hoveredMaskIndex_ >= 0 &&
         impl_->hoveredPathIndex_ >= 0;
}

bool CompositionRenderController::toggleHoveredMaskEnabled() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  if (layer->mask(impl_->hoveredMaskIndex_).isLocked()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  mask.setEnabled(!mask.isEnabled());
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::isMaskProportionalEditingEnabled() const {
  return impl_ && impl_->maskProportionalEditingEnabled_;
}

void CompositionRenderController::setMaskProportionalEditingEnabled(bool enabled) {
  if (impl_) impl_->maskProportionalEditingEnabled_ = enabled;
}

float CompositionRenderController::maskProportionalEditRadius() const {
  return impl_ ? impl_->maskProportionalEditRadius_ : 120.0f;
}

void CompositionRenderController::setMaskProportionalEditRadius(float radius) {
  if (impl_ && std::isfinite(radius)) {
    impl_->maskProportionalEditRadius_ = std::clamp(radius, 1.0f, 100000.0f);
  }
}

bool CompositionRenderController::deleteHoveredMaskForSelectedLayers() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const int maskIndex = impl_->hoveredMaskIndex_;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Delete Mask on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  bool changed = false;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex < 0 || maskIndex >= layer->maskCount() ||
        layer->mask(maskIndex).isLocked()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    layer->removeMask(maskIndex);
    std::vector<LayerMask> afterMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->hoveredMaskIndex_ = std::max(0, maskIndex - 1);
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::toggleHoveredMaskEnabledForSelectedLayers() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const int maskIndex = impl_->hoveredMaskIndex_;
  const auto primary = comp->layerById(impl_->selectedLayerId_);
  if (!primary || maskIndex >= primary->maskCount() ||
      primary->mask(maskIndex).isLocked()) {
    return false;
  }
  const bool enabled = !primary->mask(maskIndex).isEnabled();
  bool changed = false;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Toggle Mask Enabled on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex >= layer->maskCount() || layer->mask(maskIndex).isLocked()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    beforeMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    auto mask = layer->mask(maskIndex);
    mask.setEnabled(enabled);
    layer->setMask(maskIndex, mask);
    std::vector<LayerMask> afterMasks;
    afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::toggleHoveredMaskLocked() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  mask.setLocked(!mask.isLocked());
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::duplicateHoveredMask() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  layer->addMask(layer->mask(impl_->hoveredMaskIndex_));
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::moveHoveredMask(int direction) {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 || direction == 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  const int target = impl_->hoveredMaskIndex_ + (direction < 0 ? -1 : 1);
  if (target < 0 || target >= layer->maskCount()) {
    return false;
  }
  std::vector<LayerMask> masks;
  masks.reserve(static_cast<size_t>(layer->maskCount()));
  for (int i = 0; i < layer->maskCount(); ++i) {
    masks.push_back(layer->mask(i));
  }
  std::swap(masks[static_cast<size_t>(impl_->hoveredMaskIndex_)],
            masks[static_cast<size_t>(target)]);
  impl_->beginMaskEditTransaction(layer);
  while (layer->maskCount() > 0) {
    layer->removeMask(layer->maskCount() - 1);
  }
  for (const auto &mask : masks) {
    layer->addMask(mask);
  }
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->hoveredMaskIndex_ = target;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::copyHoveredMask() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  g_maskClipboard = layer->mask(impl_->hoveredMaskIndex_);
  return true;
}

bool CompositionRenderController::pasteMask() {
  if (!impl_ || !g_maskClipboard.has_value()) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  layer->addMask(*g_maskClipboard);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::toggleHoveredMaskInverted() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  impl_->beginMaskEditTransaction(layer);
  path.setInverted(!path.isInverted());
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::duplicateHoveredMaskForSelectedLayers() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const int maskIndex = impl_->hoveredMaskIndex_;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Duplicate Mask on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  bool changed = false;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex < 0 || maskIndex >= layer->maskCount() ||
        layer->mask(maskIndex).isLocked()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    layer->addMask(layer->mask(maskIndex));
    std::vector<LayerMask> afterMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::moveHoveredMaskForSelectedLayers(int direction) {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 || direction == 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const int sourceIndex = impl_->hoveredMaskIndex_;
  const int step = direction < 0 ? -1 : 1;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Move Mask on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  bool changed = false;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        sourceIndex < 0 || sourceIndex >= layer->maskCount()) {
      continue;
    }
    const int targetIndex = sourceIndex + step;
    if (targetIndex < 0 || targetIndex >= layer->maskCount() ||
        layer->mask(sourceIndex).isLocked() ||
        layer->mask(targetIndex).isLocked()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    std::vector<LayerMask> masks = beforeMasks;
    std::swap(masks[static_cast<size_t>(sourceIndex)],
              masks[static_cast<size_t>(targetIndex)]);
    for (int index = layer->maskCount() - 1; index >= 0; --index) {
      layer->removeMask(index);
    }
    for (const auto &mask : masks) {
      layer->addMask(mask);
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(masks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->hoveredMaskIndex_ = sourceIndex + step;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::toggleHoveredMaskInvertedForSelectedLayers() {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const auto primary = comp->layerById(impl_->selectedLayerId_);
  const int maskIndex = impl_->hoveredMaskIndex_;
  const int pathIndex = impl_->hoveredPathIndex_;
  if (!primary || maskIndex >= primary->maskCount() ||
      pathIndex >= primary->mask(maskIndex).maskPathCount() ||
      primary->mask(maskIndex).isLocked()) {
    return false;
  }
  const bool inverted =
      !primary->mask(maskIndex).maskPath(pathIndex).isInverted();
  bool changed = false;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Toggle Mask Inverted on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex >= layer->maskCount()) {
      continue;
    }
    auto mask = layer->mask(maskIndex);
    if (mask.isLocked() || pathIndex >= mask.maskPathCount()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    beforeMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    auto path = mask.maskPath(pathIndex);
    path.setInverted(inverted);
    mask.setMaskPath(pathIndex, path);
    layer->setMask(maskIndex, mask);
    std::vector<LayerMask> afterMasks;
    afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::adjustHoveredMaskGeometry(
    ArtifactCore::Units::LayerLocalLength featherDelta,
    ArtifactCore::Units::LayerLocalLength expansionDelta) {
  const float featherDeltaValue = featherDelta.value;
  const float expansionDeltaValue = expansionDelta.value;
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  impl_->beginMaskEditTransaction(layer);
  if (std::abs(featherDeltaValue) > 0.0f) {
    path.setFeather({
        std::max(0.0f, path.feather().value + featherDeltaValue)});
  }
  if (std::abs(expansionDeltaValue) > 0.0f) {
    path.setExpansion({path.expansion().value + expansionDeltaValue});
  }
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::adjustHoveredMaskOpacity(
    ArtifactCore::Units::OpacityDelta opacityDelta) {
  const float opacityDeltaValue = opacityDelta.value;
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 || std::abs(opacityDeltaValue) <= 0.0f) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked() || impl_->hoveredPathIndex_ >= mask.maskPathCount()) {
    return false;
  }
  MaskPath path = mask.maskPath(impl_->hoveredPathIndex_);
  const float nextOpacity =
      std::clamp(path.opacity() + opacityDeltaValue, 0.0f, 1.0f);
  if (std::abs(nextOpacity - path.opacity()) <= 0.0001f) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  path.setOpacity(nextOpacity);
  mask.setMaskPath(impl_->hoveredPathIndex_, path);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  setInfoOverlayText(
      QStringLiteral("Mask Opacity"),
      QStringLiteral("%1%").arg(QString::number(nextOpacity * 100.0f, 'f', 0)));
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::setHoveredMaskColor(const FloatColor& color) {
  if (!impl_ || impl_->hoveredMaskIndex_ < 0) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  if (!layer || impl_->hoveredMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  LayerMask mask = layer->mask(impl_->hoveredMaskIndex_);
  if (mask.isLocked()) {
    return false;
  }
  impl_->beginMaskEditTransaction(layer);
  mask.setColor(color);
  layer->setMask(impl_->hoveredMaskIndex_, mask);
  impl_->markMaskEditDirty();
  if (!impl_->commitMaskEditTransaction()) return false;
  impl_->publishLayerModified(layer, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::adjustHoveredMaskGeometryForSelectedLayers(
    ArtifactCore::Units::LayerLocalLength featherDelta,
    ArtifactCore::Units::LayerLocalLength expansionDelta) {
  const float featherDeltaValue = featherDelta.value;
  const float expansionDeltaValue = expansionDelta.value;
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 ||
      (std::abs(featherDeltaValue) <= 0.0f &&
       std::abs(expansionDeltaValue) <= 0.0f)) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const auto primary = comp->layerById(impl_->selectedLayerId_);
  const int maskIndex = impl_->hoveredMaskIndex_;
  const int pathIndex = impl_->hoveredPathIndex_;
  if (!primary || maskIndex >= primary->maskCount() ||
      pathIndex >= primary->mask(maskIndex).maskPathCount() ||
      primary->mask(maskIndex).isLocked()) {
    return false;
  }
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Adjust Mask Geometry on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  bool changed = false;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex >= layer->maskCount()) {
      continue;
    }
    auto mask = layer->mask(maskIndex);
    if (mask.isLocked() || pathIndex >= mask.maskPathCount()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    auto path = mask.maskPath(pathIndex);
    const float nextFeather =
        std::max(0.0f, path.feather().value + featherDeltaValue);
    const float nextExpansion = path.expansion().value + expansionDeltaValue;
    if (std::abs(nextFeather - path.feather().value) <= 0.0001f &&
        std::abs(nextExpansion - path.expansion().value) <= 0.0001f) {
      continue;
    }
    path.setFeather({nextFeather});
    path.setExpansion({nextExpansion});
    mask.setMaskPath(pathIndex, path);
    layer->setMask(maskIndex, mask);
    std::vector<LayerMask> afterMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

bool CompositionRenderController::adjustHoveredMaskOpacityForSelectedLayers(
    ArtifactCore::Units::OpacityDelta opacityDelta) {
  const float opacityDeltaValue = opacityDelta.value;
  if (!impl_ || impl_->hoveredMaskIndex_ < 0 ||
      impl_->hoveredPathIndex_ < 0 || std::abs(opacityDeltaValue) <= 0.0f) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!comp || !selection) {
    return false;
  }
  const auto primary = comp->layerById(impl_->selectedLayerId_);
  const int maskIndex = impl_->hoveredMaskIndex_;
  const int pathIndex = impl_->hoveredPathIndex_;
  if (!primary || maskIndex >= primary->maskCount() ||
      pathIndex >= primary->mask(maskIndex).maskPathCount() ||
      primary->mask(maskIndex).isLocked()) {
    return false;
  }
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Adjust Mask Opacity on Selected Layers"));
  std::vector<std::pair<ArtifactAbstractLayerPtr, std::vector<LayerMask>>>
      rollback;
  bool changed = false;
  for (const auto &layer : selection->selectedLayersInOrder()) {
    if (!layer || layer->isLocked() || layer->isSelectionLocked() ||
        maskIndex >= layer->maskCount()) {
      continue;
    }
    auto mask = layer->mask(maskIndex);
    if (mask.isLocked() || pathIndex >= mask.maskPathCount()) {
      continue;
    }
    std::vector<LayerMask> beforeMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      beforeMasks.push_back(layer->mask(index));
    }
    auto path = mask.maskPath(pathIndex);
    const float nextOpacity =
        std::clamp(path.opacity() + opacityDeltaValue, 0.0f, 1.0f);
    if (std::abs(nextOpacity - path.opacity()) <= 0.0001f) {
      continue;
    }
    path.setOpacity(nextOpacity);
    mask.setMaskPath(pathIndex, path);
    layer->setMask(maskIndex, mask);
    std::vector<LayerMask> afterMasks;
    for (int index = 0; index < layer->maskCount(); ++index) {
      afterMasks.push_back(layer->mask(index));
    }
    rollback.emplace_back(layer, beforeMasks);
    macro->addChild(std::make_unique<MaskEditCommand>(
        layer, std::move(beforeMasks), std::move(afterMasks)));
    changed = true;
  }
  if (changed) {
    auto *undo = UndoManager::instance();
    if (undo && !undo->push(std::move(macro))) {
      for (const auto &[layer, masks] : rollback) {
        restoreLayerMasks(layer, masks);
      }
      return false;
    }
    for (const auto &[layer, masks] : rollback) {
      impl_->publishLayerModified(layer, true);
    }
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return changed;
}

void CompositionRenderController::Impl::clearPendingMaskCreation() {

  pendingMaskCreation_ = false;

  maskSnapPreviewValid_ = false;

  maskSnapPreviewCanvasPos_ = {};

  pendingMaskLayerId_ = LayerID();

  pendingMaskPath_.clearVertices();

  pendingMaskPath_.setClosed(false);

  penToolPreviewVisible_ = false;

  penMaskPreviewValid_ = false;

}



void CompositionRenderController::Impl::clearRectangleToolSession() {

  rectangleToolDragging_ = false;

  rectangleToolFromCenter_ = false;
  rectangleToolRoundness_ = 0.0f;

  rectangleToolMode_ = RectangleToolMode::None;

  rectangleToolStartCanvasPos_ = {};

  rectangleToolCurrentCanvasPos_ = {};

  rectangleToolTargetLayer_.reset();

}



void CompositionRenderController::Impl::beginRectangleToolSession(

    RectangleToolMode mode, const ArtifactAbstractLayerPtr &layer,

    ArtifactCore::Coordinates::CompositionPoint2 canvasPos) {

  rectangleToolDragging_ = mode != RectangleToolMode::None;

  rectangleToolMode_ = mode;

  rectangleToolStartCanvasPos_ = canvasPos;

  rectangleToolCurrentCanvasPos_ = canvasPos;
  rectangleToolRoundness_ = 0.0f;

  rectangleToolTargetLayer_ = layer;

  if (!rectangleToolDragging_) {

    rectangleToolTargetLayer_.reset();

  }

}



void CompositionRenderController::Impl::beginPendingMaskCreation(

    const ArtifactAbstractLayerPtr &layer, const QPointF &localPos) {

  if (!layer) {

    return;

  }



  if (!pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {

    clearPendingMaskCreation();

    pendingMaskCreation_ = true;

    pendingMaskLayerId_ = layer->id();

  }



  MaskVertex vertex;

  vertex.position = localPos;

  vertex.inTangent = QPointF(0, 0);

  vertex.outTangent = QPointF(0, 0);

  pendingMaskPath_.addVertex(vertex);

  penMaskPreviewValid_ = true;

}



bool CompositionRenderController::Impl::finalizePendingMaskCreation(

    const ArtifactAbstractLayerPtr &layer) {

  if (!layer || !pendingMaskCreation_ || pendingMaskLayerId_ != layer->id()) {

    return false;

  }



  if (pendingMaskPath_.vertexCount() < 3) {

    return false;

  }



  MaskPath path = pendingMaskPath_;

  path.setClosed(true);



  beginMaskEditTransaction(layer);



  LayerMask newMask;

  newMask.addMaskPath(path);

  layer->addMask(newMask);

  markMaskEditDirty();


  if (!commitMaskEditTransaction()) {
    return false;
  }

  clearPendingMaskCreation();

  return true;

}

void CompositionRenderController::Impl::beginPendingShapePathCreation(
    const ArtifactAbstractLayerPtr &layer, const QPointF &localPos) {

  if (!layer) {

    return;

  }



  if (!pendingShapePathCreation_ ||
      pendingShapePathLayerId_ != layer->id()) {

    clearPendingShapePathCreation();

    pendingShapePathCreation_ = true;

    pendingShapePathLayerId_ = layer->id();

  }


  CustomPathVertex vertex;

  vertex.pos = localPos;

  vertex.inTangent = QPointF(0, 0);

  vertex.outTangent = QPointF(0, 0);

  vertex.smooth = false;

  pendingShapePathVertices_.push_back(vertex);

}



void CompositionRenderController::Impl::clearPendingShapePathCreation() {

  pendingShapePathCreation_ = false;

  pendingShapePathLayerId_ = LayerID();

  pendingShapePathVertices_.clear();

  penMaskPreviewValid_ = false;

}



bool CompositionRenderController::Impl::finalizePendingShapePathCreation(
    const ArtifactAbstractLayerPtr &layer) {

  if (!layer || !pendingShapePathCreation_ ||
      pendingShapePathLayerId_ != layer->id()) {

    return false;

  }


  auto *shape = dynamic_cast<ArtifactShapeLayer *>(layer.get());

  if (!shape || pendingShapePathVertices_.size() < 2) {

    return false;

  }


  const auto beforeVertices = shape->customPathVertices();
  const bool beforeClosed = shape->customPathClosed();
  const QJsonObject beforeGeometry = shape->customGeometrySnapshot();
  const auto afterVertices = pendingShapePathVertices_;

  shape->setCustomPathVertices(pendingShapePathVertices_, true);

  shape->setDirty(LayerDirtyFlag::Source);

  auto *undo = UndoManager::instance();
   const bool pushed = !undo || undo->push(
      std::make_unique<ShapePathVertexEditCommand>(
          layer, beforeVertices, afterVertices, beforeClosed, true,
          beforeGeometry, shape->customGeometrySnapshot()));
  if (!pushed) {
    shape->restoreCustomGeometrySnapshot(beforeGeometry);
    shape->setDirty(LayerDirtyFlag::Source);
    shape->changed();
    return false;
  }

  clearPendingShapePathCreation();

  return true;

}



bool CompositionRenderController::Impl::removeLastPendingShapePathVertex() {

  if (!pendingShapePathCreation_ || pendingShapePathVertices_.empty()) {

    return false;

  }

  pendingShapePathVertices_.pop_back();

  if (pendingShapePathVertices_.empty()) {

    clearPendingShapePathCreation();

  }

  return true;

}

void CompositionRenderController::Impl::beginMaskEditTransaction(const ArtifactAbstractLayerPtr &layer) {

  if (!layer) {

    return;

  }

  maskEditPending_ = true;

  maskEditDirty_ = false;

  maskEditLayer_ = layer;

  maskEditBefore_.clear();

  maskEditBefore_.reserve(static_cast<size_t>(layer->maskCount()));

  for (int i = 0; i < layer->maskCount(); ++i) {

    maskEditBefore_.push_back(layer->mask(i));

  }

}

void CompositionRenderController::Impl::markMaskEditDirty() {

  if (maskEditPending_) {

    maskEditDirty_ = true;

  }

}

void CompositionRenderController::Impl::resetLayerMutationNotify() {

  lastLayerMutationNotify_ = {};

}

bool CompositionRenderController::Impl::shouldPublishLayerMutation() {

  constexpr auto kLayerMutationNotifyInterval = std::chrono::milliseconds(16);

  const auto now = std::chrono::steady_clock::now();

  if (lastLayerMutationNotify_.time_since_epoch().count() == 0 ||

      now - lastLayerMutationNotify_ >= kLayerMutationNotifyInterval) {

    lastLayerMutationNotify_ = now;

    return true;

  }

  return false;

}

void CompositionRenderController::Impl::publishLayerModified(const ArtifactAbstractLayerPtr& layer, bool force) {

  if (!layer) {

    return;

  }

  if (!force && !shouldPublishLayerMutation()) {

    return;

  }

  if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {

    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(

        LayerChangedEvent{comp->id().toString(), layer->id().toString(),

                          LayerChangedEvent::ChangeType::Modified});

  }

}

bool CompositionRenderController::Impl::commitMaskEditTransaction() {

  if (!maskEditPending_) {

    return true;

  }



  auto layer = maskEditLayer_.lock();

  maskEditPending_ = false;

  maskEditLayer_.reset();



  if (!layer || !maskEditDirty_) {

    maskEditBefore_.clear();

    maskEditDirty_ = false;

    return true;

  }



  std::vector<LayerMask> afterMasks;

  afterMasks.reserve(static_cast<size_t>(layer->maskCount()));

  for (int i = 0; i < layer->maskCount(); ++i) {

    afterMasks.push_back(layer->mask(i));

  }



  auto *undo = UndoManager::instance();

  const bool pushed =
      !undo || undo->push(std::make_unique<MaskEditCommand>(
                            layer, maskEditBefore_, std::move(afterMasks)));

  if (!pushed) {
    restoreLayerMasks(layer, maskEditBefore_);

    invalidateBaseComposite();

    invalidateOverlayComposite();

  }



  maskEditBefore_.clear();

  maskEditDirty_ = false;

  return pushed;

}

bool CompositionRenderController::Impl::cyclePresetLayerMaskForLayer(const ArtifactAbstractLayerPtr &layer,

                                  bool reverse) {

  if (!layer) {

    return false;

  }



  const QRectF localBounds = layer->localBounds().normalized();

  if (!localBounds.isValid() || localBounds.width() < 1.0 ||

      localBounds.height() < 1.0) {

    return false;

  }



  const auto now = std::chrono::steady_clock::now();

  constexpr auto kCycleWindow = std::chrono::seconds(4);

  const bool isSameLayer =

      !quickMaskPresetLayerId_.isNil() && quickMaskPresetLayerId_ == layer->id();

  const bool isArmed = isSameLayer &&

                       quickMaskPresetStartedAt_.time_since_epoch().count() > 0 &&

                       now - quickMaskPresetStartedAt_ <= kCycleWindow;



  if (!isArmed) {

    quickMaskPresetLayerId_ = layer->id();

    quickMaskPresetStartedAt_ = now;

    quickMaskPresetIndex_ =

        reverse ? static_cast<int>(kQuickMaskPresetOrder.size()) - 1 : 0;

    quickMaskPresetMaskIndex_ = -1;

  } else if (reverse) {

    quickMaskPresetIndex_ =

        (quickMaskPresetIndex_ - 1 + static_cast<int>(kQuickMaskPresetOrder.size())) %

        static_cast<int>(kQuickMaskPresetOrder.size());

  } else {

    quickMaskPresetIndex_ =

        (quickMaskPresetIndex_ + 1) % static_cast<int>(kQuickMaskPresetOrder.size());

  }



  const QuickMaskPreset preset =

      kQuickMaskPresetOrder[static_cast<size_t>(quickMaskPresetIndex_)];

  const QRectF presetRect = quickMaskPresetRect(preset, localBounds);



  MaskPath path;

  for (const QPointF &corner : rectCorners(presetRect)) {

    MaskVertex vertex;

    vertex.position = corner;

    vertex.inTangent = QPointF(0.0, 0.0);

    vertex.outTangent = QPointF(0.0, 0.0);

    path.addVertex(vertex);

  }

  path.setClosed(true);



  LayerMask mask;

  mask.addMaskPath(path);



  beginMaskEditTransaction(layer);

  if (isArmed && quickMaskPresetMaskIndex_ >= 0 &&

      quickMaskPresetMaskIndex_ < layer->maskCount()) {

    layer->setMask(quickMaskPresetMaskIndex_, mask);

  } else {

    layer->addMask(mask);

    quickMaskPresetMaskIndex_ = layer->maskCount() - 1;

  }

  markMaskEditDirty();

  if (!commitMaskEditTransaction()) return false;

  publishLayerModified(layer, true);

  return true;

}

namespace CompositionRenderControllerDetail {





} // namespace CompositionRenderControllerDetail

bool CompositionRenderController::createFullLayerMaskForLayer(

    const ArtifactAbstractLayerPtr &layer) {

  return cyclePresetLayerMaskForLayer(layer, false);

}



bool CompositionRenderController::cyclePresetLayerMaskForLayer(

    const ArtifactAbstractLayerPtr &layer, bool reverse) {

  if (!impl_ || !layer) {

    return false;

  }

  return impl_->cyclePresetLayerMaskForLayer(layer, reverse);

}

bool CompositionRenderController::adjustRectangleToolRoundness(float delta) {
  if (!impl_ || !impl_->rectangleToolDragging_ ||
      (impl_->rectangleToolMode_ != RectangleToolMode::Shape &&
       impl_->rectangleToolMode_ != RectangleToolMode::EllipseShape)) {
    return false;
  }
  const QRectF rect = dragRectFromPoints(impl_->rectangleToolStartCanvasPos_,
                                         impl_->rectangleToolCurrentCanvasPos_,
                                         impl_->rectangleToolFromCenter_)
                          .normalized();
  const float maximum = std::max(
      0.0f, 0.5f * static_cast<float>(std::min(rect.width(), rect.height())));
  impl_->rectangleToolRoundness_ = std::clamp(
      impl_->rectangleToolRoundness_ + delta, 0.0f, maximum);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::adjustSelectedShapeCornerRadius(float delta) {
  if (!impl_ || impl_->rectangleToolDragging_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
                         ? comp->layerById(impl_->selectedLayerId_)
                         : ArtifactAbstractLayerPtr{};
  const auto shape = layer
                         ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
                         : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
  if (!shape) return false;
  const QRectF bounds = shape->localBounds();
  const float maximum = std::max(
      0.0f, 0.5f * static_cast<float>(std::min(bounds.width(), bounds.height())));
  const float before = shape->cornerRadius();
  const float after = std::clamp(before + delta, 0.0f, maximum);
  if (std::abs(after - before) < 0.001f) return true;
  shape->setCornerRadius(after);
  shape->setDirty(LayerDirtyFlag::Property);
  shape->changed();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<ShapeCornerRadiusUndoCommand>(
                           shape, before, after))) {
    shape->setCornerRadius(before);
    shape->setDirty(LayerDirtyFlag::Property);
    shape->changed();
    return false;
  }
  impl_->publishLayerModified(shape, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::resetSelectedShapeCornerRadius() {
  if (!impl_ || impl_->rectangleToolDragging_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
                         ? comp->layerById(impl_->selectedLayerId_)
                         : ArtifactAbstractLayerPtr{};
  const auto shape = layer
                         ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(layer)
                         : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
  if (!shape || shape->cornerRadius() <= 0.001f) return false;
  const float before = shape->cornerRadius();
  shape->setCornerRadius(0.0f);
  shape->setDirty(LayerDirtyFlag::Property);
  shape->changed();
  auto *manager = UndoManager::instance();
  if (manager && !manager->push(std::make_unique<ShapeCornerRadiusUndoCommand>(
                           shape, before, 0.0f))) {
    shape->setCornerRadius(before);
    shape->setDirty(LayerDirtyFlag::Property);
    shape->changed();
    return false;
  }
  impl_->publishLayerModified(shape, true);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

} // namespace Artifact
