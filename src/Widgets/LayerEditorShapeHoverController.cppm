module;

#include <QPointF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>

module Artifact.Widgets.LayerEditor.ShapeHoverController;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;

namespace Artifact {
namespace {

ArtifactCore::SharedPtr<ArtifactShapeLayer> shapeLayer(
    const ArtifactAbstractLayerPtr& layer)
{
 return ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
}

bool sameState(const LayerEditorShapeHoverState& left,
               const LayerEditorShapeHoverState& right)
{
 return left.polygonVertex == right.polygonVertex &&
        left.polygonSegment == right.polygonSegment &&
        left.pathVertex == right.pathVertex &&
        left.pathTangentVertex == right.pathTangentVertex &&
        left.pathTangentType == right.pathTangentType;
}

}

bool LayerEditorShapeHoverController::hitPolygonVertex(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, int& vertexIndex) const
{
 auto shape = shapeLayer(layer);
 if (!shape || !shape->hasCustomPolygon()) return false;
 const auto points = shape->customPolygonPoints();
 if (points.size() < 3) return false;
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return false;
 const QPointF local = inverse.map(canvasPosition);
 const float threshold = 8.0f / std::max(0.1f, zoom);
 for (int index = 0; index < static_cast<int>(points.size()); ++index) {
  if (std::hypot(points[static_cast<size_t>(index)].x() - local.x(),
                 points[static_cast<size_t>(index)].y() - local.y()) <= threshold) {
   vertexIndex = index;
   return true;
  }
 }
 return false;
}

bool LayerEditorShapeHoverController::hitPolygonSegment(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, int& insertIndex) const
{
 auto shape = shapeLayer(layer);
 if (!shape || !shape->hasCustomPolygon()) return false;
 const auto points = shape->customPolygonPoints();
 if (points.size() < 2) return false;
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return false;
 const QPointF local = inverse.map(canvasPosition);
 const float threshold = 10.0f / std::max(0.1f, zoom);
 const auto distanceToSegment = [](const QPointF& point,
                                   const QPointF& start,
                                   const QPointF& end) {
  const QPointF segment = end - start;
  const double lengthSquared = segment.x() * segment.x() +
                               segment.y() * segment.y();
  if (lengthSquared <= std::numeric_limits<double>::epsilon())
   return std::hypot(point.x() - start.x(), point.y() - start.y());
  const QPointF relative = point - start;
  const double ratio = std::clamp(
      (relative.x() * segment.x() + relative.y() * segment.y()) /
      lengthSquared, 0.0, 1.0);
  const QPointF projected = start + segment * ratio;
  return std::hypot(point.x() - projected.x(), point.y() - projected.y());
 };
 const int count = shape->customPolygonClosed()
     ? static_cast<int>(points.size()) : static_cast<int>(points.size()) - 1;
 double bestDistance = std::numeric_limits<double>::max();
 int bestIndex = -1;
 for (int index = 0; index < count; ++index) {
  const int next = (index + 1) % static_cast<int>(points.size());
  const double distance = distanceToSegment(
      local, points[static_cast<size_t>(index)], points[static_cast<size_t>(next)]);
  if (distance < bestDistance) {
   bestDistance = distance;
   bestIndex = index;
  }
 }
 if (bestIndex < 0 || bestDistance > threshold) return false;
 insertIndex = bestIndex + 1;
 return true;
}

bool LayerEditorShapeHoverController::hitPathVertex(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, int& vertexIndex) const
{
 auto shape = shapeLayer(layer);
 if (!shape || !shape->hasCustomPath()) return false;
 const QTransform transform = layer->getGlobalTransform();
 const float threshold = 8.0f / std::max(0.001f, zoom);
 const auto vertices = shape->customPathVertices();
 for (int index = 0; index < static_cast<int>(vertices.size()); ++index) {
  const QPointF canvas = transform.map(vertices[static_cast<size_t>(index)].pos);
  if (std::hypot(canvas.x() - canvasPosition.x(),
                 canvas.y() - canvasPosition.y()) <= threshold) {
   vertexIndex = index;
   return true;
  }
 }
 return false;
}

bool LayerEditorShapeHoverController::hitPathTangent(
    const ArtifactAbstractLayerPtr& layer, const QPointF& canvasPosition,
    float zoom, int& vertexIndex, int& tangentType) const
{
 auto shape = shapeLayer(layer);
 if (!shape || !shape->hasCustomPath()) return false;
 const QTransform transform = layer->getGlobalTransform();
 const float threshold = 7.0f / std::max(0.001f, zoom);
 const auto vertices = shape->customPathVertices();
 for (int index = 0; index < static_cast<int>(vertices.size()); ++index) {
  const auto& vertex = vertices[static_cast<size_t>(index)];
  if (!vertex.outTangent.isNull()) {
   const QPointF handle = transform.map(vertex.pos + vertex.outTangent);
   if (std::hypot(handle.x() - canvasPosition.x(),
                  handle.y() - canvasPosition.y()) <= threshold) {
    vertexIndex = index;
    tangentType = 1;
    return true;
   }
  }
  if (!vertex.inTangent.isNull()) {
   const QPointF handle = transform.map(vertex.pos + vertex.inTangent);
   if (std::hypot(handle.x() - canvasPosition.x(),
                  handle.y() - canvasPosition.y()) <= threshold) {
    vertexIndex = index;
    tangentType = 0;
    return true;
   }
  }
 }
 return false;
}

bool LayerEditorShapeHoverController::updatePolygon(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom)
{
 LayerEditorShapeHoverState next;
 int insertIndex = -1;
 if (!hitPolygonVertex(layer, canvasPosition, zoom, next.polygonVertex) &&
     hitPolygonSegment(layer, canvasPosition, zoom, insertIndex))
  next.polygonSegment = std::max(0, insertIndex - 1);
 const bool changed = !sameState(state_, next);
 state_ = next;
 return changed;
}

bool LayerEditorShapeHoverController::updatePath(
    const ArtifactAbstractLayerPtr& layer,
    const QPointF& canvasPosition, float zoom)
{
 LayerEditorShapeHoverState next;
 if (!hitPathTangent(layer, canvasPosition, zoom,
                     next.pathTangentVertex, next.pathTangentType))
  hitPathVertex(layer, canvasPosition, zoom, next.pathVertex);
 const bool changed = !sameState(state_, next);
 state_ = next;
 return changed;
}

void LayerEditorShapeHoverController::setPolygon(
    int vertexIndex, int segmentIndex) noexcept
{
 state_ = {};
 state_.polygonVertex = vertexIndex;
 state_.polygonSegment = segmentIndex;
}

void LayerEditorShapeHoverController::setPath(
    int vertexIndex, int tangentVertex, int tangentType) noexcept
{
 state_ = {};
 state_.pathVertex = vertexIndex;
 state_.pathTangentVertex = tangentVertex;
 state_.pathTangentType = tangentType;
}

void LayerEditorShapeHoverController::clear() noexcept
{
 state_ = {};
}

const LayerEditorShapeHoverState& LayerEditorShapeHoverController::state() const noexcept
{
 return state_;
}

}
