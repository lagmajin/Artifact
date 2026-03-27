module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFontMetrics>
#include <QColor>
#include <QStringList>
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

namespace {
Q_LOGGING_CATEGORY(layerViewPerfLog, "artifact.layerviewperf")
enum class ViewSurfaceMode {
 Final,
 Source,
 BeforeAfter
};
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
  bool needsRender_ = true;
  std::atomic_bool running_{ false };
  QTimer* renderTimer_ = nullptr;
  std::mutex resizeMutex_;
  quint64 renderTickCount_ = 0;
  quint64 renderExecutedCount_ = 0;
  
  
 bool released = true;
 bool m_initialized;
  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<IFence> m_layer_fence;
  LayerID targetLayerId_{};
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  EditMode editMode_ = EditMode::View;
  DisplayMode displayMode_ = DisplayMode::Color;
  ViewSurfaceMode viewSurfaceMode_ = ViewSurfaceMode::Final;
  QImage lastRenderedFrame_;
  QPointF panOffset_{ 0.0, 0.0 };
  QString debugText_;
  QString layerInfoText_;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void updateDebugText();
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  void requestRender();
  
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
  needsRender_ = true;
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
    widget_->update();
    event->accept();
    return;
   case Qt::Key_R:
    renderer_->resetView();
    zoomLevel_ = 1.0f;
    widget_->update();
    event->accept();
    return;
   case Qt::Key_1:
    zoomLevel_ = 1.0f;
    renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Plus:
   case Qt::Key_Equal:
    zoomLevel_ = std::clamp(zoomLevel_ * 1.1f, 0.05f, 32.0f);
    renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Minus:
   case Qt::Key_Underscore:
    zoomLevel_ = std::clamp(zoomLevel_ / 1.1f, 0.05f, 32.0f);
    renderer_->zoomAroundViewportPoint({ static_cast<float>(center.x()), static_cast<float>(center.y()) }, zoomLevel_);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Left:
    renderer_->panBy(24.0f, 0.0f);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Right:
    renderer_->panBy(-24.0f, 0.0f);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Up:
    renderer_->panBy(0.0f, 24.0f);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Down:
    renderer_->panBy(0.0f, -24.0f);
    widget_->update();
    event->accept();
    return;
   case Qt::Key_Delete:
   case Qt::Key_Backspace:
    // レイヤー削除ショートカット
    if (event->modifiers() & Qt::ControlModifier) {
     if (!targetLayerId_.isNil()) {
      if (auto* service = ArtifactProjectService::instance()) {
       if (auto comp = service->currentComposition().lock()) {
        comp->removeLayer(targetLayerId_);
        targetLayerId_ = LayerID();  // クリア
        widget_->update();
        event->accept();
        return;
       }
      }
     }
    }
    break;
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

