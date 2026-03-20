module;
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <memory>

module Artifact.Widgets.TransformGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Color.Float;
import Time.Rational;

namespace Artifact {

namespace {
QRectF expandedCanvasBounds(const QRectF& bbox, float zoom)
{
 const float safeZoom = zoom > 0.0001f ? zoom : 1.0f;
 const float pad = 12.0f / safeZoom;
 QRectF out = bbox;
 out.adjust(-pad, -pad, pad, pad);
 return out;
}

QRectF handleRectForViewport(const QPointF& viewportPos, float handleSize)
{
 return QRectF(viewportPos.x() - handleSize * 0.5, viewportPos.y() - handleSize * 0.5, handleSize, handleSize);
}

QRectF adjustedResizeBox(const QRectF& startBox, const QPointF& delta, TransformGizmo::HandleType handle)
{
 QRectF box = startBox;
 const double minSize = 1.0;

 switch (handle) {
 case TransformGizmo::HandleType::Scale_TL:
  box.setLeft(box.left() + delta.x());
  box.setTop(box.top() + delta.y());
  break;
 case TransformGizmo::HandleType::Scale_TR:
  box.setRight(box.right() + delta.x());
  box.setTop(box.top() + delta.y());
  break;
 case TransformGizmo::HandleType::Scale_BL:
  box.setLeft(box.left() + delta.x());
  box.setBottom(box.bottom() + delta.y());
  break;
 case TransformGizmo::HandleType::Scale_BR:
  box.setRight(box.right() + delta.x());
  box.setBottom(box.bottom() + delta.y());
  break;
 case TransformGizmo::HandleType::Scale_L:
  box.setLeft(box.left() + delta.x());
  break;
 case TransformGizmo::HandleType::Scale_R:
  box.setRight(box.right() + delta.x());
  break;
 case TransformGizmo::HandleType::Scale_T:
  box.setTop(box.top() + delta.y());
  break;
 case TransformGizmo::HandleType::Scale_B:
  box.setBottom(box.bottom() + delta.y());
  break;
 default:
  break;
 }

 if (box.width() < minSize) {
  if (handle == TransformGizmo::HandleType::Scale_L ||
      handle == TransformGizmo::HandleType::Scale_TL ||
      handle == TransformGizmo::HandleType::Scale_BL) {
   box.setLeft(box.right() - minSize);
  } else {
   box.setRight(box.left() + minSize);
  }
 }

 if (box.height() < minSize) {
  if (handle == TransformGizmo::HandleType::Scale_T ||
      handle == TransformGizmo::HandleType::Scale_TL ||
      handle == TransformGizmo::HandleType::Scale_TR) {
   box.setTop(box.bottom() - minSize);
  } else {
   box.setBottom(box.top() + minSize);
  }
 }

 return box.normalized();
}
} // namespace

TransformGizmo::TransformGizmo() {}
TransformGizmo::~TransformGizmo() {}

void TransformGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
 layer_ = layer;
 if (!isDragging_) {
  activeHandle_ = HandleType::None;
 }
}

void TransformGizmo::draw(ArtifactIRenderer* renderer) {
 if (!layer_ || !renderer) return;

 const float zoom = renderer->getZoom();
 const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
 const float lineThickness = 1.5f * invZoom;
 const float handleSize = HANDLE_SIZE * invZoom;
 QRectF bbox = expandedCanvasBounds(layer_->transformedBoundingBox(), zoom);
 if (bbox.isNull()) return;

 const Detail::float2 tl{(float)bbox.left(), (float)bbox.top()};
 const Detail::float2 tr{(float)bbox.right(), (float)bbox.top()};
 const Detail::float2 bl{(float)bbox.left(), (float)bbox.bottom()};
 const Detail::float2 br{(float)bbox.right(), (float)bbox.bottom()};
 const Detail::float2 tc{(float)bbox.center().x(), (float)bbox.top()};
 const Detail::float2 bc{(float)bbox.center().x(), (float)bbox.bottom()};
 const Detail::float2 lc{(float)bbox.left(), (float)bbox.center().y()};
 const Detail::float2 rc{(float)bbox.right(), (float)bbox.center().y()};

 FloatColor gizmoColor{0.0f, 0.5f, 1.0f, 1.0f}; // Cyan-ish blue
 if (isDragging_) gizmoColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow while dragging

 // Draw bounding box edges
 renderer->drawSolidLine(tl, tr, gizmoColor, lineThickness);
 renderer->drawSolidLine(tr, br, gizmoColor, lineThickness);
 renderer->drawSolidLine(br, bl, gizmoColor, lineThickness);
 renderer->drawSolidLine(bl, tl, gizmoColor, lineThickness);

 // Draw handles
 auto drawHandle = [&](const Detail::float2& pos) {
  renderer->drawSolidRect(pos.x - handleSize / 2.0f, pos.y - handleSize / 2.0f,
                          handleSize, handleSize);
  renderer->drawRectOutline(pos.x - handleSize / 2.0f,
                            pos.y - handleSize / 2.0f, handleSize, handleSize,
                            {1,1,1,1});
 };

 drawHandle(tl); drawHandle(tr); drawHandle(bl); drawHandle(br);
 drawHandle(tc); drawHandle(bc); drawHandle(lc); drawHandle(rc);
}

