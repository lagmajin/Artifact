module;
#include <utility>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QMatrix4x4>
#include <QVector4D>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <QPen>

module Artifact.Layer.Shape;

import std;
import Artifact.Layer.CloneEffectSupport;
import Shape.Types;
import Shape.Path;

namespace {

QPointF mapPoint(const QMatrix4x4& transform, const QPointF& point) {
 QVector4D v = transform * QVector4D(static_cast<float>(point.x()),
                                     static_cast<float>(point.y()), 0.0f, 1.0f);
 if (std::abs(v.w()) > 1e-6f) {
  return QPointF(v.x() / v.w(), v.y() / v.w());
 }
 return QPointF(v.x(), v.y());
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

} // namespace

namespace Artifact
{

class ArtifactShapeLayer::Impl {
public:
 Artifact::ShapeType shapeType_ = Artifact::ShapeType::Rect;
 int width_ = 200;
 int height_ = 200;
 FloatColor fillColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
 FloatColor strokeColor_ = FloatColor(0.0f, 0.0f, 0.0f, 1.0f);
 float strokeWidth_ = 0.0f;
 bool fillEnabled_ = true;
 bool strokeEnabled_ = false;

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

 // Phase 5: Bezier path override
 std::vector<CustomPathVertex> customPathVertices_;
 bool customPathClosed_ = true;

 bool useCachePipeline() const {
  return customPathVertices_.size() >= 3 ||
         strokeCap_ != StrokeCap::Flat ||
         strokeJoin_ != StrokeJoin::Miter ||
         strokeAlign_ != StrokeAlign::Center ||
         !dashPattern_.empty();
 }

    QImage cachedImage_;
    bool cacheDirty_ = true;

    mutable QRectF cachedLocalBounds_;
    bool localBoundsCacheDirty_ = true;

    mutable std::vector<QPointF> cachedShapePoints_;
    bool shapeContentCacheDirty_ = true;

