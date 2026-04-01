module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QDialog>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShortcut>
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
#include <functional>
#include <vector>

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
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Undo.UndoManager;
import Property.Abstract;

import Artifact.Render.IRenderer;
import Artifact.Render.CompositionRenderer;
import Artifact.Preview.Pipeline;
import Artifact.MainWindow;
import Artifact.Layer.Image;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import CvUtils;

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

struct LayerSoloCommandEntry {
 QString key;
 QString description;
 QStringList aliases;
 std::function<void()> action;
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
  QTimer* resizeDebounceTimer_ = nullptr;
  QDialog* commandPalette_ = nullptr;
  QLineEdit* commandEdit_ = nullptr;
  QListWidget* commandList_ = nullptr;
  std::vector<LayerSoloCommandEntry> commandEntries_;
  std::mutex resizeMutex_;
  quint64 renderTickCount_ = 0;
  quint64 renderExecutedCount_ = 0;
  QElapsedTimer renderThrottleClock_;
  qint64 lastRenderFrameAtMs_ = 0;
  QSize pendingResizeSize_;
  bool resizePending_ = false;
  
  
  bool released = true;
  bool m_initialized;
  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<ITexture> m_maskPreviewRT;
  Uint32 m_maskPreviewWidth = 0;
  Uint32 m_maskPreviewHeight = 0;
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
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int draggingMaskIndex_ = -1;
  int draggingPathIndex_ = -1;
  int draggingVertexIndex_ = -1;
  bool maskEditPending_ = false;
  bool maskEditDirty_ = false;
  ArtifactAbstractLayerWeak maskEditLayer_;
  std::vector<LayerMask> maskEditBefore_;
  bool immersiveMode_ = false;

  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markMaskEditDirty();
  void commitMaskEditTransaction();
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void updateDebugText();
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  bool ensureMaskPreviewRT(int width, int height);
  void requestRender();
  void toggleImmersiveMode(bool immersive);
  void showCommandPalette();
  void hideCommandPalette();
  bool isCommandPaletteVisible() const;
  void rebuildCommandPalette(const QString& text = QString());
  bool executeCommandText(const QString& text);
  
  void startRenderLoop();
  void stopRenderLoop();
  void renderOneFrame();
 };

 ArtifactLayerEditorWidgetV2::Impl::Impl()
 {
  resizeDebounceTimer_ = new QTimer(nullptr);
  resizeDebounceTimer_->setSingleShot(true);
  QObject::connect(resizeDebounceTimer_, &QTimer::timeout, [this]() {
   if (!initialized_ || !renderer_ || !widget_) {
    resizePending_ = false;
    return;
   }
   const QSize targetSize = pendingResizeSize_.isValid() ? pendingResizeSize_ : widget_->size();
   if (targetSize.width() <= 0 || targetSize.height() <= 0) {
    resizePending_ = false;
    return;
   }
   std::lock_guard<std::mutex> lock(resizeMutex_);
   renderer_->recreateSwapChain(widget_);
   renderer_->setViewportSize(static_cast<float>(targetSize.width()),
                              static_cast<float>(targetSize.height()));
   resizePending_ = false;
   requestRender();
   if (widget_) {
    widget_->update();
   }
  });

 }

ArtifactLayerEditorWidgetV2::Impl::~Impl()
{

}

