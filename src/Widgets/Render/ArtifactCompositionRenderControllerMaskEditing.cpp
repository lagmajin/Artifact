module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <cmath>
#include <limits>

#include <QPointF>
#include <QTransform>
#include <QVector>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController:MaskEditing;

import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Render.IRenderer;
import Color.Float;

namespace Artifact {

enum class MaskEditHandleType { None, InTangent, OutTangent, FeatherHandle };

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex, MaskEditHandleType handleType)

{

  const MaskVertex vertex = path.vertex(vertexIndex);

  switch (handleType) {

  case MaskEditHandleType::InTangent:

    return vertex.position + vertex.inTangent;

  case MaskEditHandleType::OutTangent:

    return vertex.position + vertex.outTangent;

  case MaskEditHandleType::FeatherHandle: {

    const int count = path.vertexCount();

    if (count < 2) return vertex.position;

    // 隣接頂点への方向を計算して法線を求める
    const int prevIdx = (vertexIndex > 0) ? vertexIndex - 1
                                          : (path.isClosed() ? count - 1 : vertexIndex);

    const int nextIdx = (vertexIndex < count - 1) ? vertexIndex + 1
                                                  : (path.isClosed() ? 0 : vertexIndex);

    const QPointF prevPos = path.vertex(prevIdx).position;

    const QPointF nextPos = path.vertex(nextIdx).position;

    QPointF tangent = nextPos - prevPos;

    const qreal len = std::hypot(tangent.x(), tangent.y());

    if (len < 1e-6) return vertex.position;

    tangent /= len;

    // 外法線（時計回り90度）— パスが時計回りの場合外側

    QPointF normal(-tangent.y(), tangent.x());

    return vertex.position +
           normal * static_cast<qreal>(path.feather().value);

  }

  case MaskEditHandleType::None:

    break;

  }

  return vertex.position;

}



QPointF maskFeatherHandleCanvasPosition(const MaskPath& path, int vertexIndex,
                                        const QTransform& globalTransform,
                                        ArtifactIRenderer* renderer,
                                        float minimumScreenDistance = 18.0f) {
  const MaskVertex vertex = path.vertex(vertexIndex);
  const QPointF anchorCanvas = globalTransform.map(vertex.position);
  if (!renderer || path.feather().value > 0.01f) {
    return globalTransform.map(
        maskHandlePosition(path, vertexIndex, MaskEditHandleType::FeatherHandle));
  }
  const int count = path.vertexCount();
  if (count < 2) return anchorCanvas;
  const int prevIndex = vertexIndex > 0 ? vertexIndex - 1
                                        : (path.isClosed() ? count - 1 : vertexIndex);
  const int nextIndex = vertexIndex + 1 < count ? vertexIndex + 1
                                                  : (path.isClosed() ? 0 : vertexIndex);
  QPointF tangent = path.vertex(nextIndex).position - path.vertex(prevIndex).position;
  const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
  if (tangentLength <= 1e-6) {
    tangent = QPointF(0.0, -1.0);
  } else {
    tangent /= tangentLength;
    tangent = QPointF(-tangent.y(), tangent.x());
  }
  const QPointF normalCanvas =
      globalTransform.map(vertex.position + tangent) - anchorCanvas;
  const auto anchorViewport = renderer->canvasToViewport(
      {static_cast<float>(anchorCanvas.x()), static_cast<float>(anchorCanvas.y())});
  const auto normalViewport = renderer->canvasToViewport(
      {static_cast<float>(anchorCanvas.x() + normalCanvas.x()),
       static_cast<float>(anchorCanvas.y() + normalCanvas.y())});
  QPointF direction(normalViewport.x - anchorViewport.x,
                    normalViewport.y - anchorViewport.y);
  const qreal directionLength = std::hypot(direction.x(), direction.y());
  direction = directionLength <= 1e-6 ? QPointF(0.0, -1.0)
                                      : direction / directionLength;
  const auto displayCanvas = renderer->viewportToCanvas(
      {anchorViewport.x + static_cast<float>(direction.x() * minimumScreenDistance),
       anchorViewport.y + static_cast<float>(direction.y() * minimumScreenDistance)});
  return QPointF(displayCanvas.x, displayCanvas.y);
}


