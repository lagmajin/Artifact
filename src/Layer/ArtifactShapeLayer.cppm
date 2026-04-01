module;
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

module Artifact.Layer.Shape;

import std;

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

void ArtifactShapeLayer::setShapeType(ShapeType type) { impl_->shapeType_ = type; }
ShapeType ArtifactShapeLayer::shapeType() const { return impl_->shapeType_; }

// ============================================================
// Size
// ============================================================

void ArtifactShapeLayer::setSize(int w, int h) { impl_->width_ = w; impl_->height_ = h; setSourceSize(Size_2D(w, h)); }
int ArtifactShapeLayer::shapeWidth() const { return impl_->width_; }
int ArtifactShapeLayer::shapeHeight() const { return impl_->height_; }

// ============================================================
// Style
// ============================================================

void ArtifactShapeLayer::setFillColor(const FloatColor& c) { impl_->fillColor_ = c; }
FloatColor ArtifactShapeLayer::fillColor() const { return impl_->fillColor_; }
void ArtifactShapeLayer::setStrokeColor(const FloatColor& c) { impl_->strokeColor_ = c; }
FloatColor ArtifactShapeLayer::strokeColor() const { return impl_->strokeColor_; }
void ArtifactShapeLayer::setStrokeWidth(float w) { impl_->strokeWidth_ = w; }
float ArtifactShapeLayer::strokeWidth() const { return impl_->strokeWidth_; }
void ArtifactShapeLayer::setFillEnabled(bool e) { impl_->fillEnabled_ = e; }
bool ArtifactShapeLayer::fillEnabled() const { return impl_->fillEnabled_; }
void ArtifactShapeLayer::setStrokeEnabled(bool e) { impl_->strokeEnabled_ = e; }
bool ArtifactShapeLayer::strokeEnabled() const { return impl_->strokeEnabled_; }

// ============================================================
// Shape Params
// ============================================================

void ArtifactShapeLayer::setCornerRadius(float r) { impl_->cornerRadius_ = r; }
float ArtifactShapeLayer::cornerRadius() const { return impl_->cornerRadius_; }
void ArtifactShapeLayer::setStarPoints(int p) { impl_->starPoints_ = std::max(3, p); }
int ArtifactShapeLayer::starPoints() const { return impl_->starPoints_; }
void ArtifactShapeLayer::setStarInnerRadius(float r) { impl_->starInnerRadius_ = std::clamp(r, 0.0f, 1.0f); }
float ArtifactShapeLayer::starInnerRadius() const { return impl_->starInnerRadius_; }
void ArtifactShapeLayer::setPolygonSides(int s) { impl_->polygonSides_ = std::max(3, s); }
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
 if (!renderer) return;
 QImage img = toQImage();
 if (img.isNull()) return;
 renderer->drawSprite(0, 0, impl_->width_, impl_->height_, img);
}

// ============================================================
// Properties
// ============================================================

