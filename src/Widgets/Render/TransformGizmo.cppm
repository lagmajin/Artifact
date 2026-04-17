module;
#include <utility>
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
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Undo.UndoManager;
import Event.Bus;
import Artifact.Event.Types;
import Color.Float;
import Time.Rational;
import Artifact.Service.Project;
import Widgets.Utils.CSS;
import ArtifactCore.Utils.PerformanceProfiler;

namespace Artifact {

namespace {
Q_LOGGING_CATEGORY(transformGizmoLog, "artifact.transformgizmo")

constexpr float kPi = 3.14159265358979323846f;

struct GizmoVisualStyle {
 static constexpr float scaleAxisThickness = 1.25f;
 static constexpr float scaleCornerThickness = 1.05f;
 static constexpr float scaleHandleBoost = 0.9f;
 static constexpr float scaleHandleOutset = 7.0f;
 static constexpr float scaleHandleSize = 0.82f;
 static constexpr float rotateRingPadding = 18.0f;
 static constexpr float rotateRingThickness = 3.0f;
 static constexpr float rotateRingInnerScale = 1.65f;
 static constexpr float rotateRingShadowBoost = 1.65f;
 static constexpr float rotateRingHitBoost = 2.2f;
 static constexpr float rotateGripPadding = 7.0f;
 static constexpr float rotateGripSize = 4.0f;
 static constexpr float rotateTickMinor = 24.0f;
 static constexpr float rotateTickMajor = 15.0f;
 static constexpr float rotateCardinalStep = 90.0f;
 static constexpr float rotateArcStep = 8.0f;
 static constexpr float centerMarkRadius = 3.5f;
};

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

struct RotateRingGeometry {
 QPointF centerWorld;
 float baseRadius = 0.0f;
 float ringRadius = 0.0f;
 float innerRadius = 0.0f;
 float outerRadius = 0.0f;
 float ringThickness = 0.0f;
 float hitRadius = 0.0f;
 float hitThickness = 0.0f;
 float gripRadius = 0.0f;
 float gripSize = 0.0f;
};

RotateRingGeometry makeVisualRotateRingGeometry(const RotateRingGeometry& geo)
{
 RotateRingGeometry visual = geo;
 visual.ringRadius = std::max(12.0f, geo.ringRadius * 0.5f);
 visual.ringThickness = std::max(1.4f, geo.ringThickness * 0.75f);
 visual.innerRadius = std::max(1.0f,
                               visual.ringRadius - visual.ringThickness * GizmoVisualStyle::rotateRingInnerScale);
 visual.outerRadius = visual.ringRadius + visual.ringThickness * GizmoVisualStyle::rotateRingShadowBoost;
 visual.gripRadius = visual.ringRadius + std::max(GizmoVisualStyle::rotateGripPadding * 0.5f, 4.0f);
 visual.gripSize = std::max(GizmoVisualStyle::rotateGripSize * 0.75f, 3.0f);
 return visual;
}

RotateRingGeometry computeRotateRingGeometry(const QRectF& localRect,
                                            const QTransform& globalTransform,
                                            float invZoom)
{
 RotateRingGeometry geo;
 geo.centerWorld = globalTransform.map(localRect.center());

 const QPointF tl = globalTransform.map(localRect.topLeft());
 const QPointF tr = globalTransform.map(localRect.topRight());
 const QPointF bl = globalTransform.map(localRect.bottomLeft());
 const QPointF br = globalTransform.map(localRect.bottomRight());

 geo.baseRadius = std::max({pointDistance(geo.centerWorld, tl),
                            pointDistance(geo.centerWorld, tr),
                            pointDistance(geo.centerWorld, bl),
                            pointDistance(geo.centerWorld, br)});
 geo.ringRadius = geo.baseRadius + std::max(GizmoVisualStyle::rotateRingPadding * invZoom, 14.0f);
 geo.ringThickness = std::max(GizmoVisualStyle::rotateRingThickness * invZoom, 2.2f);
 geo.innerRadius = std::max(1.0f, geo.ringRadius - geo.ringThickness * GizmoVisualStyle::rotateRingInnerScale);
 geo.outerRadius = geo.ringRadius + geo.ringThickness * GizmoVisualStyle::rotateRingShadowBoost;
 geo.hitRadius = geo.ringRadius;
 geo.hitThickness = std::max(geo.ringThickness * GizmoVisualStyle::rotateRingHitBoost, 9.0f * invZoom);
 geo.gripRadius = geo.ringRadius + std::max(GizmoVisualStyle::rotateGripPadding * invZoom, 7.0f);
 geo.gripSize = std::max(GizmoVisualStyle::rotateGripSize * invZoom, 4.0f);
 return geo;
}

float angleDegreesAround(const QPointF& center, const QPointF& point)
{
 return static_cast<float>(std::atan2(point.y() - center.y(), point.x() - center.x()) *
                           180.0 / kPi);
}

float normalizeAngleDeltaDegrees(float deltaDegrees)
{
 while (deltaDegrees > 180.0f) {
  deltaDegrees -= 360.0f;
 }
 while (deltaDegrees < -180.0f) {
  deltaDegrees += 360.0f;
 }
 return deltaDegrees;
}

QPointF pointOnCircle(const QPointF& center, float radius, float angleDegrees)
{
 const float radians = angleDegrees * (kPi / 180.0f);
 return QPointF(center.x() + std::cos(radians) * radius,
                center.y() + std::sin(radians) * radius);
}

void drawArc(ArtifactIRenderer* renderer,
             const QPointF& center,
             float radius,
             float startAngleDegrees,
             float sweepDegrees,
             const FloatColor& color,
             float thickness)
{
 if (!renderer || radius <= 0.0f || std::abs(sweepDegrees) <= 0.01f) {
  return;
 }

 const int segments = std::max(12, static_cast<int>(std::ceil(std::abs(sweepDegrees) / GizmoVisualStyle::rotateArcStep)));
 
 // Optimization: Pre-calculate geometry to reduce per-iteration overhead
 QVector<Detail::float2> points(segments + 1);
 const float angleStep = sweepDegrees / segments;
 const float radStep = angleStep * (kPi / 180.0f);
 const float radStart = startAngleDegrees * (kPi / 180.0f);

 // Calculate points
 for (int i = 0; i <= segments; ++i) {
  const float angle = radStart + i * radStep;
  points[i] = { static_cast<float>(center.x() + std::cos(angle) * radius),
                static_cast<float>(center.y() + std::sin(angle) * radius) };
 }

  // Draw segments as a single polyline
  std::vector<Detail::float2> vecPoints(points.begin(), points.end());
  renderer->drawPolyline(vecPoints, color, thickness);
}

void drawRotateTickMarks(ArtifactIRenderer* renderer,
                         const RotateRingGeometry& geo,
                         const FloatColor& minorColor,
                         const FloatColor& majorColor,
                         float invZoom)
{
 if (!renderer) {
  return;
 }

 const float minorLength = std::max(5.0f * invZoom, 4.0f);
 const float majorLength = std::max(9.0f * invZoom, 6.0f);
 const float minorThickness = std::max(0.9f, 0.8f * invZoom);
 const float majorThickness = std::max(1.15f, 1.0f * invZoom);

 for (int step = 0; step < 24; ++step) {
  const float angle = -90.0f + static_cast<float>(step) * GizmoVisualStyle::rotateTickMajor;
  const bool isMajor = (step % 6) == 0;
  const float tickLength = isMajor ? majorLength : minorLength;
  const QPointF outer = pointOnCircle(geo.centerWorld, geo.outerRadius + tickLength, angle);
  const QPointF inner = pointOnCircle(geo.centerWorld, geo.outerRadius + std::max(1.4f * invZoom, 1.0f), angle);
  const FloatColor color = isMajor ? majorColor : minorColor;
  renderer->drawSolidLine({static_cast<float>(inner.x()), static_cast<float>(inner.y())},
                          {static_cast<float>(outer.x()), static_cast<float>(outer.y())},
                          color, isMajor ? majorThickness : minorThickness);
 }
}

void drawRotateLeader(ArtifactIRenderer* renderer,
                      const QPointF& center,
                      const QPointF& grip,
                      const FloatColor& color,
                      float invZoom)
{
 if (!renderer) {
  return;
 }

 const float leaderThickness = std::max(1.2f, 1.1f * invZoom);
 renderer->drawSolidLine({static_cast<float>(center.x()), static_cast<float>(center.y())},
                         {static_cast<float>(grip.x()), static_cast<float>(grip.y())},
                         color, leaderThickness);

 const QPointF dir = grip - center;
 const float len = std::max(1.0f, static_cast<float>(std::sqrt(dir.x() * dir.x() + dir.y() * dir.y())));
 const QPointF n(dir.x() / len, dir.y() / len);
 const QPointF t(-n.y(), n.x());
 const float arrowLen = std::max(7.0f * invZoom, 5.0f);
 const QPointF tip = grip;
 const QPointF back = tip - n * arrowLen;
 renderer->drawSolidLine({static_cast<float>(back.x() + t.x() * arrowLen * 0.45f),
                          static_cast<float>(back.y() + t.y() * arrowLen * 0.45f)},
                         {static_cast<float>(tip.x()),
                          static_cast<float>(tip.y())},
                         color, leaderThickness);
 renderer->drawSolidLine({static_cast<float>(back.x() - t.x() * arrowLen * 0.45f),
                          static_cast<float>(back.y() - t.y() * arrowLen * 0.45f)},
                         {static_cast<float>(tip.x()),
                          static_cast<float>(tip.y())},
                         color, leaderThickness);
}

void drawRotateCardinalMarks(ArtifactIRenderer* renderer,
                             const RotateRingGeometry& geo,
                             const FloatColor& majorColor,
                             const FloatColor& minorColor,
                             float invZoom)
{
 if (!renderer) {
  return;
 }

 const float majorLength = std::max(12.0f * invZoom, 8.0f);
 const float minorLength = std::max(6.0f * invZoom, 4.5f);
 const float majorThickness = std::max(1.4f, 1.2f * invZoom);
 const float minorThickness = std::max(1.0f, 0.9f * invZoom);

 for (int i = 0; i < 4; ++i) {
  const float angle = -90.0f + static_cast<float>(i) * GizmoVisualStyle::rotateCardinalStep;
  const bool isMajor = (i % 2) == 0;
  const float outLength = geo.outerRadius + (isMajor ? majorLength : minorLength);
  const QPointF inner = pointOnCircle(geo.centerWorld, geo.outerRadius + std::max(1.0f, 0.8f * invZoom), angle);
  const QPointF outer = pointOnCircle(geo.centerWorld, outLength, angle);
  const FloatColor color = isMajor ? majorColor : minorColor;
  renderer->drawSolidLine({static_cast<float>(inner.x()), static_cast<float>(inner.y())},
                          {static_cast<float>(outer.x()), static_cast<float>(outer.y())},
                          color, isMajor ? majorThickness : minorThickness);
 }
}

void drawArrowHead(ArtifactIRenderer* renderer,
                   const QPointF& tip,
                   const QPointF& dir,
                   const FloatColor& color,
                   float invZoom)
{
 if (!renderer) {
  return;
 }

 const float len = std::max(1.0f, static_cast<float>(std::sqrt(dir.x() * dir.x() + dir.y() * dir.y())));
 const QPointF n(dir.x() / len, dir.y() / len);
 const QPointF t(-n.y(), n.x());
 const float arrowLen = std::max(12.0f * invZoom, 8.0f);
 const QPointF base = tip - n * arrowLen;
 renderer->drawSolidLine({static_cast<float>(base.x() + t.x() * arrowLen * 0.55f),
                          static_cast<float>(base.y() + t.y() * arrowLen * 0.55f)},
                         {static_cast<float>(tip.x()), static_cast<float>(tip.y())},
                         color, std::max(1.6f, 1.25f * invZoom));
 renderer->drawSolidLine({static_cast<float>(base.x() - t.x() * arrowLen * 0.55f),
                          static_cast<float>(base.y() - t.y() * arrowLen * 0.55f)},
                         {static_cast<float>(tip.x()), static_cast<float>(tip.y())},
                         color, std::max(1.6f, 1.25f * invZoom));
}

void drawMoveArrow(ArtifactIRenderer* renderer,
                   const QPointF& center,
                   const QPointF& tip,
                   const FloatColor& color,
                   float invZoom,
                   bool active)
{
 if (!renderer) {
  return;
 }

 const QPointF dir = tip - center;
 const float len = std::max(1.0f, static_cast<float>(std::sqrt(dir.x() * dir.x() + dir.y() * dir.y())));
 const QPointF n(dir.x() / len, dir.y() / len);
 const float arrowLen = std::max(12.0f * invZoom, 8.0f);
 const QPointF shaftEnd = tip - n * arrowLen;
 const FloatColor shadow{0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f};
 renderer->drawSolidLine({static_cast<float>(center.x() + 1.0f * invZoom),
                          static_cast<float>(center.y() + 1.0f * invZoom)},
                         {static_cast<float>(shaftEnd.x() + 1.0f * invZoom),
                          static_cast<float>(shaftEnd.y() + 1.0f * invZoom)},
                         shadow, std::max(1.6f, 1.25f * invZoom));
 renderer->drawSolidLine({static_cast<float>(center.x()), static_cast<float>(center.y())},
                         {static_cast<float>(shaftEnd.x()), static_cast<float>(shaftEnd.y())},
                         brighten(color, active ? 1.08f : 0.96f),
                         std::max(1.6f, 1.25f * invZoom));
  renderer->drawSolidLine({static_cast<float>(center.x() - 0.35f * invZoom),
                           static_cast<float>(center.y() - 0.35f * invZoom)},
                          {static_cast<float>(shaftEnd.x() - 0.35f * invZoom),
                           static_cast<float>(shaftEnd.y() - 0.35f * invZoom)},
                          brighten(color, 1.18f), std::max(1.1f, 0.9f * invZoom));
  drawArrowHead(renderer, tip, dir, color, invZoom);
}

void drawBoxHandle(ArtifactIRenderer* renderer,
                   const QPointF& center,
                   float size,
                   const FloatColor& fill,
                   const FloatColor& outline,
                   float invZoom);

void drawScaleCenterHandle(ArtifactIRenderer* renderer,
                           const QPointF& center,
                           const QPointF& xAxisDir,
                           const QPointF& yAxisDir,
                           float invZoom,
                           bool active)
{
 if (!renderer) {
  return;
 }

 const float shaftLen = std::max(14.0f, 16.0f * invZoom);
 const float handleSize = std::clamp(8.0f + 5.0f * invZoom, 8.0f, 15.0f);
 const FloatColor xColor = active ? FloatColor{1.0f, 0.38f, 0.15f, 1.0f}
                                  : FloatColor{0.98f, 0.18f, 0.06f, 1.0f};
 const FloatColor yColor = active ? FloatColor{0.25f, 1.0f, 0.38f, 1.0f}
                                  : FloatColor{0.08f, 0.86f, 0.22f, 1.0f};
 const FloatColor outline = active ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
                                   : FloatColor{0.12f, 0.12f, 0.12f, 1.0f};

 const QPointF xTip = center + xAxisDir * shaftLen;
 const QPointF yTip = center + yAxisDir * shaftLen;
 const FloatColor xShadow{0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f};
 const FloatColor yShadow{0.0f, 0.0f, 0.0f, active ? 0.42f : 0.30f};
 const float lineThickness = std::max(1.4f, 1.15f * invZoom);

 renderer->drawSolidLine({static_cast<float>(center.x() + 1.0f * invZoom),
                          static_cast<float>(center.y() + 1.0f * invZoom)},
                         {static_cast<float>(xTip.x() + 1.0f * invZoom),
                          static_cast<float>(xTip.y() + 1.0f * invZoom)},
                         xShadow, lineThickness);
 renderer->drawSolidLine({static_cast<float>(center.x()), static_cast<float>(center.y())},
                         {static_cast<float>(xTip.x()), static_cast<float>(xTip.y())},
                         brighten(xColor, active ? 1.06f : 0.98f), lineThickness);
 drawBoxHandle(renderer, xTip, handleSize, brighten(xColor, 1.08f), outline, invZoom);

 renderer->drawSolidLine({static_cast<float>(center.x() + 1.0f * invZoom),
                          static_cast<float>(center.y() + 1.0f * invZoom)},
                         {static_cast<float>(yTip.x() + 1.0f * invZoom),
                          static_cast<float>(yTip.y() + 1.0f * invZoom)},
                         yShadow, lineThickness);
 renderer->drawSolidLine({static_cast<float>(center.x()), static_cast<float>(center.y())},
                         {static_cast<float>(yTip.x()), static_cast<float>(yTip.y())},
                         brighten(yColor, active ? 1.06f : 0.98f), lineThickness);
 drawBoxHandle(renderer, yTip, handleSize, brighten(yColor, 1.08f), outline, invZoom);

 drawBoxHandle(renderer, center, std::clamp(handleSize * 1.08f, 8.0f, 16.0f),
               active ? FloatColor{1.0f, 0.92f, 0.35f, 1.0f}
                      : FloatColor{0.95f, 0.95f, 0.98f, 0.96f},
               outline, invZoom);
}

float distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
 const QPointF ab = b - a;
 const QPointF ap = p - a;
 const float abLenSq = static_cast<float>(ab.x() * ab.x() + ab.y() * ab.y());
 if (abLenSq <= 0.0001f) {
  return pointDistance(p, a);
 }
 float t = static_cast<float>((ap.x() * ab.x() + ap.y() * ab.y()) / abLenSq);
 t = std::clamp(t, 0.0f, 1.0f);
 const QPointF proj = a + ab * t;
 return pointDistance(p, proj);
}

void drawBoxHandle(ArtifactIRenderer* renderer,
                   const QPointF& center,
                   float size,
                   const FloatColor& fill,
                   const FloatColor& outline,
                   float invZoom)
{
 if (!renderer) {
  return;
 }

 const float half = size * 0.5f;
 renderer->drawSolidRect(static_cast<float>(center.x() - half),
                         static_cast<float>(center.y() - half),
                         size, size,
                         FloatColor{0.0f, 0.0f, 0.0f, 0.34f}, 1.0f);
 renderer->drawSolidRect(static_cast<float>(center.x() - half + std::max(0.8f, 0.6f * invZoom)),
                         static_cast<float>(center.y() - half + std::max(0.8f, 0.6f * invZoom)),
                         size - std::max(1.6f, 1.2f * invZoom),
                         size - std::max(1.6f, 1.2f * invZoom),
                         brighten(fill, 1.08f), 1.0f);
 renderer->drawRectOutline(static_cast<float>(center.x() - half),
                           static_cast<float>(center.y() - half),
                           size, size, outline);
}

QPointF offsetPointAwayFromCenter(const QPointF& center, const QPointF& point, float amount)
{
 const QPointF delta = point - center;
 const float len = std::max(1.0f, static_cast<float>(std::sqrt(delta.x() * delta.x() + delta.y() * delta.y())));
 return point + QPointF(delta.x() / len * amount, delta.y() / len * amount);
}

QPointF offsetPointAwayFromCenter(const QPointF& center, const Detail::float2& point, float amount)
{
 return offsetPointAwayFromCenter(center, QPointF(point.x, point.y), amount);
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
  if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
   ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
       LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                         LayerChangedEvent::ChangeType::Modified});
  }
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
 const float shadowShift = std::max(0.7f, 0.65f * invZoom);
 const float shadowThickness = thickness + std::max(0.55f, 0.45f * invZoom);
 const float mainThickness = std::max(1.0f, thickness + std::max(0.2f, 0.18f * invZoom));
 const FloatColor shadow = { 0.0f, 0.0f, 0.0f, active ? 0.30f : 0.18f };
 const FloatColor mainColor = brighten(color, active ? 1.08f : 1.0f);
  renderer->drawSolidLine({a.x + shadowShift, a.y + shadowShift},
                           {b.x + shadowShift, b.y + shadowShift},
                           shadow, shadowThickness);
  renderer->drawSolidLine(a, b, mainColor, mainThickness);
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

