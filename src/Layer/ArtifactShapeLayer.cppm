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

float polygonSignedArea(const std::vector<QPointF>& points) {
 if (points.size() < 3) {
  return 0.0f;
 }
 double area = 0.0;
 for (size_t i = 0; i < points.size(); ++i) {
  const QPointF& a = points[i];
  const QPointF& b = points[(i + 1) % points.size()];
  area += (a.x() * b.y()) - (b.x() * a.y());
 }
 return static_cast<float>(area * 0.5);
}

float cross2d(const QPointF& a, const QPointF& b, const QPointF& c) {
 return static_cast<float>((b.x() - a.x()) * (c.y() - a.y()) -
                           (b.y() - a.y()) * (c.x() - a.x()));
}

bool pointInTriangle(const QPointF& p, const QPointF& a, const QPointF& b,
                     const QPointF& c) {
 const float area1 = cross2d(p, a, b);
 const float area2 = cross2d(p, b, c);
 const float area3 = cross2d(p, c, a);
 const bool hasNeg = (area1 < 0.0f) || (area2 < 0.0f) || (area3 < 0.0f);
 const bool hasPos = (area1 > 0.0f) || (area2 > 0.0f) || (area3 > 0.0f);
 return !(hasNeg && hasPos);
}

std::vector<std::array<int, 3>> triangulatePolygon(const std::vector<QPointF>& points) {
 std::vector<std::array<int, 3>> triangles;
 if (points.size() < 3) {
  return triangles;
 }

 std::vector<int> indices(points.size());
 std::iota(indices.begin(), indices.end(), 0);
 const bool ccw = polygonSignedArea(points) >= 0.0f;
 int guard = 0;
 while (indices.size() > 2 && guard < 4096) {
  ++guard;
  bool earFound = false;
  const int m = static_cast<int>(indices.size());
  for (int i = 0; i < m; ++i) {
   const int i0 = indices[(i + m - 1) % m];
   const int i1 = indices[i];
   const int i2 = indices[(i + 1) % m];
   const QPointF& a = points[static_cast<size_t>(i0)];
   const QPointF& b = points[static_cast<size_t>(i1)];
   const QPointF& c = points[static_cast<size_t>(i2)];
   const float cross = cross2d(a, b, c);
   if (ccw ? (cross <= 1e-6f) : (cross >= -1e-6f)) {
    continue;
   }
   bool containsPoint = false;
   for (int j = 0; j < m; ++j) {
    const int idx = indices[j];
    if (idx == i0 || idx == i1 || idx == i2) {
     continue;
    }
    if (pointInTriangle(points[static_cast<size_t>(idx)], a, b, c)) {
     containsPoint = true;
     break;
    }
   }
   if (containsPoint) {
    continue;
   }
   triangles.push_back({i0, i1, i2});
   indices.erase(indices.begin() + i);
   earFound = true;
   break;
  }
  if (!earFound) {
   break;
  }
 }

 if (triangles.empty()) {
  for (size_t i = 1; i + 1 < points.size(); ++i) {
   triangles.push_back({0, static_cast<int>(i), static_cast<int>(i + 1)});
  }
 }
 return triangles;
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
  return {QPointF(0.0f, 0.0f), QPointF(w, 0.0f), QPointF(w, h), QPointF(0.0f, h)};
 case Artifact::ShapeType::Square: {
  const float side = std::min(w, h);
  const float left = (w - side) * 0.5f;
  const float top = (h - side) * 0.5f;
  return {QPointF(left, top), QPointF(left + side, top),
          QPointF(left + side, top + side), QPointF(left, top + side)};
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

  QImage cachedImage_;
  bool cacheDirty_ = true;

 Impl() = default;
 ~Impl() = default;
 void addShape() {}
  void markDirty() { cacheDirty_ = true; }
  void rebuildCache() {
   if (!cacheDirty_) {
    return;
   }
   QImage img(width_, height_, QImage::Format_ARGB32_Premultiplied);
   img.fill(Qt::transparent);

   QPainter painter(&img);
   painter.setRenderHint(QPainter::Antialiasing, true);

 QPainterPath path = buildShapePath(shapeType_, width_, height_, cornerRadius_,
                                      starPoints_, starInnerRadius_, polygonSides_).toPainterPath();
   if (shapeType_ == Artifact::ShapeType::Polygon && customPolygonPoints_.size() >= 3) {
    ShapePath customPath;
    customPath.setPolygon(customPolygonPoints_, customPolygonClosed_);
    path = customPath.toPainterPath();
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
    painter.setPen(pen);
    painter.drawPath(path);
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
 Q_EMIT changed();
}
int ArtifactShapeLayer::shapeWidth() const { return impl_->width_; }
int ArtifactShapeLayer::shapeHeight() const { return impl_->height_; }

// ============================================================
// Style
// ============================================================

void ArtifactShapeLayer::setFillColor(const FloatColor& c) { impl_->fillColor_ = c; impl_->markDirty(); Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillColor() const { return impl_->fillColor_; }
void ArtifactShapeLayer::setStrokeColor(const FloatColor& c) { impl_->strokeColor_ = c; impl_->markDirty(); Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::strokeColor() const { return impl_->strokeColor_; }
void ArtifactShapeLayer::setStrokeWidth(float w) { impl_->strokeWidth_ = w; impl_->markDirty(); Q_EMIT changed(); }
float ArtifactShapeLayer::strokeWidth() const { return impl_->strokeWidth_; }
void ArtifactShapeLayer::setFillEnabled(bool e) { impl_->fillEnabled_ = e; impl_->markDirty(); Q_EMIT changed(); }
bool ArtifactShapeLayer::fillEnabled() const { return impl_->fillEnabled_; }
void ArtifactShapeLayer::setStrokeEnabled(bool e) { impl_->strokeEnabled_ = e; impl_->markDirty(); Q_EMIT changed(); }
bool ArtifactShapeLayer::strokeEnabled() const { return impl_->strokeEnabled_; }

// ============================================================
// Shape Params
// ============================================================

void ArtifactShapeLayer::setCornerRadius(float r) { impl_->cornerRadius_ = r; impl_->markDirty(); Q_EMIT changed(); }
float ArtifactShapeLayer::cornerRadius() const { return impl_->cornerRadius_; }
void ArtifactShapeLayer::setStarPoints(int p) { impl_->starPoints_ = std::max(3, p); impl_->markDirty(); Q_EMIT changed(); }
int ArtifactShapeLayer::starPoints() const { return impl_->starPoints_; }
void ArtifactShapeLayer::setStarInnerRadius(float r) { impl_->starInnerRadius_ = std::clamp(r, 0.0f, 1.0f); impl_->markDirty(); Q_EMIT changed(); }
float ArtifactShapeLayer::starInnerRadius() const { return impl_->starInnerRadius_; }
void ArtifactShapeLayer::setPolygonSides(int s) { impl_->polygonSides_ = std::max(3, s); impl_->markDirty(); Q_EMIT changed(); }
int ArtifactShapeLayer::polygonSides() const { return impl_->polygonSides_; }
bool ArtifactShapeLayer::hasCustomPolygon() const { return impl_->shapeType_ == Artifact::ShapeType::Polygon && impl_->customPolygonPoints_.size() >= 3; }
void ArtifactShapeLayer::setCustomPolygonPoints(const std::vector<QPointF>& points, bool closed) { impl_->customPolygonPoints_ = points; impl_->customPolygonClosed_ = closed; impl_->markDirty(); Q_EMIT changed(); }
void ArtifactShapeLayer::clearCustomPolygonPoints() { if (impl_->customPolygonPoints_.empty()) return; impl_->customPolygonPoints_.clear(); impl_->customPolygonClosed_ = true; impl_->markDirty(); Q_EMIT changed(); }
std::vector<QPointF> ArtifactShapeLayer::customPolygonPoints() const { return impl_->customPolygonPoints_; }
bool ArtifactShapeLayer::customPolygonClosed() const { return impl_->customPolygonClosed_; }

// ============================================================
// toQImage (Software rendering)
// ============================================================

QImage ArtifactShapeLayer::toQImage() const {
 impl_->rebuildCache();
 return impl_->cachedImage_;
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
 drawWithClonerEffect(this, baseTransform,
                      [renderer, impl, this](const QMatrix4x4& transform, float weight) {
   const auto fill = FloatColor(
       impl->fillColor_.r(), impl->fillColor_.g(), impl->fillColor_.b(),
       impl->fillColor_.a() * this->opacity() * weight);
   const auto stroke = FloatColor(
       impl->strokeColor_.r(), impl->strokeColor_.g(), impl->strokeColor_.b(),
       impl->strokeColor_.a() * this->opacity() * weight);

   std::vector<QPointF> points = buildRenderablePoints(
       impl->shapeType_, impl->width_, impl->height_, impl->cornerRadius_,
       impl->starPoints_, impl->starInnerRadius_, impl->polygonSides_,
       impl->customPolygonPoints_, impl->customPolygonClosed_);
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
    const auto triangles = triangulatePolygon(mapped);
    if (!triangles.empty()) {
     for (const auto& tri : triangles) {
      renderer->drawSolidTriangleLocal(
          {static_cast<float>(mapped[static_cast<size_t>(tri[0])].x()),
           static_cast<float>(mapped[static_cast<size_t>(tri[0])].y())},
          {static_cast<float>(mapped[static_cast<size_t>(tri[1])].x()),
           static_cast<float>(mapped[static_cast<size_t>(tri[1])].y())},
          {static_cast<float>(mapped[static_cast<size_t>(tri[2])].x()),
           static_cast<float>(mapped[static_cast<size_t>(tri[2])].y())},
          fill);
     }
    }
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

 // Shape Type Group
 ArtifactCore::PropertyGroup shapeGroup;
 shapeGroup.setName("Shape");

 auto shapeTypeProp = std::make_shared<ArtifactCore::AbstractProperty>();
 shapeTypeProp->setName(QStringLiteral("shape.type"));
  shapeTypeProp->setType(ArtifactCore::PropertyType::Integer);
  shapeTypeProp->setValue(static_cast<int>(impl_->shapeType_));
 shapeTypeProp->setDisplayLabel(QStringLiteral("Type"));
  QString shapeTypeTooltip = QStringLiteral("0=Rect, 1=Ellipse, 2=Star, 3=Polygon, 4=Line, 5=Triangle, 6=Square");
  shapeTypeTooltip += QStringLiteral(" (current: ");
  shapeTypeTooltip += shapeTypeName(static_cast<int>(impl_->shapeType_));
  shapeTypeTooltip += QStringLiteral(")");
  shapeTypeProp->setTooltip(shapeTypeTooltip);
  shapeGroup.addProperty(shapeTypeProp);

 auto widthProp = std::make_shared<ArtifactCore::AbstractProperty>();
 widthProp->setName(QStringLiteral("shape.width"));
  widthProp->setType(ArtifactCore::PropertyType::Integer);
  widthProp->setValue(impl_->width_);
 widthProp->setDisplayLabel(QStringLiteral("Width"));
  shapeGroup.addProperty(widthProp);

 auto heightProp = std::make_shared<ArtifactCore::AbstractProperty>();
 heightProp->setName(QStringLiteral("shape.height"));
  heightProp->setType(ArtifactCore::PropertyType::Integer);
  heightProp->setValue(impl_->height_);
 heightProp->setDisplayLabel(QStringLiteral("Height"));
  shapeGroup.addProperty(heightProp);

 groups.push_back(shapeGroup);

 // Appearance Group
 ArtifactCore::PropertyGroup appearanceGroup;
 appearanceGroup.setName("Appearance");

 auto fillColorProp = std::make_shared<ArtifactCore::AbstractProperty>();
 fillColorProp->setName(QStringLiteral("shape.fillColor"));
  fillColorProp->setType(ArtifactCore::PropertyType::Color);
  fillColorProp->setValue(QColor(
  static_cast<int>(impl_->fillColor_.r() * 255),
  static_cast<int>(impl_->fillColor_.g() * 255),
  static_cast<int>(impl_->fillColor_.b() * 255),
  static_cast<int>(impl_->fillColor_.a() * 255)
  ));
 fillColorProp->setDisplayLabel(QStringLiteral("Fill Color"));
  appearanceGroup.addProperty(fillColorProp);

 auto fillEnabledProp = std::make_shared<ArtifactCore::AbstractProperty>();
 fillEnabledProp->setName(QStringLiteral("shape.fillEnabled"));
  fillEnabledProp->setType(ArtifactCore::PropertyType::Boolean);
  fillEnabledProp->setValue(impl_->fillEnabled_);
 fillEnabledProp->setDisplayLabel(QStringLiteral("Fill Enabled"));
  appearanceGroup.addProperty(fillEnabledProp);

 auto strokeColorProp = std::make_shared<ArtifactCore::AbstractProperty>();
 strokeColorProp->setName(QStringLiteral("shape.strokeColor"));
  strokeColorProp->setType(ArtifactCore::PropertyType::Color);
  strokeColorProp->setValue(QColor(
  static_cast<int>(impl_->strokeColor_.r() * 255),
  static_cast<int>(impl_->strokeColor_.g() * 255),
  static_cast<int>(impl_->strokeColor_.b() * 255),
  static_cast<int>(impl_->strokeColor_.a() * 255)
  ));
 strokeColorProp->setDisplayLabel(QStringLiteral("Stroke Color"));
  appearanceGroup.addProperty(strokeColorProp);

 auto strokeWidthProp = std::make_shared<ArtifactCore::AbstractProperty>();
 strokeWidthProp->setName(QStringLiteral("shape.strokeWidth"));
  strokeWidthProp->setType(ArtifactCore::PropertyType::Float);
  strokeWidthProp->setValue(impl_->strokeWidth_);
 strokeWidthProp->setDisplayLabel(QStringLiteral("Stroke Width"));
  appearanceGroup.addProperty(strokeWidthProp);

 auto strokeEnabledProp = std::make_shared<ArtifactCore::AbstractProperty>();
 strokeEnabledProp->setName(QStringLiteral("shape.strokeEnabled"));
  strokeEnabledProp->setType(ArtifactCore::PropertyType::Boolean);
  strokeEnabledProp->setValue(impl_->strokeEnabled_);
 strokeEnabledProp->setDisplayLabel(QStringLiteral("Stroke Enabled"));
  appearanceGroup.addProperty(strokeEnabledProp);

 groups.push_back(appearanceGroup);

 // Shape-specific params
 ArtifactCore::PropertyGroup paramsGroup;
 paramsGroup.setName("Shape Parameters");

 auto cornerProp = std::make_shared<ArtifactCore::AbstractProperty>();
 cornerProp->setName(QStringLiteral("shape.cornerRadius"));
  cornerProp->setType(ArtifactCore::PropertyType::Float);
  cornerProp->setValue(impl_->cornerRadius_);
 cornerProp->setDisplayLabel(QStringLiteral("Corner Radius"));
  paramsGroup.addProperty(cornerProp);
 auto pointsProp = std::make_shared<ArtifactCore::AbstractProperty>();
 pointsProp->setName(QStringLiteral("shape.starPoints"));
  pointsProp->setType(ArtifactCore::PropertyType::Integer);
  pointsProp->setValue(impl_->starPoints_);
 pointsProp->setDisplayLabel(QStringLiteral("Points"));
  paramsGroup.addProperty(pointsProp);

 auto innerProp = std::make_shared<ArtifactCore::AbstractProperty>();
 innerProp->setName(QStringLiteral("shape.starInnerRadius"));
  innerProp->setType(ArtifactCore::PropertyType::Float);
  innerProp->setValue(impl_->starInnerRadius_);
 innerProp->setDisplayLabel(QStringLiteral("Inner Radius"));
  paramsGroup.addProperty(innerProp);
 auto sidesProp = std::make_shared<ArtifactCore::AbstractProperty>();
 sidesProp->setName(QStringLiteral("shape.polygonSides"));
  sidesProp->setType(ArtifactCore::PropertyType::Integer);
  sidesProp->setValue(impl_->polygonSides_);
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
 if (layer->impl_->shapeType_ != Artifact::ShapeType::Polygon) {
  layer->impl_->customPolygonPoints_.clear();
  layer->impl_->customPolygonClosed_ = true;
 }
 layer->impl_->markDirty();
 return layer;
}

};
