module;
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <memory>

module Artifact.Widgets.TransformGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Color.Float;

namespace Artifact {

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

 QRectF bbox = layer_->transformedBoundingBox();
 if (bbox.isNull()) return;

 // Convert bbox corners to viewport
 auto tl = renderer->canvasToViewport({(float)bbox.left(), (float)bbox.top()});
 auto tr = renderer->canvasToViewport({(float)bbox.right(), (float)bbox.top()});
 auto bl = renderer->canvasToViewport({(float)bbox.left(), (float)bbox.bottom()});
 auto br = renderer->canvasToViewport({(float)bbox.right(), (float)bbox.bottom()});

 FloatColor gizmoColor{0.0f, 0.5f, 1.0f, 1.0f}; // Cyan-ish blue
 if (isDragging_) gizmoColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow while dragging

 // Draw bounding box edges
 renderer->drawSolidLine(tl, tr, gizmoColor, 1.5f);
 renderer->drawSolidLine(tr, br, gizmoColor, 1.5f);
 renderer->drawSolidLine(br, bl, gizmoColor, 1.5f);
 renderer->drawSolidLine(bl, tl, gizmoColor, 1.5f);

 // Draw handles
 auto drawHandle = [&](Detail::float2 pos) {
  renderer->drawSolidRect(pos.x - HANDLE_SIZE/2, pos.y - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE);
  renderer->drawRectOutline(pos.x - HANDLE_SIZE/2, pos.y - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE, {1,1,1,1});
 };

 drawHandle(tl); drawHandle(tr); drawHandle(bl); drawHandle(br);
}

TransformGizmo::HandleType TransformGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!layer_ || !renderer) return HandleType::None;

 QRectF bbox = layer_->transformedBoundingBox();
 if (bbox.isNull()) return HandleType::None;

 auto checkHandle = [&](float x, float y, HandleType type) {
  auto vPos = renderer->canvasToViewport({x, y});
  QRectF handleRect(vPos.x - HANDLE_SIZE, vPos.y - HANDLE_SIZE, HANDLE_SIZE * 2, HANDLE_SIZE * 2);
  return handleRect.contains(viewportPos);
 };

 if (checkHandle((float)bbox.left(), (float)bbox.top(), HandleType::Scale_TL)) return HandleType::Scale_TL;
 if (checkHandle((float)bbox.right(), (float)bbox.top(), HandleType::Scale_TR)) return HandleType::Scale_TR;
 if (checkHandle((float)bbox.left(), (float)bbox.bottom(), HandleType::Scale_BL)) return HandleType::Scale_BL;
 if (checkHandle((float)bbox.right(), (float)bbox.bottom(), HandleType::Scale_BR)) return HandleType::Scale_BR;

 auto canvasPos = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 if (bbox.contains(canvasPos.x, canvasPos.y)) {
  return HandleType::Move;
 }

 return HandleType::None;
}

bool TransformGizmo::handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!layer_ || !renderer) return false;

 activeHandle_ = hitTest(viewportPos, renderer);
 if (activeHandle_ != HandleType::None) {
  isDragging_ = true;
  auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
  lastCanvasMousePos_ = QPointF(canvasMouse.x, canvasMouse.y);
  return true;
 }
 return false;
}

bool TransformGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!isDragging_ || !layer_ || !renderer) return false;

 auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 QPointF currentCanvasPos(canvasMouse.x, canvasMouse.y);
 QPointF delta = currentCanvasPos - lastCanvasMousePos_;

 auto& t2d = layer_->transform2D();
 float x, y, sx, sy;
 t2d.position(x, y);
 t2d.scale(sx, sy);

 if (activeHandle_ == HandleType::Move) {
  t2d.setPosition(x + (float)delta.x(), y + (float)delta.y());
  layer_->setDirty(LayerDirtyFlag::Transform);
  Q_EMIT layer_->changed();
 } else if (activeHandle_ >= HandleType::Scale_TL && activeHandle_ <= HandleType::Scale_BR) {
  // Simple scaling logic (naive)
  QRectF bbox = layer_->transformedBoundingBox();
  float newW = (float)bbox.width();
  float newH = (float)bbox.height();
  
  if (activeHandle_ == HandleType::Scale_BR || activeHandle_ == HandleType::Scale_TR) newW += (float)delta.x();
  if (activeHandle_ == HandleType::Scale_BR || activeHandle_ == HandleType::Scale_BL) newH += (float)delta.y();
  
  // This needs more precise math considering rotation, but for a start:
  float scaleFactorX = (bbox.width() > 0) ? (newW / (float)bbox.width()) : 1.0f;
  float scaleFactorY = (bbox.height() > 0) ? (newH / (float)bbox.height()) : 1.0f;
  t2d.setScale(sx * scaleFactorX, sy * scaleFactorY);
  
  layer_->setDirty(LayerDirtyFlag::Transform);
  Q_EMIT layer_->changed();
 }

 lastCanvasMousePos_ = currentCanvasPos;
 return true;
}

void TransformGizmo::handleMouseRelease() {
 isDragging_ = false;
 activeHandle_ = HandleType::None;
}

}
