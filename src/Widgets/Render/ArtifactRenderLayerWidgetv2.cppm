module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QKeyEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <limits>

module Artifact.Widgets.RenderLayerWidgetv2;
import Graphics;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Artifact.Application.Manager;
import Artifact.Service.Application;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Property.Abstract;

import Artifact.Render.IRenderer;
import Artifact.Render.CompositionRenderer;
import Artifact.Preview.Pipeline;
import Artifact.Layer.Image;

namespace Artifact {

 using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactLayerEditorWidgetV2)

 class ArtifactLayerEditorWidgetV2::Impl {
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
  QWidget* widget_;
  //bool isPanning_ = false;
  bool isPlay_ = false;
  std::atomic_bool running_{ false };
  QTimer* renderTimer_ = nullptr;
  std::mutex resizeMutex_;
  
  
 bool released = true;
 bool m_initialized;
 RefCntAutoPtr<ITexture> m_layerRT;
 RefCntAutoPtr<IFence> m_layer_fence;
  LayerID targetLayerId_{};
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  
  void startRenderLoop();
  void stopRenderLoop();
  void renderOneFrame();
 };

 ArtifactLayerEditorWidgetV2::Impl::Impl()
 {

 }

 ArtifactLayerEditorWidgetV2::Impl::~Impl()
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::initialize(QWidget* window)
 {
  widget_ = window;
  renderer_ = std::make_unique<ArtifactIRenderer>();
  renderer_->initialize(window);

  if (!renderer_ || !renderer_->isInitialized()) {
   qWarning() << "[ArtifactLayerEditorWidgetV2] renderer initialize failed for"
              << window << "size=" << (window ? window->size() : QSize())
              << "DPR=" << (window ? window->devicePixelRatio() : 0.0);
   compositionRenderer_.reset();
   renderer_.reset();
   return;
  }

  compositionRenderer_ = std::make_unique<CompositionRenderer>(*renderer_);
  initialized_ = true;
 }

 void ArtifactLayerEditorWidgetV2::Impl::initializeSwapChain(QWidget* window)
 {
  if (!renderer_) {
   return;
  }
  renderer_->recreateSwapChain(window);
 }

 void ArtifactLayerEditorWidgetV2::Impl::destroy()
 {
  stopRenderLoop();
  if (renderer_) {
   renderer_->destroy();
   renderer_.reset();
  }
  compositionRenderer_.reset();
  initialized_ = false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
  if (!event || !renderer_ || !widget_) {
   return;
  }

  const QPointF center(widget_->width() * 0.5, widget_->height() * 0.5);
  switch (event->key()) {
  case Qt::Key_F:
   renderer_->fitToViewport();
   zoomLevel_ = 1.0f;
   event->accept();
   return;
  case Qt::Key_R:
   renderer_->resetView();
   zoomLevel_ = 1.0f;
   event->accept();
   return;
  case Qt::Key_1:
   zoomLevel_ = 1.0f;
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   event->accept();
   return;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
   zoomLevel_ = std::clamp(zoomLevel_ * 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   event->accept();
   return;
  case Qt::Key_Minus:
  case Qt::Key_Underscore:
   zoomLevel_ = std::clamp(zoomLevel_ / 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   event->accept();
   return;
  case Qt::Key_Left:
   renderer_->panBy(24.0f, 0.0f);
   event->accept();
   return;
  case Qt::Key_Right:
   renderer_->panBy(-24.0f, 0.0f);
   event->accept();
   return;
  case Qt::Key_Up:
   renderer_->panBy(0.0f, 24.0f);
   event->accept();
   return;
  case Qt::Key_Down:
   renderer_->panBy(0.0f, -24.0f);
   event->accept();
   return;
  default:
   break;
  }

 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyReleaseEvent(QKeyEvent* event)
 {
  Q_UNUSED(event);
 }

 bool ArtifactLayerEditorWidgetV2::Impl::isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
   if (group.name().compare(QStringLiteral("Solid"), Qt::CaseInsensitive) == 0) {
    return true;
   }
  }
  return false;
 }

 bool ArtifactLayerEditorWidgetV2::Impl::tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor)
 {
  if (!layer) {
   return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  for (const auto& group : groups) {
   if (group.name().compare(QStringLiteral("Solid"), Qt::CaseInsensitive) != 0) {
    continue;
   }
   for (const auto& property : group.allProperties()) {
    if (!property) {
     continue;
    }
    if (property->getType() != ArtifactCore::PropertyType::Color) {
     continue;
    }
    const QColor color = property->getColorValue();
    if (!color.isValid()) {
     continue;
    }
    outColor = FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    return true;
   }
  }
  return false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChainInternal(QWidget* window)
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::startRenderLoop()
 {
  if (running_)
   return;
  running_ = true;
  if (renderTimer_ && !renderTimer_->isActive()) {
   renderTimer_->start();
  }
 }

 void ArtifactLayerEditorWidgetV2::Impl::stopRenderLoop()
 {
  running_ = false;        // ループを抜ける
  if (renderTimer_) {
   renderTimer_->stop();
  }

  if (renderer_) {
   renderer_->flushAndWait();
  }
 }

 void ArtifactLayerEditorWidgetV2::Impl::renderOneFrame()
 {
 if (!initialized_ || !renderer_)
  return;
 renderer_->clear();
  if (compositionRenderer_) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     const auto compSize = composition->settings().compositionSize();
     compositionRenderer_->SetCompositionSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
    }
   }
   compositionRenderer_->ApplyCompositionSpace();
   compositionRenderer_->DrawCompositionBackground(clearColor_);
  } else {
   renderer_->drawRectLocal(-8192, -8192, 16384, 16384, clearColor_);
  }
  if (!targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     // コンポジションサイズを設定
     const auto compSize = composition->settings().compositionSize();
     if (compSize.width() > 0 && compSize.height() > 0) {
      renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
     }

     if (auto layer = composition->layerById(targetLayerId_)) {
      const auto currentFrame = ArtifactPlaybackService::instance()
          ? ArtifactPlaybackService::instance()->currentFrame()
          : composition->framePosition();
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
      }
     }
    }
   }
  }
  renderer_->flush();
  renderer_->present();
}

