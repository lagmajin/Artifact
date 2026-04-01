module;
#include <QDebug>
#include <QLoggingCategory>
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QGuiApplication>

module Artifact.Widgets.TransformGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Color.Float;
import Time.Rational;
import Artifact.Service.Project;

namespace Artifact {

namespace {
Q_LOGGING_CATEGORY(transformGizmoLog, "artifact.transformgizmo")

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

QPointF applyScaleRotateToVector(const QPointF& v, const float scaleX, const float scaleY, const float rotationDegrees)
{
 const double radians = rotationDegrees * (kPi / 180.0f);
 const double cosA = std::cos(radians);
 const double sinA = std::sin(radians);
 const double sx = v.x() * scaleX;
 const double sy = v.y() * scaleY;
 return QPointF(sx * cosA - sy * sinA, sx * sinA + sy * cosA);
}

FloatColor brighten(const FloatColor& color, const float factor)
{
 return {
  std::clamp(color.r() * factor, 0.0f, 1.0f),
  std::clamp(color.g() * factor, 0.0f, 1.0f),
  std::clamp(color.b() * factor, 0.0f, 1.0f),
  color.a()
 };
}

void drawEmphasizedLine(ArtifactIRenderer* renderer,
                        const Detail::float2& a,
                        const Detail::float2& b,
const FloatColor& color,
                        const float thickness,
                        const float invZoom,
                        const bool active)
{
 const float shadowShift = std::max(1.2f, 1.75f * invZoom);
 const float shadowThickness = thickness + std::max(1.5f, 1.6f * invZoom);
 const FloatColor shadow = { 0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f };
 const FloatColor highlight = brighten(color, active ? 1.18f : 1.08f);

 renderer->drawSolidLine({a.x + shadowShift, a.y + shadowShift},
                          {b.x + shadowShift, b.y + shadowShift},
                          shadow, shadowThickness);
 renderer->drawSolidLine(a, b, brighten(color, 0.80f), thickness + std::max(1.25f, 1.0f * invZoom));
 renderer->drawSolidLine({a.x - shadowShift * 0.35f, a.y - shadowShift * 0.35f},
                          {b.x - shadowShift * 0.35f, b.y - shadowShift * 0.35f},
                          highlight, std::max(1.5f, thickness * 0.72f));
}

void drawEmphasizedRect(ArtifactIRenderer* renderer,
                        const QRectF& rect,
                        const FloatColor& fill,
                        const float outlinePad,
                        const float invZoom,
                        const bool active)
{
 const QPointF offset(std::max(1.0f, 0.9f * invZoom), std::max(1.0f, 0.9f * invZoom));
 const QRectF shadowRect = rect.translated(offset);
 const QRectF innerRect = rect.adjusted(outlinePad, outlinePad, -outlinePad, -outlinePad);

 renderer->drawSolidRect((float)shadowRect.left(), (float)shadowRect.top(),
                         (float)shadowRect.width(), (float)shadowRect.height(),
                         {0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f}, 1.0f);
 renderer->drawSolidRect((float)innerRect.left(), (float)innerRect.top(),
                         (float)innerRect.width(), (float)innerRect.height(),
                         brighten(fill, active ? 1.12f : 1.02f), 1.0f);
 renderer->drawRectOutline((float)rect.left(), (float)rect.top(),
                           (float)rect.width(), (float)rect.height(),
                           active ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
                                  : FloatColor{0.10f, 0.10f, 0.10f, 1.0f});
}
} // namespace

TransformGizmo::TransformGizmo() {}
TransformGizmo::~TransformGizmo() {}

void TransformGizmo::setMode(Mode mode) {
 qDebug() << "[TransformGizmo] setMode:" << mode_ << "->" << mode;
 mode_ = mode;
 if (!allowsHandle(activeHandle_)) {
  activeHandle_ = HandleType::None;
  isDragging_ = false;
 }
}

TransformGizmo::Mode TransformGizmo::mode() const {
 return mode_;
}

void TransformGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
 layer_ = layer;
 if (!isDragging_) {
  activeHandle_ = HandleType::None;
 }
}

static constexpr double HANDLE_SIZE = 8.0;
static constexpr double ROTATE_HANDLE_DISTANCE = 28.0;
static constexpr double GIZMO_OFFSET = 8.0;
static constexpr int TRANSFORM_KEYFRAME_SCALE = 24;

