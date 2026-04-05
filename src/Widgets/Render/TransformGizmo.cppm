module;
#include <QDebug>
#include <QLoggingCategory>
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <QTransform>
#include <QString>
#include <algorithm>
#include <cmath>
#include <memory>
#include <QGuiApplication>

module Artifact.Widgets.TransformGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Undo.UndoManager;
import Color.Float;
import Time.Rational;
import Artifact.Service.Project;
import Widgets.Utils.CSS;

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
 case TransformGizmo::HandleType::Scale_Center:
  box.translate(delta);
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

float pointDistance(const QPointF& a, const QPointF& b)
{
 const float dx = static_cast<float>(a.x() - b.x());
 const float dy = static_cast<float>(a.y() - b.y());
 return std::sqrt(dx * dx + dy * dy);
}

float pointDistance(const QPointF& a, const Detail::float2& b)
{
 return pointDistance(a, QPointF(b.x, b.y));
}

float pointDistance(const Detail::float2& a, const QPointF& b)
{
 return pointDistance(QPointF(a.x, a.y), b);
}

float pointToSegmentDistance(const QPointF& p, const QPointF& a, const QPointF& b)
{
 const double ax = a.x();
 const double ay = a.y();
 const double bx = b.x();
 const double by = b.y();
 const double px = p.x();
 const double py = p.y();
 const double abx = bx - ax;
 const double aby = by - ay;
 const double lenSq = abx * abx + aby * aby;
 if (lenSq <= 1e-9) {
  return pointDistance(p, a);
 }
 const double t = std::clamp(((px - ax) * abx + (py - ay) * aby) / lenSq, 0.0, 1.0);
 const QPointF proj(ax + abx * t, ay + aby * t);
 return pointDistance(p, proj);
}

struct TransformSnapshot {
 bool hasPositionKey = false;
 bool hasRotationKey = false;
 bool hasScaleKey = false;
 float positionX = 0.0f;
 float positionY = 0.0f;
 float rotation = 0.0f;
 float scaleX = 1.0f;
 float scaleY = 1.0f;
 float anchorX = 0.0f;
 float anchorY = 0.0f;
 float anchorZ = 0.0f;
};

class TransformUndoCommand final : public UndoCommand {
public:
 TransformUndoCommand(ArtifactAbstractLayerPtr layer, int64_t frame, TransformSnapshot before, TransformSnapshot after)
     : layer_(layer), frame_(frame), before_(before), after_(after) {}

 void undo() override { apply(before_); }
 void redo() override { apply(after_); }
 QString label() const override { return QStringLiteral("Transform Layer"); }

private:
 void apply(const TransformSnapshot& snapshot) {
  auto layer = layer_.lock();
  if (!layer) {
   return;
  }

  const ArtifactCore::RationalTime time(frame_, 24);
  auto& t3d = layer->transform3D();
  const TransformSnapshot current{
   t3d.hasPositionKeyFrameAt(time),
   t3d.hasRotationKeyFrameAt(time),
   t3d.hasScaleKeyFrameAt(time),
   t3d.positionX(),
   t3d.positionY(),
   t3d.rotation(),
   t3d.scaleX(),
   t3d.scaleY(),
   t3d.anchorX(),
   t3d.anchorY(),
   t3d.anchorZ()
  };
  const bool alreadyMatches =
      current.hasPositionKey == snapshot.hasPositionKey &&
      current.hasRotationKey == snapshot.hasRotationKey &&
      current.hasScaleKey == snapshot.hasScaleKey &&
      std::abs(current.positionX - snapshot.positionX) <= 0.0001f &&
      std::abs(current.positionY - snapshot.positionY) <= 0.0001f &&
      std::abs(current.rotation - snapshot.rotation) <= 0.0001f &&
      std::abs(current.scaleX - snapshot.scaleX) <= 0.0001f &&
      std::abs(current.scaleY - snapshot.scaleY) <= 0.0001f &&
      std::abs(current.anchorX - snapshot.anchorX) <= 0.0001f &&
      std::abs(current.anchorY - snapshot.anchorY) <= 0.0001f &&
      std::abs(current.anchorZ - snapshot.anchorZ) <= 0.0001f;
  if (alreadyMatches) {
   return;
  }

  if (snapshot.hasPositionKey) {
   t3d.setPosition(time, snapshot.positionX, snapshot.positionY);
  } else {
   t3d.removePositionKeyFrameAt(time);
  }

  if (snapshot.hasRotationKey) {
   t3d.setRotation(time, snapshot.rotation);
  } else {
   t3d.removeRotationKeyFrameAt(time);
  }

  if (snapshot.hasScaleKey) {
   t3d.setScale(time, snapshot.scaleX, snapshot.scaleY);
  } else {
   t3d.removeScaleKeyFrameAt(time);
  }
  layer->setDirty(LayerDirtyFlag::Transform);
  layer->changed();
  if (auto* mgr = UndoManager::instance()) {
   mgr->notifyAnythingChanged();
  }
 }

