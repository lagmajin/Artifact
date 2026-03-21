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
constexpr float kPi = 3.14159265358979323846f;

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
 
 const QRectF localRect = layer_->localBounds();
 if (localRect.isNull()) return;

 const QTransform globalTransform = layer_->getGlobalTransform();
 
 FloatColor gizmoColor{0.0f, 0.5f, 1.0f, 1.0f}; // Cyan-ish blue
 if (isDragging_) gizmoColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow while dragging

 // Transformed points for bounding box
 const Detail::float2 tl_c((float)globalTransform.map(localRect.topLeft()).x(), (float)globalTransform.map(localRect.topLeft()).y());
 const Detail::float2 tr_c((float)globalTransform.map(localRect.topRight()).x(), (float)globalTransform.map(localRect.topRight()).y());
 const Detail::float2 bl_c((float)globalTransform.map(localRect.bottomLeft()).x(), (float)globalTransform.map(localRect.bottomLeft()).y());
 const Detail::float2 br_c((float)globalTransform.map(localRect.bottomRight()).x(), (float)globalTransform.map(localRect.bottomRight()).y());

 renderer->drawSolidLine(tl_c, tr_c, gizmoColor, lineThickness);
 renderer->drawSolidLine(tr_c, br_c, gizmoColor, lineThickness);
 renderer->drawSolidLine(br_c, bl_c, gizmoColor, lineThickness);
 renderer->drawSolidLine(bl_c, tl_c, gizmoColor, lineThickness);

 // Center points for handles
 const Detail::float2 tc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).y());
 const Detail::float2 bc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).y());
 const Detail::float2 lc_c((float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).y());
 const Detail::float2 rc_c((float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).y());

 // Draw handles
 auto drawHandle = [&](const Detail::float2& pos) {
  renderer->drawSolidRect(pos.x - handleSize / 2.0f, pos.y - handleSize / 2.0f,
                          handleSize, handleSize, {1,1,1,1}, 1.0f);
  renderer->drawRectOutline(pos.x - handleSize / 2.0f,
                            pos.y - handleSize / 2.0f, handleSize, handleSize,
                            {0,0,0,1});
 };

 drawHandle(tl_c); drawHandle(tr_c); drawHandle(bl_c); drawHandle(br_c);
 drawHandle(tc_c); drawHandle(bc_c); drawHandle(lc_c); drawHandle(rc_c);

 // Rotation handle: line from top-center upward with circle
 const Detail::float2 rotateTip((float)globalTransform.map(QPointF(localRect.center().x(), localRect.top() - ROTATE_HANDLE_DISTANCE)).x(), 
                                (float)globalTransform.map(QPointF(localRect.center().x(), localRect.top() - ROTATE_HANDLE_DISTANCE)).y());

 renderer->drawSolidLine(tc_c, rotateTip, gizmoColor, lineThickness);
 renderer->drawCircle(rotateTip.x, rotateTip.y, handleSize * 0.6f, gizmoColor, lineThickness, false);
 renderer->drawCircle(rotateTip.x, rotateTip.y, handleSize * 0.2f, gizmoColor, 0.0f, true);

 // Anchor point: small crosshair at anchor position
 const auto& t3d = layer_->transform3D();
 const float anchorX = t3d.anchorX();
 const float anchorY = t3d.anchorY();
 const float anchorSize = ANCHOR_HANDLE_SIZE * invZoom;
 const FloatColor anchorColor{1.0f, 0.5f, 0.0f, 1.0f}; // Orange
 renderer->drawSolidLine({anchorX - anchorSize, anchorY}, {anchorX + anchorSize, anchorY}, anchorColor, lineThickness);
 renderer->drawSolidLine({anchorX, anchorY - anchorSize}, {anchorX, anchorY + anchorSize}, anchorColor, lineThickness);
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

 // Rotation handle: above top-center
 const float zoom = renderer->getZoom();
 const float rotDist = ROTATE_HANDLE_DISTANCE * (zoom > 0.0001f ? 1.0f / zoom : 1.0f);
 const float rotTipX = (float)bbox.center().x();
 const float rotTipY = (float)bbox.top() - rotDist;
 auto rotVPos = renderer->canvasToViewport({rotTipX, rotTipY});
 const float rotHitR = ROTATE_HANDLE_RADIUS + 4.0f;
 QRectF rotRect(rotVPos.x - rotHitR, rotVPos.y - rotHitR, rotHitR * 2, rotHitR * 2);
 if (rotRect.contains(viewportPos)) return HandleType::Rotate;

 // Anchor point handle
 if (layer_) {
  const auto& t3d = layer_->transform3D();
  auto anchorVP = renderer->canvasToViewport({(float)t3d.anchorX(), (float)t3d.anchorY()});
  const float anchorHit = ANCHOR_HANDLE_SIZE + 4.0f;
  QRectF anchorRect(anchorVP.x - anchorHit, anchorVP.y - anchorHit, anchorHit * 2, anchorHit * 2);
  if (anchorRect.contains(viewportPos)) return HandleType::Move;
 }

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
  case HandleType::Rotate:
   return Qt::CrossCursor;
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
   dragStartRotation_ = t3d.rotation();
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
  } else if (activeHandle_ == HandleType::Rotate) {
   const auto& t3dStart = layer_->transform3D();
   float anchorX = t3dStart.anchorX();
   float anchorY = t3dStart.anchorY();
   // Angle from anchor to start mouse position
   float startAngle = std::atan2(dragStartCanvasPos_.y() - anchorY,
                                  dragStartCanvasPos_.x() - anchorX);
   // Angle from anchor to current mouse position
   float currentAngle = std::atan2(currentCanvasPos.y() - anchorY,
                                    currentCanvasPos.x() - anchorX);
   float deltaAngle = (currentAngle - startAngle) * 180.0f / kPi;
   t3d.setRotation(time, dragStartRotation_ + deltaAngle);
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
