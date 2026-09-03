module;
#include <utility>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QKeyEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QCursor>
#include <QPointer>
#include <QStandardPaths>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

module Artifact.Widgets.LayerEditorWidget;
import Graphics;
import Frame.Position;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Application;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Undo.UndoManager;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Widgets.LayerEditor.Geometry;
import Artifact.Widgets.LayerEditor.ContextMenu;
import Artifact.Widgets.LayerEditor.EventSubscriptions;
import Artifact.Widgets.LayerEditor.BackgroundCache;
import Artifact.Widgets.LayerEditor.FrameBackground;
import Artifact.Widgets.LayerEditor.FrameViewState;
import Artifact.Widgets.LayerEditor.KeyInputController;
import Artifact.Widgets.LayerEditor.LayerPresentation;
import Artifact.Widgets.LayerEditor.InteractionStateController;
import Artifact.Widgets.LayerEditor.HudCursor;
import Artifact.Widgets.LayerEditor.MaskEditSession;
import Artifact.Widgets.LayerEditor.MaskDragController;
import Artifact.Widgets.LayerEditor.MaskHoverController;
import Artifact.Widgets.LayerEditor.MaskMoveController;
import Artifact.Widgets.LayerEditor.MaskOverlay;
import Artifact.Widgets.LayerEditor.MaskPressInteractionController;
import Artifact.Widgets.LayerEditor.MaskPressController;
import Artifact.Widgets.LayerEditor.ModePresentation;
import Artifact.Widgets.LayerEditor.ModalTransformController;
import Artifact.Widgets.LayerEditor.ReleaseController;
import Artifact.Widgets.LayerEditor.RenderScheduler;
import Artifact.Widgets.LayerEditor.ShapeOverlay;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeDragController;
import Artifact.Widgets.LayerEditor.ShapeDeleteController;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Artifact.Widgets.LayerEditor.ShapeInputController;
import Artifact.Widgets.LayerEditor.ShapeMoveController;
import Artifact.Widgets.LayerEditor.ShapePressInteractionController;
import Artifact.Widgets.LayerEditor.ShapeParameterController;
import Artifact.Widgets.LayerEditor.SurfaceOverlay;
import Artifact.Widgets.LayerEditor.SurfaceInfo;
import Artifact.Widgets.LayerEditor.TransformOverlay;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Artifact.Widgets.LayerEditor.ViewportChromeRenderer;
import Artifact.Widgets.LayerEditor.ViewportChromeInteractionController;
import Artifact.Widgets.LayerEditor.ViewMoveController;
import Artifact.Widgets.LayerEditor.ViewKeyInputController;
import Artifact.Widgets.LayerEditor.ViewPressController;
import Artifact.Widgets.LayerEditor.ViewWheelController;
import Artifact.Effect.Abstract;
import Property.Abstract;

import Artifact.Render.IRenderer;
import Artifact.Render.CompositionRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Widgets.TransformGizmo;
import Memory.SharedPtr;

namespace Artifact {

 using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactLayerEditorWidget)

namespace {
Q_LOGGING_CATEGORY(layerViewPerfLog, "artifact.layerviewperf")

using LayerBackgroundMode = LayerEditorBackgroundMode;
using LayerSurfaceMode = LayerEditorSurfaceMode;

FramePosition currentLayerViewFrame()
{
  if (auto* playback = ArtifactPlaybackService::instance()) {
    return playback->currentFrame();
  }
  if (auto* project = ArtifactProjectService::instance()) {
    if (auto composition = project->currentComposition().lock()) {
      return composition->framePosition();
    }
  }
  return FramePosition(0);
}

bool isMaskEditingMode(EditMode mode)
{
  return mode == EditMode::Mask || mode == EditMode::Paint;
}

bool isShapeEditingMode(EditMode mode)
{
  return mode == EditMode::Paint || mode == EditMode::Shape;
}

} // namespace

 class ArtifactLayerEditorWidget::Impl {
 private:
 public:
  Impl();
  ~Impl();
  void initialize(QWidget* window);
  void initializeSwapChain(QWidget* window);
  void destroy();
  std::unique_ptr<ArtifactIRenderer> renderer_;
  std::unique_ptr<CompositionRenderer> compositionRenderer_;
  bool initialized_ = false;
  bool isPanning_=false;
  QPointF lastMousePos_;
  float zoomLevel_ = 1.0f;
  QPointer<QWidget> widget_;
  //bool isPanning_ = false;
  bool isPlay_ = false;
  std::atomic_bool running_{ false };
  QTimer* renderTimer_ = nullptr;
  std::mutex resizeMutex_;
  quint64 renderTickCount_ = 0;
  quint64 renderExecutedCount_ = 0;
  LayerEditorRenderScheduler renderScheduler_;
  bool renderInProgress_ = false;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  
  
  std::unique_ptr<TransformGizmo> transformGizmo_;
  LayerBackgroundMode backgroundMode_ = LayerBackgroundMode::Alpha;
  bool showGrid_ = false;
  bool showSafeMargins_ = false;
  LayerSurfaceMode surfaceMode_ = LayerSurfaceMode::Edit;
  EditMode editModeBeforeSurface_ = EditMode::View;
  EditMode editMode_ = EditMode::View;
  DisplayMode displayMode_ = DisplayMode::Color;
  DisplayMode displayModeBeforeMask_ = DisplayMode::Color;
  int hoveredChromeControl_ = -1;
  bool surfaceInfoDirty_ = true;
  LayerID surfaceInfoLayerId_{};
  LayerSurfaceMode surfaceInfoMode_ = LayerSurfaceMode::Edit;
  QString surfaceInfoTitle_;
  QString surfaceInfoBody_;
  std::vector<LayerID> impactParentLayerIds_;
  std::vector<LayerID> impactChildLayerIds_;
  std::vector<LayerID> impactMatteLayerIds_;
  std::vector<LayerID> impactDependentLayerIds_;
  QImage cachedMayaGradientSprite_;
  QSize cachedMayaGradientSize_;
  LayerID targetLayerId_{};
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  bool isDraggingMaskVertex_ = false;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  int draggingMaskHandleType_ = -1;
  LayerEditorMaskHoverController maskHoverController_;
  LayerEditorMaskEditSession maskEditSession_;
  LayerEditorMaskDragController maskDragController_;
  LayerEditorMaskMoveController maskMoveController_;
 LayerEditorMaskPressInteractionController maskPressInteractionController_;
  LayerEditorMaskPressController maskPressController_;
  bool proportionalEditingEnabled_ = false;
  float proportionalEditRadius_ = 96.0f;
  bool proportionalDragActive_ = false;
  QPointF proportionalDragOrigin_;
  std::vector<MaskVertex> proportionalMaskVerticesBefore_;
  std::vector<QPointF> proportionalShapePointsBefore_;
  std::vector<CustomPathVertex> proportionalPathVerticesBefore_;
  bool isDraggingMaskHandle_ = false;
  bool isDraggingShapeVertex_ = false;
  int draggingShapeVertexIndex_ = -1;
   LayerEditorShapeHoverController shapeHoverController_;
   std::vector<int> selectedShapeVertexIndices_;
   std::vector<QPointF> selectedShapeDragBefore_;
  LayerEditorShapeEditSession shapeEditSession_;
  LayerEditorShapeDragController shapeDragController_;
  LayerEditorShapeDeleteController shapeDeleteController_;
  LayerEditorShapeMoveController shapeMoveController_;
  LayerEditorShapeInputController shapeInputController_;
  LayerEditorKeyInputController keyInputController_;
  LayerEditorViewMoveController viewMoveController_;
  LayerEditorViewKeyInputController viewKeyInputController_;
  LayerEditorViewPressController viewPressController_;
  LayerEditorViewWheelController viewWheelController_;
  LayerEditorViewportChromeInteractionController viewportChromeInteractionController_;
  LayerEditorReleaseController releaseController_;
  LayerEditorInteractionStateController interactionStateController_;
  LayerEditorShapePressInteractionController shapePressInteractionController_;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  void setSurfaceMode(LayerSurfaceMode nextMode);
  bool toggleLayerState(int stateIndex);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  ArtifactAbstractLayerPtr targetLayer() const;
  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markMaskEditDirty();
  void commitMaskEditTransaction();
  void resetProportionalDragState();
  void beginShapeProportionalDragSnapshot(const std::vector<QPointF>& points, int vertexIndex);
  void beginPathProportionalDragSnapshot(const std::vector<CustomPathVertex>& verts, int vertexIndex);
  QString proportionalEditingStatusText() const;
  void drawMaskOverlay(const ArtifactAbstractLayerPtr& layer);

  void beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markShapeEditDirty();
  void commitShapeEditTransaction();
  void drawShapeOverlay(const ArtifactAbstractLayerPtr& layer);
  void drawTransformOverlay(const ArtifactAbstractLayerPtr& layer);
  void syncTransformGizmo(const ArtifactAbstractLayerPtr& layer);

  LayerEditorShapeParameterController shapeParameterController_;

  // Phase 5: Bezier path editing
  bool isDraggingPathVertex_ = false;
  bool isDraggingPathTangent_ = false;
  int draggingPathVertexIndex_ = -1;
  int draggingPathTangentType_ = 0; // 0=in, 1=out
   std::vector<int> selectedPathVertexIndices_;
   std::vector<CustomPathVertex> selectedPathDragBefore_;

  void drawShapeParamHandles(const ArtifactAbstractLayerPtr& layer);
  void drawTransformHUD(const ArtifactAbstractLayerPtr& layer);
  void drawSurfaceOverlay(const ArtifactAbstractLayerPtr& layer);
  void drawCompositionGuideOverlay();
  void drawViewportChrome(const ArtifactAbstractLayerPtr& layer);
  void refreshSurfaceInfo(const ArtifactAbstractLayerPtr& layer);
  bool handleViewportChromePress(const QPointF& viewportPos);
  bool updateViewportChromeHover(const QPointF& viewportPos);
  void drawCustomPathOverlay(const ArtifactAbstractLayerPtr& layer);
  void beginPathEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markPathEditDirty();
  void commitPathEditTransaction();
  
  void startRenderLoop();
  void stopRenderLoop();
  void requestRender();
  void renderOneFrame();
  void refreshBackgroundCache();

  LayerEditorModalTransformController modalTransform_;
  bool beginModalTransform(LayerEditorModalTransformMode mode,
                           const QPointF& viewportPosition);
  void commitModalTransform();
  void cancelModalTransform();
};