void TransformGizmo::setLayer(ArtifactAbstractLayerPtr layer) {
  layer_ = std::move(layer);
  // レイヤーが変更されたらキャッシュを無効化
  geometryCacheValid_ = false;
  if (!isDragging_) {
    activeHandle_ = HandleType::None;
  }
}

void TransformGizmo::updateGeometryCache(const QTransform& globalTransform, const QRectF& localRect, float zoom) {
  // バウンディングボックスの4頂点と中心・上下左右のポイントを事前計算
  cachedPoints_.tl = Detail::float2((float)globalTransform.map(localRect.topLeft()).x(), (float)globalTransform.map(localRect.topLeft()).y());
  cachedPoints_.tr = Detail::float2((float)globalTransform.map(localRect.topRight()).x(), (float)globalTransform.map(localRect.topRight()).y());
  cachedPoints_.bl = Detail::float2((float)globalTransform.map(localRect.bottomLeft()).x(), (float)globalTransform.map(localRect.bottomLeft()).y());
  cachedPoints_.br = Detail::float2((float)globalTransform.map(localRect.bottomRight()).x(), (float)globalTransform.map(localRect.bottomRight()).y());

  cachedPoints_.center = globalTransform.map(localRect.center());
  cachedPoints_.top = globalTransform.map(QPointF(localRect.center().x(), localRect.top()));
  cachedPoints_.bottom = globalTransform.map(QPointF(localRect.center().x(), localRect.bottom()));
  cachedPoints_.left = globalTransform.map(QPointF(localRect.left(), localRect.center().y()));
  cachedPoints_.right = globalTransform.map(QPointF(localRect.right(), localRect.center().y()));

  cachedZoom_ = zoom;
  cachedLayerTransform_ = globalTransform;
  cachedLocalRect_ = localRect;
  geometryCacheValid_ = true;
}

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