void ArtifactLayerEditorWidgetV2::Impl::showCommandPalette()
{
 if (!widget_) {
  return;
 }

 if (!commandPalette_) {
  commandPalette_ = new QDialog(widget_);
  commandPalette_->setObjectName(QStringLiteral("layerSoloCommandPalette"));
  commandPalette_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  commandPalette_->setModal(false);
  commandPalette_->setAttribute(Qt::WA_DeleteOnClose, false);

  auto* layout = new QVBoxLayout(commandPalette_);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);

  commandEdit_ = new QLineEdit(commandPalette_);
  commandEdit_->setPlaceholderText(QStringLiteral("Command search: fit, reset, source, final, compare, mask, view"));
  commandEdit_->setClearButtonEnabled(true);
  commandEdit_->setObjectName(QStringLiteral("layerSoloCommandSearch"));

  commandList_ = new QListWidget(commandPalette_);
  commandList_->setObjectName(QStringLiteral("layerSoloCommandList"));
  commandList_->setSelectionMode(QAbstractItemView::SingleSelection);
  commandList_->setUniformItemSizes(true);
  commandList_->setAlternatingRowColors(true);
  commandList_->setFocusPolicy(Qt::NoFocus);

  layout->addWidget(commandEdit_);
  layout->addWidget(commandList_);

  commandPalette_->setStyleSheet(R"(
    QDialog#layerSoloCommandPalette {
      background: #252525;
      border: 1px solid #4A4A4A;
      border-radius: 10px;
    }
    QLineEdit#layerSoloCommandSearch {
      background: #1F1F1F;
      color: #F0F0F0;
      border: 1px solid #444;
      border-radius: 8px;
      padding: 6px 10px;
      font-size: 11px;
    }
    QListWidget#layerSoloCommandList {
      background: #202020;
      color: #E8E8E8;
      border: 1px solid #3A3A3A;
      border-radius: 8px;
    }
    QListWidget#layerSoloCommandList::item {
      padding: 6px 8px;
    }
    QListWidget#layerSoloCommandList::item:selected {
      background: #4A6FA5;
      color: white;
    }
  )");

  QObject::connect(commandEdit_, &QLineEdit::textChanged, widget_, [this](const QString& text) {
    rebuildCommandPalette(text);
  });
  QObject::connect(commandEdit_, &QLineEdit::returnPressed, widget_, [this]() {
    if (!commandEdit_) {
      return;
    }
    const QString text = commandEdit_->text().trimmed();
    if (executeCommandText(text)) {
      hideCommandPalette();
    }
  });
  QObject::connect(commandList_, &QListWidget::itemActivated, widget_, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const QString key = item->data(Qt::UserRole).toString();
    if (executeCommandText(key)) {
      hideCommandPalette();
    }
  });
  QObject::connect(commandList_, &QListWidget::itemDoubleClicked, widget_, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const QString key = item->data(Qt::UserRole).toString();
    if (executeCommandText(key)) {
      hideCommandPalette();
    }
  });

  auto* cancelShortcut = new QShortcut(QKeySequence::Cancel, commandPalette_);
  QObject::connect(cancelShortcut, &QShortcut::activated, widget_, [this]() {
    hideCommandPalette();
  });
 }

 const int paletteWidth = std::clamp(widget_->width() - 40, 320, 520);
 commandPalette_->resize(paletteWidth, 320);
 const QPoint topLeft = widget_->mapToGlobal(QPoint((widget_->width() - commandPalette_->width()) / 2,
                                                    std::max(24, widget_->height() / 4)));
 commandPalette_->move(topLeft);
 rebuildCommandPalette(commandEdit_ ? commandEdit_->text() : QString());
 commandPalette_->show();
 commandPalette_->raise();
 commandPalette_->activateWindow();
 if (commandEdit_) {
  commandEdit_->setFocus(Qt::ShortcutFocusReason);
  commandEdit_->selectAll();
 }
}

void ArtifactLayerEditorWidgetV2::Impl::hideCommandPalette()
{
 if (commandPalette_) {
  commandPalette_->hide();
 }
}

bool ArtifactLayerEditorWidgetV2::Impl::isCommandPaletteVisible() const
{
 return commandPalette_ && commandPalette_->isVisible();
}

void ArtifactLayerEditorWidgetV2::Impl::rebuildCommandPalette(const QString& text)
{
 if (!commandList_) {
  return;
 }

 commandEntries_.clear();
 commandEntries_.push_back({QStringLiteral("fit"), QStringLiteral("Fit viewport to layer"), {QStringLiteral("fit viewport"), QStringLiteral("frame"), QStringLiteral("f")}, [this]() {
  if (!renderer_) return;
  renderer_->fitToViewport();
  zoomLevel_ = renderer_->getZoom();
  requestRender();
 }});
 commandEntries_.push_back({QStringLiteral("reset"), QStringLiteral("Reset pan and zoom"), {QStringLiteral("reset view"), QStringLiteral("r")}, [this]() {
  if (!renderer_) return;
  renderer_->resetView();
  zoomLevel_ = 1.0f;
  requestRender();
 }});
 commandEntries_.push_back({QStringLiteral("1:1"), QStringLiteral("Zoom to 100%"), {QStringLiteral("actual size"), QStringLiteral("100%")}, [this]() {
  if (!renderer_ || !widget_) return;
  zoomLevel_ = 1.0f;
  renderer_->zoomAroundViewportPoint({ static_cast<float>(widget_->width() * 0.5), static_cast<float>(widget_->height() * 0.5) }, zoomLevel_);
  requestRender();
 }});
 commandEntries_.push_back({QStringLiteral("final"), QStringLiteral("View final output"), {QStringLiteral("color"), QStringLiteral("display final")}, [this]() {
  if (!widget_) return;
  static_cast<ArtifactLayerEditorWidgetV2*>(widget_)->setDisplayMode(DisplayMode::Color);
 }});
 commandEntries_.push_back({QStringLiteral("source"), QStringLiteral("View source / overlay output"), {QStringLiteral("wireframe"), QStringLiteral("alpha")}, [this]() {
  if (!widget_) return;
  static_cast<ArtifactLayerEditorWidgetV2*>(widget_)->setDisplayMode(DisplayMode::Wireframe);
 }});
 commandEntries_.push_back({QStringLiteral("view"), QStringLiteral("Return to normal view mode"), {QStringLiteral("normal"), QStringLiteral("view mode")}, [this]() {
  if (!widget_) return;
  static_cast<ArtifactLayerEditorWidgetV2*>(widget_)->setEditMode(EditMode::View);
 }});
 commandEntries_.push_back({QStringLiteral("mask"), QStringLiteral("Enter mask edit mode"), {QStringLiteral("mask mode"), QStringLiteral("roto")}, [this]() {
  if (!widget_) return;
  static_cast<ArtifactLayerEditorWidgetV2*>(widget_)->setEditMode(EditMode::Mask);
 }});

 commandList_->clear();
 const QString needle = text.trimmed().toLower();
 for (const auto& entry : commandEntries_) {
  const QString haystack = (entry.key + QStringLiteral(" ") + entry.description + QStringLiteral(" ") + entry.aliases.join(QStringLiteral(" "))).toLower();
  if (!needle.isEmpty() && !haystack.contains(needle)) {
   continue;
  }
  auto* item = new QListWidgetItem(QStringLiteral("%1  -  %2").arg(entry.key, entry.description), commandList_);
  item->setData(Qt::UserRole, entry.key);
 }
 if (commandList_->count() > 0) {
  commandList_->setCurrentRow(0);
 } else {
  auto* item = new QListWidgetItem(QStringLiteral("No matching commands"), commandList_);
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
 }
}

