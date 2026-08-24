module;
#include <cstddef>
#include <array>
#include <numeric>
#include <utility>
#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QBrush>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QImage>
#include <QRectF>
#include <QTransform>
#include <QLineF>
#include <QMatrix4x4>
#include <QVector4D>
#include <QPolygonF>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cmath>
#include <QPen>

module Artifact.Layer.Shape;

import std;
import Artifact.Layers.Abstract._2D;
import Artifact.Layer.CloneEffectSupport;
import Property.Types;
import Property.Abstract;
import Shape.Group;
import Shape.TrimPaths;
import Shape.Repeater;
import Shape.AeOperators;
import Shape.Types;
import Shape.Path;
import Memory.SharedPtr;
import Physics.System;
import Physics.SoftBody;
import Physics.Mpm2D;
import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Time.Rational;

namespace {

constexpr float kStrokeEffectEpsilon = 0.0001f;
constexpr int kMaxShapeDimension = 16384;
constexpr std::uint64_t kMaxShapeCachePixels = 64ull * 1024ull * 1024ull;
constexpr int kMaxShapePathVertices = 100000;
constexpr int kMaxStarPoints = kMaxShapePathVertices / 2;
constexpr int kMaxDashPatternEntries = 1024;
constexpr float kMaxShapeCoordinate = 1000000.0f;
enum class ShapeCompatibilityFallback {
 None,
 GradientFill,
 StrokeAlignment,
 CustomStrokeEffect,
 ShapeOperator
};

const char* compatibilityFallbackName(const ShapeCompatibilityFallback reason) {
 switch (reason) {
  case ShapeCompatibilityFallback::GradientFill: return "gradient-fill";
  case ShapeCompatibilityFallback::StrokeAlignment: return "stroke-alignment";
  case ShapeCompatibilityFallback::CustomStrokeEffect: return "custom-stroke-effect";
  case ShapeCompatibilityFallback::ShapeOperator: return "shape-operator";
  default: return "none";
 }
}

using ArtifactCore::FloatColor;
using ArtifactCore::ShapePath;

static ArtifactCore::ShapePath buildShapePath(Artifact::ShapeType shapeType,
                                int width,
                                int height,
                                float cornerRadius,
                                int starPoints,
                                float starInnerRadius,
                                int polygonSides);

static ArtifactCore::ShapePath buildCustomShapePath(
    const std::vector<Artifact::CustomPathVertex>& vertices,
    bool closed) {
 ArtifactCore::ShapePath path;
 if (vertices.empty()) return path;
 path.moveTo(vertices.front().pos);
 for (size_t i = 0; i + 1 < vertices.size(); ++i) {
  const auto& v0 = vertices[i];
  const auto& v1 = vertices[i + 1];
  path.cubicTo(v0.pos + v0.outTangent, v1.pos + v1.inTangent, v1.pos);
 }
 if (closed && vertices.size() >= 2) {
  const auto& v0 = vertices.back();
  const auto& v1 = vertices.front();
  path.cubicTo(v0.pos + v0.outTangent, v1.pos + v1.inTangent, v1.pos);
  path.close();
 }
 return path;
}

QPointF mapPoint(const QMatrix4x4& transform, const QPointF& point) {
 QVector4D v = transform * QVector4D(static_cast<float>(point.x()),
                                     static_cast<float>(point.y()), 0.0f, 1.0f);
 if (std::abs(v.w()) > 1e-6f) {
  return QPointF(v.x() / v.w(), v.y() / v.w());
 }
 return QPointF(v.x(), v.y());
}

void drawDashedNativeStroke(
    Artifact::ArtifactIRenderer* renderer,
    const std::vector<std::vector<ArtifactCore::BezierSegment>>& subpaths,
    const QMatrix4x4& transform,
    const std::vector<float>& pattern,
    float width,
    const ArtifactCore::FloatColor& color) {
  if (!renderer || pattern.empty() || !std::isfinite(width) || width <= 0.0f) {
    return;
  }

  std::vector<float> normalized;
  normalized.reserve(pattern.size() + (pattern.size() % 2));
  for (const float value : pattern) {
    if (std::isfinite(value) && value > 1.0e-4f) {
      normalized.push_back(value);
    }
  }
  if (normalized.empty()) {
    return;
  }
  if (normalized.size() % 2 != 0) {
    normalized.insert(normalized.end(), normalized.begin(), normalized.end());
  }

  for (const auto& segments : subpaths) {
    std::size_t patternIndex = 0;
    float patternRemaining = normalized.front();
    bool drawing = true;
    for (const auto& segment : segments) {
      const QPointF start = segment.p0;
      const QPointF end = segment.p1;
      const QPointF delta = end - start;
      const double length = std::hypot(delta.x(), delta.y());
      if (length <= 1.0e-6) {
        continue;
      }

      double consumed = 0.0;
      while (consumed < length - 1.0e-6) {
        const double step = std::min<double>(patternRemaining, length - consumed);
        const double t0 = consumed / length;
        const double t1 = (consumed + step) / length;
        if (drawing) {
          const QPointF p0 = mapPoint(transform, start + delta * t0);
          const QPointF p1 = mapPoint(transform, start + delta * t1);
          renderer->drawThickLineLocal(
              {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
              {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
              width, color);
        }
        consumed += step;
        patternRemaining -= static_cast<float>(step);
        if (patternRemaining <= 1.0e-4f) {
          patternIndex = (patternIndex + 1) % normalized.size();
          patternRemaining = normalized[patternIndex];
          drawing = !drawing;
        }
      }
    }
  }
}

float compositionFieldContentWeight(const Artifact::ArtifactShapeLayer* layer) {
 if (!layer) {
  return 1.0f;
 }
 const QPointF localCenter = layer->localBounds().center();
 const QPointF canvasPoint = layer->getGlobalTransform().map(localCenter);
 return layer->compositionFieldInfluenceAtCanvasPoint(canvasPoint);
}

bool drawSoftBodyGrid(Artifact::ArtifactShapeLayer* layer,
                      Artifact::ArtifactIRenderer* renderer,
                      const QMatrix4x4& transform,
                      const ArtifactCore::FloatColor& fill,
                      const ArtifactCore::FloatColor& stroke,
                      float strokeWidth,
                      bool drawStroke) {
 if (!layer || !renderer || !layer->hasSoftBodyPhysics()) {
  return false;
 }
 const auto solver = ArtifactCore::PhysicsSystem::instance().getSoftBody(layer->id());
 if (!solver) {
  return false;
 }
 const int columns = solver->gridColumns();
 const int rows = solver->gridRows();
 const auto& points = solver->getPoints();
 if (columns < 2 || rows < 2 ||
     points.size() != static_cast<std::size_t>(columns * rows)) {
  return false;
 }

 auto mappedPoint = [&transform](const ArtifactCore::SoftBodyPoint& point) {
  return mapPoint(transform, QPointF(point.x, point.y));
 };
 for (int y = 0; y + 1 < rows; ++y) {
  for (int x = 0; x + 1 < columns; ++x) {
   const auto index = [columns](int px, int py) {
    return static_cast<std::size_t>(py * columns + px);
   };
   const QPointF p0 = mappedPoint(points[index(x, y)]);
   const QPointF p1 = mappedPoint(points[index(x + 1, y)]);
   const QPointF p2 = mappedPoint(points[index(x + 1, y + 1)]);
   const QPointF p3 = mappedPoint(points[index(x, y + 1)]);
   renderer->drawQuadLocal({static_cast<float>(p0.x()), static_cast<float>(p0.y())},
                           {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
                           {static_cast<float>(p2.x()), static_cast<float>(p2.y())},
                           {static_cast<float>(p3.x()), static_cast<float>(p3.y())},
                           fill);
  }
 }
 if (drawStroke && strokeWidth > 0.0f) {
  for (int y = 0; y < rows; ++y) {
   for (int x = 0; x < columns; ++x) {
    const QPointF point = mappedPoint(points[static_cast<std::size_t>(y * columns + x)]);
    if (x + 1 < columns) {
     const QPointF next = mappedPoint(points[static_cast<std::size_t>(y * columns + x + 1)]);
     renderer->drawThickLineLocal({static_cast<float>(point.x()), static_cast<float>(point.y())},
                                  {static_cast<float>(next.x()), static_cast<float>(next.y())},
                                  strokeWidth, stroke);
    }
    if (y + 1 < rows) {
     const QPointF next = mappedPoint(points[static_cast<std::size_t>((y + 1) * columns + x)]);
     renderer->drawThickLineLocal({static_cast<float>(point.x()), static_cast<float>(point.y())},
                                  {static_cast<float>(next.x()), static_cast<float>(next.y())},
                                  strokeWidth, stroke);
    }
   }
  }
 }
 return true;
}

bool drawMaterialGrid(Artifact::ArtifactShapeLayer* layer,
                      Artifact::ArtifactIRenderer* renderer,
                      const QMatrix4x4& transform,
                      const ArtifactCore::FloatColor& fill,
                      const ArtifactCore::FloatColor& stroke,
                      float strokeWidth,
                      bool drawStroke) {
 if (!layer || !renderer) return false;
 const auto solver = ArtifactCore::PhysicsSystem::instance().getMaterialSolver(layer->id());
 if (!solver) return false;
 const int columns = solver->particleGridColumns();
 const int rows = solver->particleGridRows();
 if (columns < 2 || rows < 2 ||
     solver->particleCount() != columns * rows) return false;

 auto mappedPoint = [&transform](const ArtifactCore::MpmParticle2D& particle) {
  return mapPoint(transform, QPointF(particle.pos.x, particle.pos.y));
 };
 const auto index = [columns](int x, int y) {
  return y * columns + x;
 };
 for (int y = 0; y + 1 < rows; ++y) {
  for (int x = 0; x + 1 < columns; ++x) {
   const auto& p0 = solver->particle(index(x, y));
   const auto& p1 = solver->particle(index(x + 1, y));
   const auto& p2 = solver->particle(index(x + 1, y + 1));
   const auto& p3 = solver->particle(index(x, y + 1));
   if (!p0.active || !p1.active || !p2.active || !p3.active) continue;
   const QPointF v0 = mappedPoint(p0);
   const QPointF v1 = mappedPoint(p1);
   const QPointF v2 = mappedPoint(p2);
   const QPointF v3 = mappedPoint(p3);
   renderer->drawQuadLocal({static_cast<float>(v0.x()), static_cast<float>(v0.y())},
                           {static_cast<float>(v1.x()), static_cast<float>(v1.y())},
                           {static_cast<float>(v2.x()), static_cast<float>(v2.y())},
                           {static_cast<float>(v3.x()), static_cast<float>(v3.y())}, fill);
   if (drawStroke && strokeWidth > 0.0f) {
    renderer->drawThickLineLocal({static_cast<float>(v0.x()), static_cast<float>(v0.y())},
                                 {static_cast<float>(v1.x()), static_cast<float>(v1.y())}, strokeWidth, stroke);
    renderer->drawThickLineLocal({static_cast<float>(v1.x()), static_cast<float>(v1.y())},
                                 {static_cast<float>(v2.x()), static_cast<float>(v2.y())}, strokeWidth, stroke);
   }
  }
 }
 return true;
}

QString dashPatternToString(const std::vector<float>& pattern) {
 if (pattern.empty()) return {};
 QStringList parts;
 for (float v : pattern) parts << QString::number(static_cast<double>(v), 'f', 1);
 return parts.join(QStringLiteral(","));
}

std::vector<float> stringToDashPattern(const QString& str) {
 if (str.trimmed().isEmpty()) return {};
 const auto parts = str.split(QStringLiteral(","), Qt::SkipEmptyParts);
 std::vector<float> result;
 result.reserve(std::min(parts.size(),
                         static_cast<qsizetype>(kMaxDashPatternEntries)));
 for (const auto& p : parts) {
  if (result.size() >= kMaxDashPatternEntries) break;
  bool ok = false;
  const double parsed = p.trimmed().toDouble(&ok);
  if (ok && std::isfinite(parsed) && parsed > 0.0) {
   result.push_back(static_cast<float>(std::min(
       parsed, static_cast<double>(kMaxShapeDimension))));
  }
 }
 return result;
}

FloatColor mixColor(const FloatColor& a, const FloatColor& b, const float t) {
 const float clampedT = std::clamp(t, 0.0f, 1.0f);
 return FloatColor(
     a.r() + (b.r() - a.r()) * clampedT,
     a.g() + (b.g() - a.g()) * clampedT,
     a.b() + (b.b() - a.b()) * clampedT,
     a.a() + (b.a() - a.a()) * clampedT);
}

QColor toQColor(const FloatColor& color) {
 const auto channel = [](const float value, const float fallback) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
 };
 return QColor::fromRgbF(
     channel(color.r(), 0.0f),
     channel(color.g(), 0.0f),
     channel(color.b(), 0.0f),
     channel(color.a(), 1.0f));
}

FloatColor normalizedShapeColor(const FloatColor& color) {
 const auto channel = [](const float value, const float fallback) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
 };
 return FloatColor(channel(color.r(), 0.0f), channel(color.g(), 0.0f),
                  channel(color.b(), 0.0f), channel(color.a(), 1.0f));
}

// Current composition timeline time used to evaluate animatable
// shape.* properties during playback/rendering.
ArtifactCore::RationalTime effectiveShapeTimelineTime(const ArtifactShapeLayer* layer) {
 int64_t frame = 0;
 int64_t fps = 30;
 if (layer) {
  if (auto* composition = dynamic_cast<Artifact::ArtifactAbstractComposition*>(
          layer->compositionObject())) {
   frame = composition->framePosition().framePosition();
   fps = std::max<int64_t>(
       1, static_cast<int64_t>(std::llround(composition->frameRate().framerate())));
  } else {
   frame = layer->currentFrame();
  }
 }
 return ArtifactCore::RationalTime(frame, fps);
}

// Evaluate an animatable shape.* numeric property at the current time.
// Falls back to the stored member value when the property has no keyframes.
double animatedShapeNumber(const ArtifactShapeLayer* layer,
                           const char* propertyPath,
                           double fallback) {
 if (!layer) {
  return fallback;
 }
 const auto property = layer->getProperty(QString::fromLatin1(propertyPath));
 if (!property || property->getKeyFrames().empty()) {
  return fallback;
 }
 const QVariant value =
     property->interpolateValue(effectiveShapeTimelineTime(layer));
 return value.isValid() ? value.toDouble() : fallback;
}

// Effective primitive dimensions after keyframe evaluation. All render
// pipelines (GPU / software / SVG / bounds / hit points) resolve geometry
// through this so keyframed shape.* properties stay consistent.
struct ShapeGeomDims {
 int width;
 int height;
 float cornerRadius;
 int starPoints;
 float starInnerRadius;
 int polygonSides;
};

inline bool sameShapeGeomDims(const ShapeGeomDims& a, const ShapeGeomDims& b) {
 return a.width == b.width && a.height == b.height &&
        a.cornerRadius == b.cornerRadius && a.starPoints == b.starPoints &&
        a.starInnerRadius == b.starInnerRadius &&
        a.polygonSides == b.polygonSides;
}

bool hasAnimatedShapeGeometry(const ArtifactShapeLayer* layer) {
 static constexpr const char* kPaths[] = {
     "shape.width", "shape.height", "shape.cornerRadius",
     "shape.starPoints", "shape.starInnerRadius", "shape.polygonSides"};
 if (!layer) {
  return false;
 }
 for (const char* path : kPaths) {
  const auto property = layer->getProperty(QString::fromLatin1(path));
  if (property && !property->getKeyFrames().empty()) {
   return true;
  }
 }
 return false;
}

ShapeGeomDims resolveShapeGeomDims(const ArtifactShapeLayer* layer,
                                   const int width,
                                   const int height,
                                   const float cornerRadius,
                                   const int starPoints,
                                   const float starInnerRadius,
                                   const int polygonSides) {
 return ShapeGeomDims{
     static_cast<int>(std::clamp(
         animatedShapeNumber(layer, "shape.width", width),
         1.0, static_cast<double>(kMaxShapeDimension))),
     static_cast<int>(std::clamp(
         animatedShapeNumber(layer, "shape.height", height),
         1.0, static_cast<double>(kMaxShapeDimension))),
     static_cast<float>(
         animatedShapeNumber(layer, "shape.cornerRadius", cornerRadius)),
     static_cast<int>(std::clamp(
         animatedShapeNumber(layer, "shape.starPoints", starPoints),
         3.0, static_cast<double>(kMaxStarPoints))),
     static_cast<float>(std::clamp(
         animatedShapeNumber(layer, "shape.starInnerRadius", starInnerRadius),
         0.0, 1.0)),
     static_cast<int>(std::clamp(
         animatedShapeNumber(layer, "shape.polygonSides", polygonSides),
         3.0, 100.0)),
 };
}

bool isSupportedShapePoint(const QPointF& point) {
 return std::isfinite(point.x()) && std::isfinite(point.y()) &&
        std::abs(point.x()) <= kMaxShapeCoordinate &&
        std::abs(point.y()) <= kMaxShapeCoordinate;
}

bool isSupportedCustomPathVertex(const Artifact::CustomPathVertex& vertex) {
 return isSupportedShapePoint(vertex.pos) &&
        isSupportedShapePoint(vertex.inTangent) &&
        isSupportedShapePoint(vertex.outTangent);
}

ShapePath buildLayerShapePath(
    const Artifact::ShapeType shapeType,
    const int width,
    const int height,
    const float cornerRadius,
    const int starPoints,
    const float starInnerRadius,
    const int polygonSides,
    const std::vector<QPointF>& customPolygonPoints,
    const bool customPolygonClosed,
    const std::vector<Artifact::CustomPathVertex>& customPathVertices,
    const bool customPathClosed) {
 if (customPathVertices.size() >= 3) {
  return buildCustomShapePath(customPathVertices, customPathClosed);
 }

 if (customPolygonPoints.size() >= 3) {
  ShapePath sp;
  sp.setPolygon(customPolygonPoints, customPolygonClosed);
  return sp;
 }

 return buildShapePath(shapeType, width, height, cornerRadius,
                       starPoints, starInnerRadius, polygonSides);
}

QPainterPath buildLayerPath(const Artifact::ShapeType shapeType,
                            const int width,
                            const int height,
                            const float cornerRadius,
                            const int starPoints,
                            const float starInnerRadius,
                            const int polygonSides,
                            const std::vector<QPointF>& customPolygonPoints,
                            const bool customPolygonClosed,
                            const std::vector<Artifact::CustomPathVertex>& customPathVertices,
                            const bool customPathClosed) {
 return buildLayerShapePath(
            shapeType, width, height, cornerRadius, starPoints,
            starInnerRadius, polygonSides, customPolygonPoints,
            customPolygonClosed, customPathVertices, customPathClosed)
     .toPainterPath();
}

std::unique_ptr<ArtifactCore::ShapeOperator> createShapeOperator(ArtifactCore::ShapeOperatorType type) {
  switch (type) {
  case ArtifactCore::ShapeOperatorType::TrimPaths:
    return std::make_unique<ArtifactCore::TrimPaths>();
  case ArtifactCore::ShapeOperatorType::Repeater:
    return std::make_unique<ArtifactCore::Repeater>();
  case ArtifactCore::ShapeOperatorType::MergePaths:
    return std::make_unique<ArtifactCore::MergePaths>();
  case ArtifactCore::ShapeOperatorType::OffsetPaths:
    return std::make_unique<ArtifactCore::OffsetPaths>();
  case ArtifactCore::ShapeOperatorType::PuckerBloat:
    return std::make_unique<ArtifactCore::PuckerBloat>();
  case ArtifactCore::ShapeOperatorType::RoundedCorners:
    return std::make_unique<ArtifactCore::RoundedCorners>();
  case ArtifactCore::ShapeOperatorType::WigglePaths:
    return std::make_unique<ArtifactCore::WigglePaths>();
  case ArtifactCore::ShapeOperatorType::ZigZag:
    return std::make_unique<ArtifactCore::ZigZag>();
  case ArtifactCore::ShapeOperatorType::Twist:
    return std::make_unique<ArtifactCore::Twist>();
  case ArtifactCore::ShapeOperatorType::HandDrawnWobble:
    return std::make_unique<ArtifactCore::HandDrawnWobble>();
  default:
    return nullptr;
  }
}

void normalizeRestoredShapeOperator(ArtifactCore::ShapeOperator *op) {
  if (auto trim = dynamic_cast<ArtifactCore::TrimPaths *>(op)) {
    const auto safe = [](const float value, const float fallback) {
      return std::isfinite(value) ? std::clamp(value, -100000.0f, 100000.0f) : fallback;
    };
    trim->setStart(safe(trim->start(), 0.0f));
    trim->setEnd(safe(trim->end(), 100.0f));
    trim->setOffset(safe(trim->offset(), 0.0f));
  } else if (auto repeater = dynamic_cast<ArtifactCore::Repeater *>(op)) {
    repeater->setCopies(std::clamp(repeater->copies(), 1, 1000));
    const auto safePoint = [](const QPointF &point, const QPointF &fallback) {
      return std::isfinite(point.x()) && std::isfinite(point.y())
          ? QPointF(std::clamp(point.x(), -1000000.0, 1000000.0),
                    std::clamp(point.y(), -1000000.0, 1000000.0))
          : fallback;
    };
    repeater->setAnchorPoint(safePoint(repeater->anchorPoint(), {}));
    repeater->setPosition(safePoint(repeater->position(), {}));
    repeater->setScale(safePoint(repeater->scale(), {1.0, 1.0}));
    const float offset = repeater->offset();
    repeater->setOffset(std::isfinite(offset)
        ? std::clamp(offset, -100000.0f, 100000.0f) : 0.0f);
    const float rotation = repeater->rotation();
    repeater->setRotation(std::isfinite(rotation)
        ? std::clamp(rotation, -360000.0f, 360000.0f) : 0.0f);
    const auto safeOpacity = [](const float value) {
      return std::isfinite(value) ? std::clamp(value, 0.0f, 100.0f) : 100.0f;
    };
    repeater->setStartOpacity(safeOpacity(repeater->startOpacity()));
    repeater->setEndOpacity(safeOpacity(repeater->endOpacity()));
  } else if (auto wiggle = dynamic_cast<ArtifactCore::WigglePaths *>(op)) {
    wiggle->setAmount(std::isfinite(wiggle->amount())
        ? std::clamp(wiggle->amount(), -100000.0f, 100000.0f) : 0.0f);
    wiggle->setFrequency(std::isfinite(wiggle->frequency())
        ? std::clamp(wiggle->frequency(), 0.0f, 10000.0f) : 1.0f);
  } else if (auto zigzag = dynamic_cast<ArtifactCore::ZigZag *>(op)) {
    zigzag->setAmount(std::isfinite(zigzag->amount())
        ? std::clamp(zigzag->amount(), -100000.0f, 100000.0f) : 0.0f);
    zigzag->setFrequency(std::isfinite(zigzag->frequency())
        ? std::clamp(zigzag->frequency(), 0.0f, 10000.0f) : 1.0f);
  } else if (auto wobble = dynamic_cast<ArtifactCore::HandDrawnWobble *>(op)) {
    wobble->setWobbleAmount(std::isfinite(wobble->wobbleAmount())
        ? std::clamp(wobble->wobbleAmount(), 0.0f, 100000.0f) : 0.0f);
    wobble->setWobbleFrequency(std::isfinite(wobble->wobbleFrequency())
        ? std::clamp(wobble->wobbleFrequency(), 0.0f, 10000.0f) : 1.0f);
    wobble->setPressureJitter(std::isfinite(wobble->pressureJitter())
        ? std::clamp(wobble->pressureJitter(), 0.0f, 1.0f) : 0.0f);
    wobble->setGapProbability(std::isfinite(wobble->gapProbability())
        ? std::clamp(wobble->gapProbability(), 0.0f, 1.0f) : 0.0f);
  } else if (auto offset = dynamic_cast<ArtifactCore::OffsetPaths *>(op)) {
    const float value = offset->offset();
    offset->setOffset(std::isfinite(value)
        ? std::clamp(value, -100000.0f, 100000.0f) : 0.0f);
  } else if (auto pucker = dynamic_cast<ArtifactCore::PuckerBloat *>(op)) {
    const float value = pucker->amount();
    pucker->setAmount(std::isfinite(value)
        ? std::clamp(value, -100000.0f, 100000.0f) : 0.0f);
  } else if (auto rounded = dynamic_cast<ArtifactCore::RoundedCorners *>(op)) {
    const float value = rounded->radius();
    rounded->setRadius(std::isfinite(value)
        ? std::clamp(value, 0.0f, 100000.0f) : 0.0f);
  }
}

static std::vector<ArtifactCore::ShapePath> applyShapeOperators(
    const ArtifactCore::ShapePath& basePath,
    const std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>& operators)
{
 if (operators.empty()) {
  return {basePath};
 }

 ArtifactCore::ShapeGroup group;
 auto pathShape = std::make_unique<ArtifactCore::PathShape>(basePath);
 group.addChild(std::move(pathShape));
 for (const auto& op : operators) {
  group.addOperator(op->clone());
 }
 return group.processedPaths();
}

static std::vector<QPainterPath> buildProcessedPainterPaths(
    Artifact::ShapeType shapeType,
    int width,
    int height,
    float cornerRadius,
    int starPoints,
    float starInnerRadius,
    int polygonSides,
    const std::vector<QPointF>& customPolygonPoints,
    bool customPolygonClosed,
    const std::vector<Artifact::CustomPathVertex>& customPathVertices,
    bool customPathClosed,
    const std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>& operators)
{
 const ArtifactCore::ShapePath shapePath = buildLayerShapePath(
     shapeType, width, height, cornerRadius, starPoints, starInnerRadius,
     polygonSides, customPolygonPoints, customPolygonClosed,
     customPathVertices, customPathClosed);
 const auto processed = applyShapeOperators(shapePath, operators);
 std::vector<QPainterPath> painterPaths;
 painterPaths.reserve(processed.size());
 for (const auto& path : processed) {
  painterPaths.push_back(path.toPainterPath());
 }
 return painterPaths;
}

static std::vector<ArtifactCore::ShapePath> buildProcessedShapePaths(
    Artifact::ShapeType shapeType,
    int width,
    int height,
    float cornerRadius,
    int starPoints,
    float starInnerRadius,
    int polygonSides,
    const std::vector<QPointF>& customPolygonPoints,
    bool customPolygonClosed,
    const std::vector<Artifact::CustomPathVertex>& customPathVertices,
    bool customPathClosed,
    const std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>& operators)
{
 const ArtifactCore::ShapePath basePath = buildLayerShapePath(
     shapeType, width, height, cornerRadius, starPoints, starInnerRadius,
     polygonSides, customPolygonPoints, customPolygonClosed,
     customPathVertices, customPathClosed);
 return applyShapeOperators(basePath, operators);
}

std::vector<QPointF> polygonToPoints(const QPolygonF& polygon) {
 std::vector<QPointF> points;
 points.reserve(static_cast<size_t>(polygon.size()));
 for (const auto& point : polygon) {
  points.push_back(point);
 }
 if (points.size() >= 2 && points.front() == points.back()) {
  points.pop_back();
 }
 return points;
}

void drawStrokeSegment(QPainter& painter,
                       const QPointF& p0,
                       const QPointF& p1,
                       const float width0,
                       const float width1,
                       const FloatColor& color0,
                       const FloatColor& color1) {
 const QLineF line(p0, p1);
 const qreal length = line.length();
 if (length <= 1e-5) {
  return;
 }

 const QPointF direction = line.unitVector().p2() - line.p1();
 const QPointF normal(-direction.y(), direction.x());
 const QPointF n0 = normal * (static_cast<qreal>(width0) * 0.5);
 const QPointF n1 = normal * (static_cast<qreal>(width1) * 0.5);

 QPolygonF quad;
 quad << QPointF(p0.x() + n0.x(), p0.y() + n0.y())
      << QPointF(p0.x() - n0.x(), p0.y() - n0.y())
      << QPointF(p1.x() - n1.x(), p1.y() - n1.y())
      << QPointF(p1.x() + n1.x(), p1.y() + n1.y());

 QLinearGradient gradient(p0, p1);
 gradient.setColorAt(0.0, toQColor(color0));
 gradient.setColorAt(1.0, toQColor(color1));
 painter.setPen(Qt::NoPen);
 painter.setBrush(QBrush(gradient));
 painter.drawPolygon(quad);
}

void drawStrokePath(QPainter& painter,
                    const std::vector<QPointF>& points,
                    const bool closed,
                    const float strokeWidth,
                    const float taperStart,
                    const float taperEnd,
                    const bool gradientEnabled,
                    const FloatColor& baseStrokeColor,
                    const FloatColor& gradientStartColor,
                    const FloatColor& gradientEndColor,
                    const Artifact::StrokeCap strokeCap) {
 if (points.size() < 2 || strokeWidth <= 0.0f) {
  return;
 }

 std::vector<QPointF> polyline = points;
 if (polyline.size() >= 2 && polyline.front() == polyline.back()) {
  polyline.pop_back();
 }
 if (polyline.size() < 2) {
  return;
 }

 const size_t segmentCount = closed ? polyline.size() : (polyline.size() - 1);
 if (segmentCount == 0) {
  return;
 }

 std::vector<qreal> cumulative;
 cumulative.reserve(segmentCount + 1);
 cumulative.push_back(0.0);
 qreal totalLength = 0.0;
 for (size_t i = 0; i < segmentCount; ++i) {
  const size_t next = (i + 1) % polyline.size();
  const qreal segLength = QLineF(polyline[i], polyline[next]).length();
  totalLength += segLength;
  cumulative.push_back(totalLength);
 }

 if (totalLength <= 1e-5) {
  return;
 }

 auto widthAt = [&](const qreal t) -> float {
  const float clampedT = std::clamp(static_cast<float>(t), 0.0f, 1.0f);
  const float scale = taperStart + (taperEnd - taperStart) * clampedT;
  return std::max(0.0f, strokeWidth * scale);
 };
 auto colorAt = [&](const qreal t) -> FloatColor {
  if (!gradientEnabled) {
   return baseStrokeColor;
  }
  return mixColor(gradientStartColor, gradientEndColor,
                  static_cast<float>(std::clamp(t, 0.0, 1.0)));
 };

 for (size_t i = 0; i < segmentCount; ++i) {
  const size_t next = (i + 1) % polyline.size();
  const QPointF p0 = polyline[i];
  const QPointF p1 = polyline[next];
  const qreal segLength = cumulative[i + 1] - cumulative[i];
  if (segLength <= 1e-5) {
   continue;
  }

  const qreal t0 = cumulative[i] / totalLength;
  const qreal t1 = cumulative[i + 1] / totalLength;
  const float w0 = widthAt(t0);
  const float w1 = widthAt(t1);
  if (w0 <= 0.0f && w1 <= 0.0f) {
   continue;
  }

  QPointF drawP0 = p0;
  QPointF drawP1 = p1;
  if (!closed && strokeCap == Artifact::StrokeCap::Square) {
   const QLineF line(p0, p1);
   const QPointF direction = line.unitVector().p2() - line.p1();
   if (i == 0) {
    drawP0 -= direction * (static_cast<qreal>(w0) * 0.5);
   }
   if (i + 1 == segmentCount) {
    drawP1 += direction * (static_cast<qreal>(w1) * 0.5);
   }
  }

  drawStrokeSegment(painter, drawP0, drawP1, std::max(w0, 0.0f), std::max(w1, 0.0f),
                    colorAt(t0), colorAt(t1));
 }

 if (!closed && strokeCap == Artifact::StrokeCap::Round) {
  const float startWidth = widthAt(0.0);
  const float endWidth = widthAt(1.0);
  if (startWidth > 0.0f) {
   painter.setPen(Qt::NoPen);
   painter.setBrush(toQColor(colorAt(0.0)));
   painter.drawEllipse(polyline.front(), startWidth * 0.5, startWidth * 0.5);
  }
  if (endWidth > 0.0f) {
   painter.setPen(Qt::NoPen);
   painter.setBrush(toQColor(colorAt(1.0)));
   painter.drawEllipse(polyline.back(), endWidth * 0.5, endWidth * 0.5);
  }
 }
}

void appendArcPoints(std::vector<QPointF>& points,
                     const QPointF& center,
                     float radiusX,
                     float radiusY,
                     float startAngleDeg,
                     float endAngleDeg,
                     int segments) {
 const float startRad = startAngleDeg * static_cast<float>(M_PI) / 180.0f;
 const float endRad = endAngleDeg * static_cast<float>(M_PI) / 180.0f;
 const float step = (endRad - startRad) / static_cast<float>(std::max(1, segments));
 for (int i = 0; i <= segments; ++i) {
  const float angle = startRad + step * static_cast<float>(i);
  points.push_back(QPointF(center.x() + std::cos(angle) * radiusX,
                           center.y() + std::sin(angle) * radiusY));
 }
}

std::vector<QPointF> buildRoundedRectPoints(float x, float y, float w, float h, float radius) {
 const float r = std::clamp(radius, 0.0f, std::min(w, h) * 0.5f);
 if (r <= 0.0f) {
  return {QPointF(x, y), QPointF(x + w, y), QPointF(x + w, y + h), QPointF(x, y + h)};
 }

 std::vector<QPointF> points;
 points.reserve(36);
 const int cornerSegments = 6;
 appendArcPoints(points, QPointF(x + w - r, y + r), r, r, -90.0f, 0.0f, cornerSegments);
 appendArcPoints(points, QPointF(x + w - r, y + h - r), r, r, 0.0f, 90.0f, cornerSegments);
 appendArcPoints(points, QPointF(x + r, y + h - r), r, r, 90.0f, 180.0f, cornerSegments);
 appendArcPoints(points, QPointF(x + r, y + r), r, r, 180.0f, 270.0f, cornerSegments);
 return points;
}

std::vector<QPointF> buildRenderablePoints(Artifact::ShapeType shapeType,
                                           int width, int height,
                                           float cornerRadius, int starPoints,
                                           float starInnerRadius,
                                           int polygonSides,
                                           const std::vector<QPointF>& customPolygonPoints,
                                           [[maybe_unused]] bool customPolygonClosed) {
 const float w = static_cast<float>(width);
 const float h = static_cast<float>(height);
 const float cx = w * 0.5f;
 const float cy = h * 0.5f;

 if (shapeType == Artifact::ShapeType::Polygon &&
     customPolygonPoints.size() >= 3) {
  return customPolygonPoints;
 }

 switch (shapeType) {
 case Artifact::ShapeType::Rect:
  return buildRoundedRectPoints(0.0f, 0.0f, w, h, cornerRadius);
 case Artifact::ShapeType::Square: {
  const float side = std::min(w, h);
  const float left = (w - side) * 0.5f;
  const float top = (h - side) * 0.5f;
  return buildRoundedRectPoints(left, top, side, side, cornerRadius);
 }
 case Artifact::ShapeType::Triangle:
  return {QPointF(cx, 0.0f), QPointF(w, h), QPointF(0.0f, h)};
 case Artifact::ShapeType::Line:
  return {QPointF(0.0f, cy), QPointF(w, cy)};
 case Artifact::ShapeType::Ellipse: {
  const int segments = 48;
  std::vector<QPointF> points;
  points.reserve(segments);
  for (int i = 0; i < segments; ++i) {
   const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) /
                       static_cast<float>(segments);
   points.push_back(QPointF(cx + std::cos(angle) * cx,
                            cy + std::sin(angle) * cy));
  }
  return points;
 }
 case Artifact::ShapeType::Star: {
  const int pts = std::max(3, starPoints);
  const float outerR = std::min(cx, cy);
  const float innerR = outerR * std::clamp(starInnerRadius, 0.0f, 1.0f);
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
 case Artifact::ShapeType::Polygon: {
  const int sides = std::max(3, polygonSides);
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
 }
 return {};
}

static ArtifactCore::ShapePath buildShapePath(Artifact::ShapeType shapeType,
                                int width,
                                int height,
                                float cornerRadius,
                                int starPoints,
                                float starInnerRadius,
                                int polygonSides) {
 ArtifactCore::ShapePath path;
 const float w = static_cast<float>(width);
 const float h = static_cast<float>(height);
 const float cx = w * 0.5f;
 const float cy = h * 0.5f;

 switch (shapeType) {
   case Artifact::ShapeType::Rect:
    if (cornerRadius > 0) {
     path.setRoundedRect(QRectF(0, 0, w, h), cornerRadius, cornerRadius);
    } else {
     path.setRectangle(QRectF(0, 0, w, h));
    }
   break;

  case Artifact::ShapeType::Ellipse:
   path.setEllipse(QRectF(0, 0, w, h));
   break;

  case Artifact::ShapeType::Star: {
    const int pts = starPoints;
    const float outerR = std::min(cx, cy);
    const float innerR = outerR * starInnerRadius;
   std::vector<QPointF> ptsList;
   ptsList.reserve(pts * 2);
   for (int i = 0; i < pts * 2; ++i) {
    float angle = static_cast<float>(i) * M_PI / pts - M_PI * 0.5f;
    float r = (i % 2 == 0) ? outerR : innerR;
    float x = cx + r * std::cos(angle);
    float y = cy + r * std::sin(angle);
    ptsList.push_back(QPointF(x, y));
   }
   path.setPolygon(ptsList, true);
   break;
  }

  case Artifact::ShapeType::Polygon: {
    const int sides = polygonSides;
   const float r = std::min(cx, cy);
   std::vector<QPointF> ptsList;
   ptsList.reserve(sides);
   for (int i = 0; i < sides; ++i) {
    float angle = static_cast<float>(i) * 2.0f * M_PI / sides - M_PI * 0.5f;
    float x = cx + r * std::cos(angle);
    float y = cy + r * std::sin(angle);
    ptsList.push_back(QPointF(x, y));
   }
   path.setPolygon(ptsList, true);
   break;
  }

  case Artifact::ShapeType::Line:
   path.moveTo(0, cy);
   path.lineTo(w, cy);
   break;

  case Artifact::ShapeType::Triangle: {
   std::vector<QPointF> ptsList;
   ptsList.reserve(3);
   ptsList.push_back(QPointF(cx, 0.0f));
   ptsList.push_back(QPointF(w, h));
   ptsList.push_back(QPointF(0.0f, h));
   path.setPolygon(ptsList, true);
   break;
  }

  case Artifact::ShapeType::Square: {
   const float side = std::min(w, h);
   const float left = (w - side) * 0.5f;
   const float top = (h - side) * 0.5f;
   path.setRectangle(QRectF(left, top, side, side));
   break;
  }
 }

 return path;
}

QString shapeTypeName(int type) {
 switch (type) {
  case 0: return QStringLiteral("Rect");
  case 1: return QStringLiteral("Ellipse");
  case 2: return QStringLiteral("Star");
  case 3: return QStringLiteral("Polygon");
  case 4: return QStringLiteral("Line");
  case 5: return QStringLiteral("Triangle");
  case 6: return QStringLiteral("Square");
 }
 return QStringLiteral("Rect");
}

QString operatorName(ArtifactCore::ShapeOperatorType type) {
  switch (type) {
  case ArtifactCore::ShapeOperatorType::TrimPaths: return QStringLiteral("Trim Paths");
  case ArtifactCore::ShapeOperatorType::Repeater: return QStringLiteral("Repeater");
  case ArtifactCore::ShapeOperatorType::MergePaths: return QStringLiteral("Merge Paths");
  case ArtifactCore::ShapeOperatorType::OffsetPaths: return QStringLiteral("Offset Paths");
  case ArtifactCore::ShapeOperatorType::PuckerBloat: return QStringLiteral("Pucker & Bloat");
  case ArtifactCore::ShapeOperatorType::RoundedCorners: return QStringLiteral("Rounded Corners");
  case ArtifactCore::ShapeOperatorType::WigglePaths: return QStringLiteral("Wiggle Paths");
  case ArtifactCore::ShapeOperatorType::ZigZag: return QStringLiteral("Zig Zag");
  case ArtifactCore::ShapeOperatorType::Twist: return QStringLiteral("Twist");
  case ArtifactCore::ShapeOperatorType::HandDrawnWobble: return QStringLiteral("Hand Drawn Wobble");
  default: return QStringLiteral("Unknown Operator");
  }
}

} // namespace

namespace Artifact
{

class ArtifactShapeLayer::Impl {
public:
 Artifact::ShapeType shapeType_ = Artifact::ShapeType::Rect;
 int width_ = 200;
 int height_ = 200;
  FloatColor fillColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  ArtifactSolidFillType fillType_ = ArtifactSolidFillType::Solid;
  FloatColor fillGradientStartColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  FloatColor fillGradientEndColor_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  float fillGradientAngleDegrees_ = 0.0f;
  float fillGradientCenterX_ = 0.5f;
  float fillGradientCenterY_ = 0.5f;
  float fillGradientRadius_ = 0.5f;
  FloatColor strokeColor_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
 float strokeWidth_ = 0.0f;
 bool fillEnabled_ = true;
 bool strokeEnabled_ = false;
 float strokeTaperStart_ = 1.0f;
 float strokeTaperEnd_ = 1.0f;
 bool strokeGradientEnabled_ = false;
 FloatColor strokeGradientStartColor_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
 FloatColor strokeGradientEndColor_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);

 // Rect
 float cornerRadius_ = 0.0f;

 // Star
 int starPoints_ = 5;
 float starInnerRadius_ = 0.382f;

 // Polygon
 int polygonSides_ = 6;
 std::vector<QPointF> customPolygonPoints_;
 bool customPolygonClosed_ = true;

 // Phase 3: Stroke styles
 StrokeCap strokeCap_ = StrokeCap::Flat;
 StrokeJoin strokeJoin_ = StrokeJoin::Miter;
 StrokeAlign strokeAlign_ = StrokeAlign::Center;
 std::vector<float> dashPattern_;

 bool hasCustomStrokeEffects() const {
  return std::abs(strokeTaperStart_ - 1.0f) > kStrokeEffectEpsilon ||
         std::abs(strokeTaperEnd_ - 1.0f) > kStrokeEffectEpsilon ||
         strokeGradientEnabled_;
 }

 // Phase 5: Bezier path override
 std::vector<CustomPathVertex> customPathVertices_;
 bool customPathClosed_ = true;
 ArtifactCore::PathFillRule customPathFillRule_ = ArtifactCore::PathFillRule::Winding;
 std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>> shapeOperators_;
 ShapeCompatibilityFallback lastLoggedFallback_ = ShapeCompatibilityFallback::None;

  ShapeCompatibilityFallback compatibilityFallback() const {
   if (fillEnabled_ && fillType_ != ArtifactSolidFillType::Solid) {
    return ShapeCompatibilityFallback::GradientFill;
   }
   if (strokeEnabled_ && strokeWidth_ > 0.0f &&
       strokeAlign_ != StrokeAlign::Center) {
    return ShapeCompatibilityFallback::StrokeAlignment;
   }
   if (strokeEnabled_ && strokeWidth_ > 0.0f && hasCustomStrokeEffects()) {
    return ShapeCompatibilityFallback::CustomStrokeEffect;
   }
   if (!shapeOperators_.empty()) {
    return ShapeCompatibilityFallback::ShapeOperator;
   }
   return ShapeCompatibilityFallback::None;
  }

  bool useCachePipeline() const {
   return compatibilityFallback() != ShapeCompatibilityFallback::None;
  }

    QImage cachedImage_;
    bool cacheDirty_ = true;

    mutable QRectF cachedLocalBounds_;
    bool localBoundsCacheDirty_ = true;

    mutable std::vector<QPointF> cachedShapePoints_;
    bool shapeContentCacheDirty_ = true;

    struct NativePathGeometry {
      std::vector<ArtifactCore::PathTriangle> triangles;
      std::vector<std::vector<ArtifactCore::BezierSegment>> subpaths;
    };
    std::vector<NativePathGeometry> cachedNativeGeometry_;
    double cachedNativeTolerance_ = -1.0;
    bool nativeGeometryCacheDirty_ = true;
    NativePathGeometry cachedShapeGeometry_;
    ShapeGeomDims cachedShapeGeometryDims_{0, 0, 0.0f, 3, 0.0f, 3};
    bool cachedShapeGeometryDimsValid_ = false;
    double cachedShapeTolerance_ = -1.0;
    bool shapeGeometryCacheDirty_ = true;

   Impl() = default;
 ~Impl() = default;
   void addShape() {}
   void markDirty() {
    cacheDirty_ = true;
    shapeContentCacheDirty_ = true;
    nativeGeometryCacheDirty_ = true;
    shapeGeometryCacheDirty_ = true;
   }

   const std::vector<NativePathGeometry>& nativeGeometry(
       const std::vector<ArtifactCore::ShapePath>& paths, double tolerance,
       const bool cacheable = true) {
    if (!(cacheable && !nativeGeometryCacheDirty_ &&
          std::abs(cachedNativeTolerance_ - tolerance) < 1.0e-9)) {
     cachedNativeGeometry_.clear();
     cachedNativeGeometry_.reserve(paths.size());
     for (const auto& path : paths) {
      NativePathGeometry geometry;
      geometry.triangles = path.triangulate(tolerance);
      geometry.subpaths = path.flattenSubpaths(tolerance);
      cachedNativeGeometry_.push_back(std::move(geometry));
     }
     cachedNativeTolerance_ = tolerance;
     nativeGeometryCacheDirty_ = false;
    }
    return cachedNativeGeometry_;
   }

   const NativePathGeometry& shapeGeometry(const ShapeGeomDims& dims,
                                           double tolerance) {
    if (!shapeGeometryCacheDirty_ && cachedShapeGeometryDimsValid_ &&
        sameShapeGeomDims(cachedShapeGeometryDims_, dims) &&
        std::abs(cachedShapeTolerance_ - tolerance) < 1.0e-9) {
     return cachedShapeGeometry_;
    }
    ShapePath path = buildLayerShapePath(
        shapeType_, dims.width, dims.height, dims.cornerRadius,
        dims.starPoints, dims.starInnerRadius, dims.polygonSides,
        customPolygonPoints_, customPolygonClosed_,
        customPathVertices_, customPathClosed_);
    if (customPathVertices_.size() >= 3) {
     path.setFillRule(customPathFillRule_);
    }
    cachedShapeGeometry_.triangles = path.triangulate(tolerance);
    cachedShapeGeometry_.subpaths = path.flattenSubpaths(tolerance);
    cachedShapeGeometryDims_ = dims;
    cachedShapeGeometryDimsValid_ = true;
    cachedShapeTolerance_ = tolerance;
    shapeGeometryCacheDirty_ = false;
    return cachedShapeGeometry_;
   }
  void rebuildCache(const ShapeGeomDims* dims = nullptr) {
    if (!cacheDirty_ && !dims) return;
    const int effW = dims ? dims->width : width_;
    const int effH = dims ? dims->height : height_;
    const float effCornerRadius = dims ? dims->cornerRadius : cornerRadius_;
    const int effStarPoints = dims ? dims->starPoints : starPoints_;
    const float effStarInnerRadius =
        dims ? dims->starInnerRadius : starInnerRadius_;
    const int effPolygonSides = dims ? dims->polygonSides : polygonSides_;
    QImage img(effW, effH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto paths = buildProcessedPainterPaths(shapeType_, effW, effH,
                                                  effCornerRadius, effStarPoints,
                                                  effStarInnerRadius, effPolygonSides,
                                                  customPolygonPoints_, customPolygonClosed_,
                                                  customPathVertices_, customPathClosed_,
                                                  shapeOperators_);

    if (!paths.empty()) {
     const bool isGradient = fillType_ != ArtifactSolidFillType::Solid;
     QBrush fillBrush;
     if (isGradient) {
      const int w = effW;
      const int h = effH;
      const float cx = static_cast<float>(w) * fillGradientCenterX_;
      const float cy = static_cast<float>(h) * fillGradientCenterY_;
      const float radius = static_cast<float>(std::max(w, h)) * fillGradientRadius_;
      QGradient* grad = nullptr;
      if (fillType_ == ArtifactSolidFillType::LinearGradient ||
          fillType_ == ArtifactSolidFillType::RepeatingGradient ||
          fillType_ == ArtifactSolidFillType::MirroredGradient) {
       const float rad = fillGradientAngleDegrees_ * static_cast<float>(M_PI) / 180.0f;
       const float dx = std::cos(rad) * static_cast<float>(w) * 0.5f;
       const float dy = std::sin(rad) * static_cast<float>(h) * 0.5f;
       const QPointF start(static_cast<qreal>(cx - dx), static_cast<qreal>(cy - dy));
       const QPointF end(static_cast<qreal>(cx + dx), static_cast<qreal>(cy + dy));
       auto* linear = new QLinearGradient(start, end);
       if (fillType_ == ArtifactSolidFillType::RepeatingGradient) {
        linear->setSpread(QGradient::RepeatSpread);
       } else if (fillType_ == ArtifactSolidFillType::MirroredGradient) {
        linear->setSpread(QGradient::ReflectSpread);
       }
       grad = linear;
      } else if (fillType_ == ArtifactSolidFillType::RadialGradient) {
       auto* rg = new QRadialGradient(QPointF(cx, cy), radius);
       grad = rg;
      } else {
       auto* cg = new QConicalGradient(QPointF(cx, cy), fillGradientAngleDegrees_);
       grad = cg;
      }
      grad->setColorAt(0.0, toQColor(fillGradientStartColor_));
      grad->setColorAt(1.0, toQColor(fillGradientEndColor_));
      fillBrush = QBrush(*grad);
      delete grad;
     } else {
      fillBrush = QColor(static_cast<int>(fillColor_.r() * 255),
                         static_cast<int>(fillColor_.g() * 255),
                         static_cast<int>(fillColor_.b() * 255),
                         static_cast<int>(fillColor_.a() * 255));
     }
     const bool canUseCustomStroke =
         hasCustomStrokeEffects() &&
         strokeAlign_ == StrokeAlign::Center &&
         strokeJoin_ == StrokeJoin::Miter &&
         dashPattern_.empty();
      for (const QPainterPath& path : paths) {
       if (fillEnabled_) {
        painter.fillPath(path, fillBrush);
       }

      if (strokeEnabled_ && strokeWidth_ > 0) {
       if (canUseCustomStroke) {
        const auto subpaths = path.toSubpathPolygons();
        const FloatColor gradientStart = strokeGradientEnabled_ ? strokeGradientStartColor_ : strokeColor_;
        const FloatColor gradientEnd = strokeGradientEnabled_ ? strokeGradientEndColor_ : strokeColor_;
        bool pathClosed = shapeType_ != Artifact::ShapeType::Line;
        if (customPolygonPoints_.size() >= 3) {
         pathClosed = customPolygonClosed_;
        }
        if (customPathVertices_.size() >= 3) {
         pathClosed = customPathClosed_;
        }
        for (const QPolygonF& subpath : subpaths) {
         const std::vector<QPointF> points = polygonToPoints(subpath);
         drawStrokePath(painter, points, pathClosed, strokeWidth_,
                        strokeTaperStart_, strokeTaperEnd_,
                        strokeGradientEnabled_, strokeColor_,
                        gradientStart, gradientEnd, strokeCap_);
        }
       } else {
        QColor sc(static_cast<int>(strokeColor_.r() * 255),
                  static_cast<int>(strokeColor_.g() * 255),
                  static_cast<int>(strokeColor_.b() * 255),
                  static_cast<int>(strokeColor_.a() * 255));
        QPen pen(sc, strokeWidth_);
        switch (strokeCap_) {
         case StrokeCap::Round:  pen.setCapStyle(Qt::RoundCap);  break;
         case StrokeCap::Square: pen.setCapStyle(Qt::SquareCap); break;
         default:                pen.setCapStyle(Qt::FlatCap);   break;
        }
        switch (strokeJoin_) {
         case StrokeJoin::Round: pen.setJoinStyle(Qt::RoundJoin); break;
         case StrokeJoin::Bevel: pen.setJoinStyle(Qt::BevelJoin); break;
         default:                pen.setJoinStyle(Qt::MiterJoin); break;
        }
        if (!dashPattern_.empty()) {
         QVector<qreal> qDash;
         qDash.reserve(static_cast<int>(dashPattern_.size()));
         for (float v : dashPattern_) qDash.push_back(static_cast<qreal>(v));
         pen.setDashPattern(qDash);
        }
        if (strokeAlign_ == StrokeAlign::Inside) {
         painter.save();
         painter.setClipPath(path);
         QPen widePen = pen;
         widePen.setWidthF(static_cast<qreal>(strokeWidth_) * 2.0);
         painter.setPen(widePen);
         painter.drawPath(path);
         painter.restore();
        } else if (strokeAlign_ == StrokeAlign::Outside) {
         painter.save();
         QPainterPath outside;
         outside.addRect(QRectF(-1, -1, effW + 2, effH + 2));
         outside = outside.subtracted(path);
         painter.setClipPath(outside);
         QPen widePen = pen;
         widePen.setWidthF(static_cast<qreal>(strokeWidth_) * 2.0);
         painter.setPen(widePen);
         painter.drawPath(path);
         painter.restore();
        } else {
         painter.setPen(pen);
         painter.drawPath(path);
        }
       }
      }
     }
    }

   painter.end();
   cachedImage_ = std::move(img);
   cacheDirty_ = false;
  }
};

// ============================================================
// Constructor / Destructor
// ============================================================

ArtifactShapeLayer::ArtifactShapeLayer() : impl_(new Impl()) {}
ArtifactShapeLayer::~ArtifactShapeLayer() { delete impl_; }
void ArtifactShapeLayer::addShape()
{
  if (!impl_) {
    return;
  }
  // This layer is still a single-primitive shape layer, so "add" currently
  // means "materialize the current primitive definition and invalidate caches".
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
bool ArtifactShapeLayer::isShapeLayer() const { return true; }

// ============================================================
// Shape Type
// ============================================================

void ArtifactShapeLayer::setShapeType(Artifact::ShapeType type) {
  const int raw = static_cast<int>(type);
  if (raw < static_cast<int>(Artifact::ShapeType::Rect) || raw > static_cast<int>(Artifact::ShapeType::Square)) {
   impl_->shapeType_ = Artifact::ShapeType::Rect;
  } else {
   impl_->shapeType_ = type;
  }
  if (impl_->shapeType_ != Artifact::ShapeType::Polygon) {
   impl_->customPolygonPoints_.clear();
   impl_->customPolygonClosed_ = true;
  }
  // Selecting a primitive must leave custom path editing mode; otherwise the
  // renderer continues to prefer the old Bézier path over the new type.
  impl_->customPathVertices_.clear();
  impl_->customPathClosed_ = true;
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  if (hasRigidBodyPhysics()) {
   syncRigidBodyPhysicsToBounds();
  }
  if (hasSoftBodyPhysics()) {
   syncSoftBodyPhysicsColliderToBounds();
  }
  Q_EMIT changed();
}
Artifact::ShapeType ArtifactShapeLayer::shapeType() const { return impl_->shapeType_; }

// ============================================================
// Size
// ============================================================

void ArtifactShapeLayer::setSize(int w, int h) {
  // Keep the software cache and the GPU fallback on a valid image extent.
  // Property editors can temporarily submit zero/negative values while an
  // edit is being committed; allowing those through makes QImage construction
  // fail and leaves the previous cached shape visible.
  int clampedWidth = std::clamp(w, 1, kMaxShapeDimension);
  int clampedHeight = std::clamp(h, 1, kMaxShapeDimension);
  const auto pixelCount = static_cast<std::uint64_t>(clampedWidth) *
                          static_cast<std::uint64_t>(clampedHeight);
  if (pixelCount > kMaxShapeCachePixels) {
   const double scale = std::sqrt(
       static_cast<double>(kMaxShapeCachePixels) /
       static_cast<double>(pixelCount));
   clampedWidth = std::max(1, static_cast<int>(std::floor(clampedWidth * scale)));
   clampedHeight = std::max(1, static_cast<int>(std::floor(clampedHeight * scale)));
  }
  if (impl_->width_ == clampedWidth && impl_->height_ == clampedHeight) {
   return;
  }
  impl_->width_ = clampedWidth;
  impl_->height_ = clampedHeight;
  setSourceSize(Size_2D(clampedWidth, clampedHeight));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  if (hasRigidBodyPhysics()) {
   syncRigidBodyPhysicsToBounds();
  }
  if (hasSoftBodyPhysics()) {
   syncSoftBodyPhysicsColliderToBounds();
  }
  Q_EMIT changed();
}
int ArtifactShapeLayer::shapeWidth() const { return impl_->width_; }
int ArtifactShapeLayer::shapeHeight() const { return impl_->height_; }

// ============================================================
// Style
// ============================================================

void ArtifactShapeLayer::setFillColor(const FloatColor& c) { impl_->fillColor_ = normalizedShapeColor(c); impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillColor() const { return impl_->fillColor_; }
void ArtifactShapeLayer::setFillType(ArtifactSolidFillType t) {
 int raw = std::clamp(static_cast<int>(t),
                      static_cast<int>(ArtifactSolidFillType::Solid),
                      static_cast<int>(ArtifactSolidFillType::MirroredGradient));
 const auto normalized = static_cast<ArtifactSolidFillType>(raw);
 if (impl_->fillType_ == normalized) {
  return;
 }
 impl_->fillType_ = normalized;
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
ArtifactSolidFillType ArtifactShapeLayer::fillType() const { return impl_->fillType_; }
void ArtifactShapeLayer::setFillGradientStartColor(const FloatColor& c) { impl_->fillGradientStartColor_ = normalizedShapeColor(c); impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillGradientStartColor() const { return impl_->fillGradientStartColor_; }
void ArtifactShapeLayer::setFillGradientEndColor(const FloatColor& c) { impl_->fillGradientEndColor_ = normalizedShapeColor(c); impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillGradientEndColor() const { return impl_->fillGradientEndColor_; }
void ArtifactShapeLayer::setFillGradientAngleDegrees(float d) { impl_->fillGradientAngleDegrees_ = std::isfinite(d) ? std::clamp(d, -360.0f, 360.0f) : 0.0f; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::fillGradientAngleDegrees() const { return impl_->fillGradientAngleDegrees_; }
void ArtifactShapeLayer::setFillGradientCenterX(float v) { impl_->fillGradientCenterX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::fillGradientCenterX() const { return impl_->fillGradientCenterX_; }
void ArtifactShapeLayer::setFillGradientCenterY(float v) { impl_->fillGradientCenterY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::fillGradientCenterY() const { return impl_->fillGradientCenterY_; }
void ArtifactShapeLayer::setFillGradientRadius(float v) { impl_->fillGradientRadius_ = std::isfinite(v) ? std::clamp(v, 0.0f, 100000.0f) : 0.5f; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::fillGradientRadius() const { return impl_->fillGradientRadius_; }
void ArtifactShapeLayer::setStrokeColor(const FloatColor& c) {
 impl_->strokeColor_ = normalizedShapeColor(c);
 if (!impl_->strokeGradientEnabled_) {
  impl_->strokeGradientStartColor_ = impl_->strokeColor_;
  impl_->strokeGradientEndColor_ = impl_->strokeColor_;
 }
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
FloatColor ArtifactShapeLayer::strokeColor() const { return impl_->strokeColor_; }
void ArtifactShapeLayer::setStrokeWidth(float w) {
 impl_->strokeWidth_ = std::isfinite(w)
     ? std::clamp(w, 0.0f, static_cast<float>(kMaxShapeDimension)) : 0.0f;
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
float ArtifactShapeLayer::strokeWidth() const { return impl_->strokeWidth_; }
void ArtifactShapeLayer::setFillEnabled(bool e) { impl_->fillEnabled_ = e; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
bool ArtifactShapeLayer::fillEnabled() const { return impl_->fillEnabled_; }
void ArtifactShapeLayer::setStrokeEnabled(bool e) { impl_->strokeEnabled_ = e; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
bool ArtifactShapeLayer::strokeEnabled() const { return impl_->strokeEnabled_; }
void ArtifactShapeLayer::setStrokeTaper(float startScale, float endScale) {
 impl_->strokeTaperStart_ = std::isfinite(startScale) ? std::clamp(startScale, 0.0f, 1.0f) : 1.0f;
 impl_->strokeTaperEnd_ = std::isfinite(endScale) ? std::clamp(endScale, 0.0f, 1.0f) : 1.0f;
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
float ArtifactShapeLayer::strokeTaperStart() const { return impl_->strokeTaperStart_; }
float ArtifactShapeLayer::strokeTaperEnd() const { return impl_->strokeTaperEnd_; }
void ArtifactShapeLayer::setStrokeGradientEnabled(bool enabled) {
 impl_->strokeGradientEnabled_ = enabled;
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
bool ArtifactShapeLayer::strokeGradientEnabled() const { return impl_->strokeGradientEnabled_; }
void ArtifactShapeLayer::setStrokeGradientStartColor(const FloatColor& color) {
 impl_->strokeGradientStartColor_ = normalizedShapeColor(color);
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
FloatColor ArtifactShapeLayer::strokeGradientStartColor() const { return impl_->strokeGradientStartColor_; }
void ArtifactShapeLayer::setStrokeGradientEndColor(const FloatColor& color) {
 impl_->strokeGradientEndColor_ = normalizedShapeColor(color);
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
FloatColor ArtifactShapeLayer::strokeGradientEndColor() const { return impl_->strokeGradientEndColor_; }

std::vector<QPointF> ArtifactShapeLayer::direct3DCardFillPoints() const {
 if (impl_->useCachePipeline() || !impl_->fillEnabled_ ||
     impl_->fillType_ != ArtifactSolidFillType::Solid ||
     impl_->shapeType_ == ShapeType::Line ||
     (impl_->shapeType_ == ShapeType::Polygon &&
      impl_->customPolygonPoints_.size() >= 3 &&
      !impl_->customPolygonClosed_)) {
  return {};
 }
 if (impl_->shapeContentCacheDirty_) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  impl_->cachedShapePoints_ = buildRenderablePoints(
      impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
      dims.starPoints, dims.starInnerRadius, dims.polygonSides,
      impl_->customPolygonPoints_, impl_->customPolygonClosed_);
  impl_->shapeContentCacheDirty_ = false;
 }
 if (hasAnimatedShapeGeometry(this)) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  return buildRenderablePoints(
      impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
      dims.starPoints, dims.starInnerRadius, dims.polygonSides,
      impl_->customPolygonPoints_, impl_->customPolygonClosed_);
 }
 return impl_->cachedShapePoints_;
}

FloatColor ArtifactShapeLayer::direct3DCardFillColor() const {
 const float weight = compositionFieldContentWeight(this);
 return FloatColor{impl_->fillColor_.r(), impl_->fillColor_.g(),
                   impl_->fillColor_.b(), impl_->fillColor_.a() * weight};
}

std::vector<QPointF> ArtifactShapeLayer::direct3DCardStrokePoints() const {
 if (impl_->useCachePipeline() || !impl_->strokeEnabled_ ||
     impl_->strokeWidth_ <= 0.0f) {
  return {};
 }
 if (impl_->shapeContentCacheDirty_) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  impl_->cachedShapePoints_ = buildRenderablePoints(
      impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
      dims.starPoints, dims.starInnerRadius, dims.polygonSides,
      impl_->customPolygonPoints_, impl_->customPolygonClosed_);
  impl_->shapeContentCacheDirty_ = false;
 }
 if (hasAnimatedShapeGeometry(this)) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  return buildRenderablePoints(
      impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
      dims.starPoints, dims.starInnerRadius, dims.polygonSides,
      impl_->customPolygonPoints_, impl_->customPolygonClosed_);
 }
 return impl_->cachedShapePoints_;
}

FloatColor ArtifactShapeLayer::direct3DCardStrokeColor() const {
 const float weight = compositionFieldContentWeight(this);
 return FloatColor{impl_->strokeColor_.r(), impl_->strokeColor_.g(),
                   impl_->strokeColor_.b(), impl_->strokeColor_.a() * weight};
}

bool ArtifactShapeLayer::direct3DCardStrokeClosed() const {
 return impl_->shapeType_ != ShapeType::Line &&
        !(impl_->shapeType_ == ShapeType::Polygon &&
          impl_->customPolygonPoints_.size() >= 3 &&
          !impl_->customPolygonClosed_);
}

// ============================================================
// Shape Params
// ============================================================

void ArtifactShapeLayer::setCornerRadius(float r) { impl_->cornerRadius_ = std::isfinite(r) ? std::clamp(r, 0.0f, static_cast<float>(kMaxShapeDimension)) : 0.0f; impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::cornerRadius() const { return impl_->cornerRadius_; }
void ArtifactShapeLayer::setStarPoints(int p) { impl_->starPoints_ = std::clamp(p, 3, kMaxStarPoints); impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
int ArtifactShapeLayer::starPoints() const { return impl_->starPoints_; }
void ArtifactShapeLayer::setStarInnerRadius(float r) { impl_->starInnerRadius_ = std::isfinite(r) ? std::clamp(r, 0.0f, 1.0f) : 0.5f; impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::starInnerRadius() const { return impl_->starInnerRadius_; }
void ArtifactShapeLayer::setPolygonSides(int s) { impl_->polygonSides_ = std::clamp(s, 3, kMaxShapePathVertices); impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
int ArtifactShapeLayer::polygonSides() const { return impl_->polygonSides_; }
bool ArtifactShapeLayer::hasCustomPolygon() const { return impl_->customPolygonPoints_.size() >= 3; }
void ArtifactShapeLayer::setCustomPolygonPoints(const std::vector<QPointF>& points, bool closed) {
  impl_->customPolygonPoints_.clear();
  impl_->customPolygonPoints_.reserve(
      std::min(points.size(), static_cast<size_t>(kMaxShapePathVertices)));
  for (const auto& point : points) {
    if (impl_->customPolygonPoints_.size() >= kMaxShapePathVertices) {
      break;
    }
    if (isSupportedShapePoint(point)) {
      impl_->customPolygonPoints_.push_back(point);
    }
  }
  impl_->customPolygonClosed_ = closed;
  impl_->customPathVertices_.clear(); // mutual exclusion
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
void ArtifactShapeLayer::clearCustomPolygonPoints() { if (impl_->customPolygonPoints_.empty()) return; impl_->customPolygonPoints_.clear(); impl_->customPolygonClosed_ = true; impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
std::vector<QPointF> ArtifactShapeLayer::customPolygonPoints() const { return impl_->customPolygonPoints_; }
bool ArtifactShapeLayer::customPolygonClosed() const { return impl_->customPolygonClosed_; }

// Phase 3: Stroke style setters/getters
void ArtifactShapeLayer::setStrokeCap(StrokeCap cap) {
 const auto normalized = static_cast<StrokeCap>(std::clamp(
     static_cast<int>(cap), static_cast<int>(StrokeCap::Flat),
     static_cast<int>(StrokeCap::Square)));
 if (impl_->strokeCap_ == normalized) return;
 impl_->strokeCap_ = normalized;
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
StrokeCap ArtifactShapeLayer::strokeCap() const { return impl_->strokeCap_; }
void ArtifactShapeLayer::setStrokeJoin(StrokeJoin join) {
 const auto normalized = static_cast<StrokeJoin>(std::clamp(
     static_cast<int>(join), static_cast<int>(StrokeJoin::Miter),
     static_cast<int>(StrokeJoin::Bevel)));
 if (impl_->strokeJoin_ == normalized) return;
 impl_->strokeJoin_ = normalized;
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
StrokeJoin ArtifactShapeLayer::strokeJoin() const { return impl_->strokeJoin_; }
void ArtifactShapeLayer::setStrokeAlign(StrokeAlign align) {
 const auto normalized = static_cast<StrokeAlign>(std::clamp(
     static_cast<int>(align), static_cast<int>(StrokeAlign::Center),
     static_cast<int>(StrokeAlign::Outside)));
 if (impl_->strokeAlign_ == normalized) return;
 impl_->strokeAlign_ = normalized;
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
StrokeAlign ArtifactShapeLayer::strokeAlign() const { return impl_->strokeAlign_; }
void ArtifactShapeLayer::setDashPattern(const std::vector<float>& pattern) {
 std::vector<float> normalized;
 normalized.reserve(
     std::min(pattern.size(), static_cast<size_t>(kMaxDashPatternEntries)));
 for (const float value : pattern) {
  if (normalized.size() >= kMaxDashPatternEntries) break;
  if (std::isfinite(value) && value > 0.001f) {
   normalized.push_back(std::min(value, static_cast<float>(kMaxShapeDimension)));
  }
 }
 impl_->dashPattern_ = std::move(normalized);
 impl_->markDirty();
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}
std::vector<float> ArtifactShapeLayer::dashPattern() const { return impl_->dashPattern_; }

// Phase 5: Bezier path
bool ArtifactShapeLayer::hasCustomPath() const { return impl_->customPathVertices_.size() >= 3; }
void ArtifactShapeLayer::setCustomPathVertices(const std::vector<CustomPathVertex>& vertices, bool closed) {
  impl_->customPathVertices_.clear();
  impl_->customPathVertices_.reserve(
      std::min(vertices.size(), static_cast<size_t>(kMaxShapePathVertices)));
  for (const auto& vertex : vertices) {
    if (impl_->customPathVertices_.size() >= kMaxShapePathVertices) {
      break;
    }
    if (isSupportedCustomPathVertex(vertex)) {
      impl_->customPathVertices_.push_back(vertex);
    }
  }
  impl_->customPathClosed_ = closed;
  impl_->customPolygonPoints_.clear(); // mutual exclusion
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
void ArtifactShapeLayer::clearCustomPath() {
  if (impl_->customPathVertices_.empty()) return;
  impl_->customPathVertices_.clear();
  impl_->customPathClosed_ = true;
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
std::vector<CustomPathVertex> ArtifactShapeLayer::customPathVertices() const { return impl_->customPathVertices_; }
bool ArtifactShapeLayer::customPathClosed() const { return impl_->customPathClosed_; }
ArtifactCore::PathFillRule ArtifactShapeLayer::customPathFillRule() const {
 return impl_->customPathFillRule_;
}
void ArtifactShapeLayer::setCustomPathFillRule(ArtifactCore::PathFillRule rule) {
 const auto normalized = rule == ArtifactCore::PathFillRule::EvenOdd
     ? ArtifactCore::PathFillRule::EvenOdd
     : ArtifactCore::PathFillRule::Winding;
 if (impl_->customPathFillRule_ == normalized) return;
 impl_->customPathFillRule_ = normalized;
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}

std::vector<ArtifactCore::ShapePath> ArtifactShapeLayer::nativeShapePaths() const
{
 // Animatable shape.* properties are evaluated at the current timeline
 // time so keyframed geometry animates during playback/rendering.
 const ShapeGeomDims dims = resolveShapeGeomDims(
     this, impl_->width_, impl_->height_, impl_->cornerRadius_,
     impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);

 return buildProcessedShapePaths(
     impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
     dims.starPoints, dims.starInnerRadius, dims.polygonSides,
     impl_->customPolygonPoints_, impl_->customPolygonClosed_,
     impl_->customPathVertices_, impl_->customPathClosed_,
     impl_->shapeOperators_);
}

ArtifactCore::ShapeLayer ArtifactShapeLayer::toCoreShapeLayer() const
{
 ArtifactCore::ShapeLayer core;
 core.setName(layerName());
 core.setVisible(isVisible());
 core.setOpacity(static_cast<double>(opacity()));

 auto* group = core.content();
 if (!group) {
  return core;
 }

 // Core SVG output supports gradients via <defs>; placement/taper strokes
 // are exported as outline geometry by the core exporter.
 ArtifactCore::FillSettings fill;
 fill.enabled = impl_->fillEnabled_;
 if (impl_->fillType_ == ArtifactSolidFillType::Solid) {
  fill.color = toQColor(impl_->fillColor_);
 } else {
  fill.gradientStart = toQColor(impl_->fillGradientStartColor_);
  fill.gradientEnd = toQColor(impl_->fillGradientEndColor_);
  fill.gradientAngleDegrees = impl_->fillGradientAngleDegrees_;
  fill.gradientCenterX = impl_->fillGradientCenterX_;
  fill.gradientCenterY = impl_->fillGradientCenterY_;
  fill.gradientRadiusRatio = std::clamp(impl_->fillGradientRadius_, 0.01f, 100000.0f);
  switch (impl_->fillType_) {
   case ArtifactSolidFillType::LinearGradient:
    fill.type = ArtifactCore::FillSettings::FillType::Linear; break;
   case ArtifactSolidFillType::RadialGradient:
    fill.type = ArtifactCore::FillSettings::FillType::Radial; break;
   case ArtifactSolidFillType::ConicalGradient:
    fill.type = ArtifactCore::FillSettings::FillType::Conic; break;
   case ArtifactSolidFillType::RepeatingGradient:
    fill.type = ArtifactCore::FillSettings::FillType::Repeating; break;
   case ArtifactSolidFillType::MirroredGradient:
    fill.type = ArtifactCore::FillSettings::FillType::Mirrored; break;
   case ArtifactSolidFillType::Solid: break;
  }
 }

 ArtifactCore::StrokeSettings stroke(
     toQColor(impl_->strokeGradientEnabled_
                  ? mixColor(impl_->strokeGradientStartColor_,
                             impl_->strokeGradientEndColor_, 0.5f)
                  : impl_->strokeColor_),
     static_cast<double>(impl_->strokeWidth_));
 stroke.enabled = impl_->strokeEnabled_ && impl_->strokeWidth_ > 0.0f;
 stroke.placement =
     impl_->strokeAlign_ == StrokeAlign::Inside
         ? ArtifactCore::StrokePlacement::Inside
         : (impl_->strokeAlign_ == StrokeAlign::Outside
                ? ArtifactCore::StrokePlacement::Outside
                : ArtifactCore::StrokePlacement::Center);
 stroke.taperStartScale = impl_->strokeTaperStart_;
 stroke.taperEndScale = impl_->strokeTaperEnd_;
 switch (impl_->strokeCap_) {
 case StrokeCap::Round: stroke.cap = ArtifactCore::LineCap::Round; break;
 case StrokeCap::Square: stroke.cap = ArtifactCore::LineCap::Square; break;
 case StrokeCap::Flat:
 default: stroke.cap = ArtifactCore::LineCap::Butt; break;
 }
 switch (impl_->strokeJoin_) {
 case StrokeJoin::Round: stroke.join = ArtifactCore::LineJoin::Round; break;
 case StrokeJoin::Bevel: stroke.join = ArtifactCore::LineJoin::Bevel; break;
 case StrokeJoin::Miter:
 default: stroke.join = ArtifactCore::LineJoin::Miter; break;
 }
 stroke.dashPattern.reserve(impl_->dashPattern_.size());
 for (const float dash : impl_->dashPattern_) {
  stroke.dashPattern.push_back(static_cast<double>(dash));
 }

 for (const auto& path : nativeShapePaths()) {
  auto shape = std::make_unique<ArtifactCore::PathShape>(path);
  shape->setFill(fill);
  shape->setStroke(stroke);
  group->addChild(std::move(shape));
 }
 return core;
}

void ArtifactShapeLayer::addShapeOperator(ArtifactCore::ShapeOperatorType type)
{
 if (!impl_) {
  return;
 }
 auto op = createShapeOperator(type);
 if (op) {
  impl_->shapeOperators_.push_back(std::move(op));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
 }
}

void ArtifactShapeLayer::clearShapeOperators()
{
 if (!impl_ || impl_->shapeOperators_.empty()) {
  return;
 }
 impl_->shapeOperators_.clear();
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
}

int ArtifactShapeLayer::shapeOperatorCount() const
{
 return impl_ ? static_cast<int>(impl_->shapeOperators_.size()) : 0;
}

ArtifactCore::ShapeOperatorType ArtifactShapeLayer::shapeOperatorTypeAt(int index) const
{
 if (!impl_ || index < 0 || index >= static_cast<int>(impl_->shapeOperators_.size())) {
  return ArtifactCore::ShapeOperatorType::None;
 }
  return impl_->shapeOperators_[static_cast<size_t>(index)]->type();
}

bool ArtifactShapeLayer::removeShapeOperatorAt(int index)
{
 if (!impl_ || index < 0 || index >= static_cast<int>(impl_->shapeOperators_.size())) {
  return false;
 }
 impl_->shapeOperators_.erase(impl_->shapeOperators_.begin() + index);
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
 return true;
}

bool ArtifactShapeLayer::moveShapeOperator(int fromIndex, int toIndex)
{
 if (!impl_ || fromIndex < 0 || toIndex < 0 ||
     fromIndex >= static_cast<int>(impl_->shapeOperators_.size()) ||
     toIndex >= static_cast<int>(impl_->shapeOperators_.size()) ||
     fromIndex == toIndex) {
  return false;
 }
 std::swap(impl_->shapeOperators_[static_cast<size_t>(fromIndex)],
           impl_->shapeOperators_[static_cast<size_t>(toIndex)]);
 impl_->markDirty();
 impl_->localBoundsCacheDirty_ = true;
 impl_->shapeContentCacheDirty_ = true;
 Q_EMIT changed();
 return true;
}

// ============================================================
// toQImage (Software rendering)
// ============================================================

QImage ArtifactShapeLayer::toQImage() const {
 if (hasAnimatedShapeGeometry(this)) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  impl_->rebuildCache(&dims);
 } else {
  impl_->rebuildCache();
 }
 return impl_->cachedImage_;
}

QImage ArtifactShapeLayer::getThumbnail(int width, int height) const
{
  const QSize targetSize(std::clamp(width, 1, 16384),
                         std::clamp(height, 1, 16384));
  const QImage image = toQImage();
  return image.isNull()
      ? ArtifactAbstractLayer::getThumbnail(targetSize.width(), targetSize.height())
      : image.scaled(targetSize, Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);
}

QRectF ArtifactShapeLayer::localBounds() const
{
  const bool geomAnimated = hasAnimatedShapeGeometry(this);
  if (!impl_->localBoundsCacheDirty_ && !geomAnimated) {
    return impl_->cachedLocalBounds_;
  }

  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  const auto boundsOfPoints = [](const std::vector<QPointF>& points) -> QRectF {
   if (points.empty()) {
    return QRectF();
   }
   qreal minX = points.front().x();
   qreal minY = points.front().y();
   qreal maxX = points.front().x();
   qreal maxY = points.front().y();
   for (const auto& point : points) {
    minX = std::min(minX, point.x());
    minY = std::min(minY, point.y());
    maxX = std::max(maxX, point.x());
    maxY = std::max(maxY, point.y());
   }
   return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
  };

  QRectF bounds;
  if (!impl_->shapeOperators_.empty()) {
   const auto processedPaths = buildProcessedShapePaths(
       impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
       dims.starPoints, dims.starInnerRadius, dims.polygonSides,
       impl_->customPolygonPoints_, impl_->customPolygonClosed_,
       impl_->customPathVertices_, impl_->customPathClosed_,
       impl_->shapeOperators_);
   for (const auto& path : processedPaths) {
    const QRectF pathBounds = path.boundingRect();
    bounds = bounds.isNull() ? pathBounds : bounds.united(pathBounds);
   }
  } else if (impl_->customPathVertices_.size() >= 3) {
   auto customPath = buildCustomShapePath(impl_->customPathVertices_,
                                          impl_->customPathClosed_);
   customPath.setFillRule(impl_->customPathFillRule_);
   bounds = customPath.boundingRect();
  } else if (impl_->customPathVertices_.size() >= 2) {
   std::vector<QPointF> pts;
   pts.reserve(impl_->customPathVertices_.size());
   for (const auto& v : impl_->customPathVertices_) pts.push_back(v.pos);
   bounds = boundsOfPoints(pts);
  } else if (impl_->customPolygonPoints_.size() >= 2) {
   bounds = boundsOfPoints(impl_->customPolygonPoints_);
  } else {
   bounds = buildShapePath(impl_->shapeType_, dims.width, dims.height,
                           dims.cornerRadius, dims.starPoints,
                           dims.starInnerRadius, dims.polygonSides)
                .boundingRect();
  }

  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
   const auto size = sourceSize();
   if (size.width <= 0 || size.height <= 0) {
    return QRectF();
   }
   bounds = QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
  }

  qreal strokePad = 0.0;
  if (impl_->strokeEnabled_ && impl_->strokeWidth_ > 0.0f) {
   const qreal strokeWidth = static_cast<qreal>(impl_->strokeWidth_);
   strokePad = impl_->strokeAlign_ == StrokeAlign::Outside
       ? strokeWidth
       : (impl_->strokeAlign_ == StrokeAlign::Center ? strokeWidth * 0.5 : 0.0);
   if (impl_->strokeJoin_ == StrokeJoin::Miter) {
    strokePad = std::max(strokePad, std::max<qreal>(1.0, strokeWidth) * 4.0);
   }
  }
  const qreal pad = std::max<qreal>(0.5, strokePad);
  impl_->cachedLocalBounds_ = bounds.adjusted(-pad, -pad, pad, pad);
  impl_->localBoundsCacheDirty_ = false;
  return impl_->cachedLocalBounds_;
}
// ============================================================
// draw (GPU rendering)
// ============================================================

void ArtifactShapeLayer::draw(ArtifactIRenderer* renderer) {
 if (!renderer) {
  return;
 }
 const QMatrix4x4 baseTransform = getGlobalTransform4x4();
  const float contentFieldWeight = compositionFieldContentWeight(this);
 auto* impl = impl_;
  const bool geomAnimated = hasAnimatedShapeGeometry(this);
  const ShapeGeomDims geomDims = resolveShapeGeomDims(
      this, impl->width_, impl->height_, impl->cornerRadius_,
      impl->starPoints_, impl->starInnerRadius_, impl->polygonSides_);
  const bool nativeOperatorCandidate =
      !impl->shapeOperators_.empty() &&
      (!impl->fillEnabled_ ||
       impl->fillType_ == ArtifactSolidFillType::Solid) &&
      (!impl->strokeEnabled_ || impl->strokeWidth_ <= 0.0f ||
       (impl->strokeAlign_ == StrokeAlign::Center &&
        !impl->hasCustomStrokeEffects()));
  if (nativeOperatorCandidate) {
    auto processedOperatorPaths = buildProcessedShapePaths(
        impl->shapeType_, geomDims.width, geomDims.height, geomDims.cornerRadius,
        geomDims.starPoints, geomDims.starInnerRadius, geomDims.polygonSides,
       impl->customPolygonPoints_, impl->customPolygonClosed_,
       impl->customPathVertices_, impl->customPathClosed_,
       impl->shapeOperators_);
   if (impl->customPathVertices_.size() >= 3) {
    for (auto& path : processedOperatorPaths) {
     path.setFillRule(impl->customPathFillRule_);
    }
   }
   if (!processedOperatorPaths.empty()) {
   const FloatColor fill(impl->fillColor_.r(), impl->fillColor_.g(),
                         impl->fillColor_.b(), impl->fillColor_.a());
   const FloatColor stroke(impl->strokeColor_.r(), impl->strokeColor_.g(),
                           impl->strokeColor_.b(), impl->strokeColor_.a());
   drawWithClonerEffect(
       this, baseTransform,
        [renderer, impl, processedPaths = processedOperatorPaths, fill, stroke,
        contentFieldWeight, this, geomAnimated](const QMatrix4x4& transform, float weight) {
        const float opacity = this->opacity() * weight * contentFieldWeight;
        const FloatColor drawFill(fill.r(), fill.g(), fill.b(), fill.a() * opacity);
        const FloatColor drawStroke(stroke.r(), stroke.g(), stroke.b(), stroke.a() * opacity);
        const double scaleX = std::hypot(static_cast<double>(transform(0, 0)),
                                         static_cast<double>(transform(1, 0)));
        const double scaleY = std::hypot(static_cast<double>(transform(0, 1)),
                                         static_cast<double>(transform(1, 1)));
        const double renderScale = std::max({1.0, scaleX, scaleY});
        const auto& geometry = impl->nativeGeometry(processedPaths,
                                                    0.25 / renderScale,
                                                    !geomAnimated);
        for (const auto& pathGeometry : geometry) {
         if (impl->fillEnabled_) {
          for (const auto& triangle : pathGeometry.triangles) {
           const QPointF p0 = mapPoint(transform, triangle.p0);
           const QPointF p1 = mapPoint(transform, triangle.p1);
           const QPointF p2 = mapPoint(transform, triangle.p2);
           renderer->drawSolidTriangleLocal(
               {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
               {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
               {static_cast<float>(p2.x()), static_cast<float>(p2.y())}, drawFill);
          }
         }
         for (const auto& segments : pathGeometry.subpaths) {
          if (segments.empty()) continue;
          std::vector<Detail::float2> points;
          points.reserve(segments.size() + 1);
          for (const auto& segment : segments) {
           const QPointF p = mapPoint(transform, segment.p0);
           points.push_back({static_cast<float>(p.x()), static_cast<float>(p.y())});
          }
          const QPointF end = mapPoint(transform, segments.back().p1);
          points.push_back({static_cast<float>(end.x()), static_cast<float>(end.y())});
          const bool closed = points.size() > 2 &&
                              std::hypot(points.front().x - points.back().x,
                                         points.front().y - points.back().y) < 0.01f;
          if (impl->strokeEnabled_ && impl->strokeWidth_ > 0.0f) {
           PolylineStyle style;
           style.thickness = std::max(
               1.0f, impl->strokeWidth_ * static_cast<float>(renderScale));
           style.cap = static_cast<PolylineCap>(impl->strokeCap_);
           style.join = static_cast<PolylineJoin>(impl->strokeJoin_);
           style.closed = closed;
           style.dashPattern = impl->dashPattern_;
           renderer->drawStyledPolyline(points, style, drawStroke);
          }
         }
        }
       });
   drawFractureOverlay(renderer, baseTransform,
                       QSizeF(geomDims.width, geomDims.height),
                       opacity() * contentFieldWeight);
   return;
  }
  }
 // Non-solid fills and custom stroke effects still use the compatibility
 // cache. Operator paths meeting the native candidate contract are handled
 // above; simple custom Bézier paths use ShapePath::flatten().
 if (impl->useCachePipeline()) {
  const ShapeCompatibilityFallback fallback = impl->compatibilityFallback();
  if (fallback != impl->lastLoggedFallback_) {
   qInfo() << "[ArtifactShapeLayer] compatibility fallback:"
           << compatibilityFallbackName(fallback);
   impl->lastLoggedFallback_ = fallback;
  }
  if (geomAnimated) {
   const ShapeGeomDims cacheDims = resolveShapeGeomDims(
       this, impl->width_, impl->height_, impl->cornerRadius_,
       impl->starPoints_, impl->starInnerRadius_, impl->polygonSides_);
   impl->rebuildCache(&cacheDims);
  } else {
   impl->rebuildCache();
  }
   const float layerOpacity = opacity() * contentFieldWeight;
   const ShapeGeomDims spriteDims = geomAnimated
       ? resolveShapeGeomDims(this, impl->width_, impl->height_,
                              impl->cornerRadius_, impl->starPoints_,
                              impl->starInnerRadius_, impl->polygonSides_)
       : ShapeGeomDims{impl->width_, impl->height_, impl->cornerRadius_,
                       impl->starPoints_, impl->starInnerRadius_,
                       impl->polygonSides_};
  drawWithClonerEffect(this, baseTransform,
       [renderer, impl, layerOpacity, spriteDims](const QMatrix4x4& transform, float weight) {
   renderer->drawSpriteTransformed(
       0.0f, 0.0f,
       static_cast<float>(spriteDims.width),
       static_cast<float>(spriteDims.height),
       transform, impl->cachedImage_,
       layerOpacity * weight);
  });
  drawFractureOverlay(renderer, baseTransform, QSizeF(spriteDims.width, spriteDims.height), layerOpacity);
  return;
 }
  drawWithClonerEffect(this, baseTransform,
                       [renderer, impl, this, contentFieldWeight, geomDims](const QMatrix4x4& transform, float weight) {
    const auto fill = FloatColor(
        impl->fillColor_.r(), impl->fillColor_.g(), impl->fillColor_.b(),
        impl->fillColor_.a() * this->opacity() * contentFieldWeight * weight);
    const auto stroke = FloatColor(
        impl->strokeColor_.r(), impl->strokeColor_.g(), impl->strokeColor_.b(),
        impl->strokeColor_.a() * this->opacity() * contentFieldWeight * weight);

    // A soft-body grid owns the rectangle's local vertices.  Keep all other
    // shape types on their existing path until they have a matching topology
    // bridge instead of approximating curves with an unrelated cloth mesh.
    if (impl->shapeType_ == Artifact::ShapeType::Rect &&
        drawMaterialGrid(this, renderer, transform, fill, stroke,
                         std::max(1.0f, impl->strokeWidth_),
                         impl->strokeEnabled_)) {
     return;
    }
    if (impl->shapeType_ == Artifact::ShapeType::Rect &&
        drawSoftBodyGrid(this, renderer, transform, fill, stroke,
                         std::max(1.0f, impl->strokeWidth_),
                         impl->strokeEnabled_)) {
     return;
    }

    const double scaleX = std::hypot(static_cast<double>(transform(0, 0)),
                                     static_cast<double>(transform(1, 0)));
    const double scaleY = std::hypot(static_cast<double>(transform(0, 1)),
                                     static_cast<double>(transform(1, 1)));
    const double renderScale = std::max({1.0, scaleX, scaleY});
    const auto& geometry = impl->shapeGeometry(geomDims, 0.25 / renderScale);
    if (geometry.subpaths.empty()) return;

    if (impl->fillEnabled_) {
     for (const auto& triangle : geometry.triangles) {
      const QPointF t0 = mapPoint(transform, triangle.p0);
      const QPointF t1 = mapPoint(transform, triangle.p1);
      const QPointF t2 = mapPoint(transform, triangle.p2);
      renderer->drawSolidTriangleLocal(
          {static_cast<float>(t0.x()), static_cast<float>(t0.y())},
          {static_cast<float>(t1.x()), static_cast<float>(t1.y())},
          {static_cast<float>(t2.x()), static_cast<float>(t2.y())}, fill);
     }
    }

    if (impl->strokeEnabled_ && impl->strokeWidth_ > 0.0f) {
     for (const auto& segments : geometry.subpaths) {
      if (segments.empty()) continue;
      std::vector<Detail::float2> points;
      points.reserve(segments.size() + 1);
      for (const auto& segment : segments) {
       const QPointF point = mapPoint(transform, segment.p0);
       points.push_back({static_cast<float>(point.x()),
                         static_cast<float>(point.y())});
      }
      const QPointF end = mapPoint(transform, segments.back().p1);
      points.push_back({static_cast<float>(end.x()),
                        static_cast<float>(end.y())});
      const bool closed = points.size() > 2 &&
                          std::hypot(points.front().x - points.back().x,
                                     points.front().y - points.back().y) < 0.01f;
      PolylineStyle style;
      style.thickness = std::max(
          1.0f, impl->strokeWidth_ * static_cast<float>(renderScale));
      style.cap = static_cast<PolylineCap>(impl->strokeCap_);
      style.join = static_cast<PolylineJoin>(impl->strokeJoin_);
      style.closed = closed;
      style.dashPattern = impl->dashPattern_;
      renderer->drawStyledPolyline(points, style, stroke);
     }
    }
  });
  drawFractureOverlay(renderer, baseTransform, QSizeF(impl_->width_, impl_->height_),
                      opacity() * contentFieldWeight);
}

// ============================================================
// Properties
// ============================================================

std::vector<ArtifactCore::PropertyGroup> ArtifactShapeLayer::getLayerPropertyGroups() const {
 std::vector<ArtifactCore::PropertyGroup> groups;
 auto makeProp = [this](const QString& name,
                        ArtifactCore::PropertyType type,
                        const QVariant& value,
                        int priority,
                        bool animatable = true) {
  auto prop = persistentLayerProperty(name, type, value, priority);
  prop->setAnimatable(animatable);
  return prop;
 };

 // Shape Type Group
 ArtifactCore::PropertyGroup shapeGroup;
 shapeGroup.setName("Shape");

 auto shapeTypeProp = makeProp(QStringLiteral("shape.type"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(impl_->shapeType_),
                               -220,
                               false);
 shapeTypeProp->setHardRange(0, 6);
 shapeTypeProp->setDisplayLabel(QStringLiteral("Type"));
 QString shapeTypeTooltip = QStringLiteral(
     "0=Rect, 1=Ellipse, 2=Star, 3=Polygon, 4=Line, 5=Triangle, 6=Square");
 shapeTypeTooltip += QStringLiteral(" (current: ");
 shapeTypeTooltip += shapeTypeName(static_cast<int>(impl_->shapeType_));
 shapeTypeTooltip += QStringLiteral(")");
 shapeTypeProp->setTooltip(shapeTypeTooltip);
 shapeGroup.addProperty(shapeTypeProp);

 auto widthProp = makeProp(QStringLiteral("shape.width"),
                           ArtifactCore::PropertyType::Integer, impl_->width_,
                           -219);
 widthProp->setDisplayLabel(QStringLiteral("Width"));
 widthProp->setHardRange(1, 16384);
 shapeGroup.addProperty(widthProp);

 auto heightProp = makeProp(QStringLiteral("shape.height"),
                            ArtifactCore::PropertyType::Integer,
                            impl_->height_, -218);
 heightProp->setDisplayLabel(QStringLiteral("Height"));
 heightProp->setHardRange(1, 16384);
 shapeGroup.addProperty(heightProp);

 groups.push_back(shapeGroup);

 // Appearance Group
 ArtifactCore::PropertyGroup appearanceGroup;
 appearanceGroup.setName("Appearance");

 auto fillColorProp = makeProp(QStringLiteral("shape.fillColor"),
                               ArtifactCore::PropertyType::Color,
                               QColor(
  static_cast<int>(impl_->fillColor_.r() * 255),
  static_cast<int>(impl_->fillColor_.g() * 255),
  static_cast<int>(impl_->fillColor_.b() * 255),
  static_cast<int>(impl_->fillColor_.a() * 255)
  ),
  -210);
 fillColorProp->setDisplayLabel(QStringLiteral("Fill Color"));
  appearanceGroup.addProperty(fillColorProp);

  auto fillEnabledProp = makeProp(QStringLiteral("shape.fillEnabled"),
                                  ArtifactCore::PropertyType::Boolean,
                                  impl_->fillEnabled_, -209);
  fillEnabledProp->setDisplayLabel(QStringLiteral("Fill Enabled"));
   appearanceGroup.addProperty(fillEnabledProp);

  auto fillTypeProp = makeProp(QStringLiteral("shape.fillType"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(impl_->fillType_), -199);
  fillTypeProp->setHardRange(0, 5);
  fillTypeProp->setDisplayLabel(QStringLiteral("Fill Type"));
  fillTypeProp->setTooltip(QStringLiteral("0=Solid, 1=Linear, 2=Radial, 3=Conical, 4=Repeating, 5=Mirrored"));
   appearanceGroup.addProperty(fillTypeProp);

  auto fillGradStartProp = makeProp(QStringLiteral("shape.fillGradientStartColor"),
                                    ArtifactCore::PropertyType::Color,
                                    QColor(
    static_cast<int>(impl_->fillGradientStartColor_.r() * 255),
    static_cast<int>(impl_->fillGradientStartColor_.g() * 255),
    static_cast<int>(impl_->fillGradientStartColor_.b() * 255),
    static_cast<int>(impl_->fillGradientStartColor_.a() * 255)
    ),
    -198);
  fillGradStartProp->setDisplayLabel(QStringLiteral("Gradient Start"));
   appearanceGroup.addProperty(fillGradStartProp);

  auto fillGradEndProp = makeProp(QStringLiteral("shape.fillGradientEndColor"),
                                  ArtifactCore::PropertyType::Color,
                                  QColor(
    static_cast<int>(impl_->fillGradientEndColor_.r() * 255),
    static_cast<int>(impl_->fillGradientEndColor_.g() * 255),
    static_cast<int>(impl_->fillGradientEndColor_.b() * 255),
    static_cast<int>(impl_->fillGradientEndColor_.a() * 255)
    ),
    -197);
  fillGradEndProp->setDisplayLabel(QStringLiteral("Gradient End"));
   appearanceGroup.addProperty(fillGradEndProp);

  auto fillGradAngleProp = makeProp(QStringLiteral("shape.fillGradientAngle"),
                                    ArtifactCore::PropertyType::Float,
                                    impl_->fillGradientAngleDegrees_, -196);
  fillGradAngleProp->setSoftRange(-360.0, 360.0);
  fillGradAngleProp->setHardRange(-360.0, 360.0);
  fillGradAngleProp->setDisplayLabel(QStringLiteral("Gradient Angle"));
   appearanceGroup.addProperty(fillGradAngleProp);

  auto fillGradCenterXProp = makeProp(QStringLiteral("shape.fillGradientCenterX"),
                                      ArtifactCore::PropertyType::Float,
                                      impl_->fillGradientCenterX_, -195);
  fillGradCenterXProp->setSoftRange(0.0, 1.0);
  fillGradCenterXProp->setHardRange(0.0, 1.0);
  fillGradCenterXProp->setDisplayLabel(QStringLiteral("Gradient Center X"));
   appearanceGroup.addProperty(fillGradCenterXProp);

  auto fillGradCenterYProp = makeProp(QStringLiteral("shape.fillGradientCenterY"),
                                      ArtifactCore::PropertyType::Float,
                                      impl_->fillGradientCenterY_, -194);
  fillGradCenterYProp->setSoftRange(0.0, 1.0);
  fillGradCenterYProp->setHardRange(0.0, 1.0);
  fillGradCenterYProp->setDisplayLabel(QStringLiteral("Gradient Center Y"));
   appearanceGroup.addProperty(fillGradCenterYProp);

  auto fillGradRadiusProp = makeProp(QStringLiteral("shape.fillGradientRadius"),
                                     ArtifactCore::PropertyType::Float,
                                     impl_->fillGradientRadius_, -193);
  fillGradRadiusProp->setSoftRange(0.0, 2.0);
  fillGradRadiusProp->setHardRange(0.0, 100000.0);
  fillGradRadiusProp->setDisplayLabel(QStringLiteral("Gradient Radius"));
   appearanceGroup.addProperty(fillGradRadiusProp);

 auto strokeColorProp = makeProp(QStringLiteral("shape.strokeColor"),
                                 ArtifactCore::PropertyType::Color,
                                 QColor(
  static_cast<int>(impl_->strokeColor_.r() * 255),
  static_cast<int>(impl_->strokeColor_.g() * 255),
  static_cast<int>(impl_->strokeColor_.b() * 255),
  static_cast<int>(impl_->strokeColor_.a() * 255)
  ),
  -208);
 strokeColorProp->setDisplayLabel(QStringLiteral("Stroke Color"));
  appearanceGroup.addProperty(strokeColorProp);

 auto strokeWidthProp = makeProp(QStringLiteral("shape.strokeWidth"),
                                 ArtifactCore::PropertyType::Float,
                                 impl_->strokeWidth_, -207);
 strokeWidthProp->setSoftRange(0.0, 64.0);
 strokeWidthProp->setHardRange(0.0, 16384.0);
 strokeWidthProp->setDisplayLabel(QStringLiteral("Stroke Width"));
  appearanceGroup.addProperty(strokeWidthProp);

 auto strokeEnabledProp = makeProp(QStringLiteral("shape.strokeEnabled"),
                                   ArtifactCore::PropertyType::Boolean,
                                   impl_->strokeEnabled_, -206);
 strokeEnabledProp->setDisplayLabel(QStringLiteral("Stroke Enabled"));
  appearanceGroup.addProperty(strokeEnabledProp);

 auto strokeTaperStartProp = makeProp(QStringLiteral("shape.strokeTaperStart"),
                                      ArtifactCore::PropertyType::Float,
                                      impl_->strokeTaperStart_, -205, false);
 strokeTaperStartProp->setDisplayLabel(QStringLiteral("Taper Start"));
 strokeTaperStartProp->setSoftRange(0.0, 1.0);
 strokeTaperStartProp->setHardRange(0.0, 1.0);
 strokeTaperStartProp->setTooltip(QStringLiteral("0.0 = thin, 1.0 = full width"));
 appearanceGroup.addProperty(strokeTaperStartProp);

 auto strokeTaperEndProp = makeProp(QStringLiteral("shape.strokeTaperEnd"),
                                    ArtifactCore::PropertyType::Float,
                                    impl_->strokeTaperEnd_, -204, false);
 strokeTaperEndProp->setDisplayLabel(QStringLiteral("Taper End"));
 strokeTaperEndProp->setSoftRange(0.0, 1.0);
 strokeTaperEndProp->setHardRange(0.0, 1.0);
 strokeTaperEndProp->setTooltip(QStringLiteral("0.0 = thin, 1.0 = full width"));
 appearanceGroup.addProperty(strokeTaperEndProp);

 auto strokeGradientEnabledProp = makeProp(QStringLiteral("shape.strokeGradientEnabled"),
                                           ArtifactCore::PropertyType::Boolean,
                                           impl_->strokeGradientEnabled_, -203);
 strokeGradientEnabledProp->setDisplayLabel(QStringLiteral("Stroke Gradient"));
 appearanceGroup.addProperty(strokeGradientEnabledProp);

 auto strokeGradientStartProp = makeProp(QStringLiteral("shape.strokeGradientStartColor"),
                                         ArtifactCore::PropertyType::Color,
                                         QColor(
  static_cast<int>(impl_->strokeGradientStartColor_.r() * 255),
  static_cast<int>(impl_->strokeGradientStartColor_.g() * 255),
  static_cast<int>(impl_->strokeGradientStartColor_.b() * 255),
  static_cast<int>(impl_->strokeGradientStartColor_.a() * 255)
  ),
  -202);
 strokeGradientStartProp->setDisplayLabel(QStringLiteral("Gradient Start"));
 appearanceGroup.addProperty(strokeGradientStartProp);

 auto strokeGradientEndProp = makeProp(QStringLiteral("shape.strokeGradientEndColor"),
                                       ArtifactCore::PropertyType::Color,
                                       QColor(
  static_cast<int>(impl_->strokeGradientEndColor_.r() * 255),
  static_cast<int>(impl_->strokeGradientEndColor_.g() * 255),
  static_cast<int>(impl_->strokeGradientEndColor_.b() * 255),
  static_cast<int>(impl_->strokeGradientEndColor_.a() * 255)
  ),
  -201);
 strokeGradientEndProp->setDisplayLabel(QStringLiteral("Gradient End"));
 appearanceGroup.addProperty(strokeGradientEndProp);

 auto strokeCapProp = makeProp(QStringLiteral("shape.strokeCap"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(impl_->strokeCap_), -200, false);
  strokeCapProp->setHardRange(0, 2);
  strokeCapProp->setDisplayLabel(QStringLiteral("Stroke Cap"));
  strokeCapProp->setTooltip(QStringLiteral("0=Flat, 1=Round, 2=Square"));
   appearanceGroup.addProperty(strokeCapProp);

 auto strokeJoinProp = makeProp(QStringLiteral("shape.strokeJoin"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<int>(impl_->strokeJoin_), -199, false);
  strokeJoinProp->setHardRange(0, 2);
  strokeJoinProp->setDisplayLabel(QStringLiteral("Stroke Join"));
  strokeJoinProp->setTooltip(QStringLiteral("0=Miter, 1=Round, 2=Bevel"));
   appearanceGroup.addProperty(strokeJoinProp);

 auto strokeAlignProp = makeProp(QStringLiteral("shape.strokeAlign"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<int>(impl_->strokeAlign_), -198, false);
  strokeAlignProp->setHardRange(0, 2);
  strokeAlignProp->setDisplayLabel(QStringLiteral("Stroke Align"));
  strokeAlignProp->setTooltip(QStringLiteral("0=Center, 1=Inside, 2=Outside"));
   appearanceGroup.addProperty(strokeAlignProp);

 auto dashPatternProp = makeProp(QStringLiteral("shape.dashPattern"),
                                 ArtifactCore::PropertyType::String,
                                 dashPatternToString(impl_->dashPattern_), -197, false);
 dashPatternProp->setDisplayLabel(QStringLiteral("Dash Pattern"));
 dashPatternProp->setTooltip(QStringLiteral("Comma-separated dash lengths (empty=solid). E.g. '4,2' = 4px dash, 2px gap"));
  appearanceGroup.addProperty(dashPatternProp);

 groups.push_back(appearanceGroup);

 // Shape-specific params
 ArtifactCore::PropertyGroup paramsGroup;
 paramsGroup.setName("Shape Parameters");

 auto cornerProp = makeProp(QStringLiteral("shape.cornerRadius"),
                            ArtifactCore::PropertyType::Float,
                            impl_->cornerRadius_, -200);
 cornerProp->setDisplayLabel(QStringLiteral("Corner Radius"));
 cornerProp->setSoftRange(0.0, 256.0);
 cornerProp->setHardRange(0.0, 100000.0);
  paramsGroup.addProperty(cornerProp);
 auto pointsProp = makeProp(QStringLiteral("shape.starPoints"),
                            ArtifactCore::PropertyType::Integer,
                            impl_->starPoints_, -199);
 pointsProp->setDisplayLabel(QStringLiteral("Points"));
 pointsProp->setHardRange(3, 2048);
  paramsGroup.addProperty(pointsProp);

 auto innerProp = makeProp(QStringLiteral("shape.starInnerRadius"),
                           ArtifactCore::PropertyType::Float,
                           impl_->starInnerRadius_, -198);
 innerProp->setDisplayLabel(QStringLiteral("Inner Radius"));
 innerProp->setSoftRange(0.0, 1.0);
 innerProp->setHardRange(0.0, 1.0);
  paramsGroup.addProperty(innerProp);
 auto sidesProp = makeProp(QStringLiteral("shape.polygonSides"),
                           ArtifactCore::PropertyType::Integer,
                           impl_->polygonSides_, -197);
 sidesProp->setDisplayLabel(QStringLiteral("Sides"));
 sidesProp->setHardRange(3, 100000);
  paramsGroup.addProperty(sidesProp);
 auto fillRuleProp = makeProp(QStringLiteral("shape.customPathFillRule"),
                              ArtifactCore::PropertyType::Integer,
                              static_cast<int>(impl_->customPathFillRule_), -196);
 fillRuleProp->setDisplayLabel(QStringLiteral("Custom Path Fill Rule"));
 fillRuleProp->setHardRange(0, 1);
 fillRuleProp->setTooltip(QStringLiteral("0=Winding, 1=Even Odd"));
 paramsGroup.addProperty(fillRuleProp);

 groups.push_back(paramsGroup);

 // Shape Operators
 for (int i = 0; i < shapeOperatorCount(); ++i) {
   const auto &op = impl_->shapeOperators_[static_cast<size_t>(i)];
   ArtifactCore::PropertyGroup opGroup;
   opGroup.setName(QStringLiteral("Operator %1 (%2)")
                       .arg(i + 1)
                       .arg(operatorName(op->type())));

   QString prefix = QStringLiteral("shape.operator.%1.").arg(i);

   if (auto trim = dynamic_cast<const ArtifactCore::TrimPaths *>(op.get())) {
     auto startProp = makeProp(prefix + QStringLiteral("start"),
                               ArtifactCore::PropertyType::Float,
                               trim->start(), -100);
     startProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(startProp);
     auto endProp = makeProp(prefix + QStringLiteral("end"),
                             ArtifactCore::PropertyType::Float,
                             trim->end(), -99);
     endProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(endProp);
     auto offsetProp = makeProp(prefix + QStringLiteral("offset"),
                                ArtifactCore::PropertyType::Float,
                                trim->offset(), -98);
     offsetProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(offsetProp);
     opGroup.addProperty(makeProp(prefix + QStringLiteral("trimMode"),
                                  ArtifactCore::PropertyType::Integer,
                                  static_cast<int>(trim->trimMode()), -97));
   } else if (auto repeater =
                  dynamic_cast<const ArtifactCore::Repeater *>(op.get())) {
     auto copiesProp = makeProp(prefix + QStringLiteral("copies"),
                                ArtifactCore::PropertyType::Integer,
                                repeater->copies(), -100);
     copiesProp->setHardRange(1, 1000);
     opGroup.addProperty(copiesProp);
     auto offsetProp = makeProp(prefix + QStringLiteral("offset"),
                                ArtifactCore::PropertyType::Float,
                                repeater->offset(), -99);
     offsetProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(offsetProp);
     opGroup.addProperty(makeProp(prefix + QStringLiteral("anchorPoint"),
                                  ArtifactCore::PropertyType::String,
                                  repeater->anchorPoint(), -98));
     opGroup.addProperty(makeProp(prefix + QStringLiteral("position"),
                                  ArtifactCore::PropertyType::String,
                                  repeater->position(), -97));
     opGroup.addProperty(makeProp(prefix + QStringLiteral("scale"),
                                  ArtifactCore::PropertyType::String,
                                  repeater->scale(), -96));
     auto rotationProp = makeProp(prefix + QStringLiteral("rotation"),
                                  ArtifactCore::PropertyType::Float,
                                  repeater->rotation(), -95);
     rotationProp->setHardRange(-360000.0, 360000.0);
     opGroup.addProperty(rotationProp);
     auto startOpacityProp = makeProp(prefix + QStringLiteral("startOpacity"),
                                      ArtifactCore::PropertyType::Float,
                                      repeater->startOpacity(), -94);
     startOpacityProp->setHardRange(0.0, 100.0);
     opGroup.addProperty(startOpacityProp);
     auto endOpacityProp = makeProp(prefix + QStringLiteral("endOpacity"),
                                    ArtifactCore::PropertyType::Float,
                                    repeater->endOpacity(), -93);
     endOpacityProp->setHardRange(0.0, 100.0);
     opGroup.addProperty(endOpacityProp);
   } else if (auto offset =
                  dynamic_cast<const ArtifactCore::OffsetPaths *>(op.get())) {
     auto offsetProp = makeProp(prefix + QStringLiteral("offset"),
                                ArtifactCore::PropertyType::Float,
                                offset->offset(), -100);
     offsetProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(offsetProp);
     opGroup.addProperty(makeProp(prefix + QStringLiteral("join"),
                                  ArtifactCore::PropertyType::Integer,
                                  offset->joinValue(), -99));
     opGroup.addProperty(makeProp(prefix + QStringLiteral("miterLimit"),
                                  ArtifactCore::PropertyType::Float,
                                  offset->miterLimit(), -98));
   } else if (auto pb =
                  dynamic_cast<const ArtifactCore::PuckerBloat *>(op.get())) {
     auto amountProp = makeProp(prefix + QStringLiteral("amount"),
                                ArtifactCore::PropertyType::Float,
                                pb->amount(), -100);
     amountProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(amountProp);
   } else if (auto rc =
                  dynamic_cast<const ArtifactCore::RoundedCorners *>(op.get())) {
     auto radiusProp = makeProp(prefix + QStringLiteral("radius"),
                                ArtifactCore::PropertyType::Float,
                                rc->radius(), -100);
     radiusProp->setHardRange(0.0, 100000.0);
     opGroup.addProperty(radiusProp);
   } else if (auto wp =
                  dynamic_cast<const ArtifactCore::WigglePaths *>(op.get())) {
     auto amountProp = makeProp(prefix + QStringLiteral("amount"),
                                ArtifactCore::PropertyType::Float,
                                wp->amount(), -100);
     amountProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(amountProp);
     auto frequencyProp = makeProp(prefix + QStringLiteral("frequency"),
                                   ArtifactCore::PropertyType::Float,
                                   wp->frequency(), -99);
     frequencyProp->setHardRange(0.0, 10000.0);
     opGroup.addProperty(frequencyProp);
   } else if (auto zz = dynamic_cast<const ArtifactCore::ZigZag *>(op.get())) {
     auto amountProp = makeProp(prefix + QStringLiteral("amount"),
                                ArtifactCore::PropertyType::Float,
                                zz->amount(), -100);
     amountProp->setHardRange(-100000.0, 100000.0);
     opGroup.addProperty(amountProp);
     auto frequencyProp = makeProp(prefix + QStringLiteral("frequency"),
                                   ArtifactCore::PropertyType::Float,
                                   zz->frequency(), -99);
     frequencyProp->setHardRange(0.0, 10000.0);
     opGroup.addProperty(frequencyProp);
   } else if (auto twist =
                  dynamic_cast<const ArtifactCore::Twist *>(op.get())) {
     opGroup.addProperty(makeProp(prefix + QStringLiteral("angle"),
                                  ArtifactCore::PropertyType::Float,
                                  twist->angle(), -100));
   } else if (auto wobble =
                  dynamic_cast<const ArtifactCore::HandDrawnWobble *>(op.get())) {
     auto amountProp = makeProp(prefix + QStringLiteral("wobbleAmount"),
                                ArtifactCore::PropertyType::Float,
                                wobble->wobbleAmount(), -100);
     amountProp->setHardRange(0.0, 100000.0);
     opGroup.addProperty(amountProp);
     auto frequencyProp = makeProp(prefix + QStringLiteral("wobbleFrequency"),
                                   ArtifactCore::PropertyType::Float,
                                   wobble->wobbleFrequency(), -99);
     frequencyProp->setHardRange(0.0, 10000.0);
     opGroup.addProperty(frequencyProp);
     auto pressureProp = makeProp(prefix + QStringLiteral("pressureJitter"),
                                  ArtifactCore::PropertyType::Float,
                                  wobble->pressureJitter(), -98);
     pressureProp->setHardRange(0.0, 1.0);
     opGroup.addProperty(pressureProp);
     auto gapProp = makeProp(prefix + QStringLiteral("gapProbability"),
                             ArtifactCore::PropertyType::Float,
                             wobble->gapProbability(), -97);
     gapProp->setHardRange(0.0, 1.0);
     opGroup.addProperty(gapProp);
   }

   groups.push_back(opGroup);
 }

 return groups;
}

bool ArtifactShapeLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
if (propertyPath == "shape.type") {
  setShapeType(static_cast<Artifact::ShapeType>(value.toInt()));
  return true;
 }
 if (propertyPath == "shape.fillColor") {
  auto c = value.value<QColor>();
  setFillColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
  return true;
 }
 if (propertyPath == "shape.fillEnabled") {
   setFillEnabled(value.toBool());
   return true;
  }
  if (propertyPath == "shape.fillType") {
   setFillType(static_cast<ArtifactSolidFillType>(value.toInt()));
   return true;
  }
  if (propertyPath == "shape.customPathFillRule") {
   setCustomPathFillRule(static_cast<ArtifactCore::PathFillRule>(value.toInt()));
   return true;
  }
  if (propertyPath == "shape.fillGradientStartColor") {
   auto c = value.value<QColor>();
   setFillGradientStartColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   return true;
  }
  if (propertyPath == "shape.fillGradientEndColor") {
   auto c = value.value<QColor>();
   setFillGradientEndColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   return true;
  }
  if (propertyPath == "shape.fillGradientAngle") {
   setFillGradientAngleDegrees(value.toFloat());
   return true;
  }
  if (propertyPath == "shape.fillGradientCenterX") {
   setFillGradientCenterX(value.toFloat());
   return true;
  }
  if (propertyPath == "shape.fillGradientCenterY") {
   setFillGradientCenterY(value.toFloat());
   return true;
  }
  if (propertyPath == "shape.fillGradientRadius") {
   setFillGradientRadius(value.toFloat());
   return true;
  }
 if (propertyPath == "shape.strokeColor") {
  auto c = value.value<QColor>();
  setStrokeColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
  return true;
 }
 if (propertyPath == "shape.strokeWidth") {
  setStrokeWidth(value.toFloat());
  return true;
 }
 if (propertyPath == "shape.strokeEnabled") {
  setStrokeEnabled(value.toBool());
  return true;
 }
 if (propertyPath == "shape.strokeTaperStart") {
  setStrokeTaper(value.toFloat(), impl_->strokeTaperEnd_);
  return true;
 }
 if (propertyPath == "shape.strokeTaperEnd") {
  setStrokeTaper(impl_->strokeTaperStart_, value.toFloat());
  return true;
 }
 if (propertyPath == "shape.strokeGradientEnabled") {
  setStrokeGradientEnabled(value.toBool());
  return true;
 }
 if (propertyPath == "shape.strokeGradientStartColor") {
  const auto c = value.value<QColor>();
  setStrokeGradientStartColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
  return true;
 }
 if (propertyPath == "shape.strokeGradientEndColor") {
  const auto c = value.value<QColor>();
  setStrokeGradientEndColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
  return true;
 }
 if (propertyPath == "shape.width") {
  setSize(value.toInt(), impl_->height_);
  return true;
 }
 if (propertyPath == "shape.height") {
  setSize(impl_->width_, value.toInt());
  return true;
 }
 if (propertyPath == "shape.cornerRadius") {
  setCornerRadius(value.toFloat());
  return true;
 }
 if (propertyPath == "shape.starPoints") {
  setStarPoints(value.toInt());
  return true;
 }
 if (propertyPath == "shape.starInnerRadius") {
  setStarInnerRadius(value.toFloat());
  return true;
 }
 if (propertyPath == "shape.polygonSides") {
  setPolygonSides(value.toInt());
  return true;
 }
 if (propertyPath == "shape.strokeCap") {
  setStrokeCap(static_cast<StrokeCap>(value.toInt()));
  return true;
 }
 if (propertyPath == "shape.strokeJoin") {
  setStrokeJoin(static_cast<StrokeJoin>(value.toInt()));
  return true;
 }
 if (propertyPath == "shape.strokeAlign") {
  setStrokeAlign(static_cast<StrokeAlign>(value.toInt()));
  return true;
 }
 if (propertyPath == "shape.dashPattern") {
  setDashPattern(stringToDashPattern(value.toString()));
  return true;
 }

 if (propertyPath.startsWith("shape.operator.")) {
   QStringList parts = propertyPath.split('.');
   if (parts.size() >= 4) {
     bool ok = false;
     int opIndex = parts[2].toInt(&ok);
     QString field = parts[3];
     if (ok && opIndex >= 0 &&
         opIndex < static_cast<int>(impl_->shapeOperators_.size())) {
       auto &op = impl_->shapeOperators_[static_cast<size_t>(opIndex)];
       bool handled = false;
       if (auto trim = dynamic_cast<ArtifactCore::TrimPaths *>(op.get())) {
         const auto safeTrimValue = [](const QVariant &input,
                                       const float fallback) {
           const float value = input.toFloat();
           return std::isfinite(value)
               ? std::clamp(value, -100000.0f, 100000.0f)
               : fallback;
         };
         if (field == "start") {
           trim->setStart(safeTrimValue(value, 0.0f));
           handled = true;
         } else if (field == "end") {
           trim->setEnd(safeTrimValue(value, 100.0f));
           handled = true;
         } else if (field == "offset") {
           trim->setOffset(safeTrimValue(value, 0.0f));
           handled = true;
         } else if (field == "trimMode") {
           trim->setTrimMode(static_cast<ArtifactCore::TrimMode>(value.toInt()));
           handled = true;
         }
       } else if (auto repeater =
                      dynamic_cast<ArtifactCore::Repeater *>(op.get())) {
         const auto safeRepeaterFloat = [](const QVariant &input,
                                           const float fallback,
                                           const float lower,
                                           const float upper) {
           const float value = input.toFloat();
           return std::isfinite(value) ? std::clamp(value, lower, upper)
                                       : fallback;
         };
         if (field == "copies") {
           repeater->setCopies(std::clamp(value.toInt(), 1, 1000));
           handled = true;
         } else if (field == "offset") {
           repeater->setOffset(safeRepeaterFloat(
               value, 0.0f, -100000.0f, 100000.0f));
           handled = true;
         } else if (field == "anchorPoint") {
           repeater->setAnchorPoint(value.toPointF());
           handled = true;
         } else if (field == "position") {
           repeater->setPosition(value.toPointF());
           handled = true;
         } else if (field == "scale") {
           repeater->setScale(value.toPointF());
           handled = true;
         } else if (field == "rotation") {
           repeater->setRotation(safeRepeaterFloat(
               value, 0.0f, -360000.0f, 360000.0f));
           handled = true;
         } else if (field == "startOpacity") {
           const float opacity = value.toFloat();
           repeater->setStartOpacity(std::isfinite(opacity)
               ? std::clamp(opacity, 0.0f, 100.0f) : 100.0f);
           handled = true;
         } else if (field == "endOpacity") {
           const float opacity = value.toFloat();
           repeater->setEndOpacity(std::isfinite(opacity)
               ? std::clamp(opacity, 0.0f, 100.0f) : 100.0f);
           handled = true;
         }
       } else if (auto offset =
                      dynamic_cast<ArtifactCore::OffsetPaths *>(op.get())) {
         if (field == "offset") {
           const float next = value.toFloat();
           offset->setOffset(std::isfinite(next)
               ? std::clamp(next, -100000.0f, 100000.0f) : 0.0f);
           handled = true;
         } else if (field == "join") {
           offset->setJoinValue(value.toInt());
           handled = true;
         } else if (field == "miterLimit") {
           offset->setMiterLimit(value.toFloat());
           handled = true;
         }
       } else if (auto pb =
                      dynamic_cast<ArtifactCore::PuckerBloat *>(op.get())) {
         if (field == "amount") {
           const float next = value.toFloat();
           pb->setAmount(std::isfinite(next)
               ? std::clamp(next, -100000.0f, 100000.0f) : 0.0f);
           handled = true;
         }
       } else if (auto rc =
                      dynamic_cast<ArtifactCore::RoundedCorners *>(op.get())) {
         if (field == "radius") {
           const float next = value.toFloat();
           rc->setRadius(std::isfinite(next)
               ? std::clamp(next, 0.0f, 100000.0f) : 0.0f);
           handled = true;
         }
       } else if (auto wp =
                      dynamic_cast<ArtifactCore::WigglePaths *>(op.get())) {
         const auto safeOperatorValue = [](const QVariant &input, const float fallback,
                                           const float lower, const float upper) {
           const float value = input.toFloat();
           return std::isfinite(value) ? std::clamp(value, lower, upper) : fallback;
         };
         if (field == "amount") {
           wp->setAmount(safeOperatorValue(value, 0.0f, -100000.0f, 100000.0f));
           handled = true;
         } else if (field == "frequency") {
           wp->setFrequency(safeOperatorValue(value, 1.0f, 0.0f, 10000.0f));
           handled = true;
         }
       } else if (auto zz = dynamic_cast<ArtifactCore::ZigZag *>(op.get())) {
         const auto safeOperatorValue = [](const QVariant &input, const float fallback,
                                           const float lower, const float upper) {
           const float value = input.toFloat();
           return std::isfinite(value) ? std::clamp(value, lower, upper) : fallback;
         };
         if (field == "amount") {
           zz->setAmount(safeOperatorValue(value, 0.0f, -100000.0f, 100000.0f));
           handled = true;
         } else if (field == "frequency") {
           zz->setFrequency(safeOperatorValue(value, 1.0f, 0.0f, 10000.0f));
           handled = true;
         }
       } else if (auto twist =
                      dynamic_cast<ArtifactCore::Twist *>(op.get())) {
         if (field == "angle") {
           twist->setAngle(value.toFloat());
           handled = true;
         }
       } else if (auto wobble =
                      dynamic_cast<ArtifactCore::HandDrawnWobble *>(op.get())) {
         const auto safeOperatorValue = [](const QVariant &input, const float fallback,
                                           const float lower, const float upper) {
           const float value = input.toFloat();
           return std::isfinite(value) ? std::clamp(value, lower, upper) : fallback;
         };
         if (field == "wobbleAmount") {
           wobble->setWobbleAmount(safeOperatorValue(value, 0.0f, 0.0f, 100000.0f));
           handled = true;
         } else if (field == "wobbleFrequency") {
           wobble->setWobbleFrequency(safeOperatorValue(value, 1.0f, 0.0f, 10000.0f));
           handled = true;
         } else if (field == "pressureJitter") {
           wobble->setPressureJitter(safeOperatorValue(value, 0.0f, 0.0f, 1.0f));
           handled = true;
         } else if (field == "gapProbability") {
           wobble->setGapProbability(safeOperatorValue(value, 0.0f, 0.0f, 1.0f));
           handled = true;
         }
       }

       if (handled) {
       impl_->markDirty();
        impl_->localBoundsCacheDirty_ = true;
        impl_->shapeContentCacheDirty_ = true;
        Q_EMIT changed();
         return true;
       }
     }
   }
 }
 return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
}

// ============================================================
// Serialization
// ============================================================

QJsonObject ArtifactShapeLayer::toJson() const {
 QJsonObject obj = ArtifactAbstract2DLayer::toJson();
 obj["type"] = static_cast<int>(LayerType::Shape);
 obj["layerType"] = QStringLiteral("Shape");
  obj["shapeType"] = static_cast<int>(impl_->shapeType_);
  obj["shapeWidth"] = impl_->width_;
  obj["shapeHeight"] = impl_->height_;
  obj["fillR"] = static_cast<double>(impl_->fillColor_.r());
  obj["fillG"] = static_cast<double>(impl_->fillColor_.g());
  obj["fillB"] = static_cast<double>(impl_->fillColor_.b());
  obj["fillA"] = static_cast<double>(impl_->fillColor_.a());
  obj["fillEnabled"] = impl_->fillEnabled_;
  obj["fillType"] = static_cast<int>(impl_->fillType_);
  obj["fillGradStartR"] = static_cast<double>(impl_->fillGradientStartColor_.r());
  obj["fillGradStartG"] = static_cast<double>(impl_->fillGradientStartColor_.g());
  obj["fillGradStartB"] = static_cast<double>(impl_->fillGradientStartColor_.b());
  obj["fillGradStartA"] = static_cast<double>(impl_->fillGradientStartColor_.a());
  obj["fillGradEndR"] = static_cast<double>(impl_->fillGradientEndColor_.r());
  obj["fillGradEndG"] = static_cast<double>(impl_->fillGradientEndColor_.g());
  obj["fillGradEndB"] = static_cast<double>(impl_->fillGradientEndColor_.b());
  obj["fillGradEndA"] = static_cast<double>(impl_->fillGradientEndColor_.a());
  obj["fillGradAngle"] = static_cast<double>(impl_->fillGradientAngleDegrees_);
  obj["fillGradCenterX"] = static_cast<double>(impl_->fillGradientCenterX_);
  obj["fillGradCenterY"] = static_cast<double>(impl_->fillGradientCenterY_);
  obj["fillGradRadius"] = static_cast<double>(impl_->fillGradientRadius_);
  obj["strokeR"] = static_cast<double>(impl_->strokeColor_.r());
  obj["strokeG"] = static_cast<double>(impl_->strokeColor_.g());
  obj["strokeB"] = static_cast<double>(impl_->strokeColor_.b());
  obj["strokeA"] = static_cast<double>(impl_->strokeColor_.a());
  obj["strokeWidth"] = static_cast<double>(impl_->strokeWidth_);
  obj["strokeEnabled"] = impl_->strokeEnabled_;
  obj["strokeTaperStart"] = static_cast<double>(impl_->strokeTaperStart_);
  obj["strokeTaperEnd"] = static_cast<double>(impl_->strokeTaperEnd_);
  obj["strokeGradientEnabled"] = impl_->strokeGradientEnabled_;
  obj["strokeGradientStartR"] = static_cast<double>(impl_->strokeGradientStartColor_.r());
  obj["strokeGradientStartG"] = static_cast<double>(impl_->strokeGradientStartColor_.g());
  obj["strokeGradientStartB"] = static_cast<double>(impl_->strokeGradientStartColor_.b());
  obj["strokeGradientStartA"] = static_cast<double>(impl_->strokeGradientStartColor_.a());
  obj["strokeGradientEndR"] = static_cast<double>(impl_->strokeGradientEndColor_.r());
  obj["strokeGradientEndG"] = static_cast<double>(impl_->strokeGradientEndColor_.g());
  obj["strokeGradientEndB"] = static_cast<double>(impl_->strokeGradientEndColor_.b());
  obj["strokeGradientEndA"] = static_cast<double>(impl_->strokeGradientEndColor_.a());
  obj["strokeCap"] = static_cast<int>(impl_->strokeCap_);
  obj["strokeJoin"] = static_cast<int>(impl_->strokeJoin_);
  obj["strokeAlign"] = static_cast<int>(impl_->strokeAlign_);
  obj["dashPattern"] = dashPatternToString(impl_->dashPattern_);
  obj["cornerRadius"] = static_cast<double>(impl_->cornerRadius_);
  obj["starPoints"] = impl_->starPoints_;
  obj["starInnerRadius"] = static_cast<double>(impl_->starInnerRadius_);
  obj["polygonSides"] = impl_->polygonSides_;
  obj["customPolygonClosed"] = impl_->customPolygonClosed_;
  QJsonArray customPolygonPoints;
  for (const auto& point : impl_->customPolygonPoints_) {
   QJsonObject p;
   p["x"] = point.x();
   p["y"] = point.y();
   customPolygonPoints.push_back(p);
  }
  obj["customPolygonPoints"] = customPolygonPoints;
  // Phase 5: bezier path
  obj["customPathClosed"] = impl_->customPathClosed_;
  obj["customPathFillRule"] = static_cast<int>(impl_->customPathFillRule_);
  QJsonArray customPath;
  for (const auto& v : impl_->customPathVertices_) {
   QJsonObject vObj;
   vObj["px"] = v.pos.x();    vObj["py"] = v.pos.y();
   vObj["ix"] = v.inTangent.x(); vObj["iy"] = v.inTangent.y();
   vObj["ox"] = v.outTangent.x(); vObj["oy"] = v.outTangent.y();
   vObj["smooth"] = v.smooth;
   customPath.push_back(vObj);
  }
  obj["customPath"] = customPath;
  QJsonArray operators;
  for (const auto &op : impl_->shapeOperators_) {
    QJsonObject opObj = op->toJson();
    opObj["type"] = static_cast<int>(op->type());
    operators.push_back(opObj);
  }
  obj["shapeOperators"] = operators;
  return obj;
}

SharedPtr<ArtifactShapeLayer> ArtifactShapeLayer::fromJson(const QJsonObject &obj) {
  auto layer = ArtifactCore::makeShared<ArtifactShapeLayer>();
  layer->ArtifactAbstract2DLayer::fromJsonProperties(obj);
  layer->setShapeType(static_cast<Artifact::ShapeType>(obj["shapeType"].toInt()));
  layer->setSize(obj["shapeWidth"].toInt(200), obj["shapeHeight"].toInt(200));
  layer->setFillColor(FloatColor(
      static_cast<float>(obj["fillR"].toDouble(1.0)),
      static_cast<float>(obj["fillG"].toDouble(1.0)),
      static_cast<float>(obj["fillB"].toDouble(1.0)),
      static_cast<float>(obj["fillA"].toDouble(1.0))));
  layer->setFillEnabled(obj["fillEnabled"].toBool(true));
  layer->setFillType(static_cast<ArtifactSolidFillType>(obj["fillType"].toInt(0)));
  layer->setFillGradientStartColor(FloatColor(
      static_cast<float>(obj["fillGradStartR"].toDouble(1.0)),
      static_cast<float>(obj["fillGradStartG"].toDouble(1.0)),
      static_cast<float>(obj["fillGradStartB"].toDouble(1.0)),
      static_cast<float>(obj["fillGradStartA"].toDouble(1.0))));
  layer->setFillGradientEndColor(FloatColor(
      static_cast<float>(obj["fillGradEndR"].toDouble(0.0)),
      static_cast<float>(obj["fillGradEndG"].toDouble(0.0)),
      static_cast<float>(obj["fillGradEndB"].toDouble(0.0)),
      static_cast<float>(obj["fillGradEndA"].toDouble(1.0))));
  layer->setFillGradientAngleDegrees(static_cast<float>(obj["fillGradAngle"].toDouble(0.0)));
  layer->setFillGradientCenterX(static_cast<float>(obj["fillGradCenterX"].toDouble(0.5)));
  layer->setFillGradientCenterY(static_cast<float>(obj["fillGradCenterY"].toDouble(0.5)));
  layer->setFillGradientRadius(static_cast<float>(obj["fillGradRadius"].toDouble(0.5)));
  layer->setStrokeColor(FloatColor(
      static_cast<float>(obj["strokeR"].toDouble(0.0)),
      static_cast<float>(obj["strokeG"].toDouble(0.0)),
      static_cast<float>(obj["strokeB"].toDouble(0.0)),
      static_cast<float>(obj["strokeA"].toDouble(1.0))));
  layer->setStrokeWidth(static_cast<float>(obj["strokeWidth"].toDouble(0.0)));
  layer->setStrokeEnabled(obj["strokeEnabled"].toBool(false));
  layer->setStrokeTaper(
      static_cast<float>(obj["strokeTaperStart"].toDouble(1.0)),
      static_cast<float>(obj["strokeTaperEnd"].toDouble(1.0)));
  layer->setStrokeGradientEnabled(obj["strokeGradientEnabled"].toBool(false));
  layer->setStrokeGradientStartColor(FloatColor(
      static_cast<float>(
          obj["strokeGradientStartR"].toDouble(layer->strokeColor().r())),
      static_cast<float>(
          obj["strokeGradientStartG"].toDouble(layer->strokeColor().g())),
      static_cast<float>(
          obj["strokeGradientStartB"].toDouble(layer->strokeColor().b())),
      static_cast<float>(
          obj["strokeGradientStartA"].toDouble(layer->strokeColor().a()))));
  layer->setStrokeGradientEndColor(FloatColor(
      static_cast<float>(
          obj["strokeGradientEndR"].toDouble(layer->strokeColor().r())),
      static_cast<float>(
          obj["strokeGradientEndG"].toDouble(layer->strokeColor().g())),
      static_cast<float>(
          obj["strokeGradientEndB"].toDouble(layer->strokeColor().b())),
      static_cast<float>(
          obj["strokeGradientEndA"].toDouble(layer->strokeColor().a()))));
  layer->setStrokeCap(static_cast<StrokeCap>(obj["strokeCap"].toInt(0)));
  layer->setStrokeJoin(static_cast<StrokeJoin>(obj["strokeJoin"].toInt(0)));
  layer->setStrokeAlign(static_cast<StrokeAlign>(obj["strokeAlign"].toInt(0)));
  layer->setDashPattern(stringToDashPattern(obj["dashPattern"].toString()));
  layer->setCornerRadius(static_cast<float>(obj["cornerRadius"].toDouble(0.0)));
  layer->setStarPoints(obj["starPoints"].toInt(5));
  layer->setStarInnerRadius(
      static_cast<float>(obj["starInnerRadius"].toDouble(0.382)));
  layer->setPolygonSides(obj["polygonSides"].toInt(6));
  layer->impl_->customPolygonClosed_ = obj["customPolygonClosed"].toBool(true);
  layer->impl_->customPolygonPoints_.clear();
  const QJsonArray customPolygonPoints = obj["customPolygonPoints"].toArray();
  const int polygonPointCount = std::min(
      static_cast<int>(customPolygonPoints.size()), kMaxShapePathVertices);
  layer->impl_->customPolygonPoints_.reserve(polygonPointCount);
  for (int pointIndex = 0; pointIndex < polygonPointCount; ++pointIndex) {
    const auto value = customPolygonPoints.at(pointIndex);
    const QJsonObject p = value.toObject();
    const double x = p["x"].toDouble();
    const double y = p["y"].toDouble();
    const QPointF point(x, y);
    if (isSupportedShapePoint(point)) {
      layer->impl_->customPolygonPoints_.push_back(point);
    }
  }
  // Phase 5: bezier path (takes priority over customPolygon)
  const QJsonArray customPathArr = obj["customPath"].toArray();
  if (customPathArr.size() >= 3) {
    layer->impl_->customPathClosed_ = obj["customPathClosed"].toBool(true);
    layer->impl_->customPathFillRule_ =
        obj["customPathFillRule"].toInt(0) ==
                static_cast<int>(ArtifactCore::PathFillRule::EvenOdd)
            ? ArtifactCore::PathFillRule::EvenOdd
            : ArtifactCore::PathFillRule::Winding;
    layer->impl_->customPathVertices_.clear();
    const int pathVertexCount = std::min(
        static_cast<int>(customPathArr.size()), kMaxShapePathVertices);
    layer->impl_->customPathVertices_.reserve(pathVertexCount);
    for (int vertexIndex = 0; vertexIndex < pathVertexCount; ++vertexIndex) {
      const auto val = customPathArr.at(vertexIndex);
      const QJsonObject vObj = val.toObject();
      CustomPathVertex v;
      v.pos = QPointF(vObj["px"].toDouble(), vObj["py"].toDouble());
      v.inTangent = QPointF(vObj["ix"].toDouble(), vObj["iy"].toDouble());
      v.outTangent = QPointF(vObj["ox"].toDouble(), vObj["oy"].toDouble());
      v.smooth = vObj["smooth"].toBool(false);
      if (isSupportedCustomPathVertex(v)) {
        layer->impl_->customPathVertices_.push_back(v);
      }
    }
    if (layer->impl_->customPathVertices_.size() >= 3) {
      layer->impl_->customPolygonPoints_.clear(); // mutual exclusion
    } else {
      // Keep a valid polygon when the serialized path was malformed.
      layer->impl_->customPathVertices_.clear();
    }
  }
  const QJsonArray operators = obj["shapeOperators"].toArray();
  layer->impl_->shapeOperators_.clear();
  const int operatorCount = std::min(static_cast<int>(operators.size()), 128);
  layer->impl_->shapeOperators_.reserve(operatorCount);
  for (int operatorIndex = 0; operatorIndex < operatorCount; ++operatorIndex) {
    const auto val = operators.at(operatorIndex);
    const QJsonObject opObj = val.toObject();
    const auto type = static_cast<ArtifactCore::ShapeOperatorType>(
        opObj.value(QStringLiteral("type")).toInt(0));
    auto op = createShapeOperator(type);
    if (op) {
      op->fromJson(opObj);
      normalizeRestoredShapeOperator(op.get());
       layer->impl_->shapeOperators_.push_back(std::move(op));
    }
  }
  layer->impl_->markDirty();
  return layer;
}

void ArtifactShapeLayer::restoreOperatorsFromJson(const QJsonArray& operators)
{
  if (!impl_) {
    return;
  }
  impl_->shapeOperators_.clear();
  const int operatorCount = std::min(static_cast<int>(operators.size()), 128);
  impl_->shapeOperators_.reserve(operatorCount);
  for (int operatorIndex = 0; operatorIndex < operatorCount; ++operatorIndex) {
    const auto val = operators.at(operatorIndex);
    const QJsonObject opObj = val.toObject();
    const auto type = static_cast<ArtifactCore::ShapeOperatorType>(
        opObj.value(QStringLiteral("type")).toInt(0));
    auto op = createShapeOperator(type);
    if (op) {
      op->fromJson(opObj);
      normalizeRestoredShapeOperator(op.get());
      impl_->shapeOperators_.push_back(std::move(op));
    }
  }
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}

};