void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChain(QWidget* window)
 {
  if (!initialized_ || !renderer_) {
   return;
  }
  if (!window || window->width() <= 0 || window->height() <= 0) {
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  renderer_->recreateSwapChain(window);
  renderer_->setViewportSize(static_cast<float>(window->width()), static_cast<float>(window->height()));
 }

ArtifactLayerEditorWidgetV2::ArtifactLayerEditorWidgetV2(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setMinimumSize(1, 1);

  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  setWindowTitle("ArtifactLayerEditor");

  impl_->renderTimer_ = new QTimer(this);
  impl_->renderTimer_->setInterval(16);
  QObject::connect(impl_->renderTimer_, &QTimer::timeout, this, [this]() {
   if (!impl_ || !impl_->initialized_ || !impl_->renderer_ || !impl_->running_.load(std::memory_order_acquire)) {
    return;
   }
   if (!isVisible() || width() <= 0 || height() <= 0) {
    return;
   }
   std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
   impl_->renderOneFrame();
  });

  if (auto* service = ArtifactProjectService::instance()) {
   QObject::connect(service, &ArtifactProjectService::layerSelected, this, [this](const ArtifactCore::LayerID& id) {
    setTargetLayer(id);
   });
  QObject::connect(service, &ArtifactProjectService::layerRemoved, this, [this](const ArtifactCore::CompositionID&, const ArtifactCore::LayerID& id) {
    if (impl_->targetLayerId_ == id) {
     clearTargetLayer();
    }
   });
   QObject::connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
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
   });
  }
 }

 void ArtifactLayerEditorWidgetV2::clearTargetLayer()
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  impl_->targetLayerId_ = LayerID();
  if (impl_->renderer_) {
   impl_->renderer_->clear();
   impl_->renderer_->flush();
   impl_->renderer_->present();
  }
 }

 ArtifactLayerEditorWidgetV2::~ArtifactLayerEditorWidgetV2()
 {
  impl_->destroy();
  delete impl_;
  impl_ = nullptr;
 }

 void ArtifactLayerEditorWidgetV2::keyPressEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyPressEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::keyReleaseEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mousePressEvent(QMouseEvent* event)
 {
 if (event->button() == Qt::MiddleButton ||
   (event->button() == Qt::RightButton && event->modifiers() & Qt::AltModifier))
  {
   impl_->isPanning_ = true;
   impl_->lastMousePos_ = event->position(); // 前回位置を保存
   setCursor(Qt::ClosedHandCursor);
   event->accept();
   return;
  }

  QWidget::mousePressEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseReleaseEvent(QMouseEvent* event)
 {
 if (event->button() == Qt::MiddleButton ||
      event->button() == Qt::RightButton) {
   impl_->isPanning_ = false;
   unsetCursor();
   event->accept();
   return;
  }
  QWidget::mouseReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseDoubleClickEvent(QMouseEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->isPanning_) {
   const QPointF currentPos = event->position();
   const QPointF delta = currentPos - impl_->lastMousePos_;
   impl_->lastMousePos_ = currentPos;
   panBy(delta);
   event->accept();
   return;
  }
  QWidget::mouseMoveEvent(event);
 }


 void ArtifactLayerEditorWidgetV2::wheelEvent(QWheelEvent* event)
 {
  if (!impl_->renderer_) {
   QWidget::wheelEvent(event);
   return;
  }

  const float steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
  if (std::abs(steps) <= std::numeric_limits<float>::epsilon()) {
   event->ignore();
   return;
  }

  const float zoomFactor = std::pow(1.1f, steps);
  impl_->zoomLevel_ = std::clamp(impl_->zoomLevel_ * zoomFactor, 0.05f, 32.0f);
  zoomAroundPoint(event->position(), impl_->zoomLevel_);
  event->accept();
 }

 void ArtifactLayerEditorWidgetV2::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  if (event->size().width() <= 0 || event->size().height() <= 0) {
   return;
  }
  impl_->recreateSwapChain(this);
  update();
 }

 void ArtifactLayerEditorWidgetV2::paintEvent(QPaintEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::showEvent(QShowEvent* event)
 {
  QWidget::showEvent(event);
  if (!impl_->initialized_) {
   impl_->initialize(this);
   if (impl_->initialized_) {
    impl_->initializeSwapChain(this);
    impl_->startRenderLoop();
   }
  }
  if (impl_->initialized_ && !impl_->targetLayerId_.isNil()) {
   setTargetLayer(impl_->targetLayerId_);
  }
 }
 void ArtifactLayerEditorWidgetV2::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();
 QWidget::closeEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::focusInEvent(QFocusEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::setClearColor(const FloatColor& color)
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  impl_->clearColor_ = color;
 }

void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 impl_->targetLayerId_ = id;
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
     impl_->renderer_->fitToViewport();
     impl_->zoomLevel_ = 1.0f;
     return;
    }
   }
  }
  impl_->renderer_->resetView();
 }
}

 void ArtifactLayerEditorWidgetV2::resetView()
 {
  impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) impl_->renderer_->resetView();
 }
 
 void ArtifactLayerEditorWidgetV2::fitToViewport()
 {
  impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) impl_->renderer_->fitToViewport();
 }
 
 void ArtifactLayerEditorWidgetV2::panBy(const QPointF& delta)
 {
  if (impl_->renderer_) impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
 }

 void ArtifactLayerEditorWidgetV2::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
 {
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
  }
 }

 void ArtifactLayerEditorWidgetV2::setEditMode(EditMode mode)
 {

 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {

 }

 void ArtifactLayerEditorWidgetV2::setPan(const QPointF& offset)
 {

 }

 float ArtifactLayerEditorWidgetV2::zoom() const
 {
  return impl_->zoomLevel_;
 }

 void ArtifactLayerEditorWidgetV2::setTargetLayer(LayerID& id)
 {
  setTargetLayer(static_cast<const LayerID&>(id));
 }

 QImage ArtifactLayerEditorWidgetV2::grabScreenShot()
 {
  return grab().toImage();
 }

 void ArtifactLayerEditorWidgetV2::play()
 {
  if (!impl_->initialized_) {
   return;
  }
  impl_->isPlay_ = true;
  impl_->startRenderLoop();
 }

 void ArtifactLayerEditorWidgetV2::stop()
 {
  impl_->isPlay_ = false;
  impl_->stopRenderLoop();
 }

 void ArtifactLayerEditorWidgetV2::takeScreenShot()
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
