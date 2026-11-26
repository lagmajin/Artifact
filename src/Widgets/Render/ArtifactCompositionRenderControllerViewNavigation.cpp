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

float CompositionRenderController::viewportZoom() const {

  return impl_ && impl_->renderer_ ? impl_->renderer_->getZoom() : 1.0f;

}

bool CompositionRenderController::canUndoView() const {

  return impl_ && !impl_->viewUndoStack_.empty();

}

bool CompositionRenderController::canRedoView() const {

  return impl_ && !impl_->viewRedoStack_.empty();

}

void CompositionRenderController::pushViewHistory() {

  if (!impl_) {

    return;

  }

  impl_->pushViewHistory();

}

void CompositionRenderController::undoView() {

  if (!impl_ || impl_->viewUndoStack_.empty()) {

    return;

  }

  impl_->viewRedoStack_.push_back(impl_->captureViewState());
  const auto state = impl_->viewUndoStack_.back();
  impl_->viewUndoStack_.pop_back();
  impl_->applyViewState(state);

}

void CompositionRenderController::redoView() {

  if (!impl_ || impl_->viewRedoStack_.empty()) {

    return;

  }

  impl_->viewUndoStack_.push_back(impl_->captureViewState());
  const auto state = impl_->viewRedoStack_.back();
  impl_->viewRedoStack_.pop_back();
  impl_->applyViewState(state);

}



void CompositionRenderController::resetView() {

  // A full reset is a deliberate return to the composition view, so leave
  // Fly / Walk first instead of resetting state the mode would restore.
  if (impl_->viewportNavigationMode_ != ViewportNavigationMode::Off) {
    endViewportNavigation();
  }

  if (impl_->renderer_) {
    ++impl_->smoothZoomGeneration_;
    impl_->smoothZoomActive_ = false;

    impl_->pushViewHistory();
    impl_->renderer_->resetView();

    // P0-2 tumble pivot is a preview-only override; clear it on full reset
    // so a saved camera target stays in sync with the cleared view.
    impl_->tumblePivotOverrideEnabled_ = false;
    impl_->tumblePivotCanvasPos_ = {};

    impl_->invalidateBaseComposite();

    markRenderDirty();

  }

}

// P0-1 Box zoom / crop interaction. Begin accepts the logical press
// position (mirroring the QMouseEvent::position() coordinate space used by
// the other handleMouse* helpers) and reserves the right to ignore it if
// a higher-priority modality is active. Update is fed raw logical mouse
// positions; the internal state stores physical positions to stay
// consistent with viewportRectToCanvasRect and the rubber-band pipeline.
bool CompositionRenderController::beginBoxZoomInteraction(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos,
    bool cropWindow) {
  if (!impl_ || !impl_->renderer_) {
    return false;
  }
  // Stay exclusive with the rubber-band selection marquee and gizmos.
  if (impl_->isRubberBandSelecting_ || impl_->isLassoSelecting_ ||
      impl_->isShapeVertexMarqueeSelecting_ ||
      isModalGizmoInteractionActive() || isTextEditSessionActive() ||
      isInteractionBusy()) {
    return false;
  }
  const auto physicalPos = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  impl_->isBoxZooming_ = true;
  impl_->boxZoomCropWindow_ = cropWindow;
  impl_->boxZoomStartViewportPos_ = physicalPos;
  impl_->boxZoomCurrentViewportPos_ = physicalPos;
  markRenderDirty();
  return true;
}

void CompositionRenderController::updateBoxZoomInteraction(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->isBoxZooming_) {
    return;
  }
  impl_->boxZoomCurrentViewportPos_ =
      ArtifactCore::Coordinates::toScreenPhysical(
          viewportPos, impl_->devicePixelRatio_);
  markRenderDirty();
}

