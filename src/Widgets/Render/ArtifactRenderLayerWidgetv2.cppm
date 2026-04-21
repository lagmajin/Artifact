module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <wobjectimpl.h>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QKeyEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QAction>
#include <QImage>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QStandardPaths>
#include <QTransform>
#include <vector>
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
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Undo.UndoManager;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
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

enum class LayerBackgroundMode {
  Alpha,
  Solid,
  MayaGradient
};

QImage makeMayaGradientSprite(const QSize& size, const FloatColor& bgColor)
{
  const int w = std::max(1, size.width());
  const int h = std::max(1, size.height());
  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  QLinearGradient grad(0.0, 0.0, 0.0, static_cast<qreal>(h));
  grad.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));
  grad.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));
  grad.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));
  grad.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));
  grad.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));
  grad.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));
  grad.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));
  grad.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));
  grad.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));
  grad.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));
  grad.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));
  painter.fillRect(image.rect(), grad);

  QLinearGradient glow(0.0, 0.0, static_cast<qreal>(w), static_cast<qreal>(h));
  QColor tint = QColor::fromRgbF(bgColor.r(), bgColor.g(), bgColor.b(), 1.0f);
  tint.setAlpha(72);
  glow.setColorAt(0.0, tint.lighter(112));
  QColor tintDark = tint.darker(140);
  tintDark.setAlpha(28);
  glow.setColorAt(1.0, tintDark);
  painter.fillRect(image.rect(), glow);
  return image;
}

QSize physicalViewportSize(const QWidget* widget)
{
 if (!widget) {
  return {};
 }
 const qreal dpr = widget->devicePixelRatio();
 return QSize(std::max(1, static_cast<int>(std::lround(widget->width() * dpr))),
              std::max(1, static_cast<int>(std::lround(widget->height() * dpr))));
}

class ShapeEditCommand final : public Artifact::UndoCommand {
public:
 ShapeEditCommand(Artifact::ArtifactAbstractLayerPtr layer,
                  std::vector<QPointF> beforePoints,
                  std::vector<QPointF> afterPoints)
     : layer_(layer),
       beforePoints_(std::move(beforePoints)),
       afterPoints_(std::move(afterPoints)) {}

 void undo() override {
  applySnapshot(beforePoints_);
 }

 void redo() override {
  applySnapshot(afterPoints_);
 }

 QString label() const override {
  return QStringLiteral("Edit Shape");
 }

private:
 void applySnapshot(const std::vector<QPointF>& points) {
  auto layer = layer_.lock();
  if (!layer) {
   return;
  }
  auto shape = std::dynamic_pointer_cast<Artifact::ArtifactShapeLayer>(layer);
  if (!shape) {
   return;
  }
  if (points.size() >= 3) {
   shape->setCustomPolygonPoints(points, true);
  } else {
   shape->clearCustomPolygonPoints();
  }
  if (auto* mgr = Artifact::UndoManager::instance()) {
   mgr->notifyAnythingChanged();
  }
 }

 Artifact::ArtifactAbstractLayerWeak layer_;
 std::vector<QPointF> beforePoints_;
 std::vector<QPointF> afterPoints_;
};

enum class MaskHandleType { None, InTangent, OutTangent };

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex, MaskHandleType handleType)
{
 const MaskVertex vertex = path.vertex(vertexIndex);
 switch (handleType) {
  case MaskHandleType::InTangent:
   return vertex.position + vertex.inTangent;
  case MaskHandleType::OutTangent:
   return vertex.position + vertex.outTangent;
  case MaskHandleType::None:
   break;
 }
 return vertex.position;
}

