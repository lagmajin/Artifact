module;

#include <QPointF>
#include <QLineF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

module Artifact.Widgets.LayerEditor.Geometry;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;

namespace Artifact {
namespace {
QPointF polygonCenter(const std::vector<QPointF>& points)
{
 QPointF center;
 for (const auto& point : points) center += point;
 if (!points.empty()) center /= static_cast<qreal>(points.size());
 return center;
}
}

int extrudePolygonVertex(std::vector<QPointF>& points, int sourceIndex)
{
 if (sourceIndex < 0 || sourceIndex >= static_cast<int>(points.size())) return -1;
 const int insertedIndex = sourceIndex + 1;
 points.insert(points.begin() + insertedIndex, points[static_cast<size_t>(sourceIndex)]);
 return insertedIndex;
}

int extrudePathVertex(std::vector<CustomPathVertex>& vertices, int sourceIndex)
{
 if (sourceIndex < 0 || sourceIndex >= static_cast<int>(vertices.size())) return -1;
 CustomPathVertex vertex = vertices[static_cast<size_t>(sourceIndex)];
 vertex.inTangent = QPointF();
 vertex.outTangent = QPointF();
 const int insertedIndex = sourceIndex + 1;
 vertices.insert(vertices.begin() + insertedIndex, vertex);
 return insertedIndex;
}

std::vector<QPointF> translateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& delta)
{
 auto result = source;
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size()))
  result[static_cast<size_t>(index)] += delta;
 return result;
}

std::vector<CustomPathVertex> translateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& delta)
{
 auto result = source;
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size()))
  result[static_cast<size_t>(index)].pos += delta;
 return result;
}

std::vector<CustomPathVertex> rotateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, double radians)
{
 auto result = source;
 if (source.empty()) return result;
 QPointF center;
 for (const auto& vertex : source) center += vertex.pos;
 center /= static_cast<qreal>(source.size());
 const double cosine = std::cos(radians);
 const double sine = std::sin(radians);
 const auto rotate = [cosine, sine](const QPointF& value) {
  return QPointF(value.x() * cosine - value.y() * sine,
                 value.x() * sine + value.y() * cosine);
 };
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size())) {
  const size_t offset = static_cast<size_t>(index);
  result[offset].pos = center + rotate(source[offset].pos - center);
  result[offset].inTangent = rotate(source[offset].inTangent);
  result[offset].outTangent = rotate(source[offset].outTangent);
 }
 return result;
}

std::vector<QPointF> rotateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double radians)
{
 auto result = source;
 const QPointF center = polygonCenter(source);
 const double cosine = std::cos(radians);
 const double sine = std::sin(radians);
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size())) {
  const QPointF vector = source[static_cast<size_t>(index)] - center;
  result[static_cast<size_t>(index)] = center + QPointF(
      vector.x() * cosine - vector.y() * sine,
      vector.x() * sine + vector.y() * cosine);
 }
 return result;
}

std::vector<QPointF> scaleSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double factor)
{
 auto result = source;
 const QPointF center = polygonCenter(source);
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size()))
  result[static_cast<size_t>(index)] = center +
      (source[static_cast<size_t>(index)] - center) * factor;
 return result;
}

std::vector<QPointF> scaleSelectedPolygonAxes(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& factors)
{
 auto result = source;
 const QPointF center = polygonCenter(source);
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size())) {
  const QPointF vector = source[static_cast<size_t>(index)] - center;
  result[static_cast<size_t>(index)] = center + QPointF(
      vector.x() * factors.x(), vector.y() * factors.y());
 }
 return result;
}

std::vector<CustomPathVertex> scaleSelectedPathAxes(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& factors)
{
 auto result = source;
 if (source.empty()) return result;
 QPointF center;
 for (const auto& vertex : source) center += vertex.pos;
 center /= static_cast<qreal>(source.size());
 for (int index : selected) if (index >= 0 && index < static_cast<int>(result.size())) {
  const size_t offset = static_cast<size_t>(index);
  const QPointF vector = source[offset].pos - center;
  result[offset].pos = center + QPointF(
      vector.x() * factors.x(), vector.y() * factors.y());
  result[offset].inTangent = QPointF(source[offset].inTangent.x() * factors.x(),
                                     source[offset].inTangent.y() * factors.y());
  result[offset].outTangent = QPointF(source[offset].outTangent.x() * factors.x(),
                                      source[offset].outTangent.y() * factors.y());
 }
 return result;
}

std::vector<QPointF> insetPolygon(const std::vector<QPointF>& source, double factor)
{
 auto result = source;
 const QPointF center = polygonCenter(source);
 for (auto& point : result) point = center + (point - center) * factor;
 return result;
}

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