 void ArtifactLayerEditorWidgetV2::Impl::updateDebugText()
 {
  QStringList parts;
  parts << QStringLiteral("Edit=%1")
               .arg(editMode_ == EditMode::View
                        ? QStringLiteral("View")
                        : editMode_ == EditMode::Transform
                              ? QStringLiteral("Transform")
                              : editMode_ == EditMode::Mask ? QStringLiteral("Mask")
                                                            : QStringLiteral("Paint"));
  parts << QStringLiteral("Display=%1")
               .arg(displayMode_ == DisplayMode::Color
                        ? QStringLiteral("Color")
                        : displayMode_ == DisplayMode::Alpha
                              ? QStringLiteral("Alpha")
                              : displayMode_ == DisplayMode::Mask
                                    ? QStringLiteral("Mask")
                                    : QStringLiteral("Wireframe"));
  parts << QStringLiteral("View=%1")
               .arg(viewSurfaceMode_ == ViewSurfaceMode::Final
                        ? QStringLiteral("Final")
                        : viewSurfaceMode_ == ViewSurfaceMode::Source
                              ? QStringLiteral("Source")
                              : QStringLiteral("BeforeAfter"));

  if (renderer_) {
   parts << QStringLiteral("Zoom=%1").arg(renderer_->getZoom(), 0, 'f', 2);
   float panX = 0.0f;
   float panY = 0.0f;
   renderer_->getPan(panX, panY);
   panOffset_ = QPointF(panX, panY);
   parts << QStringLiteral("Pan=%1,%2")
                .arg(panOffset_.x(), 0, 'f', 1)
                .arg(panOffset_.y(), 0, 'f', 1);
  }

  if (!targetLayerId_.isNil()) {
   parts << QStringLiteral("Layer=%1").arg(targetLayerId_.toString());
  }
  debugText_ = parts.join(QStringLiteral(" | "));
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
    if (viewSurfaceMode_ == ViewSurfaceMode::Final || viewSurfaceMode_ == ViewSurfaceMode::BeforeAfter) {
     compositionRenderer_->DrawCompositionBackground(clearColor_);
    }
   } else {
    renderer_->drawRectLocal(-8192, -8192, 16384, 16384, clearColor_);
   }
 if (!targetLayerId_.isNil()) {
   layerInfoText_.clear();
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
      const auto localBounds = layer->localBounds();
      const auto globalBounds = layer->transformedBoundingBox();
      const auto source = layer->sourceSize();
      const bool isVisible = layer->isVisible();
      const bool isLocked = layer->isLocked();
      const bool isSolo = layer->isSolo();
      const bool isActive = layer->isActiveAt(currentFrame);
      const QString stateLabel =
          !isVisible
              ? QStringLiteral("Hidden")
              : !isActive
                    ? QStringLiteral("OutOfRange")
                    : layer->opacity() <= 0.0f
                          ? QStringLiteral("Transparent")
                          : QStringLiteral("Ready");
      layerInfoText_ = QStringLiteral("%1 | %2 | Op=%3 | Src=%4x%5 | Local=%6x%7 | Global=%8x%9")
          .arg(layer->layerName().isEmpty() ? QStringLiteral("(Unnamed)") : layer->layerName())
          .arg(layer->is3D() ? QStringLiteral("3D") : QStringLiteral("2D"))
          .arg(layer->opacity(), 0, 'f', 2)
          .arg(source.width)
          .arg(source.height)
          .arg(localBounds.width(), 0, 'f', 1)
          .arg(localBounds.height(), 0, 'f', 1)
          .arg(globalBounds.width(), 0, 'f', 1)
          .arg(globalBounds.height(), 0, 'f', 1);
      layerInfoText_ += QStringLiteral(" | Vis=%1 Lock=%2 Solo=%3 Active=%4 State=%5")
          .arg(isVisible ? QStringLiteral("Y") : QStringLiteral("N"))
          .arg(isLocked ? QStringLiteral("Y") : QStringLiteral("N"))
          .arg(isSolo ? QStringLiteral("Y") : QStringLiteral("N"))
          .arg(isActive ? QStringLiteral("Y") : QStringLiteral("N"))
          .arg(stateLabel);
      if (source.width > 0 && source.height > 0) {
       // レイヤーサイズも設定（コンポジションサイズを上書きしないためコメントアウト）
       // renderer_->setCanvasSize(static_cast<float>(source.width), static_cast<float>(source.height));
      }
      if (!isVisible || !isActive || layer->opacity() <= 0.0f) {
      } else {
       layer->draw(renderer_.get());
      }
     }
     if (layerInfoText_.isEmpty()) {
      layerInfoText_ = QStringLiteral("No inspect data");
     }
    }
   }
  }
  if (targetLayerId_.isNil()) {
   layerInfoText_.clear();
  }
  renderer_->flush();
  renderer_->present();
  if (viewSurfaceMode_ == ViewSurfaceMode::Source || viewSurfaceMode_ == ViewSurfaceMode::BeforeAfter) {
   lastRenderedFrame_ = renderer_->readbackToImage();
  } else {
   lastRenderedFrame_ = QImage();
  }
  updateDebugText();
  widget_->update();
  needsRender_ = false;
 }