bool ArtifactLayerEditorWidgetV2::Impl::executeCommandText(const QString& text)
{
 const QString needle = text.trimmed().toLower();
 if (needle.isEmpty()) {
  return false;
 }
 for (const auto& entry : commandEntries_) {
  if (entry.key.compare(needle, Qt::CaseInsensitive) == 0 ||
      entry.aliases.contains(needle, Qt::CaseInsensitive) ||
      entry.description.compare(needle, Qt::CaseInsensitive) == 0) {
   if (entry.action) {
    entry.action();
    return true;
   }
  }
 }
 for (const auto& entry : commandEntries_) {
  const QString haystack = (entry.key + QStringLiteral(" ") + entry.description + QStringLiteral(" ") + entry.aliases.join(QStringLiteral(" "))).toLower();
  if (haystack.contains(needle)) {
   if (entry.action) {
    entry.action();
    return true;
   }
  }
 }
 return false;
}

 void ArtifactLayerEditorWidgetV2::Impl::initialize(QWidget* window)
 {
  widget_ = window;
 if (resizeDebounceTimer_) {
   resizeDebounceTimer_->setParent(window);
  }
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
  renderThrottleClock_.start();
  lastRenderFrameAtMs_ = 0;
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
  if (resizeDebounceTimer_) {
   resizeDebounceTimer_->stop();
  }
  if (renderer_) {
   renderer_->destroy();
   renderer_.reset();
  }
  m_maskPreviewRT = nullptr;
  m_maskPreviewWidth = 0;
  m_maskPreviewHeight = 0;
  compositionRenderer_.reset();
  initialized_ = false;
  resizePending_ = false;
  pendingResizeSize_ = QSize();
 }

 void ArtifactLayerEditorWidgetV2::Impl::beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer || maskEditPending_) {
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

 void ArtifactLayerEditorWidgetV2::Impl::markMaskEditDirty()
 {
  if (maskEditPending_) {
   maskEditDirty_ = true;
  }
 }

 void ArtifactLayerEditorWidgetV2::Impl::commitMaskEditTransaction()
 {
  if (!maskEditPending_) {
   return;
  }

  auto layer = maskEditLayer_.lock();
  maskEditPending_ = false;
  maskEditLayer_.reset();

  if (!layer || !maskEditDirty_) {
   maskEditBefore_.clear();
   maskEditDirty_ = false;
   return;
  }

  std::vector<LayerMask> afterMasks;
  afterMasks.reserve(static_cast<size_t>(layer->maskCount()));
  for (int i = 0; i < layer->maskCount(); ++i) {
   afterMasks.push_back(layer->mask(i));
  }

  if (auto* undo = UndoManager::instance()) {
   undo->push(std::make_unique<MaskEditCommand>(layer, maskEditBefore_, std::move(afterMasks)));
  }

  maskEditBefore_.clear();
  maskEditDirty_ = false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
  if (!event || !renderer_ || !widget_) {
   return;
  }

  if (editMode_ == EditMode::Mask && !targetLayerId_.isNil() &&
      (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(targetLayerId_)) {
      const int maskIndex = draggingMaskIndex_ >= 0 ? draggingMaskIndex_ : hoveredMaskIndex_;
      const int pathIndex = draggingPathIndex_ >= 0 ? draggingPathIndex_ : hoveredPathIndex_;
      const int vertexIndex = draggingVertexIndex_ >= 0 ? draggingVertexIndex_ : hoveredVertexIndex_;
      if (maskIndex >= 0 && pathIndex >= 0 && vertexIndex >= 0 && maskIndex < layer->maskCount()) {
       beginMaskEditTransaction(layer);
       LayerMask mask = layer->mask(maskIndex);
       if (pathIndex < mask.maskPathCount()) {
        MaskPath path = mask.maskPath(pathIndex);
        if (vertexIndex < path.vertexCount()) {
         path.removeVertex(vertexIndex);
         mask.setMaskPath(pathIndex, path);
         layer->setMask(maskIndex, mask);
         markMaskEditDirty();
         hoveredMaskIndex_ = hoveredPathIndex_ = hoveredVertexIndex_ = -1;
         draggingMaskIndex_ = draggingPathIndex_ = draggingVertexIndex_ = -1;
         layer->changed();
         widget_->update();
         event->accept();
         return;
        }
       }
      }
     }
    }
   }
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

  if (auto* playback = ArtifactPlaybackService::instance()) {
   const auto playbackState = playback->state();
   parts << QStringLiteral("Playback=%1")
                .arg(playbackState == PlaybackState::Playing
                         ? QStringLiteral("Playing")
                         : playbackState == PlaybackState::Paused
                               ? QStringLiteral("Paused")
                               : QStringLiteral("Stopped"));
   parts << QStringLiteral("Frame=%1").arg(playback->currentFrame().framePosition());
  }

  if (!targetLayerId_.isNil()) {
   parts << QStringLiteral("Layer=%1").arg(targetLayerId_.toString());
  }
  if (editMode_ == EditMode::Mask) {
   parts << QStringLiteral("MaskEdit=%1")
                .arg(draggingVertexIndex_ >= 0
                         ? QStringLiteral("Dragging")
                         : hoveredVertexIndex_ >= 0
                               ? QStringLiteral("Hover")
                               : QStringLiteral("Idle"));
  }
  debugText_ = parts.join(QStringLiteral(" | "));
}

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChainInternal(QWidget* window)
 {

 }

 bool ArtifactLayerEditorWidgetV2::Impl::ensureMaskPreviewRT(int width, int height)
 {
  if (!renderer_ || width <= 0 || height <= 0) {
   return false;
  }

  const Uint32 newWidth = static_cast<Uint32>(width);
  const Uint32 newHeight = static_cast<Uint32>(height);
  if (m_maskPreviewRT && m_maskPreviewWidth == newWidth && m_maskPreviewHeight == newHeight) {
   return true;
  }

  auto device = renderer_->device();
  if (!device) {
   return false;
  }

  m_maskPreviewRT = nullptr;

  TextureDesc desc;
  desc.Name = "MaskPreviewRT";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = newWidth;
  desc.Height = newHeight;
  desc.MipLevels = 1;
  desc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

  device->CreateTexture(desc, nullptr, &m_maskPreviewRT);
  if (!m_maskPreviewRT) {
   m_maskPreviewWidth = 0;
   m_maskPreviewHeight = 0;
   return false;
  }

  m_maskPreviewWidth = newWidth;
  m_maskPreviewHeight = newHeight;
  return true;
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
      if (compSize.width() > 0 && compSize.height() > 0) {
       renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
       compositionRenderer_->SetCompositionSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
      }
     }
    }
    compositionRenderer_->ApplyCompositionSpace();
    if (viewSurfaceMode_ == ViewSurfaceMode::Final || viewSurfaceMode_ == ViewSurfaceMode::BeforeAfter) {
     compositionRenderer_->DrawCompositionBackground(clearColor_);
    }
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
       const bool useMaskPreview = false;
       if (useMaskPreview) {
        const int width = std::max(1, static_cast<int>(std::lround(widget_->width() * widget_->devicePixelRatioF())));
        const int height = std::max(1, static_cast<int>(std::lround(widget_->height() * widget_->devicePixelRatioF())));
        if (ensureMaskPreviewRT(width, height)) {
         auto* previewRTV = m_maskPreviewRT ? m_maskPreviewRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : nullptr;
         auto* previewSRV = m_maskPreviewRT ? m_maskPreviewRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) : nullptr;
         if (previewRTV && previewSRV) {
          const FloatColor savedClearColor = renderer_->getClearColor();
          const float savedZoom = renderer_->getZoom();
          float savedPanX = 0.0f;
          float savedPanY = 0.0f;
          renderer_->getPan(savedPanX, savedPanY);

          renderer_->setOverrideRTV(previewRTV);
          renderer_->setClearColor(FloatColor{0.0f, 0.0f, 0.0f, 0.0f});
          renderer_->clear();
          layer->draw(renderer_.get());
          renderer_->setOverrideRTV(nullptr);
          renderer_->setClearColor(savedClearColor);
          renderer_->flushAndWait();

          ArtifactCore::ImageF32x4_RGBA maskBuffer(ArtifactCore::FloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
          maskBuffer.resize(width, height);
          auto maskMat = maskBuffer.toCVMat();
          for (int m = 0; m < layer->maskCount(); ++m) {
           const LayerMask mask = layer->mask(m);
           if (!mask.isEnabled()) {
            continue;
           }
           mask.applyToImage(maskMat.cols, maskMat.rows, &maskMat);
          }
          const QImage maskOverlay = ArtifactCore::CvUtils::cvMatToQImage(maskMat);

          renderer_->clear();
          renderer_->setCanvasSize(static_cast<float>(width), static_cast<float>(height));
          renderer_->setZoom(1.0f);
          renderer_->setPan(0.0f, 0.0f);
          renderer_->drawMaskedTextureLocal(0.0f, 0.0f,
                                            static_cast<float>(width),
                                            static_cast<float>(height),
                                            previewSRV,
                                            maskOverlay,
                                            1.0f);
          if (compSize.width() > 0 && compSize.height() > 0) {
           renderer_->setCanvasSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
          }
          renderer_->setClearColor(savedClearColor);
          renderer_->setZoom(savedZoom);
          renderer_->setPan(savedPanX, savedPanY);
         } else {
          layer->draw(renderer_.get());
         }
        } else {
         layer->draw(renderer_.get());
        }
       } else {
        layer->draw(renderer_.get());
       }
        if (editMode_ == EditMode::Mask && layer->maskCount() > 0) {
         const float zoom = std::max(0.001f, renderer_->getZoom());
         const float hitRadius = std::max(4.0f, 7.0f / zoom);
         const QTransform globalTransform = layer->getGlobalTransform();
         auto toCanvas = [&](const QPointF& localPos) -> Detail::float2 {
          const QPointF canvasPos = globalTransform.map(localPos);
          return {static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y())};
         };

         const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
         const FloatColor maskLineColor = {0.26f, 0.84f, 0.96f, 0.96f};
         const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
         const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
         const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
         const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

         for (int m = 0; m < layer->maskCount(); ++m) {
          const LayerMask mask = layer->mask(m);
          if (!mask.isEnabled()) {
           continue;
          }
          for (int p = 0; p < mask.maskPathCount(); ++p) {
           const MaskPath path = mask.maskPath(p);
           const int vertexCount = path.vertexCount();
           if (vertexCount <= 0) {
            continue;
           }

           struct VertexMarker {
            Detail::float2 pos;
            FloatColor color;
            float radius;
           };
           std::vector<VertexMarker> markers;
           markers.reserve(static_cast<size_t>(vertexCount));

           Detail::float2 lastCanvasPos;
           for (int v = 0; v < vertexCount; ++v) {
            const MaskVertex vertex = path.vertex(v);
            const Detail::float2 currentCanvasPos = toCanvas(vertex.position);
            if (v > 0) {
             renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 6.0f, maskLineShadowColor);
             renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 3.5f, maskLineColor);
            }

            FloatColor currentColor = maskPointColor;
            float currentPointRadius = 17.0f;
            if (draggingMaskIndex_ == m && draggingPathIndex_ == p &&
                draggingVertexIndex_ == v) {
             currentColor = dragColor;
             currentPointRadius = 21.0f;
            } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p &&
                       hoveredVertexIndex_ == v) {
             currentColor = hoverColor;
             currentPointRadius = 21.0f;
            }
            markers.push_back({currentCanvasPos, currentColor, currentPointRadius});
            lastCanvasPos = currentCanvasPos;
           }

           if (path.isClosed() && vertexCount > 1) {
            const Detail::float2 firstCanvasPos = toCanvas(path.vertex(0).position);
            renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 7.0f, maskLineShadowColor);
            renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 4.0f, maskLineColor);
           }

           for (const auto& marker : markers) {
            renderer_->drawPoint(marker.pos.x, marker.pos.y, marker.radius + 3.0f, maskPointShadowColor);
            renderer_->drawPoint(marker.pos.x, marker.pos.y, marker.radius, marker.color);
           }
          }
         }
        }

        // Effect partial application visualization (Rect/Mask region overlay)
        const auto effects = layer->getEffects();
        for (const auto& effect : effects) {
         if (!effect || !effect->isEnabled()) continue;
         if (effect->pipelineStage() != EffectPipelineStage::Rasterizer) continue;

         const bool hasRectRestriction = effect->hasRectRestriction();
         const bool hasMaskRestriction = effect->hasMaskRestriction();

         if (hasRectRestriction) {
          const QRectF effectRect = effect->effectRect();
          const QPointF topLeft = globalTransform.map(effectRect.topLeft());
          const QPointF bottomRight = globalTransform.map(effectRect.bottomRight());
          const float w = static_cast<float>(bottomRight.x() - topLeft.x());
          const float h = static_cast<float>(bottomRight.y() - topLeft.y());

          // Draw effect region overlay with dashed border
          const FloatColor effectRegionColor = {0.35f, 0.85f, 0.35f, 0.6f};
          const FloatColor effectBorderColor = {0.35f, 0.85f, 0.35f, 0.9f};
          renderer_->drawRectLocal(static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y()),
                                   w, h, effectRegionColor, 0.3f);
          renderer_->drawRectOutline(static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y()),
                                     w, h, effectBorderColor);
         }

         if (hasMaskRestriction) {
          // Draw mask regions that this effect is limited to
          for (int m = 0; m < layer->maskCount(); ++m) {
           const LayerMask mask = layer->mask(m);
           if (!mask.isEnabled()) continue;
           for (int p = 0; p < mask.maskPathCount(); ++p) {
            const MaskPath path = mask.maskPath(p);
            const int vertexCount = path.vertexCount();
            if (vertexCount < 2) continue;

            const FloatColor effectMaskColor = {0.85f, 0.65f, 0.25f, 0.8f};
            Detail::float2 lastPos;
            for (int v = 0; v < vertexCount; ++v) {
             const MaskVertex vertex = path.vertex(v);
             const Detail::float2 currentPos = toCanvas(vertex.position);
             if (v > 0) {
              renderer_->drawThickLineLocal(lastPos, currentPos, 3.0f, effectMaskColor);
             }
             lastPos = currentPos;
            }
            if (path.isClosed()) {
             const Detail::float2 firstPos = toCanvas(path.vertex(0).position);
             renderer_->drawThickLineLocal(lastPos, firstPos, 3.0f, effectMaskColor);
            }
           }
          }
         }
        }
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
  renderer_->flushAndWait();
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