ArtifactLayerEditorWidget::Impl::Impl()
{
 transformGizmo_ = std::make_unique<TransformGizmo>();
 interactionStateController_.bind({
     .draggingMaskVertex = &isDraggingMaskVertex_,
     .draggingMaskHandle = &isDraggingMaskHandle_,
     .draggingMaskIndex = &draggingMaskIndex_,
     .draggingMaskPathIndex = &draggingPathIndex_,
     .draggingMaskVertexIndex = &draggingVertexIndex_,
     .draggingMaskHandleType = &draggingMaskHandleType_,
     .draggingPolygonVertex = &isDraggingShapeVertex_,
     .draggingPolygonVertexIndex = &draggingShapeVertexIndex_,
     .draggingPathVertex = &isDraggingPathVertex_,
     .draggingPathTangent = &isDraggingPathTangent_,
     .draggingPathVertexIndex = &draggingPathVertexIndex_,
     .draggingPathTangentType = &draggingPathTangentType_,
     .proportionalDragActive = &proportionalDragActive_,
     .proportionalDragOrigin = &proportionalDragOrigin_,
     .proportionalMaskBefore = &proportionalMaskVerticesBefore_,
     .proportionalPolygonBefore = &proportionalShapePointsBefore_,
     .proportionalPathBefore = &proportionalPathVerticesBefore_,
     .selectedPolygonIndices = &selectedShapeVertexIndices_,
     .selectedPolygonBefore = &selectedShapeDragBefore_,
     .selectedPathIndices = &selectedPathVertexIndices_,
     .selectedPathBefore = &selectedPathDragBefore_,
     .maskHover = &maskHoverController_,
     .shapeHover = &shapeHoverController_,
     .shapeParameter = &shapeParameterController_});
}

 ArtifactLayerEditorWidget::Impl::~Impl()
 {

 }

 void ArtifactLayerEditorWidget::Impl::initialize(QWidget* window)
 {
  widget_ = window;
  renderer_ = std::make_unique<ArtifactIRenderer>();
  renderer_->initialize(window);

  if (!renderer_ || !renderer_->isInitialized()) {
   qWarning() << "[ArtifactLayerEditorWidget] renderer initialize failed for"
              << window << "size=" << (window ? window->size() : QSize())
              << "DPR=" << (window ? window->devicePixelRatio() : 0.0);
   compositionRenderer_.reset();
   renderer_.reset();
   return;
  }

  compositionRenderer_ = std::make_unique<CompositionRenderer>(*renderer_);
  initialized_ = true;
 }

 void ArtifactLayerEditorWidget::Impl::initializeSwapChain(QWidget* window)
 {
  if (!renderer_) {
   return;
  }
  renderer_->recreateSwapChain(window);
  // Set the actual widget size so ViewportTransformer doesn't stay at the
  // default {1920, 1080} until the first resizeEvent fires.
  if (window && window->width() > 0 && window->height() > 0) {
   const QSize viewportSize = layerEditorPhysicalViewportSize(window);
   renderer_->setViewportSize(static_cast<float>(viewportSize.width()),
                              static_cast<float>(viewportSize.height()));
  }
 }

void ArtifactLayerEditorWidget::Impl::destroy()
{
  // Stop EventBus callbacks before invalidating the render receiver.  Hidden
  // layer views can receive FrameChangedEvent before their first showEvent.
  eventBusSubscriptions_.clear();
  renderScheduler_.cancel();
  stopRenderLoop();
  if (transformGizmo_) {
   transformGizmo_->setLayer(nullptr);
  }
  if (renderer_) {
   renderer_->destroy();
   renderer_.reset();
  }
  compositionRenderer_.reset();
  initialized_ = false;
  widget_.clear();
 }

void ArtifactLayerEditorWidget::Impl::setSurfaceMode(
    LayerSurfaceMode nextMode)
{
 if (!widget_ || surfaceMode_ == nextMode) return;

 if (surfaceMode_ == LayerSurfaceMode::Edit) {
  editModeBeforeSurface_ = editMode_;
 }
 surfaceMode_ = nextMode;
 const QString surfaceName = nextMode == LayerSurfaceMode::Edit
     ? QStringLiteral("Edit")
     : nextMode == LayerSurfaceMode::Inspect
         ? QStringLiteral("Inspect")
         : QStringLiteral("Impact");
 widget_->setProperty("artifactSurfaceMode", surfaceName);
 surfaceInfoDirty_ = true;

 auto* editor = static_cast<ArtifactLayerEditorWidget*>(widget_.data());
 editor->setEditMode(nextMode == LayerSurfaceMode::Edit
                         ? editModeBeforeSurface_
                         : EditMode::View);
 if (transformGizmo_ && nextMode != LayerSurfaceMode::Edit) {
  transformGizmo_->setLayer(nullptr);
 }
 requestRender();
}

bool ArtifactLayerEditorWidget::Impl::toggleLayerState(int stateIndex)
{
 auto layer = targetLayer();
 if (!layer || stateIndex < 0 || stateIndex > 2) return false;

  auto* undo = UndoManager::instance();
  bool applied = false;
  if (stateIndex == 0) {
   const bool next = !layer->isVisible();
   if (undo) {
    applied = undo->push(std::make_unique<SetLayerVisibilityCommand>(layer, next));
   } else {
    layer->setVisible(next);
    applied = layer->isVisible() == next;
   }
  } else if (stateIndex == 1) {
   const bool next = !layer->isLocked();
   if (undo) {
    applied = undo->push(std::make_unique<SetLayerLockCommand>(layer, next));
   } else {
    layer->setLocked(next);
    applied = layer->isLocked() == next;
   }
  } else {
   const bool next = !layer->isSolo();
   if (undo) {
    applied = undo->push(std::make_unique<SetLayerSoloCommand>(layer, next));
   } else {
    layer->setSolo(next);
    applied = layer->isSolo() == next;
   }
  }
  if (!applied) return false;

 publishLayerEditorReadout(
     widget_, layer, layer && layer->isActiveAt(currentLayerViewFrame()));
 surfaceInfoDirty_ = true;
 if (!layer->isVisible() || layer->isLocked()) {
  shapeParameterController_.clearHover();
  shapeHoverController_.clear();
  maskHoverController_.clear();
 }
 syncTransformGizmo(layer);
 requestRender();
 return true;
}

 void ArtifactLayerEditorWidget::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
  if (!event || !renderer_ || !widget_) {
   return;
  }
  const LayerEditorViewKeyInputState state{
      .key = event->key(),
      .altModifier = event->modifiers().testFlag(Qt::AltModifier),
      .autoRepeat = event->isAutoRepeat(),
      .hasTargetLayer = static_cast<bool>(targetLayer()),
      .viewportCenter = QPointF(widget_->width() * 0.5,
                                widget_->height() * 0.5),
      .zoomLevel = &zoomLevel_};
  const LayerEditorViewKeyInputCallbacks callbacks{
      .toggleLayerState = [this](int stateIndex) {
        toggleLayerState(stateIndex);
      },
      .setSurfaceMode = [this](LayerSurfaceMode mode) {
        setSurfaceMode(mode);
      },
      .setDisplayMode = [this](DisplayMode mode) {
        static_cast<ArtifactLayerEditorWidget*>(widget_.data())
            ->setDisplayMode(mode);
        if (auto* app = Artifact::ApplicationService::instance()) {
         if (auto* toolService = app->toolService()) {
          toolService->setDisplayMode(mode);
         }
        }
      }};
  const auto result = viewKeyInputController_.handle(
      state, callbacks, *renderer_);
  if (result.requestRender) requestRender();
  if (result.consumed) event->accept();
 }

 void ArtifactLayerEditorWidget::Impl::defaultHandleKeyReleaseEvent(QKeyEvent* event)
 {
  Q_UNUSED(event);
 }

 ArtifactAbstractLayerPtr ArtifactLayerEditorWidget::Impl::targetLayer() const
 {
  if (targetLayerId_.isNil()) {
   return {};
  }
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    return composition->layerById(targetLayerId_);
   }
  }
  return {};
 }

 void ArtifactLayerEditorWidget::Impl::beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer)
 {
  maskEditSession_.begin(layer);
 }

 void ArtifactLayerEditorWidget::Impl::markMaskEditDirty()
 {
  maskEditSession_.markDirty();
 }

