module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPolygonF>
#include <QStringList>
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
import Artifact.Layer.Video;

namespace Artifact {

 using namespace ArtifactCore;

W_OBJECT_IMPL(ArtifactLayerEditorWidgetV2)

namespace {
enum class ViewSurfaceMode {
 Final,
 Source,
 BeforeAfter,
 DebugWireframe
};

static QString editModeLabel(const EditMode mode)
{
 switch (mode) {
  case EditMode::Transform: return QStringLiteral("Edit: Transform");
  case EditMode::Mask: return QStringLiteral("Edit: Mask");
  case EditMode::Paint: return QStringLiteral("Edit: Paint");
  case EditMode::View:
  default: return QStringLiteral("Edit: View");
 }
}

static QString displayModeLabel(const DisplayMode mode)
{
 switch (mode) {
  case DisplayMode::Alpha: return QStringLiteral("Display: Source / Alpha");
  case DisplayMode::Mask: return QStringLiteral("Display: Before / After");
  case DisplayMode::Wireframe: return QStringLiteral("Display: Debug / Wireframe");
  case DisplayMode::Color:
  default: return QStringLiteral("Display: Final");
 }
}

static ViewSurfaceMode toViewSurfaceMode(const DisplayMode mode)
{
 switch (mode) {
  case DisplayMode::Alpha: return ViewSurfaceMode::Source;
  case DisplayMode::Mask: return ViewSurfaceMode::BeforeAfter;
  case DisplayMode::Wireframe: return ViewSurfaceMode::DebugWireframe;
  case DisplayMode::Color:
  default: return ViewSurfaceMode::Final;
 }
}
}

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
  EditMode editMode_ = EditMode::View;
  DisplayMode displayMode_ = DisplayMode::Color;
  ViewSurfaceMode viewSurfaceMode_ = ViewSurfaceMode::Final;
  QString debugText_;
  
  
 bool released = true;
 bool m_initialized;
  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<IFence> m_layer_fence;
  LayerID targetLayerId_{}; 
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  QImage lastRenderedFrame_;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  ArtifactAbstractLayerPtr currentTargetLayer() const;
  QImage currentSourceImage() const;
  QRectF currentTargetBounds() const;
  QPointF currentTargetPivot() const;
  QString currentDebugString() const;
  void updateDebugText();
  
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
    zoomLevel_ = renderer_->getZoom();
    update();
   event->accept();
   return;
  case Qt::Key_R:
   renderer_->resetView();
   zoomLevel_ = 1.0f;
   update();
   event->accept();
   return;
  case Qt::Key_1:
   zoomLevel_ = 1.0f;
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   update();
   event->accept();
   return;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
   zoomLevel_ = std::clamp(zoomLevel_ * 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   update();
   event->accept();
   return;
  case Qt::Key_Minus:
  case Qt::Key_Underscore:
   zoomLevel_ = std::clamp(zoomLevel_ / 1.1f, 0.05f, 32.0f);
   renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
   update();
   event->accept();
   return;
  case Qt::Key_Left:
   renderer_->panBy(24.0f, 0.0f);
   update();
   event->accept();
   return;
  case Qt::Key_Right:
   renderer_->panBy(-24.0f, 0.0f);
   update();
   event->accept();
   return;
  case Qt::Key_Up:
   renderer_->panBy(0.0f, 24.0f);
   update();
   event->accept();
   return;
  case Qt::Key_Down:
   renderer_->panBy(0.0f, -24.0f);
   update();
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

 ArtifactAbstractLayerPtr ArtifactLayerEditorWidgetV2::Impl::currentTargetLayer() const
 {
  if (targetLayerId_.isNil()) {
   return {};
  }
  auto* service = ArtifactProjectService::instance();
  if (!service) {
   return {};
  }
  auto composition = service->currentComposition().lock();
  if (!composition) {
   return {};
  }
  return composition->layerById(targetLayerId_);
 }

 QImage ArtifactLayerEditorWidgetV2::Impl::currentSourceImage() const
 {
  const auto layer = currentTargetLayer();
  if (!layer) {
   return {};
  }
  if (auto* imageLayer = dynamic_cast<ArtifactImageLayer*>(layer.get())) {
   return imageLayer->toQImage();
  }
  if (auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layer.get())) {
   const auto frame = ArtifactPlaybackService::instance()
       ? ArtifactPlaybackService::instance()->currentFrame().framePosition()
       : layer->currentFrame();
   return videoLayer->decodeFrameToQImage(frame);
  }
  return layer->getThumbnail();
 }