void ArtifactLayerEditorWidgetV2::Impl::toggleImmersiveMode(bool immersive)
{
 if (!widget_) {
  return;
 }

 immersiveMode_ = immersive;
 if (auto* mw = qobject_cast<ArtifactMainWindow*>(widget_->window())) {
  mw->setDockImmersive(widget_, immersive);
 } else if (auto* topLevel = widget_->window()) {
  if (immersive) {
   topLevel->showFullScreen();
  } else {
   topLevel->showNormal();
  }
 }
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
  if (!impl_->isPlay_ && impl_->editMode_ == EditMode::Mask) {
   const qint64 nowMs = impl_->renderThrottleClock_.isValid() ? impl_->renderThrottleClock_.elapsed() : 0;
   if (impl_->lastRenderFrameAtMs_ > 0 && (nowMs - impl_->lastRenderFrameAtMs_) < 33) {
    return;
   }
   impl_->lastRenderFrameAtMs_ = nowMs;
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
    QObject::connect(service, &ArtifactProjectService::compositionCreated, this, [this](const ArtifactCore::CompositionID&) {
     // Composition が作成されたら現在の選択レイヤーを追従
     if (auto comp = service->currentComposition().lock()) {
      const auto layers = comp->allLayer();
      for (const auto& layer : layers) {
       if (layer && layer->isSolo()) {
        setTargetLayer(layer->id());
        return;
       }
      }
     }
    });
    QObject::connect(service, &ArtifactProjectService::currentCompositionChanged, this, [this]() {
     // Composition 切り替え時に target layer を再解決
     const auto targetId = impl_->targetLayerId_;
     if (targetId.isNil()) {
      return;
     }
     if (auto comp = service->currentComposition().lock()) {
      if (comp->containsLayerById(targetId)) {
       setTargetLayer(targetId);
       return;
      }
     }
     // 現在の composition にレイヤーがなければクリア
     clearTargetLayer();
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
    });
    QObject::connect(service, &ArtifactProjectService::compositionCreated, this, [this](const ArtifactCore::CompositionID&) {
     // Composition が作成されたら現在の選択レイヤーを追従
     if (auto comp = service->currentComposition().lock()) {
      const auto layers = comp->allLayer();
      for (const auto& layer : layers) {
       if (layer && layer->isSolo()) {
        setTargetLayer(layer->id());
        return;
       }
      }
     }
    });
    QObject::connect(service, &ArtifactProjectService::currentCompositionChanged, this, [this]() {
     // Composition 切り替え時に target layer を再解決
     const auto targetId = impl_->targetLayerId_;
     if (targetId.isNil()) {
      return;
     }
     if (auto comp = service->currentComposition().lock()) {
      if (comp->containsLayerById(targetId)) {
       setTargetLayer(targetId);
       return;
      }
     }
     // 現在の composition にレイヤーがなければクリア
     clearTargetLayer();
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

  auto* immersiveShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
  QObject::connect(immersiveShortcut, &QShortcut::activated, this, [this]() {
   if (impl_) {
    impl_->toggleImmersiveMode(!impl_->immersiveMode_);
   }
  });
  auto* immersiveExitShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  QObject::connect(immersiveExitShortcut, &QShortcut::activated, this, [this]() {
   if (impl_ && impl_->immersiveMode_) {
    impl_->toggleImmersiveMode(false);
   }
  });
  }

void ArtifactLayerEditorWidgetV2::clearTargetLayer()
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 impl_->targetLayerId_ = LayerID();
 impl_->hoveredMaskIndex_ = -1;
 impl_->hoveredPathIndex_ = -1;
 impl_->hoveredVertexIndex_ = -1;
 impl_->draggingMaskIndex_ = -1;
 impl_->draggingPathIndex_ = -1;
 impl_->draggingVertexIndex_ = -1;
 impl_->requestRender();
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
 if (event && event->key() == Qt::Key_F11 && !event->isAutoRepeat()) {
  if (impl_) {
   impl_->toggleImmersiveMode(!impl_->immersiveMode_);
  }
  event->accept();
  return;
 }
 if (event && event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
  impl_->showCommandPalette();
  event->accept();
  return;
 }
 if (impl_ && impl_->immersiveMode_ && event && event->key() == Qt::Key_Escape) {
  impl_->toggleImmersiveMode(false);
  event->accept();
  return;
 }
 if (impl_ && impl_->isCommandPaletteVisible() && event && event->key() == Qt::Key_Escape) {
  impl_->hideCommandPalette();
  event->accept();
  return;
 }
 impl_->defaultHandleKeyPressEvent(event);
 impl_->requestRender();
}

 void ArtifactLayerEditorWidgetV2::keyReleaseEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyReleaseEvent(event);
 }

void ArtifactLayerEditorWidgetV2::mousePressEvent(QMouseEvent* event)
{
  setFocus(Qt::MouseFocusReason);
  if (event->button() == Qt::LeftButton && impl_->editMode_ == EditMode::Mask && !impl_->targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(impl_->targetLayerId_)) {
      impl_->beginMaskEditTransaction(layer);
      const auto canvasPos = impl_->renderer_
                                 ? impl_->renderer_->viewportToCanvas(
                                       {(float)event->position().x(), (float)event->position().y()})
                                 : Detail::float2{(float)event->position().x(),
                                                  (float)event->position().y()};
      const QTransform globalTransform = layer->getGlobalTransform();
      bool invertible = false;
      const QTransform invTransform = globalTransform.inverted(&invertible);
      if (invertible) {
       const QPointF localPos = invTransform.map(QPointF(canvasPos.x, canvasPos.y));
       const float zoom = impl_->renderer_ ? std::max(0.001f, impl_->renderer_->getZoom()) : 1.0f;
       const float threshold = 8.0f / zoom;
       impl_->hoveredMaskIndex_ = -1;
       impl_->hoveredPathIndex_ = -1;
       impl_->hoveredVertexIndex_ = -1;

       for (int m = 0; m < layer->maskCount(); ++m) {
        LayerMask mask = layer->mask(m);
        for (int p = 0; p < mask.maskPathCount(); ++p) {
         MaskPath path = mask.maskPath(p);
         for (int v = 0; v < path.vertexCount(); ++v) {
          const MaskVertex vertex = path.vertex(v);
          if (std::hypot(vertex.position.x() - localPos.x(),
                         vertex.position.y() - localPos.y()) <= threshold) {
           if (v == 0 && !path.isClosed() && path.vertexCount() > 2) {
            path.setClosed(true);
            mask.setMaskPath(p, path);
            const bool appendNewPath = (p == mask.maskPathCount() - 1);
            if (appendNewPath) {
             mask.addMaskPath(MaskPath());
            }
            layer->setMask(m, mask);
            impl_->draggingMaskIndex_ = m;
            impl_->draggingPathIndex_ = p;
            impl_->draggingVertexIndex_ = -1;
            impl_->hoveredMaskIndex_ = m;
            impl_->hoveredPathIndex_ = p;
            impl_->hoveredVertexIndex_ = -1;
            impl_->markMaskEditDirty();
            layer->changed();
            setCursor(Qt::CrossCursor);
            impl_->requestRender();
            event->accept();
            return;
           }
           impl_->draggingMaskIndex_ = m;
           impl_->draggingPathIndex_ = p;
           impl_->draggingVertexIndex_ = v;
           impl_->hoveredMaskIndex_ = m;
           impl_->hoveredPathIndex_ = p;
           impl_->hoveredVertexIndex_ = v;
           impl_->requestRender();
           event->accept();
           return;
          }
         }
        }
       }

       if (layer->maskCount() == 0) {
        layer->addMask(LayerMask());
       }

       LayerMask mask = layer->mask(0);
       if (mask.maskPathCount() == 0) {
        MaskPath path;
        path.setClosed(false);
        mask.addMaskPath(path);
       }

       MaskPath path = mask.maskPath(0);
       if (path.isClosed() && path.vertexCount() > 0) {
        MaskPath newPath;
        newPath.setClosed(false);
        mask.addMaskPath(newPath);
        path = mask.maskPath(mask.maskPathCount() - 1);
       } else if (path.vertexCount() == 0) {
        path.setClosed(false);
       }

       MaskVertex vertex;
       vertex.position = localPos;
       vertex.inTangent = QPointF(0, 0);
       vertex.outTangent = QPointF(0, 0);
       path.addVertex(vertex);
       mask.setMaskPath(0, path);
       layer->setMask(0, mask);
       impl_->markMaskEditDirty();
       impl_->hoveredMaskIndex_ = 0;
       impl_->hoveredPathIndex_ = 0;
       impl_->hoveredVertexIndex_ = path.vertexCount() - 1;
       layer->changed();
       setCursor(Qt::CrossCursor);
       impl_->requestRender();
       event->accept();
       return;
      }
     }
    }
   }
  }

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
 if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton &&
     impl_->draggingVertexIndex_ >= 0) {
   impl_->markMaskEditDirty();
   impl_->draggingMaskIndex_ = -1;
   impl_->draggingPathIndex_ = -1;
   impl_->draggingVertexIndex_ = -1;
   impl_->commitMaskEditTransaction();
   impl_->requestRender();
   setCursor(Qt::CrossCursor);
   event->accept();
   return;
 }

 if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton) {
  impl_->commitMaskEditTransaction();
  impl_->requestRender();
  setCursor(Qt::CrossCursor);
  event->accept();
  return;
 }

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
 if (event->button() == Qt::LeftButton && impl_->editMode_ == EditMode::Mask && !impl_->targetLayerId_.isNil()) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    if (auto layer = composition->layerById(impl_->targetLayerId_)) {
     impl_->beginMaskEditTransaction(layer);
     if (impl_->draggingMaskIndex_ >= 0 && impl_->draggingPathIndex_ >= 0) {
      LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
      MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
      if (path.vertexCount() > 2) {
       path.setClosed(true);
       mask.setMaskPath(impl_->draggingPathIndex_, path);
       mask.addMaskPath(MaskPath());
       layer->setMask(impl_->draggingMaskIndex_, mask);
       impl_->markMaskEditDirty();
       layer->changed();
       setCursor(Qt::CrossCursor);
       impl_->requestRender();
       event->accept();
       return;
      }
     }
    }
   }
  }
 }

}