std::vector<ArtifactCore::PropertyGroup> ArtifactShapeLayer::getLayerPropertyGroups() const {
 std::vector<ArtifactCore::PropertyGroup> groups;

 // Shape Type Group
 ArtifactCore::PropertyGroup shapeGroup;
 shapeGroup.setName("Shape");
 shapeGroup.setCollapsed(false);

 ArtifactCore::Property shapeTypeProp;
 shapeTypeProp.setName("Type");
 shapeTypeProp.setType(ArtifactCore::PropertyType::Enum);
 shapeTypeProp.setValue(static_cast<int>(impl_->shapeType_));
 QStringList shapeTypes = {"Rect", "Ellipse", "Star", "Polygon", "Line"};
 shapeTypeProp.setEnumLabels(shapeTypes);
 shapeGroup.addProperty(shapeTypeProp);

 ArtifactCore::Property widthProp;
 widthProp.setName("Width");
 widthProp.setType(ArtifactCore::PropertyType::Integer);
 widthProp.setValue(impl_->width_);
 shapeGroup.addProperty(widthProp);

 ArtifactCore::Property heightProp;
 heightProp.setName("Height");
 heightProp.setType(ArtifactCore::PropertyType::Integer);
 heightProp.setValue(impl_->height_);
 shapeGroup.addProperty(heightProp);

 groups.push_back(shapeGroup);

 // Appearance Group
 ArtifactCore::PropertyGroup appearanceGroup;
 appearanceGroup.setName("Appearance");
 appearanceGroup.setCollapsed(false);

 ArtifactCore::Property fillColorProp;
 fillColorProp.setName("Fill Color");
 fillColorProp.setType(ArtifactCore::PropertyType::Color);
 fillColorProp.setValue(QColor(
  static_cast<int>(impl_->fillColor_.r() * 255),
  static_cast<int>(impl_->fillColor_.g() * 255),
  static_cast<int>(impl_->fillColor_.b() * 255),
  static_cast<int>(impl_->fillColor_.a() * 255)
 ));
 appearanceGroup.addProperty(fillColorProp);

 ArtifactCore::Property fillEnabledProp;
 fillEnabledProp.setName("Fill Enabled");
 fillEnabledProp.setType(ArtifactCore::PropertyType::Boolean);
 fillEnabledProp.setValue(impl_->fillEnabled_);
 appearanceGroup.addProperty(fillEnabledProp);

 ArtifactCore::Property strokeColorProp;
 strokeColorProp.setName("Stroke Color");
 strokeColorProp.setType(ArtifactCore::PropertyType::Color);
 strokeColorProp.setValue(QColor(
  static_cast<int>(impl_->strokeColor_.r() * 255),
  static_cast<int>(impl_->strokeColor_.g() * 255),
  static_cast<int>(impl_->strokeColor_.b() * 255),
  static_cast<int>(impl_->strokeColor_.a() * 255)
 ));
 appearanceGroup.addProperty(strokeColorProp);

 ArtifactCore::Property strokeWidthProp;
 strokeWidthProp.setName("Stroke Width");
 strokeWidthProp.setType(ArtifactCore::PropertyType::Float);
 strokeWidthProp.setValue(impl_->strokeWidth_);
 appearanceGroup.addProperty(strokeWidthProp);

 ArtifactCore::Property strokeEnabledProp;
 strokeEnabledProp.setName("Stroke Enabled");
 strokeEnabledProp.setType(ArtifactCore::PropertyType::Boolean);
 strokeEnabledProp.setValue(impl_->strokeEnabled_);
 appearanceGroup.addProperty(strokeEnabledProp);

 groups.push_back(appearanceGroup);

 // Shape-specific params
 ArtifactCore::PropertyGroup paramsGroup;
 paramsGroup.setName("Shape Parameters");
 paramsGroup.setCollapsed(false);

 if (impl_->shapeType_ == ShapeType::Rect) {
  ArtifactCore::Property cornerProp;
  cornerProp.setName("Corner Radius");
  cornerProp.setType(ArtifactCore::PropertyType::Float);
  cornerProp.setValue(impl_->cornerRadius_);
  paramsGroup.addProperty(cornerProp);
 } else if (impl_->shapeType_ == ShapeType::Star) {
  ArtifactCore::Property pointsProp;
  pointsProp.setName("Points");
  pointsProp.setType(ArtifactCore::PropertyType::Integer);
  pointsProp.setValue(impl_->starPoints_);
  paramsGroup.addProperty(pointsProp);

  ArtifactCore::Property innerProp;
  innerProp.setName("Inner Radius");
  innerProp.setType(ArtifactCore::PropertyType::Float);
  innerProp.setValue(impl_->starInnerRadius_);
  paramsGroup.addProperty(innerProp);
 } else if (impl_->shapeType_ == ShapeType::Polygon) {
  ArtifactCore::Property sidesProp;
  sidesProp.setName("Sides");
  sidesProp.setType(ArtifactCore::PropertyType::Integer);
  sidesProp.setValue(impl_->polygonSides_);
  paramsGroup.addProperty(sidesProp);
 }

 groups.push_back(paramsGroup);

 return groups;
}

bool ArtifactShapeLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
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