std::vector<QPointF> buildShapeEditSeedPoints(const ArtifactShapeLayer& shape)
{
 const float w = static_cast<float>(std::max(1, shape.shapeWidth()));
 const float h = static_cast<float>(std::max(1, shape.shapeHeight()));
 const float cx = w * 0.5f;
 const float cy = h * 0.5f;

 switch (shape.shapeType()) {
 case ShapeType::Rect:
  return {QPointF(0.0, 0.0), QPointF(w, 0.0), QPointF(w, h), QPointF(0.0, h)};
 case ShapeType::Square: {
  const float side = std::min(w, h);
  const float left = (w - side) * 0.5f;
  const float top = (h - side) * 0.5f;
  return {QPointF(left, top), QPointF(left + side, top),
          QPointF(left + side, top + side), QPointF(left, top + side)};
 }
 case ShapeType::Triangle:
  return {QPointF(cx, 0.0f), QPointF(w, h), QPointF(0.0f, h)};
 case ShapeType::Ellipse: {
  const int segments = 16;
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(segments));
  for (int i = 0; i < segments; ++i) {
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(segments);
   points.push_back(QPointF(cx + std::cos(angle) * cx,
                            cy + std::sin(angle) * cy));
  }
  return points;
 }
 case ShapeType::Star: {
  const int pts = std::max(3, shape.starPoints());
  const float outerR = std::min(cx, cy);
  const float innerR = outerR * std::clamp(shape.starInnerRadius(), 0.0f, 1.0f);
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(pts * 2));
  for (int i = 0; i < pts * 2; ++i) {
   const float angle = static_cast<float>(i) * static_cast<float>(M_PI) /
                       static_cast<float>(pts) - static_cast<float>(M_PI) * 0.5f;
   const float r = (i % 2 == 0) ? outerR : innerR;
   points.push_back(QPointF(cx + r * std::cos(angle),
                            cy + r * std::sin(angle)));
  }
  return points;
 }
 case ShapeType::Polygon: {
  const int sides = std::max(3, shape.polygonSides());
  const float r = std::min(cx, cy);
  std::vector<QPointF> points;
  points.reserve(static_cast<size_t>(sides));
  for (int i = 0; i < sides; ++i) {
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(sides) - static_cast<float>(M_PI) * 0.5f;
   points.push_back(QPointF(cx + r * std::cos(angle),
                            cy + r * std::sin(angle)));
  }
  return points;
 }
 case ShapeType::Line:
  break;
 }
 return {};
}

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos,
                       float threshold, int& maskIndex, int& pathIndex, int& vertexIndex,
                       MaskHandleType& handleType)
{
 if (!layer) {
  return false;
 }
 const QTransform globalTransform = layer->getGlobalTransform();
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF localPos = invTransform.map(canvasPos);
 const float thresholdSq = threshold * threshold;
 for (int m = 0; m < layer->maskCount(); ++m) {
  const LayerMask mask = layer->mask(m);
  if (!mask.isEnabled()) {
   continue;
  }
  for (int p = 0; p < mask.maskPathCount(); ++p) {
   const MaskPath path = mask.maskPath(p);
   for (int v = 0; v < path.vertexCount(); ++v) {
    const MaskVertex vertex = path.vertex(v);
    for (MaskHandleType candidate : {MaskHandleType::InTangent, MaskHandleType::OutTangent}) {
     if ((candidate == MaskHandleType::InTangent && vertex.inTangent == QPointF(0, 0)) ||
         (candidate == MaskHandleType::OutTangent && vertex.outTangent == QPointF(0, 0))) {
      continue;
     }
     const QPointF handlePos = maskHandlePosition(path, v, candidate);
     const QPointF delta = handlePos - localPos;
     if (QPointF::dotProduct(delta, delta) <= thresholdSq) {
      maskIndex = m;
      pathIndex = p;
      vertexIndex = v;
      handleType = candidate;
      return true;
     }
    }
   }
  }
 }
 return false;
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
  quint64 renderTickCount_ = 0;
  quint64 renderExecutedCount_ = 0;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  
  
 bool released = true;
 bool m_initialized;
 RefCntAutoPtr<ITexture> m_layerRT;
 RefCntAutoPtr<IFence> m_layer_fence;
  LayerBackgroundMode backgroundMode_ = LayerBackgroundMode::Alpha;
  EditMode editMode_ = EditMode::View;
  DisplayMode displayMode_ = DisplayMode::Color;
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
  int hoveredMaskIndex_ = -1;
  int hoveredPathIndex_ = -1;
  int hoveredVertexIndex_ = -1;
  int hoveredMaskHandleType_ = -1;
  bool maskEditPending_ = false;
  bool maskEditDirty_ = false;
  ArtifactAbstractLayerWeak maskEditLayer_;
  std::vector<LayerMask> maskEditBefore_;

  bool isDraggingMaskHandle_ = false;
  bool isDraggingShapeVertex_ = false;
  int draggingShapeVertexIndex_ = -1;
  int hoveredShapeVertexIndex_ = -1;
  bool shapeEditPending_ = false;
  bool shapeEditDirty_ = false;
  ArtifactAbstractLayerWeak shapeEditLayer_;
  std::vector<QPointF> shapeEditBefore_;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  bool isSolidLayerForPreview(const ArtifactAbstractLayerPtr& layer);
  bool tryGetSolidPreviewColor(const ArtifactAbstractLayerPtr& layer, FloatColor& outColor);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  ArtifactAbstractLayerPtr targetLayer() const;
  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markMaskEditDirty();
  void commitMaskEditTransaction();
  bool hitTestMaskVertex(const ArtifactAbstractLayerPtr& layer,
                         const QPointF& canvasPos,
                         int& maskIndex,
                         int& pathIndex,
                         int& vertexIndex) const;
  void updateMaskHover(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos);
  void drawMaskOverlay(const ArtifactAbstractLayerPtr& layer);
  bool deleteHoveredMaskVertex(const ArtifactAbstractLayerPtr& layer);

  void beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer);
  void markShapeEditDirty();
  void commitShapeEditTransaction();
  bool hitTestShapeVertex(const ArtifactAbstractLayerPtr& layer,
                          const QPointF& canvasPos,
                          int& vertexIndex) const;
  bool hitTestShapeSegment(const ArtifactAbstractLayerPtr& layer,
                           const QPointF& canvasPos,
                           int& insertIndex) const;
  void updateShapeHover(const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPos);
  void drawShapeOverlay(const ArtifactAbstractLayerPtr& layer);
  bool deleteHoveredShapeVertex(const ArtifactAbstractLayerPtr& layer);
  
  void startRenderLoop();
  void stopRenderLoop();
  void renderOneFrame();
  void refreshBackgroundCache();
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
  // Set the actual widget size so ViewportTransformer doesn't stay at the
  // default {1920, 1080} until the first resizeEvent fires.
  if (window && window->width() > 0 && window->height() > 0) {
   const QSize viewportSize = physicalViewportSize(window);
   renderer_->setViewportSize(static_cast<float>(viewportSize.width()),
                              static_cast<float>(viewportSize.height()));
  }
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

 ArtifactAbstractLayerPtr ArtifactLayerEditorWidgetV2::Impl::targetLayer() const
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

 void ArtifactLayerEditorWidgetV2::Impl::beginMaskEditTransaction(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) {
   return;
  }
  if (maskEditPending_ && maskEditLayer_.lock() == layer) {
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

void ArtifactLayerEditorWidgetV2::Impl::beginShapeEditTransaction(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return;
 }
 if (shapeEditPending_ && shapeEditLayer_.lock() == layer) {
  return;
 }
 shapeEditPending_ = true;
 shapeEditDirty_ = false;
 shapeEditLayer_ = layer;
 shapeEditBefore_.clear();
 if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
  shapeEditBefore_ = shape->customPolygonPoints();
 }
}

void ArtifactLayerEditorWidgetV2::Impl::markShapeEditDirty()
{
 if (shapeEditPending_) {
  shapeEditDirty_ = true;
 }
}