 ArtifactAbstractLayerWeak layer_;
 int64_t frame_ = 0;
 TransformSnapshot before_;
 TransformSnapshot after_;
};

void drawEmphasizedLine(ArtifactIRenderer* renderer,
                        const Detail::float2& a,
                        const Detail::float2& b,
const FloatColor& color,
                        const float thickness,
                        const float invZoom,
                        const bool active)
{
 const float dx = b.x - a.x;
 const float dy = b.y - a.y;
 const float len = std::sqrt(dx * dx + dy * dy);
 const Detail::float2 norm = len > 0.0001f ? Detail::float2{ -dy / len, dx / len } : Detail::float2{0.0f, 0.0f};
 const float shadowShift = std::max(1.0f, 1.25f * invZoom);
 const float shadowThickness = thickness + std::max(0.8f, 0.7f * invZoom);
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
 qDebug() << "[TransformGizmo] setMode:" << static_cast<int>(mode_) << "->" << static_cast<int>(mode);
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
static constexpr double GIZMO_OFFSET = 12.0;
static constexpr int TRANSFORM_KEYFRAME_SCALE = 24;

void TransformGizmo::draw(ArtifactIRenderer* renderer) {
if (!layer_ || !renderer) {
 return;
}

const float zoom = renderer->getZoom();
const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
const float lineThickness = std::clamp(2.7f * invZoom, 1.5f, 5.0f);
const float handleSize = std::clamp(HANDLE_SIZE * 1.65f * invZoom, 9.0f, 22.0f);

QRectF localRect = layer_->localBounds();
if (!localRect.isValid() || localRect.width() <= 0.0 || localRect.height() <= 0.0) {
 return;
}

// Apply visual offset
localRect.adjust(-GIZMO_OFFSET, -GIZMO_OFFSET, GIZMO_OFFSET, GIZMO_OFFSET);

const QTransform globalTransform = layer_->getGlobalTransform();
 const QColor themeAccent = QColor(ArtifactCore::currentDCCTheme().accentColor);
 FloatColor gizmoColor{themeAccent.redF() * 0.88f,
                       themeAccent.greenF() * 0.88f,
                       themeAccent.blueF() * 0.96f,
                       1.0f};
 if (isDragging_) gizmoColor = {1.0f, 0.88f, 0.22f, 1.0f}; // Warm highlight while dragging
 const bool isActive = isDragging_ || activeHandle_ != HandleType::None;

const bool showMove = mode_ == Mode::All || mode_ == Mode::Move;
const bool showScale = mode_ == Mode::All || mode_ == Mode::Scale;
const bool showRotate = mode_ == Mode::All || mode_ == Mode::Rotate;
const bool showAnchor = mode_ == Mode::All;

// Transformed points for bounding box
 const Detail::float2 tl_c((float)globalTransform.map(localRect.topLeft()).x(), (float)globalTransform.map(localRect.topLeft()).y());
 const Detail::float2 tr_c((float)globalTransform.map(localRect.topRight()).x(), (float)globalTransform.map(localRect.topRight()).y());
 const Detail::float2 bl_c((float)globalTransform.map(localRect.bottomLeft()).x(), (float)globalTransform.map(localRect.bottomLeft()).y());
 const Detail::float2 br_c((float)globalTransform.map(localRect.bottomRight()).x(), (float)globalTransform.map(localRect.bottomRight()).y());

 if (showMove || showScale) {
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
 auto drawHandle = [&](const Detail::float2& pos, const FloatColor& color) {
  const QRectF rect(pos.x - handleSize * 0.5f, pos.y - handleSize * 0.5f, handleSize, handleSize);
  drawEmphasizedRect(renderer, rect, color, std::max(1.0f, 0.8f * invZoom), invZoom, activeHandle_ != HandleType::None);
 };

 if (showScale) {
  const Detail::float2 center_c((float)globalTransform.map(localRect.center()).x(), (float)globalTransform.map(localRect.center()).y());
  const FloatColor scaleX = activeHandle_ == HandleType::Scale_L || activeHandle_ == HandleType::Scale_R
      ? FloatColor{1.0f, 0.24f, 0.10f, 1.0f}
      : FloatColor{0.98f, 0.18f, 0.06f, 1.0f};
  const FloatColor scaleY = activeHandle_ == HandleType::Scale_T || activeHandle_ == HandleType::Scale_B
      ? FloatColor{0.18f, 1.0f, 0.30f, 1.0f}
      : FloatColor{0.08f, 0.86f, 0.22f, 1.0f};
  const FloatColor scaleCorner = activeHandle_ == HandleType::Scale_TL || activeHandle_ == HandleType::Scale_TR ||
                                 activeHandle_ == HandleType::Scale_BL || activeHandle_ == HandleType::Scale_BR
      ? FloatColor{1.0f, 0.98f, 0.96f, 1.0f}
      : FloatColor{0.98f, 0.98f, 0.98f, 1.0f};

  const float axisThickness = std::max(1.05f, 1.25f * invZoom);
  const float cornerAxisThickness = std::max(0.9f, 1.05f * invZoom);
  drawEmphasizedLine(renderer, center_c, tc_c, scaleY, axisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, bc_c, scaleY, axisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, lc_c, scaleX, axisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, rc_c, scaleX, axisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, tl_c, scaleCorner, cornerAxisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, tr_c, scaleCorner, cornerAxisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, bl_c, scaleCorner, cornerAxisThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, center_c, br_c, scaleCorner, cornerAxisThickness, invZoom, isActive);

  drawHandle(tl_c, scaleCorner);
  drawHandle(tr_c, scaleCorner);
  drawHandle(bl_c, scaleCorner);
  drawHandle(br_c, scaleCorner);
  drawHandle(tc_c, scaleY);
  drawHandle(bc_c, scaleY);
  drawHandle(lc_c, scaleX);
  drawHandle(rc_c, scaleX);

  const float centerHandleSize = std::max(10.0f, handleSize * 0.9f);
  const QRectF centerRect(center_c.x - centerHandleSize * 0.5f,
                          center_c.y - centerHandleSize * 0.5f,
                          centerHandleSize, centerHandleSize);
  drawEmphasizedRect(renderer, centerRect,
                     activeHandle_ == HandleType::Scale_Center ? FloatColor{1.0f, 0.92f, 0.35f, 1.0f}
                                                               : FloatColor{0.96f, 0.96f, 0.96f, 1.0f},
                     std::max(1.0f, 0.75f * invZoom), invZoom,
                     activeHandle_ != HandleType::None);
 }

 const auto& t3d = layer_->transform3D();
 const QPointF rotateCenterWorld = globalTransform.map(localRect.center());
 const float rotateBaseRadius = std::max({pointDistance(rotateCenterWorld, tl_c),
                                          pointDistance(rotateCenterWorld, tr_c),
                                          pointDistance(rotateCenterWorld, bl_c),
                                          pointDistance(rotateCenterWorld, br_c)});
 const float rotateRingRadius = rotateBaseRadius + std::max(40.0f * invZoom, 28.0f);
 const float rotateRingThickness = std::max(1.1f * invZoom, 0.9f);

 if (showRotate) {
  const FloatColor rotateOuter = isActive ? brighten(gizmoColor, 1.16f) : brighten(gizmoColor, 1.00f);
  const FloatColor rotateInner = isActive ? FloatColor{1.0f, 1.0f, 1.0f, 0.92f} : FloatColor{1.0f, 1.0f, 1.0f, 0.72f};
  const FloatColor rotateShadow = FloatColor{0.0f, 0.0f, 0.0f, activeHandle_ == HandleType::Rotate ? 0.48f : 0.34f};
  renderer->drawCircle(rotateCenterWorld.x() + std::max(1.2f, 1.1f * invZoom),
                       rotateCenterWorld.y() + std::max(1.2f, 1.1f * invZoom),
                       rotateRingRadius,
                       rotateShadow,
                       rotateRingThickness + std::max(1.0f, 0.8f * invZoom),
                       false);
  renderer->drawCircle(rotateCenterWorld.x(),
                       rotateCenterWorld.y(),
                       rotateRingRadius,
                       rotateOuter,
                       rotateRingThickness,
                       false);
  renderer->drawSolidLine({static_cast<float>(rotateCenterWorld.x()), static_cast<float>(rotateCenterWorld.y() - rotateRingRadius * 0.88f)},
                         {static_cast<float>(rotateCenterWorld.x()), static_cast<float>(rotateCenterWorld.y() - rotateRingRadius + std::max(3.0f, 6.0f * invZoom))},
                         brighten(rotateOuter, 0.86f),
                         std::max(1.5f, 2.1f * invZoom));
  renderer->drawCircle(rotateCenterWorld.x(),
                       rotateCenterWorld.y() - rotateRingRadius,
                       std::max(handleSize * 0.34f, 3.5f),
                       rotateOuter,
                       0.0f,
                       true);
  renderer->drawCircle(rotateCenterWorld.x(),
                       rotateCenterWorld.y() - rotateRingRadius,
                       std::max(handleSize * 0.14f, 1.8f),
                       rotateInner,
                       0.0f,
                       true);
 }

 // Anchor point: crosshair at anchor position
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
    const FloatColor snapColor{
        themeAccent.redF(),
        themeAccent.greenF(),
        themeAccent.blueF(),
        0.58f};
    if (sl.isVertical) {
     renderer->drawSolidLine({sl.position, 0.0f}, {sl.position, h}, snapColor, std::max(1.0f, 1.5f * invZoom));
    } else {
     renderer->drawSolidLine({0.0f, sl.position}, {w, sl.position}, snapColor, std::max(1.0f, 1.5f * invZoom));
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
 const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
 const float handleSize = std::clamp(HANDLE_SIZE * 1.65f * invZoom, 9.0f, 22.0f);
 
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
 if (allowsHandle(HandleType::Scale_Center)) {
  const QPointF centerPoint = localRect.center();
  const QPointF worldPoint = globalTransform.map(centerPoint);
  const auto vPos = renderer->canvasToViewport({static_cast<float>(worldPoint.x()),
                                                static_cast<float>(worldPoint.y())});
  const float centerHandleSize = std::max(10.0f, 10.0f * invZoom);
  if (QRectF(vPos.x - centerHandleSize * 0.5f,
             vPos.y - centerHandleSize * 0.5f,
             centerHandleSize, centerHandleSize).contains(viewportPos)) {
   return HandleType::Scale_Center;
  }
 }

 const auto& t3d = layer_->transform3D();

 // 2. Rotation handle: ring around the object, similar to 3D DCC rotate gizmos.
 {
 const QPointF rotateCenterWorld = globalTransform.map(localRect.center());
  const float rotateBaseRadius = std::max({pointDistance(rotateCenterWorld, localRect.topLeft()),
                                          pointDistance(rotateCenterWorld, localRect.topRight()),
                                          pointDistance(rotateCenterWorld, localRect.bottomLeft()),
                                          pointDistance(rotateCenterWorld, localRect.bottomRight())});
  const float rotateRingRadius = rotateBaseRadius + std::max(40.0f * invZoom, 28.0f);
  auto mouseCanvas = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
  const QPointF mousePos(mouseCanvas.x, mouseCanvas.y);
  const float ringHit = std::max(12.0f * invZoom, 9.0f);
  const float distToRing = std::abs(pointDistance(mousePos, rotateCenterWorld) - rotateRingRadius);
  if (allowsHandle(HandleType::Rotate) && distToRing <= ringHit) return HandleType::Rotate;

  const QPointF arrowTip(rotateCenterWorld.x(), rotateCenterWorld.y() - rotateRingRadius);
  const QPointF arrowBase(rotateCenterWorld.x(), rotateCenterWorld.y() - rotateRingRadius * 0.88f);
  const float arrowRadius = std::max(handleSize * 0.34f, 3.5f);
  const float arrowHit = std::max(4.5f, 4.0f * invZoom);
  if (allowsHandle(HandleType::Rotate)) {
   if (pointDistance(mousePos, arrowTip) <= arrowRadius + ringHit) {
    return HandleType::Rotate;
   }
   if (pointToSegmentDistance(mousePos, arrowBase, arrowTip) <= arrowHit) {
    return HandleType::Rotate;
   }
  }
 }

 // 3. Anchor point handle
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
 case HandleType::Scale_Center:
  return Qt::SizeAllCursor;
 case HandleType::Rotate:
   return Qt::CrossCursor;
 case HandleType::Anchor:
   return Qt::CrossCursor;
 default:
  return Qt::ArrowCursor;
 }
}

TransformGizmo::HandleType TransformGizmo::handleAtViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const
{
 return hitTest(viewportPos, renderer);
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
  dragStartFrame_ = layer_->currentFrame();
   dragStartLayerPos_ = QPointF(t3d.positionX(), t3d.positionY());
   dragStartScaleX_ = t3d.scaleX();
   dragStartScaleY_ = t3d.scaleY();
   dragStartRotation_ = t3d.rotation();
   dragStartHasPositionKey_ = t3d.hasPositionKeyFrameAt(ArtifactCore::RationalTime(dragStartFrame_, 24));
   dragStartHasRotationKey_ = t3d.hasRotationKeyFrameAt(ArtifactCore::RationalTime(dragStartFrame_, 24));
   dragStartHasScaleKey_ = t3d.hasScaleKeyFrameAt(ArtifactCore::RationalTime(dragStartFrame_, 24));
   dragStartGlobalTransform_ = layer_->getGlobalTransform();
   dragStartBoundingBox_ = layer_->transformedBoundingBox();
  dragStartLocalBounds_ = layer_->localBounds();
  dragStartAnchor_ = QPointF(t3d.anchorX(), t3d.anchorY());
  dragStartAnchorZ_ = t3d.anchorZ();
  return true;
 }
 return false;
}

bool TransformGizmo::allowsHandle(HandleType handle) const {
 switch (mode_) {
 case Mode::All:
  return handle != HandleType::None;
case Mode::Move:
  return handle == HandleType::Move;
  case Mode::Rotate:
   return handle == HandleType::Rotate;
case Mode::Scale:
  return handle == HandleType::Scale_TL || handle == HandleType::Scale_TR ||
         handle == HandleType::Scale_BL || handle == HandleType::Scale_BR ||
         handle == HandleType::Scale_T || handle == HandleType::Scale_B ||
         handle == HandleType::Scale_L || handle == HandleType::Scale_R ||
         handle == HandleType::Scale_Center;
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
   const QPointF pivotLocal = dragStartLocalBounds_.center();
   const QPointF pivotWorldStart = dragStartGlobalTransform_.map(pivotLocal);

   // Angle from pivot to start mouse position
   const double startAngle = std::atan2(dragStartCanvasPos_.y() - pivotWorldStart.y(),
                                        dragStartCanvasPos_.x() - pivotWorldStart.x());
   // Angle from pivot to current mouse position
   const double currentAngle = std::atan2(currentCanvasPos.y() - pivotWorldStart.y(),
                                          currentCanvasPos.x() - pivotWorldStart.x());
   
   const float deltaAngle = static_cast<float>((currentAngle - startAngle) * 180.0 / 3.14159265358979323846);
   const float newRotation = dragStartRotation_ + deltaAngle;
   t3d.setRotation(time, newRotation);

   const QPointF localOffset = pivotLocal - dragStartAnchor_;
   const QPointF startOffset = applyScaleRotateToVector(
       localOffset, dragStartScaleX_, dragStartScaleY_, dragStartRotation_);
   const QPointF newOffset = applyScaleRotateToVector(
       localOffset, dragStartScaleX_, dragStartScaleY_, newRotation);
   t3d.setPosition(time,
                   dragStartLayerPos_.x() + static_cast<float>(startOffset.x() - newOffset.x()),
                   dragStartLayerPos_.y() + static_cast<float>(startOffset.y() - newOffset.y()));
   layer_->setDirty(LayerDirtyFlag::Transform);
   Q_EMIT layer_->changed();
  } else if (activeHandle_ == HandleType::Scale_Center) {
   const QPointF pivotLocal = dragStartLocalBounds_.center();
   const QPointF pivotWorldStart = dragStartGlobalTransform_.map(pivotLocal);
   const QPointF startVec = dragStartCanvasPos_ - pivotWorldStart;
   const QPointF currentVec = currentCanvasPos - pivotWorldStart;
   const float startLen = std::max(1.0f, static_cast<float>(std::sqrt(startVec.x() * startVec.x() + startVec.y() * startVec.y())));
   const float currentLen = std::max(0.001f, static_cast<float>(std::sqrt(currentVec.x() * currentVec.x() + currentVec.y() * currentVec.y())));
   const float factor = std::clamp(currentLen / startLen, 0.05f, 100.0f);
   const float newScaleX = dragStartScaleX_ * factor;
   const float newScaleY = dragStartScaleY_ * factor;

   const QPointF localOffset = pivotLocal - dragStartAnchor_;
   const QPointF startOffset = applyScaleRotateToVector(
       localOffset, dragStartScaleX_, dragStartScaleY_, dragStartRotation_);
   const QPointF newOffset = applyScaleRotateToVector(
       localOffset, newScaleX, newScaleY, dragStartRotation_);

   t3d.setScale(time, newScaleX, newScaleY);
   t3d.setPosition(time,
                   dragStartLayerPos_.x() + static_cast<float>(startOffset.x() - newOffset.x()),
                   dragStartLayerPos_.y() + static_cast<float>(startOffset.y() - newOffset.y()));
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
 if (isDragging_ && layer_) {
  const auto& t3d = layer_->transform3D();
  const ArtifactCore::RationalTime time(dragStartFrame_, 24);
   const TransformSnapshot before{
    dragStartHasPositionKey_,
    dragStartHasRotationKey_,
    dragStartHasScaleKey_,
    static_cast<float>(dragStartLayerPos_.x()),
    static_cast<float>(dragStartLayerPos_.y()),
    dragStartRotation_,
    dragStartScaleX_,
    dragStartScaleY_,
    static_cast<float>(dragStartAnchor_.x()),
    static_cast<float>(dragStartAnchor_.y()),
    dragStartAnchorZ_
   };
  const TransformSnapshot after{
   t3d.hasPositionKeyFrameAt(time),
   t3d.hasRotationKeyFrameAt(time),
   t3d.hasScaleKeyFrameAt(time),
   t3d.positionX(),
   t3d.positionY(),
   t3d.rotation(),
   t3d.scaleX(),
   t3d.scaleY(),
   t3d.anchorX(),
   t3d.anchorY(),
   t3d.anchorZ()
  };

  const bool changed = before.hasPositionKey != after.hasPositionKey ||
                       before.hasRotationKey != after.hasRotationKey ||
                       before.hasScaleKey != after.hasScaleKey ||
                       std::abs(before.positionX - after.positionX) > 0.0001f ||
                       std::abs(before.positionY - after.positionY) > 0.0001f ||
                       std::abs(before.rotation - after.rotation) > 0.0001f ||
                       std::abs(before.scaleX - after.scaleX) > 0.0001f ||
                       std::abs(before.scaleY - after.scaleY) > 0.0001f;

  if (changed) {
   if (auto* mgr = UndoManager::instance()) {
    mgr->push(std::make_unique<TransformUndoCommand>(layer_, dragStartFrame_, before, after));
   }
  }
 }
 isDragging_ = false;
 activeHandle_ = HandleType::None;
 activeSnapLines_.clear();
}

}
