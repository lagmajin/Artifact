module;

#include <QApplication>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTransform>

#include <algorithm>

module Artifact.Widgets.LayerEditor.ShapeOverlay;

import Artifact.Layer.Shape;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.Geometry;
import Color.Float;
import Memory.SharedPtr;

namespace Artifact {

void drawLayerEditorShapeOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorShapeOverlayState& state)
{
 if (!renderer || !layer) return;
 const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || !shape->hasCustomPolygon()) return;
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) return;

 const QTransform transform = layer->getGlobalTransform();
 const FloatColor outlineShadow{0.0f, 0.0f, 0.0f, 0.30f};
 const FloatColor outline{0.98f, 0.72f, 0.28f, 0.96f};
 const FloatColor segmentHover{0.42f, 0.86f, 1.0f, 0.90f};
 const FloatColor pointShadow{0.0f, 0.0f, 0.0f, 0.42f};
 const FloatColor point{0.98f, 0.99f, 1.0f, 1.0f};
 const FloatColor hover{1.0f, 0.76f, 0.28f, 1.0f};
 const FloatColor selected{0.28f, 0.78f, 1.0f, 1.0f};
 const FloatColor drag{1.0f, 0.40f, 0.24f, 1.0f};

 Detail::float2 first{};
 Detail::float2 previous{};
 for (int index = 0; index < static_cast<int>(points.size()); ++index) {
  const QPointF canvas = transform.map(points[static_cast<size_t>(index)]);
  const Detail::float2 current{static_cast<float>(canvas.x()), static_cast<float>(canvas.y())};
  if (index > 0) {
   renderer->drawThickLineLocal(previous, current, 6.0f, outlineShadow);
   renderer->drawThickLineLocal(previous, current, 3.5f, outline);
  } else {
   first = current;
  }
  previous = current;
 }
 if (shape->customPolygonClosed()) {
  renderer->drawThickLineLocal(previous, first, 6.0f, outlineShadow);
  renderer->drawThickLineLocal(previous, first, 3.5f, outline);
 }

 if (state.hoveredSegmentIndex >= 0 &&
     state.hoveredSegmentIndex < static_cast<int>(points.size())) {
  const int next = shape->customPolygonClosed()
      ? (state.hoveredSegmentIndex + 1) % static_cast<int>(points.size())
      : state.hoveredSegmentIndex + 1;
  if (next >= 0 && next < static_cast<int>(points.size())) {
   const QPointF a = transform.map(points[static_cast<size_t>(state.hoveredSegmentIndex)]);
   const QPointF b = transform.map(points[static_cast<size_t>(next)]);
   renderer->drawThickLineLocal(
       {static_cast<float>(a.x()), static_cast<float>(a.y())},
       {static_cast<float>(b.x()), static_cast<float>(b.y())}, 5.5f, segmentHover);
   const QPointF middle = (a + b) * 0.5;
   renderer->drawCircle(static_cast<float>(middle.x()), static_cast<float>(middle.y()),
                        12.0f, segmentHover, 1.0f, true);
  }
 }

 for (int index = 0; index < static_cast<int>(points.size()); ++index) {
  const QPointF canvas = transform.map(points[static_cast<size_t>(index)]);
  const bool isSelected = std::find(state.selectedVertexIndices.begin(),
                                    state.selectedVertexIndices.end(), index) !=
                          state.selectedVertexIndices.end();
  FloatColor color = isSelected ? selected : point;
  float radius = isSelected ? 18.0f : 16.0f;
  if (state.draggingVertex && state.draggingVertexIndex == index) {
   color = drag;
   radius = 20.0f;
  } else if (state.hoveredVertexIndex == index) {
   color = hover;
   radius = 18.0f;
  }
  renderer->drawCircle(static_cast<float>(canvas.x()), static_cast<float>(canvas.y()),
                       radius + 4.0f, pointShadow, 1.0f, true);
  renderer->drawCircle(static_cast<float>(canvas.x()), static_cast<float>(canvas.y()),
                       radius, color, 1.0f, true);
 }

 if (state.hoveredVertexIndex < 0 && state.hoveredSegmentIndex < 0) return;
 QFont font = QApplication::font();
 font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
 const bool hoverVertex = state.hoveredVertexIndex >= 0;
 const QString headline = hoverVertex
     ? QStringLiteral("vertex %1%2").arg(state.hoveredVertexIndex + 1).arg(
           state.draggingVertex ? QStringLiteral(" selected") : QString())
     : QStringLiteral("segment %1").arg(state.hoveredSegmentIndex + 1);
 const QString detail = hoverVertex
     ? (state.draggingVertex ? QStringLiteral("dragging / delete / convert")
                             : QStringLiteral("drag / delete / convert"))
     : QStringLiteral("insert / split / convert");
 const QPointF anchor = transform.map(points.front()) + QPointF(14.0, -30.0);
 const float zoom = std::max(0.1f, renderer->getZoom());
 const QRectF rect(anchor, QSizeF(228.0f / zoom, 48.0f / zoom));
 renderer->drawOverlayPanel(rect.x(), rect.y(), rect.width(), rect.height(),
                            {0.06f, 0.09f, 0.13f, 0.88f},
                            {0.42f, 0.72f, 0.98f, 0.90f});
 renderer->drawText(rect.adjusted(8.0, 5.0, -8.0, -4.0),
                    QStringLiteral("Shape %1\n%2\n%3")
                        .arg(headline, detail, state.proportionalStatus),
                    font, {0.95f, 0.97f, 1.0f, 1.0f},
                    Qt::AlignLeft | Qt::AlignVCenter);
}