QPointF cubicBezierPoint(const QPointF &p0, const QPointF &p1,

                         const QPointF &p2, const QPointF &p3, double t) {

  const double u = 1.0 - t;

  const double tt = t * t;

  const double uu = u * u;

  const double uuu = uu * u;

  const double ttt = tt * t;

  return p0 * uuu + p1 * (3.0 * uu * t) + p2 * (3.0 * u * tt) + p3 * ttt;

}



QVector<QPointF> maskSegmentPolyline(const MaskVertex &start,

                                     const MaskVertex &end,

                                     int subdivisions = 18) {

  QVector<QPointF> points;

  points.reserve(std::max(2, subdivisions + 1));

  const QPointF p0 = start.position;

  const QPointF p1 = start.position + start.outTangent;

  const QPointF p2 = end.position + end.inTangent;

  const QPointF p3 = end.position;

  for (int i = 0; i <= subdivisions; ++i) {

    const double t = static_cast<double>(i) / static_cast<double>(subdivisions);

    points.push_back(cubicBezierPoint(p0, p1, p2, p3, t));

  }

  return points;

}



bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       ArtifactIRenderer* renderer,
                       const QPointF& viewportPos,
                       float threshold,
                       int& outMaskIndex,
                       int& outPathIndex,
                       int& outVertexIndex,
                       MaskEditHandleType& outHandleType)
{
  if (!layer || !renderer) return false;

  const QTransform globalTransform = layer->getGlobalTransform();
  const float thresholdSq = threshold * threshold;
  float bestDistanceSq = std::numeric_limits<float>::max();
  for (int m = 0; m < layer->maskCount(); ++m) {
    const LayerMask mask = layer->mask(m);
    if (!mask.isEnabled() || mask.isLocked()) continue;
    for (int p = 0; p < mask.maskPathCount(); ++p) {
      const MaskPath path = mask.maskPath(p);
      for (int v = 0; v < path.vertexCount(); ++v) {
        const MaskVertex vertex = path.vertex(v);
        for (MaskEditHandleType handleType : {MaskEditHandleType::InTangent,
                                          MaskEditHandleType::OutTangent,
                                          MaskEditHandleType::FeatherHandle}) {
          if ((handleType == MaskEditHandleType::InTangent && vertex.inTangent == QPointF(0, 0)) ||
              (handleType == MaskEditHandleType::OutTangent && vertex.outTangent == QPointF(0, 0))) continue;
          const QPointF handleCanvas = handleType == MaskEditHandleType::FeatherHandle
              ? maskFeatherHandleCanvasPosition(path, v, globalTransform, renderer)
              : globalTransform.map(maskHandlePosition(path, v, handleType));
          const auto handleViewport = renderer->canvasToViewport(
              {static_cast<float>(handleCanvas.x()), static_cast<float>(handleCanvas.y())});
          const float dx = handleViewport.x - static_cast<float>(viewportPos.x());
          const float dy = handleViewport.y - static_cast<float>(viewportPos.y());
          const float distanceSq = dx * dx + dy * dy;
          if (distanceSq <= thresholdSq && distanceSq < bestDistanceSq) {
            outMaskIndex = m;
            outPathIndex = p;
            outVertexIndex = v;
            outHandleType = handleType;
            bestDistanceSq = distanceSq;
          }
        }
      }
    }
  }
  return outHandleType != MaskEditHandleType::None;
}



