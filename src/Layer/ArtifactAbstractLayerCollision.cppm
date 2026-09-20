#include <compare>

import Artifact.Layer.Abstract;
import Physics.Fluid;
import Core.ArtifactMath;

// QString is provided by Artifact.Layer.Abstract's public surface, but
// preprocessor macros do not cross an IFC boundary.
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {
using namespace ArtifactCore;
using CollisionIndex = decltype(std::vector<QPointF>{}.size());

std::vector<QPointF> layerCollisionPolygonLocalPoints(
    const ArtifactAbstractLayer* layer) {
  if (!layer) {
    return {};
  }

  const auto shapeProperty =
      layer->getProperty(QStringLiteral("component.collision.shape"));
  const int shape = shapeProperty
                        ? artifactClamp(shapeProperty->getValue().toInt(), 0, 3)
                        : 0;
  if (shape != 3) {
    return {};
  }

  std::vector<QPointF> points = layer->collisionOutlineLocalPoints();
  if (points.size() < 3) {
    return {};
  }

  const auto floatProperty = [layer](const QString& propertyPath) {
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toFloat() : 0.0f;
  };
  const QPointF offset(
      static_cast<qreal>(floatProperty(
          QStringLiteral("component.collision.offsetX"))),
      static_cast<qreal>(floatProperty(
          QStringLiteral("component.collision.offsetY"))));
  for (QPointF& point : points) {
    point += offset;
  }
  return points;
}

bool configureLiquidContainerPolygon(
    const ArtifactAbstractLayer* layer, ArtifactCore::LiquidSolver2D& liquid,
    int requestedOpeningEdge = -1,
    std::vector<QPointF>* configuredPoints = nullptr,
    CollisionIndex* configuredOpeningEdge = nullptr) {
  if (configuredPoints) configuredPoints->clear();
  if (configuredOpeningEdge) *configuredOpeningEdge = 0;
  if (!layer) {
    liquid.clearContainerPolygon();
    return false;
  }
  const auto enabledProperty =
      layer->getProperty(QStringLiteral("component.collision.enabled"));
  const auto shapeProperty =
      layer->getProperty(QStringLiteral("component.collision.shape"));
  if (!enabledProperty || !enabledProperty->getValue().toBool() ||
      !shapeProperty || shapeProperty->getValue().toInt() != 3) {
    liquid.clearContainerPolygon();
    return false;
  }

  const QRectF bounds = layer->localBounds();
  const auto points = layerCollisionPolygonLocalPoints(layer);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0 ||
      points.size() < 3) {
    liquid.clearContainerPolygon();
    return false;
  }

  std::vector<ArtifactCore::LiquidContainerPoint2D> normalized;
  normalized.reserve(points.size());
  for (const QPointF& point : points) {
    normalized.push_back({
        static_cast<float>((point.x() - bounds.left()) / bounds.width()),
        static_cast<float>((point.y() - bounds.top()) / bounds.height())});
  }

  CollisionIndex openingEdge = 0;
  double openingMidpointY = NumericTraits<double>::maxValue();
  double openingLengthSquared = -1.0;
  for (CollisionIndex i = 0; i < normalized.size(); ++i) {
    const auto& a = normalized[i];
    const auto& b = normalized[(i + 1) % normalized.size()];
    const double midpointY = (static_cast<double>(a.y) + b.y) * 0.5;
    const double dx = static_cast<double>(b.x) - a.x;
    const double dy = static_cast<double>(b.y) - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (midpointY < openingMidpointY - 1.0e-8 ||
        (artifactAbs(midpointY - openingMidpointY) <= 1.0e-8 &&
         lengthSquared > openingLengthSquared)) {
      openingEdge = i;
      openingMidpointY = midpointY;
      openingLengthSquared = lengthSquared;
    }
  }
  if (requestedOpeningEdge >= 0) {
    openingEdge = artifactMin(
        static_cast<CollisionIndex>(requestedOpeningEdge),
        normalized.size() - 1);
  }
  if (!liquid.setContainerPolygon(normalized, openingEdge)) {
    liquid.clearContainerPolygon();
    return false;
  }
  if (configuredPoints) *configuredPoints = points;
  if (configuredOpeningEdge) *configuredOpeningEdge = openingEdge;
  return true;
}