constexpr float kMinProportionalEditRadius = 8.0f;
constexpr float kMaxProportionalEditRadius = 4096.0f;

float proportionalEditWeight(const qreal distance, const qreal radius)
{
 if (radius <= 0.0) {
  return distance <= 0.0 ? 1.0f : 0.0f;
 }
 if (distance >= radius) {
  return 0.0f;
 }
 const qreal t = std::clamp(1.0 - (distance / radius), 0.0, 1.0);
 return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

std::vector<MaskVertex> proportionalMaskVertices(
    const std::vector<MaskVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius)
{
 auto result = source;
 const QPointF delta = target - origin;
 for (size_t index = 0; index < result.size(); ++index) {
  const float weight = proportionalEditWeight(
      QLineF(source[index].position, origin).length(), radius);
  result[index].position = source[index].position + delta * weight;
 }
 return result;
}

std::vector<QPointF> proportionalShapePoints(
    const std::vector<QPointF>& source, const QPointF& origin,
    const QPointF& target, qreal radius, qreal width, qreal height)
{
 auto result = source;
 const QPointF delta = target - origin;
 for (size_t index = 0; index < result.size(); ++index) {
  const float weight = proportionalEditWeight(
      QLineF(source[index], origin).length(), radius);
  const QPointF moved = source[index] + delta * weight;
  result[index] = QPointF(std::clamp(moved.x(), 0.0, static_cast<double>(width)),
                          std::clamp(moved.y(), 0.0, static_cast<double>(height)));
 }
 return result;
}

std::vector<CustomPathVertex> proportionalPathVertices(
    const std::vector<CustomPathVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius)
{
 auto result = source;
 const QPointF delta = target - origin;
 for (size_t index = 0; index < result.size(); ++index) {
  const float weight = proportionalEditWeight(
      QLineF(source[index].pos, origin).length(), radius);
  result[index].pos = source[index].pos + delta * weight;
 }
 return result;
}

bool hitTestMaskVertexGeometry(const ArtifactAbstractLayerPtr& layer,
                               const QPointF& canvasPos, float threshold,
                               int& maskIndex, int& pathIndex,
                               int& vertexIndex)
{
 if (!layer) return false;
 bool invertible = false;
 const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
 if (!invertible) return false;
 const QPointF localPos = inverse.map(canvasPos);
 for (int mask = 0; mask < layer->maskCount(); ++mask) {
  const LayerMask layerMask = layer->mask(mask);
  for (int path = 0; path < layerMask.maskPathCount(); ++path) {
   const MaskPath maskPath = layerMask.maskPath(path);
   for (int vertex = 0; vertex < maskPath.vertexCount(); ++vertex) {
    const QPointF position = maskPath.vertex(vertex).position;
    if (QLineF(position, localPos).length() <= threshold) {
     maskIndex = mask;
     pathIndex = path;
     vertexIndex = vertex;
     return true;
    }
   }
  }
 }
 return false;
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
   const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> /
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
   const float angle = static_cast<float>(i) * std::numbers::pi_v<float> /
                       static_cast<float>(pts) - std::numbers::pi_v<float> * 0.5f;
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
   const float angle = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> /
                       static_cast<float>(sides) - std::numbers::pi_v<float> * 0.5f;
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

bool ensureShapeEditSeedGeometry(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || !layer->isVisible() || layer->isLocked()) return false;
 auto shape = ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
     ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer));
 if (!shape || shape->hasCustomPolygon()) return false;
 const auto points = buildShapeEditSeedPoints(*shape);
 if (points.size() < 3) return false;
 shape->setCustomPolygonPoints(points, true);
 return true;
}

QPointF shapeCornerRadiusHandlePosition(const ArtifactShapeLayer& shape)
{
 const float corner = shape.cornerRadius();
 const float width = static_cast<float>(shape.shapeWidth());
 if (shape.shapeType() == ShapeType::Square) {
  const float height = static_cast<float>(shape.shapeHeight());
  const float side = std::min(width, height);
  return QPointF((width - side) * 0.5f + side - corner,
                 (height - side) * 0.5f);
 }
 return QPointF(width - corner, 0.0f);
}

QPointF shapeStarInnerRadiusHandlePosition(const ArtifactShapeLayer& shape)
{
 const float outerRadius = std::min(shape.shapeWidth(), shape.shapeHeight()) * 0.5f;
 return QPointF(shape.shapeWidth() * 0.5f,
                shape.shapeHeight() * 0.5f - outerRadius * shape.starInnerRadius());
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


} // namespace Artifact