void ArtifactLayerEditorWidget::Impl::commitMaskEditTransaction()
{
 maskEditSession_.commit();
}

void ArtifactLayerEditorWidget::Impl::resetProportionalDragState()
{
 proportionalDragActive_ = false;
 proportionalDragOrigin_ = {};
 proportionalMaskVerticesBefore_.clear();
 proportionalShapePointsBefore_.clear();
 proportionalPathVerticesBefore_.clear();
}

void ArtifactLayerEditorWidget::Impl::beginShapeProportionalDragSnapshot(const std::vector<QPointF>& points,
                                                                           const int vertexIndex)
{
 resetProportionalDragState();
 if (!proportionalEditingEnabled_ || vertexIndex < 0 ||
     vertexIndex >= static_cast<int>(points.size())) {
  return;
 }
 proportionalShapePointsBefore_ = points;
 proportionalDragOrigin_ = points[static_cast<size_t>(vertexIndex)];
 proportionalDragActive_ = true;
}

void ArtifactLayerEditorWidget::Impl::beginPathProportionalDragSnapshot(
    const std::vector<CustomPathVertex>& verts,
    const int vertexIndex)
{
 resetProportionalDragState();
 if (!proportionalEditingEnabled_ || vertexIndex < 0 ||
     vertexIndex >= static_cast<int>(verts.size())) {
  return;
 }
 proportionalPathVerticesBefore_ = verts;
 proportionalDragOrigin_ = verts[static_cast<size_t>(vertexIndex)].pos;
 proportionalDragActive_ = true;
}

QString ArtifactLayerEditorWidget::Impl::proportionalEditingStatusText() const
{
 const QString state = proportionalEditingEnabled_ ? QStringLiteral("prop on") : QStringLiteral("prop off");
 return QStringLiteral("%1 %2 / O / [ ]")
     .arg(state)
     .arg(static_cast<int>(std::lround(proportionalEditRadius_)));
}

void ArtifactLayerEditorWidget::Impl::beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer)
{
 shapeEditSession_.beginPolygon(layer);
}

void ArtifactLayerEditorWidget::Impl::markShapeEditDirty()
{
 shapeEditSession_.markPolygonDirty();
}

void ArtifactLayerEditorWidget::Impl::commitShapeEditTransaction()
{
 shapeEditSession_.commitPolygon();
}

void ArtifactLayerEditorWidget::Impl::drawShapeOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer) return;
 const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (shape && shape->hasCustomPath()) {
  drawCustomPathOverlay(layer);
  return;
 }
 LayerEditorShapeOverlayState state;
 const auto& hover = shapeHoverController_.state();
 state.draggingVertex = isDraggingShapeVertex_;
 state.draggingVertexIndex = draggingShapeVertexIndex_;
 state.hoveredVertexIndex = hover.polygonVertex;
 state.hoveredSegmentIndex = hover.polygonSegment;
 state.selectedVertexIndices = selectedShapeVertexIndices_;
 state.proportionalStatus = proportionalEditingStatusText();
 drawLayerEditorShapeOverlay(renderer_.get(), layer, state);
}
void ArtifactLayerEditorWidget::Impl::drawTransformOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !transformGizmo_ || !layer) {
  return;
 }
 if (displayMode_ == DisplayMode::Mask || isMaskEditingMode(editMode_)) {
  return;
 }
 std::vector<ArtifactAbstractLayerPtr> selectedTargets;
 if (auto *app = ArtifactApplicationManager::instance()) {
  if (auto *selection = app->layerSelectionManager()) {
   for (const auto &candidate : selection->selectedLayers()) {
    if (candidate) {
     selectedTargets.push_back(candidate);
    }
   }
  }
 }
 if (selectedTargets.size() > 1) {
  transformGizmo_->setTargetLayers(std::move(selectedTargets));
 } else {
  transformGizmo_->setLayer(layer);
 }
 transformGizmo_->setMode(TransformGizmo::Mode::All);
 transformGizmo_->draw(renderer_.get());
}

void ArtifactLayerEditorWidget::Impl::syncTransformGizmo(const ArtifactAbstractLayerPtr& layer)
{
 if (!transformGizmo_) {
  return;
 }
 if (surfaceMode_ != LayerSurfaceMode::Edit ||
     !layer || !layer->isVisible() || layer->isLocked() ||
     displayMode_ == DisplayMode::Mask ||
     isMaskEditingMode(editMode_)) {
  transformGizmo_->setLayer(nullptr);
  return;
 }
 std::vector<ArtifactAbstractLayerPtr> selectedTargets;
 if (auto *app = ArtifactApplicationManager::instance()) {
  if (auto *selection = app->layerSelectionManager()) {
   for (const auto &candidate : selection->selectedLayers()) {
    if (candidate) {
     selectedTargets.push_back(candidate);
    }
   }
  }
 }
 if (selectedTargets.size() > 1) {
  transformGizmo_->setTargetLayers(std::move(selectedTargets));
 } else {
  transformGizmo_->setLayer(layer);
 }
 transformGizmo_->setMode(TransformGizmo::Mode::All);
}

// ============================================================
// Phase 1: Parametric shape handles
// ============================================================

void ArtifactLayerEditorWidget::Impl::drawShapeParamHandles(const ArtifactAbstractLayerPtr& layer)
{
 drawLayerEditorShapeParameterHandles(
     renderer_.get(), layer,
     shapeParameterController_.cornerRadiusHovered(),
     shapeParameterController_.starInnerRadiusHovered());
}
// ============================================================
// Phase 4: viewport-fixed XYWH selection HUD
// ============================================================