void ArtifactLayerEditorWidgetV2::mouseMoveEvent(QMouseEvent* event)
{
  if (impl_->editMode_ == EditMode::Mask && !impl_->targetLayerId_.isNil()) {
   if (!impl_->isPanning_) {
    setCursor(Qt::CrossCursor);
   }
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(impl_->targetLayerId_)) {
      const auto canvasPos = impl_->renderer_
                                 ? impl_->renderer_->viewportToCanvas(
                                       {(float)event->position().x(), (float)event->position().y()})
                                 : Detail::float2{(float)event->position().x(),
                                                  (float)event->position().y()};
      const QTransform globalTransform = layer->getGlobalTransform();
      bool invertible = false;
      const QTransform invTransform = globalTransform.inverted(&invertible);
      if (invertible) {
       const QPointF localPos = invTransform.map(QPointF(canvasPos.x, canvasPos.y));
       const float zoom = impl_->renderer_ ? std::max(0.001f, impl_->renderer_->getZoom()) : 1.0f;
       const float threshold = 8.0f / zoom;
       if (impl_->draggingVertexIndex_ >= 0 && impl_->draggingMaskIndex_ >= 0 && impl_->draggingPathIndex_ >= 0) {
        impl_->beginMaskEditTransaction(layer);
        LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
        MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
        MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
        vertex.position = localPos;
        path.setVertex(impl_->draggingVertexIndex_, vertex);
        mask.setMaskPath(impl_->draggingPathIndex_, path);
        layer->setMask(impl_->draggingMaskIndex_, mask);
        impl_->markMaskEditDirty();
        layer->changed();
        impl_->requestRender();
        event->accept();
        return;
       }

       int bestMask = -1;
       int bestPath = -1;
       int bestVertex = -1;
       for (int m = 0; m < layer->maskCount(); ++m) {
        LayerMask mask = layer->mask(m);
        for (int p = 0; p < mask.maskPathCount(); ++p) {
         MaskPath path = mask.maskPath(p);
         for (int v = 0; v < path.vertexCount(); ++v) {
          const MaskVertex vertex = path.vertex(v);
          if (std::hypot(vertex.position.x() - localPos.x(),
                         vertex.position.y() - localPos.y()) <= threshold) {
           bestMask = m;
           bestPath = p;
           bestVertex = v;
           break;
          }
         }
         if (bestVertex >= 0) break;
        }
        if (bestVertex >= 0) break;
       }
       const bool changed =
           bestMask != impl_->hoveredMaskIndex_ ||
           bestPath != impl_->hoveredPathIndex_ ||
           bestVertex != impl_->hoveredVertexIndex_;
       impl_->hoveredMaskIndex_ = bestMask;
       impl_->hoveredPathIndex_ = bestPath;
       impl_->hoveredVertexIndex_ = bestVertex;
       if (changed) {
        impl_->requestRender();
       }
      }
     }
    }
   }
  }

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
  if (impl_->renderer_) {
   impl_->renderer_->setViewportSize(static_cast<float>(event->size().width()),
                                     static_cast<float>(event->size().height()));
  }
  impl_->pendingResizeSize_ = event->size();
  impl_->resizePending_ = true;
  if (impl_->resizeDebounceTimer_) {
   impl_->resizeDebounceTimer_->stop();
   impl_->resizeDebounceTimer_->start(80);
  }
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
  if (impl_->editMode_ == EditMode::Mask) {
   setCursor(Qt::CrossCursor);
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
 impl_->commitMaskEditTransaction();
 impl_->destroy();
 QWidget::closeEvent(event);
}