bool CompositionRenderController::endBoxZoomInteraction() {
  if (!impl_ || !impl_->isBoxZooming_) {
    return false;
  }
  const auto startViewport = impl_->boxZoomStartViewportPos_;
  const auto endViewport = impl_->boxZoomCurrentViewportPos_;
  const bool cropWindow = impl_->boxZoomCropWindow_;
  impl_->isBoxZooming_ = false;
  impl_->boxZoomStartViewportPos_ = {};
  impl_->boxZoomCurrentViewportPos_ = {};
  if (!impl_->renderer_) {
    markRenderDirty();
    return false;
  }
  // Crop window is reserved for a later milestone; do not mutate the view.
  if (cropWindow) {
    markRenderDirty();
    return true;
  }
  // Mirror the rubber-band threshold so a jittery click does not zoom.
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const float threshold = 6.0f / zoom;
  const auto delta = endViewport - startViewport;
  if (std::abs(delta.x) + std::abs(delta.y) < threshold) {
    markRenderDirty();
    return false;
  }
  const QRectF canvasRect = viewportRectToCanvasRect(
      impl_->renderer_.get(),
      ArtifactCore::Coordinates::toQPointF(startViewport),
      ArtifactCore::Coordinates::toQPointF(endViewport)).normalized();
  if (canvasRect.isEmpty() || canvasRect.width() < 1.0f ||
      canvasRect.height() < 1.0f) {
    markRenderDirty();
    return false;
  }
  impl_->pushViewHistory();
  // Compute the zoom factor that fits the canvas rectangle into the
  // viewport. The renderer does not expose a single fit-to-rect entry, so
  // we derive the ratio directly and apply it via the smooth-zoom anchor.
  const float viewportW = std::max(1.0f, impl_->hostWidth_);
  const float viewportH = std::max(1.0f, impl_->hostHeight_);
  const float fitX = viewportW / std::max(1.0f,
      static_cast<float>(canvasRect.width()));
  const float fitY = viewportH / std::max(1.0f,
      static_cast<float>(canvasRect.height()));
  const float targetZoom = std::min(fitX, fitY);
  const ArtifactCore::Coordinates::ScreenPhysicalPoint2 centerViewport{
      (startViewport.x + endViewport.x) * 0.5f,
      (startViewport.y + endViewport.y) * 0.5f};
  const float currentZoom = std::max(0.001f, impl_->renderer_->getZoom());
  const ArtifactCore::Units::ScaleFactor factor{
      std::isfinite(targetZoom) && targetZoom > 0.0f
          ? (targetZoom / currentZoom)
          : 1.0f};
  const float devicePixelRatio = std::max(impl_->devicePixelRatio_, 0.001f);
  const ArtifactCore::Coordinates::ScreenLogicalPoint2 centerViewportLogical{
      centerViewport.x / devicePixelRatio,
      centerViewport.y / devicePixelRatio};
  zoomAtFactor(
      centerViewportLogical, factor);
  impl_->invalidateBaseComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::cancelBoxZoomInteraction() {
  if (!impl_ || !impl_->isBoxZooming_) {
    return;
  }
  impl_->isBoxZooming_ = false;
  impl_->boxZoomStartViewportPos_ = {};
  impl_->boxZoomCurrentViewportPos_ = {};
  markRenderDirty();
}

bool CompositionRenderController::isBoxZoomInteractionActive() const {
  return impl_ && impl_->isBoxZooming_;
}

// P0-2 Tumble pivot under cursor. Stores the cursor point in canvas space
// and feeds it into the viewport orientation view matrix as a temporary
// look-at target. The camera layer's stored target is never modified.
bool CompositionRenderController::setTumblePivotAtViewportPos(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->renderer_) {
    return false;
  }
  // Front orthographic does not tumble; refuse to leave a stale override.
  if (!impl_->usesSpatialViewportOrientation()) {
    impl_->tumblePivotOverrideEnabled_ = false;
    impl_->tumblePivotCanvasPos_ = {};
    markRenderDirty();
    return false;
  }
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  const QRectF canvasRect = viewportRectToCanvasRect(
      impl_->renderer_.get(), physicalPos, physicalPos);
  if (canvasRect.isEmpty()) {
    return false;
  }
  if (!impl_->tumblePivotOverrideEnabled_) {
    impl_->pushViewHistory();
  }
  impl_->tumblePivotOverrideEnabled_ = true;
  impl_->tumblePivotCanvasPos_ =
      ArtifactCore::Coordinates::compositionPointFromQPointF(
          QPointF(canvasRect.x(), canvasRect.y()));
  impl_->invalidateBaseComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::clearTumblePivot() {
  if (!impl_ || !impl_->tumblePivotOverrideEnabled_) {
    return;
  }
  impl_->pushViewHistory();
  impl_->tumblePivotOverrideEnabled_ = false;
  impl_->tumblePivotCanvasPos_ = {};
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

bool CompositionRenderController::isTumblePivotOverrideEnabled() const {
  return impl_ && impl_->tumblePivotOverrideEnabled_;
}

ArtifactCore::Coordinates::CompositionPoint2
CompositionRenderController::tumblePivotCanvasPos() const {
  return impl_ ? impl_->tumblePivotCanvasPos_
               : ArtifactCore::Coordinates::CompositionPoint2{};
}

// P0-3a Interactive Render Region. The rectangle is owned by the
// controller; render-path integration (RenderContext::roi) is deferred
// to a separate milestone and intentionally out of scope here.
void CompositionRenderController::setInteractiveRenderRegion(
    CompositionBounds2 canvasBounds) {
  if (!impl_ || !impl_->renderer_) {
    return;
  }
  const QRectF canvasRect(
      ArtifactCore::Coordinates::toQPointF(canvasBounds.minimum),
      ArtifactCore::Coordinates::toQPointF(canvasBounds.maximum));
  QRectF normalized = canvasRect.normalized();
  if (normalized.width() < 2.0f || normalized.height() < 2.0f) {
    return;
  }
  impl_->interactiveRenderRegionActive_ = true;
  impl_->interactiveRenderRegionRect_ = normalized;
  // P0-3d / decision doc D4: force quality preset to Preview while an
  // IRR is active. The original preset is restored on clear.
  if (!impl_->irrForcedPreview_ &&
      impl_->previewQualityPreset_ == PreviewQualityPreset::Final) {
    impl_->irrForcedPreview_ = true;
    setPreviewQualityPreset(PreviewQualityPreset::Preview);
  }
  // HUD readout (display-only). The rect does not yet drive a partial
  // re-render in P0-3a; P0-3b will consume it.
  const int widthPercent = static_cast<int>(std::round(
      impl_->interactiveRenderRegionResolutionScale_ * 100.0f));
  QString detail = QStringLiteral(
      "%1%% res  %2 x %3").arg(widthPercent)
      .arg(static_cast<int>(normalized.width()))
      .arg(static_cast<int>(normalized.height()));
  if (impl_->irrForcedPreview_) {
    detail += QStringLiteral("  (quality forced to Preview)");
  }
  setInfoOverlayText(QStringLiteral("IRR"), detail);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::clearInteractiveRenderRegion() {
  if (!impl_ || !impl_->interactiveRenderRegionActive_) {
    return;
  }
  impl_->interactiveRenderRegionActive_ = false;
  impl_->interactiveRenderRegionRect_ = {};
  impl_->interactiveRenderRegionHandleDrag_ = 0;
  // P0-3d / decision doc D4: restore the user-selected preset if it was
  // downgraded by setInteractiveRenderRegion. The previous value lives
  // in previewQualityPreset_ because setPreviewQualityPreset() saves it.
  if (impl_->irrForcedPreview_) {
    impl_->irrForcedPreview_ = false;
    setPreviewQualityPreset(impl_->previewQualityPreset_);
  }
  setInfoOverlayText(QString(), QString());
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isInteractiveRenderRegionActive() const {
  return impl_ && impl_->interactiveRenderRegionActive_;
}

CompositionBounds2 CompositionRenderController::interactiveRenderRegion() const {
  if (!impl_) {
    return {};
  }
  const QRectF &rect = impl_->interactiveRenderRegionRect_;
  return {{rect.left(), rect.top()}, {rect.right(), rect.bottom()}};
}

void CompositionRenderController::setInteractiveRenderRegionResolutionScale(
    float scale) {
  if (!impl_) {
    return;
  }
  impl_->interactiveRenderRegionResolutionScale_ = std::isnan(scale)
      ? 1.0f : std::clamp(scale, 0.25f, 1.0f);
  markRenderDirty();
}

float CompositionRenderController::interactiveRenderRegionResolutionScale()
    const {
  return impl_ ? impl_->interactiveRenderRegionResolutionScale_ : 1.0f;
}

// P0-3a IRR handle hit-testing. The handle IDs are:
//   0 = no hit, 1 = move, 2..9 = NW, N, NE, E, SE, S, SW, W.
// The canvas-space rect is converted to a viewport-space rect via the
// current pan/zoom and compared against the click position.
int CompositionRenderController::interactiveRenderRegionHandleAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {
  if (!impl_ || !impl_->renderer_ ||
      !impl_->interactiveRenderRegionActive_) {
    return 0;
  }
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  float panX = 0.0f;
  float panY = 0.0f;
  impl_->renderer_->getPan(panX, panY);
  const QRectF canvasRect = impl_->interactiveRenderRegionRect_;
  const QRectF viewportRect(
      canvasRect.left() * zoom + panX,
      canvasRect.top() * zoom + panY,
      canvasRect.width() * zoom,
      canvasRect.height() * zoom);
  const auto viewportPhysical = ArtifactCore::Coordinates::toScreenPhysical(
      viewportPos, impl_->devicePixelRatio_);
  // Move hitbox = interior of the rect.
  if (viewportRect.contains(viewportPhysical.x, viewportPhysical.y)) {
    return 1;
  }
  const float hitRadius = std::max(8.0f, 12.0f);
  const auto radiusPhysical = ArtifactCore::Coordinates::toScreenPhysical(
      ArtifactCore::Coordinates::ScreenLogicalVector2{hitRadius, hitRadius},
      impl_->devicePixelRatio_);
  const float physicalRadius = std::max(radiusPhysical.x, radiusPhysical.y);
  auto nearPoint = [&](float px, float py) {
    return std::hypot(viewportPhysical.x - px, viewportPhysical.y - py) <=
           physicalRadius;
  };
  if (nearPoint(viewportRect.left(), viewportRect.top())) return 2;
  if (nearPoint(viewportRect.center().x(), viewportRect.top())) return 3;
  if (nearPoint(viewportRect.right(), viewportRect.top())) return 4;
  if (nearPoint(viewportRect.right(), viewportRect.center().y())) return 5;
  if (nearPoint(viewportRect.right(), viewportRect.bottom())) return 6;
  if (nearPoint(viewportRect.center().x(), viewportRect.bottom())) return 7;
  if (nearPoint(viewportRect.left(), viewportRect.bottom())) return 8;
  if (nearPoint(viewportRect.left(), viewportRect.center().y())) return 9;
  return 0;
}

bool CompositionRenderController::beginInteractiveRenderRegionDrag(
    int handle, ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->renderer_ ||
      !impl_->interactiveRenderRegionActive_ || handle <= 0 || handle > 9) {
    return false;
  }
  impl_->interactiveRenderRegionHandleDrag_ = handle;
  impl_->interactiveRenderRegionDragStartViewport_ = viewportPos;
  impl_->interactiveRenderRegionDragStartRect_ =
      impl_->interactiveRenderRegionRect_;
  markRenderDirty();
  return true;
}

void CompositionRenderController::updateInteractiveRenderRegionDrag(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_ || !impl_->renderer_ ||
      impl_->interactiveRenderRegionHandleDrag_ == 0) {
    return;
  }
  const float zoom = std::max(0.001f, impl_->renderer_->getZoom());
  const auto deltaLogical = ArtifactCore::Coordinates::ScreenLogicalVector2{
      viewportPos.x - impl_->interactiveRenderRegionDragStartViewport_.x,
      viewportPos.y - impl_->interactiveRenderRegionDragStartViewport_.y};
  const float dx = deltaLogical.x * zoom;
  const float dy = deltaLogical.y * zoom;
  const QRectF &startRect = impl_->interactiveRenderRegionDragStartRect_;
  QRectF updated = startRect;
  const int handle = impl_->interactiveRenderRegionHandleDrag_;
  // 1 = move, others adjust edges.
  if (handle == 1) {
    updated.translate(dx, dy);
  } else {
    float left = static_cast<float>(startRect.left());
    float right = static_cast<float>(startRect.right());
    float top = static_cast<float>(startRect.top());
    float bottom = static_cast<float>(startRect.bottom());
    auto moveLeft = [&](bool on) {
      if (on) left += dx;
    };
    auto moveRight = [&](bool on) {
      if (on) right += dx;
    };
    auto moveTop = [&](bool on) {
      if (on) top += dy;
    };
    auto moveBottom = [&](bool on) {
      if (on) bottom += dy;
    };
    switch (handle) {
    case 2: moveLeft(true); moveTop(true); break;
    case 3: moveTop(true); break;
    case 4: moveRight(true); moveTop(true); break;
    case 5: moveRight(true); break;
    case 6: moveRight(true); moveBottom(true); break;
    case 7: moveBottom(true); break;
    case 8: moveLeft(true); moveBottom(true); break;
    case 9: moveLeft(true); break;
    default: break;
    }
    if (right - left < 2.0f) {
      if (handle == 2 || handle == 8 || handle == 9) left = right - 2.0f;
      else right = left + 2.0f;
    }
    if (bottom - top < 2.0f) {
      if (handle == 2 || handle == 3 || handle == 4) top = bottom - 2.0f;
      else bottom = top + 2.0f;
    }
    updated = QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
  }
  impl_->interactiveRenderRegionRect_ = updated;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::endInteractiveRenderRegionDrag() {
  if (!impl_ || impl_->interactiveRenderRegionHandleDrag_ == 0) {
    return false;
  }
  impl_->interactiveRenderRegionHandleDrag_ = 0;
  impl_->interactiveRenderRegionDragStartViewport_ = {};
  impl_->interactiveRenderRegionDragStartRect_ = {};
  markRenderDirty();
  return true;
}

void CompositionRenderController::cancelInteractiveRenderRegionDrag() {
  if (!impl_ || impl_->interactiveRenderRegionHandleDrag_ == 0) {
    return;
  }
  impl_->interactiveRenderRegionHandleDrag_ = 0;
  impl_->interactiveRenderRegionRect_ =
      impl_->interactiveRenderRegionDragStartRect_;
  impl_->interactiveRenderRegionDragStartViewport_ = {};
  impl_->interactiveRenderRegionDragStartRect_ = {};
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isInteractiveRenderRegionDragActive()
    const {
  return impl_ && impl_->interactiveRenderRegionHandleDrag_ != 0;
}



void CompositionRenderController::zoomInAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (impl_->renderer_) {
    zoomAtFactor(viewportPos, ArtifactCore::Units::ScaleFactor{1.1f});

  }

}



void CompositionRenderController::zoomOutAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (impl_->renderer_) {
    zoomAtFactor(viewportPos,
                 ArtifactCore::Units::ScaleFactor{1.0f / 1.1f});

  }

}



void CompositionRenderController::zoomAtFactor(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos,
    ArtifactCore::Units::ScaleFactor factor) {

  if (!impl_->renderer_) {

    return;

  }

  if (!std::isfinite(factor.value) || factor.value <= 0.0f) {
    return;
  }
  if (!std::isfinite(viewportPos.x) ||
      !std::isfinite(viewportPos.y)) {
    return;
  }
  if (impl_->viewportNavigationMode_ != ViewportNavigationMode::Off) {
    return;
  }

  notifyViewportInteractionActivity();
  if (impl_->interactionPerfEnabled_) impl_->interactionPerfAction_ |= 2;

  if (impl_->restoringViewState_) {
    return;
  }

  if (!impl_->smoothZoomActive_) {
    impl_->pushViewHistory();
  }

  const float currentZoom = impl_->renderer_->getZoom();

  const float baseZoom = impl_->smoothZoomActive_
      ? impl_->smoothZoomTarget_
      : currentZoom;
  const QPointF physicalAnchor =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  impl_->smoothZoomStart_ = currentZoom;
  impl_->smoothZoomTarget_ = std::clamp(
      baseZoom * factor.value, kMinimumViewportZoom, kMaximumViewportZoom);
  impl_->smoothZoomAnchorViewportPx_ = physicalAnchor;
  impl_->smoothZoomStartedAt_ = std::chrono::steady_clock::now();
  impl_->smoothZoomActive_ = true;
  const quint64 generation = ++impl_->smoothZoomGeneration_;

  auto step = std::make_shared<std::function<void()>>();
  const std::weak_ptr<std::function<void()>> weakStep = step;
  *step = [this, weakStep, generation]() {
    if (!impl_ || !impl_->renderer_ || !impl_->smoothZoomActive_ ||
        generation != impl_->smoothZoomGeneration_) return;
    constexpr auto kDuration = std::chrono::milliseconds(150);
    const auto elapsed = std::chrono::steady_clock::now() -
                         impl_->smoothZoomStartedAt_;
    const float t = std::clamp(
        static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
            static_cast<float>(kDuration.count()),
        0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);
    const float zoom = impl_->smoothZoomStart_ +
        (impl_->smoothZoomTarget_ - impl_->smoothZoomStart_) * eased;
    impl_->renderer_->zoomAroundViewportPoint(
        {static_cast<float>(impl_->smoothZoomAnchorViewportPx_.x()),
         static_cast<float>(impl_->smoothZoomAnchorViewportPx_.y())},
        zoom);
    impl_->invalidateBaseComposite();
    markRenderDirty();
    if (t >= 1.0f) {
      impl_->smoothZoomActive_ = false;
      return;
    }
    if (const auto nextStep = weakStep.lock()) {
      QTimer::singleShot(16, this, [nextStep]() { (*nextStep)(); });
    }
  };
  QTimer::singleShot(0, this, [step]() { (*step)(); });

}



void CompositionRenderController::zoomFit() {

  if (impl_->renderer_) {
    ++impl_->smoothZoomGeneration_;
    impl_->smoothZoomActive_ = false;

    impl_->pushViewHistory();
    impl_->renderer_->fitToViewport(0.0f);

    impl_->invalidateBaseComposite();

    float panX, panY;

    impl_->renderer_->getPan(panX, panY);

    const float zoom = impl_->renderer_->getZoom();

    qDebug() << "[CompositionRenderController] zoomFit done:"

             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"

             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;

    markRenderDirty();

  }

}



void CompositionRenderController::zoomFitSelection() {
  if (!impl_->renderer_ || impl_->selectedLayerId_.isNil()) {
    return;
  }
  ++impl_->smoothZoomGeneration_;
  impl_->smoothZoomActive_ = false;
  const auto composition = impl_->previewPipeline_.composition();
  if (!composition) {
    return;
  }
  const auto layer = composition->layerById(impl_->selectedLayerId_);
  if (!layer) {
    return;
  }
  const QRectF bounds = layer->transformedBoundingBox().normalized();
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0 ||
      impl_->hostWidth_ <= 0 || impl_->hostHeight_ <= 0) {
    return;
  }

  impl_->pushViewHistory();
  const float margin = 0.05f *
      static_cast<float>(std::min(impl_->hostWidth_, impl_->hostHeight_));
  const float availableWidth = std::max(
      1.0f, static_cast<float>(impl_->hostWidth_) - 2.0f * margin);
  const float availableHeight = std::max(
      1.0f, static_cast<float>(impl_->hostHeight_) - 2.0f * margin);
  const float zoom = std::clamp(
      std::min(availableWidth / static_cast<float>(bounds.width()),
               availableHeight / static_cast<float>(bounds.height())),
      0.05f, 64.0f);
  impl_->renderer_->setZoom(zoom);
  impl_->renderer_->setPan(
      static_cast<float>(impl_->hostWidth_) * 0.5f -
          static_cast<float>(bounds.center().x()) * zoom,
      static_cast<float>(impl_->hostHeight_) * 0.5f -
          static_cast<float>(bounds.center().y()) * zoom);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

void CompositionRenderController::zoomFitVisible() {
  if (!impl_->renderer_) {
    return;
  }
  ++impl_->smoothZoomGeneration_;
  impl_->smoothZoomActive_ = false;
  const auto composition = impl_->previewPipeline_.composition();
  if (!composition) {
    return;
  }

  QRectF bounds;
  bool hasBounds = false;
  for (const auto& layer : composition->allLayer()) {
    if (!layer || !layer->isVisible()) {
      continue;
    }
    const QRectF layerBounds = layer->transformedBoundingBox().normalized();
    if (!layerBounds.isValid() || layerBounds.width() <= 0.0 ||
        layerBounds.height() <= 0.0) {
      continue;
    }
    bounds = hasBounds ? bounds.united(layerBounds) : layerBounds;
    hasBounds = true;
  }
  if (!hasBounds || impl_->hostWidth_ <= 0 || impl_->hostHeight_ <= 0) {
    return;
  }

  impl_->pushViewHistory();
  const float margin = 0.05f *
      static_cast<float>(std::min(impl_->hostWidth_, impl_->hostHeight_));
  const float availableWidth = std::max(
      1.0f, static_cast<float>(impl_->hostWidth_) - 2.0f * margin);
  const float availableHeight = std::max(
      1.0f, static_cast<float>(impl_->hostHeight_) - 2.0f * margin);
  const float zoom = std::clamp(
      std::min(availableWidth / static_cast<float>(bounds.width()),
               availableHeight / static_cast<float>(bounds.height())),
      0.05f, 64.0f);
  impl_->renderer_->setZoom(zoom);
  impl_->renderer_->setPan(
      static_cast<float>(impl_->hostWidth_) * 0.5f -
          static_cast<float>(bounds.center().x()) * zoom,
      static_cast<float>(impl_->hostHeight_) * 0.5f -
          static_cast<float>(bounds.center().y()) * zoom);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

void CompositionRenderController::zoomFill() {

  if (impl_->renderer_) {

    impl_->pushViewHistory();
    impl_->renderer_->fillToViewport();

    impl_->invalidateBaseComposite();

    float panX, panY;

    impl_->renderer_->getPan(panX, panY);

    const float zoom = impl_->renderer_->getZoom();

    qDebug() << "[CompositionRenderController] zoomFill done:"

             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"

             << "hostSize=" << impl_->hostWidth_ << "x" << impl_->hostHeight_;

    markRenderDirty();

  }

}



void CompositionRenderController::zoom100() {

  if (impl_->renderer_) {
    ++impl_->smoothZoomGeneration_;
    impl_->smoothZoomActive_ = false;

    impl_->pushViewHistory();
    impl_->renderer_->setZoom(1.0f);

    // Center the canvas in the viewport at 100% zoom.

    // hostWidth_/hostHeight_ are physical pixels. lastCanvasWidth_/Height_ are
    // already the renderer's canvas size in the same physical render space, so
    // do not apply DPR again here.

    const float canvasW = impl_->lastCanvasWidth_;

    const float canvasH = impl_->lastCanvasHeight_;

    const float panX = (impl_->hostWidth_ - canvasW) * 0.5f;

    const float panY = (impl_->hostHeight_ - canvasH) * 0.5f;

    impl_->renderer_->setPan(panX, panY);

    impl_->invalidateBaseComposite();

    markRenderDirty();

  }

}

void CompositionRenderController::focusSelectedLayer() {

  if (!impl_->renderer_) {

    return;

  }



  auto comp = impl_->previewPipeline_.composition();

  if (!comp || impl_->selectedLayerId_.isNil()) {

    zoomFit();

    return;

  }



  const auto layer = comp->layerById(impl_->selectedLayerId_);

  if (!layer) {

    zoomFit();

    return;

  }



  const QRectF bounds = layer->transformedBoundingBox();

  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {

    zoomFit();

    return;

  }



  const float viewW = impl_->hostWidth_ > 0.0f ? impl_->hostWidth_ : 1.0f;

  const float viewH = impl_->hostHeight_ > 0.0f ? impl_->hostHeight_ : 1.0f;

  const float margin = 48.0f;

  const float availableW = std::max(1.0f, viewW - margin * 2.0f);

  const float availableH = std::max(1.0f, viewH - margin * 2.0f);

  const float zoomX = availableW / static_cast<float>(bounds.width());

  const float zoomY = availableH / static_cast<float>(bounds.height());

  const float zoom = std::clamp(std::min(zoomX, zoomY), 0.02f, 64.0f);

  const QPointF center = bounds.center();



  impl_->renderer_->setZoom(zoom);

  impl_->renderer_->setPan(viewW * 0.5f - static_cast<float>(center.x()) * zoom,

                           viewH * 0.5f -

                               static_cast<float>(center.y()) * zoom);

  impl_->invalidateBaseComposite();

  markRenderDirty();

}

bool CompositionRenderController::focusActiveCameraAtViewportPos(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  auto comp = impl_->previewPipeline_.composition();
  if (!comp) {
    return false;
  }
  const QPointF physPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  const WorldRay worldRay = createPickingRay(screenPhysicalPoint(physPos));
  if (QVector3D(worldRay.direction.x, worldRay.direction.y,
                worldRay.direction.z).lengthSquared() <= 1.0e-12f) {
    return false;
  }
  const auto currentFrame = currentFrameForComposition(comp);
  ArtifactCore::Units::WorldLength nearestDistance{
      std::numeric_limits<float>::max()};
  bool hit = false;
  for (const auto &layer : comp->allLayerRef()) {
    if (!isLayerEffectivelyVisible(layer) || !layer->isActiveAt(currentFrame)) {
      continue;
    }
    const auto modelLayer =
        ArtifactCore::dynamicPointerCast<Artifact3DLayer>(layer);
    ArtifactCore::Units::WorldLength distance{};
    if (modelLayer &&
        intersectModelLayerPickingRay(*modelLayer, worldRay, distance) &&
        distance.value < nearestDistance.value) {
      nearestDistance = distance;
      hit = true;
    }
  }
  if (!hit || !std::isfinite(nearestDistance.value) ||
      nearestDistance.value <= 0.0f) {
    return false;
  }
  ArtifactCameraLayer *activeCamera = nullptr;
  for (const auto &l : comp->allLayerRef()) {
    auto *cam = dynamic_cast<ArtifactCameraLayer *>(l.get());
    if (cam && isLayerEffectivelyVisible(l) && cam->isActiveAt(currentFrame) &&
        cam->isActiveCamera() &&
        (!activeCamera || cam->cameraPriority() > activeCamera->cameraPriority())) {
      activeCamera = cam;
    }
  }
  if (!activeCamera) {
    return false;
  }
  // Exact hit point is view-independent; project onto the active camera
  // forward axis for a true focus-plane distance.
  const auto hitPoint = worldRay.origin +
                        worldRay.direction * nearestDistance.value;
  const QMatrix4x4 camGlobal = activeCamera->effectiveGlobalTransform();
  const auto camPos = ArtifactCore::Coordinates::worldPoint3FromQVector3D(
      camGlobal.map(QVector3D(0.0f, 0.0f, 0.0f)));
  QVector3D forward = camGlobal.mapVector(QVector3D(0.0f, 0.0f, -1.0f));
  if (forward.lengthSquared() <= 1.0e-12f) {
    return false;
  }
  forward.normalize();
  const auto worldFocusDistance = ArtifactCore::Units::WorldLength{
      QVector3D::dotProduct(
          ArtifactCore::Coordinates::toQVector3D(hitPoint - camPos), forward)};
  const float distance = worldFocusDistance.value;
  if (!std::isfinite(distance) || distance <= 0.0f) {
    return false;
  }
  const float clamped = std::clamp(
      distance, activeCamera->nearClipPlane().value,
      activeCamera->farClipPlane().value);
  // Camera focus and clip properties use the existing pixel-number contract;
  // this preserves the current numeric mapping from world-ray distance.
  return activeCamera->setLayerPropertyValue(
      QStringLiteral("Camera Options/Focus Distance"), clamped);
}

void CompositionRenderController::zoomFitWorkArea() {
  if (!impl_->renderer_) {
    return;
  }
  const auto composition = impl_->previewPipeline_.composition();
  if (!composition || impl_->hostWidth_ <= 0 || impl_->hostHeight_ <= 0) {
    return;
  }

  const auto workArea = composition->workAreaRange().normalized();
  QRectF bounds;
  bool hasBounds = false;
  for (const auto& layer : composition->allLayer()) {
    if (!layer || !layer->isVisible()) {
      continue;
    }
    const auto layerIn = layer->inPoint().framePosition();
    const auto layerOut = layer->outPoint().framePosition();
    if (layerOut <= workArea.start() || layerIn >= workArea.end()) {
      continue;
    }
    const QRectF layerBounds = layer->transformedBoundingBox().normalized();
    if (!layerBounds.isValid() || layerBounds.width() <= 0.0 ||
        layerBounds.height() <= 0.0) {
      continue;
    }
    bounds = hasBounds ? bounds.united(layerBounds) : layerBounds;
    hasBounds = true;
  }
  if (!hasBounds) {
    return;
  }

  ++impl_->smoothZoomGeneration_;
  impl_->smoothZoomActive_ = false;
  impl_->pushViewHistory();
  const float margin = 0.05f *
      static_cast<float>(std::min(impl_->hostWidth_, impl_->hostHeight_));
  const float availableWidth = std::max(
      1.0f, static_cast<float>(impl_->hostWidth_) - 2.0f * margin);
  const float availableHeight = std::max(
      1.0f, static_cast<float>(impl_->hostHeight_) - 2.0f * margin);
  const float zoom = std::clamp(
      std::min(availableWidth / static_cast<float>(bounds.width()),
               availableHeight / static_cast<float>(bounds.height())),
      0.05f, 64.0f);
  impl_->renderer_->setZoom(zoom);
  impl_->renderer_->setPan(
      static_cast<float>(impl_->hostWidth_) * 0.5f -
          static_cast<float>(bounds.center().x()) * zoom,
      static_cast<float>(impl_->hostHeight_) * 0.5f -
          static_cast<float>(bounds.center().y()) * zoom);
  impl_->invalidateBaseComposite();
  markRenderDirty();
}

CompositionRenderController::CameraFrustumVisual

CompositionRenderController::cameraFrustumVisual() const {

  return buildCameraFrustumVisual(impl_->previewPipeline_.composition(),

                                  impl_->selectedLayerId_);

}

CompositionRenderController::ViewportCullingStats
CompositionRenderController::viewportCullingStats() const {
  return impl_ ? impl_->viewportCullingStats_
               : CompositionRenderController::ViewportCullingStats{};
}

void CompositionRenderController::clearViewportOrientation() {
  if (!impl_) {
    return;
  }
  if (impl_->viewportNavigationMode_ !=
      CompositionRenderController::ViewportNavigationMode::Off) {
    return;
  }
  // "2D" is the front orthographic view of the same spatial viewport.
  impl_->viewportOrientationNavigator_.snapTo(
      ArtifactCore::ViewOrientationHotspot::Front, true);
  impl_->viewportOrientationMatricesValid_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}



void CompositionRenderController::setViewportOrientation(

    ArtifactCore::ViewOrientationHotspot hotspot) {

  if (!impl_) {

    return;

  }

  if (impl_->maskViewNavigationLocked() &&
      hotspot != ArtifactCore::ViewOrientationHotspot::Front) {
    return;
  }

  // Fly / Walk renders a free camera, so a hotspot snap would be invisible
  // and would still move the stored orientation the mode restores on exit.
  if (impl_->viewportNavigationMode_ !=
      ViewportNavigationMode::Off) {
    return;
  }

  if (impl_->viewportOrientationNavigator_.activeHotspot() == hotspot) {

    return;

  }

  impl_->pushViewHistory();
  impl_->viewportOrientationNavigator_.snapTo(hotspot, true);

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



ArtifactCore::ViewOrientationHotspot

CompositionRenderController::viewportOrientation() const {

  if (!impl_) {

    return ArtifactCore::ViewOrientationHotspot::Front;

  }

  return impl_->viewportOrientationNavigator_.activeHotspot();

}

QQuaternion CompositionRenderController::viewportOrientationQuaternion() const {

  if (!impl_) {

    return QQuaternion();

  }

  return impl_->viewportOrientationNavigator_.currentOrientation();

}

void CompositionRenderController::setViewportOrientationQuaternion(

    const QQuaternion &orientation) {

  if (!impl_) {

    return;

  }

  if (impl_->maskViewNavigationLocked()) {
    return;
  }

  // View-cube / orbit drags are ignored while the free camera owns the view.
  if (impl_->viewportNavigationMode_ !=
      ViewportNavigationMode::Off) {
    return;
  }

  const QQuaternion normalized = orientation.normalized();
  const QQuaternion current =
      impl_->viewportOrientationNavigator_.currentOrientation();
  if (std::abs(QQuaternion::dotProduct(current, normalized)) > 0.999999f) {

    return;

  }

  // Continuous view-cube/orbit updates must not append one history entry per
  // mouse move. Discrete hotspot snaps still record history in
  // setViewportOrientation().
  impl_->viewportOrientationNavigator_.setCurrentOrientation(normalized);

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

void CompositionRenderController::setPreviewOrbitActive(bool active) {
  if (!impl_ || impl_->previewOrbitActive_ == active) {
    return;
  }
  impl_->previewOrbitActive_ = active;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isPreviewOrbitActive() const {
  return impl_ && impl_->previewOrbitActive_;
}

CompositionRenderController::ViewportNavigationMode
CompositionRenderController::viewportNavigationMode() const {
  return impl_ ? impl_->viewportNavigationMode_
               : ViewportNavigationMode::Off;
}

bool CompositionRenderController::beginViewportNavigation(
    const ViewportNavigationMode mode) {
  if (!impl_ || !impl_->renderer_ || mode == ViewportNavigationMode::Off) {
    return false;
  }
  // Mask editing owns the pointer and the camera; do not steal it.
  if (impl_->maskViewNavigationLocked()) {
    return false;
  }
  if (impl_->viewportNavigationMode_ == mode) {
    return true;
  }
  if (impl_->viewportNavigationMode_ != ViewportNavigationMode::Off) {
    // Fly <-> Walk keeps the free camera where it is. Walk starts from the
    // elevation it currently has so the switch does not jump the view.
    if (mode == ViewportNavigationMode::Walk) {
      const QVector3D forward = impl_->viewportNavigationForwardVector(
          impl_->viewportNavigationYawDeg_, impl_->viewportNavigationPitchDeg_);
      impl_->viewportNavigationWalkUp_ =
          Impl::viewportNavigationWalkUpFor(forward);
      impl_->viewportNavigationWalkHeight_ = QVector3D::dotProduct(
          impl_->viewportNavigationEye_, impl_->viewportNavigationWalkUp_);
    }
    impl_->viewportNavigationMode_ = mode;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return true;
  }

  float panX = 0.0f;
  float panY = 0.0f;
  impl_->renderer_->getPan(panX, panY);
  impl_->viewportNavigationSavedPanX_ = panX;
  impl_->viewportNavigationSavedPanY_ = panY;
  impl_->viewportNavigationSavedZoom_ = impl_->renderer_->getZoom();
  impl_->viewportNavigationSavedOrientation_ =
      impl_->viewportOrientationNavigator_.currentOrientation();
  impl_->viewportNavigationSavedViewValid_ = true;
  impl_->beginViewportNavigationState(mode);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::endViewportNavigation() {
  if (!impl_ ||
      impl_->viewportNavigationMode_ == ViewportNavigationMode::Off) {
    return;
  }
  impl_->viewportNavigationMode_ = ViewportNavigationMode::Off;
  impl_->viewportNavigationForward_ = 0.0f;
  impl_->viewportNavigationStrafe_ = 0.0f;
  impl_->viewportNavigationLift_ = 0.0f;
  if (impl_->renderer_ && impl_->viewportNavigationSavedViewValid_) {
    impl_->renderer_->setPan(impl_->viewportNavigationSavedPanX_,
                             impl_->viewportNavigationSavedPanY_);
    impl_->renderer_->setZoom(impl_->viewportNavigationSavedZoom_);
  }
  if (impl_->viewportNavigationSavedViewValid_) {
    impl_->viewportOrientationNavigator_.setCurrentOrientation(
        impl_->viewportNavigationSavedOrientation_);
  }
  impl_->viewportNavigationSavedViewValid_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::setViewportNavigationMoveInput(
    const float forward, const float strafe, const float lift) {
  if (!impl_ ||
      impl_->viewportNavigationMode_ == ViewportNavigationMode::Off) {
    return;
  }
  const float nextForward = std::clamp(forward, -1.0f, 1.0f);
  const float nextStrafe = std::clamp(strafe, -1.0f, 1.0f);
  const float nextLift = std::clamp(lift, -1.0f, 1.0f);
  if (nextForward == impl_->viewportNavigationForward_ &&
      nextStrafe == impl_->viewportNavigationStrafe_ &&
      nextLift == impl_->viewportNavigationLift_) {
    return;
  }
  impl_->viewportNavigationForward_ = nextForward;
  impl_->viewportNavigationStrafe_ = nextStrafe;
  impl_->viewportNavigationLift_ = nextLift;
  markRenderDirty();
}

void CompositionRenderController::addViewportNavigationLook(
    const float yawDegrees, const float pitchDegrees) {
  if (!impl_ ||
      impl_->viewportNavigationMode_ == ViewportNavigationMode::Off) {
    return;
  }
  float yaw = impl_->viewportNavigationYawDeg_ + yawDegrees;
  while (yaw > 180.0f) yaw -= 360.0f;
  while (yaw < -180.0f) yaw += 360.0f;
  impl_->viewportNavigationYawDeg_ = yaw;
  impl_->viewportNavigationPitchDeg_ =
      std::clamp(impl_->viewportNavigationPitchDeg_ + pitchDegrees, -89.0f,
                 89.0f);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

void CompositionRenderController::setViewportNavigationSpeedScale(
    const float scale) {
  if (!impl_ ||
      impl_->viewportNavigationMode_ == ViewportNavigationMode::Off) {
    return;
  }
  impl_->viewportNavigationSpeedScale_ = std::clamp(scale, 0.05f, 10.0f);
}

float CompositionRenderController::viewportNavigationSpeedScale() const {
  return impl_ ? impl_->viewportNavigationSpeedScale_ : 1.0f;
}

bool CompositionRenderController::placeWorkCursorAtViewportPos(

    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {

  if (!impl_ || !impl_->renderer_) {

    return false;

  }

  const auto canvasPos = impl_->renderer_->viewportToCanvas(
      {viewportPos.x * impl_->devicePixelRatio_,
       viewportPos.y * impl_->devicePixelRatio_});

  // Preserve the existing placement on the z=0 world plane explicitly.
  setWorkCursorWorldPosition(
      {canvasPos.x, canvasPos.y, 0.0f});

  return true;

}

void CompositionRenderController::setWorkCursorCanvasPosition(
    ArtifactCore::Coordinates::CompositionPoint2 canvasPos) {

  if (!impl_) {

    return;

  }

  const ArtifactCore::Coordinates::CompositionPoint2 normalized{
      std::isfinite(canvasPos.x) ? canvasPos.x : 0.0f,
      std::isfinite(canvasPos.y) ? canvasPos.y : 0.0f};
  if (impl_->workCursorVisible_ &&
      impl_->workCursorState_.canvasPosition.x == normalized.x &&
      impl_->workCursorState_.canvasPosition.y == normalized.y &&
      !impl_->workCursorState_.spatial) {

    return;

  }

  impl_->workCursorState_.canvasPosition = normalized;
  impl_->workCursorState_.spatial = false;

  impl_->workCursorVisible_ = true;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

ArtifactCore::Coordinates::CompositionPoint2
CompositionRenderController::workCursorCanvasPosition() const {
  return impl_ ? impl_->workCursorState_.canvasPosition
               : ArtifactCore::Coordinates::CompositionPoint2{};
}

void CompositionRenderController::setWorkCursorWorldPosition(
    ArtifactCore::Coordinates::WorldPoint3 position) {
  if (!impl_) {
    return;
  }
  const auto finiteOrZero = [](const float value) {
    return std::isfinite(value) ? value : 0.0f;
  };
  WorkCursorState next = impl_->workCursorState_;
  next.worldPosition = {finiteOrZero(position.x), finiteOrZero(position.y),
                        finiteOrZero(position.z)};
  next.spatial = true;
  if (impl_->workCursorVisible_ &&
      next.worldPosition.x == impl_->workCursorState_.worldPosition.x &&
      next.worldPosition.y == impl_->workCursorState_.worldPosition.y &&
      next.worldPosition.z == impl_->workCursorState_.worldPosition.z &&
      impl_->workCursorState_.spatial) {
    return;
  }
  impl_->workCursorState_ = next;
  impl_->workCursorVisible_ = true;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

WorkCursorState CompositionRenderController::workCursorState() const {
  return impl_ ? impl_->workCursorState_ : WorkCursorState{};
}

bool CompositionRenderController::moveWorkCursorToSelection() {
  if (!impl_) {
    return false;
  }
  auto *selection = ArtifactLayerSelectionManager::instance();
  if (!selection) {
    return false;
  }
  const auto selected = selection->selectedLayers();
  if (selected.isEmpty()) {
    return false;
  }
  QVector3D center{0.0f, 0.0f, 0.0f};
  int count = 0;
  bool spatial = false;
  for (const auto &layer : selected) {
    if (!layer) {
      continue;
    }
    const auto point = layer->getGlobalTransform4x4().map(QVector3D(0.0f, 0.0f, 0.0f));
    center += point;
    spatial = spatial || layer->is3D();
    ++count;
  }
  if (count <= 0) {
    return false;
  }
  const float invCount = 1.0f / static_cast<float>(count);
  if (spatial) {
    setWorkCursorWorldPosition(
        {center.x() * invCount, center.y() * invCount,
         center.z() * invCount});
  } else {
    setWorkCursorCanvasPosition({center.x() * invCount,
                                  center.y() * invCount});
  }
  return true;
}

void CompositionRenderController::moveWorkCursorToWorldOrigin() {
  setWorkCursorWorldPosition({0.0f, 0.0f, 0.0f});
}

void CompositionRenderController::setWorkCursorLabel(const QString &label) {

  if (!impl_) {

    return;

  }

  const QString normalized = label.trimmed();

  if (impl_->workCursorLabel_ == normalized) {

    return;

  }

  impl_->workCursorLabel_ = normalized;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

QString CompositionRenderController::workCursorLabel() const {

  return impl_ ? impl_->workCursorLabel_ : QString();

}

void CompositionRenderController::setWorkCursorVisible(bool visible) {

  if (!impl_ || impl_->workCursorVisible_ == visible) {

    return;

  }

  impl_->workCursorVisible_ = visible;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isWorkCursorVisible() const {

  return impl_ && impl_->workCursorVisible_;

}

void CompositionRenderController::clearWorkCursor() {

  if (!impl_ || !impl_->workCursorVisible_) {

    return;

  }

  impl_->workCursorVisible_ = false;

  impl_->workCursorLabel_.clear();

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

} // namespace Artifact
