module;

#include <QPointF>
#include <QRectF>
#include <QSize>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

module Artifact.Widgets.LayerEditor.SurfaceOverlay;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Color.Float;
import Utils.Id;

namespace Artifact {
namespace {

QPointF boundaryPoint(const QRectF& rect, const QPointF& toward)
{
 const QPointF center = rect.center();
 const QPointF delta = toward - center;
 constexpr qreal epsilon = 0.001;
 if (std::abs(delta.x()) <= epsilon && std::abs(delta.y()) <= epsilon) return center;
 qreal scaleX = std::numeric_limits<qreal>::max();
 qreal scaleY = std::numeric_limits<qreal>::max();
 if (std::abs(delta.x()) > epsilon) scaleX = rect.width() * 0.5 / std::abs(delta.x());
 if (std::abs(delta.y()) > epsilon) scaleY = rect.height() * 0.5 / std::abs(delta.y());
 return center + delta * std::min(scaleX, scaleY);
}

}

void drawLayerEditorSurfaceOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    bool impactMode, const LayerEditorImpactLayers& impactLayers)
{
 if (!renderer || !layer) return;
 const QRectF bounds = layer->transformedBoundingBox();
 if (!bounds.isValid() || bounds.isEmpty()) return;
 const FloatColor accent = impactMode ? FloatColor{1.0f, 0.58f, 0.16f, 0.96f}
                                      : FloatColor{0.24f, 0.66f, 1.0f, 0.96f};
 renderer->setUseExternalMatrices(false);
 renderer->drawDashedRectOutline(static_cast<float>(bounds.x()), static_cast<float>(bounds.y()),
                                 static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
                                 accent, 1.5f, 10.0f, 5.0f);
 const float zoom = std::max(0.1f, renderer->getZoom());
 const QPointF center = bounds.center();
 const auto& transform = layer->transform3D();
 const QPointF pivot = layer->getGlobalTransform().map(
     QPointF(transform.anchorX(), transform.anchorY()));
 renderer->drawCrosshair(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()),
                         9.0f / zoom, {0.02f, 0.03f, 0.04f, 0.92f});
 renderer->drawCrosshair(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()),
                         7.0f / zoom, accent);
 renderer->drawCircle(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()),
                      4.0f / zoom, accent, 1.0f, false);
 if (!impactMode) return;

 int linkCount = 0;
 constexpr int maxLinks = 24;
 std::vector<LayerID> drawn;
 drawn.reserve(maxLinks);
 const auto drawLink = [&](const ArtifactAbstractLayerPtr& other,
                           const FloatColor& color, bool incoming) {
  if (!other || other->id() == layer->id() || linkCount >= maxLinks ||
      std::find(drawn.begin(), drawn.end(), other->id()) != drawn.end()) return;
  const QRectF otherBounds = other->transformedBoundingBox();
  if (!otherBounds.isValid() || otherBounds.isEmpty()) return;
  const QPointF otherCenter = otherBounds.center();
  const QPointF targetEdge = boundaryPoint(bounds, otherCenter);
  const QPointF otherEdge = boundaryPoint(otherBounds, center);
  renderer->drawDashedRectOutline(
      static_cast<float>(otherBounds.x()), static_cast<float>(otherBounds.y()),
      static_cast<float>(otherBounds.width()), static_cast<float>(otherBounds.height()),
      color, 1.0f / zoom, 8.0f / zoom, 4.0f / zoom);
  renderer->drawSolidLine(
      {static_cast<float>(targetEdge.x()), static_cast<float>(targetEdge.y())},
      {static_cast<float>(otherEdge.x()), static_cast<float>(otherEdge.y())},
      color, 1.25f / zoom);
  const QPointF from = incoming ? otherEdge : targetEdge;
  const QPointF tip = incoming ? targetEdge : otherEdge;
  const QPointF delta = tip - from;
  const qreal distance = std::hypot(delta.x(), delta.y());
  if (distance > 0.001) {
   const QPointF direction = delta / distance;
   const QPointF perpendicular(-direction.y(), direction.x());
   const QPointF base = tip - direction * (8.0 / zoom);
   const QPointF wing = perpendicular * (3.5 / zoom);
   for (const QPointF& point : {base + wing, base - wing})
    renderer->drawSolidLine(
        {static_cast<float>(tip.x()), static_cast<float>(tip.y())},
        {static_cast<float>(point.x()), static_cast<float>(point.y())},
        color, 1.25f / zoom);
  }
  renderer->drawCircle(static_cast<float>(otherCenter.x()), static_cast<float>(otherCenter.y()),
                       4.0f / zoom, color, 1.0f, true);
  drawn.push_back(other->id());
  ++linkCount;
 };

 for (const auto& other : impactLayers.parents)
  drawLink(other, {0.30f, 0.86f, 0.58f, 0.88f}, true);
 for (const auto& other : impactLayers.children)
  drawLink(other, {0.24f, 0.66f, 1.0f, 0.82f}, false);
 for (const auto& other : impactLayers.mattes)
  drawLink(other, {0.78f, 0.42f, 1.0f, 0.90f}, true);
 for (const auto& other : impactLayers.dependents)
  drawLink(other, {1.0f, 0.58f, 0.16f, 0.88f}, false);

 renderer->drawDashedRectOutline(static_cast<float>(bounds.x()), static_cast<float>(bounds.y()),
                                 static_cast<float>(bounds.width()), static_cast<float>(bounds.height()),
                                 accent, 1.5f, 10.0f, 5.0f);
 renderer->drawCrosshair(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()),
                         7.0f / zoom, accent);
 renderer->drawCircle(static_cast<float>(pivot.x()), static_cast<float>(pivot.y()),
                      4.0f / zoom, accent, 1.0f, false);
}