void ArtifactLayerEditorWidgetV2::Impl::commitShapeEditTransaction()
{
 if (!shapeEditPending_) {
  return;
 }
 auto layer = shapeEditLayer_.lock();
 shapeEditPending_ = false;
 shapeEditLayer_.reset();
 if (!layer || !shapeEditDirty_) {
  shapeEditBefore_.clear();
  shapeEditDirty_ = false;
  return;
 }
 std::vector<QPointF> afterPoints;
 if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
  afterPoints = shape->customPolygonPoints();
 }
 if (auto* undo = UndoManager::instance()) {
  undo->push(std::make_unique<ShapeEditCommand>(layer, shapeEditBefore_, std::move(afterPoints)));
 }
 shapeEditBefore_.clear();
 shapeEditDirty_ = false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestShapeVertex(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& vertexIndex) const
{
 if (!renderer_ || !layer) {
  return false;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) {
  return false;
 }
 const QTransform globalTransform = layer->getGlobalTransform();
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF localPos = invTransform.map(canvasPos);
 const float threshold = 8.0f / std::max(0.1f, renderer_->getZoom());
 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
  if (std::hypot(points[static_cast<size_t>(i)].x() - localPos.x(),
                 points[static_cast<size_t>(i)].y() - localPos.y()) <= threshold) {
   vertexIndex = i;
   return true;
  }
 }
 return false;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestShapeSegment(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& insertIndex) const
{
 if (!renderer_ || !layer) {
  return false;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 2) {
  return false;
 }
 const QTransform globalTransform = layer->getGlobalTransform();
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (!invertible) {
  return false;
 }
 const QPointF localPos = invTransform.map(canvasPos);
 const float threshold = 10.0f / std::max(0.1f, renderer_->getZoom());
 const auto distanceToSegment = [](const QPointF& p, const QPointF& a, const QPointF& b) -> double {
  const QPointF ab = b - a;
  const double abLenSq = ab.x() * ab.x() + ab.y() * ab.y();
  if (abLenSq <= std::numeric_limits<double>::epsilon()) {
   return std::hypot(p.x() - a.x(), p.y() - a.y());
  }
  const QPointF ap = p - a;
  const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / abLenSq, 0.0, 1.0);
  const QPointF proj = a + ab * t;
  return std::hypot(p.x() - proj.x(), p.y() - proj.y());
 };

 const bool closed = shape->customPolygonClosed();
 const int segmentCount = closed ? static_cast<int>(points.size())
                                 : static_cast<int>(points.size()) - 1;
 double bestDistance = std::numeric_limits<double>::max();
 int bestIndex = -1;
 for (int i = 0; i < segmentCount; ++i) {
  const int next = (i + 1) % static_cast<int>(points.size());
  const double distance = distanceToSegment(localPos,
                                            points[static_cast<size_t>(i)],
                                            points[static_cast<size_t>(next)]);
  if (distance < bestDistance) {
   bestDistance = distance;
   bestIndex = i;
  }
 }
 if (bestIndex >= 0 && bestDistance <= threshold) {
  insertIndex = bestIndex + 1;
  return true;
 }
 return false;
}

void ArtifactLayerEditorWidgetV2::Impl::updateShapeHover(const ArtifactAbstractLayerPtr& layer,
                                                         const QPointF& canvasPos)
{
 hoveredShapeVertexIndex_ = -1;
 int vertexIndex = -1;
 if (hitTestShapeVertex(layer, canvasPos, vertexIndex)) {
  hoveredShapeVertexIndex_ = vertexIndex;
 }
}

void ArtifactLayerEditorWidgetV2::Impl::drawShapeOverlay(const ArtifactAbstractLayerPtr& layer)
{
 if (!renderer_ || !layer) {
  return;
 }
 const auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return;
 }
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) {
  return;
 }

 const QTransform globalTransform = layer->getGlobalTransform();
 const FloatColor outlineShadow = {0.0f, 0.0f, 0.0f, 0.30f};
 const FloatColor outlineColor = {0.98f, 0.72f, 0.28f, 0.96f};
 const FloatColor pointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
 const FloatColor pointColor = {0.98f, 0.99f, 1.0f, 1.0f};
 const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
 const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};

 Detail::float2 firstCanvasPos{};
 Detail::float2 lastCanvasPos{};
 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
  const QPointF canvasPos = globalTransform.map(points[static_cast<size_t>(i)]);
  const Detail::float2 currentCanvasPos{static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y())};
  if (i > 0) {
   renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 6.0f, outlineShadow);
   renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 3.5f, outlineColor);
  } else {
   firstCanvasPos = currentCanvasPos;
  }
  lastCanvasPos = currentCanvasPos;
 }
 renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 6.0f, outlineShadow);
 renderer_->drawThickLineLocal(lastCanvasPos, firstCanvasPos, 3.5f, outlineColor);

 for (int i = 0; i < static_cast<int>(points.size()); ++i) {
  const QPointF canvasPos = globalTransform.map(points[static_cast<size_t>(i)]);
  FloatColor currentColor = pointColor;
  float currentRadius = 16.0f;
  if (isDraggingShapeVertex_ && draggingShapeVertexIndex_ == i) {
   currentColor = dragColor;
   currentRadius = 20.0f;
  } else if (hoveredShapeVertexIndex_ == i) {
   currentColor = hoverColor;
   currentRadius = 18.0f;
  }
  renderer_->drawCircle(static_cast<float>(canvasPos.x()),
                        static_cast<float>(canvasPos.y()),
                        currentRadius + 4.0f,
                        pointShadowColor, 1.0f, true);
  renderer_->drawCircle(static_cast<float>(canvasPos.x()),
                        static_cast<float>(canvasPos.y()),
                        currentRadius,
                        currentColor, 1.0f, true);
 }
}

bool ArtifactLayerEditorWidgetV2::Impl::deleteHoveredShapeVertex(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || hoveredShapeVertexIndex_ < 0) {
  return false;
 }
 auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
 if (!shape || !shape->hasCustomPolygon()) {
  return false;
 }
 auto points = shape->customPolygonPoints();
 if (hoveredShapeVertexIndex_ < 0 || hoveredShapeVertexIndex_ >= static_cast<int>(points.size())) {
  return false;
 }
 beginShapeEditTransaction(layer);
 points.erase(points.begin() + hoveredShapeVertexIndex_);
 if (points.size() >= 3) {
  shape->setCustomPolygonPoints(points, true);
 } else {
  shape->clearCustomPolygonPoints();
 }
 markShapeEditDirty();
 hoveredShapeVertexIndex_ = -1;
 return true;
}