TransformGizmo::HandleType TransformGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
 if (!layer_ || !renderer) return HandleType::None;

 QRectF bbox = expandedCanvasBounds(layer_->transformedBoundingBox(), renderer->getZoom());
 if (bbox.isNull()) return HandleType::None;

 auto checkHandle = [&](float x, float y) {
  auto vPos = renderer->canvasToViewport({x, y});
  QRectF handleRect = handleRectForViewport({vPos.x, vPos.y}, HANDLE_SIZE);
  return handleRect.contains(viewportPos);
 };

 if (checkHandle((float)bbox.left(), (float)bbox.top())) return HandleType::Scale_TL;
 if (checkHandle((float)bbox.right(), (float)bbox.top())) return HandleType::Scale_TR;
 if (checkHandle((float)bbox.left(), (float)bbox.bottom())) return HandleType::Scale_BL;
 if (checkHandle((float)bbox.right(), (float)bbox.bottom())) return HandleType::Scale_BR;
 if (checkHandle((float)bbox.center().x(), (float)bbox.top())) return HandleType::Scale_T;
 if (checkHandle((float)bbox.center().x(), (float)bbox.bottom())) return HandleType::Scale_B;
 if (checkHandle((float)bbox.left(), (float)bbox.center().y())) return HandleType::Scale_L;
 if (checkHandle((float)bbox.right(), (float)bbox.center().y())) return HandleType::Scale_R;

 auto canvasPos = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 if (bbox.contains(canvasPos.x, canvasPos.y)) {
  return HandleType::Move;
 }

 return HandleType::None;
}

Qt::CursorShape TransformGizmo::cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const
{
 const auto handle = hitTest(viewportPos, renderer);
 switch (handle) {
 case HandleType::Move:
  return Qt::OpenHandCursor;
 case HandleType::Scale_TL:
 case HandleType::Scale_BR:
  return Qt::SizeFDiagCursor;
 case HandleType::Scale_TR:
 case HandleType::Scale_BL:
  return Qt::SizeBDiagCursor;
 case HandleType::Scale_L:
 case HandleType::Scale_R:
  return Qt::SizeHorCursor;
 case HandleType::Scale_T:
 case HandleType::Scale_B:
  return Qt::SizeVerCursor;
 default:
  return Qt::ArrowCursor;
 }
}

bool TransformGizmo::handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!layer_ || !renderer) return false;

 activeHandle_ = hitTest(viewportPos, renderer);
 if (activeHandle_ != HandleType::None) {
  isDragging_ = true;
  auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
  dragStartCanvasPos_ = QPointF(canvasMouse.x, canvasMouse.y);
  lastCanvasMousePos_ = dragStartCanvasPos_;
  const auto &t3d = layer_->transform3D();
  dragStartLayerPos_ = QPointF(t3d.positionX(), t3d.positionY());
  dragStartScaleX_ = t3d.scaleX();
  dragStartScaleY_ = t3d.scaleY();
  dragStartBoundingBox_ = layer_->transformedBoundingBox();
  dragStartLocalBounds_ = layer_->localBounds();
  dragStartAnchor_ = QPointF(t3d.anchorX(), t3d.anchorY());
  return true;
 }
 return false;
}

bool TransformGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!isDragging_ || !layer_ || !renderer) return false;

 auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 QPointF currentCanvasPos(canvasMouse.x, canvasMouse.y);
 QPointF delta = currentCanvasPos - dragStartCanvasPos_;
 ArtifactCore::RationalTime time(layer_->currentFrame(), 30000);
 auto &t3d = layer_->transform3D();

 if (activeHandle_ == HandleType::Move) {
  t3d.setPosition(time,
                  dragStartLayerPos_.x() + static_cast<float>(delta.x()),
                  dragStartLayerPos_.y() + static_cast<float>(delta.y()));
  layer_->setDirty(LayerDirtyFlag::Transform);
  Q_EMIT layer_->changed();
 } else if (activeHandle_ >= HandleType::Scale_TL && activeHandle_ <= HandleType::Scale_R) {
  if (std::abs(delta.x()) < 0.01 && std::abs(delta.y()) < 0.01) {
   lastCanvasMousePos_ = currentCanvasPos;
   return true;
  }
  const QRectF startBox = dragStartBoundingBox_;
  const QRectF localBounds = dragStartLocalBounds_;
  if (startBox.isValid() && startBox.width() > 0.0 && startBox.height() > 0.0 &&
      localBounds.isValid() && localBounds.width() > 0.0 && localBounds.height() > 0.0) {
   const QRectF targetBox = adjustedResizeBox(startBox, delta, activeHandle_);
   const double baseW = std::max(1.0, startBox.width());
   const double baseH = std::max(1.0, startBox.height());
   const float newScaleX = dragStartScaleX_ * static_cast<float>(targetBox.width() / baseW);
   const float newScaleY = dragStartScaleY_ * static_cast<float>(targetBox.height() / baseH);
   const float localLeft = static_cast<float>(localBounds.left());
   const float localTop = static_cast<float>(localBounds.top());
   const float anchorX = dragStartAnchor_.x();
   const float anchorY = dragStartAnchor_.y();
   const float newPosX = static_cast<float>(targetBox.left()) - newScaleX * (localLeft - anchorX);
   const float newPosY = static_cast<float>(targetBox.top()) - newScaleY * (localTop - anchorY);

   t3d.setScale(time, newScaleX, newScaleY);
   t3d.setPosition(time, newPosX, newPosY);

   layer_->setDirty(LayerDirtyFlag::Transform);
   Q_EMIT layer_->changed();
  }
  }

 lastCanvasMousePos_ = currentCanvasPos;
 return true;
}

void TransformGizmo::handleMouseRelease() {
 isDragging_ = false;
 activeHandle_ = HandleType::None;
}

}