void TransformGizmo::draw(ArtifactIRenderer* renderer) {
if (!layer_ || !renderer) {
 return;
}

const float zoom = renderer->getZoom();
const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
const float lineThickness = std::clamp(10.0f * invZoom, 5.5f, 16.0f);
const float handleSize = std::clamp(HANDLE_SIZE * 2.05f * invZoom, 12.0f, 28.0f);

QRectF localRect = layer_->localBounds();
if (!localRect.isValid() || localRect.width() <= 0.0 || localRect.height() <= 0.0) {
 return;
}

// Apply visual offset
localRect.adjust(-GIZMO_OFFSET, -GIZMO_OFFSET, GIZMO_OFFSET, GIZMO_OFFSET);

const QTransform globalTransform = layer_->getGlobalTransform();
 FloatColor gizmoColor{0.0f, 0.5f, 1.0f, 1.0f}; // Cyan-ish blue
 if (isDragging_) gizmoColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow while dragging
 const bool isActive = isDragging_ || activeHandle_ != HandleType::None;

const bool showMove = mode_ == Mode::All || mode_ == Mode::Move;
const bool showScale = mode_ == Mode::All || mode_ == Mode::Scale;
const bool showRotate = mode_ == Mode::All || mode_ == Mode::Rotate;
const bool showAnchor = mode_ == Mode::All || mode_ == Mode::Move;

// Transformed points for bounding box
 const Detail::float2 tl_c((float)globalTransform.map(localRect.topLeft()).x(), (float)globalTransform.map(localRect.topLeft()).y());
 const Detail::float2 tr_c((float)globalTransform.map(localRect.topRight()).x(), (float)globalTransform.map(localRect.topRight()).y());
 const Detail::float2 bl_c((float)globalTransform.map(localRect.bottomLeft()).x(), (float)globalTransform.map(localRect.bottomLeft()).y());
 const Detail::float2 br_c((float)globalTransform.map(localRect.bottomRight()).x(), (float)globalTransform.map(localRect.bottomRight()).y());

 if (showMove || showScale || showRotate) {
  drawEmphasizedLine(renderer, tl_c, tr_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, tr_c, br_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, br_c, bl_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, bl_c, tl_c, gizmoColor, lineThickness, invZoom, isActive);
 }

 // Center points for handles
 const Detail::float2 tc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).y());
 const Detail::float2 bc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).y());
 const Detail::float2 lc_c((float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).y());
 const Detail::float2 rc_c((float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).y());

 // Draw handles
 auto drawHandle = [&](const Detail::float2& pos) {
  const QRectF rect(pos.x - handleSize * 0.5f, pos.y - handleSize * 0.5f, handleSize, handleSize);
  drawEmphasizedRect(renderer, rect, {1.0f, 1.0f, 1.0f, 1.0f}, std::max(1.0f, 0.8f * invZoom), invZoom, activeHandle_ != HandleType::None);
 };

 if (showScale) {
  drawHandle(tl_c); drawHandle(tr_c); drawHandle(bl_c); drawHandle(br_c);
  drawHandle(tc_c); drawHandle(bc_c); drawHandle(lc_c); drawHandle(rc_c);
 }

 // Rotation handle: line from top-center upward with circle
 const Detail::float2 rotateTip((float)globalTransform.map(QPointF(localRect.center().x(), localRect.top() - ROTATE_HANDLE_DISTANCE)).x(), 
                                (float)globalTransform.map(QPointF(localRect.center().x(), localRect.top() - ROTATE_HANDLE_DISTANCE)).y());

 if (showRotate) {
  drawEmphasizedLine(renderer, tc_c, rotateTip, gizmoColor, lineThickness, invZoom, isActive);
  renderer->drawCircle(rotateTip.x, rotateTip.y, handleSize * 0.72f, brighten(gizmoColor, 1.05f), lineThickness, false);
  renderer->drawCircle(rotateTip.x, rotateTip.y, handleSize * 0.28f, {1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, true);
 }

 // Anchor point: crosshair at anchor position
 const auto& t3d = layer_->transform3D();
 const QPointF anchorWorld = globalTransform.map(QPointF(t3d.anchorX(), t3d.anchorY()));
 if (showAnchor) {
  const FloatColor outer = activeHandle_ == HandleType::Anchor
      ? FloatColor{0.95f, 0.95f, 0.95f, 1.0f}
      : FloatColor{0.0f, 0.0f, 0.0f, 1.0f};
  const FloatColor inner = activeHandle_ == HandleType::Anchor
      ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
      : FloatColor{1.0f, 0.82f, 0.18f, 1.0f};
  renderer->drawCrosshair((float)anchorWorld.x() + std::max(1.0f, 0.9f * invZoom),
                           (float)anchorWorld.y() + std::max(1.0f, 0.9f * invZoom),
                           handleSize * 2.45f, {0.0f, 0.0f, 0.0f, activeHandle_ == HandleType::Anchor ? 0.50f : 0.36f});
  renderer->drawCrosshair((float)anchorWorld.x(),
                           (float)anchorWorld.y(),
                           handleSize * 2.0f, outer);
  renderer->drawCrosshair((float)anchorWorld.x() - std::max(1.0f, 0.4f * invZoom),
                           (float)anchorWorld.y() - std::max(1.0f, 0.4f * invZoom),
                           handleSize * 1.32f, inner);
 }

 // Draw Smart Guides (Snapping Lines)
 if (!activeSnapLines_.empty()) {
  auto comp = ArtifactProjectService::instance()->currentComposition().lock();
  if (comp) {
   const auto size = comp->settings().compositionSize();
   const float w = static_cast<float>(size.width() > 0 ? size.width() : 1920);
   const float h = static_cast<float>(size.height() > 0 ? size.height() : 1080);
   for (const auto& sl : activeSnapLines_) {
    if (sl.isVertical) {
     renderer->drawSolidLine({sl.position, 0.0f}, {sl.position, h}, {1.0f, 0.0f, 1.0f, 0.8f}, std::max(1.0f, 1.5f * invZoom));
    } else {
     renderer->drawSolidLine({0.0f, sl.position}, {w, sl.position}, {1.0f, 0.0f, 1.0f, 0.8f}, std::max(1.0f, 1.5f * invZoom));
    }
   }
  }
 }
}