bool hitTestMaskSegment(const ArtifactAbstractLayerPtr& layer,

                        const QPointF& canvasPos,

                        float threshold,

                        int& outMaskIndex,

                        int& outPathIndex,

                        int& outSegmentIndex)

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

  const auto distanceToSegmentSq = [](const QPointF &p, const QPointF &a,

                                      const QPointF &b) {

    const QPointF ab = b - a;

    const double abLenSq = QPointF::dotProduct(ab, ab);

    if (abLenSq <= std::numeric_limits<double>::epsilon()) {

      const QPointF delta = p - a;

      return QPointF::dotProduct(delta, delta);

    }



    const QPointF ap = p - a;

    const double t =

        std::clamp(QPointF::dotProduct(ap, ab) / abLenSq, 0.0, 1.0);

    const QPointF proj = a + ab * t;

    const QPointF delta = p - proj;

    return QPointF::dotProduct(delta, delta);

  };



  for (int m = 0; m < layer->maskCount(); ++m) {

    const LayerMask mask = layer->mask(m);

    if (!mask.isEnabled() || mask.isLocked()) {

      continue;

    }

    for (int p = 0; p < mask.maskPathCount(); ++p) {

      const MaskPath path = mask.maskPath(p);

      const int vertexCount = path.vertexCount();

      if (vertexCount < 2) {

        continue;

      }



      const int segmentCount = path.isClosed() ? vertexCount : vertexCount - 1;

      for (int s = 0; s < segmentCount; ++s) {

        const MaskVertex startVertex = path.vertex(s);

        const MaskVertex endVertex = path.vertex((s + 1) % vertexCount);

        const QVector<QPointF> samples =

            maskSegmentPolyline(startVertex, endVertex, 14);

        for (int i = 1; i < samples.size(); ++i) {

          if (distanceToSegmentSq(localPos, samples[i - 1], samples[i]) <=

              thresholdSq) {

            outMaskIndex = m;

            outPathIndex = p;

            outSegmentIndex = s;

            return true;

          }

        }

      }

    }

  }



  return false;

}



struct MaskSegmentNearestPoint {
  QPointF position;
  double t = 0.0;
};

MaskSegmentNearestPoint closestPointOnMaskSegment(const MaskVertex &start, const MaskVertex &end,

                                  const QPointF &localPos) {

  const QVector<QPointF> samples = maskSegmentPolyline(start, end, 18);

  MaskSegmentNearestPoint best{start.position, 0.0};

  double bestDistSq = std::numeric_limits<double>::max();

  for (int i = 1; i < samples.size(); ++i) {

    const QPointF a = samples[i - 1];

    const QPointF b = samples[i];

    const QPointF ab = b - a;

    const double abLenSq = QPointF::dotProduct(ab, ab);

    QPointF proj;
    double segmentT = 0.0;

    if (abLenSq <= std::numeric_limits<double>::epsilon()) {

      proj = a;

    } else {

      const QPointF ap = localPos - a;

      segmentT =

          std::clamp(QPointF::dotProduct(ap, ab) / abLenSq, 0.0, 1.0);

      proj = a + ab * segmentT;

    }

    const QPointF delta = localPos - proj;

    const double distSq = QPointF::dotProduct(delta, delta);

    if (distSq < bestDistSq) {

      bestDistSq = distSq;

      best.position = proj;
      const double startT = static_cast<double>(i - 1) / 18.0;
      best.t = startT + (1.0 / 18.0) * segmentT;

    }

  }

  return best;

}



