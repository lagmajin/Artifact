module;
#include <utility>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

module Artifact.Layer.Shape;

import std;
import Artifact.Layer.CloneEffectSupport;

namespace Artifact
{

class ArtifactShapeLayer::Impl {
public:
 ShapeType shapeType_ = ShapeType::Rect;
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

 Impl() = default;
 ~Impl() = default;
 void addShape() {}
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

void ArtifactShapeLayer::setShapeType(ShapeType type) {
 impl_->shapeType_ = type;
 Q_EMIT changed();
}
ShapeType ArtifactShapeLayer::shapeType() const { return impl_->shapeType_; }

// ============================================================
// Size
// ============================================================

void ArtifactShapeLayer::setSize(int w, int h) {
 impl_->width_ = w;
 impl_->height_ = h;
 setSourceSize(Size_2D(w, h));
 Q_EMIT changed();
}
int ArtifactShapeLayer::shapeWidth() const { return impl_->width_; }
int ArtifactShapeLayer::shapeHeight() const { return impl_->height_; }

// ============================================================
// Style
// ============================================================

void ArtifactShapeLayer::setFillColor(const FloatColor& c) { impl_->fillColor_ = c; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::fillColor() const { return impl_->fillColor_; }
void ArtifactShapeLayer::setStrokeColor(const FloatColor& c) { impl_->strokeColor_ = c; Q_EMIT changed(); }
FloatColor ArtifactShapeLayer::strokeColor() const { return impl_->strokeColor_; }
void ArtifactShapeLayer::setStrokeWidth(float w) { impl_->strokeWidth_ = w; Q_EMIT changed(); }
float ArtifactShapeLayer::strokeWidth() const { return impl_->strokeWidth_; }
void ArtifactShapeLayer::setFillEnabled(bool e) { impl_->fillEnabled_ = e; Q_EMIT changed(); }
bool ArtifactShapeLayer::fillEnabled() const { return impl_->fillEnabled_; }
void ArtifactShapeLayer::setStrokeEnabled(bool e) { impl_->strokeEnabled_ = e; Q_EMIT changed(); }
bool ArtifactShapeLayer::strokeEnabled() const { return impl_->strokeEnabled_; }

// ============================================================
// Shape Params
// ============================================================

void ArtifactShapeLayer::setCornerRadius(float r) { impl_->cornerRadius_ = r; Q_EMIT changed(); }
float ArtifactShapeLayer::cornerRadius() const { return impl_->cornerRadius_; }
void ArtifactShapeLayer::setStarPoints(int p) { impl_->starPoints_ = std::max(3, p); Q_EMIT changed(); }
int ArtifactShapeLayer::starPoints() const { return impl_->starPoints_; }
void ArtifactShapeLayer::setStarInnerRadius(float r) { impl_->starInnerRadius_ = std::clamp(r, 0.0f, 1.0f); Q_EMIT changed(); }
float ArtifactShapeLayer::starInnerRadius() const { return impl_->starInnerRadius_; }
void ArtifactShapeLayer::setPolygonSides(int s) { impl_->polygonSides_ = std::max(3, s); Q_EMIT changed(); }
int ArtifactShapeLayer::polygonSides() const { return impl_->polygonSides_; }

// ============================================================
// QPainter Path Generation
// ============================================================

static QPainterPath buildShapePath(ShapeType shapeType,
                                   int width,
                                   int height,
                                   float cornerRadius,
                                   int starPoints,
                                   float starInnerRadius,
                                   int polygonSides) {
 QPainterPath path;
 const float w = static_cast<float>(width);
 const float h = static_cast<float>(height);
 const float cx = w * 0.5f;
 const float cy = h * 0.5f;

 switch (shapeType) {
   case ShapeType::Rect:
    if (cornerRadius > 0) {
     path.addRoundedRect(0, 0, w, h, cornerRadius, cornerRadius);
    } else {
     path.addRect(0, 0, w, h);
    }
   break;

  case ShapeType::Ellipse:
   path.addEllipse(0, 0, w, h);
   break;

  case ShapeType::Star: {
    const int pts = starPoints;
    const float outerR = std::min(cx, cy);
    const float innerR = outerR * starInnerRadius;
   for (int i = 0; i < pts * 2; ++i) {
    float angle = static_cast<float>(i) * M_PI / pts - M_PI * 0.5f;
    float r = (i % 2 == 0) ? outerR : innerR;
    float x = cx + r * std::cos(angle);
    float y = cy + r * std::sin(angle);
    if (i == 0) path.moveTo(x, y);
    else path.lineTo(x, y);
   }
   path.closeSubpath();
   break;
  }

  case ShapeType::Polygon: {
    const int sides = polygonSides;
   const float r = std::min(cx, cy);
   for (int i = 0; i < sides; ++i) {
    float angle = static_cast<float>(i) * 2.0f * M_PI / sides - M_PI * 0.5f;
    float x = cx + r * std::cos(angle);
    float y = cy + r * std::sin(angle);
    if (i == 0) path.moveTo(x, y);
    else path.lineTo(x, y);
   }
   path.closeSubpath();
   break;
  }

  case ShapeType::Line:
   path.moveTo(0, cy);
   path.lineTo(w, cy);
   break;
 }

 return path;
}

// ============================================================
// toQImage (Software rendering)
// ============================================================

QImage ArtifactShapeLayer::toQImage() const {
 QImage img(impl_->width_, impl_->height_, QImage::Format_ARGB32_Premultiplied);
 img.fill(Qt::transparent);

 QPainter painter(&img);
 painter.setRenderHint(QPainter::Antialiasing, true);

 QPainterPath path = buildShapePath(impl_->shapeType_, impl_->width_, impl_->height_, impl_->cornerRadius_,
                                    impl_->starPoints_, impl_->starInnerRadius_, impl_->polygonSides_);

 if (impl_->fillEnabled_) {
   QColor fc(static_cast<int>(impl_->fillColor_.r() * 255),
             static_cast<int>(impl_->fillColor_.g() * 255),
             static_cast<int>(impl_->fillColor_.b() * 255),
             static_cast<int>(impl_->fillColor_.a() * 255));
  painter.fillPath(path, fc);
 }

 if (impl_->strokeEnabled_ && impl_->strokeWidth_ > 0) {
   QColor sc(static_cast<int>(impl_->strokeColor_.r() * 255),
             static_cast<int>(impl_->strokeColor_.g() * 255),
             static_cast<int>(impl_->strokeColor_.b() * 255),
             static_cast<int>(impl_->strokeColor_.a() * 255));
  QPen pen(sc, impl_->strokeWidth_);
  painter.setPen(pen);
  painter.drawPath(path);
 }

 painter.end();
 return img;
}

// ============================================================
// draw (GPU rendering — delegates to toQImage for now)
// ============================================================

void ArtifactShapeLayer::draw(ArtifactIRenderer* renderer) {
 if (!renderer) {
  return;
 }
 QImage img = toQImage();
 if (img.isNull()) {
  return;
 }
 const QMatrix4x4 baseTransform = getGlobalTransform4x4();
 drawWithClonerEffect(this, baseTransform, [renderer, img, this](const QMatrix4x4& transform, float weight) {
  renderer->drawSpriteTransformed(0.0f,
                                  0.0f,
                                  static_cast<float>(impl_->width_),
                                  static_cast<float>(impl_->height_),
                                  transform,
                                  img,
                                  this->opacity() * weight);
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
 shapeTypeProp->setTooltip(QStringLiteral("0=Rect,1=Ellipse,2=Star,3=Polygon,4=Line"));
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
  setShapeType(static_cast<ShapeType>(value.toInt()));
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
  return obj;
}

std::shared_ptr<ArtifactShapeLayer> ArtifactShapeLayer::fromJson(const QJsonObject& obj) {
 auto layer = std::make_shared<ArtifactShapeLayer>();
 layer->setShapeType(static_cast<ShapeType>(obj["shapeType"].toInt()));
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
 return layer;
}

};
