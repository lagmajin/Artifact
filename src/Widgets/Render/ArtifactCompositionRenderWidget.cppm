module;
#include <utility>
#define NOMINMAX
#include <windows.h>
#include <QWidget>
#include <QMenu>
#include <QCursor>
#include <QHash>
#include <QPixmap>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEnterEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QRectF>
#include <QTimer>
#include <QDebug>
#include <QLoggingCategory>
#include <QString>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionRenderWidget;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Application.Manager;
import Artifact.Widgets.CompositionEditor;
import Artifact.Layers.Selection.Manager;
import Artifact.Tool.Manager;
import Artifact.Service.ActiveContext;
import Artifact.Service.Playback;
import Event.Bus;
import Artifact.Event.Types;
import Image.ImageF32x4_RGBA;
import InputEvent;
import Input.Operator;
import Utils.Path;
import Undo.UndoManager;
import UI.ShortcutBindings;
import Time.Rational;
import Utils.Id;
import Utils.Point.Like;
import Settings.Accessibility;
import InputEvent;
import Input.Operator;

namespace Artifact {

 using namespace ArtifactCore;
 Q_LOGGING_CATEGORY(compositionWidgetLog, "artifact.compositionwidget");

 W_OBJECT_IMPL(ArtifactCompositionRenderWidget)

namespace {
QPoint accessibilityMenuPosition(const QMenu &menu, const QPoint &origin) {
  int x = origin.x();
  int y = origin.y();
  Accessibility::adjustContextMenuPosition(x, y, menu.sizeHint().width());
  return QPoint(x, y);
}

enum class LayerDragMode {
  None,
  Move,
  ScaleTL,
  ScaleTR,
  ScaleBL,
  ScaleBR
};

struct LayerHitTestResult {
  ArtifactAbstractLayerPtr layer;
  QRectF bbox;
};

LayerDragMode hitTestLayerDragMode(const QRectF& bbox,
                                   const QPointF& viewportPos,
                                   ArtifactIRenderer* renderer)
{
  if (!renderer || !bbox.isValid() || bbox.width() <= 0.0 ||
      bbox.height() <= 0.0) {
   return LayerDragMode::None;
  }

  const float kHandleHitSize = static_cast<float>(Accessibility::scaledSize(16));
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

LayerDragMode hitTestLayerDragMode(const ArtifactAbstractLayerPtr& layer,
                                   const QPointF& viewportPos,
                                   ArtifactIRenderer* renderer)
{
  if (!layer || !renderer) {
   return LayerDragMode::None;
  }
  return hitTestLayerDragMode(layer->transformedBoundingBox(), viewportPos,
                              renderer);
 }

LayerHitTestResult hitTestTopVisibleLayer(
    const ArtifactCompositionPtr& comp,
    ArtifactIRenderer* renderer, const QPointF& viewportPos)
{
  if (!comp || !renderer) {
   return {};
  }
  const auto cPos =
      renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
  const auto& layers = comp->allLayerRef();
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
   const auto& layer = layers[i];
   if (!layer || !layer->isVisible()) {
    continue;
   }
   const QRectF bbox = layer->transformedBoundingBox();
   if (bbox.contains(cPos.x, cPos.y)) {
    return {layer, bbox};
   }
  }
  return {};
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

const QCursor& hudCursor(const QString& iconName,
                         const Qt::CursorShape fallbackShape,
                         const int hotX = 12,
                         const int hotY = 12)
{
  static QHash<QString, QCursor> cache;
  const QString key = iconName + QStringLiteral("|%1|%2|%3")
                                   .arg(static_cast<int>(fallbackShape))
                                   .arg(hotX)
                                   .arg(hotY);
  auto it = cache.constFind(key);
  if (it != cache.constEnd()) {
    return it.value();
  }

  QPixmap pixmap(ArtifactCore::resolveIconPath(QStringLiteral("Studio/%1").arg(iconName)));
  if (!pixmap.isNull()) {
    pixmap = pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    it = cache.insert(key, QCursor(pixmap, hotX, hotY));
    return it.value();
  }

  it = cache.insert(key, QCursor(fallbackShape));
  return it.value();
}

const QCursor& hudCursorForLayerDragMode(LayerDragMode mode, bool dragging)
{
  switch (mode) {
  case LayerDragMode::Move:
    return hudCursor(QStringLiteral("hud_cursor_move.svg"),
                     dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
  case LayerDragMode::ScaleTL:
  case LayerDragMode::ScaleTR:
  case LayerDragMode::ScaleBL:
  case LayerDragMode::ScaleBR:
    return hudCursor(QStringLiteral("hud_cursor_scale_uniform.svg"),
                     cursorForLayerDragMode(mode, dragging));
  default:
    return hudCursor(QStringLiteral("hud_cursor_select.svg"), Qt::ArrowCursor, 2, 2);
  }
}

int compositionPreviewIntervalMs(
    const ArtifactCompositionPtr& comp)
{
  const double fps = comp ? comp->frameRate().framerate() : 0.0;
  if (fps <= 0.0) {
    return 16;
  }
  return std::max(1, static_cast<int>(std::lround(1000.0 / fps)));
}
  } // namespace

 class ArtifactCompositionRenderWidget::Impl {
 public:
  std::unique_ptr<ArtifactIRenderer> renderer_;
  ArtifactPreviewCompositionPipeline previewPipeline_;
  bool initialized_ = false;
  std::atomic_bool isPlaying_{ false };
  std::atomic_bool needsRender_{ true };
  std::atomic_uint64_t renderGeneration_{ 0 };
  std::atomic_bool running_{ false };
  std::thread renderTask_;
  std::mutex renderMutex_;
  std::condition_variable renderCv_;
  std::mutex renderCvMutex_;
  QWidget* widget_ = nullptr;
  QTimer* resizeDebounceTimer_ = nullptr;
  QTimer* wheelRenderTimer_ = nullptr;
  bool zoomAnimationActive_ = false;
  float zoomAnimationStart_ = 1.0f;
  float zoomAnimationTarget_ = 1.0f;
  QPointF zoomAnimationAnchorViewport_;
  QPointF zoomAnimationAnchorCanvas_;
  std::chrono::steady_clock::time_point zoomAnimationStartedAt_{};
  bool panMomentumActive_ = false;
  QPointF panVelocityPerMs_;
  std::chrono::steady_clock::time_point lastPanSampleAt_{};
  QSize pendingResizeSize_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  QString lastRamPreviewFallbackSummary_;
  
  QPointF lastMousePos_;
  ArtifactCore::LayerID selectedLayerId_ = ArtifactCore::LayerID::Nil();
  bool isDraggingLayer_ = false;
  bool isPanningViewport_ = false;
  bool isRotatingViewport_ = false;
  bool spaceHandActive_ = false;
  ToolType toolBeforeSpace_ = ToolType::Selection;
  bool zoomMarqueeActive_ = false;
  QPointF zoomMarqueeStart_;
  QPointF zoomMarqueeEnd_;
  float rotationDragStart_ = 0.0f;
  QPointF rotationDragStartPos_;
  float rotationSnapDegrees_ = 45.0f;
  LayerDragMode dragMode_ = LayerDragMode::None;
  QPointF dragStartCanvasPos_;
  QPointF dragStartLayerPos_;
  QRectF dragStartBoundingBox_;
  float dragStartScaleX_ = 1.0f;
  float dragStartScaleY_ = 1.0f;
  int64_t dragFrame_ = 0;
  QPointF dragAppliedDelta_;
  std::chrono::steady_clock::time_point lastDragMutationNotify_{};
  
  Impl() = default;
  ~Impl() { destroy(); }

  void startSmoothZoomTo(const QPointF& viewportAnchor,
                         const QPointF& canvasAnchor,
                         float targetZoom) {
   if (!renderer_) return;
   zoomAnimationStart_ = renderer_->getZoom();
   zoomAnimationTarget_ = std::clamp(targetZoom, 0.05f, 64.0f);
   zoomAnimationAnchorViewport_ = viewportAnchor;
   zoomAnimationAnchorCanvas_ = canvasAnchor;
   zoomAnimationStartedAt_ = std::chrono::steady_clock::now();
   zoomAnimationActive_ = true;
  }

  void startSmoothZoom(const QPointF& viewportPos, float factor) {
   if (!renderer_) return;
   const float currentZoom = renderer_->getZoom();
   const float baseZoom = zoomAnimationActive_ ? zoomAnimationTarget_ : currentZoom;
   const auto canvasPoint = renderer_->viewportToCanvas(
       {static_cast<float>(viewportPos.x()), static_cast<float>(viewportPos.y())});
   startSmoothZoomTo(viewportPos, QPointF(canvasPoint.x, canvasPoint.y),
                     baseZoom * factor);
  }

  bool stepSmoothZoom() {
   if (!renderer_ || !zoomAnimationActive_) return false;
   constexpr auto kDuration = std::chrono::milliseconds(150);
   const auto elapsed = std::chrono::steady_clock::now() - zoomAnimationStartedAt_;
   const float linear = std::clamp(
       static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
           static_cast<float>(kDuration.count()),
       0.0f, 1.0f);
   const float eased = linear * linear * (3.0f - 2.0f * linear);
   const float zoom = zoomAnimationStart_ +
                      (zoomAnimationTarget_ - zoomAnimationStart_) * eased;
   renderer_->setZoom(zoom);
   renderer_->setPan(
       static_cast<float>(zoomAnimationAnchorViewport_.x()) -
           static_cast<float>(zoomAnimationAnchorCanvas_.x()) * zoom,
       static_cast<float>(zoomAnimationAnchorViewport_.y()) -
           static_cast<float>(zoomAnimationAnchorCanvas_.y()) * zoom);
   if (linear >= 1.0f) zoomAnimationActive_ = false;
   return zoomAnimationActive_;
  }

  bool stepPanMomentum() {
   if (!renderer_ || !panMomentumActive_) return false;
   renderer_->panBy(static_cast<float>(panVelocityPerMs_.x() * 16.0),
                    static_cast<float>(panVelocityPerMs_.y() * 16.0));
   panVelocityPerMs_ *= 0.86;
   if (std::hypot(panVelocityPerMs_.x(), panVelocityPerMs_.y()) < 0.015) {
    panVelocityPerMs_ = {};
    panMomentumActive_ = false;
   }
   return panMomentumActive_;
  }

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
    renderer_->setDevicePixelRatio(static_cast<float>(widget_->devicePixelRatioF()));
    requestRender();
   });
   wheelRenderTimer_ = new QTimer(widget_);
   wheelRenderTimer_->setSingleShot(true);
   QObject::connect(wheelRenderTimer_, &QTimer::timeout, widget_, [this]() {
    const bool continueZoom = stepSmoothZoom();
    const bool continuePan = stepPanMomentum();
    requestRender();
    if ((continueZoom || continuePan) && wheelRenderTimer_) {
     wheelRenderTimer_->start(16);
    }
   });
   eventBusSubscriptions_.push_back(
       eventBus_.subscribe<PlaybackStateChangedEvent>(
           [this](const PlaybackStateChangedEvent &event) {
             isPlaying_.store(event.state == ::Artifact::PlaybackState::Playing,
                              std::memory_order_release);
             requestRender();
           }));
   eventBusSubscriptions_.push_back(
       eventBus_.subscribe<FrameChangedEvent>(
           [this](const FrameChangedEvent &) { requestRender(); }));
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
   if (renderTask_.joinable()) {
    renderTask_.join();
   }
   running_ = true;
   renderTask_ = std::thread([this]() {
     while (running_.load(std::memory_order_acquire)) {
       const int frameIntervalMs =
           compositionPreviewIntervalMs(previewPipeline_.composition());
       const bool playing = isPlaying_.load(std::memory_order_acquire);
       const bool dirty = needsRender_.exchange(false, std::memory_order_acq_rel);
       if (!playing && !dirty) {
         std::unique_lock<std::mutex> waitLock(renderCvMutex_);
         renderCv_.wait_for(waitLock, std::chrono::milliseconds(frameIntervalMs), [this]() {
          return !running_.load(std::memory_order_acquire) ||
                 isPlaying_.load(std::memory_order_acquire) ||
                 needsRender_.load(std::memory_order_acquire);
         });
         continue;
      }
       {
        std::lock_guard<std::mutex> lock(renderMutex_);
        const std::uint64_t generation =
            renderGeneration_.load(std::memory_order_acquire);
        QElapsedTimer frameTimer;
        frameTimer.start();
        renderOneFrame(generation);
        const qint64 renderElapsedMs = frameTimer.elapsed();
        if (playing) {
          const int remainingMs =
              std::max(0, frameIntervalMs - static_cast<int>(renderElapsedMs));
          if (remainingMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(remainingMs));
          }
        }
       }
     }
    });
   }

  void stopRenderLoop() {
   running_ = false;
   renderCv_.notify_all();
   if (renderTask_.joinable()) {
    renderTask_.join();
   }
   if (renderer_) renderer_->flushAndWait();
  }

  void renderOneFrame(const std::uint64_t generation) {
    if (!initialized_ || !renderer_) return;
    auto comp = previewPipeline_.composition();
    FramePosition targetFrame = comp ? comp->framePosition() : FramePosition(0);
    ArtifactCore::ImageF32x4_RGBA ramPreviewFrameImage;
    bool useRamPreviewFallback = false;
    QString ramPreviewFallbackReason = QStringLiteral("no-playback-service");
    bool playbackPlaying = false;
    bool playbackAllowsRamFallbackWhilePlaying = false;
    if (auto* playback = ArtifactPlaybackService::instance()) {
     const auto playbackComp = playback->currentComposition();
     if (!playbackComp || (comp && playbackComp->id() == comp->id())) {
      targetFrame = playback->currentFrame();
      playbackPlaying = playback->isPlaying();
      const auto previewState = playback->ramPreviewFrameState(targetFrame.framePosition());
      playbackAllowsRamFallbackWhilePlaying =
          playback->ramPreviewPlaybackFallbackWhilePlaying();
      if (!previewState.playable) {
       ramPreviewFallbackReason = ramPreviewNotReadyReason(previewState);
      } else if (!playback->tryGetRamPreviewFrameImage(
                         targetFrame.framePosition(), ramPreviewFrameImage)) {
       ramPreviewFallbackReason = QStringLiteral("ready-missing-image");
      } else if (playbackPlaying && !playbackAllowsRamFallbackWhilePlaying) {
       ramPreviewFallbackReason = QStringLiteral("playing-policy-disabled");
      } else {
       useRamPreviewFallback = true;
       ramPreviewFallbackReason =
           playbackPlaying ? QStringLiteral("ready-playing")
                           : QStringLiteral("ready");
      }
     } else {
      ramPreviewFallbackReason = QStringLiteral("composition-mismatch");
     }
    }
    const QString ramPreviewFallbackSummary =
        QStringLiteral("ramPreviewFallback=%1 reason=%2 playing=%3 allowPlaying=%4")
            .arg(useRamPreviewFallback ? 1 : 0)
            .arg(ramPreviewFallbackReason)
            .arg(playbackPlaying ? 1 : 0)
            .arg(playbackAllowsRamFallbackWhilePlaying ? 1 : 0);
    if (ramPreviewFallbackSummary != lastRamPreviewFallbackSummary_) {
     lastRamPreviewFallbackSummary_ = ramPreviewFallbackSummary;
     qCDebug(compositionWidgetLog) << "[CompositionRenderWidget][RamPreviewFallback]"
                                   << ramPreviewFallbackSummary;
    }
    if (comp) {
     auto size = comp->settings().compositionSize();
     renderer_->setCanvasSize((float)size.width(), (float)size.height());
    previewPipeline_.setCurrentFrame(targetFrame.framePosition());
   }
   if (useRamPreviewFallback && comp) {
    QMatrix4x4 identity;
    renderer_->clear();
    renderer_->drawSpriteTransformed(
        0.0f, 0.0f, (float)comp->settings().compositionSize().width(),
        (float)comp->settings().compositionSize().height(), identity,
        ramPreviewFrameImage, 1.0f);
    if (generation == renderGeneration_.load(std::memory_order_acquire)) {
     renderer_->present();
    }
    return;
   }
   previewPipeline_.render(renderer_.get());
   // Property edits can arrive while CPU preparation/effect evaluation is
   // running on this worker. Never publish an older frame after a newer edit;
   // the pending request will render the latest generation next.
   if (generation == renderGeneration_.load(std::memory_order_acquire)) {
    renderer_->present();
   }
  }

  void requestRender() {
    renderGeneration_.fetch_add(1, std::memory_order_acq_rel);
    needsRender_.store(true, std::memory_order_release);
    renderCv_.notify_one();
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

  bool shouldPublishDragMutation()
  {
   constexpr auto kDragMutationNotifyInterval = std::chrono::milliseconds(16);
   const auto now = std::chrono::steady_clock::now();
   if (lastDragMutationNotify_.time_since_epoch().count() == 0 ||
       now - lastDragMutationNotify_ >= kDragMutationNotifyInterval) {
    lastDragMutationNotify_ = now;
    return true;
   }
   return false;
  }

  void updateHoverCursor(const QPointF& viewportPos)
  {
   if (isPanningViewport_) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                                 Qt::ClosedHandCursor));
    return;
   }

   if (isDraggingLayer_) {
    widget_->setCursor(hudCursorForLayerDragMode(dragMode_, true));
    return;
   }

   if (!renderer_) {
    widget_->setCursor(Qt::ArrowCursor);
    return;
   }

   auto* toolManager = ArtifactApplicationManager::instance()->toolManager();
   if (toolManager && toolManager->activeTool() == ToolType::Zoom) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_zoom.svg"),
                                 Qt::CrossCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::Rotation) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_rotate.svg"),
                                 Qt::CrossCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::AnchorPoint) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_anchor.svg"),
                                 Qt::CrossCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::Move) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_move.svg"),
                                 Qt::SizeAllCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::Scale) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_scale_uniform.svg"),
                                 Qt::SizeFDiagCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::RigSelect) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_select.svg"),
                                 Qt::CrossCursor));
    return;
   }
   if (toolManager && toolManager->activeTool() == ToolType::RigWeight) {
    widget_->setCursor(hudCursor(QStringLiteral("hud_cursor_select.svg"),
                                 Qt::CrossCursor));
    return;
   }

   std::lock_guard<std::mutex> lock(renderMutex_);
  auto comp = previewPipeline_.composition();
  if (!comp) {
   auto* tm = ArtifactApplicationManager::instance()->toolManager();
   widget_->setCursor(tm && tm->activeTool() == ToolType::Hand
                          ? hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                                      Qt::OpenHandCursor)
                          : hudCursor(QStringLiteral("hud_cursor_select.svg"),
                                      Qt::ArrowCursor, 2, 2));
   return;
  }

  const auto hit = hitTestTopVisibleLayer(comp, renderer_.get(), viewportPos);
  if (hit.layer) {
   widget_->setCursor(hudCursorForLayerDragMode(
       hitTestLayerDragMode(hit.bbox, viewportPos, renderer_.get()), false));
   return;
  }

   auto* tm = ArtifactApplicationManager::instance()->toolManager();
   widget_->setCursor(tm && tm->activeTool() == ToolType::Hand
                          ? hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                                      Qt::OpenHandCursor)
                          : hudCursor(QStringLiteral("hud_cursor_select.svg"),
                                      Qt::ArrowCursor, 2, 2));
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
  setAccessibleName(QStringLiteral("Composition View"));
  setAccessibleDescription(QStringLiteral("Preview and interact with the active composition"));
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
   const float fitMargin = 0.05f * static_cast<float>(std::min(width(), height()));
   impl_->renderer_->fitToViewport(fitMargin);
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
   impl_->zoomAnimationActive_ = false;
   impl_->renderer_->resetView();
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomIn() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->startSmoothZoom(QPointF(width() / 2.0, height() / 2.0), 1.1f);
   if (impl_->wheelRenderTimer_) {
    impl_->stepSmoothZoom();
    impl_->wheelRenderTimer_->start(16);
   }
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomOut() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->startSmoothZoom(QPointF(width() / 2.0, height() / 2.0), 0.909f);
   if (impl_->wheelRenderTimer_) {
    impl_->stepSmoothZoom();
    impl_->wheelRenderTimer_->start(16);
   }
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoomFit() {
 if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->zoomAnimationActive_ = false;
   const float fitMargin = 0.05f * static_cast<float>(std::min(width(), height()));
   impl_->renderer_->fitToViewport(fitMargin);
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::zoom100() {
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->zoomAnimationActive_ = false;
   impl_->renderer_->setZoom(1.0f);
   // Center the canvas in the viewport at 100% zoom
   if (auto comp = impl_->previewPipeline_.composition()) {
    auto size = comp->settings().compositionSize();
    const float cw  = static_cast<float>(size.width());
    const float ch  = static_cast<float>(size.height());
    const float dpr = static_cast<float>(devicePixelRatioF());
    const float panX = (width()  - cw) * 0.5f;
    const float panY = (height() - ch) * 0.5f;
    impl_->renderer_->setDevicePixelRatio(dpr);
    impl_->renderer_->setPan(panX, panY);
   }
   impl_->requestRender();
  }
 }

 void ArtifactCompositionRenderWidget::rotateCanvas(float degrees) {
  if (!impl_->renderer_) return;
  std::lock_guard<std::mutex> lock(impl_->renderMutex_);
  impl_->renderer_->setRotation(degrees);
  impl_->requestRender();
 }

 void ArtifactCompositionRenderWidget::setRotationSnapDegrees(float degrees) {
  // Keep the public setting predictable while allowing the documented
  // 15/30/45/90 degree presets and custom positive values from tooling.
  if (!std::isfinite(degrees) || degrees <= 0.0f) return;
  impl_->rotationSnapDegrees_ = std::max(1.0f, std::min(360.0f, degrees));
 }

 float ArtifactCompositionRenderWidget::rotationSnapDegrees() const {
  return impl_->rotationSnapDegrees_;
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
    impl_->renderer_->setDevicePixelRatio((float)devicePixelRatioF());

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
            [this](const LayerSelectionChangedEvent& event) {
              impl_->selectedLayerId_ = LayerID(event.layerId);
              impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);
              impl_->requestRender();
            }));

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ToolChangedEvent>(
            [this](const ToolChangedEvent &event) {
              if (event.toolType == ToolType::Hand) {
                setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                                    Qt::OpenHandCursor));
              } else {
                unsetCursor();
              }
              impl_->requestRender();
            }));

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
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    input->setActiveContext(QStringLiteral("Viewport.Composition"));
  }
 }

 void ArtifactCompositionRenderWidget::focusOutEvent(QFocusEvent* event) {
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    if (input->activeContext() == QStringLiteral("Viewport.Composition")) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
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
  if (event && event->button() == Qt::LeftButton) {
   if (auto* editor = qobject_cast<ArtifactCompositionEditor *>(parentWidget())) {
    auto* toolManager = ArtifactApplicationManager::instance()
                            ? ArtifactApplicationManager::instance()->toolManager()
                            : nullptr;
    if ((event->modifiers() & Qt::ControlModifier) != 0 && toolManager &&
        toolManager->activeTool() == ToolType::AnchorPoint) {
     if (auto* controller = editor->renderController();
         controller && controller->resetSelected2DAnchorToCenter()) {
      event->accept();
      return;
     }
    }
    if (toolManager && toolManager->activeTool() == ToolType::Pen) {
      if (auto* controller = editor->renderController();
          controller && (controller->resetHoveredMaskTangent() ||
                         controller->resetHoveredMaskVertexTangents())) {
       event->accept();
       return;
      }
    }
    if (auto* controller = editor->renderController();
        controller && controller->editTextAtViewport(event->position())) {
     event->accept();
     return;
    }
    if (auto* controller = editor->renderController();
        controller && controller->resetSelected3DTransform()) {
     event->accept();
     return;
    }
   }
  }
  // Double click to reset view is disabled to prevent accidental resets
  // during normal workflow. Use the toolbar button or View > Reset View instead.
  event->accept();
 }

 void ArtifactCompositionRenderWidget::wheelEvent(QWheelEvent* event) {
  const QPoint angleDelta = event->angleDelta();
  const QPoint pixelDelta = event->pixelDelta();
  // Trackpads often provide only pixelDelta(). Keep the old 120-unit mouse
  // wheel convention, but normalize pixel input to a comparable step size.
  const float verticalDelta = angleDelta.y() != 0
                                  ? static_cast<float>(angleDelta.y()) / 120.0f
                                  : static_cast<float>(pixelDelta.y()) / 48.0f;
  if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   impl_->panMomentumActive_ = false;
   if (event->modifiers() & Qt::ShiftModifier) {
    // Prefer a genuine horizontal wheel delta.  A vertical-only wheel is
    // retained as a compatibility fallback for mice without tilt support.
    const int rawHorizontalDelta = angleDelta.x() != 0 ? angleDelta.x()
                                                        : pixelDelta.x();
    const int rawVerticalDelta = angleDelta.y() != 0 ? angleDelta.y()
                                                      : pixelDelta.y();
    const float horizontalDelta = rawHorizontalDelta != 0
                                      ? (angleDelta.x() != 0
                                             ? static_cast<float>(rawHorizontalDelta)
                                             : static_cast<float>(rawHorizontalDelta) * 2.5f)
                                      : (angleDelta.y() != 0
                                             ? static_cast<float>(rawVerticalDelta)
                                             : static_cast<float>(rawVerticalDelta) * 2.5f);
    impl_->renderer_->panBy(horizontalDelta, 0.0f);
    impl_->requestRender();
    event->accept();
    return;
   }
   if (std::abs(verticalDelta) < 0.001f) {
    event->accept();
    return;
   }
   const float zoomFactor = std::pow(1.1f, verticalDelta);
   QPointF pos = event->position();
   impl_->startSmoothZoom(pos, zoomFactor);
   if (impl_->wheelRenderTimer_) {
    impl_->stepSmoothZoom();
    impl_->wheelRenderTimer_->start(16);
   } else {
    impl_->requestRender();
   }
  }
  event->accept();
 }

 void ArtifactCompositionRenderWidget::mousePressEvent(QMouseEvent* event) {
  qCDebug(compositionWidgetLog) << "[MousePress] ENTER pos:" << event->position()
                                << "button:" << event->button()
                                << "modifiers:" << event->modifiers();

  auto* tm = ArtifactApplicationManager::instance()->toolManager();
  if (event->button() == Qt::LeftButton && tm &&
      tm->activeTool() == ToolType::Zoom && impl_->renderer_) {
   impl_->zoomMarqueeStart_ = event->position();
   impl_->zoomMarqueeEnd_ = event->position();
   impl_->zoomMarqueeActive_ = false;
   grabMouse();
   event->accept();
   return;
  }
  if (event->button() == Qt::LeftButton &&
      (event->modifiers() & Qt::ShiftModifier)) {
   impl_->lastMousePos_ = event->position();
   impl_->rotationDragStart_ = impl_->renderer_ ? impl_->renderer_->getRotation() : 0.0f;
   impl_->rotationDragStartPos_ = event->position();
   impl_->isRotatingViewport_ = true;
   grabMouse();
   event->accept();
   return;
  }
  const bool isHandShortcut = impl_->spaceHandActive_;

  if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && (tm->activeTool() == ToolType::Hand || isHandShortcut))) {
   impl_->panMomentumActive_ = false;
   impl_->panVelocityPerMs_ = {};
   impl_->lastPanSampleAt_ = std::chrono::steady_clock::now();
   setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                       Qt::ClosedHandCursor));
   impl_->lastMousePos_ = event->position();
   impl_->isPanningViewport_ = true;
   grabMouse();
   impl_->requestRender();
   event->accept();
  } else if (event->button() == Qt::RightButton) {
      // Context Menu
      if (impl_->renderer_) {
          std::lock_guard<std::mutex> lock(impl_->renderMutex_);
          auto comp = impl_->previewPipeline_.composition();
          if (comp) {
              const auto hit =
                  hitTestTopVisibleLayer(comp, impl_->renderer_.get(),
                                         event->position());
              if (hit.layer) {
                  ArtifactApplicationManager::instance()->layerSelectionManager()->selectLayer(hit.layer);
                  
                  QMenu menu(this);
                  menu.addAction("Center in Comp", [layer = hit.layer, comp]() {
                      auto size = comp->settings().compositionSize();
                      auto& t3 = layer->transform3D();
                      t3.setPosition(ArtifactCore::RationalTime(comp->framePosition().framePosition(), 30000), size.width() / 2.0f, size.height() / 2.0f);
                      layer->changed();
                  });
                  menu.addSeparator();
                  menu.addAction("Bring to Front", [layer = hit.layer, comp]() { comp->bringToFront(layer->id()); });
                  menu.addAction("Send to Back", [layer = hit.layer, comp]() { comp->sendToBack(layer->id()); });
                  menu.exec(accessibilityMenuPosition(
                      menu, event->globalPosition().toPoint()));
              }
      }
      }
      event->accept();
      impl_->updateHoverCursor(event->position());
      impl_->requestRender();
  } else if (event->button() == Qt::LeftButton) {
   if (impl_->renderer_) {
    std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    auto comp = impl_->previewPipeline_.composition();
    if (comp) {
     const auto hit =
         hitTestTopVisibleLayer(comp, impl_->renderer_.get(), event->position());
     
     if (hit.layer) {
      if (event->modifiers() & Qt::ShiftModifier) {
          ArtifactApplicationManager::instance()->layerSelectionManager()->addToSelection(hit.layer);
      } else {
          ArtifactApplicationManager::instance()->layerSelectionManager()->selectLayer(hit.layer);
      }
      impl_->selectedLayerId_ = hit.layer->id();
      impl_->previewPipeline_.setSelectedLayerId(impl_->selectedLayerId_);

      if (tm->activeTool() != ToolType::Selection &&
          tm->activeTool() != ToolType::Move &&
          tm->activeTool() != ToolType::Scale) {
          impl_->isDraggingLayer_ = false;
          impl_->dragMode_ = LayerDragMode::None;
          impl_->updateHoverCursor(event->position());
          event->accept();
          return;
      }

      const auto cPos = impl_->renderer_->viewportToCanvas(
          {(float)event->position().x(), (float)event->position().y()});
      impl_->dragStartCanvasPos_ = QPointF(cPos.x, cPos.y);
      impl_->dragStartLayerPos_ = QPointF(hit.layer->transform3D().positionX(),
                                          hit.layer->transform3D().positionY());
      impl_->dragStartScaleX_ = hit.layer->transform3D().scaleX();
      impl_->dragStartScaleY_ = hit.layer->transform3D().scaleY();
      impl_->dragStartBoundingBox_ = hit.bbox;
      impl_->dragFrame_ = comp->framePosition().framePosition();
      impl_->dragAppliedDelta_ = QPointF(0.0, 0.0);
      impl_->lastDragMutationNotify_ = {};
      impl_->dragMode_ =
          hitTestLayerDragMode(hit.bbox, event->position(), impl_->renderer_.get());
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
  qCDebug(compositionWidgetLog) << "[MouseRelease] ENTER pos:" << event->position()
                                << "button:" << event->button();

  if (event->button() == Qt::LeftButton &&
      ArtifactApplicationManager::instance()->toolManager()->activeTool() == ToolType::Zoom &&
      impl_->renderer_) {
   const QRectF marquee(impl_->zoomMarqueeStart_, impl_->zoomMarqueeEnd_);
   const QRectF normalized = marquee.normalized();
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
   if (impl_->zoomMarqueeActive_ && normalized.width() >= 4.0 &&
       normalized.height() >= 4.0) {
    const auto topLeft = impl_->renderer_->viewportToCanvas(
        {(float)normalized.left(), (float)normalized.top()});
    const auto bottomRight = impl_->renderer_->viewportToCanvas(
        {(float)normalized.right(), (float)normalized.bottom()});
    const float canvasWidth = std::abs(bottomRight.x - topLeft.x);
    const float canvasHeight = std::abs(bottomRight.y - topLeft.y);
    if (canvasWidth > 0.001f && canvasHeight > 0.001f) {
     const float zoomX = static_cast<float>(width()) / canvasWidth;
     const float zoomY = static_cast<float>(height()) / canvasHeight;
     const float zoom = std::min(zoomX, zoomY) * 0.95f;
     const float centerX = (topLeft.x + bottomRight.x) * 0.5f;
     const float centerY = (topLeft.y + bottomRight.y) * 0.5f;
     impl_->startSmoothZoomTo(
         QPointF(width() * 0.5, height() * 0.5),
         QPointF(centerX, centerY), zoom);
     if (impl_->wheelRenderTimer_) {
      impl_->stepSmoothZoom();
      impl_->wheelRenderTimer_->start(16);
     }
    }
   } else {
    const float zoomFactor = (event->modifiers() & Qt::AltModifier) ? 0.909f : 1.1f;
    impl_->startSmoothZoom(event->position(), zoomFactor);
    if (impl_->wheelRenderTimer_) {
     impl_->stepSmoothZoom();
     impl_->wheelRenderTimer_->start(16);
    }
   }
   impl_->zoomMarqueeActive_ = false;
   releaseMouse();
   impl_->requestRender();
   setCursor(hudCursor(QStringLiteral("hud_cursor_zoom.svg"), Qt::CrossCursor));
   event->accept();
   return;
  }

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
  if (tm->activeTool() == ToolType::Hand) {
   setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                       Qt::OpenHandCursor));
  } else {
   impl_->updateHoverCursor(event->position());
  }
  impl_->isDraggingLayer_ = false;
  const bool wasRotatingViewport = impl_->isRotatingViewport_;
  const bool wasPanningViewport = impl_->isPanningViewport_;
  impl_->isRotatingViewport_ = false;
  impl_->isPanningViewport_ = false;
  if (wasPanningViewport &&
      std::hypot(impl_->panVelocityPerMs_.x(),
                 impl_->panVelocityPerMs_.y()) > 0.05) {
   impl_->panMomentumActive_ = true;
   if (impl_->wheelRenderTimer_) impl_->wheelRenderTimer_->start(16);
  }
  if (wasRotatingViewport) {
   if (auto* editor = qobject_cast<ArtifactCompositionEditor*>(parentWidget())) {
    if (auto* controller = editor->renderController()) {
     controller->clearInfoOverlayText();
    }
   }
  }
  releaseMouse();
  impl_->dragMode_ = LayerDragMode::None;
  impl_->requestRender();
  event->accept();
 }

 void ArtifactCompositionRenderWidget::mouseMoveEvent(QMouseEvent* event) {
  qCDebug(compositionWidgetLog) << "[MouseMove] ENTER pos:" << event->position()
                                << "buttons:" << event->buttons();
  auto* tm = ArtifactApplicationManager::instance()->toolManager();

  if ((event->buttons() & Qt::LeftButton) &&
      ArtifactApplicationManager::instance()->toolManager()->activeTool() == ToolType::Zoom &&
      impl_->renderer_) {
   impl_->zoomMarqueeEnd_ = event->position();
   if ((impl_->zoomMarqueeEnd_ - impl_->zoomMarqueeStart_).manhattanLength() >= 4) {
    impl_->zoomMarqueeActive_ = true;
   }
   event->accept();
  } else if (impl_->isRotatingViewport_) {
   if (impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->renderMutex_);
    const QPointF viewportCenter(width() * 0.5, height() * 0.5);
    const QPointF startVector = impl_->rotationDragStartPos_ - viewportCenter;
    const QPointF currentVector = event->position() - viewportCenter;
    const float startLength = std::hypot(static_cast<float>(startVector.x()),
                                         static_cast<float>(startVector.y()));
    const float currentLength = std::hypot(static_cast<float>(currentVector.x()),
                                           static_cast<float>(currentVector.y()));
    float rotation = impl_->rotationDragStart_;
    if (startLength > 2.0f && currentLength > 2.0f) {
     constexpr float kRadiansToDegrees = 57.29577951308232f;
     const float startAngle = std::atan2(static_cast<float>(startVector.y()),
                                         static_cast<float>(startVector.x()));
     const float currentAngle = std::atan2(static_cast<float>(currentVector.y()),
                                           static_cast<float>(currentVector.x()));
     float deltaAngle = (currentAngle - startAngle) * kRadiansToDegrees;
     while (deltaAngle > 180.0f) deltaAngle -= 360.0f;
     while (deltaAngle < -180.0f) deltaAngle += 360.0f;
     rotation += deltaAngle;
    } else {
     // Near the pivot the angle is undefined; preserve the old horizontal
     // fallback so a drag beginning at the center remains usable.
     rotation += static_cast<float>(event->position().x() -
                                    impl_->rotationDragStartPos_.x());
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
     float snap = std::max(1.0f, impl_->rotationSnapDegrees_);
     // Modifier presets keep the common 15°/90° variants available without
     // forcing a trip to a settings panel.  The widget API remains the source
     // of the normal/default increment.
     if (event->modifiers().testFlag(Qt::AltModifier)) {
      snap = 15.0f;
     } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
      snap = 90.0f;
     }
     rotation = std::round(rotation / snap) * snap;
    }
    impl_->renderer_->setRotation(rotation);
    if (auto* editor = qobject_cast<ArtifactCompositionEditor*>(parentWidget())) {
     if (auto* controller = editor->renderController()) {
      controller->setInfoOverlayText(
          QStringLiteral("Rotation"),
          QStringLiteral("%1°").arg(rotation, 0, 'f', 1));
     }
    }
    impl_->requestRender();
   }
   event->accept();
  } else if (impl_->isPanningViewport_) {
   QPointF delta = event->position() - impl_->lastMousePos_;
   impl_->lastMousePos_ = event->position();
   const auto now = std::chrono::steady_clock::now();
   const auto elapsedMs = std::max<int64_t>(
       1, std::chrono::duration_cast<std::chrono::milliseconds>(
              now - impl_->lastPanSampleAt_).count());
   impl_->panVelocityPerMs_ = delta / static_cast<double>(elapsedMs);
   impl_->panVelocityPerMs_.setX(
       std::clamp(impl_->panVelocityPerMs_.x(), -3.0, 3.0));
   impl_->panVelocityPerMs_.setY(
       std::clamp(impl_->panVelocityPerMs_.y(), -3.0, 3.0));
   impl_->lastPanSampleAt_ = now;
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
                if (auto* editor = qobject_cast<ArtifactCompositionEditor*>(parentWidget())) {
                    if (auto* controller = editor->renderController()) {
                        const QPointF snapped = controller->snapCanvasToGrid(
                            impl_->dragStartCanvasPos_ + totalDelta);
                        totalDelta = snapped - impl_->dragStartCanvasPos_;
                    }
                }
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
       if (impl_->shouldPublishDragMutation()) {
        layer->changed();
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
       }
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
        float scaleFactorX = static_cast<float>(safeW / safeStartW);
        float scaleFactorY = static_cast<float>(safeH / safeStartH);
        if (tm->activeTool() == ToolType::Scale &&
            !(event->modifiers() & Qt::ShiftModifier)) {
         const float uniformFactor =
             std::max(std::abs(scaleFactorX), std::abs(scaleFactorY));
         scaleFactorX = uniformFactor;
         scaleFactorY = uniformFactor;
        }
        t3.setScale(t0, impl_->dragStartScaleX_ * scaleFactorX,
                    impl_->dragStartScaleY_ * scaleFactorY);
        layer->setDirty(LayerDirtyFlag::Transform);
        if (impl_->shouldPublishDragMutation()) {
         layer->changed();
         ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
             LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                               LayerChangedEvent::ChangeType::Modified});
        }
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
  auto* editor = qobject_cast<ArtifactCompositionEditor*>(parentWidget());
  auto& shortcuts = ShortcutBindings::instance();
  auto* renderController = editor ? editor->renderController() : nullptr;
  const auto activeTool = tm ? tm->activeTool() : ToolType::Selection;
  if (event && !event->isAutoRepeat() && renderController &&
      activeTool == ToolType::Pen) {
    if (event->key() == Qt::Key_Escape) {
      renderController->cancelMaskInteraction();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
      const bool deleted = renderController->deleteSelectedMaskVertices() ||
                           renderController->deleteHoveredMaskVertex();
      if (deleted) {
        event->accept();
        return;
      }
    }
    if (event->key() == Qt::Key_A &&
        event->modifiers().testFlag(Qt::ControlModifier)) {
      renderController->selectAllMaskVertices();
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_D &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        renderController->duplicateHoveredMask()) {
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_C &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        renderController->copyHoveredMask()) {
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_V &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        renderController->pasteMask()) {
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Up &&
        renderController->moveHoveredMask(-1)) {
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_Down &&
        renderController->moveHoveredMask(1)) {
      event->accept();
      return;
    }
  }
  if (event && !event->isAutoRepeat() &&
      shortcuts.matches(event, ShortcutId::Undo)) {
    if ((activeTool == ToolType::Brush || activeTool == ToolType::RotoBrush ||
         activeTool == ToolType::Eraser) &&
        editor && editor->renderController() &&
        editor->renderController()->undoSelectedPaintStroke()) {
      event->accept();
      return;
    }
  }
  if (auto* input = ArtifactCore::InputOperator::instance()) {
    input->setActiveContext(QStringLiteral("Viewport.Composition"));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }
  
  if (!event->isAutoRepeat()) {
      if (event->key() == Qt::Key_Space && !impl_->spaceHandActive_) {
          impl_->toolBeforeSpace_ = tm->activeTool();
          impl_->spaceHandActive_ = true;
          setCursor(hudCursor(QStringLiteral("hud_cursor_pan.svg"),
                              Qt::OpenHandCursor));
          event->accept();
          return;
      }
      if (shortcuts.matches(event, ShortcutId::Undo)) {
          const auto activeTool = tm ? tm->activeTool() : ToolType::Selection;
          if ((activeTool == ToolType::Brush ||
               activeTool == ToolType::RotoBrush ||
               activeTool == ToolType::Eraser) &&
              editor && editor->renderController() &&
              editor->renderController()->undoSelectedPaintStroke()) {
              event->accept();
              return;
          }
          if (auto* undo = UndoManager::instance()) {
              undo->undo();
          }
          event->accept();
          return;
      }
      if (shortcuts.matches(event, ShortcutId::Redo)) {
          if (auto* undo = UndoManager::instance()) {
              undo->redo();
          }
          event->accept();
          return;
      }
      if (shortcuts.matches(event, ShortcutId::SelectionTool)) tm->setActiveTool(ToolType::Selection);
      else if (shortcuts.matches(event, ShortcutId::HandTool)) tm->setActiveTool(ToolType::Hand);
      else if (shortcuts.matches(event, ShortcutId::ZoomTool)) tm->setActiveTool(ToolType::Zoom);
      else if (shortcuts.matches(event, ShortcutId::RotateTool)) tm->setActiveTool(ToolType::Rotation);
      else if (shortcuts.matches(event, ShortcutId::AnchorPointTool)) tm->setActiveTool(ToolType::AnchorPoint);
      else if (shortcuts.matches(event, ShortcutId::PlaybackToggle)) {
          auto* actions = ArtifactCore::ActionManager::instance();
          if (actions && actions->getAction(QStringLiteral("playback.play_pause"))) {
              actions->executeAction(QStringLiteral("playback.play_pause"));
          } else {
              ctx->togglePlayPause();
          }
      }
      else if (event->key() == Qt::Key_Home) ctx->goToStart();
      else if (event->key() == Qt::Key_End) ctx->goToEnd();
      else if (event->key() == Qt::Key_PageUp) ctx->prevFrame();
      else if (event->key() == Qt::Key_PageDown) ctx->nextFrame();
      else if (event->key() == Qt::Key_BracketLeft) ctx->setLayerInAtCurrentTime();
      else if (event->key() == Qt::Key_BracketRight) ctx->setLayerOutAtCurrentTime();
      else if (event->key() == Qt::Key_F) {
          if (event->modifiers() & Qt::ShiftModifier) {
              if (auto* ctrl = editor ? editor->renderController() : nullptr) {
                  ctrl->focusSelectedLayer();
              }
          } else {
              zoomFit();
          }
      }
      else if (event->key() == Qt::Key_R && event->modifiers() == Qt::NoModifier) {
          rotateCanvas(0.0f);
          event->accept();
          return;
      }
      else if (event->key() == Qt::Key_1 && (event->modifiers() & Qt::ControlModifier)) {
          zoom100();
      }
      else if (event->key() == Qt::Key_G) {
          if (auto* ctrl = editor ? editor->renderController() : nullptr) {
              ctrl->setShowGrid(!ctrl->isShowGrid());
          }
      }
      else if (event->key() == Qt::Key_Apostrophe) {
          if (auto* ctrl = editor ? editor->renderController() : nullptr) {
              ctrl->setShowGuides(!ctrl->isShowGuides());
          }
      }
      else if (event->key() == Qt::Key_Semicolon) {
          if (auto* ctrl = editor ? editor->renderController() : nullptr) {
              ctrl->setShowSafeMargins(!ctrl->isShowSafeMargins());
          }
      }
      
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
  if (!event->isAutoRepeat() && event->key() == Qt::Key_Space &&
      impl_->spaceHandActive_) {
   impl_->spaceHandActive_ = false;
   auto* tm = ArtifactApplicationManager::instance()->toolManager();
   if (tm && tm->activeTool() == ToolType::Hand) {
    tm->setActiveTool(impl_->toolBeforeSpace_);
   }
   if (!impl_->isPanningViewport_) {
    impl_->updateHoverCursor(mapFromGlobal(QCursor::pos()));
   }
   event->accept();
   return;
  }
  event->accept();
}

}