void ArtifactLayerEditorWidgetV2::Impl::requestRender()
{
 needsRender_ = true;
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
   if (!impl_->isPlay_ && !impl_->needsRender_) {
    return;
   }
   std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
   QElapsedTimer frameTimer;
   frameTimer.start();
   impl_->renderOneFrame();
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
  impl_->requestRender();
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
  impl_->requestRender();
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
   impl_->requestRender();
   event->accept();
   return;
  }
  
  // Left button click - select layer or manipulate gizmo
  if (event->button() == Qt::LeftButton) {
   impl_->lastMousePos_ = event->position();
   impl_->requestRender();
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
   impl_->requestRender();
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
   impl_->requestRender();
   event->accept();
   return;
  }
  
  // パニング中でなくてもマウスイベントは常に処理する
  // ギズモのホバー判定やカーソル変化のために必要
  impl_->lastMousePos_ = event->position();
  impl_->requestRender();
  event->accept();
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
  impl_->requestRender();
  event->accept();
 }

 void ArtifactLayerEditorWidgetV2::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  if (event->size().width() <= 0 || event->size().height() <= 0) {
   return;
  }
  impl_->recreateSwapChain(this);
  impl_->requestRender();
  update();
 }

void ArtifactLayerEditorWidgetV2::paintEvent(QPaintEvent* event)
{
 Q_UNUSED(event);
 QPainter painter(this);
 painter.setRenderHint(QPainter::Antialiasing, true);
 painter.setRenderHint(QPainter::TextAntialiasing, true);

 const QRect overlayRect(12, 12, std::max(240, std::min(width() - 24, 560)), 96);
 painter.setPen(Qt::NoPen);
 painter.setBrush(QColor(16, 18, 22, 185));
 painter.drawRoundedRect(overlayRect, 10, 10);
 painter.setPen(QColor(240, 240, 240));
 QFont font = painter.font();
 font.setPointSizeF(std::max(9.0, font.pointSizeF()));
 font.setBold(true);
 painter.setFont(font);

 const QString modeLabel =
     impl_->viewSurfaceMode_ == ViewSurfaceMode::Final
         ? QStringLiteral("FINAL")
         : impl_->viewSurfaceMode_ == ViewSurfaceMode::Source
               ? QStringLiteral("SOURCE")
               : QStringLiteral("COMPARE");
 const QColor modeColor =
     impl_->viewSurfaceMode_ == ViewSurfaceMode::Final
         ? QColor(84, 160, 255)
         : impl_->viewSurfaceMode_ == ViewSurfaceMode::Source
               ? QColor(255, 173, 76)
               : QColor(156, 102, 255);
 const QRect modeRect(width() - 118, 12, 106, 28);
 painter.setPen(Qt::NoPen);
 painter.setBrush(QColor(modeColor.red(), modeColor.green(), modeColor.blue(), 210));
 painter.drawRoundedRect(modeRect, 9, 9);
 painter.setPen(Qt::white);
 painter.drawText(modeRect, Qt::AlignCenter, modeLabel);

 const int textX = overlayRect.x() + 14;
 int textY = overlayRect.y() + 22;
 painter.drawText(textX, textY, QStringLiteral("Layer Solo View"));
 font.setBold(false);
 painter.setFont(font);
 textY += 20;
 painter.drawText(textX, textY, impl_->debugText_.isEmpty() ? QStringLiteral("No layer selected") : impl_->debugText_);
 textY += 18;
 painter.setPen(QColor(205, 214, 226));
 painter.drawText(textX, textY, impl_->layerInfoText_.isEmpty() ? QStringLiteral("Inspect: -") : impl_->layerInfoText_);

 if (impl_->viewSurfaceMode_ != ViewSurfaceMode::Final && !impl_->lastRenderedFrame_.isNull()) {
  const QSize thumbSize(std::min(180, width() / 4), std::min(100, height() / 5));
  const QRect thumbRect(width() - thumbSize.width() - 12, 12, thumbSize.width(), thumbSize.height());
  painter.setPen(QColor(240, 240, 240, 160));
  painter.setBrush(QColor(0, 0, 0, 140));
  painter.drawRoundedRect(thumbRect.adjusted(0, 0, -1, -1), 8, 8);
  painter.drawImage(thumbRect, impl_->lastRenderedFrame_.scaled(thumbRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
 }
}

 void ArtifactLayerEditorWidgetV2::showEvent(QShowEvent* event)
 {
 QWidget::showEvent(event);
  qCDebug(layerViewPerfLog) << "[LayerView][Show]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
  if (!impl_->initialized_) {
   QTimer::singleShot(0, this, [this]() {
    if (!impl_ || impl_->initialized_ || !isVisible()) {
     return;
    }
    impl_->initialize(this);
    if (impl_->initialized_) {
     impl_->initializeSwapChain(this);
     impl_->renderer_->fitToViewport();
     impl_->zoomLevel_ = impl_->renderer_->getZoom();
     impl_->requestRender();
     impl_->startRenderLoop();
    }
   });
  }
  if (impl_->initialized_ && !impl_->targetLayerId_.isNil()) {
   setTargetLayer(impl_->targetLayerId_);
  }
 }

 void ArtifactLayerEditorWidgetV2::hideEvent(QHideEvent* event)
 {
  qCDebug(layerViewPerfLog) << "[LayerView][Hide]"
                            << "initialized=" << impl_->initialized_
                            << "visible=" << isVisible()
                            << "size=" << size();
  if (impl_->initialized_) {
   impl_->stopRenderLoop();
  }
  QWidget::hideEvent(event);
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
  impl_->requestRender();
  update();
}

void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 impl_->targetLayerId_ = id;
 impl_->requestRender();
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
      impl_->zoomLevel_ = impl_->renderer_->getZoom();
      impl_->requestRender();
      return;
    }
   }
  }
  impl_->renderer_->resetView();
  impl_->updateDebugText();
  impl_->requestRender();
  update();
  }
 }