void ArtifactLayerEditorWidget::Impl::drawTransformHUD(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !widget_ || !layer) return;
 QRectF bounds = layer->transformedBoundingBox();
 if (transformGizmo_ && transformGizmo_->isDragging()) {
  const QRectF draggingBounds = transformGizmo_->currentCanvasBoundingRect();
  if (draggingBounds.isValid() && !draggingBounds.isEmpty()) bounds = draggingBounds;
 }
 QSize restoreCanvasSize;
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock())
   restoreCanvasSize = composition->settings().compositionSize();
 }
 drawLayerEditorTransformHud(
     renderer_.get(), layer, bounds, layerEditorPhysicalViewportSize(widget_), restoreCanvasSize);
}
void ArtifactLayerEditorWidget::Impl::drawSurfaceOverlay(
    const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer || surfaceMode_ == LayerSurfaceMode::Edit) return;
 const bool impactMode = surfaceMode_ == LayerSurfaceMode::Impact;
 LayerEditorImpactLayers impactLayers;
 if (impactMode) {
  refreshSurfaceInfo(layer);
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    const auto resolve = [&](const std::vector<LayerID>& ids,
                             std::vector<ArtifactAbstractLayerPtr>& output) {
     output.reserve(ids.size());
     for (const auto& id : ids) {
      if (auto related = composition->layerById(id)) output.push_back(related);
     }
    };
    resolve(impactParentLayerIds_, impactLayers.parents);
    resolve(impactChildLayerIds_, impactLayers.children);
    resolve(impactMatteLayerIds_, impactLayers.mattes);
    resolve(impactDependentLayerIds_, impactLayers.dependents);
   }
  }
 }
 drawLayerEditorSurfaceOverlay(renderer_.get(), layer, impactMode, impactLayers);
}
void ArtifactLayerEditorWidget::Impl::drawCompositionGuideOverlay()
{
 if (!renderer_ || (!showGrid_ && !showSafeMargins_)) return;
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock())
   drawLayerEditorCompositionGuides(
       renderer_.get(), composition->settings().compositionSize(),
       showGrid_, showSafeMargins_);
 }
}
void ArtifactLayerEditorWidget::Impl::refreshSurfaceInfo(
    const ArtifactAbstractLayerPtr& layer)
{
 const LayerID layerId = layer ? layer->id() : LayerID{};
 if (!surfaceInfoDirty_ && surfaceInfoLayerId_ == layerId &&
     surfaceInfoMode_ == surfaceMode_) return;
 surfaceInfoDirty_ = false;
 surfaceInfoLayerId_ = layerId;
 surfaceInfoMode_ = surfaceMode_;
 ArtifactCompositionPtr composition;
 if (auto* service = ArtifactProjectService::instance())
  composition = service->currentComposition().lock();
 auto info = buildLayerEditorSurfaceInfo(
     layer, composition, currentLayerViewFrame(), displayMode_,
     surfaceMode_ == LayerSurfaceMode::Inspect);
 surfaceInfoTitle_ = std::move(info.title);
 surfaceInfoBody_ = std::move(info.body);
 impactParentLayerIds_ = std::move(info.parentLayerIds);
 impactChildLayerIds_ = std::move(info.childLayerIds);
 impactMatteLayerIds_ = std::move(info.matteLayerIds);
 impactDependentLayerIds_ = std::move(info.dependentLayerIds);
}
void ArtifactLayerEditorWidget::Impl::drawViewportChrome(
    const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !widget_) return;
 if (surfaceMode_ != LayerSurfaceMode::Edit) refreshSurfaceInfo(layer);
 LayerEditorViewportChromeState state;
 state.viewportSize = layerEditorPhysicalViewportSize(widget_);
 state.surfaceMode = surfaceMode_;
 state.editMode = editMode_;
 state.displayMode = displayMode_;
 state.hoveredControl = hoveredChromeControl_;
 state.surfaceInfoTitle = surfaceInfoTitle_;
 state.surfaceInfoBody = surfaceInfoBody_;
 state.layerName = layerEditorLayerNameLabel(layer);
 state.layerType = layerEditorLayerTypeLabel(layer);
 state.layerActive = layer && layer->isActiveAt(currentLayerViewFrame());
 state.viewToolEnabled = layerEditorEditModeAvailable(layer, EditMode::View);
 state.transformToolEnabled = layerEditorEditModeAvailable(layer, EditMode::Transform);
 state.shapeToolEnabled = layerEditorEditModeAvailable(layer, EditMode::Shape);
 state.maskToolEnabled = layerEditorEditModeAvailable(layer, EditMode::Mask);
 if (auto* service = ArtifactProjectService::instance()) {
  if (auto composition = service->currentComposition().lock())
   state.restoreCanvasSize = composition->settings().compositionSize();
 }
 drawLayerEditorViewportChrome(renderer_.get(), layer, state);
}
bool ArtifactLayerEditorWidget::Impl::handleViewportChromePress(
    const QPointF& viewportPos)
{
 if (!renderer_ || !widget_) return false;
 const qreal dpr = widget_->devicePixelRatioF();
 const LayerEditorViewportChromeInteractionState state{
     .viewportPosition = viewportPos,
     .physicalViewportSize = layerEditorPhysicalViewportSize(widget_),
     .physicalViewportCenter = QPointF(
         widget_->width() * 0.5 * dpr, widget_->height() * 0.5 * dpr),
     .devicePixelRatio = dpr,
     .surfaceMode = surfaceMode_,
     .editMode = editMode_,
     .hasLayerIdentity = !targetLayerId_.isNil(),
     .layer = targetLayer(),
     .hoveredControl = &hoveredChromeControl_,
     .zoomLevel = &zoomLevel_};
 const LayerEditorViewportChromeCallbacks callbacks{
     .setSurfaceMode = [this](LayerSurfaceMode mode) { setSurfaceMode(mode); },
     .setEditMode = [this](EditMode mode) {
       static_cast<ArtifactLayerEditorWidget*>(widget_.data())->setEditMode(mode);
     },
     .setDisplayMode = [this](DisplayMode mode) {
       static_cast<ArtifactLayerEditorWidget*>(widget_.data())->setDisplayMode(mode);
       if (auto* app = Artifact::ApplicationService::instance())
        if (auto* toolService = app->toolService()) toolService->setDisplayMode(mode);
     },
     .toggleLayerState = [this](int stateIndex) {
       return toggleLayerState(stateIndex);
     }};
 const auto result = viewportChromeInteractionController_.press(
     state, callbacks, *renderer_);
 if (result.requestRender) requestRender();
 return result.consumed;
}
bool ArtifactLayerEditorWidget::Impl::updateViewportChromeHover(
    const QPointF& viewportPos)
{
 if (!widget_) return false;
 const qreal dpr = widget_->devicePixelRatioF();
 const LayerEditorViewportChromeInteractionState state{
     .viewportPosition = viewportPos,
     .physicalViewportSize = layerEditorPhysicalViewportSize(widget_),
     .physicalViewportCenter = {},
     .devicePixelRatio = dpr,
     .surfaceMode = surfaceMode_,
     .editMode = editMode_,
     .hasLayerIdentity = !targetLayerId_.isNil(),
     .layer = targetLayer(),
     .hoveredControl = &hoveredChromeControl_,
     .zoomLevel = &zoomLevel_};
 const auto result = viewportChromeInteractionController_.hover(state);
 if (result.updateToolTip) widget_->setToolTip(result.toolTip);
 if (result.requestRender) requestRender();
 switch (result.cursor) {
  case LayerEditorChromeCursor::Pointing:
   widget_->setCursor(Qt::PointingHandCursor); break;
  case LayerEditorChromeCursor::Arrow:
   widget_->setCursor(Qt::ArrowCursor); break;
  case LayerEditorChromeCursor::Cross:
   widget_->setCursor(Qt::CrossCursor); break;
  case LayerEditorChromeCursor::Unset:
   widget_->unsetCursor(); break;
  case LayerEditorChromeCursor::Unchanged:
   break;
 }
 return result.overChrome;
}

// ============================================================
// Phase 5: Bezier path overlay + transactions
// ============================================================

void ArtifactLayerEditorWidget::Impl::beginPathEditTransaction(const ArtifactAbstractLayerPtr& layer)
{
 shapeEditSession_.beginPath(layer);
}

void ArtifactLayerEditorWidget::Impl::markPathEditDirty()
{
 shapeEditSession_.markPathDirty();
}

void ArtifactLayerEditorWidget::Impl::commitPathEditTransaction()
{
 shapeEditSession_.commitPath();
}

void ArtifactLayerEditorWidget::Impl::drawCustomPathOverlay(const ArtifactAbstractLayerPtr& layer)
{
 const auto& hover = shapeHoverController_.state();
 LayerEditorShapeOverlayState state;
 state.draggingPathVertex = isDraggingPathVertex_;
 state.draggingPathTangent = isDraggingPathTangent_;
 state.hoveredPathVertexIndex = hover.pathVertex;
 state.hoveredPathTangentIndex = hover.pathTangentVertex;
 state.hoveredPathTangentType = hover.pathTangentType;
 state.selectedPathVertexIndices = selectedPathVertexIndices_;
 state.proportionalStatus = proportionalEditingStatusText();
 drawLayerEditorCustomPathOverlay(renderer_.get(), layer, state);
}
void ArtifactLayerEditorWidget::Impl::drawMaskOverlay(
    const ArtifactAbstractLayerPtr& layer)
{
 const auto& hover = maskHoverController_.state();
 LayerEditorMaskOverlayState state;
 state.draggingVertex = isDraggingMaskVertex_;
 state.draggingHandle = isDraggingMaskHandle_;
 state.draggingMask = draggingMaskIndex_;
 state.draggingPath = draggingPathIndex_;
 state.draggingVertexIndex = draggingVertexIndex_;
 state.draggingHandleType = draggingMaskHandleType_;
 state.hoveredMask = hover.maskIndex;
 state.hoveredPath = hover.pathIndex;
 state.hoveredVertex = hover.vertexIndex;
 state.hoveredHandleType = static_cast<int>(hover.handleType);
 drawLayerEditorMaskOverlay(renderer_.get(), layer, state);
}

void ArtifactLayerEditorWidget::Impl::startRenderLoop()
{
 if (running_)
  return;
 running_ = true;
 if (renderTimer_ && !renderTimer_->isActive()) {
  renderTimer_->start();
 }
 requestRender();
}

 void ArtifactLayerEditorWidget::Impl::stopRenderLoop()
 {
  running_ = false;        // ループを抜ける
  if (renderTimer_) {
   renderTimer_->stop();
  }

 if (renderer_) {
  renderer_->flushAndWait();
 }
}

void ArtifactLayerEditorWidget::Impl::requestRender()
{
 if (!widget_) {
  return;
 }
 // Property sliders and effect controls can emit several mutations inside one
 // display interval. Keep only the newest state and render at most once per
 // interval instead of blocking the UI thread for every intermediate value.
 renderScheduler_.request(widget_, [this]() {
  if (!initialized_ || !renderer_ || !widget_ || !widget_->isVisible() ||
      widget_->width() <= 0 || widget_->height() <= 0) {
   return;
  }
  if (renderInProgress_) {
   requestRender();
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  if (renderInProgress_) {
   requestRender();
   return;
  }
  renderInProgress_ = true;
  QElapsedTimer frameTimer;
  frameTimer.start();
  renderOneFrame();
  renderInProgress_ = false;
  ++renderExecutedCount_;
  const qint64 elapsedMs = frameTimer.elapsed();
  if (elapsedMs >= 8 || (renderExecutedCount_ % 120ull) == 1ull) {
   qCDebug(layerViewPerfLog) << "[LayerView][Frame]"
                             << "ms=" << elapsedMs
                             << "executed=" << renderExecutedCount_
                             << "targetLayerNil=" << targetLayerId_.isNil()
                             << "visible=" << widget_->isVisible()
                             << "size=" << widget_->size();
  }
 });
}

void ArtifactLayerEditorWidget::Impl::renderOneFrame()
{
 if (!initialized_ || !renderer_)
  return;
 renderer_->clear();
 const QSize viewportSize = layerEditorPhysicalViewportSize(widget_);
 const auto frameViewState = beginLayerEditorFrameView(*renderer_, viewportSize);
 if (backgroundMode_ == LayerBackgroundMode::MayaGradient) {
  refreshBackgroundCache();
 }
 drawLayerEditorFrameBackground(*renderer_, {
     .viewportSize = viewportSize,
     .mode = backgroundMode_,
     .mayaGradientSprite = &cachedMayaGradientSprite_,
     .clearColor = clearColor_});
 if (compositionRenderer_) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    const auto compSize = composition->settings().compositionSize();
    compositionRenderer_->SetCompositionSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
   }
  }
  compositionRenderer_->ApplyCompositionSpace();
 }
 restoreLayerEditorFrameView(*renderer_, frameViewState);
 ArtifactAbstractLayerPtr displayedLayer;
  if (!targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     // コンポジションサイズを設定
     const auto compSize = composition->settings().compositionSize();
     if (compSize.width() > 0 && compSize.height() > 0) {
      renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
     }

     if (auto layer = composition->layerById(targetLayerId_)) {
      displayedLayer = layer;
      const FramePosition currentFrame = currentLayerViewFrame();
      layer->goToFrame(currentFrame.framePosition());
      const auto source = layer->sourceSize();
      if (source.width > 0 && source.height > 0) {
       // レイヤーサイズも設定（コンポジションサイズを上書きしないためコメントアウト）
       // renderer_->setCanvasSize(static_cast<float>(source.width), static_cast<float>(source.height));
      }
      
      const bool isVisible = layer->isVisible();
      const bool isActive = layer->isActiveAt(currentFrame);
      if (!isVisible || !isActive || layer->opacity() <= 0.0f) {
      } else {
       layer->draw(renderer_.get());
       if (surfaceMode_ == LayerSurfaceMode::Inspect &&
           displayMode_ == DisplayMode::Mask) {
        drawMaskOverlay(layer);
        drawSurfaceOverlay(layer);
       } else if (surfaceMode_ != LayerSurfaceMode::Edit) {
        drawSurfaceOverlay(layer);
       } else if (!layer->isLocked()) {
        if (displayMode_ == DisplayMode::Mask || editMode_ == EditMode::Mask) {
         drawMaskOverlay(layer);
        } else if (isShapeEditingMode(editMode_)) {
         drawShapeOverlay(layer);
        } else {
         drawTransformOverlay(layer);
         drawShapeParamHandles(layer);
         drawTransformHUD(layer);
        }
       }
      }
     }
    }
   }
  }
 drawCompositionGuideOverlay();
 if (!targetLayerId_.isNil()) {
  if (auto layer = targetLayer()) {
   syncTransformGizmo(layer);
  } else if (transformGizmo_) {
   transformGizmo_->setLayer(nullptr);
  }
 }
 drawViewportChrome(displayedLayer);
 renderer_->flush();
 renderer_->present();
}

