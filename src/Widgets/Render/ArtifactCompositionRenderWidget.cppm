module;
#include <utility>
#define NOMINMAX
#include <windows.h>
#include <tbb/tbb.h>
#include <QWidget>
#include <QMenu>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEnterEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QRectF>
#include <QTimer>
#include <QDebug>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderWidget;

import Artifact.Render.IRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Tool.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Service.Playback;
import Event.Bus;
import Artifact.Event.Types;
import InputEvent;
import Input.Operator;
import Undo.UndoManager;
import Time.Rational;
import Utils.Id;
import Utils.Point.Like;
import InputEvent;
import Input.Operator;

namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactCompositionRenderWidget)

 namespace {
 enum class LayerDragMode {
  None,
  Move,
  ScaleTL,
  ScaleTR,
  ScaleBL,
  ScaleBR
 };

 LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr& layer,
                                    const QPointF& viewportPos,
                                    ArtifactIRenderer* renderer)
 {
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

  if (containsHandle((float)bbox.left(), (float)bbox.top())) {
   return LayerDragMode::ScaleTL;
  }
  if (containsHandle((float)bbox.right(), (float)bbox.top())) {
   return LayerDragMode::ScaleTR;
  }
  if (containsHandle((float)bbox.left(), (float)bbox.bottom())) {
   return LayerDragMode::ScaleBL;
  }
 if (containsHandle((float)bbox.right(), (float)bbox.bottom())) {
   return LayerDragMode::ScaleBR;
  }

  return LayerDragMode::Move;
 }

 Qt::CursorShape cursorForLayerDragMode(LayerDragMode mode, bool dragging)
 {
  switch (mode) {
  case LayerDragMode::Move:
   return dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor;
  case LayerDragMode::ScaleTL:
  case LayerDragMode::ScaleBR:
   return Qt::SizeFDiagCursor;
  case LayerDragMode::ScaleTR:
  case LayerDragMode::ScaleBL:
   return Qt::SizeBDiagCursor;
  default:
   return Qt::ArrowCursor;
  }
 }
 } // namespace

 class ArtifactCompositionRenderWidget::Impl {
 public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  bool initialized_ = false;
  std::atomic_bool isPlaying_{ false };
  std::atomic_bool needsRender_{ true };
  std::atomic_bool running_{ false };
  tbb::task_group renderTask_;
  std::mutex renderMutex_;
  QWidget* widget_ = nullptr;
  QTimer* resizeDebounceTimer_ = nullptr;
  QTimer* wheelRenderTimer_ = nullptr;
  QSize pendingResizeSize_;
  
  QPointF lastMousePos_;
  ArtifactCore::LayerID selectedLayerId_ = ArtifactCore::LayerID::Nil();
  bool isDraggingLayer_ = false;
  bool isPanningViewport_ = false;
  LayerDragMode dragMode_ = LayerDragMode::None;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  QRectF dragStartBoundingBox_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  int64_t dragFrame_ = 0;
  QPointF dragAppliedDelta_;
  
  Impl() = default;
  ~Impl() { destroy(); }

  void initialize(QWidget* window) {
   widget_ = window;
   renderer_ = std::make_unique<ArtifactIRenderer>();
   renderer_->initialize(window);
   resizeDebounceTimer_ = new QTimer(widget_);
   resizeDebounceTimer_->setSingleShot(true);
   QObject::connect(resizeDebounceTimer_, &QTimer::timeout, widget_, [this]() {
    if (!initialized_ || !renderer_) {
     return;
    }
    const QSize pendingSize = pendingResizeSize_.isValid() ? pendingResizeSize_ : widget_->size();
    renderer_->recreateSwapChain(widget_);
    renderer_->setViewportSize(static_cast<float>(pendingSize.width()),
                               static_cast<float>(pendingSize.height()));
    requestRender();
   });
   wheelRenderTimer_ = new QTimer(widget_);
   wheelRenderTimer_->setSingleShot(true);
   QObject::connect(wheelRenderTimer_, &QTimer::timeout, widget_, [this]() {
    requestRender();
   });
   if (auto* playback = ArtifactPlaybackService::instance()) {
   QObject::connect(playback, &ArtifactPlaybackService::playbackStateChanged, widget_,
                     [this](::Artifact::PlaybackState state) {
      isPlaying_.store(state == ::Artifact::PlaybackState::Playing, std::memory_order_release);
      requestRender();
     });
    QObject::connect(playback, &ArtifactPlaybackService::frameChanged, widget_,
                     [this](const auto&) {
      requestRender();
     });
   }
   initialized_ = true;
   needsRender_.store(true, std::memory_order_release);
  }

  void destroy() {
   stopRenderLoop();
   if (renderer_) renderer_->destroy();
   initialized_ = false;
  }

  void startRenderLoop() {
   if (running_) return;
   running_ = true;
   renderTask_.run([this]() {
    while (running_.load(std::memory_order_acquire)) {
     const bool playing = isPlaying_.load(std::memory_order_acquire);
     const bool dirty = needsRender_.exchange(false, std::memory_order_acq_rel);
     if (!playing && !dirty) {
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
      continue;
     }
     {
      std::lock_guard<std::mutex> lock(renderMutex_);
      renderOneFrame();
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
   });
  }

  void stopRenderLoop() {
   running_ = false;
   renderTask_.wait();
   if (renderer_) renderer_->flushAndWait();
  }

  void renderOneFrame() {
   if (!initialized_ || !renderer_) return;
   auto comp = previewPipeline_.composition();
   if (comp) {
    auto size = comp->settings().compositionSize();
    renderer_->setCanvasSize((float)size.width(), (float)size.height());
    previewPipeline_.setCurrentFrame(comp->framePosition().framePosition());
   }
   previewPipeline_.render(renderer_.get());
   renderer_->present();
  }

  void requestRender() {
   needsRender_.store(true, std::memory_order_release);
  }
  
  static ArtifactCore::InputEvent::Modifiers translateModifiers(Qt::KeyboardModifiers qtMods) {
   ArtifactCore::InputEvent::Modifiers mods = ArtifactCore::InputEvent::ModifierKey::None;
   if (qtMods & Qt::ShiftModifier) mods |= ArtifactCore::InputEvent::ModifierKey::LShift;
   if (qtMods & Qt::ControlModifier) mods |= ArtifactCore::InputEvent::ModifierKey::LCtrl;
   if (qtMods & Qt::AltModifier) mods |= ArtifactCore::InputEvent::ModifierKey::LAlt;
   if (qtMods & Qt::MetaModifier) mods |= ArtifactCore::InputEvent::ModifierKey::LMeta;
   return mods;
  }

  // Snapping logic
  float snapValue(float val, float target, float threshold) {
   if (std::abs(val - target) < threshold) return target;
   return val;
  }

  void updateHoverCursor(const QPointF& viewportPos)
  {
   if (isPanningViewport_) {
    widget_->setCursor(Qt::ClosedHandCursor);
    return;
   }

   if (isDraggingLayer_) {
    widget_->setCursor(cursorForLayerDragMode(dragMode_, true));
    return;
   }

   if (!renderer_) {
    widget_->setCursor(Qt::ArrowCursor);
    return;
   }

   std::lock_guard<std::mutex> lock(renderMutex_);
   auto comp = previewPipeline_.composition();
   if (!comp) {
    auto* tm = ArtifactApplicationManager::instance()->toolManager();
    widget_->setCursor(tm && tm->activeTool() == ToolType::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
   }

   auto cPos = renderer_->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
   ArtifactAbstractLayerPtr hitLayer = nullptr;
   const auto layers = comp->allLayer();
   for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i] || !layers[i]->isVisible()) continue;
    if (layers[i]->transformedBoundingBox().contains(cPos.x, cPos.y)) {
     hitLayer = layers[i];
     break;
    }
   }

   if (hitLayer) {
    widget_->setCursor(cursorForLayerDragMode(hitTestLayerDragMode(hitLayer, viewportPos, renderer_.get()), false));
    return;
   }

   auto* tm = ArtifactApplicationManager::instance()->toolManager();
   widget_->setCursor(tm && tm->activeTool() == ToolType::Hand ? Qt::OpenHandCursor : Qt::ArrowCursor);
  }
 };

 ArtifactCompositionRenderWidget::ArtifactCompositionRenderWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl()) {
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_DontCreateNativeAncestors);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
 }

 ArtifactCompositionRenderWidget::~ArtifactCompositionRenderWidget() {
  delete impl_;
 }

 void ArtifactCompositionRenderWidget::setComposition(ArtifactCompositionPtr composition) {
  std::lock_guard<std::mutex> lock(impl_->renderMutex_);
  impl_->previewPipeline_.setComposition(composition);
  if (auto* playback = ArtifactPlaybackService::instance()) {
   playback->setCurrentComposition(composition);
  }
  if (composition && impl_->renderer_) {
   auto size = composition->settings().compositionSize();
   impl_->renderer_->setCanvasSize((float)size.width(), (float)size.height());
   impl_->renderer_->fitToViewport();
  }
  impl_->requestRender();
  ArtifactApplicationManager::instance()->activeContextService()->setActiveComposition(composition);
 }

 void ArtifactCompositionRenderWidget::setClearColor(const FloatColor& color) {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->setClearColor(color);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::play() {
  if (auto* playback = ArtifactPlaybackService::instance()) {
   playback->play();
  } else {
   impl_->isPlaying_.store(true, std::memory_order_release);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::stop() {
  if (auto* playback = ArtifactPlaybackService::instance()) {
   playback->stop();
  }
  impl_->isPlaying_.store(false, std::memory_order_release);
  impl_->requestRender();
 }

 void ArtifactCompositionRenderWidget::resetView() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->resetView();
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomIn() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->zoomAroundViewportPoint({width() / 2.0f, height() / 2.0f}, 1.1f);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomOut() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->zoomAroundViewportPoint({width() / 2.0f, height() / 2.0f}, 0.909f);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomFit() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->fitToViewport(0.0f);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoom100() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->setZoom(1.0f);
   // Center the canvas in the viewport at 100% zoom
   if (auto comp = impl_->previewPipeline_.composition()) {
    auto size = comp->settings().compositionSize();
    const float cw  = static_cast<float>(size.width());
    const float ch  = static_cast<float>(size.height());
    const float dpr = static_cast<float>(devicePixelRatioF());
    const float panX = (width()  * dpr - cw) * 0.5f;
    const float panY = (height() * dpr - ch) * 0.5f;
    impl_->renderer_->setPan(panX, panY);
   }
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (impl_->initialized_) {
   impl_->pendingResizeSize_ = event->size();
   if (impl_->resizeDebounceTimer_) {
    impl_->resizeDebounceTimer_->stop();
    impl_->resizeDebounceTimer_->start(80);
   }
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::paintEvent(QPaintEvent*) {}

 void ArtifactCompositionRenderWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!impl_->initialized_) {
   QTimer::singleShot(0, this, [this]() {
    if (!impl_ || impl_->initialized_ || !isVisible()) {
     return;
    }
    impl_->initialize(this);
    impl_->renderer_->setViewportSize((float)width(), (float)height());

    connect(ArtifactApplicationManager::instance()->layerSelectionManager(), &ArtifactLayerSelectionManager::selectionChanged, this, [this]() {
     auto* selection = ArtifactApplicationManager::instance()->layerSelectionManager();
     auto current = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
     if (current) impl_->selectedLayerId_ = current->id();
     else impl_->selectedLayerId_ = ArtifactCore::LayerID::Nil();
     impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);
     impl_->requestRender();
    });

    connect(ArtifactApplicationManager::instance()->toolManager(), &ArtifactToolManager::toolChanged, this, [this](ToolType tool) {
        if (tool == ToolType::Hand) setCursor(Qt::OpenHandCursor);
        else setCursor(Qt::ArrowCursor);
        impl_->requestRender();
    });

    impl_->requestRender();
    impl_->startRenderLoop();
   });
  }
 }

 void ArtifactCompositionRenderWidget::closeEvent(QCloseEvent* event) {
  impl_->destroy();
  QWidget::closeEvent(event);
 }

  void ArtifactCompositionRenderWidget::focusInEvent(QFocusEvent* event) {
  QWidget::focusInEvent(event);
  ArtifactCore::InputOperator::instance()->setActiveContext("Viewport");
 }

 void ArtifactCompositionRenderWidget::focusOutEvent(QFocusEvent* event) {
  QWidget::focusOutEvent(event);
 }

void ArtifactCompositionRenderWidget::enterEvent(QEnterEvent* event) {
  QWidget::enterEvent(event);
  impl_->updateHoverCursor(event->position());
}

 void ArtifactCompositionRenderWidget::leaveEvent(QEvent* event) {
  if (!impl_->isPanningViewport_ && !impl_->isDraggingLayer_) {
   unsetCursor();
  }
  QWidget::leaveEvent(event);
 }

 void ArtifactCompositionRenderWidget::mouseDoubleClickEvent(QMouseEvent* event) {
  resetView();
  event->accept();
 }

 void ArtifactCompositionRenderWidget::wheelEvent(QWheelEvent* event) {
  float delta = event->angleDelta().y() / 120.0f;
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   float zoomFactor = (delta > 0) ? 1.1f : 0.909f;
   QPointF pos = event->position();
   impl_->renderer_->zoomAroundViewportPoint({(float)pos.x(), (float)pos.y()}, zoomFactor);
   if (impl_->wheelRenderTimer_) {
    impl_->wheelRenderTimer_->start(16);
   } else {
    impl_->requestRender();
   }
  }
  event->accept();
 }

 void ArtifactCompositionRenderWidget::mousePressEvent(QMouseEvent* event) {
  auto* tm = ArtifactApplicationManager::instance()->toolManager();
  bool isHandShortcut = false; // Space checking needs explicit key tracking
  
  if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && (tm->activeTool() == ToolType::Hand || isHandShortcut))) {
   setCursor(Qt::ClosedHandCursor);
   impl_->lastMousePos_ = event->position();
   impl_->isPanningViewport_ = true;
   grabMouse();
   impl_->requestRender();
   event->accept();
  } else if (event->button() == Qt::RightButton) {
      // Context Menu
      if (impl_->renderer_) {
          std::lock_guard<std::mutex> lock(impl_->renderMutex_);
          auto cPos = impl_->renderer_->viewportToCanvas({(float)event->position().x(), (float)event->position().y()});
          auto comp = impl_->previewPipeline_.composition();
          if (comp) {
              auto layers = comp->allLayer();
              ArtifactAbstractLayerPtr hitLayer = nullptr;
              for (int i = (int)layers.size() - 1; i >= 0; --i) {
                  if (layers[i] && layers[i]->isVisible() && layers[i]->transformedBoundingBox().contains(cPos.x, cPos.y)) {
                      hitLayer = layers[i];
                      break;
                  }
              }
              
              if (hitLayer) {
                  ArtifactApplicationManager::instance()->layerSelectionManager()->selectLayer(hitLayer);
                  
                  QMenu menu(this);
                  menu.addAction("Center in Comp", [hitLayer, comp]() {
                      auto size = comp->settings().compositionSize();
                      auto& t3 = hitLayer->transform3D();
                      t3.setPosition(ArtifactCore::RationalTime(comp->framePosition().framePosition(), 30000), size.width() / 2.0f, size.height() / 2.0f);
                      hitLayer->changed();
                  });
                  menu.addSeparator();
                  menu.addAction("Bring to Front", [hitLayer, comp]() { comp->bringToFront(hitLayer->id()); });
                  menu.addAction("Send to Back", [hitLayer, comp]() { comp->sendToBack(hitLayer->id()); });
                  menu.exec(event->globalPosition().toPoint());
              }
      }
      }
      event->accept();
      impl_->updateHoverCursor(event->position());
      impl_->requestRender();
  } else if (event->button() == Qt::LeftButton) {
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    auto cPos = impl_->renderer_->viewportToCanvas({(float)event->position().x(), (float)event->position().y()});
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
     auto layers = comp->allLayer();
     ArtifactAbstractLayerPtr hitLayer = nullptr;
     for (int i = (int)layers.size() - 1; i >= 0; --i) {
      if (!layers[i] || !layers[i]->isVisible()) continue;
      if (layers[i]->transformedBoundingBox().contains(cPos.x, cPos.y)) {
       hitLayer = layers[i];
       break;
      }
     }
     
     if (hitLayer) {
      if (event->modifiers() & Qt::ShiftModifier) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->addToSelection(hitLayer);
      } else {
          ArtifactApplicationManager::instance()->layerSelectionManager()->selectLayer(hitLayer);
      }
      impl_->selectedLayerId_ = hitLayer->id();
      impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);

      if (tm->activeTool() != ToolType::Selection) {
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
          impl_->updateHoverCursor(event->position());
          event->accept();
          return;
      }

      impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
      impl_->dragStartLayerPos_ = QPointF(hitLayer->transform3D().positionX(),
                                          hitLayer->transform3D().positionY());
      impl_->dragStartScaleX_ = hitLayer->transform3D().scaleX();
      impl_->dragStartScaleY_ = hitLayer->transform3D().scaleY();
      impl_->dragStartBoundingBox_ = hitLayer->transformedBoundingBox();
      impl_->dragFrame_ = comp->framePosition().framePosition();
      impl_->dragAppliedDelta_ = QPointF(0.0, 0.0);
      impl_->dragMode_ = hitTestLayerDragMode(hitLayer, event->position(), impl_->renderer_.get());
      if (impl_->dragMode_ == LayerDragMode::None) {
       impl_->dragMode_ = LayerDragMode::Move;
      }
      impl_->isDraggingLayer_ = true;
     } else {
      if (!(event->modifiers() & Qt::ShiftModifier)) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->clearSelection();
          impl_->selectedLayerId_ = ArtifactCore::LayerID::Nil();
          impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);
      }
      impl_->isDraggingLayer_ = false;
      impl_->dragMode_ = LayerDragMode::None;
     }
    }
   }
   impl_->updateHoverCursor(event->position());
   impl_->requestRender();
   event->accept();
  }
 }

 void ArtifactCompositionRenderWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (impl_->isDraggingLayer_) {
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    QPointF totalDelta = impl_->dragAppliedDelta_;
    
    // Applying snapping/constraints on total delta if Shift was held during release? 
    // Usually handled during Move.
    
    auto comp = impl_->previewPipeline_.composition();
    if (comp && !impl_->selectedLayerId_.isNil() && impl_->dragMode_ == LayerDragMode::Move &&
        (std::abs(totalDelta.x()) > 0.01 || std::abs(totalDelta.y()) > 0.01)) {
     auto layer = comp->layerById(impl_->selectedLayerId_);
     if (layer) {
      // Final position snapshot for undo
      auto& t3 = layer->transform3D();
      ArtifactCore::RationalTime t0(comp->framePosition().framePosition(), 30000);
      
      // Since it was already moving in real-time in mouseMove, 
      // we need to calculate the REAL delta from drag start.
      // But UndoCommand usually takes the delta from the OLD state.
      
      // Let's assume MoveLayerCommand(layer, dx, dy) adds dx, dy to OLD values on Redo.
      // So we must pass the actual delta we moved.
      
      auto cmd = std::make_unique<MoveLayerCommand>(layer, (float)totalDelta.x(), (float)totalDelta.y(), comp->framePosition().framePosition());
      
      // Revert the real-time move before pushing so redo() applies it cleanly
      t3.setPosition(t0, t3.positionX() - (float)totalDelta.x(), t3.positionY() - (float)totalDelta.y());
      UndoManager::instance()->push(std::move(cmd));
      layer->changed(); // Final notification
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
     }
    }
    impl_->dragAppliedDelta_ = totalDelta;
   }
  }
  auto* tm = ArtifactApplicationManager::instance()->toolManager();
  if (tm->activeTool() == ToolType::Hand) setCursor(Qt::OpenHandCursor);
  else impl_->updateHoverCursor(event->position());
  impl_->isDraggingLayer_ = false;
  impl_->isPanningViewport_ = false;
  releaseMouse();
  impl_->dragMode_ = LayerDragMode::None;
  impl_->requestRender();
  event->accept();
 }

 void ArtifactCompositionRenderWidget::mouseMoveEvent(QMouseEvent* event) {
  if (impl_->isPanningViewport_) {
   QPointF delta = event->position() - impl_->lastMousePos_;
   impl_->lastMousePos_ = event->position();
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
    impl_->requestRender();
   }
   event->accept();
  } else if (event->buttons() & Qt::LeftButton && impl_->isDraggingLayer_) {
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    auto cPos = impl_->renderer_->viewportToCanvas({(float)event->position().x(), (float)event->position().y()});
    
    QPointF currentCanvasPos(cPos.x, cPos.y);
    QPointF totalDelta = currentCanvasPos - impl_->dragStartCanvasPos_;
    
    // 1. Constraint (Shift)
    if (event->modifiers() & Qt::ShiftModifier) {
        if (std::abs(totalDelta.x()) > std::abs(totalDelta.y())) totalDelta.setY(0);
        else totalDelta.setX(0);
    }
    
    // 2. Snapping (Ctrl or default)
    bool isSnapping = (event->modifiers() & Qt::ControlModifier);
    if (isSnapping) {
        auto comp = impl_->previewPipeline_.composition();
        if (comp) {
            auto size = comp->settings().compositionSize();
            float centerX = size.width() / 2.0f;
            float centerY = size.height() / 2.0f;
            
            auto layer = comp->layerById(impl_->selectedLayerId_);
            if (layer) {
                // Potential snap targets
                float threshold = 10.0f; // in canvas units
                totalDelta.setX(impl_->snapValue(impl_->dragStartCanvasPos_.x() + totalDelta.x(), centerX, threshold) - impl_->dragStartCanvasPos_.x());
                totalDelta.setY(impl_->snapValue(impl_->dragStartCanvasPos_.y() + totalDelta.y(), centerY, threshold) - impl_->dragStartCanvasPos_.y());
            }
        }
    }

    auto comp = impl_->previewPipeline_.composition();
    if (comp && !impl_->selectedLayerId_.isNil()) {
     auto layer = comp->layerById(impl_->selectedLayerId_);
      if (layer) {
      auto& t3 = layer->transform3D();
      ArtifactCore::RationalTime t0(comp->framePosition().framePosition(), 30000);
      if (impl_->dragMode_ == LayerDragMode::Move) {
       QPointF moveDelta = totalDelta;
       t3.setPosition(t0,
                      impl_->dragStartLayerPos_.x() + static_cast<float>(moveDelta.x()),
                      impl_->dragStartLayerPos_.y() + static_cast<float>(moveDelta.y()));
       layer->setDirty(LayerDirtyFlag::Transform);
       layer->changed();
       ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
           LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                             LayerChangedEvent::ChangeType::Modified});
      } else {
       if (std::abs(totalDelta.x()) < 0.01 && std::abs(totalDelta.y()) < 0.01) {
        impl_->dragAppliedDelta_ = totalDelta;
        event->accept();
        return;
       }
       const QRectF startBox = impl_->dragStartBoundingBox_;
       if (startBox.isValid() && startBox.width() > 0.0 && startBox.height() > 0.0) {
        double newW = startBox.width();
        double newH = startBox.height();
        switch (impl_->dragMode_) {
        case LayerDragMode::ScaleTL:
         newW -= totalDelta.x();
         newH -= totalDelta.y();
         break;
        case LayerDragMode::ScaleTR:
         newW += totalDelta.x();
         newH -= totalDelta.y();
         break;
        case LayerDragMode::ScaleBL:
         newW -= totalDelta.x();
         newH += totalDelta.y();
         break;
        case LayerDragMode::ScaleBR:
         newW += totalDelta.x();
         newH += totalDelta.y();
         break;
        default:
         break;
        }

        const double safeStartW = std::max(1.0, startBox.width());
        const double safeStartH = std::max(1.0, startBox.height());
        const double safeW = std::max(1.0, newW);
        const double safeH = std::max(1.0, newH);
        const float scaleFactorX = static_cast<float>(safeW / safeStartW);
        const float scaleFactorY = static_cast<float>(safeH / safeStartH);
        t3.setScale(t0, impl_->dragStartScaleX_ * scaleFactorX,
                    impl_->dragStartScaleY_ * scaleFactorY);
        layer->setDirty(LayerDirtyFlag::Transform);
        layer->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
       }
      }
     }
   }
   impl_->dragAppliedDelta_ = totalDelta;
   }
   impl_->requestRender();
   event->accept();
  } else {
   impl_->updateHoverCursor(event->position());
  }
 }

 void ArtifactCompositionRenderWidget::keyPressEvent(QKeyEvent* event) {
  auto* am = ArtifactApplicationManager::instance();
  auto* tm = am->toolManager();
  auto* ctx = am->activeContextService();
  
  if (!event->isAutoRepeat()) {
      if (event->key() == Qt::Key_V) tm->setActiveTool(ToolType::Selection);
      else if (event->key() == Qt::Key_H) tm->setActiveTool(ToolType::Hand);
      else if (event->key() == Qt::Key_Z) tm->setActiveTool(ToolType::Zoom);
      else if (event->key() == Qt::Key_W) tm->setActiveTool(ToolType::Rotation);
      else if (event->key() == Qt::Key_Space) ctx->togglePlayPause();
      else if (event->key() == Qt::Key_Home) ctx->goToStart();
      else if (event->key() == Qt::Key_End) ctx->goToEnd();
      else if (event->key() == Qt::Key_PageUp) ctx->prevFrame();
      else if (event->key() == Qt::Key_PageDown) ctx->nextFrame();
      else if (event->key() == Qt::Key_Z && event->modifiers() & Qt::ControlModifier) {
          if (event->modifiers() & Qt::ShiftModifier) UndoManager::instance()->redo();
          else UndoManager::instance()->undo();
      }
      else if (event->key() == Qt::Key_BracketLeft) ctx->setLayerInAtCurrentTime();
      else if (event->key() == Qt::Key_BracketRight) ctx->setLayerOutAtCurrentTime();
      
      // Arrow keys for nudge
      else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
          auto l = am->layerSelectionManager()->currentLayer();
          if (l) {
              float step = (event->modifiers() & Qt::ShiftModifier) ? 10.0f : 1.0f;
              auto& t3 = l->transform3D();
              auto comp = am->activeContextService()->activeComposition();
              ArtifactCore::RationalTime t0(comp ? comp->framePosition().framePosition() : 0, 30000);
              float dx = 0, dy = 0;
              if (event->key() == Qt::Key_Left) dx = -step;
              if (event->key() == Qt::Key_Right) dx = step;
              if (event->key() == Qt::Key_Up) dy = -step;
              if (event->key() == Qt::Key_Down) dy = step;
              
              auto cmd = std::make_unique<MoveLayerCommand>(l, dx, dy, t0.value());
              UndoManager::instance()->push(std::move(cmd));
              l->changed();
          }
      }
  }
  impl_->requestRender();
  event->accept();
 }

 void ArtifactCompositionRenderWidget::keyReleaseEvent(QKeyEvent* event) {
  event->accept();
 }

}
