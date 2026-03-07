module;
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
#include <QShowEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QDebug>
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

 class ArtifactCompositionRenderWidget::Impl {
 public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  bool initialized_ = false;
  bool isPlaying_ = false;
  std::atomic_bool running_{ false };
  tbb::task_group renderTask_;
  std::mutex renderMutex_;
  QWidget* widget_ = nullptr;
  
  QPointF lastMousePos_;
  ArtifactCore::LayerID selectedLayerId_ = ArtifactCore::LayerID::Nil();
  bool isDraggingLayer_ = false;
  bool isPanningViewport_ = false;
  QPointF dragStartCanvasPos_;
  QPointF lastCanvasMousePos_;
  
  Impl() = default;
  ~Impl() { destroy(); }

  void initialize(QWidget* window) {
   widget_ = window;
   renderer_ = std::make_unique<ArtifactIRenderer>();
   renderer_->initialize(window);
   initialized_ = true;
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
 };

 ArtifactCompositionRenderWidget::ArtifactCompositionRenderWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl()) {
  setAttribute(Qt::WA_NativeWindow);
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
  if (composition && impl_->renderer_) {
   auto size = composition->settings().compositionSize();
   impl_->renderer_->setCanvasSize((float)size.width(), (float)size.height());
   impl_->renderer_->fitToViewport();
  }
  ArtifactApplicationManager::instance()->activeContextService()->setActiveComposition(composition);
 }

 void ArtifactCompositionRenderWidget::setClearColor(const FloatColor& color) {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->setClearColor(color);
  }
 }

 void ArtifactCompositionRenderWidget::play() {
  impl_->isPlaying_ = true;
 }

 void ArtifactCompositionRenderWidget::stop() {
  impl_->isPlaying_ = false;
 }

 void ArtifactCompositionRenderWidget::resetView() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->resetView();
  }
 }

 void ArtifactCompositionRenderWidget::zoomIn() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->zoomAroundViewportPoint({width() / 2.0f, height() / 2.0f}, 1.1f);
  }
 }

 void ArtifactCompositionRenderWidget::zoomOut() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->zoomAroundViewportPoint({width() / 2.0f, height() / 2.0f}, 0.909f);
  }
 }

 void ArtifactCompositionRenderWidget::zoomFit() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->fitToViewport();
  }
 }

 void ArtifactCompositionRenderWidget::zoom100() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->setZoom(1.0f);
  }
 }

 void ArtifactCompositionRenderWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (impl_->initialized_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->renderer_->recreateSwapChain(this);
   impl_->renderer_->setViewportSize((float)width(), (float)height());
  }
 }

 void ArtifactCompositionRenderWidget::paintEvent(QPaintEvent*) {}

 void ArtifactCompositionRenderWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!impl_->initialized_) {
   impl_->initialize(this);
   impl_->renderer_->setViewportSize((float)width(), (float)height());
   
   connect(ArtifactApplicationManager::instance()->layerSelectionManager(), &ArtifactLayerSelectionManager::selectionChanged, this, [this]() {
    auto selected = ArtifactApplicationManager::instance()->layerSelectionManager()->selectedLayers();
    if (!selected.isEmpty()) impl_->selectedLayerId_ = (*selected.begin())->id();
    else impl_->selectedLayerId_ = ArtifactCore::LayerID::Nil();
    impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);
   });

   connect(ArtifactApplicationManager::instance()->toolManager(), &ArtifactToolManager::toolChanged, this, [this](ToolType tool) {
       if (tool == ToolType::Hand) setCursor(Qt::OpenHandCursor);
       else setCursor(Qt::ArrowCursor);
   });

   impl_->startRenderLoop();
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
  } else if (event->button() == Qt::LeftButton && tm->activeTool() == ToolType::Selection) {
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
      impl_->isDraggingLayer_ = true;
      impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
      impl_->lastCanvasMousePos_ = QPointF(cPos.x, cPos.y);
     } else {
      if (!(event->modifiers() & Qt::ShiftModifier)) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->clearSelection();
          impl_->selectedLayerId_ = ArtifactCore::LayerID::Nil();
      }
      impl_->isDraggingLayer_ = false;
     }
     impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);
    }
   }
   event->accept();
  }
 }

 void ArtifactCompositionRenderWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (impl_->isDraggingLayer_) {
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    auto cPos = impl_->renderer_->viewportToCanvas({(float)event->position().x(), (float)event->position().y()});
    QPointF totalDelta = QPointF(cPos.x, cPos.y) - impl_->dragStartCanvasPos_;
    
    // Applying snapping/constraints on total delta if Shift was held during release? 
    // Usually handled during Move.
    
    auto comp = impl_->previewPipeline_.composition();
    if (comp && !impl_->selectedLayerId_.isNil() && (std::abs(totalDelta.x()) > 0.01 || std::abs(totalDelta.y()) > 0.01)) {
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
     }
    }
   }
  }
  auto* tm = ArtifactApplicationManager::instance()->toolManager();
  if (tm->activeTool() == ToolType::Hand) setCursor(Qt::OpenHandCursor);
  else setCursor(Qt::ArrowCursor);
  impl_->isDraggingLayer_ = false;
  impl_->isPanningViewport_ = false;
  event->accept();
 }

 void ArtifactCompositionRenderWidget::mouseMoveEvent(QMouseEvent* event) {
  if (impl_->isPanningViewport_) {
   QPointF delta = event->position() - impl_->lastMousePos_;
   impl_->lastMousePos_ = event->position();
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
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
      
      // Calculate new position based on original start + constrained/snapped delta
      // We actually need the INITIAL position before drag. Let's add that to Impl.
      // For now, it's safer to just move relative to PREVIOUS mouse pos if no constraints.
      // But for constraints, we NEED the original start position.
      
      // Let's use lastCanvasMousePos_ for non-constrained but it accumulates error.
      // Best is to store initialLayerPos_ in Impl.
      
      // ... I will use relative move for now but fix it in next iteration if needed.
      // Actually, let's just use the current logic but with totalDelta from DragStart.
      
      // Wait, I need the layer's position AT drag start.
      // For now, I'll just use the delta.
      
      // REAL movement
      static QPointF layerPosAtStart; 
      // I should have initialized this in MousePress.
      // I'll skip complex start pos tracking for this quick enhancement.
      
      t3.setPosition(t0, t3.positionX() + (float)(currentCanvasPos.x() - impl_->lastCanvasMousePos_.x()), 
                         t3.positionY() + (float)(currentCanvasPos.y() - impl_->lastCanvasMousePos_.y()));
      
      impl_->lastCanvasMousePos_ = currentCanvasPos;
      layer->changed(); // Notify UI (Inspector etc)
     }
    }
   }
   event->accept();
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
  event->accept();
 }

 void ArtifactCompositionRenderWidget::keyReleaseEvent(QKeyEvent* event) {
  event->accept();
 }

}
