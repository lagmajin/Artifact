module;

#include <QPointF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <vector>

module Artifact.Widgets.LayerEditor.Geometry;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;

namespace Artifact {
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
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
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
   const float angle = static_cast<float>(i) * static_cast<float>(M_PI) /
                       static_cast<float>(pts) - static_cast<float>(M_PI) * 0.5f;
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
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(sides) - static_cast<float>(M_PI) * 0.5f;
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