void ArtifactLayerEditorWidget::Impl::refreshBackgroundCache()
{
 if (backgroundMode_ != LayerBackgroundMode::MayaGradient) {
  cachedMayaGradientSprite_ = QImage();
  cachedMayaGradientSize_ = QSize();
  return;
 }
 const QSize viewportSize = layerEditorPhysicalViewportSize(widget_);
 if (viewportSize.isEmpty() || cachedMayaGradientSize_ == viewportSize) {
  return;
 }
 cachedMayaGradientSize_ = viewportSize;
 cachedMayaGradientSprite_ = makeLayerEditorMayaGradientSprite(
     viewportSize, clearColor_);
}

 static LayerID currentSelectedLayerId()
 {
  if (auto *app = ArtifactApplicationManager::instance()) {
   if (auto *selectionManager = app->layerSelectionManager()) {
    if (auto current = selectionManager->currentLayer()) {
     return current->id();
    }
   }
  }
  return LayerID();
 }

void ArtifactLayerEditorWidget::Impl::recreateSwapChain(QWidget* window)
 {
  if (!initialized_ || !renderer_) {
   return;
  }
  if (!window || window->width() <= 0 || window->height() <= 0) {
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  renderer_->recreateSwapChain(window);
  const QSize viewportSize = layerEditorPhysicalViewportSize(window);
  renderer_->setViewportSize(static_cast<float>(viewportSize.width()), static_cast<float>(viewportSize.height()));
 }

ArtifactLayerEditorWidget::ArtifactLayerEditorWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  // requestRender() uses this object as the queued-call receiver.  Assign it at
  // construction time rather than waiting for renderer initialization.
  impl_->widget_ = this;
  setMinimumSize(1, 1);

  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  setWindowTitle(QStringLiteral("Layer Solo View"));
  setProperty("artifactSurfaceMode", QStringLiteral("Edit"));
  publishLayerEditorModeReadout(this, impl_->editMode_, impl_->displayMode_);
  publishLayerEditorReadout(this, ArtifactAbstractLayerPtr{}, false);

  impl_->renderTimer_ = new QTimer(this);
  impl_->renderTimer_->setInterval(16);
  QObject::connect(impl_->renderTimer_, &QTimer::timeout, this, [this]() {
   ++impl_->renderTickCount_;
   if ((impl_->renderTickCount_ % 120ull) == 1ull) {
    qCDebug(layerViewPerfLog) << "[LayerView][Timer]"
                              << "ticks=" << impl_->renderTickCount_
                              << "executed=" << impl_->renderExecutedCount_
                              << "visible=" << isVisible()
                              << "hidden=" << isHidden()
                              << "windowVisible=" << (window() ? window()->isVisible() : false)
                              << "size=" << size()
                              << "running=" << impl_->running_.load(std::memory_order_acquire);
   }
  if (!impl_ || !impl_->initialized_ || !impl_->renderer_ || !impl_->running_.load(std::memory_order_acquire)) {
   return;
  }
  if (!isVisible() || width() <= 0 || height() <= 0) {
   return;
  }
  if (impl_->renderInProgress_) {
   return;
  }
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  if (impl_->renderInProgress_) {
   return;
  }
  impl_->renderInProgress_ = true;
  QElapsedTimer frameTimer;
  frameTimer.start();
  impl_->renderOneFrame();
  impl_->renderInProgress_ = false;
  ++impl_->renderExecutedCount_;
  const qint64 elapsedMs = frameTimer.elapsed();
  if (elapsedMs >= 8 || (impl_->renderExecutedCount_ % 120ull) == 1ull) {
   qCDebug(layerViewPerfLog) << "[LayerView][Frame]"
                             << "ms=" << elapsedMs
                             << "executed=" << impl_->renderExecutedCount_
                             << "targetLayerNil=" << impl_->targetLayerId_.isNil()
                             << "visible=" << isVisible()
                             << "size=" << size();
  }
 });

  LayerEditorEventCallbacks eventCallbacks;
  eventCallbacks.layerSelectionChanged =
          [this](const LayerSelectionChangedEvent& event) {
            setTargetLayer(LayerID(event.layerId));
          };
  eventCallbacks.layerChanged =
          [this](const LayerChangedEvent& event) {
            if (impl_->surfaceMode_ == LayerSurfaceMode::Impact ||
                impl_->targetLayerId_.toString() == event.layerId) {
              impl_->surfaceInfoDirty_ = true;
            }
            if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                impl_->targetLayerId_.toString() == event.layerId) {
              clearTargetLayer();
              return;
            }
            if (impl_->targetLayerId_.isNil()) {
              return;
            }
            auto targetLayer = impl_->targetLayer();
            if (!targetLayer) {
              return;
            }
            if (impl_->targetLayerId_.toString() == event.layerId) {
              publishLayerEditorReadout(
                  this, targetLayer,
                  targetLayer->isActiveAt(currentLayerViewFrame()));
            }
            if (impl_->surfaceMode_ == LayerSurfaceMode::Impact ||
                impl_->targetLayerId_.toString() == event.layerId ||
                targetLayer->parentLayerId().toString() == event.layerId) {
              impl_->requestRender();
            }
          };
  eventCallbacks.frameChanged =
          [this](const FrameChangedEvent& event) {
            if (impl_->targetLayerId_.isNil()) {
              return;
            }
            if (impl_->surfaceMode_ == LayerSurfaceMode::Inspect) {
              impl_->surfaceInfoDirty_ = true;
            }
            if (impl_->isPlay_ && impl_->running_.load(std::memory_order_acquire)) {
              return;
            }
            if (auto* service = ArtifactProjectService::instance()) {
              if (auto composition = service->currentComposition().lock()) {
                if (composition->id().toString() == event.compositionId) {
                  impl_->requestRender();
                }
              }
            }
          };
  eventCallbacks.playbackStateChanged =
          [this](const PlaybackStateChangedEvent& event) {
            impl_->isPlay_ = (event.state == ArtifactCore::PlaybackState::Playing);
            if (impl_->isPlay_) {
              if (isVisible()) {
                impl_->startRenderLoop();
              }
              return;
            }
            impl_->stopRenderLoop();
            impl_->requestRender();
          };
  eventCallbacks.projectChanged =
          [this](const ProjectChangedEvent&) {
            impl_->surfaceInfoDirty_ = true;
            const auto targetId = impl_->targetLayerId_;
            if (targetId.isNil()) {
              return;
            }
            if (auto* currentService = ArtifactProjectService::instance()) {
              if (auto composition = currentService->currentComposition().lock()) {
                if (composition->containsLayerById(targetId)) {
                  setTargetLayer(targetId);
                  return;
                }
              }
            }
            clearTargetLayer();
          };
  eventCallbacks.currentCompositionChanged =
          [this](const CurrentCompositionChangedEvent&) {
            impl_->surfaceInfoDirty_ = true;
            const LayerID selectedId = currentSelectedLayerId();
            if (!selectedId.isNil()) {
              setTargetLayer(selectedId);
              return;
            }
            if (!impl_->targetLayerId_.isNil()) {
              if (auto* service = ArtifactProjectService::instance()) {
                if (auto composition = service->currentComposition().lock()) {
                  if (composition->containsLayerById(impl_->targetLayerId_)) {
                    setTargetLayer(impl_->targetLayerId_);
                    return;
                  }
                }
              }
            }
            clearTargetLayer();
          };
  impl_->eventBusSubscriptions_ =
      subscribeLayerEditorEvents(impl_->eventBus_, std::move(eventCallbacks));
 }

 void ArtifactLayerEditorWidget::clearTargetLayer()
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  if (impl_->shapeEditSession_.polygonPending()) {
   impl_->commitShapeEditTransaction();
  }
  if (impl_->shapeEditSession_.pathPending()) {
   impl_->commitPathEditTransaction();
  }
 impl_->targetLayerId_ = LayerID();
 publishLayerEditorReadout(this, ArtifactAbstractLayerPtr{}, false);
 impl_->surfaceInfoDirty_ = true;
 impl_->interactionStateController_.resetForClearTarget();
 if (impl_->renderer_) {
  impl_->renderer_->clear();
 impl_->renderer_->flush();
 impl_->renderer_->present();
 }
 if (impl_->transformGizmo_) {
  impl_->transformGizmo_->setLayer(nullptr);
 }
}