bool ArtifactLayerEditorWidgetV2::Impl::hitTestMaskVertex(const ArtifactAbstractLayerPtr& layer,
                                                           const QPointF& canvasPos,
                                                           int& maskIndex,
                                                           int& pathIndex,
                                                           int& vertexIndex) const
 {
  if (!renderer_ || !layer) {
   return false;
  }
  const QTransform globalTransform = layer->getGlobalTransform();
  bool invertible = false;
  const QTransform invTransform = globalTransform.inverted(&invertible);
  if (!invertible) {
   return false;
  }
  const QPointF localPos = invTransform.map(canvasPos);
  const float threshold = 8.0f / std::max(0.1f, renderer_->getZoom());
  for (int m = 0; m < layer->maskCount(); ++m) {
   const LayerMask mask = layer->mask(m);
   for (int p = 0; p < mask.maskPathCount(); ++p) {
    const MaskPath path = mask.maskPath(p);
    for (int v = 0; v < path.vertexCount(); ++v) {
     const MaskVertex vertex = path.vertex(v);
     if (std::hypot(vertex.position.x() - localPos.x(), vertex.position.y() - localPos.y()) <= threshold) {
      maskIndex = m;
      pathIndex = p;
      vertexIndex = v;
      return true;
     }
    }
   }
  }
  return false;
 }

 void ArtifactLayerEditorWidgetV2::Impl::updateMaskHover(const ArtifactAbstractLayerPtr& layer,
                                                         const QPointF& canvasPos)
 {
 hoveredMaskIndex_ = -1;
 hoveredPathIndex_ = -1;
 hoveredVertexIndex_ = -1;
 hoveredMaskHandleType_ = -1;
 int maskIndex = -1;
 int pathIndex = -1;
 int vertexIndex = -1;
 MaskHandleType handleType = MaskHandleType::None;
 const float handleThreshold = 10.0f / std::max(0.1f, renderer_->getZoom());
 if (hitTestMaskHandle(layer, canvasPos, handleThreshold,
                       maskIndex, pathIndex, vertexIndex, handleType)) {
  hoveredMaskIndex_ = maskIndex;
  hoveredPathIndex_ = pathIndex;
  hoveredVertexIndex_ = vertexIndex;
  hoveredMaskHandleType_ = static_cast<int>(handleType);
 } else if (hitTestMaskVertex(layer, canvasPos, maskIndex, pathIndex, vertexIndex)) {
  hoveredMaskIndex_ = maskIndex;
  hoveredPathIndex_ = pathIndex;
  hoveredVertexIndex_ = vertexIndex;
 }
}

 void ArtifactLayerEditorWidgetV2::Impl::drawMaskOverlay(const ArtifactAbstractLayerPtr& layer)
 {
  if (!renderer_ || !layer || layer->maskCount() <= 0) {
   return;
  }

  const QTransform globalTransform = layer->getGlobalTransform();
  const FloatColor maskLineShadowColor = {0.0f, 0.0f, 0.0f, 0.30f};
  const FloatColor maskLineColor = {0.26f, 0.84f, 0.96f, 0.96f};
  const FloatColor maskPointShadowColor = {0.0f, 0.0f, 0.0f, 0.42f};
  const FloatColor maskPointColor = {0.97f, 0.99f, 1.0f, 1.0f};
  const FloatColor hoverColor = {1.0f, 0.76f, 0.28f, 1.0f};
  const FloatColor dragColor = {1.0f, 0.40f, 0.24f, 1.0f};
  const FloatColor handleLineColor = {0.74f, 0.82f, 0.92f, 0.55f};
  const FloatColor handlePointColor = {0.70f, 0.90f, 1.0f, 0.95f};
  const FloatColor handleHoverColor = {1.0f, 0.78f, 0.32f, 1.0f};
  const FloatColor handleDragColor = {1.0f, 0.44f, 0.24f, 1.0f};

  for (int m = 0; m < layer->maskCount(); ++m) {
   LayerMask mask = layer->mask(m);
   if (!mask.isEnabled()) {
    continue;
   }
   for (int p = 0; p < mask.maskPathCount(); ++p) {
    MaskPath path = mask.maskPath(p);
    const int vertexCount = path.vertexCount();
    if (vertexCount == 0) {
     continue;
    }

    Detail::float2 lastCanvasPos{};
    for (int v = 0; v < vertexCount; ++v) {
     MaskVertex vertex = path.vertex(v);
     QPointF canvasPos = globalTransform.map(vertex.position);
     Detail::float2 currentCanvasPos = {(float)canvasPos.x(), (float)canvasPos.y()};
     const QPointF inHandlePos = globalTransform.map(vertex.position + vertex.inTangent);
     const QPointF outHandlePos = globalTransform.map(vertex.position + vertex.outTangent);
     const Detail::float2 inHandleCanvas = {(float)inHandlePos.x(), (float)inHandlePos.y()};
     const Detail::float2 outHandleCanvas = {(float)outHandlePos.x(), (float)outHandlePos.y()};

     if (vertex.inTangent != QPointF(0, 0)) {
      renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, 4.0f, maskLineShadowColor);
      renderer_->drawThickLineLocal(currentCanvasPos, inHandleCanvas, 2.0f, handleLineColor);
      FloatColor handleColor = handlePointColor;
      if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
          draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       handleColor = handleDragColor;
      } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                 hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       handleColor = handleHoverColor;
      }
      renderer_->drawPoint(inHandleCanvas.x, inHandleCanvas.y, 10.0f, maskPointShadowColor);
      renderer_->drawPoint(inHandleCanvas.x, inHandleCanvas.y, 6.5f, handleColor);
     }
     if (vertex.outTangent != QPointF(0, 0)) {
      renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, 4.0f, maskLineShadowColor);
      renderer_->drawThickLineLocal(currentCanvasPos, outHandleCanvas, 2.0f, handleLineColor);
      FloatColor handleColor = handlePointColor;
      if (isDraggingMaskHandle_ && draggingMaskIndex_ == m && draggingPathIndex_ == p &&
          draggingVertexIndex_ == v && draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       handleColor = handleDragColor;
      } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v &&
                 hoveredMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       handleColor = handleHoverColor;
      }
      renderer_->drawPoint(outHandleCanvas.x, outHandleCanvas.y, 10.0f, maskPointShadowColor);
      renderer_->drawPoint(outHandleCanvas.x, outHandleCanvas.y, 6.5f, handleColor);
     }
     if (v > 0) {
      renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 6.0f, maskLineShadowColor);
      renderer_->drawThickLineLocal(lastCanvasPos, currentCanvasPos, 3.5f, maskLineColor);
     }
     lastCanvasPos = currentCanvasPos;
    }

    if (path.isClosed() && vertexCount > 1) {
     MaskVertex firstVertex = path.vertex(0);
     QPointF firstCanvasPos = globalTransform.map(firstVertex.position);
     renderer_->drawThickLineLocal(lastCanvasPos,
                                   {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                                   7.0f, maskLineShadowColor);
     renderer_->drawThickLineLocal(lastCanvasPos,
                                   {(float)firstCanvasPos.x(), (float)firstCanvasPos.y()},
                                   4.0f, maskLineColor);
    }

    for (int v = 0; v < vertexCount; ++v) {
     MaskVertex vertex = path.vertex(v);
     QPointF canvasPos = globalTransform.map(vertex.position);
     FloatColor currentColor = maskPointColor;
     float currentRadius = 17.0f;
     if (isDraggingMaskVertex_ && draggingMaskIndex_ == m && draggingPathIndex_ == p && draggingVertexIndex_ == v) {
      currentColor = dragColor;
      currentRadius = 21.0f;
     } else if (hoveredMaskIndex_ == m && hoveredPathIndex_ == p && hoveredVertexIndex_ == v) {
      currentColor = hoverColor;
      currentRadius = 21.0f;
     }
     renderer_->drawPoint(static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y()),
                          currentRadius + 3.0f, maskPointShadowColor);
     renderer_->drawPoint(static_cast<float>(canvasPos.x()), static_cast<float>(canvasPos.y()),
                          currentRadius, currentColor);
    }
   }
  }
 }

 bool ArtifactLayerEditorWidgetV2::Impl::deleteHoveredMaskVertex(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer || hoveredMaskIndex_ < 0 || hoveredPathIndex_ < 0 || hoveredVertexIndex_ < 0) {
   return false;
  }

  LayerMask mask = layer->mask(hoveredMaskIndex_);
  if (hoveredPathIndex_ >= mask.maskPathCount()) {
   return false;
  }

  MaskPath path = mask.maskPath(hoveredPathIndex_);
  if (hoveredVertexIndex_ >= path.vertexCount()) {
   return false;
  }

  path.removeVertex(hoveredVertexIndex_);
  if (path.vertexCount() <= 0) {
   mask.removeMaskPath(hoveredPathIndex_);
   if (mask.maskPathCount() <= 0) {
    layer->removeMask(hoveredMaskIndex_);
   } else {
    layer->setMask(hoveredMaskIndex_, mask);
   }
  } else {
   mask.setMaskPath(hoveredPathIndex_, path);
   layer->setMask(hoveredMaskIndex_, mask);
  }

  markMaskEditDirty();
  hoveredVertexIndex_ = -1;
  hoveredPathIndex_ = -1;
  hoveredMaskIndex_ = -1;
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
 const QSize viewportSize = physicalViewportSize(widget_);
 const float viewportW = static_cast<float>(std::max(1, viewportSize.width()));
 const float viewportH = static_cast<float>(std::max(1, viewportSize.height()));
 const float prevZoom = renderer_->getZoom();
 float prevPanX = 0.0f;
 float prevPanY = 0.0f;
 renderer_->getPan(prevPanX, prevPanY);
 renderer_->setViewportSize(viewportW, viewportH);
 renderer_->setCanvasSize(viewportW, viewportH);
 renderer_->setZoom(1.0f);
 renderer_->setPan(0.0f, 0.0f);
 renderer_->setUseExternalMatrices(false);
 renderer_->resetGizmoCameraMatrices();
 renderer_->reset3DCameraMatrices();
 if (backgroundMode_ == LayerBackgroundMode::Alpha) {
  renderer_->drawCheckerboard(0.0f, 0.0f, viewportW, viewportH, 16.0f,
                              FloatColor(0.24f, 0.24f, 0.26f, 1.0f),
                              FloatColor(0.16f, 0.16f, 0.18f, 1.0f));
 } else if (backgroundMode_ == LayerBackgroundMode::MayaGradient) {
  refreshBackgroundCache();
  if (!cachedMayaGradientSprite_.isNull()) {
   renderer_->drawSprite(0.0f, 0.0f, viewportW, viewportH,
                         cachedMayaGradientSprite_, 1.0f);
  }
 } else {
  renderer_->drawRectLocal(0.0f, 0.0f, viewportW, viewportH, clearColor_);
 }
 if (compositionRenderer_) {
  if (auto* service = ArtifactProjectService::instance()) {
   if (auto composition = service->currentComposition().lock()) {
    const auto compSize = composition->settings().compositionSize();
    compositionRenderer_->SetCompositionSize(static_cast<float>(compSize.width()), static_cast<float>(compSize.height()));
   }
  }
  compositionRenderer_->ApplyCompositionSpace();
 }
 renderer_->setZoom(prevZoom);
 renderer_->setPan(prevPanX, prevPanY);
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
       if (displayMode_ == DisplayMode::Mask || editMode_ == EditMode::Mask) {
        drawMaskOverlay(layer);
       } else if (editMode_ == EditMode::Paint) {
        drawShapeOverlay(layer);
       }
      }
     }
    }
   }
  }
 renderer_->flush();
 renderer_->present();
}