QRectF layerCollisionLocalBounds(const ArtifactAbstractLayer* layer) {
  if (!layer) {
    return QRectF();
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid()) {
    return QRectF();
  }

  const auto collisionIntProperty = [layer](const QString& propertyPath,
                                            int fallback) {
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toInt() : fallback;
  };
  const auto collisionFloatProperty = [layer](const QString& propertyPath,
                                              float fallback) {
    const auto property = layer->getProperty(propertyPath);
    return property ? property->getValue().toFloat() : fallback;
  };

  const int shape = collisionIntProperty(
      QStringLiteral("component.collision.shape"), 0);
  const float width = artifactMax(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.width"), 0.0f));
  const float height = artifactMax(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.height"), 0.0f));
  const float radius = artifactMax(
      0.0f, collisionFloatProperty(
                QStringLiteral("component.collision.radius"), 0.0f));
  const float offsetX = collisionFloatProperty(
      QStringLiteral("component.collision.offsetX"), 0.0f);
  const float offsetY = collisionFloatProperty(
      QStringLiteral("component.collision.offsetY"), 0.0f);
  const QPointF center = localBounds.center() +
                         QPointF(static_cast<qreal>(offsetX),
                                 static_cast<qreal>(offsetY));

  if (shape == 1) {
    const qreal boxWidth = width > 0.0f ? static_cast<qreal>(width)
                                        : localBounds.width();
    const qreal boxHeight = height > 0.0f ? static_cast<qreal>(height)
                                          : localBounds.height();
    return QRectF(center.x() - boxWidth * 0.5, center.y() - boxHeight * 0.5,
                  boxWidth, boxHeight);
  }

  if (shape == 2) {
    const qreal circleRadius =
        radius > 0.0f
            ? static_cast<qreal>(radius)
            : static_cast<qreal>(
                  artifactMax(localBounds.width(), localBounds.height()) * 0.5);
    return QRectF(center.x() - circleRadius, center.y() - circleRadius,
                  circleRadius * 2.0, circleRadius * 2.0);
  }

  if (shape == 3) {
    const std::vector<QPointF> polygon =
        layerCollisionPolygonLocalPoints(layer);
    if (polygon.size() >= 3) {
      auto boundsIt = polygon.begin();
      qreal minX = boundsIt->x();
      qreal minY = boundsIt->y();
      qreal maxX = minX;
      qreal maxY = minY;
      for (; boundsIt != polygon.end(); ++boundsIt) {
        minX = artifactMin(minX, boundsIt->x());
        minY = artifactMin(minY, boundsIt->y());
        maxX = artifactMax(maxX, boundsIt->x());
        maxY = artifactMax(maxY, boundsIt->y());
      }
      return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }
    // No usable outline: fall through to the auto-bounds proxy.
  }

  return localBounds.translated(static_cast<qreal>(offsetX),
                                static_cast<qreal>(offsetY));
}

namespace {

bool pointInsideCollisionPolygon(const QPointF& point,
                                 const std::vector<QPointF>& polygon) {
  bool inside = false;
  if (polygon.size() < 3) return false;
  for (CollisionIndex i = 0, j = polygon.size() - 1; i < polygon.size();
       j = i++) {
    const QPointF& a = polygon[i];
    const QPointF& b = polygon[j];
    const bool crosses = ((a.y() > point.y()) != (b.y() > point.y())) &&
        (point.x() < (b.x() - a.x()) * (point.y() - a.y()) /
                             ((b.y() - a.y()) + 1.0e-12) +
                         a.x());
    if (crosses) inside = !inside;
  }
  return inside;
}

QPointF closestPointOnCollisionSegment(const QPointF& point,
                                       const QPointF& a,
                                       const QPointF& b) {
  const QPointF edge = b - a;
  const double lengthSquared = QPointF::dotProduct(edge, edge);
  if (lengthSquared <= 1.0e-12) return a;
  const double t = artifactClamp(
      QPointF::dotProduct(point - a, edge) / lengthSquared, 0.0, 1.0);
  return a + edge * t;
}

bool liquidSegmentIntersection(const QPointF& start, const QPointF& end,
                               const QPointF& a, const QPointF& b,
                               double& hitT) {
  const QPointF motion = end - start;
  const QPointF edge = b - a;
  const double cross = motion.x() * edge.y() - motion.y() * edge.x();
  if (artifactAbs(cross) <= 1.0e-12) return false;
  const QPointF offset = a - start;
  const double t = (offset.x() * edge.y() - offset.y() * edge.x()) / cross;
  const double u = (offset.x() * motion.y() - offset.y() * motion.x()) / cross;
  if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) return false;
  hitT = t;
  return true;
}

} // namespace

bool resolveLiquidPointAgainstCollisionLayer(
    const ArtifactAbstractLayer* layer, int64_t frameNumber,
    float particleRadius, float previousWorldX, float previousWorldY,
    float& worldX, float& worldY,
    float& worldVx, float& worldVy, float& collisionImpact) {
  if (!layer || particleRadius <= 0.0f) return false;
  const auto enabledProperty =
      layer->getProperty(QStringLiteral("component.collision.enabled"));
  if (!enabledProperty || !enabledProperty->getValue().toBool()) return false;

  bool invertible = false;
  const QTransform layerTransform = layer->getGlobalTransformAt(frameNumber);
  const QTransform inverseTransform = layerTransform.inverted(&invertible);
  if (!invertible) return false;

  const QPointF worldPoint(worldX, worldY);
  const QPointF localPoint = inverseTransform.map(worldPoint);
  const QPointF previousLocalPoint =
      inverseTransform.map(QPointF(previousWorldX, previousWorldY));
  const QPointF localMotion = localPoint - previousLocalPoint;
  const double scaleX = artifactHypot(layerTransform.m11(), layerTransform.m12());
  const double scaleY = artifactHypot(layerTransform.m21(), layerTransform.m22());
  const double minScale = artifactMax(1.0e-6, artifactMin(scaleX, scaleY));
  const double localRadius = static_cast<double>(particleRadius) / minScale;
  const auto shapeProperty =
      layer->getProperty(QStringLiteral("component.collision.shape"));
  const int shape = shapeProperty
                        ? artifactClamp(shapeProperty->getValue().toInt(), 0, 3)
                        : 0;

  QPointF correctedLocal = localPoint;
  QPointF localNormal(0.0, -1.0);
  bool collided = false;
  if (shape == 2) {
    const QRectF bounds = layerCollisionLocalBounds(layer);
    if (!bounds.isValid()) return false;
    const QPointF center = bounds.center();
    QPointF delta = localPoint - center;
    double distance = artifactHypot(delta.x(), delta.y());
    const double collisionRadius = bounds.width() * 0.5 + localRadius;
    if (distance < collisionRadius) {
      if (distance < 1.0e-8) {
        delta = QPointF(0.0, -1.0);
        distance = 1.0;
      }
      localNormal = delta / distance;
      correctedLocal = center + localNormal * collisionRadius;
      collided = true;
    } else {
      const QPointF offset = previousLocalPoint - center;
      const double a = QPointF::dotProduct(localMotion, localMotion);
      const double b = 2.0 * QPointF::dotProduct(offset, localMotion);
      const double c = QPointF::dotProduct(offset, offset) -
                       collisionRadius * collisionRadius;
      const double discriminant = b * b - 4.0 * a * c;
      if (a > 1.0e-12 && c > 0.0 && discriminant >= 0.0) {
        const double hitT = (-b - artifactSqrt(discriminant)) / (2.0 * a);
        if (hitT >= 0.0 && hitT <= 1.0) {
          const QPointF hit = previousLocalPoint + localMotion * hitT;
          const QPointF hitDelta = hit - center;
          const double hitLength = artifactHypot(hitDelta.x(), hitDelta.y());
          if (hitLength > 1.0e-8) {
            localNormal = hitDelta / hitLength;
            correctedLocal = center + localNormal * collisionRadius;
            collided = true;
          }
        }
      }
    }
  } else if (shape == 3) {
    const auto polygon = layerCollisionPolygonLocalPoints(layer);
    if (polygon.size() < 3) return false;
    QPointF closest;
    double closestDistance = NumericTraits<double>::maxValue();
    for (CollisionIndex i = 0; i < polygon.size(); ++i) {
      const QPointF candidate = closestPointOnCollisionSegment(
          localPoint, polygon[i], polygon[(i + 1) % polygon.size()]);
      const double distance = artifactHypot(candidate.x() - localPoint.x(),
                                         candidate.y() - localPoint.y());
      if (distance < closestDistance) {
        closestDistance = distance;
        closest = candidate;
      }
    }
    const bool inside = pointInsideCollisionPolygon(localPoint, polygon);
    if (inside || closestDistance < localRadius) {
      QPointF direction = inside ? closest - localPoint : localPoint - closest;
      double directionLength = artifactHypot(direction.x(), direction.y());
      if (directionLength < 1.0e-8) {
        direction = QPointF(0.0, -1.0);
        directionLength = 1.0;
      }
      localNormal = direction / directionLength;
      correctedLocal = closest + localNormal * localRadius;
      collided = true;
    } else {
      double earliestT = NumericTraits<double>::maxValue();
      QPointF earliestA;
      QPointF earliestB;
      for (CollisionIndex i = 0; i < polygon.size(); ++i) {
        double hitT = 0.0;
        const QPointF& a = polygon[i];
        const QPointF& b = polygon[(i + 1) % polygon.size()];
        if (liquidSegmentIntersection(previousLocalPoint, localPoint,
                                      a, b, hitT) && hitT < earliestT) {
          earliestT = hitT;
          earliestA = a;
          earliestB = b;
        }
      }
      if (earliestT <= 1.0) {
        const QPointF hit = previousLocalPoint + localMotion * earliestT;
        const QPointF edge = earliestB - earliestA;
        localNormal = QPointF(-edge.y(), edge.x());
        double normalLength = artifactHypot(localNormal.x(), localNormal.y());
        if (normalLength > 1.0e-8) {
          localNormal /= normalLength;
          if (QPointF::dotProduct(localNormal, localMotion) > 0.0) {
            localNormal = -localNormal;
          }
          correctedLocal = hit + localNormal * localRadius;
          collided = true;
        }
      }
    }
  } else {
    const QRectF bounds = layerCollisionLocalBounds(layer);
    if (!bounds.isValid()) return false;
    const QRectF expanded = bounds.adjusted(-localRadius, -localRadius,
                                             localRadius, localRadius);
    if (expanded.contains(localPoint)) {
      const double left = localPoint.x() - expanded.left();
      const double right = expanded.right() - localPoint.x();
      const double top = localPoint.y() - expanded.top();
      const double bottom = expanded.bottom() - localPoint.y();
      const double nearest = artifactMin(artifactMin(left, right),
                                         artifactMin(top, bottom));
      if (nearest == left) {
        localNormal = QPointF(-1.0, 0.0);
        correctedLocal.setX(expanded.left());
      } else if (nearest == right) {
        localNormal = QPointF(1.0, 0.0);
        correctedLocal.setX(expanded.right());
      } else if (nearest == top) {
        localNormal = QPointF(0.0, -1.0);
        correctedLocal.setY(expanded.top());
      } else {
        localNormal = QPointF(0.0, 1.0);
        correctedLocal.setY(expanded.bottom());
      }
      collided = true;
    } else {
      double entryT = 0.0;
      double exitT = 1.0;
      QPointF entryNormal;
      const auto clipAxis = [&](double start, double delta, double minimum,
                                double maximum, const QPointF& minimumNormal,
                                const QPointF& maximumNormal) {
        if (artifactAbs(delta) <= 1.0e-12) {
          return start >= minimum && start <= maximum;
        }
        double nearT = (minimum - start) / delta;
        double farT = (maximum - start) / delta;
        QPointF nearNormal = minimumNormal;
        if (nearT > farT) {
          artifactSwap(nearT, farT);
          nearNormal = maximumNormal;
        }
        if (nearT > entryT) {
          entryT = nearT;
          entryNormal = nearNormal;
        }
        exitT = artifactMin(exitT, farT);
        return entryT <= exitT;
      };
      if (!expanded.contains(previousLocalPoint) &&
          clipAxis(previousLocalPoint.x(), localMotion.x(), expanded.left(),
                   expanded.right(), QPointF(-1.0, 0.0), QPointF(1.0, 0.0)) &&
          clipAxis(previousLocalPoint.y(), localMotion.y(), expanded.top(),
                   expanded.bottom(), QPointF(0.0, -1.0), QPointF(0.0, 1.0)) &&
          entryT >= 0.0 && entryT <= 1.0) {
        localNormal = entryNormal;
        correctedLocal = previousLocalPoint + localMotion * entryT;
        collided = true;
      }
    }
  }

  if (!collided) return false;
  const QPointF correctedWorld = layerTransform.map(correctedLocal);
  const QPointF worldOrigin = layerTransform.map(QPointF(0.0, 0.0));
  QPointF worldNormalPoint = layerTransform.map(localNormal) - worldOrigin;
  const double normalLength =
      artifactHypot(worldNormalPoint.x(), worldNormalPoint.y());
  if (normalLength < 1.0e-8) return false;
  worldNormalPoint /= normalLength;
  worldX = static_cast<float>(correctedWorld.x());
  worldY = static_cast<float>(correctedWorld.y());

  const double normalVelocity =
      static_cast<double>(worldVx) * worldNormalPoint.x() +
      static_cast<double>(worldVy) * worldNormalPoint.y();
  if (normalVelocity < 0.0) {
    collisionImpact = artifactMax(
        collisionImpact, static_cast<float>(-normalVelocity));
    constexpr double restitution = 0.05;
    constexpr double tangentRetention = 0.88;
    const double tangentVx =
        static_cast<double>(worldVx) - normalVelocity * worldNormalPoint.x();
    const double tangentVy =
        static_cast<double>(worldVy) - normalVelocity * worldNormalPoint.y();
    worldVx = static_cast<float>(
        tangentVx * tangentRetention -
        normalVelocity * restitution * worldNormalPoint.x());
    worldVy = static_cast<float>(
        tangentVy * tangentRetention -
        normalVelocity * restitution * worldNormalPoint.y());
  }
  return true;
}

} // namespace Artifact