bool ArtifactLayerEditorWidget::Impl::beginModalTransform(
    LayerEditorModalTransformMode mode, const QPointF& viewportPosition)
{
 auto layer = targetLayer();
 if (!layer || !layer->isVisible() || layer->isLocked()) return false;
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || !modalTransform_.begin(mode, layer, viewportPosition,
                                      selectedShapeVertexIndices_,
                                      selectedPathVertexIndices_)) return false;
 if (modalTransform_.editsPath()) beginPathEditTransaction(layer);
 else beginShapeEditTransaction(layer);
 return true;
}

void ArtifactLayerEditorWidget::Impl::commitModalTransform()
{
 if (!modalTransform_.active()) return;
 if (modalTransform_.editsPath()) commitPathEditTransaction();
 else commitShapeEditTransaction();
 modalTransform_.finish();
}

void ArtifactLayerEditorWidget::Impl::cancelModalTransform()
{
 shapeEditSession_.cancelPath();
 shapeEditSession_.cancelPolygon();
 modalTransform_.finish();
}

 ArtifactLayerEditorWidget::~ArtifactLayerEditorWidget()
 {
  impl_->destroy();
  delete impl_;
  impl_ = nullptr;
 }

 void ArtifactLayerEditorWidget::keyPressEvent(QKeyEvent* event)
 {
  if (!event) return;
  LayerEditorKeyInputState state{
      .key = event->key(),
      .controlModifier = event->modifiers().testFlag(Qt::ControlModifier),
      .maskEditing = impl_->editMode_ == EditMode::Mask,
      .shapeEditing = impl_->displayMode_ != DisplayMode::Mask &&
                      isShapeEditingMode(impl_->editMode_),
      .rendererAvailable = impl_->renderer_ != nullptr,
      .proportionalEditingEnabled = &impl_->proportionalEditingEnabled_,
      .proportionalEditRadius = &impl_->proportionalEditRadius_,
      .layer = impl_->targetLayer(),
      .selectedPolygonIndices = &impl_->selectedShapeVertexIndices_,
      .selectedPathIndices = &impl_->selectedPathVertexIndices_};
  LayerEditorKeyInputCallbacks callbacks;
  callbacks.beginModalTransform = [this](LayerEditorModalTransformMode mode) {
    return impl_->beginModalTransform(mode, mapFromGlobal(QCursor::pos()));
  };
  callbacks.commitModalTransform = [this]() { impl_->commitModalTransform(); };
  callbacks.cancelModalTransform = [this]() { impl_->cancelModalTransform(); };
  callbacks.deleteMaskVertex = [this]() {
    const auto layer = impl_->targetLayer();
    const bool editable = layer && layer->isVisible() && !layer->isLocked();
    if (editable) impl_->beginMaskEditTransaction(layer);
    const bool handled = editable &&
        impl_->maskHoverController_.deleteHoveredVertex(layer);
    if (handled) impl_->markMaskEditDirty();
    impl_->commitMaskEditTransaction();
    return handled;
  };
  callbacks.deleteShapeGeometry = [this]() {
    const auto layer = impl_->targetLayer();
    const bool editable = layer && layer->isVisible() && !layer->isLocked();
    const auto deleteResult = editable
        ? impl_->shapeDeleteController_.handle(
              layer, impl_->shapeHoverController_, impl_->shapeEditSession_)
        : LayerEditorShapeDeleteResult{};
    if (deleteResult.startPolygonDrag) {
     impl_->isDraggingShapeVertex_ = true;
     impl_->draggingShapeVertexIndex_ = deleteResult.polygonVertexIndex;
    }
    impl_->commitShapeEditTransaction();
    return deleteResult.handled;
  };
  const auto result = impl_->keyInputController_.handle(
      state, std::move(callbacks), impl_->modalTransform_,
      impl_->shapeInputController_, impl_->shapeEditSession_);
  if (result.consumed) {
   switch (result.cursor) {
    case LayerEditorKeyCursor::Unset: unsetCursor(); break;
    case LayerEditorKeyCursor::Move: setCursor(Qt::SizeAllCursor); break;
    case LayerEditorKeyCursor::Cross: setCursor(Qt::CrossCursor); break;
    case LayerEditorKeyCursor::HorizontalResize: setCursor(Qt::SizeHorCursor); break;
    case LayerEditorKeyCursor::Unchanged: break;
   }
   if (result.requestRender) impl_->requestRender();
   event->accept();
   return;
  }
  impl_->defaultHandleKeyPressEvent(event);
 }

 void ArtifactLayerEditorWidget::keyReleaseEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyReleaseEvent(event);
 }

 void ArtifactLayerEditorWidget::mousePressEvent(QMouseEvent* event)
 {
  const bool transformViewEnabled =
      impl_->surfaceMode_ == LayerSurfaceMode::Edit &&
      impl_->displayMode_ != DisplayMode::Mask &&
      impl_->editMode_ != EditMode::Mask &&
      !isShapeEditingMode(impl_->editMode_);
  const LayerEditorViewPressState viewState{
      .button = static_cast<int>(event->button()),
      .altModifier = event->modifiers().testFlag(Qt::AltModifier),
      .viewportPosition = event->position(),
      .transformViewEnabled = transformViewEnabled,
      .layer = impl_->targetLayer(),
      .panning = &impl_->isPanning_,
      .lastMousePosition = &impl_->lastMousePos_};
  const LayerEditorViewPressCallbacks viewCallbacks{
      .pressViewportChrome = [this](const QPointF& position) {
        return impl_->handleViewportChromePress(position);
      },
      .clearViewportChromeHover = [this]() {
        impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
      }};
  const auto viewResult = impl_->viewPressController_.handle(
      viewState, viewCallbacks, impl_->renderer_.get(),
      impl_->shapeParameterController_, impl_->transformGizmo_.get());
  if (viewResult.consumed) {
   switch (viewResult.cursor) {
    case LayerEditorViewPressCursor::Pan:
     setCursor(layerEditorHudCursor(QStringLiteral("hud_cursor_pan.svg"),
                         Qt::ClosedHandCursor));
     break;
    case LayerEditorViewPressCursor::ParameterHorizontal:
     setCursor(layerEditorHudCursor(QStringLiteral("hud_cursor_scale_horizontal.svg"),
                         Qt::SizeHorCursor));
     break;
    case LayerEditorViewPressCursor::ParameterVertical:
     setCursor(layerEditorHudCursor(QStringLiteral("hud_cursor_scale_vertical.svg"),
                         Qt::SizeVerCursor));
     break;
    case LayerEditorViewPressCursor::Gizmo:
     setCursor(layerEditorHudCursorForTransformHandle(viewResult.gizmoHandle, true));
     break;
    case LayerEditorViewPressCursor::Unchanged:
     break;
   }
   if (viewResult.requestRender) impl_->requestRender();
    event->accept();
    return;
  }

 if (impl_->displayMode_ != DisplayMode::Mask &&
     isShapeEditingMode(impl_->editMode_) &&
     event->button() == Qt::LeftButton && impl_->renderer_) {
  const auto layer = impl_->targetLayer();
  const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(event->position().x()),
       static_cast<float>(event->position().y())});
  const auto state = impl_->interactionStateController_.shapePressState(
      impl_->proportionalEditingEnabled_);
  const auto result = impl_->shapePressInteractionController_.handle(
      layer, QPointF(canvasPos.x, canvasPos.y), impl_->renderer_->getZoom(),
      event->modifiers().testFlag(Qt::ShiftModifier) ||
          event->modifiers().testFlag(Qt::ControlModifier),
      state, impl_->shapeHoverController_, impl_->shapeEditSession_);
  if (result.consumed) {
   if (result.useMoveCursor) {
    setCursor(layerEditorHudCursor(QStringLiteral("hud_cursor_move.svg"),
                        Qt::ClosedHandCursor));
   }
   if (result.requestRender) {
    impl_->requestRender();
   }
    event->accept();
    return;
   }
 }

  if (impl_->editMode_ == EditMode::Mask &&
      event->button() == Qt::LeftButton && impl_->renderer_) {
   const auto layer = impl_->targetLayer();
   const Detail::float2 canvas = impl_->renderer_->viewportToCanvas(
       {static_cast<float>(event->position().x()),
        static_cast<float>(event->position().y())});
   const auto state = impl_->interactionStateController_.maskPressState(
       impl_->proportionalEditingEnabled_);
   const auto result = impl_->maskPressInteractionController_.handle(
       layer, QPointF(canvas.x, canvas.y), impl_->renderer_->getZoom(), state,
       impl_->maskHoverController_, impl_->maskEditSession_);
   if (result.consumed) {
    if (result.useMoveCursor) {
     setCursor(layerEditorHudCursor(QStringLiteral("hud_cursor_move.svg"),
                         Qt::ClosedHandCursor));
    }
    if (result.requestRender) {
     impl_->requestRender();
    }
    event->accept();
    return;
   }
  }


  QWidget::mousePressEvent(event);
 }

