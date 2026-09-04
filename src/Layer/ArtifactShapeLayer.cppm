module;
#include <cstddef>
#include <map>
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
#include <QFile>
#include <QXmlStreamReader>
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
ArtifactCore::RationalTime effectiveShapeTimelineTime(const Artifact::ArtifactShapeLayer* layer) {
 int64_t frame = 0;
 int64_t fps = 30;
 if (layer) {
  if (auto* composition = dynamic_cast<Artifact::ArtifactAbstractComposition*>(
          layer->compositionObject())) {
   frame = composition->framePosition().framePosition();
   const double rawFps = composition->frameRate().framerate();
   fps = std::isfinite(rawFps) && rawFps > 0.0
             ? std::max<int64_t>(
                   1, static_cast<int64_t>(std::llround(
                          std::clamp(rawFps, 1.0, 10000.0))))
             : 30;
  } else {
   frame = layer->currentFrame();
  }
 }
 return ArtifactCore::RationalTime(frame, fps);
}

// Evaluate an animatable shape.* numeric property at the current time.
// Falls back to the stored member value when the property has no keyframes.
double animatedShapeNumber(const Artifact::ArtifactShapeLayer* layer,
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

bool hasAnimatedShapeGeometry(const Artifact::ArtifactShapeLayer* layer) {
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

} // namespace

namespace Artifact {
bool isSupportedCustomPathVertex(const CustomPathVertex& vertex);
}

// Path keyframe animation: vertices are stored as a JSON document on the
// animatable layer property "shape.path.keyframes". Each entry maps a frame
// number to a serialized CustomPathVertex array. Between frames, matching
// vertices are interpolated (position and tangents) with linear blending.
namespace {

using Artifact::CustomPathVertex;

QJsonArray serializePathVertices(const std::vector<CustomPathVertex>& verts) {
 QJsonArray arr;
 for (const auto& v : verts) {
  QJsonObject o;
  o["px"] = v.pos.x();   o["py"] = v.pos.y();
  o["ix"] = v.inTangent.x(); o["iy"] = v.inTangent.y();
  o["ox"] = v.outTangent.x(); o["oy"] = v.outTangent.y();
  o["smooth"] = v.smooth;
  arr.push_back(o);
 }
 return arr;
}

std::vector<CustomPathVertex> deserializePathVertices(const QJsonArray& arr) {
 std::vector<CustomPathVertex> verts;
 verts.reserve(static_cast<size_t>(arr.size()));
 for (const auto& val : arr) {
  const QJsonObject o = val.toObject();
  CustomPathVertex v;
  v.pos = QPointF(o["px"].toDouble(), o["py"].toDouble());
  v.inTangent = QPointF(o["ix"].toDouble(), o["iy"].toDouble());
  v.outTangent = QPointF(o["ox"].toDouble(), o["oy"].toDouble());
  v.smooth = o["smooth"].toBool(false);
  if (Artifact::isSupportedCustomPathVertex(v)) {
   verts.push_back(v);
  }
 }
 return verts;
}

CustomPathVertex lerpPathVertex(const CustomPathVertex& a,
                                const CustomPathVertex& b, double t) {
 CustomPathVertex out;
 out.pos = a.pos + (b.pos - a.pos) * t;
 out.inTangent =
     a.inTangent + (b.inTangent - a.inTangent) * t;
 out.outTangent =
     a.outTangent + (b.outTangent - a.outTangent) * t;
 out.smooth = t < 0.5 ? a.smooth : b.smooth;
 return out;
}

} // namespace

namespace Artifact {

static ArtifactCore::ShapePath buildShapePath(Artifact::ShapeType shapeType,
                                int width,
                                int height,
                                float cornerRadius,
                                int starPoints,
                                float starInnerRadius,
                                int polygonSides);

void ArtifactShapeLayer::setPathKeyframe(int64_t frame,
                                         const std::vector<CustomPathVertex>& verts) {
 auto property = getProperty(QStringLiteral("shape.path.keyframes"));
 if (!property) {
  property = persistentLayerProperty(
      QStringLiteral("shape.path.keyframes"),
      ArtifactCore::PropertyType::String, QString{}, -190);
 }
 property->setAnimatable(true);
 QJsonDocument doc =
     QJsonDocument::fromJson(
         property->getValue().toString().toUtf8());
 QJsonObject root = doc.object();
 root[QString::number(static_cast<qint64>(frame))] =
     serializePathVertices(verts);
 property->setValue(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
 Q_EMIT changed();
}

bool ArtifactShapeLayer::hasPathKeyframes() const {
 auto property = getProperty(QStringLiteral("shape.path.keyframes"));
 if (!property) {
  return false;
 }
 // The property itself is the animation carrier; presence of any stored
 // frame marks an animated path. Keyframes live on this dedicated property
 // so the timeline can treat it like any other animatable channel.
 return !property->getValue().toString().isEmpty() ||
        !property->getKeyFrames().empty();
}

std::vector<CustomPathVertex> ArtifactShapeLayer::evaluatePathAt(int64_t frame) const {
 if (!hasPathKeyframes()) {
  return customPathVertices();
 }
 auto property = getProperty(QStringLiteral("shape.path.keyframes"));
 if (!property) {
  return customPathVertices();
 }
 QJsonDocument doc =
     QJsonDocument::fromJson(
         property->getValue().toString().toUtf8());
 const QJsonObject root = doc.object();
 if (root.isEmpty()) {
  return customPathVertices();
 }
 std::map<int64_t, std::vector<CustomPathVertex>> samples;
 for (auto it = root.begin(); it != root.end(); ++it) {
  const int64_t sampleFrame = static_cast<int64_t>(it.key().toLongLong());
  samples[sampleFrame] =
      deserializePathVertices(it.value().toArray());
 }
 if (samples.empty()) {
  return customPathVertices();
 }
 auto lower = samples.lower_bound(frame);
 if (lower != samples.end() && lower->first == frame) {
  return lower->second;
 }
 auto upper = lower;
 if (lower == samples.begin()) {
  return lower->second; // Before first key: hold.
 }
 --lower;
 if (upper == samples.end()) {
  return lower->second; // After last key: hold.
 }
 const int64_t f0 = lower->first;
 const int64_t f1 = upper->first;
 if (f1 <= f0) {
  return lower->second;
 }
 const double t = static_cast<double>(frame - f0) /
                  static_cast<double>(f1 - f0);
 const auto& a = lower->second;
 const auto& b = upper->second;
 if (a.size() != b.size()) {
  // Topology change between keys: snap instead of blending mismatched sets.
  return t < 0.5 ? a : b;
 }
 std::vector<CustomPathVertex> blended;
 blended.reserve(a.size());
 for (size_t i = 0; i < a.size(); ++i) {
  blended.push_back(lerpPathVertex(a[i], b[i], t));
 }
 return blended;
}

// True when any shape operator parameter has keyframes. Animated operator
// parameters must bypass the geometry caches so TrimPaths / Repeater values
// evaluate per frame.
bool hasAnimatedShapeOperators(const Artifact::ArtifactShapeLayer* layer) {
 if (!layer) {
  return false;
 }
 for (int i = 0; i < layer->shapeOperatorCount(); ++i) {
  const QString prefix =
      QStringLiteral("shape.operator.%1.").arg(i);
  static const char* kFloatFields[] = {"start", "end", "offset", "copies",
                                       "rotation", "amount", "radius",
                                       "startOpacity", "endOpacity"};
  for (const char* field : kFloatFields) {
   const auto property =
       layer->getProperty(prefix + QString::fromLatin1(field));
   if (property && !property->getKeyFrames().empty()) {
    return true;
   }
  }
  const auto modeProperty = layer->getProperty(
      prefix + QStringLiteral("mode"));
  if (modeProperty && !modeProperty->getKeyFrames().empty()) {
   return true;
  }
 }
 return false;
}

// Apply keyframed operator parameters onto the operator instances before
// path processing. Static values stay as authored via setLayerPropertyValue.
void applyAnimatedOperatorParameters(const Artifact::ArtifactShapeLayer* layer,
                                     std::vector<
                                         std::unique_ptr<ArtifactCore::ShapeOperator>>&
                                         operators) {
 if (!layer) {
  return;
 }
 for (int i = 0; i < static_cast<int>(operators.size()); ++i) {
  ArtifactCore::ShapeOperator& op = *operators[static_cast<size_t>(i)];
  const QString prefix = QStringLiteral("shape.operator.%1.").arg(i);
  auto applyFloat = [&](const char* field, auto setter) {
   const auto property =
       layer->getProperty(prefix + QString::fromLatin1(field));
   if (property && !property->getKeyFrames().empty()) {
    const QVariant value =
        property->interpolateValue(effectiveShapeTimelineTime(layer));
    if (value.isValid()) {
     setter(value.toDouble());
    }
   }
  };
  if (auto* trim = dynamic_cast<ArtifactCore::TrimPaths*>(&op)) {
   applyFloat("start", [&](double v) { trim->setStart(static_cast<float>(v)); });
   applyFloat("end", [&](double v) { trim->setEnd(static_cast<float>(v)); });
   applyFloat("offset", [&](double v) { trim->setOffset(static_cast<float>(v)); });
  } else if (auto* merge = dynamic_cast<ArtifactCore::MergePaths*>(&op)) {
   const auto modeProperty = layer->getProperty(
       prefix + QStringLiteral("mode"));
   if (modeProperty && !modeProperty->getKeyFrames().empty()) {
    const QVariant value =
        modeProperty->interpolateValue(effectiveShapeTimelineTime(layer));
    if (value.isValid()) {
     merge->setMode(std::clamp(value.toInt(), 0, 4));
    }
   }
  } else if (auto* repeater = dynamic_cast<ArtifactCore::Repeater*>(&op)) {
   applyFloat("offset",
              [&](double v) { repeater->setOffset(static_cast<float>(v)); });
   applyFloat("rotation",
              [&](double v) { repeater->setRotation(static_cast<float>(v)); });
   applyFloat("startOpacity", [&](double v) {
    repeater->setStartOpacity(static_cast<float>(v));
   });
   applyFloat("endOpacity", [&](double v) {
    repeater->setEndOpacity(static_cast<float>(v));
   });
   const auto copiesProperty =
       layer->getProperty(prefix + QStringLiteral("copies"));
   if (copiesProperty && !copiesProperty->getKeyFrames().empty()) {
    const QVariant value =
        copiesProperty->interpolateValue(effectiveShapeTimelineTime(layer));
    if (value.isValid()) {
     repeater->setCopies(std::clamp(value.toInt(), 1, 1000));
    }
   }
  } else if (auto* offsetOp = dynamic_cast<ArtifactCore::OffsetPaths*>(&op)) {
   applyFloat("offset", [&](double v) {
    offsetOp->setOffset(
        std::isfinite(static_cast<float>(v))
            ? std::clamp(static_cast<float>(v), -100000.0f, 100000.0f)
            : 0.0f);
   });
  } else if (auto* pucker = dynamic_cast<ArtifactCore::PuckerBloat*>(&op)) {
   applyFloat("amount", [&](double v) {
    pucker->setAmount(static_cast<float>(v));
   });
  } else if (auto* rounded = dynamic_cast<ArtifactCore::RoundedCorners*>(&op)) {
   applyFloat("radius", [&](double v) {
    rounded->setRadius(static_cast<float>(v));
   });
  }
 }
}

ShapeGeomDims resolveShapeGeomDims(const Artifact::ArtifactShapeLayer* layer,
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
  } else if (auto merge = dynamic_cast<ArtifactCore::MergePaths *>(op)) {
    merge->setMode(std::clamp(merge->mode(), 0, 4));
  }
}

static std::vector<ArtifactCore::ShapePath> applyShapeOperators(
    const ArtifactCore::ShapePath& basePath,
    const std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>& operators)
{
 if (operators.empty()) {
  return {basePath};
 }

 bool hasMergePaths = false;
 for (const auto& op : operators) {
  if (op && op->type() == ArtifactCore::ShapeOperatorType::MergePaths) {
   hasMergePaths = true;
   break;
  }
 }

 ArtifactCore::ShapeGroup group;
 const auto inputPaths = hasMergePaths ? basePath.subpaths()
                                       : std::vector<ArtifactCore::ShapePath>{basePath};
 if (inputPaths.empty()) {
  return {};
 }
 for (const auto& inputPath : inputPaths) {
  group.addChild(std::make_unique<ArtifactCore::PathShape>(inputPath));
 }
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

// ---- Multi-content + GPU-native helpers (gaps 1 & 3) ----
// All painting stays on existing renderer primitives
// (drawSolidTriangleLocal / drawThickLineLocal / drawStyledPolyline).
// No new shader, signal, QImage, or QPainter paths are introduced here.

static ArtifactCore::ShapePath buildContentShapePath(const Artifact::ShapeContent& content) {
  const auto& g = content.geometry;
  const int w = std::clamp(g.width, 1, kMaxShapeDimension);
  const int h = std::clamp(g.height, 1, kMaxShapeDimension);
  // Contents accept 2-vertex open paths (e.g. imported <line>) as
  // stroke-only geometry; triangulation of open paths is empty by design.
  if (g.pathVertices.size() >= 2) {
    ArtifactCore::ShapePath path = buildCustomShapePath(g.pathVertices, g.pathClosed);
    path.setFillRule(g.fillRule);
    return path;
  }
  if (g.polygonPoints.size() >= 3) {
    ArtifactCore::ShapePath sp;
    sp.setPolygon(g.polygonPoints, g.polygonClosed);
    return sp;
  }
  return buildShapePath(g.type, w, h,
                        std::isfinite(g.cornerRadius) ? g.cornerRadius : 0.0f,
                        std::clamp(g.starPoints, 3, kMaxStarPoints),
                        std::clamp(g.starInnerRadius, 0.0f, 1.0f),
                        std::clamp(g.polygonSides, 3, 100));
}

static QPainterPath contentPathsToPainter(const std::vector<ArtifactCore::ShapePath>& paths) {
  QPainterPath out;
  for (const auto& p : paths) {
    out.addPath(p.toPainterPath());
  }
  return out;
}

static std::vector<ArtifactCore::ShapePath> painterToContentPaths(const QPainterPath& painter) {
  if (painter.isEmpty()) {
    return {};
  }
  const QPainterPath simplified = painter.simplified();
  if (simplified.isEmpty()) {
    return {};
  }
  return ArtifactCore::ShapePath::fromPainterPath(simplified).subpaths();
}

// Gradient parameter t in [0,1] for a local point. Fill type ints match
// ArtifactSolidFillType: 0=Solid 1=Linear 2=Radial 3=Conical 4=Repeating 5=Mirrored.
static float contentGradientT(const Artifact::ShapeContentFill& fill,
                              float x, float y, float w, float h) {
  const float cx = w * std::clamp(fill.gradientCenterX, 0.0f, 1.0f);
  const float cy = h * std::clamp(fill.gradientCenterY, 0.0f, 1.0f);
  const int type = static_cast<int>(fill.type);
  if (type == 2) {
    const float radius = std::max(w, h) * std::max(fill.gradientRadius, 0.0001f);
    const float dx = x - cx;
    const float dy = y - cy;
    return std::clamp(std::sqrt(dx * dx + dy * dy) / radius, 0.0f, 1.0f);
  }
  if (type == 3) {
    const float ang = std::atan2(y - cy, x - cx) * 180.0f / 3.14159265f;
    float t = (ang - fill.gradientAngleDegrees) / 360.0f;
    t = t - std::floor(t);
    return std::clamp(t, 0.0f, 1.0f);
  }
  const float rad = fill.gradientAngleDegrees * 3.14159265f / 180.0f;
  const float half = 0.5f * std::sqrt(w * w + h * h);
  float t = (half <= 0.0001f)
      ? 0.0f
      : 0.5f + ((x - cx) * std::cos(rad) + (y - cy) * std::sin(rad)) / (2.0f * half);
  if (type == 4) {
    t = t - std::floor(t);
  } else if (type == 5) {
    const float m = t - std::floor(t / 2.0f) * 2.0f;
    t = (m > 1.0f) ? 2.0f - m : m;
  } else {
    t = std::clamp(t, 0.0f, 1.0f);
  }
  return std::clamp(t, 0.0f, 1.0f);
}

static ArtifactCore::FloatColor contentGradientColorAt(const Artifact::ShapeContentFill& fill,
                                                       float x, float y, float w, float h) {
  if (fill.type == ArtifactSolidFillType::Solid) {
    return fill.color;
  }
  return mixColor(fill.gradientStart, fill.gradientEnd,
                  contentGradientT(fill, x, y, w, h));
}

// Segmented variable-width / gradient-color stroker for the GPU path.
// Taper and along-path gradient cannot be expressed by PolylineStyle,
// so each flattened segment is drawn with its midpoint width/color.
// Dash + taper/gradient stays on drawStyledPolyline (dash wins).
static void drawTaperedPolylineGPU(Artifact::ArtifactIRenderer* renderer,
                                   const std::vector<Artifact::Detail::float2>& points,
                                   bool closed,
                                   float width,
                                   float taperStart,
                                   float taperEnd,
                                   const ArtifactCore::FloatColor& gradStart,
                                   const ArtifactCore::FloatColor& gradEnd,
                                   bool gradientEnabled,
                                   const ArtifactCore::FloatColor& baseColor,
                                   Artifact::StrokeCap cap) {
  if (!renderer || points.size() < 2 || !std::isfinite(width) || width <= 0.0f) {
    return;
  }
  const size_t segmentCount = closed ? points.size() : (points.size() - 1);
  if (segmentCount == 0) {
    return;
  }
  std::vector<double> cumulative;
  cumulative.reserve(segmentCount + 1);
  cumulative.push_back(0.0);
  double total = 0.0;
  for (size_t i = 0; i < segmentCount; ++i) {
    const auto& p0 = points[i % points.size()];
    const auto& p1 = points[(i + 1) % points.size()];
    const double dx = static_cast<double>(p1.x) - p0.x;
    const double dy = static_cast<double>(p1.y) - p0.y;
    total += std::sqrt(dx * dx + dy * dy);
    cumulative.push_back(total);
  }
  if (total <= 1e-6) {
    return;
  }
  const float t0 = std::clamp(taperStart, 0.0f, 1.0f);
  const float t1 = std::clamp(taperEnd, 0.0f, 1.0f);
  for (size_t i = 0; i < segmentCount; ++i) {
    const float midT = static_cast<float>((cumulative[i] + cumulative[i + 1]) * 0.5 / total);
    const float segWidth = std::max(0.0f, width * (t0 + (t1 - t0) * midT));
    if (segWidth <= 0.0f) {
      continue;
    }
    const ArtifactCore::FloatColor segColor = gradientEnabled
        ? mixColor(gradStart, gradEnd, midT)
        : baseColor;
    auto p0 = points[i % points.size()];
    auto p1 = points[(i + 1) % points.size()];
    if (!closed && cap == Artifact::StrokeCap::Square) {
      const float dx = p1.x - p0.x;
      const float dy = p1.y - p0.y;
      const float len = std::sqrt(dx * dx + dy * dy);
      if (len > 1e-6f) {
        const float ex = dx / len * segWidth * 0.5f;
        const float ey = dy / len * segWidth * 0.5f;
        if (i == 0) {
          p0.x -= ex;
          p0.y -= ey;
        }
        if (i + 1 == segmentCount) {
          p1.x += ex;
          p1.y += ey;
        }
      }
    }
    renderer->drawThickLineLocal(p0, p1, segWidth, segColor);
  }
}

// Inside/Outside strokes without a clip path: stroke a path offset by
// half the width so a center stroke lands exactly inside/outside.
// Closed paths only; open paths and Line shapes keep center strokes.
static ArtifactCore::ShapePath strokeAlignedPath(const ArtifactCore::ShapePath& path,
                                                Artifact::StrokeAlign align,
                                                float width) {
  if (align == Artifact::StrokeAlign::Center || !std::isfinite(width) || width <= 0.0f) {
    return path.clone();
  }
  if (path.isEmpty() || !path.isClosed()) {
    return path.clone();
  }
  const double delta = static_cast<double>(width) * 0.5 *
      (align == Artifact::StrokeAlign::Inside ? -1.0 : 1.0);
  ArtifactCore::ShapePath offset = path.offsetPath(delta, 128);
  if (offset.isEmpty()) {
    return path.clone();
  }
  offset.setFillRule(path.fillRule());
  return offset;
}

// Resolve per-content visible paths after merge modes.
// Subtract removes itself and punches holes in contents below it.
// Intersect keeps the overlap with the union below. Difference (XOR with
// top-wins paint) repaints the symmetric difference with its own style.
static std::vector<std::vector<ArtifactCore::ShapePath>> resolveContentVisPaths(
    const std::vector<std::vector<ArtifactCore::ShapePath>>& processed,
    const std::vector<Artifact::ShapeContent>& contents) {
  std::vector<std::vector<ArtifactCore::ShapePath>> vis = processed;
  if (processed.empty()) {
    return vis;
  }
  std::vector<QPainterPath> geom(processed.size());
  for (size_t i = 0; i < processed.size(); ++i) {
    geom[i] = contentPathsToPainter(processed[i]);
  }
  for (size_t j = 0; j < contents.size() && j < vis.size(); ++j) {
    const int mode = static_cast<int>(contents[j].merge);
    if (mode == static_cast<int>(Artifact::ShapeContentMerge::Subtract)) {
      if (geom[j].isEmpty()) {
        vis[j].clear();
        continue;
      }
      for (size_t k = 0; k < j; ++k) {
        if (geom[k].isEmpty()) {
          continue;
        }
        geom[k] = geom[k].subtracted(geom[j]).simplified();
        vis[k] = painterToContentPaths(geom[k]);
      }
      vis[j].clear();
      geom[j] = QPainterPath();
    } else if (mode == static_cast<int>(Artifact::ShapeContentMerge::Intersect)) {
      QPainterPath below;
      for (size_t k = 0; k < j; ++k) {
        below.addPath(geom[k]);
      }
      geom[j] = geom[j].intersected(below).simplified();
      vis[j] = painterToContentPaths(geom[j]);
    } else if (mode == static_cast<int>(Artifact::ShapeContentMerge::Difference)) {
      QPainterPath below;
      for (size_t k = 0; k < j; ++k) {
        below.addPath(geom[k]);
      }
      const QPainterPath uni = geom[j].united(below).simplified();
      const QPainterPath inter = geom[j].intersected(below).simplified();
      geom[j] = uni.subtracted(inter).simplified();
      vis[j] = painterToContentPaths(geom[j]);
    }
  }
  return vis;
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
  float dashOffset_ = 0.0f;

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

  // Gap 1: multi-content model. Empty = legacy single-primitive behavior.
   std::vector<Artifact::ShapeContent> shapeContents_;
   int activeContentIndex_ = -1;
   struct ContentDrawCache {
   std::vector<std::vector<ArtifactCore::ShapePath>> visPaths;
   bool valid = false;
  };
  ContentDrawCache contentCache_;

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
     contentCache_.valid = false;
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

   const NativePathGeometry& shapeGeometry(
       const ShapeGeomDims& dims, double tolerance,
       const std::vector<CustomPathVertex>* pathVertices = nullptr) {
    if (!pathVertices && !shapeGeometryCacheDirty_ && cachedShapeGeometryDimsValid_ &&
        sameShapeGeomDims(cachedShapeGeometryDims_, dims) &&
        std::abs(cachedShapeTolerance_ - tolerance) < 1.0e-9) {
     return cachedShapeGeometry_;
    }
    const auto& effectivePathVertices = pathVertices ? *pathVertices
                                                     : customPathVertices_;
    ShapePath path = buildLayerShapePath(
        shapeType_, dims.width, dims.height, dims.cornerRadius,
        dims.starPoints, dims.starInnerRadius, dims.polygonSides,
        customPolygonPoints_, customPolygonClosed_,
        effectivePathVertices, customPathClosed_);
    if (effectivePathVertices.size() >= 3) {
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
  void rebuildCache(
      const ShapeGeomDims* dims = nullptr,
      const std::vector<CustomPathVertex>* pathVertices = nullptr) {
    if (!cacheDirty_ && !dims && !pathVertices) return;
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

    const auto& effectivePathVertices = pathVertices ? *pathVertices
                                                     : customPathVertices_;
    const auto paths = buildProcessedPainterPaths(shapeType_, effW, effH,
                                                  effCornerRadius, effStarPoints,
                                                  effStarInnerRadius, effPolygonSides,
                                                  customPolygonPoints_, customPolygonClosed_,
                                                  effectivePathVertices, customPathClosed_,
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
        if (effectivePathVertices.size() >= 3) {
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
          pen.setDashOffset(static_cast<qreal>(dashOffset_));
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
  if (impl_->shapeType_ == Artifact::ShapeType::Line) {
   // A line is stroke-only by default; otherwise a newly created line is
   // invisible because the shared shape defaults are fill-on / stroke-off.
   impl_->fillEnabled_ = false;
   impl_->strokeEnabled_ = true;
   if (impl_->strokeWidth_ <= 0.0f) {
    impl_->strokeWidth_ = 1.0f;
   }
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
  // Multi-content layers paint through the vector path; the single-card
  // fast path cannot represent per-content styles.
  if (!impl_->shapeContents_.empty()) {
    return {};
  }
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
  if (!impl_->shapeContents_.empty()) {
    return {};
  }
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
void ArtifactShapeLayer::setDashOffset(float offset) {
  const float normalized = std::isfinite(offset)
      ? std::clamp(offset, -1000000.0f, 1000000.0f) : 0.0f;
  if (impl_->dashOffset_ == normalized) {
    return;
  }
  impl_->dashOffset_ = normalized;
  impl_->markDirty();
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
float ArtifactShapeLayer::dashOffset() const { return impl_->dashOffset_; }

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
  if (impl_ && !impl_->shapeContents_.empty()) {
    ensureContentVisPaths();
    std::vector<ArtifactCore::ShapePath> paths;
    for (const auto& vis : impl_->contentCache_.visPaths) {
      for (const auto& path : vis) {
        paths.push_back(path.clone());
      }
    }
    return paths;
  }
  // Animatable shape.* properties are evaluated at the current timeline
  // time so keyframed geometry animates during playback/rendering.
  const ShapeGeomDims dims = resolveShapeGeomDims(
     this, impl_->width_, impl_->height_, impl_->cornerRadius_,
     impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);

 // Keyframed path animation overrides the static authored vertices.
 std::vector<CustomPathVertex> pathVertices = impl_->customPathVertices_;
 bool pathClosed = impl_->customPathClosed_;
 if (hasPathKeyframes()) {
  pathVertices = evaluatePathAt(currentFrame());
 }

 return buildProcessedShapePaths(
     impl_->shapeType_, dims.width, dims.height, dims.cornerRadius,
     dims.starPoints, dims.starInnerRadius, dims.polygonSides,
     impl_->customPolygonPoints_, impl_->customPolygonClosed_,
     pathVertices, pathClosed,
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

  // Gap 1: multi-content export keeps each content's own fill/stroke.
  if (impl_ && !impl_->shapeContents_.empty()) {
    ensureContentVisPaths();
    for (size_t ci = 0; ci < impl_->shapeContents_.size(); ++ci) {
      const auto& content = impl_->shapeContents_[ci];
      if (!content.visible) {
        continue;
      }
      ArtifactCore::FillSettings cfill;
      cfill.enabled = content.fill.enabled;
      if (content.fill.type == ArtifactSolidFillType::Solid) {
        cfill.color = toQColor(content.fill.color);
      } else {
        cfill.gradientStart = toQColor(content.fill.gradientStart);
        cfill.gradientEnd = toQColor(content.fill.gradientEnd);
        cfill.gradientAngleDegrees = content.fill.gradientAngleDegrees;
        cfill.gradientCenterX = content.fill.gradientCenterX;
        cfill.gradientCenterY = content.fill.gradientCenterY;
        cfill.gradientRadiusRatio =
            std::clamp(content.fill.gradientRadius, 0.01f, 100000.0f);
        switch (content.fill.type) {
          case ArtifactSolidFillType::LinearGradient:
            cfill.type = ArtifactCore::FillSettings::FillType::Linear; break;
          case ArtifactSolidFillType::RadialGradient:
            cfill.type = ArtifactCore::FillSettings::FillType::Radial; break;
          case ArtifactSolidFillType::ConicalGradient:
            cfill.type = ArtifactCore::FillSettings::FillType::Conic; break;
          case ArtifactSolidFillType::RepeatingGradient:
            cfill.type = ArtifactCore::FillSettings::FillType::Repeating; break;
          case ArtifactSolidFillType::MirroredGradient:
            cfill.type = ArtifactCore::FillSettings::FillType::Mirrored; break;
          case ArtifactSolidFillType::Solid: break;
        }
      }
      ArtifactCore::StrokeSettings cstroke(
          toQColor(content.stroke.gradientEnabled
                       ? mixColor(content.stroke.gradientStart,
                                  content.stroke.gradientEnd, 0.5f)
                       : content.stroke.color),
          static_cast<double>(content.stroke.width));
      cstroke.dashOffset = static_cast<double>(content.stroke.dashOffset);
      cstroke.enabled = content.stroke.enabled && content.stroke.width > 0.0f;
      cstroke.placement =
          content.stroke.align == StrokeAlign::Inside
              ? ArtifactCore::StrokePlacement::Inside
              : (content.stroke.align == StrokeAlign::Outside
                     ? ArtifactCore::StrokePlacement::Outside
                     : ArtifactCore::StrokePlacement::Center);
      cstroke.taperStartScale = content.stroke.taperStart;
      cstroke.taperEndScale = content.stroke.taperEnd;
      switch (content.stroke.cap) {
        case StrokeCap::Round: cstroke.cap = ArtifactCore::LineCap::Round; break;
        case StrokeCap::Square: cstroke.cap = ArtifactCore::LineCap::Square; break;
        case StrokeCap::Flat:
        default: cstroke.cap = ArtifactCore::LineCap::Butt; break;
      }
      switch (content.stroke.join) {
        case StrokeJoin::Round: cstroke.join = ArtifactCore::LineJoin::Round; break;
        case StrokeJoin::Bevel: cstroke.join = ArtifactCore::LineJoin::Bevel; break;
        case StrokeJoin::Miter:
        default: cstroke.join = ArtifactCore::LineJoin::Miter; break;
      }
      cstroke.dashPattern.reserve(content.stroke.dashPattern.size());
      for (const float dash : content.stroke.dashPattern) {
        cstroke.dashPattern.push_back(static_cast<double>(dash));
      }
      if (ci < impl_->contentCache_.visPaths.size()) {
        for (const auto& path : impl_->contentCache_.visPaths[ci]) {
          auto shape = std::make_unique<ArtifactCore::PathShape>(path);
          shape->setFill(cfill);
          shape->setStroke(cstroke);
          group->addChild(std::move(shape));
        }
      }
    }
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
  stroke.dashOffset = static_cast<double>(impl_->dashOffset_);
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
// Multi-content API (gap 1)
// ============================================================

static Artifact::ShapeContent normalizedShapeContent(const Artifact::ShapeContent& content) {
  Artifact::ShapeContent out = content;
  auto& g = out.geometry;
  g.width = std::clamp(g.width, 1, kMaxShapeDimension);
  g.height = std::clamp(g.height, 1, kMaxShapeDimension);
  const auto pixelCount = static_cast<std::uint64_t>(g.width) *
                          static_cast<std::uint64_t>(g.height);
  if (pixelCount > kMaxShapeCachePixels) {
    const double scale = std::sqrt(static_cast<double>(kMaxShapeCachePixels) /
                                   static_cast<double>(pixelCount));
    g.width = std::max(1, static_cast<int>(std::floor(g.width * scale)));
    g.height = std::max(1, static_cast<int>(std::floor(g.height * scale)));
  }
  const int rawType = static_cast<int>(g.type);
  g.type = (rawType < static_cast<int>(Artifact::ShapeType::Rect) ||
            rawType > static_cast<int>(Artifact::ShapeType::Square))
      ? Artifact::ShapeType::Rect
      : g.type;
  g.cornerRadius = std::isfinite(g.cornerRadius)
      ? std::clamp(g.cornerRadius, 0.0f, static_cast<float>(kMaxShapeDimension)) : 0.0f;
  g.starPoints = std::clamp(g.starPoints, 3, kMaxStarPoints);
  g.starInnerRadius = std::isfinite(g.starInnerRadius)
      ? std::clamp(g.starInnerRadius, 0.0f, 1.0f) : 0.382f;
  g.polygonSides = std::clamp(g.polygonSides, 3, kMaxShapePathVertices);
  if (g.polygonPoints.size() > static_cast<size_t>(kMaxShapePathVertices)) {
    g.polygonPoints.resize(static_cast<size_t>(kMaxShapePathVertices));
  }
  {
    std::vector<QPointF> kept;
    kept.reserve(g.polygonPoints.size());
    for (const auto& p : g.polygonPoints) {
      if (isSupportedShapePoint(p)) {
        kept.push_back(p);
      }
    }
    g.polygonPoints = std::move(kept);
  }
  if (g.pathVertices.size() > static_cast<size_t>(kMaxShapePathVertices)) {
    g.pathVertices.resize(static_cast<size_t>(kMaxShapePathVertices));
  }
  {
    std::vector<Artifact::CustomPathVertex> kept;
    kept.reserve(g.pathVertices.size());
    for (const auto& v : g.pathVertices) {
      if (isSupportedCustomPathVertex(v)) {
        kept.push_back(v);
      }
    }
    g.pathVertices = std::move(kept);
  }
  out.fill.color = normalizedShapeColor(out.fill.color);
  out.fill.gradientStart = normalizedShapeColor(out.fill.gradientStart);
  out.fill.gradientEnd = normalizedShapeColor(out.fill.gradientEnd);
  {
    const int raw = std::clamp(static_cast<int>(out.fill.type),
                               static_cast<int>(ArtifactSolidFillType::Solid),
                               static_cast<int>(ArtifactSolidFillType::MirroredGradient));
    out.fill.type = static_cast<ArtifactSolidFillType>(raw);
  }
  out.fill.gradientAngleDegrees = std::isfinite(out.fill.gradientAngleDegrees)
      ? std::clamp(out.fill.gradientAngleDegrees, -360.0f, 360.0f) : 0.0f;
  out.fill.gradientCenterX = std::isfinite(out.fill.gradientCenterX)
      ? std::clamp(out.fill.gradientCenterX, 0.0f, 1.0f) : 0.5f;
  out.fill.gradientCenterY = std::isfinite(out.fill.gradientCenterY)
      ? std::clamp(out.fill.gradientCenterY, 0.0f, 1.0f) : 0.5f;
  out.fill.gradientRadius = std::isfinite(out.fill.gradientRadius)
      ? std::clamp(out.fill.gradientRadius, 0.0f, 100000.0f) : 0.5f;
  out.stroke.color = normalizedShapeColor(out.stroke.color);
  out.stroke.gradientStart = normalizedShapeColor(out.stroke.gradientStart);
  out.stroke.gradientEnd = normalizedShapeColor(out.stroke.gradientEnd);
  out.stroke.width = std::isfinite(out.stroke.width)
      ? std::clamp(out.stroke.width, 0.0f, static_cast<float>(kMaxShapeDimension)) : 0.0f;
  out.stroke.cap = static_cast<Artifact::StrokeCap>(std::clamp(
      static_cast<int>(out.stroke.cap), 0, 2));
  out.stroke.join = static_cast<Artifact::StrokeJoin>(std::clamp(
      static_cast<int>(out.stroke.join), 0, 2));
  out.stroke.align = static_cast<Artifact::StrokeAlign>(std::clamp(
      static_cast<int>(out.stroke.align), 0, 2));
  {
    std::vector<float> dash;
    dash.reserve(std::min(out.stroke.dashPattern.size(),
                          static_cast<size_t>(kMaxDashPatternEntries)));
    for (const float value : out.stroke.dashPattern) {
      if (dash.size() >= static_cast<size_t>(kMaxDashPatternEntries)) {
        break;
      }
      if (std::isfinite(value) && value > 0.001f) {
        dash.push_back(std::min(value, static_cast<float>(kMaxShapeDimension)));
      }
    }
    out.stroke.dashPattern = std::move(dash);
  }
  out.stroke.dashOffset = std::isfinite(out.stroke.dashOffset)
      ? std::clamp(out.stroke.dashOffset, -1000000.0f, 1000000.0f) : 0.0f;
  out.stroke.taperStart = std::isfinite(out.stroke.taperStart)
      ? std::clamp(out.stroke.taperStart, 0.0f, 1.0f) : 1.0f;
  out.stroke.taperEnd = std::isfinite(out.stroke.taperEnd)
      ? std::clamp(out.stroke.taperEnd, 0.0f, 1.0f) : 1.0f;
  out.opacity = std::isfinite(out.opacity) ? std::clamp(out.opacity, 0.0f, 1.0f) : 1.0f;
  out.merge = static_cast<Artifact::ShapeContentMerge>(
      std::clamp(static_cast<int>(out.merge), 0, 3));
  return out;
}

Artifact::ShapeContent ArtifactShapeLayer::makeContentFromLegacy() const {
  Artifact::ShapeContent content;
  content.name = QStringLiteral("Shape 1");
  if (!impl_) {
    return content;
  }
  auto& g = content.geometry;
  g.type = impl_->shapeType_;
  g.width = impl_->width_;
  g.height = impl_->height_;
  g.cornerRadius = impl_->cornerRadius_;
  g.starPoints = impl_->starPoints_;
  g.starInnerRadius = impl_->starInnerRadius_;
  g.polygonSides = impl_->polygonSides_;
  g.polygonPoints = impl_->customPolygonPoints_;
  g.polygonClosed = impl_->customPolygonClosed_;
  g.pathVertices = impl_->customPathVertices_;
  g.pathClosed = impl_->customPathClosed_;
  g.fillRule = impl_->customPathFillRule_;
  content.fill.enabled = impl_->fillEnabled_;
  content.fill.color = impl_->fillColor_;
  content.fill.type = impl_->fillType_;
  content.fill.gradientStart = impl_->fillGradientStartColor_;
  content.fill.gradientEnd = impl_->fillGradientEndColor_;
  content.fill.gradientAngleDegrees = impl_->fillGradientAngleDegrees_;
  content.fill.gradientCenterX = impl_->fillGradientCenterX_;
  content.fill.gradientCenterY = impl_->fillGradientCenterY_;
  content.fill.gradientRadius = impl_->fillGradientRadius_;
  content.stroke.enabled = impl_->strokeEnabled_;
  content.stroke.color = impl_->strokeColor_;
  content.stroke.width = impl_->strokeWidth_;
  content.stroke.cap = impl_->strokeCap_;
  content.stroke.join = impl_->strokeJoin_;
  content.stroke.align = impl_->strokeAlign_;
  content.stroke.dashPattern = impl_->dashPattern_;
  content.stroke.dashOffset = impl_->dashOffset_;
  content.stroke.taperStart = impl_->strokeTaperStart_;
  content.stroke.taperEnd = impl_->strokeTaperEnd_;
  content.stroke.gradientEnabled = impl_->strokeGradientEnabled_;
  content.stroke.gradientStart = impl_->strokeGradientStartColor_;
  content.stroke.gradientEnd = impl_->strokeGradientEndColor_;
  return content;
}

int ArtifactShapeLayer::shapeContentCount() const {
  return impl_ ? static_cast<int>(impl_->shapeContents_.size()) : 0;
}

bool ArtifactShapeLayer::hasMultiShapeContents() const {
  return shapeContentCount() > 0;
}

int ArtifactShapeLayer::addShapeContent(const Artifact::ShapeContent& content) {
  if (!impl_) {
    return -1;
  }
  if (impl_->shapeContents_.empty()) {
    impl_->shapeContents_.push_back(makeContentFromLegacy());
  }
  Artifact::ShapeContent normalized = normalizedShapeContent(content);
  if (normalized.name.isEmpty()) {
    normalized.name = QStringLiteral("Shape %1").arg(impl_->shapeContents_.size() + 1);
  }
  impl_->shapeContents_.push_back(std::move(normalized));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return static_cast<int>(impl_->shapeContents_.size()) - 1;
}

bool ArtifactShapeLayer::setShapeContentAt(int index, const Artifact::ShapeContent& content) {
  if (!impl_ || index < 0 ||
      index >= static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  impl_->shapeContents_[static_cast<size_t>(index)] = normalizedShapeContent(content);
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return true;
}

Artifact::ShapeContent ArtifactShapeLayer::shapeContentAt(int index) const {
  if (!impl_ || index < 0 ||
      index >= static_cast<int>(impl_->shapeContents_.size())) {
    return Artifact::ShapeContent();
  }
  return impl_->shapeContents_[static_cast<size_t>(index)];
}

bool ArtifactShapeLayer::removeShapeContentAt(int index) {
  if (!impl_ || index < 0 ||
      index >= static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  impl_->shapeContents_.erase(impl_->shapeContents_.begin() + index);
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return true;
}

void ArtifactShapeLayer::clearShapeContents() {
  if (!impl_ || impl_->shapeContents_.empty()) {
    return;
  }
  impl_->shapeContents_.clear();
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}

int ArtifactShapeLayer::activeContentIndex() const {
  return impl_ ? impl_->activeContentIndex_ : -1;
}

bool ArtifactShapeLayer::setActiveContentIndex(int index) {
  if (!impl_) {
    return false;
  }
  if (index < -1 ||
      index >= static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  impl_->activeContentIndex_ = index;
  Q_EMIT changed();
  return true;
}

ArtifactShapeLayer::ShapeContentProxy::ShapeContentProxy(
    ArtifactShapeLayer* layer, int index)
    : layer_(layer), index_(index) {}

bool ArtifactShapeLayer::ShapeContentProxy::isValid() const {
  return layer_ && index_ >= 0 &&
         index_ < layer_->shapeContentCount();
}

Artifact::ShapeContent ArtifactShapeLayer::ShapeContentProxy::content() const {
  return isValid() ? layer_->shapeContentAt(index_) : Artifact::ShapeContent();
}

void ArtifactShapeLayer::ShapeContentProxy::setContent(const ShapeContent& c) {
  if (isValid()) {
    layer_->setShapeContentAt(index_, c);
  }
}

Artifact::ShapeContent ArtifactShapeLayer::ShapeContentProxy::pull() const {
  return isValid() ? layer_->shapeContentAt(index_) : Artifact::ShapeContent();
}

QString ArtifactShapeLayer::ShapeContentProxy::name() const {
  const auto c = pull();
  return c.name;
}

void ArtifactShapeLayer::ShapeContentProxy::setName(const QString& name) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.name = name;
  setContent(c);
}

bool ArtifactShapeLayer::ShapeContentProxy::visible() const {
  const auto c = pull();
  return c.visible;
}

void ArtifactShapeLayer::ShapeContentProxy::setVisible(bool visible) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.visible = visible;
  setContent(c);
}

float ArtifactShapeLayer::ShapeContentProxy::opacity() const {
  const auto c = pull();
  return c.opacity;
}

void ArtifactShapeLayer::ShapeContentProxy::setOpacity(float opacity) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.opacity = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
  setContent(c);
}

Artifact::ShapeContentMerge ArtifactShapeLayer::ShapeContentProxy::merge() const {
  const auto c = pull();
  return c.merge;
}

void ArtifactShapeLayer::ShapeContentProxy::setMerge(ShapeContentMerge merge) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.merge = merge;
  setContent(c);
}

Artifact::ShapeContentFill ArtifactShapeLayer::ShapeContentProxy::fill() const {
  const auto c = pull();
  return c.fill;
}

void ArtifactShapeLayer::ShapeContentProxy::setFill(const ShapeContentFill& fill) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.fill = fill;
  setContent(c);
}

Artifact::ShapeContentStroke ArtifactShapeLayer::ShapeContentProxy::stroke() const {
  const auto c = pull();
  return c.stroke;
}

void ArtifactShapeLayer::ShapeContentProxy::setStroke(const ShapeContentStroke& stroke) {
  if (!isValid()) {
    return;
  }
  auto c = pull();
  c.stroke = stroke;
  setContent(c);
}

bool ArtifactShapeLayer::ShapeContentProxy::duplicate() {
  if (!isValid()) {
    return false;
  }
  layer_->duplicateShapeContent(index_);
  return true;
}

ArtifactShapeLayer::ShapeContentProxy ArtifactShapeLayer::activeContent() {
  return ShapeContentProxy(this, impl_ ? impl_->activeContentIndex_ : -1);
}

int ArtifactShapeLayer::duplicateShapeContent(int index) {
  if (!impl_ || index < 0 ||
      index >= static_cast<int>(impl_->shapeContents_.size())) {
    return -1;
  }
  auto content = normalizedShapeContent(
      impl_->shapeContents_[static_cast<size_t>(index)]);
  content.name = QStringLiteral("Copy of %1").arg(content.name);
  impl_->shapeContents_.insert(
      impl_->shapeContents_.begin() + index + 1, content);
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return index + 1;
}

bool ArtifactShapeLayer::moveShapeContent(int fromIndex, int toIndex) {
  if (!impl_ || fromIndex < 0 || toIndex < 0 ||
      fromIndex >= static_cast<int>(impl_->shapeContents_.size()) ||
      toIndex >= static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  if (fromIndex == toIndex) {
    return true;
  }
  auto content = std::move(impl_->shapeContents_[static_cast<size_t>(fromIndex)]);
  impl_->shapeContents_.erase(impl_->shapeContents_.begin() + fromIndex);
  const int dest = toIndex > fromIndex ? toIndex - 1 : toIndex;
  impl_->shapeContents_.insert(
      impl_->shapeContents_.begin() + dest, std::move(content));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return true;
}

bool ArtifactShapeLayer::insertShapeContent(int index, const ShapeContent& content) {
  if (!impl_ || index < 0 ||
      index > static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  if (impl_->shapeContents_.empty()) {
    impl_->shapeContents_.push_back(makeContentFromLegacy());
  }
  impl_->shapeContents_.insert(
      impl_->shapeContents_.begin() + index, normalizedShapeContent(content));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return true;
}

bool ArtifactShapeLayer::swapShapeContents(int a, int b) {
  if (!impl_ || a < 0 || b < 0 ||
      a >= static_cast<int>(impl_->shapeContents_.size()) ||
      b >= static_cast<int>(impl_->shapeContents_.size())) {
    return false;
  }
  if (a == b) {
    return true;
  }
  std::swap(impl_->shapeContents_[static_cast<size_t>(a)],
            impl_->shapeContents_[static_cast<size_t>(b)]);
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
  return true;
}

void ArtifactShapeLayer::ensureContentVisPaths() const {
  if (!impl_ || impl_->contentCache_.valid || impl_->shapeContents_.empty()) {
    return;
  }
  std::vector<std::vector<ArtifactCore::ShapePath>> processed;
  processed.reserve(impl_->shapeContents_.size());
  for (const auto& content : impl_->shapeContents_) {
    processed.push_back(
        applyShapeOperators(buildContentShapePath(content), impl_->shapeOperators_));
  }
  impl_->contentCache_.visPaths =
      resolveContentVisPaths(processed, impl_->shapeContents_);
  impl_->contentCache_.valid = true;
}

// ============================================================
// toQImage (Software rendering)
// ============================================================

QImage ArtifactShapeLayer::renderContentsToImage() const {
  const QRectF bounds = localBounds();
  if (!impl_ || bounds.isNull() || !bounds.isValid()) {
    return {};
  }
  int imgW = std::clamp(static_cast<int>(std::ceil(bounds.width())), 1, kMaxShapeDimension);
  int imgH = std::clamp(static_cast<int>(std::ceil(bounds.height())), 1, kMaxShapeDimension);
  const auto pixelCount = static_cast<std::uint64_t>(imgW) * static_cast<std::uint64_t>(imgH);
  if (pixelCount > kMaxShapeCachePixels) {
    const double scale = std::sqrt(static_cast<double>(kMaxShapeCachePixels) /
                                   static_cast<double>(pixelCount));
    imgW = std::max(1, static_cast<int>(std::floor(imgW * scale)));
    imgH = std::max(1, static_cast<int>(std::floor(imgH * scale)));
  }
  QImage img(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter painter(&img);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(-bounds.left(), -bounds.top());
  ensureContentVisPaths();
  for (size_t ci = 0; ci < impl_->shapeContents_.size(); ++ci) {
    const auto& content = impl_->shapeContents_[ci];
    if (!content.visible || content.opacity <= 0.0f) {
      continue;
    }
    if (ci >= impl_->contentCache_.visPaths.size()) {
      continue;
    }
    const float gw = static_cast<float>(std::max(1, content.geometry.width));
    const float gh = static_cast<float>(std::max(1, content.geometry.height));
    QBrush fillBrush;
    if (content.fill.type != ArtifactSolidFillType::Solid) {
      const float cx = gw * content.fill.gradientCenterX;
      const float cy = gh * content.fill.gradientCenterY;
      const float radius = std::max(gw, gh) * content.fill.gradientRadius;
      QGradient* grad = nullptr;
      if (content.fill.type == ArtifactSolidFillType::LinearGradient ||
          content.fill.type == ArtifactSolidFillType::RepeatingGradient ||
          content.fill.type == ArtifactSolidFillType::MirroredGradient) {
        const float rad = content.fill.gradientAngleDegrees * static_cast<float>(M_PI) / 180.0f;
        const float dx = std::cos(rad) * gw * 0.5f;
        const float dy = std::sin(rad) * gh * 0.5f;
        auto* linear = new QLinearGradient(QPointF(cx - dx, cy - dy), QPointF(cx + dx, cy + dy));
        if (content.fill.type == ArtifactSolidFillType::RepeatingGradient) {
          linear->setSpread(QGradient::RepeatSpread);
        } else if (content.fill.type == ArtifactSolidFillType::MirroredGradient) {
          linear->setSpread(QGradient::ReflectSpread);
        }
        grad = linear;
      } else if (content.fill.type == ArtifactSolidFillType::RadialGradient) {
        grad = new QRadialGradient(QPointF(cx, cy), radius);
      } else {
        grad = new QConicalGradient(QPointF(cx, cy), content.fill.gradientAngleDegrees);
      }
      grad->setColorAt(0.0, toQColor(FloatColor(
          content.fill.gradientStart.r(), content.fill.gradientStart.g(),
          content.fill.gradientStart.b(),
          content.fill.gradientStart.a() * content.opacity)));
      grad->setColorAt(1.0, toQColor(FloatColor(
          content.fill.gradientEnd.r(), content.fill.gradientEnd.g(),
          content.fill.gradientEnd.b(),
          content.fill.gradientEnd.a() * content.opacity)));
      fillBrush = QBrush(*grad);
      delete grad;
    } else {
      fillBrush = QColor(
          static_cast<int>(content.fill.color.r() * 255),
          static_cast<int>(content.fill.color.g() * 255),
          static_cast<int>(content.fill.color.b() * 255),
          static_cast<int>(content.fill.color.a() * content.opacity * 255));
    }
    const bool useTaper =
        std::abs(content.stroke.taperStart - 1.0f) > kStrokeEffectEpsilon ||
        std::abs(content.stroke.taperEnd - 1.0f) > kStrokeEffectEpsilon ||
        content.stroke.gradientEnabled;
    const bool canTaper = useTaper &&
        content.stroke.align == StrokeAlign::Center &&
        content.stroke.join == StrokeJoin::Miter &&
        content.stroke.dashPattern.empty();
    bool pathClosed = content.geometry.type != Artifact::ShapeType::Line;
    if (content.geometry.pathVertices.size() >= 3) {
      pathClosed = content.geometry.pathClosed;
    } else if (content.geometry.polygonPoints.size() >= 3) {
      pathClosed = content.geometry.polygonClosed;
    }
    for (const auto& shapePath : impl_->contentCache_.visPaths[ci]) {
      const QPainterPath path = shapePath.toPainterPath();
      if (path.isEmpty()) {
        continue;
      }
      if (content.fill.enabled) {
        painter.fillPath(path, fillBrush);
      }
      if (content.stroke.enabled && content.stroke.width > 0.0f) {
        if (canTaper) {
          const auto subpaths = path.toSubpathPolygons();
          const FloatColor gs = content.stroke.gradientEnabled
              ? content.stroke.gradientStart : content.stroke.color;
          const FloatColor ge = content.stroke.gradientEnabled
              ? content.stroke.gradientEnd : content.stroke.color;
          for (const QPolygonF& subpath : subpaths) {
            drawStrokePath(painter, polygonToPoints(subpath), pathClosed,
                           content.stroke.width, content.stroke.taperStart,
                           content.stroke.taperEnd, content.stroke.gradientEnabled,
                           content.stroke.color, gs, ge, content.stroke.cap);
          }
        } else {
          QColor sc(static_cast<int>(content.stroke.color.r() * 255),
                    static_cast<int>(content.stroke.color.g() * 255),
                    static_cast<int>(content.stroke.color.b() * 255),
                    static_cast<int>(content.stroke.color.a() * content.opacity * 255));
          QPen pen(sc, content.stroke.width);
          switch (content.stroke.cap) {
            case StrokeCap::Round: pen.setCapStyle(Qt::RoundCap); break;
            case StrokeCap::Square: pen.setCapStyle(Qt::SquareCap); break;
            default: pen.setCapStyle(Qt::FlatCap); break;
          }
          switch (content.stroke.join) {
            case StrokeJoin::Round: pen.setJoinStyle(Qt::RoundJoin); break;
            case StrokeJoin::Bevel: pen.setJoinStyle(Qt::BevelJoin); break;
            default: pen.setJoinStyle(Qt::MiterJoin); break;
          }
           if (!content.stroke.dashPattern.empty()) {
             QVector<qreal> qDash;
             qDash.reserve(static_cast<int>(content.stroke.dashPattern.size()));
             for (float v : content.stroke.dashPattern) {
               qDash.push_back(static_cast<qreal>(v));
             }
             pen.setDashPattern(qDash);
             pen.setDashOffset(static_cast<qreal>(content.stroke.dashOffset));
           }
          if (content.stroke.align == StrokeAlign::Inside) {
            painter.save();
            painter.setClipPath(path);
            QPen widePen = pen;
            widePen.setWidthF(static_cast<qreal>(content.stroke.width) * 2.0);
            painter.setPen(widePen);
            painter.drawPath(path);
            painter.restore();
          } else if (content.stroke.align == StrokeAlign::Outside) {
            painter.save();
            QPainterPath outside;
            outside.addRect(QRectF(bounds.left() - 2, bounds.top() - 2,
                                   bounds.width() + 4, bounds.height() + 4));
            outside = outside.subtracted(path);
            painter.setClipPath(outside);
            QPen widePen = pen;
            widePen.setWidthF(static_cast<qreal>(content.stroke.width) * 2.0);
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
  return img;
}

QImage ArtifactShapeLayer::toQImage() const {
  if (impl_ && !impl_->shapeContents_.empty()) {
    return renderContentsToImage();
  }
  const bool geomAnimated = hasAnimatedShapeGeometry(this);
  const bool pathAnimated = hasPathKeyframes();
 if (geomAnimated || pathAnimated) {
  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  const std::vector<CustomPathVertex> evaluatedPathVertices =
      pathAnimated ? evaluatePathAt(currentFrame())
                   : impl_->customPathVertices_;
  impl_->rebuildCache(&dims, &evaluatedPathVertices);
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
  if (impl_ && !impl_->shapeContents_.empty()) {
    if (!impl_->localBoundsCacheDirty_) {
      return impl_->cachedLocalBounds_;
    }
    ensureContentVisPaths();
    QRectF bounds;
    qreal maxPad = 0.5;
    for (size_t ci = 0; ci < impl_->shapeContents_.size(); ++ci) {
      const auto& content = impl_->shapeContents_[ci];
      if (!content.visible) {
        continue;
      }
      if (ci < impl_->contentCache_.visPaths.size()) {
        for (const auto& path : impl_->contentCache_.visPaths[ci]) {
          const QRectF pathBounds = path.boundingRect();
          bounds = bounds.isNull() ? pathBounds : bounds.united(pathBounds);
        }
      }
      if (content.stroke.enabled && content.stroke.width > 0.0f) {
        const qreal w = static_cast<qreal>(content.stroke.width);
        qreal pad = content.stroke.align == StrokeAlign::Outside
            ? w
            : (content.stroke.align == StrokeAlign::Center ? w * 0.5 : 0.0);
        if (content.stroke.join == StrokeJoin::Miter) {
          pad = std::max(pad, std::max<qreal>(1.0, w) * 4.0);
        }
        maxPad = std::max(maxPad, pad);
      }
    }
    if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
      const auto size = sourceSize();
      if (size.width <= 0 || size.height <= 0) {
        return QRectF();
      }
      bounds = QRectF(0.0, 0.0, static_cast<qreal>(size.width),
                      static_cast<qreal>(size.height));
    }
    impl_->cachedLocalBounds_ = bounds.adjusted(-maxPad, -maxPad, maxPad, maxPad);
    impl_->localBoundsCacheDirty_ = false;
    return impl_->cachedLocalBounds_;
  }
  const bool geomAnimated = hasAnimatedShapeGeometry(this);
  const bool pathAnimated = hasPathKeyframes();
  const std::vector<CustomPathVertex> evaluatedPathVertices =
      pathAnimated ? evaluatePathAt(currentFrame())
                   : impl_->customPathVertices_;
  if (!impl_->localBoundsCacheDirty_ && !geomAnimated && !pathAnimated) {
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
       evaluatedPathVertices, impl_->customPathClosed_,
       impl_->shapeOperators_);
   for (const auto& path : processedPaths) {
    const QRectF pathBounds = path.boundingRect();
    bounds = bounds.isNull() ? pathBounds : bounds.united(pathBounds);
   }
  } else if (evaluatedPathVertices.size() >= 3) {
   auto customPath = buildCustomShapePath(evaluatedPathVertices,
                                          impl_->customPathClosed_);
   customPath.setFillRule(impl_->customPathFillRule_);
   bounds = customPath.boundingRect();
  } else if (evaluatedPathVertices.size() >= 2) {
   std::vector<QPointF> pts;
   pts.reserve(evaluatedPathVertices.size());
   for (const auto& v : evaluatedPathVertices) pts.push_back(v.pos);
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

std::vector<QPointF> ArtifactShapeLayer::collisionOutlineLocalPoints() const
{
  // Multi-content: concatenate per-content outlines (custom path, polygon,
  // or primitive sample points). Operator/merge reshaping falls back to
  // the auto-bounds path, same as the legacy operator case below.
  if (impl_ && !impl_->shapeContents_.empty()) {
    if (!impl_->shapeOperators_.empty()) {
      return {};
    }
    std::vector<QPointF> points;
    for (const auto& content : impl_->shapeContents_) {
      if (!content.visible) {
        continue;
      }
      const auto& g = content.geometry;
      if (g.pathVertices.size() >= 3) {
        for (const auto& vertex : g.pathVertices) {
          points.push_back(vertex.pos);
        }
        continue;
      }
      if (g.polygonPoints.size() >= 3) {
        for (const auto& p : g.polygonPoints) {
          points.push_back(p);
        }
        continue;
      }
      if (g.type == Artifact::ShapeType::Line) {
        continue;
      }
      const auto sampled = buildRenderablePoints(
          g.type, std::max(1, g.width), std::max(1, g.height), g.cornerRadius,
          std::max(3, g.starPoints), g.starInnerRadius, std::max(3, g.polygonSides),
          g.polygonPoints, g.polygonClosed);
      points.insert(points.end(), sampled.begin(), sampled.end());
    }
    return points;
  }
  // Operator stacks reshape the outline; a stale proxy would misrepresent the
  // collision shape, so fall back to the auto-bounds path instead.
  if (!impl_->shapeOperators_.empty()) {
    return {};
  }

  if (impl_->customPathVertices_.size() >= 3) {
   std::vector<QPointF> points;
   points.reserve(impl_->customPathVertices_.size());
   for (const auto& vertex : impl_->customPathVertices_) {
    points.push_back(vertex.pos);
   }
   return points;
  }

  if (impl_->customPolygonPoints_.size() >= 3) {
    return impl_->customPolygonPoints_;
  }

  if (impl_->shapeType_ == Artifact::ShapeType::Line) {
    return {};
  }

  const ShapeGeomDims dims = resolveShapeGeomDims(
      this, impl_->width_, impl_->height_, impl_->cornerRadius_,
      impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);
  return buildRenderablePoints(impl_->shapeType_, dims.width, dims.height,
                               dims.cornerRadius, dims.starPoints,
                               dims.starInnerRadius, dims.polygonSides,
                               impl_->customPolygonPoints_,
                               impl_->customPolygonClosed_);
}
// ============================================================
// GPU vector painting (gaps 1 & 3)
// ============================================================

struct GpuPaintItem {
  std::vector<ArtifactCore::ShapePath> fillPaths;
  std::vector<ArtifactCore::ShapePath> strokePaths;
  Artifact::ShapeContentFill fill;
  Artifact::ShapeContentStroke stroke;
  float gradientW = 1.0f;
  float gradientH = 1.0f;
  float itemOpacity = 1.0f;
};

// Unified GPU painter for gradient fills, Inside/Outside strokes and
// taper/gradient strokes. Triangulation is intentionally fresh per frame:
// this path replaces the QImage cache sprite, which was strictly more
// expensive. The legacy solid fast paths below keep their caches.
static void paintGpuPaintItems(Artifact::ArtifactIRenderer* renderer,
                               const QMatrix4x4& transform,
                               float baseOpacity,
                               const std::vector<GpuPaintItem>& items,
                               double tolerance,
                               float renderScale) {
  if (!renderer || items.empty()) {
    return;
  }
  const double tol = (std::isfinite(tolerance) && tolerance > 0.0) ? tolerance : 0.25;
  for (const auto& item : items) {
    const float opacity = baseOpacity * item.itemOpacity;
    const bool hasStroke = item.stroke.enabled && item.stroke.width > 0.0f;
    if (opacity <= 0.0f || (!item.fill.enabled && !hasStroke)) {
      continue;
    }
    const float gradW = item.gradientW > 0.0f ? item.gradientW : 1.0f;
    const float gradH = item.gradientH > 0.0f ? item.gradientH : 1.0f;
    if (item.fill.enabled) {
      for (const auto& path : item.fillPaths) {
        const auto triangles = path.triangulate(tol);
        for (const auto& tri : triangles) {
          const double cx = (tri.p0.x() + tri.p1.x() + tri.p2.x()) / 3.0;
          const double cy = (tri.p0.y() + tri.p1.y() + tri.p2.y()) / 3.0;
          ArtifactCore::FloatColor c = contentGradientColorAt(
              item.fill, static_cast<float>(cx), static_cast<float>(cy), gradW, gradH);
          c = ArtifactCore::FloatColor(c.r(), c.g(), c.b(), c.a() * opacity);
          const QPointF p0 = mapPoint(transform, tri.p0);
          const QPointF p1 = mapPoint(transform, tri.p1);
          const QPointF p2 = mapPoint(transform, tri.p2);
          renderer->drawSolidTriangleLocal(
              {static_cast<float>(p0.x()), static_cast<float>(p0.y())},
              {static_cast<float>(p1.x()), static_cast<float>(p1.y())},
              {static_cast<float>(p2.x()), static_cast<float>(p2.y())}, c);
        }
      }
    }
    if (hasStroke) {
      const bool hasTaper =
          std::abs(item.stroke.taperStart - 1.0f) > kStrokeEffectEpsilon ||
          std::abs(item.stroke.taperEnd - 1.0f) > kStrokeEffectEpsilon;
      // Taper needs the segmented stroker (PolylineStyle has no taper
      // channel). Gradient-only and dashed strokes stay on
      // drawStyledPolyline so joins, caps, dash phase and along-path
      // gradient compose. Dash wins over taper.
      const bool useTaper = hasTaper && item.stroke.dashPattern.empty();
      const float thickness = std::max(1.0f, item.stroke.width * renderScale);
      const ArtifactCore::FloatColor baseStroke = ArtifactCore::FloatColor(
          item.stroke.color.r(), item.stroke.color.g(), item.stroke.color.b(),
          item.stroke.color.a() * opacity);
      const ArtifactCore::FloatColor gradStrokeStart = ArtifactCore::FloatColor(
          item.stroke.gradientStart.r(), item.stroke.gradientStart.g(),
          item.stroke.gradientStart.b(), item.stroke.gradientStart.a() * opacity);
      const ArtifactCore::FloatColor gradStrokeEnd = ArtifactCore::FloatColor(
          item.stroke.gradientEnd.r(), item.stroke.gradientEnd.g(),
          item.stroke.gradientEnd.b(), item.stroke.gradientEnd.a() * opacity);
      for (const auto& strokePath : item.strokePaths) {
        const auto subpaths = strokePath.flattenSubpaths(tol);
        for (const auto& segments : subpaths) {
          if (segments.empty()) {
            continue;
          }
          std::vector<Artifact::Detail::float2> points;
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
          if (useTaper) {
            drawTaperedPolylineGPU(renderer, points, closed, thickness,
                                   item.stroke.taperStart, item.stroke.taperEnd,
                                   gradStrokeStart, gradStrokeEnd,
                                   item.stroke.gradientEnabled, baseStroke,
                                   item.stroke.cap);
          } else {
            PolylineStyle style;
            style.thickness = thickness;
            style.cap = static_cast<PolylineCap>(item.stroke.cap);
            style.join = static_cast<PolylineJoin>(item.stroke.join);
            style.closed = closed;
            style.dashPattern = item.stroke.dashPattern;
            style.dashOffset = item.stroke.dashOffset;
            style.gradientEnabled = item.stroke.gradientEnabled;
            style.gradientStart = gradStrokeStart;
            style.gradientEnd = gradStrokeEnd;
            renderer->drawStyledPolyline(points, style, baseStroke);
          }
        }
      }
    }
  }
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
  // Gap 1: multi-content GPU rendering. Each visible content paints with its
  // own fill/stroke after merge resolution. Physics grids stay legacy-only.
  if (!impl->shapeContents_.empty()) {
   const bool contentsOpsAnimated = hasAnimatedShapeOperators(this);
   std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>> contentsOpClones;
   if (contentsOpsAnimated) {
    contentsOpClones.reserve(impl->shapeOperators_.size());
    for (const auto& op : impl->shapeOperators_) {
     contentsOpClones.push_back(op->clone());
    }
    applyAnimatedOperatorParameters(this, contentsOpClones);
    std::vector<std::vector<ArtifactCore::ShapePath>> processed;
    processed.reserve(impl->shapeContents_.size());
    for (const auto& content : impl->shapeContents_) {
     processed.push_back(
         applyShapeOperators(buildContentShapePath(content), contentsOpClones));
    }
    impl->contentCache_.visPaths =
        resolveContentVisPaths(processed, impl->shapeContents_);
    impl->contentCache_.valid = false;
   } else {
    ensureContentVisPaths();
   }
   std::vector<GpuPaintItem> contentItems;
   contentItems.reserve(impl->shapeContents_.size());
   for (size_t ci = 0; ci < impl->shapeContents_.size(); ++ci) {
    const auto& content = impl->shapeContents_[ci];
    if (!content.visible || content.opacity <= 0.0f) {
     continue;
    }
    if (ci >= impl->contentCache_.visPaths.size() ||
        impl->contentCache_.visPaths[ci].empty()) {
     continue;
    }
    GpuPaintItem item;
    item.fillPaths = impl->contentCache_.visPaths[ci];
    item.strokePaths.reserve(item.fillPaths.size());
    for (const auto& path : item.fillPaths) {
     item.strokePaths.push_back(
         strokeAlignedPath(path, content.stroke.align, content.stroke.width));
    }
    item.fill = content.fill;
    item.stroke = content.stroke;
    item.gradientW = static_cast<float>(std::max(1, content.geometry.width));
    item.gradientH = static_cast<float>(std::max(1, content.geometry.height));
    item.itemOpacity = content.opacity;
    contentItems.push_back(std::move(item));
   }
   for (const auto& lensPass : twoPointFiveDRenderPasses(baseTransform)) {
    drawWithClonerEffect(
        this, lensPass.transform,
        [renderer, this, contentItems, contentFieldWeight,
         lensOpacity = lensPass.opacity](const QMatrix4x4& transform, float weight) {
         const float baseOpacity =
             this->opacity() * weight * contentFieldWeight * lensOpacity;
         const double scaleX = std::hypot(static_cast<double>(transform(0, 0)),
                                          static_cast<double>(transform(1, 0)));
         const double scaleY = std::hypot(static_cast<double>(transform(0, 1)),
                                          static_cast<double>(transform(1, 1)));
         const double renderScale = std::max({1.0, scaleX, scaleY});
         paintGpuPaintItems(renderer, transform, baseOpacity, contentItems,
                             0.25 / renderScale, static_cast<float>(renderScale));
        });
   }
   drawFractureOverlay(renderer, baseTransform, localBounds().size(),
                       opacity() * contentFieldWeight);
   return;
  }
  const bool geomAnimated = hasAnimatedShapeGeometry(this);
  const bool pathAnimated = hasPathKeyframes();
  const std::vector<CustomPathVertex> evaluatedPathVertices =
      pathAnimated ? evaluatePathAt(currentFrame())
                   : impl->customPathVertices_;
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
  const bool operatorsAnimated = hasAnimatedShapeOperators(this);
  std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>
      animatedOperatorClones;
  if (nativeOperatorCandidate) {
    if (operatorsAnimated) {
     // Evaluate keyframed operator parameters on clones so the authored
     // operator instances keep their static values for the Inspector.
     animatedOperatorClones.reserve(impl->shapeOperators_.size());
     for (const auto& op : impl->shapeOperators_) {
      animatedOperatorClones.push_back(op->clone());
     }
     applyAnimatedOperatorParameters(this, animatedOperatorClones);
    }
    auto processedOperatorPaths = buildProcessedShapePaths(
        impl->shapeType_, geomDims.width, geomDims.height, geomDims.cornerRadius,
        geomDims.starPoints, geomDims.starInnerRadius, geomDims.polygonSides,
       impl->customPolygonPoints_, impl->customPolygonClosed_,
       evaluatedPathVertices, impl->customPathClosed_,
       operatorsAnimated ? animatedOperatorClones
                         : impl->shapeOperators_);
   if (evaluatedPathVertices.size() >= 3) {
    for (auto& path : processedOperatorPaths) {
     path.setFillRule(impl->customPathFillRule_);
    }
   }
   if (!processedOperatorPaths.empty()) {
   const FloatColor fill(impl->fillColor_.r(), impl->fillColor_.g(),
                         impl->fillColor_.b(), impl->fillColor_.a());
   const FloatColor stroke(impl->strokeColor_.r(), impl->strokeColor_.g(),
                           impl->strokeColor_.b(), impl->strokeColor_.a());
   for (const auto& lensPass : twoPointFiveDRenderPasses(baseTransform)) {
   drawWithClonerEffect(
       this, lensPass.transform,
        [renderer, impl, processedPaths = processedOperatorPaths, fill, stroke,
        contentFieldWeight, this, geomAnimated, pathAnimated, lensOpacity = lensPass.opacity](const QMatrix4x4& transform, float weight) {
        const float opacity = this->opacity() * weight * contentFieldWeight * lensOpacity;
        const FloatColor drawFill(fill.r(), fill.g(), fill.b(), fill.a() * opacity);
        const FloatColor drawStroke(stroke.r(), stroke.g(), stroke.b(), stroke.a() * opacity);
        const double scaleX = std::hypot(static_cast<double>(transform(0, 0)),
                                         static_cast<double>(transform(1, 0)));
        const double scaleY = std::hypot(static_cast<double>(transform(0, 1)),
                                         static_cast<double>(transform(1, 1)));
        const double renderScale = std::max({1.0, scaleX, scaleY});
        const auto& geometry = impl->nativeGeometry(processedPaths,
                                                    0.25 / renderScale,
                                                    !geomAnimated && !pathAnimated);
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
            style.dashOffset = impl->dashOffset_;
            renderer->drawStyledPolyline(points, style, drawStroke);
          }
         }
        }
       });
   }
   drawFractureOverlay(renderer, baseTransform,
                       QSizeF(geomDims.width, geomDims.height),
                       opacity() * contentFieldWeight);
   return;
  }
  }
  // Gap 3: gradient fills, Inside/Outside strokes, taper/gradient strokes
  // and operator stacks render on the GPU vector path instead of the QImage
  // cache sprite. The cache remains for toQImage()/thumbnails only.
  const bool needsUnifiedGpu =
      (impl->fillEnabled_ && impl->fillType_ != ArtifactSolidFillType::Solid) ||
      (impl->strokeEnabled_ && impl->strokeWidth_ > 0.0f &&
       (impl->strokeAlign_ != StrokeAlign::Center || impl->hasCustomStrokeEffects()));
  if (!nativeOperatorCandidate && needsUnifiedGpu) {
   const ShapeCompatibilityFallback unifiedFallback = impl->compatibilityFallback();
   if (unifiedFallback != impl->lastLoggedFallback_) {
    qInfo() << "[ArtifactShapeLayer] gpu vector path:"
            << compatibilityFallbackName(unifiedFallback);
    impl->lastLoggedFallback_ = unifiedFallback;
   }
   std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>> legacyOpClones;
   const std::vector<std::unique_ptr<ArtifactCore::ShapeOperator>>* legacyOps =
       &impl->shapeOperators_;
   if (operatorsAnimated) {
    legacyOpClones.reserve(impl->shapeOperators_.size());
    for (const auto& op : impl->shapeOperators_) {
     legacyOpClones.push_back(op->clone());
    }
    applyAnimatedOperatorParameters(this, legacyOpClones);
    legacyOps = &legacyOpClones;
   }
   const auto legacyProcessed = buildProcessedShapePaths(
       impl->shapeType_, geomDims.width, geomDims.height, geomDims.cornerRadius,
       geomDims.starPoints, geomDims.starInnerRadius, geomDims.polygonSides,
       impl->customPolygonPoints_, impl->customPolygonClosed_,
       evaluatedPathVertices, impl->customPathClosed_, *legacyOps);
   GpuPaintItem legacyItem;
   legacyItem.fillPaths = legacyProcessed;
   legacyItem.strokePaths.reserve(legacyProcessed.size());
   for (const auto& path : legacyProcessed) {
    legacyItem.strokePaths.push_back(
        strokeAlignedPath(path, impl->strokeAlign_, impl->strokeWidth_));
   }
   legacyItem.fill.enabled = impl->fillEnabled_;
   legacyItem.fill.color = impl->fillColor_;
   legacyItem.fill.type = impl->fillType_;
   legacyItem.fill.gradientStart = impl->fillGradientStartColor_;
   legacyItem.fill.gradientEnd = impl->fillGradientEndColor_;
   legacyItem.fill.gradientAngleDegrees = impl->fillGradientAngleDegrees_;
   legacyItem.fill.gradientCenterX = impl->fillGradientCenterX_;
   legacyItem.fill.gradientCenterY = impl->fillGradientCenterY_;
   legacyItem.fill.gradientRadius = impl->fillGradientRadius_;
   legacyItem.stroke.enabled = impl->strokeEnabled_;
   legacyItem.stroke.color = impl->strokeColor_;
   legacyItem.stroke.width = impl->strokeWidth_;
   legacyItem.stroke.cap = impl->strokeCap_;
   legacyItem.stroke.join = impl->strokeJoin_;
   legacyItem.stroke.align = impl->strokeAlign_;
   legacyItem.stroke.dashPattern = impl->dashPattern_;
   legacyItem.stroke.dashOffset = impl->dashOffset_;
   legacyItem.stroke.taperStart = impl->strokeTaperStart_;
   legacyItem.stroke.taperEnd = impl->strokeTaperEnd_;
   legacyItem.stroke.gradientEnabled = impl->strokeGradientEnabled_;
   legacyItem.stroke.gradientStart = impl->strokeGradientStartColor_;
   legacyItem.stroke.gradientEnd = impl->strokeGradientEndColor_;
   legacyItem.gradientW = static_cast<float>(std::max(1, geomDims.width));
   legacyItem.gradientH = static_cast<float>(std::max(1, geomDims.height));
   std::vector<GpuPaintItem> legacyItems;
   legacyItems.push_back(std::move(legacyItem));
   for (const auto& lensPass : twoPointFiveDRenderPasses(baseTransform)) {
    drawWithClonerEffect(
        this, lensPass.transform,
        [renderer, this, legacyItems, contentFieldWeight,
         lensOpacity = lensPass.opacity](const QMatrix4x4& transform, float weight) {
         const float baseOpacity =
             this->opacity() * weight * contentFieldWeight * lensOpacity;
         const double scaleX = std::hypot(static_cast<double>(transform(0, 0)),
                                          static_cast<double>(transform(1, 0)));
         const double scaleY = std::hypot(static_cast<double>(transform(0, 1)),
                                          static_cast<double>(transform(1, 1)));
         const double renderScale = std::max({1.0, scaleX, scaleY});
         paintGpuPaintItems(renderer, transform, baseOpacity, legacyItems,
                             0.25 / renderScale, static_cast<float>(renderScale));
        });
   }
   drawFractureOverlay(renderer, baseTransform,
                       QSizeF(geomDims.width, geomDims.height),
                       opacity() * contentFieldWeight);
   return;
  }
  for (const auto& lensPass : twoPointFiveDRenderPasses(baseTransform)) {
  drawWithClonerEffect(this, lensPass.transform,
                       [renderer, impl, this, contentFieldWeight, geomDims,
                        pathAnimated, evaluatedPathVertices, lensOpacity = lensPass.opacity](const QMatrix4x4& transform, float weight) {
    const auto fill = FloatColor(
        impl->fillColor_.r(), impl->fillColor_.g(), impl->fillColor_.b(),
        impl->fillColor_.a() * this->opacity() * contentFieldWeight * weight * lensOpacity);
    const auto stroke = FloatColor(
        impl->strokeColor_.r(), impl->strokeColor_.g(), impl->strokeColor_.b(),
        impl->strokeColor_.a() * this->opacity() * contentFieldWeight * weight * lensOpacity);

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
    const auto& geometry = impl->shapeGeometry(
        geomDims, 0.25 / renderScale,
        pathAnimated ? &evaluatedPathVertices : nullptr);
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
       style.dashOffset = impl->dashOffset_;
       renderer->drawStyledPolyline(points, style, stroke);
     }
    }
  });
  }
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

  auto dashOffsetProp = makeProp(QStringLiteral("shape.dashOffset"),
                                 ArtifactCore::PropertyType::Float,
                                 impl_->dashOffset_, -196);
  dashOffsetProp->setSoftRange(-256.0, 256.0);
  dashOffsetProp->setHardRange(-1000000.0, 1000000.0);
  dashOffsetProp->setDisplayLabel(QStringLiteral("Dash Offset"));
  dashOffsetProp->setTooltip(QStringLiteral("Dash pattern phase shift (marching ants)"));
   appearanceGroup.addProperty(dashOffsetProp);

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

  // Multi-content group (gap 1): per-content paint/visibility/merge.
  // Geometry editing stays on the ShapeContent API for now.
  if (!impl_->shapeContents_.empty()) {
    ArtifactCore::PropertyGroup contentsGroup;
    contentsGroup.setName("Contents");
auto countProp = makeProp(QStringLiteral("shape.contents.count"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(impl_->shapeContents_.size()),
                               -190, false);
     countProp->setDisplayLabel(QStringLiteral("Content Count"));
     contentsGroup.addProperty(countProp);
     auto activeProp = makeProp(QStringLiteral("shape.activeContentIndex"),
                                   ArtifactCore::PropertyType::Integer,
                                   impl_->activeContentIndex_, -189, false);
     activeProp->setDisplayLabel(QStringLiteral("Active Content"));
     activeProp->setTooltip(QStringLiteral("-1 = legacy mode, 0+ = index of edited content"));
     contentsGroup.addProperty(activeProp);
     for (size_t ci = 0; ci < impl_->shapeContents_.size(); ++ci) {
      const auto& content = impl_->shapeContents_[ci];
      const QString prefix = QStringLiteral("shape.content.%1.").arg(ci);
      const QString title = content.name.isEmpty()
          ? QStringLiteral("Content %1").arg(ci + 1)
          : content.name;
      auto nameProp = makeProp(prefix + QStringLiteral("name"),
                               ArtifactCore::PropertyType::String,
                               content.name, -189, false);
      nameProp->setDisplayLabel(title);
      contentsGroup.addProperty(nameProp);
      auto visibleProp = makeProp(prefix + QStringLiteral("visible"),
                                  ArtifactCore::PropertyType::Boolean,
                                  content.visible, -188);
      visibleProp->setDisplayLabel(QStringLiteral("Visible"));
      contentsGroup.addProperty(visibleProp);
      auto opacityProp = makeProp(prefix + QStringLiteral("opacity"),
                                  ArtifactCore::PropertyType::Float,
                                  content.opacity, -187);
      opacityProp->setSoftRange(0.0, 1.0);
      opacityProp->setHardRange(0.0, 1.0);
      opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
      contentsGroup.addProperty(opacityProp);
      auto mergeProp = makeProp(prefix + QStringLiteral("merge"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<int>(content.merge), -186, false);
      mergeProp->setHardRange(0, 3);
      mergeProp->setDisplayLabel(QStringLiteral("Merge"));
      mergeProp->setTooltip(QStringLiteral("0=Add, 1=Subtract, 2=Intersect, 3=Difference"));
      contentsGroup.addProperty(mergeProp);
      auto fillEnabledProp = makeProp(
          prefix + QStringLiteral("fillEnabled"),
          ArtifactCore::PropertyType::Boolean, content.fill.enabled, -185);
      fillEnabledProp->setDisplayLabel(QStringLiteral("Fill Enabled"));
      contentsGroup.addProperty(fillEnabledProp);
      auto fillColorProp = makeProp(
          prefix + QStringLiteral("fillColor"),
          ArtifactCore::PropertyType::Color,
          QColor(static_cast<int>(content.fill.color.r() * 255),
                 static_cast<int>(content.fill.color.g() * 255),
                 static_cast<int>(content.fill.color.b() * 255),
                 static_cast<int>(content.fill.color.a() * 255)),
          -184);
      fillColorProp->setDisplayLabel(QStringLiteral("Fill Color"));
      contentsGroup.addProperty(fillColorProp);
      auto strokeEnabledProp = makeProp(
          prefix + QStringLiteral("strokeEnabled"),
          ArtifactCore::PropertyType::Boolean, content.stroke.enabled, -183);
      strokeEnabledProp->setDisplayLabel(QStringLiteral("Stroke Enabled"));
      contentsGroup.addProperty(strokeEnabledProp);
      auto strokeColorProp = makeProp(
          prefix + QStringLiteral("strokeColor"),
          ArtifactCore::PropertyType::Color,
          QColor(static_cast<int>(content.stroke.color.r() * 255),
                 static_cast<int>(content.stroke.color.g() * 255),
                 static_cast<int>(content.stroke.color.b() * 255),
                 static_cast<int>(content.stroke.color.a() * 255)),
          -182);
      strokeColorProp->setDisplayLabel(QStringLiteral("Stroke Color"));
      contentsGroup.addProperty(strokeColorProp);
      auto strokeWidthProp = makeProp(
          prefix + QStringLiteral("strokeWidth"),
          ArtifactCore::PropertyType::Float, content.stroke.width, -181);
      strokeWidthProp->setSoftRange(0.0, 64.0);
      strokeWidthProp->setHardRange(0.0, 16384.0);
      strokeWidthProp->setDisplayLabel(QStringLiteral("Stroke Width"));
      contentsGroup.addProperty(strokeWidthProp);
      auto contentDashOffsetProp = makeProp(
          prefix + QStringLiteral("dashOffset"),
          ArtifactCore::PropertyType::Float, content.stroke.dashOffset, -180);
      contentDashOffsetProp->setSoftRange(-256.0, 256.0);
      contentDashOffsetProp->setHardRange(-1000000.0, 1000000.0);
      contentDashOffsetProp->setDisplayLabel(QStringLiteral("Dash Offset"));
      contentsGroup.addProperty(contentDashOffsetProp);
    }
    groups.push_back(contentsGroup);
  }

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
   } else if (auto merge =
                  dynamic_cast<const ArtifactCore::MergePaths *>(op.get())) {
     auto modeProp = makeProp(prefix + QStringLiteral("mode"),
                              ArtifactCore::PropertyType::Integer,
                              merge->mode(), -100);
     modeProp->setDisplayLabel(QStringLiteral("Mode"));
     modeProp->setHardRange(0, 4);
     modeProp->setAnimatable(true);
     modeProp->setTooltip(QStringLiteral(
         "0=Add, 1=Subtract, 2=Intersect, 3=Difference, 4=Merge"));
     opGroup.addProperty(modeProp);
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

 if (impl_->shapeType_ == Artifact::ShapeType::Line) {
  for (auto& group : groups) {
   group.removeProperty(QStringLiteral("shape.height"));
   for (const auto& property : {
            QStringLiteral("shape.fillColor"),
            QStringLiteral("shape.fillEnabled"),
            QStringLiteral("shape.fillType"),
            QStringLiteral("shape.fillGradientStartColor"),
            QStringLiteral("shape.fillGradientEndColor"),
            QStringLiteral("shape.fillGradientAngle"),
            QStringLiteral("shape.fillGradientCenterX"),
            QStringLiteral("shape.fillGradientCenterY"),
            QStringLiteral("shape.fillGradientRadius")}) {
    group.removeProperty(property);
   }
  }
  groups.erase(std::remove_if(groups.begin(), groups.end(), [](const auto& group) {
    return group.name() == QStringLiteral("Shape Parameters");
  }), groups.end());
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
if (propertyPath == "shape.dashOffset") {
    setDashOffset(value.toFloat());
    return true;
   }
   if (propertyPath == "shape.activeContentIndex") {
    setActiveContentIndex(value.toInt());
    return true;
   }

   if (propertyPath.startsWith("shape.content.")) {
    const QStringList parts = propertyPath.split('.');
    if (parts.size() == 4) {
      bool ok = false;
      const int contentIndex = parts[2].toInt(&ok);
      const QString field = parts[3];
      if (ok && contentIndex >= 0 &&
          contentIndex < static_cast<int>(impl_->shapeContents_.size())) {
        auto content = impl_->shapeContents_[static_cast<size_t>(contentIndex)];
        bool handled = false;
        if (field == "name") {
          content.name = value.toString();
          handled = true;
        } else if (field == "visible") {
          content.visible = value.toBool();
          handled = true;
        } else if (field == "opacity") {
          const float next = value.toFloat();
          content.opacity = std::isfinite(next) ? std::clamp(next, 0.0f, 1.0f) : 1.0f;
          handled = true;
        } else if (field == "merge") {
          content.merge = static_cast<Artifact::ShapeContentMerge>(
              std::clamp(value.toInt(), 0, 3));
          handled = true;
        } else if (field == "fillEnabled") {
          content.fill.enabled = value.toBool();
          handled = true;
        } else if (field == "fillColor") {
          const auto c = value.value<QColor>();
          content.fill.color = normalizedShapeColor(
              FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
          handled = true;
        } else if (field == "strokeEnabled") {
          content.stroke.enabled = value.toBool();
          handled = true;
        } else if (field == "strokeColor") {
          const auto c = value.value<QColor>();
          content.stroke.color = normalizedShapeColor(
              FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
          handled = true;
        } else if (field == "strokeWidth") {
          const float next = value.toFloat();
          content.stroke.width = std::isfinite(next)
              ? std::clamp(next, 0.0f, static_cast<float>(kMaxShapeDimension)) : 0.0f;
          handled = true;
        } else if (field == "dashOffset") {
          const float next = value.toFloat();
          content.stroke.dashOffset = std::isfinite(next)
              ? std::clamp(next, -1000000.0f, 1000000.0f) : 0.0f;
          handled = true;
        }
        if (handled) {
          impl_->shapeContents_[static_cast<size_t>(contentIndex)] =
              normalizedShapeContent(content);
          impl_->markDirty();
          impl_->localBoundsCacheDirty_ = true;
          impl_->shapeContentCacheDirty_ = true;
          Q_EMIT changed();
          return true;
        }
      }
    }
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
       } else if (auto merge =
                      dynamic_cast<ArtifactCore::MergePaths *>(op.get())) {
         if (field == "mode") {
           merge->setMode(std::clamp(value.toInt(), 0, 4));
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

static QJsonObject shapeContentToJson(const Artifact::ShapeContent& content) {
  QJsonObject obj;
  obj["name"] = content.name;
  obj["visible"] = content.visible;
  obj["opacity"] = static_cast<double>(content.opacity);
  obj["merge"] = static_cast<int>(content.merge);
  QJsonObject geom;
  geom["type"] = static_cast<int>(content.geometry.type);
  geom["width"] = content.geometry.width;
  geom["height"] = content.geometry.height;
  geom["cornerRadius"] = static_cast<double>(content.geometry.cornerRadius);
  geom["starPoints"] = content.geometry.starPoints;
  geom["starInnerRadius"] = static_cast<double>(content.geometry.starInnerRadius);
  geom["polygonSides"] = content.geometry.polygonSides;
  geom["polygonClosed"] = content.geometry.polygonClosed;
  QJsonArray polyPoints;
  for (const auto& point : content.geometry.polygonPoints) {
    QJsonObject p;
    p["x"] = point.x();
    p["y"] = point.y();
    polyPoints.push_back(p);
  }
  geom["polygonPoints"] = polyPoints;
  geom["pathClosed"] = content.geometry.pathClosed;
  geom["fillRule"] = static_cast<int>(content.geometry.fillRule);
  QJsonArray pathVerts;
  for (const auto& v : content.geometry.pathVertices) {
    QJsonObject vObj;
    vObj["px"] = v.pos.x();    vObj["py"] = v.pos.y();
    vObj["ix"] = v.inTangent.x(); vObj["iy"] = v.inTangent.y();
    vObj["ox"] = v.outTangent.x(); vObj["oy"] = v.outTangent.y();
    vObj["smooth"] = v.smooth;
    pathVerts.push_back(vObj);
  }
  geom["pathVertices"] = pathVerts;
  obj["geometry"] = geom;
  QJsonObject fill;
  fill["enabled"] = content.fill.enabled;
  fill["r"] = static_cast<double>(content.fill.color.r());
  fill["g"] = static_cast<double>(content.fill.color.g());
  fill["b"] = static_cast<double>(content.fill.color.b());
  fill["a"] = static_cast<double>(content.fill.color.a());
  fill["type"] = static_cast<int>(content.fill.type);
  fill["gradStartR"] = static_cast<double>(content.fill.gradientStart.r());
  fill["gradStartG"] = static_cast<double>(content.fill.gradientStart.g());
  fill["gradStartB"] = static_cast<double>(content.fill.gradientStart.b());
  fill["gradStartA"] = static_cast<double>(content.fill.gradientStart.a());
  fill["gradEndR"] = static_cast<double>(content.fill.gradientEnd.r());
  fill["gradEndG"] = static_cast<double>(content.fill.gradientEnd.g());
  fill["gradEndB"] = static_cast<double>(content.fill.gradientEnd.b());
  fill["gradEndA"] = static_cast<double>(content.fill.gradientEnd.a());
  fill["gradAngle"] = static_cast<double>(content.fill.gradientAngleDegrees);
  fill["gradCenterX"] = static_cast<double>(content.fill.gradientCenterX);
  fill["gradCenterY"] = static_cast<double>(content.fill.gradientCenterY);
  fill["gradRadius"] = static_cast<double>(content.fill.gradientRadius);
  obj["fill"] = fill;
  QJsonObject stroke;
  stroke["enabled"] = content.stroke.enabled;
  stroke["r"] = static_cast<double>(content.stroke.color.r());
  stroke["g"] = static_cast<double>(content.stroke.color.g());
  stroke["b"] = static_cast<double>(content.stroke.color.b());
  stroke["a"] = static_cast<double>(content.stroke.color.a());
  stroke["width"] = static_cast<double>(content.stroke.width);
  stroke["cap"] = static_cast<int>(content.stroke.cap);
  stroke["join"] = static_cast<int>(content.stroke.join);
  stroke["align"] = static_cast<int>(content.stroke.align);
  stroke["dash"] = dashPatternToString(content.stroke.dashPattern);
  stroke["dashOffset"] = static_cast<double>(content.stroke.dashOffset);
  stroke["taperStart"] = static_cast<double>(content.stroke.taperStart);
  stroke["taperEnd"] = static_cast<double>(content.stroke.taperEnd);
  stroke["gradEnabled"] = content.stroke.gradientEnabled;
  stroke["gradStartR"] = static_cast<double>(content.stroke.gradientStart.r());
  stroke["gradStartG"] = static_cast<double>(content.stroke.gradientStart.g());
  stroke["gradStartB"] = static_cast<double>(content.stroke.gradientStart.b());
  stroke["gradStartA"] = static_cast<double>(content.stroke.gradientStart.a());
  stroke["gradEndR"] = static_cast<double>(content.stroke.gradientEnd.r());
  stroke["gradEndG"] = static_cast<double>(content.stroke.gradientEnd.g());
  stroke["gradEndB"] = static_cast<double>(content.stroke.gradientEnd.b());
  stroke["gradEndA"] = static_cast<double>(content.stroke.gradientEnd.a());
  obj["stroke"] = stroke;
  return obj;
}

static Artifact::ShapeContent shapeContentFromJson(const QJsonObject& obj) {
  Artifact::ShapeContent content;
  content.name = obj["name"].toString();
  content.visible = obj["visible"].toBool(true);
  content.opacity = static_cast<float>(obj["opacity"].toDouble(1.0));
  content.merge = static_cast<Artifact::ShapeContentMerge>(
      std::clamp(obj["merge"].toInt(0), 0, 3));
  const QJsonObject geom = obj["geometry"].toObject();
  content.geometry.type = static_cast<Artifact::ShapeType>(
      std::clamp(geom["type"].toInt(0), 0, 6));
  content.geometry.width = geom["width"].toInt(200);
  content.geometry.height = geom["height"].toInt(200);
  content.geometry.cornerRadius = static_cast<float>(geom["cornerRadius"].toDouble(0.0));
  content.geometry.starPoints = geom["starPoints"].toInt(5);
  content.geometry.starInnerRadius =
      static_cast<float>(geom["starInnerRadius"].toDouble(0.382));
  content.geometry.polygonSides = geom["polygonSides"].toInt(6);
  content.geometry.polygonClosed = geom["polygonClosed"].toBool(true);
  for (const auto& val : geom["polygonPoints"].toArray()) {
    const QJsonObject p = val.toObject();
    content.geometry.polygonPoints.push_back(QPointF(p["x"].toDouble(), p["y"].toDouble()));
  }
  content.geometry.pathClosed = geom["pathClosed"].toBool(true);
  content.geometry.fillRule = geom["fillRule"].toInt(1) == 1
      ? ArtifactCore::PathFillRule::EvenOdd
      : ArtifactCore::PathFillRule::Winding;
  for (const auto& val : geom["pathVertices"].toArray()) {
    const QJsonObject vObj = val.toObject();
    Artifact::CustomPathVertex v;
    v.pos = QPointF(vObj["px"].toDouble(), vObj["py"].toDouble());
    v.inTangent = QPointF(vObj["ix"].toDouble(), vObj["iy"].toDouble());
    v.outTangent = QPointF(vObj["ox"].toDouble(), vObj["oy"].toDouble());
    v.smooth = vObj["smooth"].toBool(false);
    content.geometry.pathVertices.push_back(v);
  }
  const QJsonObject fill = obj["fill"].toObject();
  content.fill.enabled = fill["enabled"].toBool(true);
  content.fill.color = FloatColor(
      static_cast<float>(fill["r"].toDouble(1.0)),
      static_cast<float>(fill["g"].toDouble(1.0)),
      static_cast<float>(fill["b"].toDouble(1.0)),
      static_cast<float>(fill["a"].toDouble(1.0)));
  content.fill.type = static_cast<ArtifactSolidFillType>(
      std::clamp(fill["type"].toInt(0), 0, 5));
  content.fill.gradientStart = FloatColor(
      static_cast<float>(fill["gradStartR"].toDouble(1.0)),
      static_cast<float>(fill["gradStartG"].toDouble(1.0)),
      static_cast<float>(fill["gradStartB"].toDouble(1.0)),
      static_cast<float>(fill["gradStartA"].toDouble(1.0)));
  content.fill.gradientEnd = FloatColor(
      static_cast<float>(fill["gradEndR"].toDouble(0.0)),
      static_cast<float>(fill["gradEndG"].toDouble(0.0)),
      static_cast<float>(fill["gradEndB"].toDouble(0.0)),
      static_cast<float>(fill["gradEndA"].toDouble(1.0)));
  content.fill.gradientAngleDegrees = static_cast<float>(fill["gradAngle"].toDouble(0.0));
  content.fill.gradientCenterX = static_cast<float>(fill["gradCenterX"].toDouble(0.5));
  content.fill.gradientCenterY = static_cast<float>(fill["gradCenterY"].toDouble(0.5));
  content.fill.gradientRadius = static_cast<float>(fill["gradRadius"].toDouble(0.5));
  const QJsonObject stroke = obj["stroke"].toObject();
  content.stroke.enabled = stroke["enabled"].toBool(false);
  content.stroke.color = FloatColor(
      static_cast<float>(stroke["r"].toDouble(0.0)),
      static_cast<float>(stroke["g"].toDouble(0.0)),
      static_cast<float>(stroke["b"].toDouble(0.0)),
      static_cast<float>(stroke["a"].toDouble(1.0)));
  content.stroke.width = static_cast<float>(stroke["width"].toDouble(0.0));
  content.stroke.cap = static_cast<Artifact::StrokeCap>(std::clamp(stroke["cap"].toInt(0), 0, 2));
  content.stroke.join = static_cast<Artifact::StrokeJoin>(std::clamp(stroke["join"].toInt(0), 0, 2));
  content.stroke.align = static_cast<Artifact::StrokeAlign>(std::clamp(stroke["align"].toInt(0), 0, 2));
  content.stroke.dashPattern = stringToDashPattern(stroke["dash"].toString());
  content.stroke.dashOffset = static_cast<float>(stroke["dashOffset"].toDouble(0.0));
  content.stroke.taperStart = static_cast<float>(stroke["taperStart"].toDouble(1.0));
  content.stroke.taperEnd = static_cast<float>(stroke["taperEnd"].toDouble(1.0));
  content.stroke.gradientEnabled = stroke["gradEnabled"].toBool(false);
  content.stroke.gradientStart = FloatColor(
      static_cast<float>(stroke["gradStartR"].toDouble(0.0)),
      static_cast<float>(stroke["gradStartG"].toDouble(0.0)),
      static_cast<float>(stroke["gradStartB"].toDouble(0.0)),
      static_cast<float>(stroke["gradStartA"].toDouble(1.0)));
  content.stroke.gradientEnd = FloatColor(
      static_cast<float>(stroke["gradEndR"].toDouble(0.0)),
      static_cast<float>(stroke["gradEndG"].toDouble(0.0)),
      static_cast<float>(stroke["gradEndB"].toDouble(0.0)),
      static_cast<float>(stroke["gradEndA"].toDouble(1.0)));
  return normalizedShapeContent(content);
}

// ---- SVG interop (gap 8) ----

static QString svgNumber(double value) {
  if (!std::isfinite(value)) {
    return QStringLiteral("0");
  }
  if (std::abs(value) < 0.0005) {
    return QStringLiteral("0");
  }
  QString s = QString::number(value, 'f', 3);
  while (s.endsWith(QChar('0'))) {
    s.chop(1);
  }
  if (s.endsWith(QChar('.'))) {
    s.chop(1);
  }
  if (s.isEmpty() || s == QStringLiteral("-0")) {
    return QStringLiteral("0");
  }
  return s;
}

static QString svgColorRgb(const ArtifactCore::FloatColor& c, QString* opacityOut) {
  const auto channel = [](float v) {
    return std::clamp(static_cast<int>(std::round(v * 255.0f)), 0, 255);
  };
  if (opacityOut) {
    *opacityOut = svgNumber(std::clamp(c.a(), 0.0f, 1.0f));
  }
  return QStringLiteral("rgb(%1,%2,%3)").arg(channel(c.r())).arg(channel(c.g())).arg(channel(c.b()));
}

static QString svgPathData(const ArtifactCore::ShapePath& path) {
  QString d;
  d.reserve(path.commandCount() * 24);
  for (const auto& cmd : path.commands()) {
    switch (cmd.type) {
      case ArtifactCore::PathCommandType::MoveTo:
        d += QStringLiteral("M%1 %2 ").arg(svgNumber(cmd.points[0].x())).arg(svgNumber(cmd.points[0].y()));
        break;
      case ArtifactCore::PathCommandType::LineTo:
        d += QStringLiteral("L%1 %2 ").arg(svgNumber(cmd.points[0].x())).arg(svgNumber(cmd.points[0].y()));
        break;
      case ArtifactCore::PathCommandType::CubicTo:
        d += QStringLiteral("C%1 %2 %3 %4 %5 %6 ").arg(svgNumber(cmd.points[0].x())).arg(svgNumber(cmd.points[0].y())).arg(svgNumber(cmd.points[1].x())).arg(svgNumber(cmd.points[1].y())).arg(svgNumber(cmd.points[2].x())).arg(svgNumber(cmd.points[2].y()));
        break;
      case ArtifactCore::PathCommandType::QuadTo:
        d += QStringLiteral("Q%1 %2 %3 %4 ").arg(svgNumber(cmd.points[0].x())).arg(svgNumber(cmd.points[0].y())).arg(svgNumber(cmd.points[1].x())).arg(svgNumber(cmd.points[1].y()));
        break;
      case ArtifactCore::PathCommandType::Close:
        d += QStringLiteral("Z ");
        break;
    }
  }
  return d.trimmed();
}

// Gradients export as objectBoundingBox (content-local fractions), which
// round-trips through the importer below. Conical has no SVG equivalent
// and falls back to the start color.
static QString svgFillAttr(const Artifact::ShapeContentFill& fill, int contentIndex,
                           QString* defsOut) {
  if (!fill.enabled) {
    return QStringLiteral("none");
  }
  const int type = static_cast<int>(fill.type);
  if (type == 1 || type == 2 || type == 4 || type == 5) {
    const QString gid = QStringLiteral("ag%1f").arg(contentIndex);
    QString stops;
    QString so0;
    stops += QStringLiteral("<stop offset=\"0\" stop-color=\"%1\" stop-opacity=\"%2\"/>")
                 .arg(svgColorRgb(fill.gradientStart, &so0), so0);
    QString so1;
    stops += QStringLiteral("<stop offset=\"1\" stop-color=\"%1\" stop-opacity=\"%2\"/>")
                 .arg(svgColorRgb(fill.gradientEnd, &so1), so1);
    if (type == 1 || type == 4 || type == 5) {
      const float rad = fill.gradientAngleDegrees * 3.14159265f / 180.0f;
      const float dx = std::cos(rad) * 0.5f;
      const float dy = std::sin(rad) * 0.5f;
      QString spread;
      if (type == 4) {
        spread = QStringLiteral(" spreadMethod=\"repeat\"");
      } else if (type == 5) {
        spread = QStringLiteral(" spreadMethod=\"reflect\"");
      }
      *defsOut += QStringLiteral("<linearGradient id=\"%1\" x1=\"%2\" y1=\"%3\" x2=\"%4\" y2=\"%5\"%6>%7</linearGradient>")
                       .arg(gid)
                       .arg(svgNumber(fill.gradientCenterX - dx))
                       .arg(svgNumber(fill.gradientCenterY - dy))
                       .arg(svgNumber(fill.gradientCenterX + dx))
                       .arg(svgNumber(fill.gradientCenterY + dy))
                       .arg(spread, stops);
    } else {
      *defsOut += QStringLiteral("<radialGradient id=\"%1\" cx=\"%2\" cy=\"%3\" r=\"%4\">%5</radialGradient>")
                       .arg(gid)
                       .arg(svgNumber(fill.gradientCenterX))
                       .arg(svgNumber(fill.gradientCenterY))
                       .arg(svgNumber(fill.gradientRadius))
                       .arg(stops);
    }
    return QStringLiteral("url(#%1)").arg(gid);
  }
  QString opacity;
  const QString rgb = svgColorRgb(type == 0 ? fill.color : fill.gradientStart, &opacity);
  if (opacity != QStringLiteral("1")) {
    return rgb + QStringLiteral("|") + opacity;
  }
  return rgb;
}

QString ArtifactShapeLayer::shapeContentsToSvg() const {
  struct ExportItem {
    Artifact::ShapeContent content;
    std::vector<ArtifactCore::ShapePath> paths;
  };
  std::vector<ExportItem> items;
  if (impl_) {
    if (!impl_->shapeContents_.empty()) {
      ensureContentVisPaths();
      for (size_t ci = 0; ci < impl_->shapeContents_.size(); ++ci) {
        const auto& content = impl_->shapeContents_[ci];
        if (!content.visible || content.opacity <= 0.0f) {
          continue;
        }
        ExportItem item;
        item.content = content;
        if (ci < impl_->contentCache_.visPaths.size()) {
          item.paths = impl_->contentCache_.visPaths[ci];
        }
        if (!item.paths.empty()) {
          items.push_back(std::move(item));
        }
      }
    } else {
      ExportItem item;
      item.content = makeContentFromLegacy();
      auto paths = nativeShapePaths();
      if (hasCustomPath()) {
        for (auto& path : paths) {
          path.setFillRule(impl_->customPathFillRule_);
        }
      }
      item.paths = std::move(paths);
      if (!item.paths.empty()) {
        items.push_back(std::move(item));
      }
    }
  }
  QRectF bounds;
  for (const auto& item : items) {
    for (const auto& path : item.paths) {
      const QRectF pb = path.boundingRect();
      bounds = bounds.isNull() ? pb : bounds.united(pb);
    }
  }
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    bounds = QRectF(0.0, 0.0, 200.0, 200.0);
  }
  const float layerOpacity = opacity();
  QString defs;
  QString body;
  int contentIndex = 0;
  for (const auto& item : items) {
    const auto& content = item.content;
    const float itemOpacity = std::clamp(content.opacity * layerOpacity, 0.0f, 1.0f);
    for (const auto& path : item.paths) {
      const QString d = svgPathData(path);
      if (d.isEmpty()) {
        continue;
      }
      QString fillValue = svgFillAttr(content.fill, contentIndex, &defs);
      QString fillOpacityAttr;
      const int pipeAt = fillValue.indexOf(QChar('|'));
      if (pipeAt >= 0) {
        fillOpacityAttr = QStringLiteral(" fill-opacity=\"%1\"").arg(fillValue.mid(pipeAt + 1));
        fillValue = fillValue.left(pipeAt);
      }
      const char* fillRule = (path.fillRule() == ArtifactCore::PathFillRule::EvenOdd)
          ? "evenodd" : "nonzero";
      QString strokeValue = QStringLiteral("none");
      QString strokeAttrs;
      if (content.stroke.enabled && content.stroke.width > 0.0f) {
        QString strokeOpacity;
        const ArtifactCore::FloatColor mid = content.stroke.gradientEnabled
            ? mixColor(content.stroke.gradientStart, content.stroke.gradientEnd, 0.5f)
            : content.stroke.color;
        strokeValue = svgColorRgb(mid, &strokeOpacity);
        strokeAttrs += QStringLiteral(" stroke-opacity=\"%1\"").arg(strokeOpacity);
        strokeAttrs += QStringLiteral(" stroke-width=\"%1\"").arg(svgNumber(content.stroke.width));
        strokeAttrs += QStringLiteral(" stroke-linecap=\"%1\"").arg(
            content.stroke.cap == Artifact::StrokeCap::Round ? "round"
            : (content.stroke.cap == Artifact::StrokeCap::Square ? "square" : "butt"));
        strokeAttrs += QStringLiteral(" stroke-linejoin=\"%1\"").arg(
            content.stroke.join == Artifact::StrokeJoin::Round ? "round"
            : (content.stroke.join == Artifact::StrokeJoin::Bevel ? "bevel" : "miter"));
        if (!content.stroke.dashPattern.empty()) {
          QStringList dashes;
          for (float v : content.stroke.dashPattern) {
            dashes << svgNumber(v);
          }
          strokeAttrs += QStringLiteral(" stroke-dasharray=\"%1\"").arg(dashes.join(QChar(' ')));
          if (std::abs(content.stroke.dashOffset) > 0.0005f) {
            strokeAttrs += QStringLiteral(" stroke-dashoffset=\"%1\"").arg(svgNumber(content.stroke.dashOffset));
          }
        }
      }
      body += QStringLiteral("<g id=\"c%1\" opacity=\"%2\"><path d=\"%3\" fill=\"%4\"%5 fill-rule=\"%6\" stroke=\"%7\"%8/></g>")
                  .arg(contentIndex)
                  .arg(svgNumber(itemOpacity))
                  .arg(d)
                  .arg(fillValue)
                  .arg(fillOpacityAttr)
                  .arg(QString::fromLatin1(fillRule))
                  .arg(strokeValue)
                  .arg(strokeAttrs);
    }
    ++contentIndex;
  }
  // Gradients export as objectBoundingBox fractions; the importer maps
  // them back onto content-local gradient parameters (approximate for
  // custom paths whose bounds differ from width/height).
  const QString defsBlock =
      defs.isEmpty() ? QString() : QStringLiteral("<defs>%1</defs>").arg(defs);
  return QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"%3 %4 %5 %6\">%7%8</svg>")
      .arg(svgNumber(bounds.width()))
      .arg(svgNumber(bounds.height()))
      .arg(svgNumber(bounds.left()))
      .arg(svgNumber(bounds.top()))
      .arg(svgNumber(bounds.width()))
      .arg(svgNumber(bounds.height()))
      .arg(defsBlock)
      .arg(body);
}

// ---- SVG import (gap 8) ----

namespace SvgImport {

struct GradientStop {
  double offset = 0.0;
  ArtifactCore::FloatColor color = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
};

struct Gradient {
  bool radial = false;
  bool userSpace = false;
  double x1 = 0.0, y1 = 0.0, x2 = 1.0, y2 = 0.0;
  double cx = 0.5, cy = 0.5, r = 0.5;
  int spread = 0; // 0 pad, 1 repeat, 2 reflect
  std::vector<GradientStop> stops;
};

struct Paint {
  bool fillNone = true;
  bool fillUrl = false;
  QString fillUrlId;
  bool strokeUrl = false;
  QString strokeUrlId;
  ArtifactCore::FloatColor fillColor = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  bool strokeNone = true;
  ArtifactCore::FloatColor strokeColor = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  double strokeWidth = 1.0;
  int lineCap = 0; // 0 butt, 1 round, 2 square
  int lineJoin = 0; // 0 miter, 1 round, 2 bevel
  std::vector<float> dash;
  double dashOffset = 0.0;
  double opacity = 1.0;
  int fillRule = 0; // 0 nonzero, 1 evenodd
};

struct Pending {
  Artifact::ShapeContent content;
  Paint paint;
  QString name;
};

static std::vector<double> parseSvgNumbers(const QString& s) {
  std::vector<double> out;
  QString token;
  auto flush = [&]() {
    if (token.isEmpty()) {
      return;
    }
    bool ok = false;
    const double v = token.toDouble(&ok);
    if (ok && std::isfinite(v)) {
      out.push_back(v);
    }
    token.clear();
  };
  for (int i = 0; i < s.size(); ++i) {
    const QChar c = s[i];
    if (c == QChar(',') || c.isSpace()) {
      flush();
      continue;
    }
    if ((c == QChar('-') || c == QChar('+')) && !token.isEmpty()) {
      const QChar last = token[token.size() - 1];
      if (last != QChar('e') && last != QChar('E')) {
        flush();
      }
    }
    if (c == QChar('.') && token.contains(QChar('.'))) {
      flush();
    }
    token += c;
  }
  flush();
  return out;
}

static bool parseSvgColor(const QString& text, ArtifactCore::FloatColor* color) {
  if (!color) {
    return false;
  }
  const QString s = text.trimmed().toLower();
  if (s.isEmpty() || s == QStringLiteral("none")) {
    return false;
  }
  if (s == QStringLiteral("transparent")) {
    *color = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f);
    return true;
  }
  if (s[0] == QChar('#')) {
    const QString h = s.mid(1);
    bool ok = false;
    auto byte = [&](int pos, int len, int fallback) {
      bool partOk = false;
      const int v = h.mid(pos, len).toInt(&partOk, 16);
      if (!partOk) {
        ok = false;
      }
      return partOk ? v : fallback;
    };
    ok = true;
    if (h.size() == 3) {
      const int r = byte(0, 1, 0), g = byte(1, 1, 0), b = byte(2, 1, 0);
      if (!ok) {
        return false;
      }
      *color = ArtifactCore::FloatColor(r / 15.0f, g / 15.0f, b / 15.0f, 1.0f);
      return true;
    }
    if (h.size() == 6 || h.size() == 8) {
      const int r = byte(0, 2, 0), g = byte(2, 2, 0), b = byte(4, 2, 0);
      const int a = h.size() == 8 ? byte(6, 2, 255) : 255;
      if (!ok) {
        return false;
      }
      *color = ArtifactCore::FloatColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
      return true;
    }
    return false;
  }
  if (s.startsWith(QStringLiteral("rgb(")) || s.startsWith(QStringLiteral("rgba("))) {
    const int open = s.indexOf(QChar('('));
    const int close = s.lastIndexOf(QChar(')'));
    if (open < 0 || close < open) {
      return false;
    }
    const auto nums = parseSvgNumbers(s.mid(open + 1, close - open - 1));
    if (nums.size() < 3) {
      return false;
    }
    auto channel = [](double v) {
      return static_cast<float>(std::clamp(v / 255.0, 0.0, 1.0));
    };
    float alpha = 1.0f;
    if (nums.size() >= 4) {
      alpha = static_cast<float>(std::clamp(nums[3] <= 1.0 ? nums[3] : nums[3] / 100.0, 0.0, 1.0));
    }
    *color = ArtifactCore::FloatColor(channel(nums[0]), channel(nums[1]), channel(nums[2]), alpha);
    return true;
  }
  static const std::pair<const char*, unsigned> kNames[] = {
      {"black", 0x000000}, {"white", 0xFFFFFF}, {"red", 0xFF0000},
      {"green", 0x008000}, {"lime", 0x00FF00}, {"blue", 0x0000FF},
      {"yellow", 0xFFFF00}, {"cyan", 0x00FFFF}, {"aqua", 0x00FFFF},
      {"magenta", 0xFF00FF}, {"fuchsia", 0xFF00FF}, {"gray", 0x808080},
      {"grey", 0x808080}, {"silver", 0xC0C0C0}, {"maroon", 0x800000},
      {"olive", 0x808000}, {"navy", 0x000080}, {"teal", 0x008080},
      {"purple", 0x800080}, {"orange", 0xFFA500}, {"brown", 0xA52A2A},
      {"pink", 0xFFC0CB},
  };
  for (const auto& entry : kNames) {
    if (s == QLatin1String(entry.first)) {
      const unsigned v = entry.second;
      *color = ArtifactCore::FloatColor(((v >> 16) & 255) / 255.0f,
                                        ((v >> 8) & 255) / 255.0f,
                                        (v & 255) / 255.0f, 1.0f);
      return true;
    }
  }
  return false;
}

static double svgAttrNumber(const QString& s, double fallback) {
  // Leading-numeric parse so unit suffixes ("2px", "12pt") still convert.
  const QString t = s.trimmed();
  int len = 0;
  while (len < t.size()) {
    const QChar c = t[len];
    if (c.isDigit() || c == QChar('.') || c == QChar('-') || c == QChar('+') ||
        c == QChar('e') || c == QChar('E')) {
      ++len;
    } else {
      break;
    }
  }
  if (len <= 0) {
    return fallback;
  }
  bool ok = false;
  const double v = t.left(len).toDouble(&ok);
  return (ok && std::isfinite(v)) ? v : fallback;
}

static QTransform parseSvgTransform(const QString& text) {
  QTransform result;
  int pos = 0;
  const int n = text.size();
  while (pos < n) {
    while (pos < n && (text[pos].isSpace() || text[pos] == QChar(','))) {
      ++pos;
    }
    const int nameStart = pos;
    while (pos < n && text[pos].isLetter()) {
      ++pos;
    }
    const QString name = text.mid(nameStart, pos - nameStart).trimmed().toLower();
    while (pos < n && text[pos].isSpace()) {
      ++pos;
    }
    if (name.isEmpty()) {
      break;
    }
    if (pos >= n || text[pos] != QChar('(')) {
      continue;
    }
    int depth = 0;
    const int argStart = pos;
    while (pos < n) {
      if (text[pos] == QChar('(')) {
        ++depth;
      } else if (text[pos] == QChar(')')) {
        --depth;
        if (depth == 0) {
          break;
        }
      }
      ++pos;
    }
    const QString argsText = text.mid(argStart + 1, pos - argStart - 1);
    if (pos < n) {
      ++pos;
    }
    const auto args = parseSvgNumbers(argsText);
    QTransform local;
    bool valid = true;
    if (name == QStringLiteral("translate")) {
      local.translate(args.size() > 0 ? args[0] : 0.0,
                      args.size() > 1 ? args[1] : 0.0);
    } else if (name == QStringLiteral("scale")) {
      const double sx = args.size() > 0 ? args[0] : 1.0;
      local.scale(sx, args.size() > 1 ? args[1] : sx);
    } else if (name == QStringLiteral("rotate")) {
      const double a = args.empty() ? 0.0 : args[0];
      if (args.size() >= 3) {
        local = QTransform().translate(args[1], args[2]).rotate(a).translate(-args[1], -args[2]);
      } else {
        local.rotate(a);
      }
    } else if (name == QStringLiteral("skewx")) {
      local.shear(args.empty() ? 0.0 : std::tan(args[0] * 3.14159265 / 180.0), 0.0);
    } else if (name == QStringLiteral("skewy")) {
      local.shear(0.0, args.empty() ? 0.0 : std::tan(args[0] * 3.14159265 / 180.0));
    } else if (name == QStringLiteral("matrix") && args.size() >= 6) {
      local = QTransform(args[0], args[1], args[2], args[3], args[4], args[5]);
    } else {
      valid = false;
    }
    if (valid) {
      result = result * local;
    }
  }
  return result;
}

static Artifact::CustomPathVertex svgLineVertex(const QPointF& p) {
  Artifact::CustomPathVertex v;
  v.pos = p;
  v.smooth = false;
  return v;
}

static Artifact::CustomPathVertex svgCurveVertex(const QPointF& p) {
  Artifact::CustomPathVertex v;
  v.pos = p;
  v.smooth = true;
  return v;
}

static Artifact::CustomPathVertex svgTransformedVertex(const QTransform& xf,
                                                       const Artifact::CustomPathVertex& v) {
  Artifact::CustomPathVertex out = v;
  const QPointF mappedPos = xf.map(v.pos);
  out.outTangent = xf.map(v.pos + v.outTangent) - mappedPos;
  out.inTangent = xf.map(v.pos + v.inTangent) - mappedPos;
  out.pos = mappedPos;
  return out;
}

// W3C SVG F.6.5 endpoint parametrization + <=90 degree cubic spans.
static void appendSvgArc(ArtifactCore::ShapePath& path, const QPointF& p0,
                         double rx, double ry, double rotDeg,
                         bool largeArc, bool sweep, const QPointF& p1) {
  rx = std::abs(rx);
  ry = std::abs(ry);
  if (rx <= 1e-9 || ry <= 1e-9 || p0 == p1) {
    path.lineTo(p1);
    return;
  }
  const double rot = rotDeg * 3.14159265 / 180.0;
  const double cosR = std::cos(rot);
  const double sinR = std::sin(rot);
  const double dx = (p0.x() - p1.x()) * 0.5;
  const double dy = (p0.y() - p1.y()) * 0.5;
  double x1p = cosR * dx + sinR * dy;
  double y1p = -sinR * dx + cosR * dy;
  double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
  if (lambda > 1.0) {
    const double s = std::sqrt(lambda);
    rx *= s;
    ry *= s;
    x1p = cosR * dx + sinR * dy;
    y1p = -sinR * dx + cosR * dy;
  }
  double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
  double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
  double factor = (den <= 1e-12) ? 0.0 : std::sqrt(std::max(0.0, num / den));
  if (largeArc == sweep) {
    factor = -factor;
  }
  const double cxp = factor * rx * y1p / ry;
  const double cyp = -factor * ry * x1p / rx;
  const double cx = cosR * cxp - sinR * cyp + (p0.x() + p1.x()) * 0.5;
  const double cy = sinR * cxp + cosR * cyp + (p0.y() + p1.y()) * 0.5;
  auto angleOf = [](double ux, double uy, double vx, double vy) {
    const double dot = ux * vx + uy * vy;
    const double lu = std::hypot(ux, uy);
    const double lv = std::hypot(vx, vy);
    if (lu <= 1e-12 || lv <= 1e-12) {
      return 0.0;
    }
    double c = std::clamp(dot / (lu * lv), -1.0, 1.0);
    double a = std::acos(c);
    if (ux * vy - uy * vx < 0.0) {
      a = -a;
    }
    return a;
  };
  double startAngle = angleOf(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
  double sweepAngle = angleOf((x1p - cxp) / rx, (y1p - cyp) / ry,
                              (-x1p - cxp) / rx, (-y1p - cyp) / ry);
  if (!sweep && sweepAngle > 0.0) {
    sweepAngle -= 2.0 * 3.14159265;
  } else if (sweep && sweepAngle < 0.0) {
    sweepAngle += 2.0 * 3.14159265;
  }
  const int spans = std::max(1, static_cast<int>(
      std::ceil(std::abs(sweepAngle) / (3.14159265 * 0.5))));
  const double delta = sweepAngle / spans;
  auto pointAt = [&](double angle) {
    return QPointF(cx + rx * cosR * std::cos(angle) - ry * sinR * std::sin(angle),
                   cy + rx * sinR * std::cos(angle) + ry * cosR * std::sin(angle));
  };
  auto tangentAt = [&](double angle) {
    return QPointF(-rx * cosR * std::sin(angle) - ry * sinR * std::cos(angle),
                   -rx * sinR * std::sin(angle) + ry * cosR * std::cos(angle));
  };
  for (int i = 0; i < spans; ++i) {
    const double a0 = startAngle + delta * i;
    const double a1 = a0 + delta;
    const double k = 4.0 / 3.0 * std::tan((a1 - a0) / 4.0);
    const QPointF q0 = pointAt(a0);
    const QPointF q1 = pointAt(a1);
    const QPointF t0 = tangentAt(a0);
    const QPointF t1 = tangentAt(a1);
    path.cubicTo(q0 + t0 * k, q1 - t1 * k, q1);
  }
}

struct PathScan {
  const QString& s;
  int pos = 0;
  void skipSep() {
    while (pos < s.size() && (s[pos].isSpace() || s[pos] == QChar(','))) {
      ++pos;
    }
  }
  bool readCommand(QChar* cmd) {
    skipSep();
    if (pos >= s.size()) {
      return false;
    }
    if (!s[pos].isLetter()) {
      return false;
    }
    *cmd = s[pos++];
    return true;
  }
  bool readNumber(double* v) {
    skipSep();
    if (pos >= s.size()) {
      return false;
    }
    const int start = pos;
    if (s[pos] == QChar('-') || s[pos] == QChar('+')) {
      ++pos;
    }
    bool any = false;
    while (pos < s.size() && s[pos].isDigit()) {
      ++pos;
      any = true;
    }
    if (pos < s.size() && s[pos] == QChar('.')) {
      ++pos;
      while (pos < s.size() && s[pos].isDigit()) {
        ++pos;
        any = true;
      }
    }
    if (pos < s.size() && (s[pos] == QChar('e') || s[pos] == QChar('E'))) {
      const int epos = pos++;
      if (pos < s.size() && (s[pos] == QChar('-') || s[pos] == QChar('+'))) {
        ++pos;
      }
      bool ed = false;
      while (pos < s.size() && s[pos].isDigit()) {
        ++pos;
        ed = true;
      }
      if (!ed) {
        pos = epos;
      }
    }
    if (!any) {
      return false;
    }
    bool ok = false;
    const double val = s.mid(start, pos - start).toDouble(&ok);
    if (!ok || !std::isfinite(val)) {
      return false;
    }
    *v = val;
    return true;
  }
};

static ArtifactCore::ShapePath parseSvgPathData(const QString& d) {
  ArtifactCore::ShapePath path;
  PathScan scan{d};
  QChar cmd = QChar(' ');
  QPointF current;
  QPointF subStart;
  QPointF prevCubicCp2;
  QPointF prevQuadCp;
  bool prevCubic = false;
  bool prevQuad = false;
  bool hasCurrent = false;
  int guard = 0;
  auto line = [&](const QPointF& p) {
    if (!hasCurrent) {
      path.moveTo(p);
      subStart = p;
    } else {
      path.lineTo(p);
    }
    current = p;
    hasCurrent = true;
    prevCubic = prevQuad = false;
  };
  while (guard++ < 100000) {
    QChar next;
    if (scan.readCommand(&next)) {
      cmd = next;
    } else {
      break;
    }
    bool relative = cmd.isLower();
    QChar base = cmd.toUpper();
    if (base == QChar('Z')) {
      if (hasCurrent) {
        path.close();
        current = subStart;
        prevCubic = prevQuad = false;
      }
      cmd = QChar(' ');
      continue;
    }
    int need = 0;
    if (base == QChar('M') || base == QChar('L') || base == QChar('T')) {
      need = 2;
    } else if (base == QChar('H') || base == QChar('V')) {
      need = 1;
    } else if (base == QChar('C')) {
      need = 6;
    } else if (base == QChar('S') || base == QChar('Q')) {
      need = 4;
    } else if (base == QChar('A')) {
      need = 7;
    } else {
      break;
    }
    bool firstMove = (base == QChar('M'));
    while (guard++ < 100000) {
      double v[7] = {0, 0, 0, 0, 0, 0, 0};
      int got = 0;
      for (; got < need; ++got) {
        if (!scan.readNumber(&v[got])) {
          break;
        }
      }
      if (got < need) {
        break;
      }
      auto absPoint = [&](double x, double y) {
        return relative ? current + QPointF(x, y) : QPointF(x, y);
      };
      if (base == QChar('M')) {
        const QPointF p = absPoint(v[0], v[1]);
        path.moveTo(p);
        current = p;
        subStart = p;
        hasCurrent = true;
        prevCubic = prevQuad = false;
        if (firstMove) {
          // Subsequent pairs after moveto are implicit linetos
          // (relative flag carries over from m/M).
          firstMove = false;
          base = QChar('L');
          need = 2;
        }
      } else if (base == QChar('L')) {
        line(absPoint(v[0], v[1]));
      } else if (base == QChar('T')) {
        const QPointF cp = prevQuad ? current * 2.0 - prevQuadCp : current;
        const QPointF p = absPoint(v[0], v[1]);
        if (!hasCurrent) {
          path.moveTo(p);
        } else {
          path.quadTo(cp, p);
        }
        prevQuadCp = cp;
        prevQuad = true;
        prevCubic = false;
        current = p;
        hasCurrent = true;
      } else if (base == QChar('H')) {
        const QPointF p = relative ? current + QPointF(v[0], 0.0) : QPointF(v[0], current.y());
        line(p);
      } else if (base == QChar('V')) {
        const QPointF p = relative ? current + QPointF(0.0, v[0]) : QPointF(current.x(), v[0]);
        line(p);
      } else if (base == QChar('C')) {
        const QPointF c1 = absPoint(v[0], v[1]);
        const QPointF c2 = absPoint(v[2], v[3]);
        const QPointF p = absPoint(v[4], v[5]);
        if (!hasCurrent) {
          path.moveTo(p);
        } else {
          path.cubicTo(c1, c2, p);
        }
        prevCubicCp2 = c2;
        prevCubic = true;
        prevQuad = false;
        current = p;
        hasCurrent = true;
      } else if (base == QChar('S')) {
        const QPointF c1 = prevCubic ? current * 2.0 - prevCubicCp2 : current;
        const QPointF c2 = absPoint(v[0], v[1]);
        const QPointF p = absPoint(v[2], v[3]);
        if (!hasCurrent) {
          path.moveTo(p);
        } else {
          path.cubicTo(c1, c2, p);
        }
        prevCubicCp2 = c2;
        prevCubic = true;
        prevQuad = false;
        current = p;
        hasCurrent = true;
      } else if (base == QChar('Q')) {
        const QPointF cp = absPoint(v[0], v[1]);
        const QPointF p = absPoint(v[2], v[3]);
        if (!hasCurrent) {
          path.moveTo(p);
        } else {
          path.quadTo(cp, p);
        }
        prevQuadCp = cp;
        prevQuad = true;
        prevCubic = false;
        current = p;
        hasCurrent = true;
      } else if (base == QChar('A')) {
        const QPointF p = absPoint(v[5], v[6]);
        if (!hasCurrent) {
          path.moveTo(p);
        } else {
          appendSvgArc(path, current, v[0], v[1], v[2], v[3] != 0.0, v[4] != 0.0, p);
        }
        prevCubic = prevQuad = false;
        current = p;
        hasCurrent = true;
      }
      PathScan probe = scan;
      double peek = 0.0;
      if (!probe.readNumber(&peek)) {
        break;
      }
    }
    cmd = QChar(' ');
  }
  return path;
}

static void applySvgPaintAttr(Paint* paint, const QString& name, const QString& value) {
  if (!paint) {
    return;
  }
  const QString key = name.trimmed().toLower();
  const QString val = value.trimmed();
  if (key == QStringLiteral("fill")) {
    if (val.startsWith(QStringLiteral("url("))) {
      const int hash = val.indexOf(QChar('#'));
      const int end = val.indexOf(QChar(')'), hash);
      if (hash >= 0 && end > hash) {
        paint->fillNone = false;
        paint->fillUrl = true;
        paint->fillUrlId = val.mid(hash + 1, end - hash - 1);
      }
    } else {
      ArtifactCore::FloatColor c(0.0f, 0.0f, 0.0f, 1.0f);
      if (parseSvgColor(val, &c)) {
        paint->fillNone = false;
        paint->fillUrl = false;
        paint->fillColor = c;
      } else {
        paint->fillNone = true;
        paint->fillUrl = false;
      }
    }
  } else if (key == QStringLiteral("stroke")) {
    if (val.startsWith(QStringLiteral("url("))) {
      const int hash = val.indexOf(QChar('#'));
      const int end = val.indexOf(QChar(')'), hash);
      if (hash >= 0 && end > hash) {
        paint->strokeNone = false;
        paint->strokeUrl = true;
        paint->strokeUrlId = val.mid(hash + 1, end - hash - 1);
      }
      return;
    }
    ArtifactCore::FloatColor c(0.0f, 0.0f, 0.0f, 1.0f);
    if (parseSvgColor(val, &c)) {
      paint->strokeNone = false;
      paint->strokeUrl = false;
      paint->strokeColor = c;
    } else {
      paint->strokeNone = true;
      paint->strokeUrl = false;
    }
  } else if (key == QStringLiteral("stroke-width")) {
    paint->strokeWidth = std::max(0.0, svgAttrNumber(val, 1.0));
  } else if (key == QStringLiteral("stroke-linecap")) {
    const QString v = val.toLower();
    paint->lineCap = (v == QStringLiteral("round")) ? 1 : ((v == QStringLiteral("square")) ? 2 : 0);
  } else if (key == QStringLiteral("stroke-linejoin")) {
    const QString v = val.toLower();
    paint->lineJoin = (v == QStringLiteral("round")) ? 1 : ((v == QStringLiteral("bevel")) ? 2 : 0);
  } else if (key == QStringLiteral("stroke-dasharray")) {
    paint->dash.clear();
    if (val.toLower() != QStringLiteral("none")) {
      for (double v : parseSvgNumbers(val)) {
        if (paint->dash.size() >= 64) {
          break;
        }
        if (std::isfinite(v) && v > 0.001) {
          paint->dash.push_back(static_cast<float>(v));
        }
      }
      if (paint->dash.size() % 2 == 1) {
        const size_t n = paint->dash.size();
        for (size_t i = 0; i < n; ++i) {
          paint->dash.push_back(paint->dash[i]);
        }
      }
    }
  } else if (key == QStringLiteral("stroke-dashoffset")) {
    paint->dashOffset = svgAttrNumber(val, 0.0);
  } else if (key == QStringLiteral("stroke-opacity") || key == QStringLiteral("fill-opacity")) {
    const double a = std::clamp(svgAttrNumber(val, 1.0), 0.0, 1.0);
    if (key == QStringLiteral("stroke-opacity")) {
      paint->strokeColor = ArtifactCore::FloatColor(paint->strokeColor.r(), paint->strokeColor.g(),
                                                    paint->strokeColor.b(),
                                                    paint->strokeColor.a() * static_cast<float>(a));
    } else {
      paint->fillColor = ArtifactCore::FloatColor(paint->fillColor.r(), paint->fillColor.g(),
                                                  paint->fillColor.b(),
                                                  paint->fillColor.a() * static_cast<float>(a));
    }
  } else if (key == QStringLiteral("opacity")) {
    paint->opacity *= std::clamp(svgAttrNumber(val, 1.0), 0.0, 1.0);
  } else if (key == QStringLiteral("fill-rule")) {
    paint->fillRule = (val.toLower() == QStringLiteral("evenodd")) ? 1 : 0;
  }
}

static void applySvgStyleText(Paint* paint, const QString& styleText) {
  const auto decls = styleText.split(QChar(';'));
  for (const auto& decl : decls) {
    const int colon = decl.indexOf(QChar(':'));
    if (colon < 0) {
      continue;
    }
    applySvgPaintAttr(paint, decl.left(colon), decl.mid(colon + 1));
  }
}

static void applySvgAttributes(Paint* paint, const QXmlStreamAttributes& attrs) {
  static const char* kKeys[] = {"fill", "stroke", "stroke-width", "stroke-linecap",
                                "stroke-linejoin", "stroke-dasharray", "stroke-dashoffset",
                                "stroke-opacity", "fill-opacity", "opacity", "fill-rule"};
  for (const char* key : kKeys) {
    if (attrs.hasAttribute(QLatin1String(key))) {
      applySvgPaintAttr(paint, QLatin1String(key), attrs.value(QLatin1String(key)).toString());
    }
  }
  if (attrs.hasAttribute(QStringLiteral("style"))) {
    applySvgStyleText(paint, attrs.value(QStringLiteral("style")).toString());
  }
}

static Gradient parseSvgGradientElement(QXmlStreamReader* xml, bool radial) {
  Gradient grad;
  grad.radial = radial;
  const auto attrs = xml->attributes();
  const QString units = attrs.value(QStringLiteral("gradientUnits")).toString().trimmed();
  grad.userSpace = (units == QStringLiteral("userSpaceOnUse"));
  const QString spread = attrs.value(QStringLiteral("spreadMethod")).toString().trimmed().toLower();
  grad.spread = (spread == QStringLiteral("repeat")) ? 1 : ((spread == QStringLiteral("reflect")) ? 2 : 0);
  if (radial) {
    grad.cx = svgAttrNumber(attrs.value(QStringLiteral("cx")).toString(), 0.5);
    grad.cy = svgAttrNumber(attrs.value(QStringLiteral("cy")).toString(), 0.5);
    grad.r = svgAttrNumber(attrs.value(QStringLiteral("r")).toString(), 0.5);
  } else {
    grad.x1 = svgAttrNumber(attrs.value(QStringLiteral("x1")).toString(), 0.0);
    grad.y1 = svgAttrNumber(attrs.value(QStringLiteral("y1")).toString(), 0.0);
    grad.x2 = svgAttrNumber(attrs.value(QStringLiteral("x2")).toString(), 1.0);
    grad.y2 = svgAttrNumber(attrs.value(QStringLiteral("y2")).toString(), 0.0);
  }
  const QString elementName = radial ? QStringLiteral("radialGradient") : QStringLiteral("linearGradient");
  while (!xml->atEnd()) {
    const auto token = xml->readNext();
    if (token == QXmlStreamReader::EndElement) {
      const QString name = xml->name().toString();
      if (name == elementName) {
        break;
      }
      continue;
    }
    if (token != QXmlStreamReader::StartElement || xml->name() != QLatin1String("stop")) {
      continue;
    }
    const auto stopAttrs = xml->attributes();
    GradientStop stop;
    QString offsetText = stopAttrs.value(QStringLiteral("offset")).toString().trimmed();
    if (offsetText.endsWith(QChar('%'))) {
      offsetText.chop(1);
      stop.offset = std::clamp(svgAttrNumber(offsetText, 0.0) / 100.0, 0.0, 1.0);
    } else {
      stop.offset = std::clamp(svgAttrNumber(offsetText, 0.0), 0.0, 1.0);
    }
    ArtifactCore::FloatColor c(0.0f, 0.0f, 0.0f, 1.0f);
    const QString stopColor = stopAttrs.value(QStringLiteral("stop-color")).toString();
    if (!stopColor.isEmpty() && parseSvgColor(stopColor, &c)) {
      stop.color = c;
    }
    Paint stopPaint;
    stopPaint.fillColor = stop.color;
    // applySvgAttributes covers presentation attributes and style="" text.
    applySvgAttributes(&stopPaint, stopAttrs);
    stop.color = stopPaint.fillColor;
    const QString stopOpacity = stopAttrs.value(QStringLiteral("stop-opacity")).toString();
    if (!stopOpacity.isEmpty()) {
      stop.color = ArtifactCore::FloatColor(stop.color.r(), stop.color.g(), stop.color.b(),
                                            stop.color.a() * static_cast<float>(std::clamp(svgAttrNumber(stopOpacity, 1.0), 0.0, 1.0)));
    }
    grad.stops.push_back(stop);
  }
  std::sort(grad.stops.begin(), grad.stops.end(),
            [](const GradientStop& a, const GradientStop& b) { return a.offset < b.offset; });
  return grad;
}

static void resolveSvgGradientUrl(const std::map<QString, Gradient>& gradients,
                                  const QString& urlId, const QRectF& bbox,
                                  Artifact::ShapeContentFill* fill) {
  if (!fill) {
    return;
  }
  const auto it = gradients.find(urlId);
  if (it == gradients.end() || it->second.stops.empty()) {
    fill->enabled = false;
    return;
  }
  const Gradient& grad = it->second;
  const auto& first = grad.stops.front();
  const auto& last = grad.stops.back();
  fill->enabled = true;
  fill->gradientStart = first.color;
  fill->gradientEnd = last.color;
  if (grad.radial) {
    fill->type = ArtifactSolidFillType::RadialGradient;
    if (grad.userSpace && bbox.width() > 0.0 && bbox.height() > 0.0) {
      fill->gradientCenterX = static_cast<float>(std::clamp((grad.cx - bbox.left()) / bbox.width(), 0.0, 1.0));
      fill->gradientCenterY = static_cast<float>(std::clamp((grad.cy - bbox.top()) / bbox.height(), 0.0, 1.0));
      fill->gradientRadius = static_cast<float>(std::clamp(grad.r / std::max(bbox.width(), bbox.height()), 0.0, 1.0));
    } else {
      fill->gradientCenterX = static_cast<float>(std::clamp(grad.cx, 0.0, 1.0));
      fill->gradientCenterY = static_cast<float>(std::clamp(grad.cy, 0.0, 1.0));
      fill->gradientRadius = static_cast<float>(std::clamp(grad.r, 0.0, 1.0));
    }
  } else {
    if (grad.spread == 1) {
      fill->type = ArtifactSolidFillType::RepeatingGradient;
    } else if (grad.spread == 2) {
      fill->type = ArtifactSolidFillType::MirroredGradient;
    } else {
      fill->type = ArtifactSolidFillType::LinearGradient;
    }
    double x1 = grad.x1, y1 = grad.y1, x2 = grad.x2, y2 = grad.y2;
    if (grad.userSpace && bbox.width() > 0.0 && bbox.height() > 0.0) {
      x1 = (x1 - bbox.left()) / bbox.width();
      y1 = (y1 - bbox.top()) / bbox.height();
      x2 = (x2 - bbox.left()) / bbox.width();
      y2 = (y2 - bbox.top()) / bbox.height();
    }
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    fill->gradientAngleDegrees = static_cast<float>(std::atan2(dy, dx) * 180.0 / 3.14159265);
    fill->gradientCenterX = static_cast<float>(std::clamp((x1 + x2) * 0.5, 0.0, 1.0));
    fill->gradientCenterY = static_cast<float>(std::clamp((y1 + y2) * 0.5, 0.0, 1.0));
  }
}

static QRectF svgContentBounds(const Artifact::ShapeContent& content) {
  QRectF bounds;
  bool first = true;
  auto include = [&](const QPointF& p) {
    if (first) {
      bounds = QRectF(p, QSizeF(0, 0));
      first = false;
    } else {
      bounds = bounds.united(QRectF(p, QSizeF(0, 0)));
    }
  };
  for (const auto& v : content.geometry.pathVertices) {
    include(v.pos);
  }
  for (const auto& p : content.geometry.polygonPoints) {
    include(p);
  }
  return first ? QRectF() : bounds;
}

static void normalizeSvgContent(Artifact::ShapeContent* content) {
  if (!content) {
    return;
  }
  const QRectF bounds = svgContentBounds(*content);
  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    return;
  }
  const QPointF origin = bounds.topLeft();
  for (auto& v : content->geometry.pathVertices) {
    v.pos -= origin;
    // Tangents are relative; translation does not affect them.
  }
  for (auto& p : content->geometry.polygonPoints) {
    p -= origin;
  }
  content->geometry.width = std::max(1, static_cast<int>(std::ceil(bounds.width())));
  content->geometry.height = std::max(1, static_cast<int>(std::ceil(bounds.height())));
}

static Artifact::ShapeContent finishSvgContent(Artifact::ShapeContent content,
                                               const Paint& paint,
                                               const QString& name,
                                               const std::map<QString, Gradient>& gradients) {
  content.name = name;
  content.opacity = static_cast<float>(std::clamp(paint.opacity, 0.0, 1.0));
  content.fill.enabled = !paint.fillNone;
  content.fill.color = paint.fillColor;
  content.fill.type = ArtifactSolidFillType::Solid;
  content.stroke.enabled = !paint.strokeNone && paint.strokeWidth > 0.0;
  content.stroke.color = paint.strokeColor;
  content.stroke.width = static_cast<float>(std::max(0.0, paint.strokeWidth));
  content.stroke.cap = static_cast<Artifact::StrokeCap>(std::clamp(paint.lineCap, 0, 2));
  content.stroke.join = static_cast<Artifact::StrokeJoin>(std::clamp(paint.lineJoin, 0, 2));
  content.stroke.align = Artifact::StrokeAlign::Center;
  content.stroke.dashPattern = paint.dash;
  content.stroke.dashOffset = static_cast<float>(paint.dashOffset);
  content.stroke.taperStart = 1.0f;
  content.stroke.taperEnd = 1.0f;
  content.stroke.gradientEnabled = false;
  content.geometry.fillRule = (paint.fillRule == 1)
      ? ArtifactCore::PathFillRule::EvenOdd
      : ArtifactCore::PathFillRule::Winding;
  // Resolve userSpace gradients against pre-normalization coordinates,
  // then translate vertices so bounds start at the origin.
  const QRectF preBounds = svgContentBounds(content);
  normalizeSvgContent(&content);
  if (paint.fillUrl && !paint.fillUrlId.isEmpty()) {
    resolveSvgGradientUrl(gradients, paint.fillUrlId, preBounds, &content.fill);
  }
  if (paint.strokeUrl && !paint.strokeUrlId.isEmpty()) {
    const auto it = gradients.find(paint.strokeUrlId);
    if (it != gradients.end() && !it->second.stops.empty()) {
      const auto& firstStop = it->second.stops.front();
      const auto& lastStop = it->second.stops.back();
      content.stroke.gradientEnabled = true;
      content.stroke.gradientStart = firstStop.color;
      content.stroke.gradientEnd = lastStop.color;
      content.stroke.color = mixColor(firstStop.color, lastStop.color, 0.5f);
    }
  }
  return normalizedShapeContent(content);
}

static std::vector<Artifact::ShapeContent> parseSvgDocument(const QString& svgText) {
  std::vector<Artifact::ShapeContent> contents;
  std::map<QString, Gradient> gradients;
  std::vector<Pending> pending;
  struct Frame {
    QTransform transform;
    Paint paint;
    bool skip = false;
  };
  std::vector<Frame> stack;
  Frame root;
  root.paint.fillNone = false;
  root.paint.fillColor = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
  stack.push_back(root);
  QXmlStreamReader xml(svgText);
  int shapeIndex = 0;
  int guard = 0;
  auto isHidden = [](const QXmlStreamAttributes& attrs) {
    const QString display = attrs.value(QStringLiteral("display")).toString().trimmed().toLower();
    const QString visibility = attrs.value(QStringLiteral("visibility")).toString().trimmed().toLower();
    return display == QStringLiteral("none") || visibility == QStringLiteral("hidden") ||
           visibility == QStringLiteral("collapse");
  };
  auto transformVertices = [](const QTransform& xf, std::vector<Artifact::CustomPathVertex> verts) {
    for (auto& v : verts) {
      v = svgTransformedVertex(xf, v);
    }
    return verts;
  };
  auto transformPoints = [](const QTransform& xf, std::vector<QPointF> points) {
    for (auto& p : points) {
      p = xf.map(p);
    }
    return points;
  };
  // Paint and gradient urls resolve after the walk so forward-referenced
  // defs work; coordinates normalize per content at finish time.
  auto pushContent = [&](Artifact::ShapeContent content, const Paint& paint, const QString& name) {
    if (content.geometry.pathVertices.size() + content.geometry.polygonPoints.size() < 2) {
      return;
    }
    if (pending.size() >= 256) {
      return;
    }
    Pending item;
    item.content = std::move(content);
    item.paint = paint;
    item.name = name;
    pending.push_back(std::move(item));
  };
  while (!xml.atEnd() && guard++ < 200000) {
    const auto token = xml.readNext();
    if (token == QXmlStreamReader::StartElement) {
      const QString tag = xml.name().toString().toLower();
      const auto attrs = xml.attributes();
      if (tag == QStringLiteral("lineargradient")) {
        const QString id = attrs.value(QStringLiteral("id")).toString();
        Gradient grad = parseSvgGradientElement(&xml, false);
        if (!id.isEmpty()) {
          gradients[id] = std::move(grad);
        }
        continue;
      }
      if (tag == QStringLiteral("radialgradient")) {
        const QString id = attrs.value(QStringLiteral("id")).toString();
        Gradient grad = parseSvgGradientElement(&xml, true);
        if (!id.isEmpty()) {
          gradients[id] = std::move(grad);
        }
        continue;
      }
      Frame frame = stack.back();
      if (frame.skip || isHidden(attrs)) {
        Frame skipped;
        skipped.skip = true;
        stack.push_back(skipped);
        continue;
      }
      if (attrs.hasAttribute(QStringLiteral("transform"))) {
        frame.transform = frame.transform *
            parseSvgTransform(attrs.value(QStringLiteral("transform")).toString());
      }
      applySvgAttributes(&frame.paint, attrs);
      const bool isContainer = (tag == QStringLiteral("g") || tag == QStringLiteral("svg") ||
                                tag == QStringLiteral("defs") || tag == QStringLiteral("symbol"));
      if (isContainer) {
        stack.push_back(frame);
        continue;
      }
      const QString idAttr = attrs.value(QStringLiteral("id")).toString();
      const QString shapeName = idAttr.isEmpty()
          ? QStringLiteral("SVG %1").arg(++shapeIndex)
          : idAttr;
      const QTransform& xf = frame.transform;
      if (tag == QStringLiteral("path")) {
        ArtifactCore::ShapePath parsed = parseSvgPathData(attrs.value(QStringLiteral("d")).toString());
        if (parsed.isEmpty()) {
          continue;
        }
        // Rebuild vertices from commands to preserve curves exactly.
        // Note: multiple subpaths merge into one vertex run per element;
        // stroking such elements draws connectors between subpaths.
        Artifact::ShapeContent content;
        std::vector<Artifact::CustomPathVertex> verts;
        QPointF current;
        bool hasCurrent = false;
        bool elementClosed = false;
        for (const auto& command : parsed.commands()) {
          switch (command.type) {
            case ArtifactCore::PathCommandType::MoveTo: {
              Artifact::CustomPathVertex v = svgLineVertex(command.points[0]);
              verts.push_back(v);
              current = command.points[0];
              hasCurrent = true;
              break;
            }
            case ArtifactCore::PathCommandType::LineTo: {
              if (!hasCurrent) {
                verts.push_back(svgLineVertex(command.points[0]));
                current = command.points[0];
                hasCurrent = true;
                break;
              }
              verts.push_back(svgLineVertex(command.points[0]));
              current = command.points[0];
              break;
            }
            case ArtifactCore::PathCommandType::CubicTo: {
              if (!hasCurrent || verts.empty()) {
                verts.push_back(svgLineVertex(command.points[2]));
                current = command.points[2];
                hasCurrent = true;
                break;
              }
              verts.back().outTangent = command.points[0] - verts.back().pos;
              verts.back().smooth = true;
              Artifact::CustomPathVertex v = svgCurveVertex(command.points[2]);
              v.inTangent = command.points[1] - command.points[2];
              verts.push_back(v);
              current = command.points[2];
              break;
            }
            case ArtifactCore::PathCommandType::QuadTo: {
              if (!hasCurrent || verts.empty()) {
                verts.push_back(svgLineVertex(command.points[1]));
                current = command.points[1];
                hasCurrent = true;
                break;
              }
              // Degree-elevate quadratic to cubic control points.
              const QPointF p0 = current;
              const QPointF cp = command.points[0];
              const QPointF p1 = command.points[1];
              const QPointF c1 = p0 + (cp - p0) * (2.0 / 3.0);
              const QPointF c2 = p1 + (cp - p1) * (2.0 / 3.0);
              verts.back().outTangent = c1 - verts.back().pos;
              verts.back().smooth = true;
              Artifact::CustomPathVertex v = svgCurveVertex(p1);
              v.inTangent = c2 - p1;
              verts.push_back(v);
              current = p1;
              break;
            }
            case ArtifactCore::PathCommandType::Close: {
              elementClosed = true;
              break;
            }
          }
        }
        if (verts.size() < 2) {
          continue;
        }
        content.geometry.pathVertices = transformVertices(xf, std::move(verts));
        content.geometry.pathClosed = elementClosed;
        pushContent(std::move(content), frame.paint, shapeName);
      } else if (tag == QStringLiteral("rect")) {
        const double x = svgAttrNumber(attrs.value(QStringLiteral("x")).toString(), 0.0);
        const double y = svgAttrNumber(attrs.value(QStringLiteral("y")).toString(), 0.0);
        const double w = svgAttrNumber(attrs.value(QStringLiteral("width")).toString(), 0.0);
        const double h = svgAttrNumber(attrs.value(QStringLiteral("height")).toString(), 0.0);
        if (w <= 0.0 || h <= 0.0) {
          continue;
        }
        double rx = svgAttrNumber(attrs.value(QStringLiteral("rx")).toString(), 0.0);
        double ry = svgAttrNumber(attrs.value(QStringLiteral("ry")).toString(), 0.0);
        if (attrs.hasAttribute(QStringLiteral("rx")) && !attrs.hasAttribute(QStringLiteral("ry"))) {
          ry = rx;
        }
        if (attrs.hasAttribute(QStringLiteral("ry")) && !attrs.hasAttribute(QStringLiteral("rx"))) {
          rx = ry;
        }
        rx = std::clamp(rx, 0.0, w * 0.5);
        ry = std::clamp(ry, 0.0, h * 0.5);
        // One vertex per joint; each span sets the previous vertex's
        // out-tangent and pushes its end vertex with the in-tangent.
        std::vector<Artifact::CustomPathVertex> verts;
        if (rx <= 0.0 || ry <= 0.0) {
          verts.push_back(svgLineVertex(QPointF(x, y)));
          verts.push_back(svgLineVertex(QPointF(x + w, y)));
          verts.push_back(svgLineVertex(QPointF(x + w, y + h)));
          verts.push_back(svgLineVertex(QPointF(x, y + h)));
        } else {
          verts.push_back(svgLineVertex(QPointF(x + rx, y)));
          auto spanTo = [&](const QPointF& end, const QPointF& c1, const QPointF& c2) {
            verts.back().outTangent = c1 - verts.back().pos;
            verts.back().smooth = true;
            Artifact::CustomPathVertex b = svgCurveVertex(end);
            b.inTangent = c2 - end;
            verts.push_back(b);
          };
          const double k = 0.5522847498;
          spanTo(QPointF(x + w - rx, y), QPointF(x + w - rx, y), QPointF(x + w - rx, y));
          spanTo(QPointF(x + w, y + ry),
                 QPointF(x + w - rx + rx * k, y), QPointF(x + w, y + ry - ry * k));
          spanTo(QPointF(x + w, y + h - ry),
                 QPointF(x + w, y + ry), QPointF(x + w, y + h - ry));
          spanTo(QPointF(x + w - rx, y + h),
                 QPointF(x + w, y + h - ry + ry * k), QPointF(x + w - rx + rx * k, y + h));
          spanTo(QPointF(x + rx, y + h),
                 QPointF(x + w - rx, y + h), QPointF(x + rx, y + h));
          spanTo(QPointF(x, y + h - ry),
                 QPointF(x + rx - rx * k, y + h), QPointF(x, y + h - ry + ry * k));
          spanTo(QPointF(x, y + ry),
                 QPointF(x, y + h - ry), QPointF(x, y + ry));
          spanTo(QPointF(x + rx, y),
                 QPointF(x, y + ry - ry * k), QPointF(x + rx - rx * k, y));
          // Last vertex duplicates the start; pathClosed seals the shape.
          verts.pop_back();
        }
        Artifact::ShapeContent content;
        content.geometry.pathVertices = transformVertices(xf, std::move(verts));
        content.geometry.pathClosed = true;
        pushContent(std::move(content), frame.paint, shapeName);
      } else if (tag == QStringLiteral("circle") || tag == QStringLiteral("ellipse")) {
        double cx = svgAttrNumber(attrs.value(QStringLiteral("cx")).toString(), 0.0);
        double cy = svgAttrNumber(attrs.value(QStringLiteral("cy")).toString(), 0.0);
        double rx = 0.0, ry = 0.0;
        if (tag == QStringLiteral("circle")) {
          rx = ry = svgAttrNumber(attrs.value(QStringLiteral("r")).toString(), 0.0);
        } else {
          rx = svgAttrNumber(attrs.value(QStringLiteral("rx")).toString(), 0.0);
          ry = svgAttrNumber(attrs.value(QStringLiteral("ry")).toString(), 0.0);
        }
        if (rx <= 0.0 || ry <= 0.0) {
          continue;
        }
        const double k = 0.5522847498;
        std::vector<Artifact::CustomPathVertex> verts;
        verts.reserve(4);
        auto ellipseVertex = [&](double px, double py, double ix, double iy,
                                 double ox, double oy) {
          Artifact::CustomPathVertex v = svgCurveVertex(QPointF(cx + px, cy + py));
          v.inTangent = QPointF(ix, iy);
          v.outTangent = QPointF(ox, oy);
          verts.push_back(v);
        };
        ellipseVertex(rx, 0, 0, ry * k, 0, -ry * k);
        ellipseVertex(0, -ry, -rx * k, 0, rx * k, 0);
        ellipseVertex(-rx, 0, 0, -ry * k, 0, ry * k);
        ellipseVertex(0, ry, rx * k, 0, -rx * k, 0);
        Artifact::ShapeContent content;
        content.geometry.pathVertices = transformVertices(xf, std::move(verts));
        content.geometry.pathClosed = true;
        pushContent(std::move(content), frame.paint, shapeName);
      } else if (tag == QStringLiteral("polygon") || tag == QStringLiteral("polyline") ||
                 tag == QStringLiteral("line")) {
        std::vector<QPointF> points;
        if (tag == QStringLiteral("line")) {
          const double x1 = svgAttrNumber(attrs.value(QStringLiteral("x1")).toString(), 0.0);
          const double y1 = svgAttrNumber(attrs.value(QStringLiteral("y1")).toString(), 0.0);
          const double x2 = svgAttrNumber(attrs.value(QStringLiteral("x2")).toString(), 0.0);
          const double y2 = svgAttrNumber(attrs.value(QStringLiteral("y2")).toString(), 0.0);
          points.push_back(QPointF(x1, y1));
          points.push_back(QPointF(x2, y2));
        } else {
          const auto nums = parseSvgNumbers(attrs.value(QStringLiteral("points")).toString());
          for (size_t i = 0; i + 1 < nums.size(); i += 2) {
            points.push_back(QPointF(nums[i], nums[i + 1]));
          }
        }
        if (points.size() < 2) {
          continue;
        }
        Artifact::ShapeContent content;
        content.geometry.polygonPoints = transformPoints(xf, std::move(points));
        content.geometry.polygonClosed = (tag != QStringLiteral("polyline")) &&
                                         (tag != QStringLiteral("line"));
        pushContent(std::move(content), frame.paint, shapeName);
      }
    } else if (token == QXmlStreamReader::EndElement) {
      if (stack.size() > 1) {
        stack.pop_back();
      }
    }
  }
  // Finish all shapes now that every gradient def is known.
  for (auto& item : pending) {
    contents.push_back(finishSvgContent(std::move(item.content), item.paint, item.name, gradients));
  }
  return contents;
}

} // namespace SvgImport

std::vector<Artifact::ShapeContent> ArtifactShapeLayer::parseShapeContentsFromSvg(
    const QString& svgText) {
  if (svgText.trimmed().isEmpty()) {
    return {};
  }
  return SvgImport::parseSvgDocument(svgText);
}

int ArtifactShapeLayer::addShapeContentsFromSvg(const QString& svgText) {
  if (!impl_) {
    return 0;
  }
  const auto parsed = parseShapeContentsFromSvg(svgText);
  int added = 0;
  for (const auto& content : parsed) {
    if (addShapeContent(content) >= 0) {
      ++added;
    }
  }
  return added;
}

int ArtifactShapeLayer::importSvgFileContents(const QString& filePath) {
  const QString trimmed = filePath.trimmed();
  if (trimmed.isEmpty()) {
    return -1;
  }
  QFile file(trimmed);
  if (!file.exists() || file.size() > 64 * 1024 * 1024) {
    return -1;
  }
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return -1;
  }
  const QString text = QString::fromUtf8(file.readAll());
  file.close();
  if (text.trimmed().isEmpty()) {
    return -1;
  }
  return addShapeContentsFromSvg(text);
}

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
   obj["dashOffset"] = static_cast<double>(impl_->dashOffset_);
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
   QJsonArray contents;
   for (const auto& content : impl_->shapeContents_) {
     contents.push_back(shapeContentToJson(content));
   }
obj["shapeContents"] = contents;
    obj["activeContentIndex"] = impl_->activeContentIndex_;
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
   layer->setDashOffset(static_cast<float>(obj["dashOffset"].toDouble(0.0)));
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
  const QJsonArray contentsArr = obj["shapeContents"].toArray();
  layer->impl_->shapeContents_.clear();
  const int contentCount = std::min(static_cast<int>(contentsArr.size()), 256);
  layer->impl_->shapeContents_.reserve(contentCount);
for (int contentIndex = 0; contentIndex < contentCount; ++contentIndex) {
    layer->impl_->shapeContents_.push_back(
        shapeContentFromJson(contentsArr.at(contentIndex).toObject()));
  }
  layer->impl_->activeContentIndex_ =
      obj.contains("activeContentIndex") ? obj["activeContentIndex"].toInt(-1)
                                         : -1;
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