void ArtifactLayerEditorWidgetV2::resetView()
{
  impl_->zoomLevel_ = 1.0f;
  if (impl_->renderer_) {
   impl_->renderer_->resetView();
   float panX = 0.0f;
   float panY = 0.0f;
   impl_->renderer_->getPan(panX, panY);
   impl_->panOffset_ = QPointF(panX, panY);
   impl_->updateDebugText();
   impl_->requestRender();
  }
  update();
}
 
void ArtifactLayerEditorWidgetV2::fitToViewport()
  {
   if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
    float panX = 0.0f;
    float panY = 0.0f;
    impl_->renderer_->getPan(panX, panY);
    impl_->panOffset_ = QPointF(panX, panY);
    impl_->updateDebugText();
    impl_->requestRender();
   }
   update();
  }
 
void ArtifactLayerEditorWidgetV2::panBy(const QPointF& delta)
{
  if (impl_->renderer_) {
   impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
   float panX = 0.0f;
   float panY = 0.0f;
   impl_->renderer_->getPan(panX, panY);
   impl_->panOffset_ = QPointF(panX, panY);
   impl_->updateDebugText();
   impl_->requestRender();
  }
  update();
}

void ArtifactLayerEditorWidgetV2::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
{
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
      float panX = 0.0f;
      float panY = 0.0f;
      impl_->renderer_->getPan(panX, panY);
      impl_->panOffset_ = QPointF(panX, panY);
      impl_->zoomLevel_ = impl_->renderer_->getZoom();
      impl_->updateDebugText();
      impl_->requestRender();
  }
  update();
}

 void ArtifactLayerEditorWidgetV2::setEditMode(EditMode mode)
 {
  if (impl_->editMode_ == mode) {
   return;
  }
  impl_->editMode_ = mode;
  impl_->updateDebugText();
  impl_->requestRender();
  update();
 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {
  if (impl_->displayMode_ == mode) {
   return;
  }
  impl_->displayMode_ = mode;
  switch (mode) {
  case DisplayMode::Color:
   impl_->viewSurfaceMode_ = ViewSurfaceMode::Final;
   break;
  case DisplayMode::Alpha:
  case DisplayMode::Mask:
  case DisplayMode::Wireframe:
   impl_->viewSurfaceMode_ = ViewSurfaceMode::Source;
   break;
  }
  impl_->updateDebugText();
  impl_->requestRender();
  update();
 }

 void ArtifactLayerEditorWidgetV2::setPan(const QPointF& offset)
 {
  impl_->panOffset_ = offset;
  if (impl_->renderer_) {
   impl_->renderer_->setPan((float)offset.x(), (float)offset.y());
  }
  impl_->updateDebugText();
  impl_->requestRender();
  update();
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
  impl_->requestRender();
  impl_->startRenderLoop();
 }

 void ArtifactLayerEditorWidgetV2::stop()
 {
  impl_->isPlay_ = false;
  impl_->requestRender();
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