void drawLayerEditorCustomPathOverlay(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    const LayerEditorShapeOverlayState& state)
{
 if (!renderer || !layer) return;
 const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || !shape->hasCustomPath()) return;
 const QTransform transform = layer->getGlobalTransform();
 const float zoom = std::max(0.001f, renderer->getZoom());
 const float vertexRadius = 5.0f / zoom;
 const float tangentRadius = 4.0f / zoom;
 const auto vertices = shape->customPathVertices();
 const int count = static_cast<int>(vertices.size());
 if (count == 0) return;

 for (int index = 0; index < count; ++index) {
  const int next = (index + 1) % count;
  if (!shape->customPathClosed() && next == 0) break;
  const QPointF a = transform.map(vertices[static_cast<size_t>(index)].pos);
  const QPointF b = transform.map(vertices[static_cast<size_t>(next)].pos);
  renderer->drawThickLineLocal(
      {static_cast<float>(a.x()), static_cast<float>(a.y())},
      {static_cast<float>(b.x()), static_cast<float>(b.y())},
      2.0f, {0.4f, 0.7f, 1.0f, 0.5f});
 }

 for (int index = 0; index < count; ++index) {
  const auto& vertex = vertices[static_cast<size_t>(index)];
  const QPointF point = transform.map(vertex.pos);
  const auto drawTangent = [&](const QPointF& tangent, int tangentType) {
   if (tangent == QPointF()) return;
   const QPointF endpoint = transform.map(vertex.pos + tangent);
   renderer->drawThickLineLocal(
       {static_cast<float>(point.x()), static_cast<float>(point.y())},
       {static_cast<float>(endpoint.x()), static_cast<float>(endpoint.y())},
       1.0f, {1.0f, 1.0f, 1.0f, 0.3f});
   const bool hovered = state.hoveredPathTangentIndex == index &&
                        state.hoveredPathTangentType == tangentType;
   renderer->drawCircle(static_cast<float>(endpoint.x()), static_cast<float>(endpoint.y()),
                        tangentRadius,
                        hovered ? FloatColor{1.0f, 0.7f, 0.0f, 1.0f}
                                : FloatColor{0.8f, 0.5f, 1.0f, 0.9f},
                        1.0f, true);
  };
  drawTangent(vertex.outTangent, 1);
  drawTangent(vertex.inTangent, 0);

  const bool hovered = state.hoveredPathVertexIndex == index;
  const bool selected = std::find(state.selectedPathVertexIndices.begin(),
                                  state.selectedPathVertexIndices.end(), index) !=
                        state.selectedPathVertexIndices.end();
  renderer->drawCircle(static_cast<float>(point.x()), static_cast<float>(point.y()),
                       selected ? vertexRadius * 1.35f : vertexRadius,
                       hovered ? FloatColor{1.0f, 0.5f, 0.0f, 1.0f}
                               : selected ? FloatColor{0.25f, 0.85f, 1.0f, 1.0f}
                                          : FloatColor{0.2f, 0.8f, 1.0f, 1.0f},
                       1.0f, true);
  renderer->drawCircle(static_cast<float>(point.x()), static_cast<float>(point.y()),
                       vertexRadius, {1.0f, 1.0f, 1.0f, 0.8f}, 1.0f, false);
 }

 if (state.hoveredPathVertexIndex < 0 && state.hoveredPathTangentIndex < 0) return;
 QFont font = QApplication::font();
 font.setPointSizeF(std::max(9.0, static_cast<double>(font.pointSizeF())));
 const bool hoverTangent = state.hoveredPathTangentIndex >= 0;
 const QString headline = hoverTangent
     ? QStringLiteral("tangent %1%2").arg(state.hoveredPathTangentIndex + 1).arg(
           state.draggingPathTangent ? QStringLiteral(" selected") : QString())
     : QStringLiteral("vertex %1%2").arg(state.hoveredPathVertexIndex + 1).arg(
           state.draggingPathVertex ? QStringLiteral(" selected") : QString());
 const QString detail = hoverTangent
     ? (state.draggingPathTangent ? QStringLiteral("dragging / rebalance / smooth")
                                  : QStringLiteral("drag / rebalance / smooth"))
     : QStringLiteral("drag / delete / toggle smooth");
 const QPointF anchor = transform.map(vertices.front().pos) + QPointF(14.0, -30.0);
 const QRectF rect(anchor, QSizeF(236.0 / zoom, 48.0 / zoom));
 renderer->drawOverlayPanel(rect.x(), rect.y(), rect.width(), rect.height(),
                            {0.06f, 0.09f, 0.13f, 0.88f},
                            {0.42f, 0.72f, 0.98f, 0.90f});
 renderer->drawText(rect.adjusted(8.0, 5.0, -8.0, -4.0),
                    QStringLiteral("Path %1\n%2\n%3")
                        .arg(headline, detail, state.proportionalStatus),
                    font, {0.95f, 0.97f, 1.0f, 1.0f},
                    Qt::AlignLeft | Qt::AlignVCenter);
}