TransformGizmo::HandleType TransformGizmo::hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const {
 if (!layer_ || !renderer) return HandleType::None;

 QRectF localRect = layer_->localBounds();
 if (!localRect.isValid() || localRect.width() <= 0.0 || localRect.height() <= 0.0) return HandleType::None;

 // Apply visual offset to match draw()
 localRect.adjust(-GIZMO_OFFSET, -GIZMO_OFFSET, GIZMO_OFFSET, GIZMO_OFFSET);

 const QTransform globalTransform = layer_->getGlobalTransform();
 const float zoom = renderer->getZoom();
 
 // 1. Check handles first (they should have priority over the body)
 auto checkLocalPoint = [&](const QPointF& localPoint) {
  QPointF worldPoint = globalTransform.map(localPoint);
  auto vPos = renderer->canvasToViewport({(float)worldPoint.x(), (float)worldPoint.y()});
  QRectF handleRect = handleRectForViewport({vPos.x, vPos.y}, HANDLE_SIZE);
  return handleRect.contains(viewportPos);
 };

 if (allowsHandle(HandleType::Scale_TL) && checkLocalPoint(localRect.topLeft())) return HandleType::Scale_TL;
 if (allowsHandle(HandleType::Scale_TR) && checkLocalPoint(localRect.topRight())) return HandleType::Scale_TR;
 if (allowsHandle(HandleType::Scale_BL) && checkLocalPoint(localRect.bottomLeft())) return HandleType::Scale_BL;
 if (allowsHandle(HandleType::Scale_BR) && checkLocalPoint(localRect.bottomRight())) return HandleType::Scale_BR;
 if (allowsHandle(HandleType::Scale_T) && checkLocalPoint(QPointF(localRect.center().x(), localRect.top()))) return HandleType::Scale_T;
 if (allowsHandle(HandleType::Scale_B) && checkLocalPoint(QPointF(localRect.center().x(), localRect.bottom()))) return HandleType::Scale_B;
 if (allowsHandle(HandleType::Scale_L) && checkLocalPoint(QPointF(localRect.left(), localRect.center().y()))) return HandleType::Scale_L;
 if (allowsHandle(HandleType::Scale_R) && checkLocalPoint(QPointF(localRect.right(), localRect.center().y()))) return HandleType::Scale_R;

 // 2. Rotation handle: above top-center
 const QPointF localRotTip(localRect.center().x(), localRect.top() - ROTATE_HANDLE_DISTANCE);
 QPointF worldRotTip = globalTransform.map(localRotTip);
 auto vRotTip = renderer->canvasToViewport({(float)worldRotTip.x(), (float)worldRotTip.y()});
 const float rotHitR = ROTATE_HANDLE_RADIUS + 6.0f;
 QRectF rotRect(vRotTip.x - rotHitR, vRotTip.y - rotHitR, rotHitR * 2, rotHitR * 2);
 if (allowsHandle(HandleType::Rotate) && rotRect.contains(viewportPos)) return HandleType::Rotate;

 // 3. Anchor point handle
 const auto& t3d = layer_->transform3D();
 // In current implementation anchor is in local coords (usually 0,0)
 QPointF worldAnchor = globalTransform.map(QPointF(t3d.anchorX(), t3d.anchorY()));
 auto anchorVP = renderer->canvasToViewport({(float)worldAnchor.x(), (float)worldAnchor.y()});
 const float anchorHit = ANCHOR_HANDLE_SIZE + 4.0f;
 QRectF anchorRect(anchorVP.x - anchorHit, anchorVP.y - anchorHit, anchorHit * 2, anchorHit * 2);
  if (allowsHandle(HandleType::Anchor) && anchorRect.contains(viewportPos)) {
   return HandleType::Anchor;
  }

 // 4. Body hit test (Move) using inverse transform
 auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (invertible && allowsHandle(HandleType::Move)) {
  QPointF localMouse = invTransform.map(QPointF(canvasMouse.x, canvasMouse.y));
  if (localRect.contains(localMouse)) {
   return HandleType::Move;
  }
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
 case HandleType::Anchor:
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
   dragStartGlobalTransform_ = layer_->getGlobalTransform();
   dragStartBoundingBox_ = layer_->transformedBoundingBox();
  dragStartLocalBounds_ = layer_->localBounds();
  dragStartAnchor_ = QPointF(t3d.anchorX(), t3d.anchorY());
  return true;
 }
 return false;
}