void ArtifactLayerEditorWidgetV2::Impl::refreshBackgroundCache()
{
 if (backgroundMode_ != LayerBackgroundMode::MayaGradient) {
  cachedMayaGradientSprite_ = QImage();
  cachedMayaGradientSize_ = QSize();
  return;
 }
 const QSize viewportSize = physicalViewportSize(widget_);
 if (viewportSize.isEmpty() || cachedMayaGradientSize_ == viewportSize) {
  return;
 }
 cachedMayaGradientSize_ = viewportSize;
 cachedMayaGradientSprite_ = makeMayaGradientSprite(viewportSize, clearColor_);
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
  const QSize viewportSize = physicalViewportSize(window);
  renderer_->setViewportSize(static_cast<float>(viewportSize.width()), static_cast<float>(viewportSize.height()));
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

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
            setTargetLayer(LayerID(event.layerId));
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
            if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                impl_->targetLayerId_.toString() == event.layerId) {
              clearTargetLayer();
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
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
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent&) {
            const LayerID selectedId = currentSelectedLayerId();
            if (!selectedId.isNil()) {
              setTargetLayer(selectedId);
              return;
            }
            clearTargetLayer();
          }));
 }

 void ArtifactLayerEditorWidgetV2::clearTargetLayer()
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  if (impl_->shapeEditPending_) {
   impl_->commitShapeEditTransaction();
  }
  impl_->targetLayerId_ = LayerID();
  impl_->isDraggingShapeVertex_ = false;
  impl_->draggingShapeVertexIndex_ = -1;
  impl_->hoveredShapeVertexIndex_ = -1;
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
  if (impl_->editMode_ == EditMode::Mask && impl_->renderer_ && event) {
   if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    auto layer = impl_->targetLayer();
    if (layer) {
     impl_->beginMaskEditTransaction(layer);
    }
    if (layer && impl_->deleteHoveredMaskVertex(layer)) {
     impl_->commitMaskEditTransaction();
     impl_->renderOneFrame();
     event->accept();
     return;
    }
    impl_->commitMaskEditTransaction();
   }
  }
  if (impl_->editMode_ == EditMode::Paint && impl_->renderer_ && event) {
   if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    auto layer = impl_->targetLayer();
    if (layer) {
     impl_->beginShapeEditTransaction(layer);
    }
    if (layer && impl_->deleteHoveredShapeVertex(layer)) {
     impl_->commitShapeEditTransaction();
     impl_->renderOneFrame();
     event->accept();
     return;
    }
    impl_->commitShapeEditTransaction();
   }
  }
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

  if (impl_->editMode_ == EditMode::Paint && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
   if (shape && shape->shapeType() != ShapeType::Line) {
    const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                              static_cast<qreal>(canvasPos.y));

    int vertexIndex = -1;
    if (impl_->hitTestShapeVertex(layer, canvasPoint, vertexIndex)) {
     impl_->beginShapeEditTransaction(layer);
     impl_->isDraggingShapeVertex_ = true;
     impl_->draggingShapeVertexIndex_ = vertexIndex;
     setCursor(Qt::ClosedHandCursor);
     event->accept();
     return;
    }

    int insertIndex = -1;
    if (impl_->hitTestShapeSegment(layer, canvasPoint, insertIndex)) {
     impl_->beginShapeEditTransaction(layer);
     std::vector<QPointF> points = shape->customPolygonPoints();
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      const QPointF rawLocalPos = invTransform.map(canvasPoint);
      const QPointF localPos(
          std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
          std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
      const auto clampedInsertIndex = std::clamp(insertIndex, 0, static_cast<int>(points.size()));
      points.insert(points.begin() + clampedInsertIndex, localPos);
      shape->setCustomPolygonPoints(points, true);
      impl_->markShapeEditDirty();
      impl_->hoveredShapeVertexIndex_ = clampedInsertIndex;
      impl_->isDraggingShapeVertex_ = true;
      impl_->draggingShapeVertexIndex_ = clampedInsertIndex;
      setCursor(Qt::ClosedHandCursor);
      impl_->renderOneFrame();
      event->accept();
      return;
     }
    }

    impl_->beginShapeEditTransaction(layer);
    std::vector<QPointF> points = shape->customPolygonPoints();
    const QTransform globalTransform = layer->getGlobalTransform();
    bool invertible = false;
    const QTransform invTransform = globalTransform.inverted(&invertible);
    if (invertible) {
     const QPointF rawLocalPos = invTransform.map(canvasPoint);
     const QPointF localPos(
         std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
         std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
     if (points.size() < 3) {
      points.clear();
      const int sides = std::max(3, shape->polygonSides());
      const double width = std::max(1, shape->shapeWidth());
      const double height = std::max(1, shape->shapeHeight());
      const QPointF center(width * 0.5, height * 0.5);
      const double radius = std::min(width, height) * 0.45;
      for (int i = 0; i < sides; ++i) {
       const double angle = static_cast<double>(i) * 2.0 * M_PI / static_cast<double>(sides) - M_PI * 0.5;
       points.push_back(center + QPointF(std::cos(angle) * radius, std::sin(angle) * radius));
      }
     } else {
      points.push_back(localPos);
     }
     shape->setCustomPolygonPoints(points, true);
     impl_->markShapeEditDirty();
     impl_->hoveredShapeVertexIndex_ = static_cast<int>(points.size()) - 1;
     impl_->renderOneFrame();
     event->accept();
     return;
    }
   }
  }

    if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));

    int handleMaskIndex = -1;
    int handlePathIndex = -1;
    int handleVertexIndex = -1;
    MaskHandleType handleType = MaskHandleType::None;
    const float handleThreshold = 10.0f / std::max(0.1f, impl_->renderer_->getZoom());
    if (hitTestMaskHandle(layer, canvasPoint, handleThreshold,
                          handleMaskIndex, handlePathIndex, handleVertexIndex,
                          handleType)) {
      impl_->beginMaskEditTransaction(layer);
      impl_->isDraggingMaskVertex_ = false;
      impl_->isDraggingMaskHandle_ = true;
      impl_->draggingMaskIndex_ = handleMaskIndex;
      impl_->draggingPathIndex_ = handlePathIndex;
      impl_->draggingVertexIndex_ = handleVertexIndex;
      impl_->draggingMaskHandleType_ = static_cast<int>(handleType);
      impl_->hoveredMaskIndex_ = handleMaskIndex;
      impl_->hoveredPathIndex_ = handlePathIndex;
      impl_->hoveredVertexIndex_ = handleVertexIndex;
      impl_->hoveredMaskHandleType_ = static_cast<int>(handleType);
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }

    int maskIndex = -1;
    int pathIndex = -1;
    int vertexIndex = -1;
     if (impl_->hitTestMaskVertex(layer, canvasPoint, maskIndex, pathIndex, vertexIndex)) {
      LayerMask mask = layer->mask(maskIndex);
      MaskPath path = mask.maskPath(pathIndex);
      if (vertexIndex == 0 && !path.isClosed() && path.vertexCount() > 2) {
       impl_->beginMaskEditTransaction(layer);
       path.setClosed(true);
       mask.setMaskPath(pathIndex, path);
       layer->setMask(maskIndex, mask);
       impl_->markMaskEditDirty();
       impl_->renderOneFrame();
       event->accept();
       return;
      }
      impl_->beginMaskEditTransaction(layer);
      impl_->isDraggingMaskVertex_ = true;
      impl_->draggingMaskIndex_ = maskIndex;
      impl_->draggingPathIndex_ = pathIndex;
      impl_->draggingVertexIndex_ = vertexIndex;
     setCursor(Qt::ClosedHandCursor);
     event->accept();
     return;
    }

    // 空クリックでは新規マスクを自動生成しない。
    // 既存の頂点・ハンドル編集だけを行い、作成は別の明示操作に寄せる。
    event->accept();
    return;
   }
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

  if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton) {
   if (impl_->isDraggingMaskHandle_) {
    impl_->isDraggingMaskHandle_ = false;
    impl_->draggingMaskIndex_ = -1;
    impl_->draggingPathIndex_ = -1;
    impl_->draggingVertexIndex_ = -1;
    impl_->draggingMaskHandleType_ = -1;
    impl_->commitMaskEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
   if (impl_->isDraggingMaskVertex_) {
    impl_->isDraggingMaskVertex_ = false;
    impl_->draggingMaskIndex_ = -1;
    impl_->draggingPathIndex_ = -1;
    impl_->draggingVertexIndex_ = -1;
    impl_->commitMaskEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
  }
  if (impl_->editMode_ == EditMode::Paint && event->button() == Qt::LeftButton) {
   if (impl_->isDraggingShapeVertex_) {
    impl_->isDraggingShapeVertex_ = false;
    impl_->draggingShapeVertexIndex_ = -1;
    impl_->commitShapeEditTransaction();
    unsetCursor();
    event->accept();
    return;
   }
   if (impl_->shapeEditPending_) {
    impl_->commitShapeEditTransaction();
    event->accept();
    return;
   }
  }
  QWidget::mouseReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseDoubleClickEvent(QMouseEvent* event)
 {
  if (impl_->editMode_ == EditMode::Mask && event->button() == Qt::LeftButton && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));
    int maskIndex = -1;
    int pathIndex = -1;
    int vertexIndex = -1;
    if (impl_->hitTestMaskVertex(layer, canvasPoint, maskIndex, pathIndex, vertexIndex)) {
     LayerMask mask = layer->mask(maskIndex);
     MaskPath path = mask.maskPath(pathIndex);
     if (vertexIndex == 0 && !path.isClosed() && path.vertexCount() > 2) {
      impl_->beginMaskEditTransaction(layer);
      path.setClosed(true);
      mask.setMaskPath(pathIndex, path);
      layer->setMask(maskIndex, mask);
      impl_->markMaskEditDirty();
      impl_->renderOneFrame();
      event->accept();
      return;
     }
    }
   }
  }
  QWidget::mouseDoubleClickEvent(event);
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

  if (impl_->editMode_ == EditMode::Mask && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   if (layer) {
     const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
         {(float)event->position().x(), (float)event->position().y()});
     const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                               static_cast<qreal>(canvasPos.y));
    if (impl_->isDraggingMaskHandle_) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
      MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
      MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
      const QPointF localPos = invTransform.map(canvasPoint);
      const QPointF delta = localPos - vertex.position;
      if (impl_->draggingMaskHandleType_ == static_cast<int>(MaskHandleType::InTangent)) {
       vertex.inTangent = delta;
      } else if (impl_->draggingMaskHandleType_ == static_cast<int>(MaskHandleType::OutTangent)) {
       vertex.outTangent = delta;
      }
      path.setVertex(impl_->draggingVertexIndex_, vertex);
      mask.setMaskPath(impl_->draggingPathIndex_, path);
      layer->setMask(impl_->draggingMaskIndex_, mask);
      impl_->markMaskEditDirty();
      impl_->renderOneFrame();
      event->accept();
      return;
     }
    }
    if (impl_->isDraggingMaskVertex_) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      LayerMask mask = layer->mask(impl_->draggingMaskIndex_);
      MaskPath path = mask.maskPath(impl_->draggingPathIndex_);
      MaskVertex vertex = path.vertex(impl_->draggingVertexIndex_);
      vertex.position = invTransform.map(canvasPoint);
      path.setVertex(impl_->draggingVertexIndex_, vertex);
      mask.setMaskPath(impl_->draggingPathIndex_, path);
      layer->setMask(impl_->draggingMaskIndex_, mask);
      impl_->markMaskEditDirty();
      impl_->renderOneFrame();
      event->accept();
      return;
     }
    } else {
     const int beforeMask = impl_->hoveredMaskIndex_;
     const int beforePath = impl_->hoveredPathIndex_;
     const int beforeVertex = impl_->hoveredVertexIndex_;
     const int beforeHandleType = impl_->hoveredMaskHandleType_;
      impl_->updateMaskHover(layer, canvasPoint);
      if (beforeMask != impl_->hoveredMaskIndex_ ||
         beforePath != impl_->hoveredPathIndex_ ||
         beforeVertex != impl_->hoveredVertexIndex_ ||
         beforeHandleType != impl_->hoveredMaskHandleType_) {
      impl_->renderOneFrame();
     }
     if (impl_->hoveredVertexIndex_ >= 0) {
      setCursor(Qt::CrossCursor);
     } else {
      unsetCursor();
     }
    }
   }
  }
  if (impl_->editMode_ == EditMode::Paint && impl_->renderer_) {
   auto layer = impl_->targetLayer();
   auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer);
   if (shape && shape->hasCustomPolygon()) {
    const Detail::float2 canvasPos = impl_->renderer_->viewportToCanvas(
        {(float)event->position().x(), (float)event->position().y()});
    const QPointF canvasPoint(static_cast<qreal>(canvasPos.x),
                              static_cast<qreal>(canvasPos.y));
    if (impl_->isDraggingShapeVertex_) {
     const QTransform globalTransform = layer->getGlobalTransform();
     bool invertible = false;
     const QTransform invTransform = globalTransform.inverted(&invertible);
     if (invertible) {
      std::vector<QPointF> points = shape->customPolygonPoints();
      if (impl_->draggingShapeVertexIndex_ >= 0 &&
          impl_->draggingShapeVertexIndex_ < static_cast<int>(points.size())) {
       const QPointF rawLocalPos = invTransform.map(canvasPoint);
       points[static_cast<size_t>(impl_->draggingShapeVertexIndex_)] = QPointF(
           std::clamp(rawLocalPos.x(), 0.0, static_cast<double>(shape->shapeWidth())),
           std::clamp(rawLocalPos.y(), 0.0, static_cast<double>(shape->shapeHeight())));
       impl_->beginShapeEditTransaction(layer);
       shape->setCustomPolygonPoints(points, true);
       impl_->markShapeEditDirty();
       impl_->renderOneFrame();
       event->accept();
       return;
      }
     }
    } else {
     const int beforeVertex = impl_->hoveredShapeVertexIndex_;
     impl_->updateShapeHover(layer, canvasPoint);
     if (beforeVertex != impl_->hoveredShapeVertexIndex_) {
      impl_->renderOneFrame();
     }
     if (impl_->hoveredShapeVertexIndex_ >= 0) {
      setCursor(Qt::CrossCursor);
     } else {
      unsetCursor();
     }
    }
   }
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