void drawLayerEditorCompositionGuides(
    ArtifactIRenderer* renderer, const QSize& compositionSize,
    bool showGrid, bool showSafeMargins)
{
 if (!renderer || (!showGrid && !showSafeMargins) ||
     compositionSize.width() <= 0 || compositionSize.height() <= 0) return;
 const float width = static_cast<float>(compositionSize.width());
 const float height = static_cast<float>(compositionSize.height());
 const float zoom = std::max(0.001f, renderer->getZoom());
 const auto niceInterval = [](float raw) {
  const float safe = std::max(1.0f, raw);
  const float scale = std::pow(10.0f, std::floor(std::log10(safe)));
  const float normalized = safe / scale;
  return (normalized <= 1.0f ? 1.0f : normalized <= 2.0f ? 2.0f
          : normalized <= 5.0f ? 5.0f : 10.0f) * scale;
 };
 const float major = niceInterval(64.0f / zoom);
 const float minor = major / 4.0f;
 renderer->setUseExternalMatrices(false);
 if (showGrid && minor * zoom >= 8.0f)
  renderer->drawGrid(0.0f, 0.0f, width, height, minor, 0.65f / zoom,
                     {0.48f, 0.58f, 0.72f, 0.16f});
 if (showGrid) {
  renderer->drawGrid(0.0f, 0.0f, width, height, major, 1.0f / zoom,
                     {0.48f, 0.66f, 0.92f, 0.36f});
  renderer->drawDashedRectOutline(0.0f, 0.0f, width, height,
                                  {0.30f, 0.68f, 1.0f, 0.86f},
                                  1.25f / zoom, 9.0f / zoom, 5.0f / zoom);
 }
 if (showSafeMargins) {
  const auto drawSafe = [&](float ratio, const FloatColor& color) {
   const float insetX = width * (1.0f - ratio) * 0.5f;
   const float insetY = height * (1.0f - ratio) * 0.5f;
   renderer->drawRectOutlineLocal(insetX, insetY, width - insetX * 2.0f,
                                  height - insetY * 2.0f, color);
  };
  drawSafe(0.9f, {1.0f, 0.78f, 0.24f, 0.78f});
  drawSafe(0.8f, {0.98f, 0.44f, 0.28f, 0.68f});
 }
}

}