bool TransformGizmo::allowsHandle(HandleType handle) const {
 switch (mode_) {
 case Mode::All:
  return handle != HandleType::None;
 case Mode::Move:
  return handle == HandleType::Move || handle == HandleType::Anchor;
  case Mode::Rotate:
   return handle == HandleType::Rotate;
 case Mode::Scale:
  return handle == HandleType::Scale_TL || handle == HandleType::Scale_TR ||
         handle == HandleType::Scale_BL || handle == HandleType::Scale_BR ||
         handle == HandleType::Scale_T || handle == HandleType::Scale_B ||
         handle == HandleType::Scale_L || handle == HandleType::Scale_R;
 }
 return false;
}

bool TransformGizmo::handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer) {
 if (!isDragging_ || !layer_ || !renderer) return false;

 auto canvasMouse = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 QPointF currentCanvasPos(canvasMouse.x, canvasMouse.y);
 QPointF delta = currentCanvasPos - dragStartCanvasPos_;
 ArtifactCore::RationalTime time(layer_->currentFrame(), TRANSFORM_KEYFRAME_SCALE);
 auto &t3d = layer_->transform3D();

  if (activeHandle_ == HandleType::Move) {
   float newX = dragStartLayerPos_.x() + static_cast<float>(delta.x());
   float newY = dragStartLayerPos_.y() + static_cast<float>(delta.y());

   activeSnapLines_.clear();
   const bool enableSnapping = !(QGuiApplication::keyboardModifiers() & Qt::AltModifier);

   if (enableSnapping) {
    auto comp = ArtifactProjectService::instance()->currentComposition().lock();
    if (comp) {
     const float SNAP_DIST = 10.0f / (renderer->getZoom() > 0.001f ? renderer->getZoom() : 1.0f);
     std::vector<float> vLines;
     std::vector<float> hLines;

     // Composition bounds/center
     const auto size = comp->settings().compositionSize();
     vLines.push_back(size.width() / 2.0f);
     hLines.push_back(size.height() / 2.0f);
     vLines.push_back(0.0f);
     vLines.push_back(size.width());
     hLines.push_back(0.0f);
     hLines.push_back(size.height());

     // Other layers' bounds/center
     for (const auto& other : comp->allLayer()) {
      if (!other || !other->isVisible() || other->id() == layer_->id() || other->isLocked()) continue;
      QRectF bounds = other->transformedBoundingBox();
      if (bounds.isValid() && bounds.width() > 0) {
       vLines.push_back(bounds.left());
       vLines.push_back(bounds.center().x());
       vLines.push_back(bounds.right());
       hLines.push_back(bounds.top());
       hLines.push_back(bounds.center().y());
       hLines.push_back(bounds.bottom());
      }
     }

     QRectF currentBBox = dragStartBoundingBox_;
     currentBBox.translate(delta);

     // Snap X (center horizontal axis)
     float centerV = currentBBox.center().x();
     float bestVDist = SNAP_DIST;
     float bestVLine = 0.0f;
     bool snappedV = false;
     for (float vl : vLines) {
      if (std::abs(centerV - vl) < bestVDist) {
       bestVDist = std::abs(centerV - vl);
       bestVLine = vl;
       snappedV = true;
      }
     }
     if (snappedV) {
      newX += (bestVLine - centerV);
      activeSnapLines_.push_back({true, bestVLine});
     }

     // Snap Y (center vertical axis)
     float centerH = currentBBox.center().y();
     float bestHDist = SNAP_DIST;
     float bestHLine = 0.0f;
     bool snappedH = false;
     for (float hl : hLines) {
      if (std::abs(centerH - hl) < bestHDist) {
       bestHDist = std::abs(centerH - hl);
       bestHLine = hl;
       snappedH = true;
      }
     }
     if (snappedH) {
      newY += (bestHLine - centerH);
      activeSnapLines_.push_back({false, bestHLine});
     }
    }
   }

   t3d.setPosition(time, newX, newY);
   layer_->setDirty(LayerDirtyFlag::Transform);
   Q_EMIT layer_->changed();
  } else if (activeHandle_ == HandleType::Anchor) {
   bool invertible = false;
   const QTransform inv = dragStartGlobalTransform_.inverted(&invertible);
   if (invertible) {
    QPointF targetLocalAnchor = inv.map(currentCanvasPos);

    // --- Snapping in Local Space ---
    const bool enableSnapping = !(QGuiApplication::keyboardModifiers() & Qt::AltModifier);
    if (enableSnapping) {
     const float SNAP_DIST = 8.0f / (renderer->getZoom() > 0.001f ? renderer->getZoom() : 1.0f);
     const QRectF b = dragStartLocalBounds_;
     const std::vector<float> vLines = { (float)b.left(), (float)b.center().x(), (float)b.right() };
     const std::vector<float> hLines = { (float)b.top(), (float)b.center().y(), (float)b.bottom() };
     
     for (float vl : vLines) {
      if (std::abs(targetLocalAnchor.x() - vl) < SNAP_DIST) {
       targetLocalAnchor.setX(vl);
       break; 
      }
     }
     for (float hl : hLines) {
      if (std::abs(targetLocalAnchor.y() - hl) < SNAP_DIST) {
       targetLocalAnchor.setY(hl);
       break;
      }
     }
    }

    const QPointF deltaAnchor = targetLocalAnchor - dragStartAnchor_;
    const QPointF compensation = applyScaleRotateToVector(
        deltaAnchor, dragStartScaleX_, dragStartScaleY_, dragStartRotation_);

    t3d.setAnchor(time,
                  static_cast<float>(targetLocalAnchor.x()),
                  static_cast<float>(targetLocalAnchor.y()),
                  t3d.anchorZ());
    t3d.setPosition(time,
                    dragStartLayerPos_.x() + static_cast<float>(compensation.x()),
                    dragStartLayerPos_.y() + static_cast<float>(compensation.y()));
    layer_->setDirty(LayerDirtyFlag::Transform);
    Q_EMIT layer_->changed();
   }
  } else if (activeHandle_ == HandleType::Rotate) {
   // The anchor point in world space is exactly the layer's position property
   // because position is where the anchor is "pinned" on the canvas.
   const float anchorWorldX = dragStartLayerPos_.x();
   const float anchorWorldY = dragStartLayerPos_.y();

   // Angle from anchor to start mouse position
   const double startAngle = std::atan2(dragStartCanvasPos_.y() - anchorWorldY,
                                        dragStartCanvasPos_.x() - anchorWorldX);
   // Angle from anchor to current mouse position
   const double currentAngle = std::atan2(currentCanvasPos.y() - anchorWorldY,
                                          currentCanvasPos.x() - anchorWorldX);
   
   const float deltaAngle = static_cast<float>((currentAngle - startAngle) * 180.0 / 3.14159265358979323846);
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
 activeSnapLines_.clear();
}

}