void ArtifactLayerEditorWidgetV2::contextMenuEvent(QContextMenuEvent* event)
{
  if (!impl_) {
   QWidget::contextMenuEvent(event);
   return;
  }

  QMenu menu(this);
  QAction* alphaAct = menu.addAction(QStringLiteral("Alpha"));
  QAction* solidAct = menu.addAction(QStringLiteral("Solid"));
  QAction* mayaAct = menu.addAction(QStringLiteral("Maya Gradient"));
  alphaAct->setCheckable(true);
  solidAct->setCheckable(true);
  mayaAct->setCheckable(true);
  switch (impl_->backgroundMode_) {
   case LayerBackgroundMode::Alpha:
    alphaAct->setChecked(true);
    break;
   case LayerBackgroundMode::Solid:
    solidAct->setChecked(true);
    break;
   case LayerBackgroundMode::MayaGradient:
    mayaAct->setChecked(true);
    break;
  }

  QAction* chosen = menu.exec(event->globalPos());
  if (!chosen) {
   return;
  }
  if (chosen == alphaAct) {
   impl_->backgroundMode_ = LayerBackgroundMode::Alpha;
  } else if (chosen == solidAct) {
   impl_->backgroundMode_ = LayerBackgroundMode::Solid;
  } else if (chosen == mayaAct) {
   impl_->backgroundMode_ = LayerBackgroundMode::MayaGradient;
  } else {
   return;
  }

  impl_->refreshBackgroundCache();
  if (impl_->initialized_ && impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
   impl_->renderOneFrame();
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
   impl_->initialize(this);
   if (impl_->initialized_) {
    impl_->initializeSwapChain(this);
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
   }
  }
  if (impl_->initialized_) {
    impl_->startRenderLoop();
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

 void ArtifactLayerEditorWidgetV2::focusOutEvent(QFocusEvent* event)
 {
  QWidget::focusOutEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::setClearColor(const FloatColor& color)
 {
  std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
  impl_->clearColor_ = color;
 }

void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 if (impl_->shapeEditPending_) {
  impl_->commitShapeEditTransaction();
 }
 impl_->targetLayerId_ = id;
 impl_->isDraggingShapeVertex_ = false;
 impl_->draggingShapeVertexIndex_ = -1;
 impl_->hoveredShapeVertexIndex_ = -1;
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
     if (impl_->editMode_ == EditMode::Paint) {
      if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
       if (!shape->hasCustomPolygon()) {
        const std::vector<QPointF> points = buildShapeEditSeedPoints(*shape);
        if (points.size() >= 3) {
         shape->setCustomPolygonPoints(points, true);
        }
       }
      }
     }
      impl_->renderer_->fitToViewport();
      impl_->zoomLevel_ = impl_->renderer_->getZoom();
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
   if (impl_->renderer_) {
    impl_->renderer_->fitToViewport();
    impl_->zoomLevel_ = impl_->renderer_->getZoom();
   }
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
  impl_->editMode_ = mode;
  if (mode == EditMode::Mask || mode == EditMode::Paint) {
   setCursor(Qt::CrossCursor);
  } else {
   unsetCursor();
   impl_->isDraggingMaskVertex_ = false;
   impl_->draggingMaskIndex_ = -1;
   impl_->draggingPathIndex_ = -1;
   impl_->draggingVertexIndex_ = -1;
   impl_->isDraggingMaskHandle_ = false;
   impl_->draggingMaskHandleType_ = -1;
   impl_->isDraggingShapeVertex_ = false;
   impl_->draggingShapeVertexIndex_ = -1;
   impl_->hoveredShapeVertexIndex_ = -1;
   impl_->hoveredMaskHandleType_ = -1;
   if (impl_->shapeEditPending_) {
    impl_->commitShapeEditTransaction();
   }
  }
  if (mode == EditMode::Paint && !impl_->targetLayerId_.isNil()) {
   if (auto* service = ArtifactProjectService::instance()) {
    if (auto composition = service->currentComposition().lock()) {
     if (auto layer = composition->layerById(impl_->targetLayerId_)) {
      if (auto shape = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
       if (!shape->hasCustomPolygon()) {
        const std::vector<QPointF> points = buildShapeEditSeedPoints(*shape);
        if (points.size() >= 3) {
         shape->setCustomPolygonPoints(points, true);
        }
       }
      }
     }
    }
   }
  }
  if (impl_->initialized_ && impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
   impl_->renderOneFrame();
  }
 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {
  impl_->displayMode_ = mode;
  if (impl_->initialized_ && impl_->renderer_) {
   std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
   impl_->renderOneFrame();
  }
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