 QRectF ArtifactLayerEditorWidgetV2::Impl::currentTargetBounds() const
 {
  const auto layer = currentTargetLayer();
  if (!layer) {
   return {};
  }
  return layer->transformedBoundingBox();
 }

 QPointF ArtifactLayerEditorWidgetV2::Impl::currentTargetPivot() const
 {
  const auto layer = currentTargetLayer();
  if (!layer) {
   return {};
  }
  const auto& t3 = layer->transform3D();
  return layer->getGlobalTransform().map(QPointF(t3.anchorX(), t3.anchorY()));
 }

 QString ArtifactLayerEditorWidgetV2::Impl::currentDebugString() const
 {
  const auto layer = currentTargetLayer();
  if (!layer) {
   return QStringLiteral("cache:none | debug:none");
  }

  const auto bounds = layer->transformedBoundingBox();
  const auto source = layer->sourceSize();
  const QString cacheState = layer->isDirty() ? QStringLiteral("dirty") : QStringLiteral("clean");
  const QString maskState = QStringLiteral("masks:%1").arg(layer->maskCount());
  const QString effectState = QStringLiteral("effects:%1").arg(layer->effectCount());

  return QStringLiteral("cache:%1 | debug:%2 | %3 | %4 | src:%5x%6 | bounds:%7x%8")
      .arg(cacheState)
      .arg(layer->isVisible() ? QStringLiteral("visible") : QStringLiteral("hidden"))
      .arg(maskState)
      .arg(effectState)
      .arg(source.width)
      .arg(source.height)
      .arg(qRound(bounds.width()))
      .arg(qRound(bounds.height()));
 }

 void ArtifactLayerEditorWidgetV2::Impl::updateDebugText()
 {
  debugText_ = currentDebugString();
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
       if (viewSurfaceMode_ != ViewSurfaceMode::DebugWireframe) {
        layer->draw(renderer_.get());
       }
      }