void ArtifactLayerEditorWidgetV2::focusInEvent(QFocusEvent* event)
{
 if (impl_ && impl_->initialized_) {
  impl_->startRenderLoop();
  impl_->requestRender();
 }
 QWidget::focusInEvent(event);
}

void ArtifactLayerEditorWidgetV2::focusOutEvent(QFocusEvent* event)
{
 if (impl_) {
  impl_->isPanning_ = false;
  unsetCursor();
 }
 QWidget::focusOutEvent(event);
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
 impl_->commitMaskEditTransaction();
 impl_->targetLayerId_ = id;
 impl_->hoveredMaskIndex_ = -1;
 impl_->hoveredPathIndex_ = -1;
 impl_->hoveredVertexIndex_ = -1;
 impl_->draggingMaskIndex_ = -1;
 impl_->draggingPathIndex_ = -1;
 impl_->draggingVertexIndex_ = -1;
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
  if (impl_->editMode_ == EditMode::Mask) {
   setCursor(Qt::CrossCursor);
  }
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
  if (mode != EditMode::Mask) {
   impl_->commitMaskEditTransaction();
   impl_->hoveredMaskIndex_ = -1;
   impl_->hoveredPathIndex_ = -1;
   impl_->hoveredVertexIndex_ = -1;
   impl_->draggingMaskIndex_ = -1;
   impl_->draggingPathIndex_ = -1;
   impl_->draggingVertexIndex_ = -1;
  } else {
   setCursor(Qt::CrossCursor);
  }
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