void ArtifactLayerEditorWidget::mouseReleaseEvent(QMouseEvent* event)
 {
 if (impl_->isPanning_ &&
     (event->button() == Qt::MiddleButton ||
      event->button() == Qt::RightButton)) {
   impl_->isPanning_ = false;
   if (!impl_->updateViewportChromeHover(event->position())) {
    const auto layer = impl_->targetLayer();
    if (isMaskEditingMode(impl_->editMode_) && layer &&
        layer->isVisible() && !layer->isLocked()) {
     setCursor(Qt::CrossCursor);
    } else {
     unsetCursor();
    }
   }
   event->accept();
   return;
  }

  auto state = impl_->interactionStateController_.releaseState(
      impl_->modalTransform_.active(),
      impl_->editMode_ == EditMode::Mask,
      isShapeEditingMode(impl_->editMode_),
      impl_->shapeParameterController_.active(),
      impl_->transformGizmo_ && impl_->transformGizmo_->isDragging());
  state.button = static_cast<int>(event->button());
  state.pathEditPending = impl_->shapeEditSession_.pathPending();
  state.polygonEditPending = impl_->shapeEditSession_.polygonPending();
  LayerEditorReleaseCallbacks callbacks{
      .commitModal = [this]() { impl_->commitModalTransform(); },
      .cancelModal = [this]() { impl_->cancelModalTransform(); },
      .resetProportionalState = [this]() { impl_->resetProportionalDragState(); },
      .commitMaskEdit = [this]() { impl_->commitMaskEditTransaction(); },
      .commitPathEdit = [this]() { impl_->commitPathEditTransaction(); },
      .commitPolygonEdit = [this]() { impl_->commitShapeEditTransaction(); },
      .commitParameterEdit = [this]() { impl_->shapeParameterController_.commit(); },
      .releaseGizmo = [this]() { impl_->transformGizmo_->handleMouseRelease(); }};
  const auto result = impl_->releaseController_.handle(state, callbacks);
  if (result.consumed) {
   if (result.requestRender) impl_->requestRender();
   if (result.unsetCursor) unsetCursor();
   event->accept();
   return;
  }
  QWidget::mouseReleaseEvent(event);
 }

 void ArtifactLayerEditorWidget::mouseDoubleClickEvent(QMouseEvent* event)
 {
 if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));
    if (impl_->maskPressController_.closeOpenPathOnDoubleClick(
            layer, canvasPoint, impl_->renderer_->getZoom(),
            impl_->maskEditSession_)) {
      impl_->requestRender();
      event->accept();
      return;
    }
   }
  }
  QWidget::mouseDoubleClickEvent(event);
 }

 void ArtifactLayerEditorWidget::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->isPanning_) {
   const QPointF currentPos = event->position();
   const QPointF delta = currentPos - impl_->lastMousePos_;
   impl_->lastMousePos_ = currentPos;
   panBy(delta);
   event->accept();
   return;
  }

  if (event->buttons() == Qt::NoButton &&
      impl_->updateViewportChromeHover(event->position())) {
   event->accept();
   return;
  }

  if (impl_->renderer_) {
   const bool transformViewEnabled =
       impl_->surfaceMode_ == LayerSurfaceMode::Edit &&
       impl_->displayMode_ != DisplayMode::Mask &&
       impl_->editMode_ != EditMode::Mask &&
       !isShapeEditingMode(impl_->editMode_);
   const LayerEditorViewMoveState state{
       .viewportPosition = event->position(),
       .precision = event->modifiers().testFlag(Qt::ShiftModifier),
       .snap = event->modifiers().testFlag(Qt::ControlModifier),
       .transformViewEnabled = transformViewEnabled,
       .layer = impl_->targetLayer()};
   const auto result = impl_->viewMoveController_.handle(
       state, *impl_->renderer_, impl_->modalTransform_,
       impl_->shapeEditSession_, impl_->shapeParameterController_,
       impl_->transformGizmo_.get());
   if (result.cursor == LayerEditorViewMoveCursor::Unset) {
    unsetCursor();
   } else if (result.cursor == LayerEditorViewMoveCursor::Gizmo) {
    setCursor(layerEditorHudCursorForTransformHandle(
        result.gizmoHandle, result.gizmoDragging));
   }
   if (result.requestRender) impl_->requestRender();
   if (result.consumed) {
    event->accept();
    return;
   }
  }

  if (impl_->editMode_ == EditMode::Mask && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
       {static_cast<float>(event->position().x()),
        static_cast<float>(event->position().y())});
   const auto state = impl_->interactionStateController_.maskMoveState(
       impl_->proportionalEditRadius_);
   const auto result = impl_->maskMoveController_.handle(
       layer, QPointF(canvasPos.x, canvasPos.y), impl_->renderer_->getZoom(),
       state, impl_->maskDragController_, impl_->maskHoverController_,
       impl_->maskEditSession_);
   if (result.kind == LayerEditorMaskMoveKind::GeometryChanged) {
    impl_->requestRender();
    event->accept();
    return;
   }
   if (result.kind == LayerEditorMaskMoveKind::HoverChanged) {
    impl_->requestRender();
   }
   if (result.cursorRelevant && !state.draggingVertex) {
    if (result.vertexHovered) setCursor(Qt::CrossCursor);
    else unsetCursor();
   }
  }
  if (impl_->displayMode_ != DisplayMode::Mask &&
      isShapeEditingMode(impl_->editMode_) && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
       {static_cast<float>(event->position().x()),
        static_cast<float>(event->position().y())});
   const auto state = impl_->interactionStateController_.shapeMoveState(
       impl_->proportionalEditingEnabled_, impl_->proportionalEditRadius_);
   const auto result = impl_->shapeMoveController_.handle(
       layer, QPointF(canvasPos.x, canvasPos.y), impl_->renderer_->getZoom(),
       event->modifiers().testFlag(Qt::AltModifier), state,
       impl_->shapeDragController_, impl_->shapeHoverController_,
       impl_->shapeEditSession_);
   if (result.kind == LayerEditorShapeMoveKind::GeometryChanged) {
    impl_->requestRender();
    event->accept();
    return;
   }
   if (result.kind == LayerEditorShapeMoveKind::HoverChanged) {
    impl_->requestRender();
   }
   if (result.pathConsumed) {
    return;
   }
   if (result.polygonCursorRelevant) {
    if (result.polygonHandleHovered) setCursor(Qt::CrossCursor);
    else unsetCursor();
   }
 } // if (editMode_ == Paint)
 QWidget::mouseMoveEvent(event);
}


void ArtifactLayerEditorWidget::wheelEvent(QWheelEvent* event)
{
 if (!impl_->renderer_) {
  QWidget::wheelEvent(event);
  return;
 }

 const auto result = impl_->viewWheelController_.handle({
     event->angleDelta(),
     event->pixelDelta(),
     static_cast<bool>(event->modifiers() & Qt::ShiftModifier)});
 if (result.action == LayerEditorViewWheelAction::PanHorizontal) {
  if (std::abs(result.value) > 0.001f) {
   impl_->renderer_->panBy(result.value, 0.0f);
   impl_->requestRender();
  }
 } else if (result.action == LayerEditorViewWheelAction::Zoom) {
  impl_->zoomLevel_ = std::clamp(
      impl_->renderer_->getZoom() * result.value, 0.05f, 32.0f);
  zoomAroundPoint(event->position(), impl_->zoomLevel_);
 }
 event->accept();
}

void ArtifactLayerEditorWidget::resizeEvent(QResizeEvent* event)
{
 QWidget::resizeEvent(event);
 if (event->size().width() <= 0 || event->size().height() <= 0) {
  return;
 }
 if (impl_->hoveredChromeControl_ >= 0) {
  impl_->hoveredChromeControl_ = -1;
  setToolTip(QString{});
  const auto layer = impl_->targetLayer();
  if (isMaskEditingMode(impl_->editMode_) && layer &&
      layer->isVisible() && !layer->isLocked()) {
   setCursor(Qt::CrossCursor);
  } else {
   unsetCursor();
  }
 }
 impl_->recreateSwapChain(this);
 if (impl_->initialized_ && impl_->renderer_) {
  impl_->requestRender();
 }
}

void ArtifactLayerEditorWidget::paintEvent(QPaintEvent* event)
 {

 }

void ArtifactLayerEditorWidget::contextMenuEvent(QContextMenuEvent* event)
{
 if (!impl_) {
  QWidget::contextMenuEvent(event);
  return;
 }

 if (impl_->displayMode_ != DisplayMode::Mask &&
     isShapeEditingMode(impl_->editMode_) && impl_->renderer_) {
  const auto result = runLayerEditorShapeContextMenu(
      this, event->globalPos(), event->pos(), impl_->renderer_.get(),
      impl_->targetLayer(), impl_->shapeHoverController_,
      impl_->shapeEditSession_);
  if (result.consumed) {
   if (result.changed) impl_->requestRender();
   event->accept();
   return;
  }
 }

 if (runLayerEditorBackgroundContextMenu(
         this, event->globalPos(), impl_->showGrid_, impl_->showSafeMargins_,
         impl_->backgroundMode_)) {
  impl_->refreshBackgroundCache();
  if (impl_->initialized_ && impl_->renderer_) impl_->requestRender();
 }
 event->accept();
}