void drawLayerEditorShapeParameterHandles(
    ArtifactIRenderer* renderer, const ArtifactAbstractLayerPtr& layer,
    bool hoverCornerRadius, bool hoverStarInnerRadius)
{
 if (!renderer || !layer) return;
 const auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape) return;
 QPointF local;
 bool hovered = false;
 if (shape->shapeType() == ShapeType::Rect || shape->shapeType() == ShapeType::Square) {
  local = shapeCornerRadiusHandlePosition(*shape);
  hovered = hoverCornerRadius;
 } else if (shape->shapeType() == ShapeType::Star) {
  local = shapeStarInnerRadiusHandlePosition(*shape);
  hovered = hoverStarInnerRadius;
 } else {
  return;
 }
 const QPointF point = layer->getGlobalTransform().map(local);
 const float radius = 6.0f / std::max(0.001f, renderer->getZoom());
 const FloatColor color = hovered ? FloatColor{1.0f, 0.6f, 0.0f, 1.0f}
                                  : FloatColor{0.0f, 0.7f, 1.0f, 1.0f};
 renderer->drawCircle(static_cast<float>(point.x()), static_cast<float>(point.y()),
                      radius, color, 1.0f, true);
 renderer->drawCircle(static_cast<float>(point.x()), static_cast<float>(point.y()),
                      radius, {1.0f, 1.0f, 1.0f, 0.7f}, 1.0f, false);
}

}