   Impl() = default;
 ~Impl() = default;
   void addShape() {}
   void markDirty() { cacheDirty_ = true; shapeContentCacheDirty_ = true; }
   void rebuildCache() {
    if (!cacheDirty_) return;
    QImage img(width_, height_, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    if (customPathVertices_.size() >= 3) {
     path.moveTo(customPathVertices_[0].pos);
     const size_t n = customPathVertices_.size();
     for (size_t i = 0; i < n; ++i) {
      const size_t next = (i + 1) % n;
      if (!customPathClosed_ && next == 0) break;
      const CustomPathVertex& v0 = customPathVertices_[i];
      const CustomPathVertex& v1 = customPathVertices_[next];
      path.cubicTo(v0.pos + v0.outTangent, v1.pos + v1.inTangent, v1.pos);
     }
     if (customPathClosed_) path.closeSubpath();
    } else if (customPolygonPoints_.size() >= 3) {
     ShapePath sp;
     sp.setPolygon(customPolygonPoints_, customPolygonClosed_);
     path = sp.toPainterPath();
    } else {
     path = buildShapePath(shapeType_, width_, height_, cornerRadius_,
                           starPoints_, starInnerRadius_, polygonSides_).toPainterPath();
    }

    if (fillEnabled_) {
     QColor fc(static_cast<int>(fillColor_.r() * 255),
               static_cast<int>(fillColor_.g() * 255),
               static_cast<int>(fillColor_.b() * 255),
               static_cast<int>(fillColor_.a() * 255));
     painter.fillPath(path, fc);
    }

    if (strokeEnabled_ && strokeWidth_ > 0) {
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
     outside.addRect(QRectF(-1, -1, width_ + 2, height_ + 2));
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
void ArtifactShapeLayer::addShape() { impl_->addShape(); }
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
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
Artifact::ShapeType ArtifactShapeLayer::shapeType() const { return impl_->shapeType_; }

// ============================================================
// Size
// ============================================================

void ArtifactShapeLayer::setSize(int w, int h) {
  impl_->width_ = w;
  impl_->height_ = h;
  setSourceSize(Size_2D(w, h));
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
int ArtifactShapeLayer::shapeWidth() const { return impl_->width_; }
int ArtifactShapeLayer::shapeHeight() const { return impl_->height_; }

// ============================================================
// Style
// ============================================================

void ArtifactShapeLayer::setFillColor(const FloatColor& c) { impl_->fillColor_ = c; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillColor() const { return impl_->fillColor_; }
void ArtifactShapeLayer::setStrokeColor(const FloatColor& c) { impl_->strokeColor_ = c; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::strokeColor() const { return impl_->strokeColor_; }
void ArtifactShapeLayer::setStrokeWidth(float w) { impl_->strokeWidth_ = w; impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::strokeWidth() const { return impl_->strokeWidth_; }
void ArtifactShapeLayer::setFillEnabled(bool e) { impl_->fillEnabled_ = e; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
bool ArtifactShapeLayer::fillEnabled() const { return impl_->fillEnabled_; }
void ArtifactShapeLayer::setStrokeEnabled(bool e) { impl_->strokeEnabled_ = e; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
bool ArtifactShapeLayer::strokeEnabled() const { return impl_->strokeEnabled_; }

// ============================================================
// Shape Params
// ============================================================

void ArtifactShapeLayer::setCornerRadius(float r) { impl_->cornerRadius_ = r; impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::cornerRadius() const { return impl_->cornerRadius_; }
void ArtifactShapeLayer::setStarPoints(int p) { impl_->starPoints_ = std::max(3, p); impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
int ArtifactShapeLayer::starPoints() const { return impl_->starPoints_; }
void ArtifactShapeLayer::setStarInnerRadius(float r) { impl_->starInnerRadius_ = std::clamp(r, 0.0f, 1.0f); impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
float ArtifactShapeLayer::starInnerRadius() const { return impl_->starInnerRadius_; }
void ArtifactShapeLayer::setPolygonSides(int s) { impl_->polygonSides_ = std::max(3, s); impl_->markDirty(); impl_->localBoundsCacheDirty_ = true; impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
int ArtifactShapeLayer::polygonSides() const { return impl_->polygonSides_; }
bool ArtifactShapeLayer::hasCustomPolygon() const { return impl_->customPolygonPoints_.size() >= 3; }
void ArtifactShapeLayer::setCustomPolygonPoints(const std::vector<QPointF>& points, bool closed) {
  impl_->customPolygonPoints_ = points;
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
void ArtifactShapeLayer::setStrokeCap(StrokeCap cap) { impl_->strokeCap_ = cap; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
StrokeCap ArtifactShapeLayer::strokeCap() const { return impl_->strokeCap_; }
void ArtifactShapeLayer::setStrokeJoin(StrokeJoin join) { impl_->strokeJoin_ = join; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
StrokeJoin ArtifactShapeLayer::strokeJoin() const { return impl_->strokeJoin_; }
void ArtifactShapeLayer::setStrokeAlign(StrokeAlign align) { impl_->strokeAlign_ = align; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
StrokeAlign ArtifactShapeLayer::strokeAlign() const { return impl_->strokeAlign_; }
void ArtifactShapeLayer::setDashPattern(const std::vector<float>& pattern) { impl_->dashPattern_ = pattern; impl_->markDirty(); impl_->shapeContentCacheDirty_ = true; Q_EMIT changed(); }
std::vector<float> ArtifactShapeLayer::dashPattern() const { return impl_->dashPattern_; }

// Phase 5: Bezier path
bool ArtifactShapeLayer::hasCustomPath() const { return impl_->customPathVertices_.size() >= 3; }
void ArtifactShapeLayer::setCustomPathVertices(const std::vector<CustomPathVertex>& vertices, bool closed) {
  impl_->customPathVertices_ = vertices;
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
  impl_->markDirty();
  impl_->localBoundsCacheDirty_ = true;
  impl_->shapeContentCacheDirty_ = true;
  Q_EMIT changed();
}
std::vector<CustomPathVertex> ArtifactShapeLayer::customPathVertices() const { return impl_->customPathVertices_; }
bool ArtifactShapeLayer::customPathClosed() const { return impl_->customPathClosed_; }

// ============================================================
// toQImage (Software rendering)
// ============================================================

QImage ArtifactShapeLayer::toQImage() const {
 impl_->rebuildCache();
 return impl_->cachedImage_;
}

QRectF ArtifactShapeLayer::localBounds() const
{
  if (!impl_->localBoundsCacheDirty_) {
    return impl_->cachedLocalBounds_;
  }

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
  if (impl_->customPathVertices_.size() >= 2) {
   std::vector<QPointF> pts;
   pts.reserve(impl_->customPathVertices_.size());
   for (const auto& v : impl_->customPathVertices_) pts.push_back(v.pos);
   bounds = boundsOfPoints(pts);
  } else if (impl_->customPolygonPoints_.size() >= 2) {
   bounds = boundsOfPoints(impl_->customPolygonPoints_);
  } else {
   const QPainterPath path = buildShapePath(impl_->shapeType_, impl_->width_, impl_->height_,
                                           impl_->cornerRadius_, impl_->starPoints_,
                                           impl_->starInnerRadius_, impl_->polygonSides_)
                                .toPainterPath();
   bounds = path.boundingRect();
  }

  if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
   const auto size = sourceSize();
   if (size.width <= 0 || size.height <= 0) {
    return QRectF();
   }
   bounds = QRectF(0.0, 0.0, static_cast<qreal>(size.width), static_cast<qreal>(size.height));
  }

  const qreal pad = std::max<qreal>(0.5, static_cast<qreal>(impl_->strokeWidth_) * 0.5);
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
 auto* impl = impl_;
 // When bezier path or non-default stroke styles are active, render via QImage cache
 if (impl->useCachePipeline()) {
  impl->rebuildCache();
  const float layerOpacity = opacity();
  drawWithClonerEffect(this, baseTransform,
                       [renderer, impl, layerOpacity](const QMatrix4x4& transform, float weight) {
   renderer->drawSpriteTransformed(
       0.0f, 0.0f,
       static_cast<float>(impl->width_),
       static_cast<float>(impl->height_),
       transform, impl->cachedImage_,
       layerOpacity * weight);
  });
  return;
 }
  drawWithClonerEffect(this, baseTransform,
                       [renderer, impl, this](const QMatrix4x4& transform, float weight) {
    const auto fill = FloatColor(
        impl->fillColor_.r(), impl->fillColor_.g(), impl->fillColor_.b(),
        impl->fillColor_.a() * this->opacity() * weight);
    const auto stroke = FloatColor(
        impl->strokeColor_.r(), impl->strokeColor_.g(), impl->strokeColor_.b(),
        impl->strokeColor_.a() * this->opacity() * weight);

    if (impl->shapeContentCacheDirty_) {
     impl->cachedShapePoints_ = buildRenderablePoints(
         impl->shapeType_, impl->width_, impl->height_, impl->cornerRadius_,
         impl->starPoints_, impl->starInnerRadius_, impl->polygonSides_,
         impl->customPolygonPoints_, impl->customPolygonClosed_);
     impl->shapeContentCacheDirty_ = false;
    }
    const std::vector<QPointF>& points = impl->cachedShapePoints_;
    if (points.empty()) {
     return;
    }

    const bool closed =
        impl->shapeType_ != Artifact::ShapeType::Line &&
        !(impl->shapeType_ == Artifact::ShapeType::Polygon &&
          impl->customPolygonPoints_.size() >= 3 && !impl->customPolygonClosed_);

    std::vector<QPointF> mapped;
    mapped.reserve(points.size());
    for (const auto& p : points) {
     mapped.push_back(mapPoint(transform, p));
    }

    if (impl->fillEnabled_ && closed &&
        mapped.size() >= 3) {
     std::vector<Detail::float2> polygon;
     polygon.reserve(mapped.size());
     for (const auto& point : mapped) {
      polygon.push_back({static_cast<float>(point.x()),
                         static_cast<float>(point.y())});
     }
     renderer->drawSolidPolygonLocal(polygon, fill);
    }

    if (impl->strokeEnabled_ && impl->strokeWidth_ > 0.0f && mapped.size() >= 2) {
     const int edgeCount = closed ? static_cast<int>(mapped.size())
                                  : static_cast<int>(mapped.size()) - 1;
     for (int i = 0; i < edgeCount; ++i) {
      const int next = (i + 1) % static_cast<int>(mapped.size());
      renderer->drawThickLineLocal(
          {static_cast<float>(mapped[static_cast<size_t>(i)].x()),
           static_cast<float>(mapped[static_cast<size_t>(i)].y())},
          {static_cast<float>(mapped[static_cast<size_t>(next)].x()),
           static_cast<float>(mapped[static_cast<size_t>(next)].y())},
          std::max(1.0f, impl->strokeWidth_), stroke);
     }
    }
  });
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
                               -220);
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
 shapeGroup.addProperty(widthProp);

 auto heightProp = makeProp(QStringLiteral("shape.height"),
                            ArtifactCore::PropertyType::Integer,
                            impl_->height_, -218);
 heightProp->setDisplayLabel(QStringLiteral("Height"));
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
 strokeWidthProp->setDisplayLabel(QStringLiteral("Stroke Width"));
  appearanceGroup.addProperty(strokeWidthProp);

 auto strokeEnabledProp = makeProp(QStringLiteral("shape.strokeEnabled"),
                                   ArtifactCore::PropertyType::Boolean,
                                   impl_->strokeEnabled_, -206);
 strokeEnabledProp->setDisplayLabel(QStringLiteral("Stroke Enabled"));
  appearanceGroup.addProperty(strokeEnabledProp);

 auto strokeCapProp = makeProp(QStringLiteral("shape.strokeCap"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(impl_->strokeCap_), -205, false);
 strokeCapProp->setDisplayLabel(QStringLiteral("Stroke Cap"));
 strokeCapProp->setTooltip(QStringLiteral("0=Flat, 1=Round, 2=Square"));
  appearanceGroup.addProperty(strokeCapProp);

 auto strokeJoinProp = makeProp(QStringLiteral("shape.strokeJoin"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<int>(impl_->strokeJoin_), -204, false);
 strokeJoinProp->setDisplayLabel(QStringLiteral("Stroke Join"));
 strokeJoinProp->setTooltip(QStringLiteral("0=Miter, 1=Round, 2=Bevel"));
  appearanceGroup.addProperty(strokeJoinProp);

 auto strokeAlignProp = makeProp(QStringLiteral("shape.strokeAlign"),
                                 ArtifactCore::PropertyType::Integer,
                                 static_cast<int>(impl_->strokeAlign_), -203, false);
 strokeAlignProp->setDisplayLabel(QStringLiteral("Stroke Align"));
 strokeAlignProp->setTooltip(QStringLiteral("0=Center, 1=Inside, 2=Outside"));
  appearanceGroup.addProperty(strokeAlignProp);

 groups.push_back(appearanceGroup);

 // Shape-specific params
 ArtifactCore::PropertyGroup paramsGroup;
 paramsGroup.setName("Shape Parameters");

 auto cornerProp = makeProp(QStringLiteral("shape.cornerRadius"),
                            ArtifactCore::PropertyType::Float,
                            impl_->cornerRadius_, -200);
 cornerProp->setDisplayLabel(QStringLiteral("Corner Radius"));
  paramsGroup.addProperty(cornerProp);
 auto pointsProp = makeProp(QStringLiteral("shape.starPoints"),
                            ArtifactCore::PropertyType::Integer,
                            impl_->starPoints_, -199);
 pointsProp->setDisplayLabel(QStringLiteral("Points"));
  paramsGroup.addProperty(pointsProp);

 auto innerProp = makeProp(QStringLiteral("shape.starInnerRadius"),
                           ArtifactCore::PropertyType::Float,
                           impl_->starInnerRadius_, -198);
 innerProp->setDisplayLabel(QStringLiteral("Inner Radius"));
  paramsGroup.addProperty(innerProp);
 auto sidesProp = makeProp(QStringLiteral("shape.polygonSides"),
                           ArtifactCore::PropertyType::Integer,
                           impl_->polygonSides_, -197);
 sidesProp->setDisplayLabel(QStringLiteral("Sides"));
  paramsGroup.addProperty(sidesProp);

 groups.push_back(paramsGroup);

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
 return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

// ============================================================
// Serialization
// ============================================================

QJsonObject ArtifactShapeLayer::toJson() const {
 QJsonObject obj = ArtifactAbstractLayer::toJson();
 obj["layerType"] = QStringLiteral("Shape");
  obj["shapeType"] = static_cast<int>(impl_->shapeType_);
  obj["shapeWidth"] = impl_->width_;
  obj["shapeHeight"] = impl_->height_;
  obj["fillR"] = static_cast<double>(impl_->fillColor_.r());
  obj["fillG"] = static_cast<double>(impl_->fillColor_.g());
  obj["fillB"] = static_cast<double>(impl_->fillColor_.b());
  obj["fillA"] = static_cast<double>(impl_->fillColor_.a());
  obj["fillEnabled"] = impl_->fillEnabled_;
  obj["strokeR"] = static_cast<double>(impl_->strokeColor_.r());
  obj["strokeG"] = static_cast<double>(impl_->strokeColor_.g());
  obj["strokeB"] = static_cast<double>(impl_->strokeColor_.b());
  obj["strokeA"] = static_cast<double>(impl_->strokeColor_.a());
  obj["strokeWidth"] = static_cast<double>(impl_->strokeWidth_);
  obj["strokeEnabled"] = impl_->strokeEnabled_;
  obj["strokeCap"] = static_cast<int>(impl_->strokeCap_);
  obj["strokeJoin"] = static_cast<int>(impl_->strokeJoin_);
  obj["strokeAlign"] = static_cast<int>(impl_->strokeAlign_);
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
  return obj;
}

std::shared_ptr<ArtifactShapeLayer> ArtifactShapeLayer::fromJson(const QJsonObject& obj) {
 auto layer = std::make_shared<ArtifactShapeLayer>();
 layer->setShapeType(static_cast<Artifact::ShapeType>(obj["shapeType"].toInt()));
 layer->setSize(obj["shapeWidth"].toInt(200), obj["shapeHeight"].toInt(200));
 layer->setFillColor(FloatColor(
  static_cast<float>(obj["fillR"].toDouble(1.0)),
  static_cast<float>(obj["fillG"].toDouble(1.0)),
  static_cast<float>(obj["fillB"].toDouble(1.0)),
  static_cast<float>(obj["fillA"].toDouble(1.0))
 ));
 layer->setFillEnabled(obj["fillEnabled"].toBool(true));
 layer->setStrokeColor(FloatColor(
  static_cast<float>(obj["strokeR"].toDouble(0.0)),
  static_cast<float>(obj["strokeG"].toDouble(0.0)),
  static_cast<float>(obj["strokeB"].toDouble(0.0)),
  static_cast<float>(obj["strokeA"].toDouble(1.0))
 ));
 layer->setStrokeWidth(static_cast<float>(obj["strokeWidth"].toDouble(0.0)));
 layer->setStrokeEnabled(obj["strokeEnabled"].toBool(false));
 layer->setStrokeCap(static_cast<StrokeCap>(obj["strokeCap"].toInt(0)));
 layer->setStrokeJoin(static_cast<StrokeJoin>(obj["strokeJoin"].toInt(0)));
 layer->setStrokeAlign(static_cast<StrokeAlign>(obj["strokeAlign"].toInt(0)));
 layer->setCornerRadius(static_cast<float>(obj["cornerRadius"].toDouble(0.0)));
 layer->setStarPoints(obj["starPoints"].toInt(5));
 layer->setStarInnerRadius(static_cast<float>(obj["starInnerRadius"].toDouble(0.382)));
 layer->setPolygonSides(obj["polygonSides"].toInt(6));
 layer->impl_->customPolygonClosed_ = obj["customPolygonClosed"].toBool(true);
 layer->impl_->customPolygonPoints_.clear();
 const QJsonArray customPolygonPoints = obj["customPolygonPoints"].toArray();
 layer->impl_->customPolygonPoints_.reserve(customPolygonPoints.size());
 for (const auto& value : customPolygonPoints) {
  const QJsonObject p = value.toObject();
  layer->impl_->customPolygonPoints_.push_back(QPointF(p["x"].toDouble(), p["y"].toDouble()));
 }
 // Phase 5: bezier path (takes priority over customPolygon)
 const QJsonArray customPathArr = obj["customPath"].toArray();
 if (!customPathArr.isEmpty()) {
  layer->impl_->customPathClosed_ = obj["customPathClosed"].toBool(true);
  layer->impl_->customPathVertices_.clear();
  layer->impl_->customPathVertices_.reserve(customPathArr.size());
  for (const auto& val : customPathArr) {
   const QJsonObject vObj = val.toObject();
   CustomPathVertex v;
   v.pos = QPointF(vObj["px"].toDouble(), vObj["py"].toDouble());
   v.inTangent = QPointF(vObj["ix"].toDouble(), vObj["iy"].toDouble());
   v.outTangent = QPointF(vObj["ox"].toDouble(), vObj["oy"].toDouble());
   v.smooth = vObj["smooth"].toBool(false);
   layer->impl_->customPathVertices_.push_back(v);
  }
  layer->impl_->customPolygonPoints_.clear(); // mutual exclusion
 }
 layer->impl_->markDirty();
 return layer;
}

};