      if (viewSurfaceMode_ == ViewSurfaceMode::DebugWireframe) {
       const QRectF bounds = layer->transformedBoundingBox();
       if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
        renderer_->drawRectOutlineLocal(static_cast<float>(bounds.x()),
                                        static_cast<float>(bounds.y()),
                                        static_cast<float>(bounds.width()),
                                        static_cast<float>(bounds.height()),
                                        FloatColor(0.92f, 0.84f, 0.24f, 1.0f));
        const auto& t3 = layer->transform3D();
        const QPointF pivot = layer->getGlobalTransform().map(QPointF(t3.anchorX(), t3.anchorY()));
        renderer_->drawCrosshair(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()), 12.0f,
                                 FloatColor(0.22f, 1.0f, 0.62f, 1.0f));
       }
      }
     }
    }
   }
  }
  renderer_->flush();
  renderer_->present();
  if (viewSurfaceMode_ == ViewSurfaceMode::Source || viewSurfaceMode_ == ViewSurfaceMode::BeforeAfter) {
   lastRenderedFrame_ = renderer_->readbackToImage();
  } else {
   lastRenderedFrame_ = QImage();
  }
  updateDebugText();
  update();
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
  impl_->updateDebugText();
  update();
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

 const float currentZoom = impl_->renderer_->getZoom();
 const float zoomFactor = std::pow(1.1f, steps);
 impl_->zoomLevel_ = std::clamp(currentZoom * zoomFactor, 0.05f, 32.0f);
 zoomAroundPoint(event->position(), impl_->zoomLevel_);
 update();
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
 Q_UNUSED(event);
 if (!impl_ || !impl_->initialized_) {
  return;
 }

 QPainter painter(this);
 painter.setRenderHint(QPainter::Antialiasing, true);
 painter.setRenderHint(QPainter::TextAntialiasing, true);

 const QRect overlayRect(12, 12, std::max(0, std::min(width() - 24, 560)), std::max(0, std::min(height() - 24, 180)));
 painter.setPen(Qt::NoPen);
 painter.setBrush(QColor(12, 12, 16, 175));
 painter.drawRoundedRect(overlayRect, 10, 10);

 painter.setPen(QColor(230, 230, 240));
 QFont titleFont = painter.font();
 titleFont.setBold(true);
 titleFont.setPointSize(std::max(8, titleFont.pointSize() + 1));
 painter.setFont(titleFont);

 const auto layer = impl_->currentTargetLayer();
 painter.drawText(overlayRect.adjusted(14, 10, -14, -overlayRect.height() + 34),
                  Qt::AlignLeft | Qt::AlignTop,
                  layer ? layer->layerName() : QStringLiteral("(no layer)"));

 QFont bodyFont = painter.font();
 bodyFont.setBold(false);
 bodyFont.setPointSize(std::max(8, bodyFont.pointSize() - 1));
 painter.setFont(bodyFont);

 QStringList lines;
 lines << editModeLabel(impl_->editMode_);
 lines << displayModeLabel(impl_->displayMode_);
 lines << QStringLiteral("Zoom: %1").arg(impl_->renderer_ ? impl_->renderer_->getZoom() : impl_->zoomLevel_, 0, 'f', 2);
 lines << QStringLiteral("Stage: %1").arg(layer ? layer->layerName() : QStringLiteral("none"));
 lines << (impl_->debugText_.isEmpty() ? impl_->currentDebugString() : impl_->debugText_);

 painter.setPen(QColor(205, 210, 220));
 painter.drawText(overlayRect.adjusted(14, 40, -14, -12),
                  Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                  lines.join(QStringLiteral("\n")));

 if (!layer) {
  return;
 }

 const QImage sourcePreview = impl_->currentSourceImage();
 const QImage finalPreview = impl_->lastRenderedFrame_.isNull() ? QImage() : impl_->lastRenderedFrame_;

 if (impl_->viewSurfaceMode_ == ViewSurfaceMode::Source) {
  const QRect previewRect = QRect(24, 220, std::max(0, width() - 48), std::max(0, height() - 244));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(16, 16, 20, 210));
  painter.drawRoundedRect(previewRect, 12, 12);
  painter.setPen(QColor(220, 220, 220));
  painter.drawText(previewRect.adjusted(16, 12, -16, -previewRect.height() + 32),
                   Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("Source"));
  if (!sourcePreview.isNull()) {
   const QSize scaledSize = sourcePreview.size().scaled(previewRect.size() - QSize(24, 44), Qt::KeepAspectRatio);
   const QRect imageRect(QPoint(previewRect.center().x() - scaledSize.width() / 2,
                                previewRect.center().y() - scaledSize.height() / 2 + 12),
                         scaledSize);
   painter.drawImage(imageRect, sourcePreview);
  }
 }

 if (impl_->viewSurfaceMode_ == ViewSurfaceMode::BeforeAfter) {
  const QRect compareRect(12, std::max(12, height() - 156), std::max(0, std::min(width() - 24, 560)), 132);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(20, 20, 24, 190));
  painter.drawRoundedRect(compareRect, 10, 10);
  painter.setPen(QColor(220, 220, 220));
  painter.drawText(compareRect.adjusted(14, 10, -14, -compareRect.height() + 28),
                   Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("Before / After"));

  const QRect leftCard = QRect(compareRect.left() + 12, compareRect.top() + 34,
                               compareRect.width() / 2 - 18, compareRect.height() - 46);
  const QRect rightCard = QRect(compareRect.center().x() + 6, compareRect.top() + 34,
                                compareRect.width() / 2 - 18, compareRect.height() - 46);

  painter.setBrush(QColor(40, 40, 48, 220));
  painter.drawRoundedRect(leftCard, 8, 8);
  painter.drawRoundedRect(rightCard, 8, 8);
  if (!sourcePreview.isNull()) {
   const QSize scaledSize = sourcePreview.size().scaled(leftCard.size() - QSize(16, 16), Qt::KeepAspectRatio);
   const QRect imageRect(QPoint(leftCard.center().x() - scaledSize.width() / 2,
                                leftCard.center().y() - scaledSize.height() / 2),
                         scaledSize);
   painter.drawImage(imageRect, sourcePreview);
  }
  if (!finalPreview.isNull()) {
   const QSize scaledSize = finalPreview.size().scaled(rightCard.size() - QSize(16, 16), Qt::KeepAspectRatio);
   const QRect imageRect(QPoint(rightCard.center().x() - scaledSize.width() / 2,
                                rightCard.center().y() - scaledSize.height() / 2),
                         scaledSize);
   painter.drawImage(imageRect, finalPreview);
  }
  painter.setPen(QColor(240, 240, 240));
  painter.drawText(leftCard.adjusted(10, 10, -10, -10), Qt::AlignTop | Qt::AlignHCenter, QStringLiteral("Before"));
  painter.drawText(rightCard.adjusted(10, 10, -10, -10), Qt::AlignTop | Qt::AlignHCenter, QStringLiteral("After"));
 }

 const QRectF bounds = impl_->currentTargetBounds();
 if (bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0) {
  auto mapPoint = [this](const QPointF& canvasPos) -> QPointF {
   if (!impl_ || !impl_->renderer_) {
    return canvasPos;
   }
   const auto vp = impl_->renderer_->canvasToViewport(
       Detail::float2{ static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y()) });
   return QPointF(vp.x, vp.y);
  };

  const QPointF p1 = mapPoint(bounds.topLeft());
  const QPointF p2 = mapPoint(bounds.topRight());
  const QPointF p3 = mapPoint(bounds.bottomRight());
  const QPointF p4 = mapPoint(bounds.bottomLeft());

  QPolygonF polygon;
  polygon << p1 << p2 << p3 << p4;

  QPen boundsPen(QColor(255, 208, 92, 210));
  boundsPen.setWidth(2);
  painter.setPen(boundsPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPolygon(polygon);

  const QPointF pivot = mapPoint(impl_->currentTargetPivot());
  QPen pivotPen(QColor(82, 245, 170, 230));
  pivotPen.setWidth(2);
  painter.setPen(pivotPen);
  painter.drawLine(QPointF(pivot.x() - 8.0, pivot.y()), QPointF(pivot.x() + 8.0, pivot.y()));
  painter.drawLine(QPointF(pivot.x(), pivot.y() - 8.0), QPointF(pivot.x(), pivot.y() + 8.0));
 }
}

 void ArtifactLayerEditorWidgetV2::showEvent(QShowEvent* event)
 {
  QWidget::showEvent(event);
 if (!impl_->initialized_) {
   impl_->initialize(this);
   if (impl_->initialized_) {
    impl_->initializeSwapChain(this);
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
    impl_->startRenderLoop();
   }
  }
  if (impl_->initialized_ && !impl_->targetLayerId_.isNil()) {
   setTargetLayer(impl_->targetLayerId_);
  }
  update();
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
  update();
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
    const auto compSize = composition->settings().compositionSize();
    if (compSize.width() > 0 && compSize.height() > 0) {
     impl_->renderer_->setCanvasSize(static_cast<float>(compSize.width()),
                                     static_cast<float>(compSize.height()));
    }

    if (auto layer = composition->layerById(id)) {
     const auto source = layer->sourceSize();
     if (source.width > 0 && source.height > 0) {
      // レイヤーサイズは使用しない（コンポジションサイズを優先）
     }
     impl_->renderer_->fitToViewport();
     impl_->zoomLevel_ = impl_->renderer_->getZoom();
     impl_->updateDebugText();
     update();
     return;
    }
   }
  }
  impl_->renderer_->resetView();
  impl_->updateDebugText();
  update();
}

 void ArtifactLayerEditorWidgetV2::resetView()
 {
  impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) impl_->renderer_->resetView();
  update();
 }
 
 void ArtifactLayerEditorWidgetV2::fitToViewport()
  {
   if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
    update();
   }
  }
 
 void ArtifactLayerEditorWidgetV2::panBy(const QPointF& delta)
 {
  if (impl_->renderer_) {
   impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
   update();
  }
 }

 void ArtifactLayerEditorWidgetV2::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
 {
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
      impl_->zoomLevel_ = newZoom;
      update();
  }
 }

 void ArtifactLayerEditorWidgetV2::setEditMode(EditMode mode)
 {
  impl_->editMode_ = mode;
  update();
 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {
  impl_->displayMode_ = mode;
  impl_->viewSurfaceMode_ = toViewSurfaceMode(mode);
  update();
 }

 void ArtifactLayerEditorWidgetV2::setPan(const QPointF& offset)
 {
  if (impl_->renderer_) {
   impl_->renderer_->setPan(static_cast<float>(offset.x()), static_cast<float>(offset.y()));
  }
  update();
 }

 float ArtifactLayerEditorWidgetV2::zoom() const
 {
  return impl_->renderer_ ? impl_->renderer_->getZoom() : impl_->zoomLevel_;
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
  update();
 }

 void ArtifactLayerEditorWidgetV2::stop()
 {
  impl_->isPlay_ = false;
  impl_->stopRenderLoop();
  update();
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