static constexpr double HANDLE_SIZE = 8.0;
static constexpr double ROTATE_HANDLE_DISTANCE = 28.0;
static constexpr double GIZMO_OFFSET = 0.0;  // was 15.0; handles use offsetPointAwayFromCenter so no border expansion needed
static constexpr int TRANSFORM_KEYFRAME_SCALE = 24;

void TransformGizmo::draw(ArtifactIRenderer* renderer) {
 // 5. 不要時の描画スキップ: 早期リターンで無駄な計算を防止
 if (!layer_ || !renderer) {
  return;
 }

 // レイヤーが非表示ならギズモも描画しない
 if (!layer_->isVisible()) {
  return;
 }

 // 選択されていない場合は描画しない（外部で制御されているべきだが念のため）
 if (!isSelected_) {
  return;
 }

 ArtifactCore::ProfileScope _profGizmoDraw(
     "TransformGizmoDraw", ArtifactCore::ProfileCategory::Render);

 const float zoom = renderer->getZoom();
 const float invZoom = zoom > 0.0001f ? 1.0f / zoom : 1.0f;
 const float lineThickness = std::clamp(3.0f * invZoom, 1.65f, 5.4f);
 const float handleSize = std::clamp(HANDLE_SIZE * 1.62f * invZoom, 9.0f, 21.0f);

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
 if (isDragging_) gizmoColor = {1.0f, 0.88f, 0.22f, 1.0f};
 const bool isActive = isDragging_ || activeHandle_ != HandleType::None;

 const bool showMove = mode_ == Mode::All || mode_ == Mode::Move;
 const bool showScale = mode_ == Mode::All || mode_ == Mode::Scale;
 const bool showRotate = mode_ == Mode::Rotate;
 const bool showAnchor = mode_ == Mode::All;

 // 1. ジオメトリキャッシュ: 毎フレーム再生成していた計算結果をキャッシュ
 // ズーム値やレイヤー変更に伴ってキャッシュを更新
 const bool needsGeometryUpdate = !geometryCacheValid_ ||
                                   cachedZoom_ != zoom ||
                                   cachedLayerTransform_ != globalTransform ||
                                   cachedLocalRect_ != localRect;

 if (needsGeometryUpdate) {
  updateGeometryCache(globalTransform, localRect, zoom);
 }

 // キャッシュからポイントを使用
 const Detail::float2 tl_c = cachedPoints_.tl;
 const Detail::float2 tr_c = cachedPoints_.tr;
 const Detail::float2 bl_c = cachedPoints_.bl;
 const Detail::float2 br_c = cachedPoints_.br;
 const QPointF centerPoint = cachedPoints_.center;
 const QPointF topPoint = cachedPoints_.top;
 const QPointF bottomPoint = cachedPoints_.bottom;
 const QPointF leftPoint = cachedPoints_.left;
 const QPointF rightPoint = cachedPoints_.right;

 if (showMove) {
  ArtifactCore::ProfileScope _profMove(
      "TransformGizmoMove", ArtifactCore::ProfileCategory::Render);
  // X (red) and Y (green) axis arrows originating at the layer's visual center.
  const QPointF moveCenterW = globalTransform.map(localRect.center());
  const bool moveActive = activeHandle_ == HandleType::Move;

  // Compute local X (+right) and visual Y (+up = canvas -Y) directions in world space.
  const QPointF mapOrigin = globalTransform.map(QPointF(0.0, 0.0));
  QPointF xAxisRaw = globalTransform.map(QPointF(1.0, 0.0)) - mapOrigin;
  const float xALen = static_cast<float>(
      std::sqrt(xAxisRaw.x() * xAxisRaw.x() + xAxisRaw.y() * xAxisRaw.y()));
  const QPointF xAxisDir = xALen > 0.001f ? xAxisRaw / xALen : QPointF(1.0, 0.0);
  QPointF yAxisRaw = globalTransform.map(QPointF(0.0, -1.0)) - mapOrigin;
  const float yALen = static_cast<float>(
      std::sqrt(yAxisRaw.x() * yAxisRaw.x() + yAxisRaw.y() * yAxisRaw.y()));
  const QPointF yAxisDir = yALen > 0.001f ? yAxisRaw / yALen : QPointF(0.0, -1.0);

  const float arrowCanvasLen = 50.0f * invZoom;
  const QPointF xTip = moveCenterW + xAxisDir * arrowCanvasLen;
  const QPointF yTip = moveCenterW + yAxisDir * arrowCanvasLen;

  const FloatColor xColor = moveActive ? FloatColor{1.0f, 0.38f, 0.15f, 1.0f}
                                       : FloatColor{0.98f, 0.18f, 0.06f, 1.0f};
  const FloatColor yColor = moveActive ? FloatColor{0.25f, 1.0f, 0.38f, 1.0f}
                                       : FloatColor{0.08f, 0.86f, 0.22f, 1.0f};
  drawMoveArrow(renderer, moveCenterW, xTip, xColor, invZoom, moveActive);
  drawMoveArrow(renderer, moveCenterW, yTip, yColor, invZoom, moveActive);

  // Dark filled dot at origin
  const float dotRadius = std::max(4.5f, 4.0f * invZoom);
  renderer->drawCircle((float)moveCenterW.x() + std::max(1.0f, 0.9f * invZoom),
                       (float)moveCenterW.y() + std::max(1.0f, 0.9f * invZoom),
                       dotRadius, {0.0f, 0.0f, 0.0f, moveActive ? 0.45f : 0.35f}, 0.0f, true);
  renderer->drawCircle((float)moveCenterW.x(), (float)moveCenterW.y(), dotRadius,
                       moveActive ? FloatColor{1.0f, 0.82f, 0.28f, 1.0f}
                                  : FloatColor{0.20f, 0.20f, 0.22f, 0.94f},
                       0.0f, true);
 }

 if (showScale) {
  ArtifactCore::ProfileScope _profScale(
      "TransformGizmoScale", ArtifactCore::ProfileCategory::Render);
  drawEmphasizedLine(renderer, tl_c, tr_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, tr_c, br_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, br_c, bl_c, gizmoColor, lineThickness, invZoom, isActive);
  drawEmphasizedLine(renderer, bl_c, tl_c, gizmoColor, lineThickness, invZoom, isActive);

   const float handleSizeScale = std::clamp(handleSize * GizmoVisualStyle::scaleHandleSize, 7.0f, 16.0f);
   const FloatColor cornerFill = activeHandle_ == HandleType::Scale_TL ||
                                 activeHandle_ == HandleType::Scale_TR ||
                                 activeHandle_ == HandleType::Scale_BL ||
                                 activeHandle_ == HandleType::Scale_BR
       ? FloatColor{1.0f, 0.95f, 0.72f, 1.0f}
       : FloatColor{0.94f, 0.96f, 0.98f, 0.96f};
   const FloatColor axisFill = activeHandle_ == HandleType::Scale_T ||
                               activeHandle_ == HandleType::Scale_B ||
                               activeHandle_ == HandleType::Scale_L ||
                               activeHandle_ == HandleType::Scale_R
       ? FloatColor{0.68f, 0.96f, 0.82f, 1.0f}
       : FloatColor{0.90f, 0.94f, 0.96f, 0.95f};
   const FloatColor boxOutline = activeHandle_ != HandleType::None
        ? FloatColor{1.0f, 1.0f, 1.0f, 1.0f}
        : FloatColor{0.12f, 0.12f, 0.12f, 1.0f};

   const float outward = std::max(4.2f, GizmoVisualStyle::scaleHandleOutset * 0.58f * invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, tl_c, outward), handleSizeScale, cornerFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, tr_c, outward), handleSizeScale, cornerFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, bl_c, outward), handleSizeScale, cornerFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, br_c, outward), handleSizeScale, cornerFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, topPoint, outward * 0.85f), handleSizeScale * 0.92f, axisFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, bottomPoint, outward * 0.85f), handleSizeScale * 0.92f, axisFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, leftPoint, outward * 0.85f), handleSizeScale * 0.92f, axisFill, boxOutline, invZoom);
   drawBoxHandle(renderer, offsetPointAwayFromCenter(centerPoint, rightPoint, outward * 0.85f), handleSizeScale * 0.92f, axisFill, boxOutline, invZoom);

  if (mode_ == Mode::Scale) {
   const QPointF mapOrigin = globalTransform.map(QPointF(0.0, 0.0));
   QPointF xAxisRaw = globalTransform.map(QPointF(1.0, 0.0)) - mapOrigin;
   const float xALen = static_cast<float>(
       std::sqrt(xAxisRaw.x() * xAxisRaw.x() + xAxisRaw.y() * xAxisRaw.y()));
   const QPointF xAxisDir = xALen > 0.001f ? xAxisRaw / xALen : QPointF(1.0, 0.0);
   QPointF yAxisRaw = globalTransform.map(QPointF(0.0, -1.0)) - mapOrigin;
   const float yALen = static_cast<float>(
       std::sqrt(yAxisRaw.x() * yAxisRaw.x() + yAxisRaw.y() * yAxisRaw.y()));
   const QPointF yAxisDir = yALen > 0.001f ? yAxisRaw / yALen : QPointF(0.0, -1.0);
   drawScaleCenterHandle(renderer, centerPoint, xAxisDir, yAxisDir, invZoom, activeHandle_ == HandleType::Scale_Center);
  } else {
   const FloatColor centerMark = activeHandle_ == HandleType::Move
       ? FloatColor{1.0f, 0.82f, 0.28f, 1.0f}
       : FloatColor{0.82f, 0.86f, 0.92f, 0.92f};
   const float centerMarkSize = std::max(4.5f, GizmoVisualStyle::centerMarkRadius * invZoom);
   renderer->drawCircle(static_cast<float>(centerPoint.x()),
                        static_cast<float>(centerPoint.y()),
                        centerMarkSize,
                        centerMark, std::max(1.0f, 0.9f * invZoom), false);
  }
  }

 // Center points for handles
 const Detail::float2 tc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.top())).y());
 const Detail::float2 bc_c((float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).x(), (float)globalTransform.map(QPointF(localRect.center().x(), localRect.bottom())).y());
 const Detail::float2 lc_c((float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.left(), localRect.center().y())).y());
 const Detail::float2 rc_c((float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).x(), (float)globalTransform.map(QPointF(localRect.right(), localRect.center().y())).y());

 if (showScale) {
  const Detail::float2 center_c((float)globalTransform.map(localRect.center()).x(), (float)globalTransform.map(localRect.center()).y());
  const FloatColor scaleX = activeHandle_ == HandleType::Scale_L || activeHandle_ == HandleType::Scale_R
      ? FloatColor{1.0f, 0.24f, 0.10f, 1.0f}
      : FloatColor{0.98f, 0.18f, 0.06f, 1.0f};
  const FloatColor scaleY = activeHandle_ == HandleType::Scale_T || activeHandle_ == HandleType::Scale_B
      ? FloatColor{0.18f, 1.0f, 0.30f, 1.0f}
      : FloatColor{0.08f, 0.86f, 0.22f, 1.0f};

  const float centerHandleSize = std::max(8.5f, handleSize * GizmoVisualStyle::scaleHandleBoost);
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
 const RotateRingGeometry rotateGeo =
     computeRotateRingGeometry(localRect, globalTransform, invZoom);
 const RotateRingGeometry visualRotateGeo = makeVisualRotateRingGeometry(rotateGeo);

 if (showRotate) {
  ArtifactCore::ProfileScope _profRotate(
      "TransformGizmoRotate", ArtifactCore::ProfileCategory::Render);
  const bool rotateActive = activeHandle_ == HandleType::Rotate;
  const bool rotateModeSelected = mode_ == Mode::Rotate;
  const bool rotateEmphasis = rotateActive || rotateModeSelected;
  const FloatColor ringShadow =
      FloatColor{0.0f, 0.0f, 0.0f, rotateEmphasis ? 0.28f : 0.16f};
  const FloatColor ringBase = rotateEmphasis
      ? FloatColor{0.18f, 0.20f, 0.24f, rotateActive ? 0.96f : 0.84f}
      : FloatColor{0.18f, 0.20f, 0.23f, 0.52f};
  const FloatColor ringInner = rotateEmphasis
      ? FloatColor{0.12f, 0.13f, 0.16f, rotateActive ? 0.72f : 0.52f}
      : FloatColor{0.10f, 0.11f, 0.13f, 0.34f};
  const FloatColor horizontalColor = rotateEmphasis
      ? FloatColor{1.0f, 0.44f, 0.18f, rotateActive ? 0.96f : 0.84f}
      : FloatColor{0.62f, 0.38f, 0.28f, 0.58f};
  const FloatColor verticalColor = rotateEmphasis
      ? FloatColor{0.26f, 0.84f, 0.52f, rotateActive ? 0.96f : 0.84f}
      : FloatColor{0.28f, 0.56f, 0.42f, 0.58f};
  const FloatColor gripAccent =
      rotateEmphasis ? FloatColor{1.0f, 0.90f, 0.34f, 1.0f}
                     : FloatColor{0.88f, 0.90f, 0.94f, 0.82f};
  const float shadowOffset = std::max(1.2f, 1.0f * invZoom);
  const float segmentSweep = 68.0f;

  renderer->drawCircle(visualRotateGeo.centerWorld.x() + shadowOffset,
                       visualRotateGeo.centerWorld.y() + shadowOffset,
                       visualRotateGeo.ringRadius,
                       ringShadow,
                       visualRotateGeo.ringThickness + std::max(0.8f, 0.65f * invZoom),
                       false);
  renderer->drawCircle(visualRotateGeo.centerWorld.x(),
                       visualRotateGeo.centerWorld.y(),
                       visualRotateGeo.ringRadius,
                       ringBase,
                       visualRotateGeo.ringThickness,
                       false);
  renderer->drawCircle(visualRotateGeo.centerWorld.x(),
                       visualRotateGeo.centerWorld.y(),
                       visualRotateGeo.innerRadius,
                       ringInner,
                       std::max(0.6f, 0.55f * invZoom),
                       false);

  // 2D rotate ring: color the horizontal and vertical directions separately
  // so the handle reads like a DCC-style axis-aware ring instead of a white halo.
  drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
          -segmentSweep * 0.5f, segmentSweep, horizontalColor,
          visualRotateGeo.ringThickness);
  drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
          180.0f - segmentSweep * 0.5f, segmentSweep, horizontalColor,
          visualRotateGeo.ringThickness);
  drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
          90.0f - segmentSweep * 0.5f, segmentSweep, verticalColor,
          visualRotateGeo.ringThickness);
  drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
          270.0f - segmentSweep * 0.5f, segmentSweep, verticalColor,
          visualRotateGeo.ringThickness);

  const float restAngle = rotateActive
      ? dragStartPointerAngle_ + std::remainder(dragAccumulatedRotationDelta_, 360.0f)
      : -90.0f;
  const QPointF gripCenter = pointOnCircle(visualRotateGeo.centerWorld, visualRotateGeo.gripRadius, restAngle);
  drawRotateLeader(renderer, visualRotateGeo.centerWorld, gripCenter,
                   gripAccent,
                   invZoom);
  renderer->drawCircle(static_cast<float>(gripCenter.x()) + shadowOffset,
                       static_cast<float>(gripCenter.y()) + shadowOffset,
                       visualRotateGeo.gripSize + std::max(0.8f * invZoom, 0.7f),
                       FloatColor{0.0f, 0.0f, 0.0f, 0.30f},
                       0.0f, true);
  renderer->drawCircle(static_cast<float>(gripCenter.x()),
                       static_cast<float>(gripCenter.y()),
                       visualRotateGeo.gripSize + std::max(0.7f * invZoom, 0.6f),
                       rotateEmphasis ? FloatColor{1.0f, 0.72f, 0.18f, 0.90f}
                                      : FloatColor{0.28f, 0.34f, 0.38f, 0.78f},
                       0.0f, true);
  renderer->drawCircle(static_cast<float>(gripCenter.x()),
                       static_cast<float>(gripCenter.y()),
                       std::max(visualRotateGeo.gripSize * 0.40f, 2.0f),
                       FloatColor{1.0f, 0.92f, 0.74f, rotateEmphasis ? 0.92f : 0.72f},
                       0.0f, true);
  if (rotateEmphasis) {
   const float arcSweep = rotateActive ? std::remainder(dragAccumulatedRotationDelta_, 360.0f) : 42.0f;
   drawArc(renderer, visualRotateGeo.centerWorld,
           visualRotateGeo.ringRadius + std::max(0.6f, 0.45f * invZoom),
           -90.0f, arcSweep,
           FloatColor{1.0f, 0.68f, 0.16f, rotateActive ? 0.96f : 0.70f},
           std::max(2.4f, 1.8f * invZoom));
  }

  if (rotateActive) {
   const float arcSweep =
       std::remainder(dragAccumulatedRotationDelta_, 360.0f);
   const float currentAngle = dragStartPointerAngle_ + arcSweep;
   const QPointF startPoint =
       pointOnCircle(visualRotateGeo.centerWorld, visualRotateGeo.ringRadius, dragStartPointerAngle_);
   const QPointF currentPoint =
       pointOnCircle(visualRotateGeo.centerWorld, visualRotateGeo.ringRadius, currentAngle);
   const FloatColor arcColor{1.0f, 0.74f, 0.24f, 0.96f};
   const FloatColor arcGlow{1.0f, 0.88f, 0.42f, 0.34f};
   const FloatColor startLineColor{1.0f, 1.0f, 1.0f, 0.34f};
   const FloatColor currentLineColor{1.0f, 0.84f, 0.34f, 0.90f};
   const FloatColor baseSweepColor{1.0f, 0.66f, 0.18f, 0.18f};

   drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
           dragStartPointerAngle_, arcSweep, arcGlow,
           std::max(visualRotateGeo.ringThickness * 2.1f, 4.8f * invZoom));
   drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius,
           dragStartPointerAngle_, arcSweep, arcColor,
           std::max(2.6f * invZoom, 2.0f));
   drawArc(renderer, visualRotateGeo.centerWorld, visualRotateGeo.ringRadius * 0.965f,
           -90.0f, arcSweep, baseSweepColor,
           std::max(1.3f, 1.05f * invZoom));
   drawRotateLeader(renderer, visualRotateGeo.centerWorld, currentPoint,
                    FloatColor{1.0f, 0.84f, 0.34f, 0.95f}, invZoom);
   renderer->drawSolidLine({static_cast<float>(visualRotateGeo.centerWorld.x()),
                            static_cast<float>(visualRotateGeo.centerWorld.y())},
                           {static_cast<float>(startPoint.x()),
                            static_cast<float>(startPoint.y())},
                           startLineColor, std::max(1.1f, 1.2f * invZoom));
   renderer->drawSolidLine({static_cast<float>(visualRotateGeo.centerWorld.x()),
                            static_cast<float>(visualRotateGeo.centerWorld.y())},
                           {static_cast<float>(currentPoint.x()),
                            static_cast<float>(currentPoint.y())},
                           currentLineColor, std::max(1.6f, 1.8f * invZoom));
   renderer->drawCircle(static_cast<float>(currentPoint.x()),
                        static_cast<float>(currentPoint.y()),
                        visualRotateGeo.gripSize + std::max(0.8f * invZoom, 0.8f),
                        FloatColor{0.98f, 0.62f, 0.14f, 0.28f},
                        0.0f, true);
   renderer->drawCircle(static_cast<float>(currentPoint.x()),
                        static_cast<float>(currentPoint.y()),
                        std::max(handleSize * 0.22f, 3.8f),
                        arcColor, 0.0f, true);
  }
 }

 // Anchor point: crosshair at anchor position
 const QPointF anchorWorld = globalTransform.map(QPointF(t3d.anchorX(), t3d.anchorY()));
 if (showAnchor) {
  ArtifactCore::ProfileScope _profAnchor(
      "TransformGizmoAnchor", ArtifactCore::ProfileCategory::Render);
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
  ArtifactCore::ProfileScope _profSnap(
      "TransformGizmoSnapGuides", ArtifactCore::ProfileCategory::Render);
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
 auto mouseCanvas = renderer->viewportToCanvas({(float)viewportPos.x(), (float)viewportPos.y()});
 const QPointF mousePos(mouseCanvas.x, mouseCanvas.y);
 // 1. Check handles first (they should have priority over the body)
 auto checkLocalPoint = [&](const QPointF& localPoint) {
  QPointF worldPoint = globalTransform.map(localPoint);
  auto vPos = renderer->canvasToViewport({(float)worldPoint.x(), (float)worldPoint.y()});
  QRectF handleRect = handleRectForViewport({vPos.x, vPos.y}, HANDLE_SIZE);
  return handleRect.contains(viewportPos);
 };

 const QPointF centerPoint = localRect.center();
 const float scaleOutset = std::max(2.75f, GizmoVisualStyle::scaleHandleOutset * 0.65f * invZoom);
 const QPointF topPoint(localRect.center().x(), localRect.top());
 const QPointF bottomPoint(localRect.center().x(), localRect.bottom());
 const QPointF leftPoint(localRect.left(), localRect.center().y());
 const QPointF rightPoint(localRect.right(), localRect.center().y());
 const QPointF tlPoint = localRect.topLeft();
 const QPointF trPoint = localRect.topRight();
 const QPointF blPoint = localRect.bottomLeft();
 const QPointF brPoint = localRect.bottomRight();
 const QPointF tlHit = offsetPointAwayFromCenter(centerPoint, tlPoint, scaleOutset);
 const QPointF trHit = offsetPointAwayFromCenter(centerPoint, trPoint, scaleOutset);
 const QPointF blHit = offsetPointAwayFromCenter(centerPoint, blPoint, scaleOutset);
 const QPointF brHit = offsetPointAwayFromCenter(centerPoint, brPoint, scaleOutset);
 const QPointF topHit = offsetPointAwayFromCenter(centerPoint, topPoint, scaleOutset * 0.58f);
 const QPointF bottomHit = offsetPointAwayFromCenter(centerPoint, bottomPoint, scaleOutset * 0.58f);
 const QPointF leftHit = offsetPointAwayFromCenter(centerPoint, leftPoint, scaleOutset * 0.58f);
 const QPointF rightHit = offsetPointAwayFromCenter(centerPoint, rightPoint, scaleOutset * 0.58f);

 if (allowsHandle(HandleType::Scale_TR) && checkLocalPoint(trHit)) return HandleType::Scale_TR;
 if (allowsHandle(HandleType::Scale_BL) && checkLocalPoint(blHit)) return HandleType::Scale_BL;
 if (allowsHandle(HandleType::Scale_BR) && checkLocalPoint(brHit)) return HandleType::Scale_BR;
 if (allowsHandle(HandleType::Scale_T) && checkLocalPoint(topHit)) return HandleType::Scale_T;
 if (allowsHandle(HandleType::Scale_B) && checkLocalPoint(bottomHit)) return HandleType::Scale_B;
 if (allowsHandle(HandleType::Scale_L) && checkLocalPoint(leftHit)) return HandleType::Scale_L;
 if (allowsHandle(HandleType::Scale_R) && checkLocalPoint(rightHit)) return HandleType::Scale_R;
 if (allowsHandle(HandleType::Scale_TL) && checkLocalPoint(tlHit)) return HandleType::Scale_TL;
 if (mode_ == Mode::Scale && allowsHandle(HandleType::Scale_Center)) {
  const QPointF worldPoint = globalTransform.map(centerPoint);
  const auto vPos = renderer->canvasToViewport({static_cast<float>(worldPoint.x()),
                                                static_cast<float>(worldPoint.y())});
  const float centerHandleSize = std::max(20.0f, 22.0f * invZoom);
  if (QRectF(vPos.x - centerHandleSize * 0.5f,
             vPos.y - centerHandleSize * 0.5f,
             centerHandleSize, centerHandleSize).contains(viewportPos)) {
   return HandleType::Scale_Center;
  }
 }

 const auto& t3d = layer_->transform3D();

 // 2. Rotation handle: ImGui-style outer ring around the object.
 {
  const RotateRingGeometry rotateGeo =
      computeRotateRingGeometry(localRect, globalTransform, invZoom);
  const float distToCenter = pointDistance(mousePos, rotateGeo.centerWorld);
  const float distToRing = std::abs(distToCenter - rotateGeo.hitRadius);
  const QPointF idleGripCenter =
      pointOnCircle(rotateGeo.centerWorld, rotateGeo.gripRadius, -90.0f);
  const bool hitGrip =
      pointDistance(mousePos, idleGripCenter) <= rotateGeo.gripSize * 1.8f;
  const bool hitRing = distToRing <= rotateGeo.hitThickness;
  if (allowsHandle(HandleType::Rotate) && mode_ == Mode::Rotate && (hitRing || hitGrip)) {
   return HandleType::Rotate;
  }
 }

 // 3b. Body drag: allow dragging from inside the transformed layer bounds.
 if (allowsHandle(HandleType::Move)) {
  bool invertible = false;
  const QTransform inv = globalTransform.inverted(&invertible);
  if (invertible) {
   const QPointF localPos = inv.map(mousePos);
   if (localRect.contains(localPos)) {
    return HandleType::Move;
   }
  }
 }

 // 4. Anchor point handle
 // In current implementation anchor is in local coords (usually 0,0)
 QPointF worldAnchor = globalTransform.map(QPointF(t3d.anchorX(), t3d.anchorY()));
 auto anchorVP = renderer->canvasToViewport({(float)worldAnchor.x(), (float)worldAnchor.y()});
 const float anchorHit = ANCHOR_HANDLE_SIZE + 4.0f;
 QRectF anchorRect(anchorVP.x - anchorHit, anchorVP.y - anchorHit, anchorHit * 2, anchorHit * 2);
 if (allowsHandle(HandleType::Anchor) && anchorRect.contains(viewportPos)) {
   return HandleType::Anchor;
  }

 // 5. Move handle: X/Y arrow shafts and center dot at layer center
 if (allowsHandle(HandleType::Move)) {
  const QPointF moveCenterW = globalTransform.map(localRect.center());
  const QPointF mapOrigin = globalTransform.map(QPointF(0.0, 0.0));
  QPointF xAxisRaw = globalTransform.map(QPointF(1.0, 0.0)) - mapOrigin;
  const float xALen = static_cast<float>(
      std::sqrt(xAxisRaw.x() * xAxisRaw.x() + xAxisRaw.y() * xAxisRaw.y()));
  const QPointF xAxisDir = xALen > 0.001f ? xAxisRaw / xALen : QPointF(1.0, 0.0);
  QPointF yAxisRaw = globalTransform.map(QPointF(0.0, -1.0)) - mapOrigin;
  const float yALen = static_cast<float>(
      std::sqrt(yAxisRaw.x() * yAxisRaw.x() + yAxisRaw.y() * yAxisRaw.y()));
  const QPointF yAxisDir = yALen > 0.001f ? yAxisRaw / yALen : QPointF(0.0, -1.0);

  const float arrowCanvasLen = 50.0f * invZoom;
  const QPointF xTip = moveCenterW + xAxisDir * arrowCanvasLen;
  const QPointF yTip = moveCenterW + yAxisDir * arrowCanvasLen;

  const float shaftThreshold = std::max(5.5f, 6.0f * invZoom);
  if (distanceToSegment(mousePos, moveCenterW, xTip) <= shaftThreshold ||
      distanceToSegment(mousePos, moveCenterW, yTip) <= shaftThreshold ||
      pointDistance(mousePos, moveCenterW) <= shaftThreshold * 2.0f) {
   return HandleType::Move;
  }
 }

 // 5. Body hit test (Move) using inverse transform
 bool invertible = false;
 const QTransform invTransform = globalTransform.inverted(&invertible);
 if (invertible && allowsHandle(HandleType::Move)) {
  QPointF localMouse = invTransform.map(QPointF(mouseCanvas.x, mouseCanvas.y));
  const QRectF moveRect = localRect.adjusted(
      GizmoVisualStyle::scaleHandleOutset * 0.12f,
      GizmoVisualStyle::scaleHandleOutset * 0.12f,
      -GizmoVisualStyle::scaleHandleOutset * 0.12f,
      -GizmoVisualStyle::scaleHandleOutset * 0.12f);
  if (moveRect.isValid() && moveRect.contains(localMouse)) {
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
  return Qt::SizeAllCursor;
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
  dragAccumulatedRotationDelta_ = 0.0f;
  if (activeHandle_ == HandleType::Rotate) {
   const QPointF pivotWorld = dragStartGlobalTransform_.map(dragStartLocalBounds_.center());
   dragStartPointerAngle_ = angleDegreesAround(pivotWorld, dragStartCanvasPos_);
  }
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
      if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer_->composition())) {
       ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
           LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                             LayerChangedEvent::ChangeType::Modified});
      }
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
    if (isDragging_) {
     if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer_->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
     }
    }
   }
  } else if (activeHandle_ == HandleType::Rotate) {
   const QPointF pivotLocal = dragStartLocalBounds_.center();
   const QPointF pivotWorldStart = dragStartGlobalTransform_.map(pivotLocal);
   const float previousAngle = angleDegreesAround(pivotWorldStart, lastCanvasMousePos_);
   const float currentAngle = angleDegreesAround(pivotWorldStart, currentCanvasPos);
   dragAccumulatedRotationDelta_ +=
       normalizeAngleDeltaDegrees(currentAngle - previousAngle);
   const float newRotation = dragStartRotation_ + dragAccumulatedRotationDelta_;
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
    if (isDragging_) {
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer_->composition())) {
     ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
         LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                           LayerChangedEvent::ChangeType::Modified});
    }
   }
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
   if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer_->composition())) {
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
   }
  }
 }
 isDragging_ = false;
 activeHandle_ = HandleType::None;
 dragAccumulatedRotationDelta_ = 0.0f;
 activeSnapLines_.clear();
}

}