bool insertVertexOnMaskSegment(const ArtifactAbstractLayerPtr &layer,

                               int maskIndex, int pathIndex, int segmentIndex,

                               const QPointF &localPos) {

  if (!layer || maskIndex < 0 || pathIndex < 0 || segmentIndex < 0) {

    return false;

  }



  LayerMask mask = layer->mask(maskIndex);

  MaskPath path = mask.maskPath(pathIndex);

  const int vertexCount = path.vertexCount();

  if (vertexCount < 2 || segmentIndex >= vertexCount) {

    return false;

  }



  const MaskVertex startVertex = path.vertex(segmentIndex);

  const MaskVertex endVertex = path.vertex((segmentIndex + 1) % vertexCount);

  const MaskSegmentNearestPoint nearest =

      closestPointOnMaskSegment(startVertex, endVertex, localPos);
  const double t = nearest.t;
  const QPointF p0 = startVertex.position;
  const QPointF p1 = startVertex.position + startVertex.outTangent;
  const QPointF p2 = endVertex.position + endVertex.inTangent;
  const QPointF p3 = endVertex.position;
  const auto lerp = [t](const QPointF& a, const QPointF& b) {
    return a + (b - a) * t;
  };
  const QPointF p01 = lerp(p0, p1);
  const QPointF p12 = lerp(p1, p2);
  const QPointF p23 = lerp(p2, p3);
  const QPointF p012 = lerp(p01, p12);
  const QPointF p123 = lerp(p12, p23);
  const QPointF splitPoint = lerp(p012, p123);
  MaskVertex splitStart = startVertex;
  splitStart.outTangent = p01 - p0;
  MaskVertex splitEnd = endVertex;
  splitEnd.inTangent = p23 - p3;
  MaskVertex newVertex;
  newVertex.position = splitPoint;
  newVertex.inTangent = p012 - splitPoint;
  newVertex.outTangent = p123 - splitPoint;
  path.setVertex(segmentIndex, splitStart);
  path.setVertex((segmentIndex + 1) % vertexCount, splitEnd);

  path.insertVertex(segmentIndex + 1, newVertex);

  mask.setMaskPath(pathIndex, path);

  layer->setMask(maskIndex, mask);

  return true;

}

void setMaskVertexHandle(MaskVertex &vertex, MaskEditHandleType handleType,

                         const QPointF &handleDelta, bool breakTangents) {

  const QPointF mirroredDelta(-handleDelta.x(), -handleDelta.y());

  if (handleType == MaskEditHandleType::InTangent) {

    vertex.inTangent = handleDelta;

    if (!breakTangents) {

      vertex.outTangent = mirroredDelta;

    }

  } else if (handleType == MaskEditHandleType::OutTangent) {

    vertex.outTangent = handleDelta;

    if (!breakTangents) {

      vertex.inTangent = mirroredDelta;

    }

  }

}



void drawMaskSquareMarker(ArtifactIRenderer *renderer,

                          const Detail::float2 &center,

                          float size,

                          const FloatColor &color,

                          const FloatColor *shadowColor = nullptr,

                          float shadowExpand = 0.0f)

{

  if (!renderer) {

    return;

  }

  const auto drawSquareOutline = [&](float squareSize,

                                     const FloatColor &outlineColor,

                                     float thickness) {

    const float half = squareSize * 0.5f;

    const Detail::float2 tl{center.x - half, center.y - half};

    const Detail::float2 tr{center.x + half, center.y - half};

    const Detail::float2 br{center.x + half, center.y + half};

    const Detail::float2 bl{center.x - half, center.y + half};

    renderer->drawThickLineLocal(tl, tr, thickness, outlineColor);

    renderer->drawThickLineLocal(tr, br, thickness, outlineColor);

    renderer->drawThickLineLocal(br, bl, thickness, outlineColor);

    renderer->drawThickLineLocal(bl, tl, thickness, outlineColor);

  };



  if (shadowColor) {

    const float shadowSize = size + shadowExpand;

    const float shadowHalf = shadowSize * 0.5f;

    renderer->drawSolidRect(center.x - shadowHalf, center.y - shadowHalf,

                            shadowSize, shadowSize, *shadowColor, 0.25f);

    drawSquareOutline(shadowSize, *shadowColor, 1.75f);

  }



  const float half = size * 0.5f;

  renderer->drawSolidRect(center.x - half, center.y - half, size, size,

                          color, 0.20f);

  drawSquareOutline(size, color, 1.5f);

}

} // namespace Artifact