void ArtifactLayerEditorWidget::showEvent(QShowEvent* event)
 {
 QWidget::showEvent(event);
 if (auto *app = Artifact::ApplicationService::instance()) {
  if (auto *toolService = app->toolService()) {
   setDisplayMode(toolService->displayMode());
  }
 }
  qCDebug(layerViewPerfLog) << "[LayerView][Show]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
 if (!impl_->initialized_) {
  impl_->initialize(this);
  if (impl_->initialized_) {
   impl_->initializeSwapChain(this);
   impl_->renderer_->fitToViewport();
   impl_->zoomLevel_ = impl_->renderer_->getZoom();
  }
 }
 if (impl_->initialized_) {
  if (!impl_->targetLayerId_.isNil()) {
   setTargetLayer(impl_->targetLayerId_);
  } else {
    const LayerID selectedId = currentSelectedLayerId();
    if (!selectedId.isNil()) {
     setTargetLayer(selectedId);
    }
   }
   if (impl_->isPlay_) {
    impl_->startRenderLoop();
   } else {
    impl_->requestRender();
   }
  }
 }

 void ArtifactLayerEditorWidget::hideEvent(QHideEvent* event)
 {
  qCDebug(layerViewPerfLog) << "[LayerView][Hide]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
 if (impl_->initialized_) {
   impl_->stopRenderLoop();
  }
  impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
  QWidget::hideEvent(event);
 }

 void ArtifactLayerEditorWidget::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();
 QWidget::closeEvent(event);
 }

 void ArtifactLayerEditorWidget::focusInEvent(QFocusEvent* event)
 {
  QWidget::focusInEvent(event);
  if (auto *app = Artifact::ApplicationService::instance()) {
   if (auto *toolService = app->toolService()) {
    setDisplayMode(toolService->displayMode());
   }
  }
 }

 void ArtifactLayerEditorWidget::focusOutEvent(QFocusEvent* event)
 {
  impl_->updateViewportChromeHover(QPointF(-1.0, -1.0));
  QWidget::focusOutEvent(event);
 }

void ArtifactLayerEditorWidget::setClearColor(const FloatColor& color)
{
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  impl_->clearColor_ = color;
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
}

void ArtifactLayerEditorWidget::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 if (impl_->shapeEditSession_.polygonPending()) {
  impl_->commitShapeEditTransaction();
 }
 if (impl_->shapeEditSession_.pathPending()) {
  impl_->commitPathEditTransaction();
 }
 impl_->targetLayerId_ = id;
 const auto targetLayer = impl_->targetLayer();
 publishLayerEditorReadout(
     this, targetLayer,
     targetLayer && targetLayer->isActiveAt(currentLayerViewFrame()));
 impl_->surfaceInfoDirty_ = true;
 impl_->interactionStateController_.resetForTargetChange();
 const uint seed = qHash(id.toString());
 const auto channel = [seed](int shift) -> float {
  const int value = static_cast<int>((seed >> shift) & 0xFFu);
  return 0.25f + (static_cast<float>(value) / 255.0f) * 0.65f;
 };
 impl_->targetLayerTint_ = FloatColor(channel(0), channel(8), channel(16), 1.0f);
 if (impl_->renderer_) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    // コンポジションサイズを設定
    const auto compSize = composition->settings().compositionSize();
    if (compSize.width() > 0 && compSize.height() > 0) {
     impl_->renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
    }
    
    if (auto layer = composition->layerById(id)) {
     const auto source = layer->sourceSize();
     if (source.width > 0 && source.height > 0) {
      // レイヤーサイズは使用しない（コンポジションサイズを優先）
      // impl_->renderer_->setCanvasSize(static_cast<float>(source.width), static_cast<float>(source.height));
     }
     if (isShapeEditingMode(impl_->editMode_)) {
      ensureShapeEditSeedGeometry(layer);
     }
      impl_->syncTransformGizmo(layer);
      impl_->renderer_->fitToViewport();
      impl_->zoomLevel_ = impl_->renderer_->getZoom();
      impl_->requestRender();
      return;
     }
   }
  }
  if (impl_->transformGizmo_) {
   impl_->transformGizmo_->setLayer(nullptr);
  }
  impl_->renderer_->resetView();
  impl_->requestRender();
 }
}

 void ArtifactLayerEditorWidget::resetView()
 {
 impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) {
   impl_->renderer_->resetView();
   impl_->requestRender();
  }
}
 
 void ArtifactLayerEditorWidget::fitToViewport()
  {
   if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
    impl_->requestRender();
   }
  }
 
void ArtifactLayerEditorWidget::panBy(const QPointF& delta)
{
  if (impl_->renderer_) {
   impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
   impl_->requestRender();
  }
}

void ArtifactLayerEditorWidget::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
{
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
      impl_->requestRender();
  }
}

 void ArtifactLayerEditorWidget::setEditMode(EditMode mode)
 {
  if (impl_->surfaceMode_ != LayerSurfaceMode::Edit && mode != EditMode::View) {
   impl_->editModeBeforeSurface_ = mode;
   mode = EditMode::View;
  }
  const bool wasMaskMode = isMaskEditingMode(impl_->editMode_);
  const bool isMaskMode = isMaskEditingMode(mode);
 impl_->editMode_ = mode;
 if (isMaskMode) {
   if (!wasMaskMode && impl_->displayMode_ != DisplayMode::Mask) {
    impl_->displayModeBeforeMask_ = impl_->displayMode_;
   }
   impl_->displayMode_ = DisplayMode::Mask;
   const auto layer = impl_->targetLayer();
   if (layer && layer->isVisible() && !layer->isLocked()) {
    setCursor(Qt::CrossCursor);
   } else {
    unsetCursor();
   }
  } else {
   unsetCursor();
   impl_->interactionStateController_.resetForNonMaskMode();
   if (impl_->shapeEditSession_.polygonPending()) {
    impl_->commitShapeEditTransaction();
   }
   if (impl_->shapeEditSession_.pathPending()) {
    impl_->commitPathEditTransaction();
   }
   if (wasMaskMode) {
    impl_->displayMode_ = impl_->displayModeBeforeMask_;
   }
  }
  if (impl_->transformGizmo_) {
   if (isMaskMode) {
    impl_->transformGizmo_->setLayer(nullptr);
   } else {
    impl_->syncTransformGizmo(impl_->targetLayer());
   }
  }
  if (isShapeEditingMode(mode) && !impl_->targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(impl_->targetLayerId_)) {
      ensureShapeEditSeedGeometry(layer);
     }
    }
   }
  }
  publishLayerEditorModeReadout(
      impl_->widget_, impl_->editMode_, impl_->displayMode_);
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
 }

 void ArtifactLayerEditorWidget::setDisplayMode(DisplayMode mode)
 {
  if (isMaskEditingMode(impl_->editMode_)) {
    if (mode != DisplayMode::Mask) {
     impl_->displayModeBeforeMask_ = mode;
    }
    impl_->displayMode_ = DisplayMode::Mask;
  } else {
    impl_->displayMode_ = mode;
    if (mode != DisplayMode::Mask) {
     impl_->displayModeBeforeMask_ = mode;
    }
  }
  if (impl_->transformGizmo_) {
   impl_->syncTransformGizmo(impl_->targetLayer());
  }
  publishLayerEditorModeReadout(
      impl_->widget_, impl_->editMode_, impl_->displayMode_);
  if (impl_->initialized_ && impl_->renderer_) {
   impl_->requestRender();
  }
 }

void ArtifactLayerEditorWidget::setPan(const QPointF& offset)
{
 if (impl_->renderer_) {
  impl_->renderer_->setPan((float)offset.x(), (float)offset.y());
  impl_->requestRender();
 }
}

 float ArtifactLayerEditorWidget::zoom() const
 {
  return impl_->zoomLevel_;
 }

void ArtifactLayerEditorWidget::setTargetLayer(LayerID& id)
{
 setTargetLayer(static_cast<const LayerID&>(id));
}

 QImage ArtifactLayerEditorWidget::grabScreenShot()
 {
  return grab().toImage();
 }

 void ArtifactLayerEditorWidget::play()
 {
  if (!impl_->initialized_) {
   return;
  }
  impl_->isPlay_ = true;
  impl_->startRenderLoop();
 }

 void ArtifactLayerEditorWidget::stop()
 {
  impl_->isPlay_ = false;
  impl_->stopRenderLoop();
 }

 void ArtifactLayerEditorWidget::takeScreenShot()
 {
  const QImage image = grabScreenShot();
  if (image.isNull()) {
   return;
  }

  QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (defaultDir.isEmpty()) {
   defaultDir = QDir::homePath();
  }
  const QString defaultPath = QDir(defaultDir).filePath(
   QStringLiteral("artifact-layer-view-%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))));
  const QString filePath = QFileDialog::getSaveFileName(
   this,
   QStringLiteral("Save Layer View Snapshot"),
   defaultPath,
   QStringLiteral("PNG Image (*.png)"));
  if (filePath.isEmpty()) {
   return;
  }
  image.save(filePath);
 }

};
