module;
#include <utility>
#include <QList>
#include <QColor>
#include <QConicalGradient>
#include <QImage>
#include <QJsonObject>
#include <QLinearGradient>
#include <QMatrix4x4>
#include <QPainter>
#include <QRadialGradient>
#include <QVariant>
#include <Layer/ArtifactCloneEffectSupport.hpp>
#include <Layer/ArtifactSolidGradientUtil.hpp>
#include <algorithm>
#include <cmath>

module Artifact.Layer.Solid2D;

import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;

namespace Artifact
{
 
  class ArtifactSolid2DLayer::Impl
 {
 private:
  FloatColor color_;
  ArtifactSolidFillType fillType_ = ArtifactSolidFillType::Solid;
  FloatColor gradientStartColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  FloatColor gradientEndColor_ = FloatColor(0.2f, 0.2f, 0.2f, 1.0f);
  float gradientAngleDegrees_ = 90.0f;
  bool gradientReverse_ = false;
  float gradientCenterX_ = 0.5f;
  float gradientCenterY_ = 0.5f;
  float gradientScale_ = 1.0f;
  float gradientOffset_ = 0.0f;

 public:
  Impl();
  ~Impl();
  FloatColor color() const { return color_; }
  void setColor(const FloatColor& c) { color_ = c; }
  ArtifactSolidFillType fillType() const { return fillType_; }
  void setFillType(const ArtifactSolidFillType v) { fillType_ = v; }
  FloatColor gradientStartColor() const { return gradientStartColor_; }
  void setGradientStartColor(const FloatColor& c) { gradientStartColor_ = c; }
  FloatColor gradientEndColor() const { return gradientEndColor_; }
  void setGradientEndColor(const FloatColor& c) { gradientEndColor_ = c; }
  float gradientAngleDegrees() const { return gradientAngleDegrees_; }
  void setGradientAngleDegrees(const float v) { gradientAngleDegrees_ = v; }
  bool gradientReverse() const { return gradientReverse_; }
  void setGradientReverse(const bool v) { gradientReverse_ = v; }
  float gradientCenterX() const { return gradientCenterX_; }
  void setGradientCenterX(const float v) { gradientCenterX_ = v; }
  float gradientCenterY() const { return gradientCenterY_; }
  void setGradientCenterY(const float v) { gradientCenterY_ = v; }
  float gradientScale() const { return gradientScale_; }
  void setGradientScale(const float v) { gradientScale_ = v; }
  float gradientOffset() const { return gradientOffset_; }
  void setGradientOffset(const float v) { gradientOffset_ = v; }
 };

 ArtifactSolid2DLayer::Impl::Impl()
  : color_(1.0f, 1.0f, 1.0f, 1.0f)
 {

 }

 ArtifactSolid2DLayer::Impl::~Impl()
 {

 }

 ArtifactSolid2DLayer::ArtifactSolid2DLayer()
  : impl_(new Impl())
 {

 }

 ArtifactSolid2DLayer::~ArtifactSolid2DLayer()
 {
  delete impl_;
 }

 FloatColor ArtifactSolid2DLayer::color() const
 {
  return impl_->color();
 }

 void ArtifactSolid2DLayer::setColor(const FloatColor& color)
 {
  impl_->setColor(color);
 }

 ArtifactSolidFillType ArtifactSolid2DLayer::fillType() const
 {
  return impl_->fillType();
 }

 void ArtifactSolid2DLayer::setFillType(const ArtifactSolidFillType fillType)
 {
  impl_->setFillType(fillType);
 }

 bool ArtifactSolid2DLayer::isGradientEnabled() const
 {
  return fillType() != ArtifactSolidFillType::Solid;
 }

 FloatColor ArtifactSolid2DLayer::gradientStartColor() const
 {
  return impl_->gradientStartColor();
 }

 void ArtifactSolid2DLayer::setGradientStartColor(const FloatColor& color)
 {
  impl_->setGradientStartColor(color);
 }

 FloatColor ArtifactSolid2DLayer::gradientEndColor() const
 {
  return impl_->gradientEndColor();
 }

 void ArtifactSolid2DLayer::setGradientEndColor(const FloatColor& color)
 {
  impl_->setGradientEndColor(color);
 }

 float ArtifactSolid2DLayer::gradientAngleDegrees() const
 {
  return impl_->gradientAngleDegrees();
 }

 void ArtifactSolid2DLayer::setGradientAngleDegrees(const float degrees)
 {
  impl_->setGradientAngleDegrees(degrees);
 }

 bool ArtifactSolid2DLayer::gradientReverse() const
 {
  return impl_->gradientReverse();
 }

 void ArtifactSolid2DLayer::setGradientReverse(const bool reverse)
 {
  impl_->setGradientReverse(reverse);
 }

 float ArtifactSolid2DLayer::gradientCenterX() const
 {
  return impl_->gradientCenterX();
 }

 void ArtifactSolid2DLayer::setGradientCenterX(const float value)
 {
  impl_->setGradientCenterX(value);
 }

 float ArtifactSolid2DLayer::gradientCenterY() const
 {
  return impl_->gradientCenterY();
 }

 void ArtifactSolid2DLayer::setGradientCenterY(const float value)
 {
  impl_->setGradientCenterY(value);
 }

 float ArtifactSolid2DLayer::gradientScale() const
 {
  return impl_->gradientScale();
 }

 void ArtifactSolid2DLayer::setGradientScale(const float value)
 {
  impl_->setGradientScale(value);
 }

 float ArtifactSolid2DLayer::gradientOffset() const
 {
  return impl_->gradientOffset();
 }

 void ArtifactSolid2DLayer::setGradientOffset(const float value)
 {
  impl_->setGradientOffset(value);
 }

 void ArtifactSolid2DLayer::setSize(int width, int height)
 {
  setSourceSize(Size_2D(width, height));
 }

 QJsonObject ArtifactSolid2DLayer::toJson() const
 {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Solid);
  obj["solidWidth"] = sourceSize().width;
  obj["solidHeight"] = sourceSize().height;
  QJsonObject colorObj;
  const auto c = color();
  colorObj["r"] = c.r();
  colorObj["g"] = c.g();
  colorObj["b"] = c.b();
  colorObj["a"] = c.a();
  obj["solidColor"] = colorObj;
  obj["solidFillType"] = static_cast<int>(fillType());
  QJsonObject startObj;
  const auto start = gradientStartColor();
  startObj["r"] = start.r();
  startObj["g"] = start.g();
  startObj["b"] = start.b();
  startObj["a"] = start.a();
  obj["solidGradientStartColor"] = startObj;
  QJsonObject endObj;
  const auto end = gradientEndColor();
  endObj["r"] = end.r();
  endObj["g"] = end.g();
  endObj["b"] = end.b();
  endObj["a"] = end.a();
  obj["solidGradientEndColor"] = endObj;
  obj["solidGradientAngleDegrees"] = gradientAngleDegrees();
  obj["solidGradientReverse"] = gradientReverse();
  obj["solidGradientCenterX"] = gradientCenterX();
  obj["solidGradientCenterY"] = gradientCenterY();
  obj["solidGradientScale"] = gradientScale();
  obj["solidGradientOffset"] = gradientOffset();
  return obj;
 }

 void ArtifactSolid2DLayer::fromJsonProperties(const QJsonObject& obj)
 {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);
  if (obj.contains("solidWidth") || obj.contains("solidHeight")) {
   const int width = obj.value("solidWidth").toInt(sourceSize().width);
   const int height = obj.value("solidHeight").toInt(sourceSize().height);
   setSize(width, height);
  }
  if (obj.contains("solidColor") && obj["solidColor"].isObject()) {
   const auto colorObj = obj["solidColor"].toObject();
   setColor(FloatColor(static_cast<float>(colorObj.value("r").toDouble(1.0)),
                       static_cast<float>(colorObj.value("g").toDouble(1.0)),
                       static_cast<float>(colorObj.value("b").toDouble(1.0)),
                       static_cast<float>(colorObj.value("a").toDouble(1.0))));
  }
  setFillType(static_cast<ArtifactSolidFillType>(
      obj.value("solidFillType").toInt(static_cast<int>(ArtifactSolidFillType::Solid))));
  if (obj.contains("solidGradientStartColor") && obj["solidGradientStartColor"].isObject()) {
   const auto startObj = obj["solidGradientStartColor"].toObject();
   setGradientStartColor(FloatColor(static_cast<float>(startObj.value("r").toDouble(1.0)),
                                    static_cast<float>(startObj.value("g").toDouble(1.0)),
                                    static_cast<float>(startObj.value("b").toDouble(1.0)),
                                    static_cast<float>(startObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientEndColor") && obj["solidGradientEndColor"].isObject()) {
   const auto endObj = obj["solidGradientEndColor"].toObject();
   setGradientEndColor(FloatColor(static_cast<float>(endObj.value("r").toDouble(0.2)),
                                  static_cast<float>(endObj.value("g").toDouble(0.2)),
                                  static_cast<float>(endObj.value("b").toDouble(0.2)),
                                  static_cast<float>(endObj.value("a").toDouble(1.0))));
  }
  if (obj.contains("solidGradientAngleDegrees")) {
   setGradientAngleDegrees(static_cast<float>(obj.value("solidGradientAngleDegrees").toDouble(90.0)));
  }
  setGradientReverse(obj.value("solidGradientReverse").toBool(false));
  setGradientCenterX(static_cast<float>(obj.value("solidGradientCenterX").toDouble(0.5)));
  setGradientCenterY(static_cast<float>(obj.value("solidGradientCenterY").toDouble(0.5)));
  setGradientScale(static_cast<float>(obj.value("solidGradientScale").toDouble(1.0)));
  setGradientOffset(static_cast<float>(obj.value("solidGradientOffset").toDouble(0.0)));
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactSolid2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  const auto c = color();
  auto p = persistentLayerProperty(QStringLiteral("solid.color"),
                                   ArtifactCore::PropertyType::Color,
                                   QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()),
                                   -120);
  p->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  p->setValue(p->getColorValue());
  p->setAnimatable(true);
  p->setDisplayLabel(QStringLiteral("Color"));
  solidGroup.addProperty(p);

  auto fillTypeProp = persistentLayerProperty(QStringLiteral("solid.fillType"),
                                              ArtifactCore::PropertyType::Integer,
                                              static_cast<int>(fillType()),
                                              -119);
  fillTypeProp->setValue(static_cast<int>(fillType()));
  fillTypeProp->setDisplayLabel(QStringLiteral("Fill Mode"));
  fillTypeProp->setTooltip(QStringLiteral("Solid, linear, radial, conical, repeating, or mirrored gradient"));
  solidGroup.addProperty(fillTypeProp);

  const auto start = gradientStartColor();
  auto startProp = persistentLayerProperty(QStringLiteral("solid.gradientStartColor"),
                                           ArtifactCore::PropertyType::Color,
                                           QColor::fromRgbF(start.r(), start.g(), start.b(), start.a()),
                                           -118);
  startProp->setColorValue(QColor::fromRgbF(start.r(), start.g(), start.b(), start.a()));
  startProp->setValue(startProp->getColorValue());
  startProp->setDisplayLabel(QStringLiteral("開始色"));
  solidGroup.addProperty(startProp);

  const auto end = gradientEndColor();
  auto endProp = persistentLayerProperty(QStringLiteral("solid.gradientEndColor"),
                                         ArtifactCore::PropertyType::Color,
                                         QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()),
                                         -117);
  endProp->setColorValue(QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()));
  endProp->setValue(endProp->getColorValue());
  endProp->setDisplayLabel(QStringLiteral("終了色"));
  solidGroup.addProperty(endProp);

  auto angleProp = persistentLayerProperty(QStringLiteral("solid.gradientAngleDegrees"),
                                           ArtifactCore::PropertyType::Float,
                                           gradientAngleDegrees(),
                                           -116);
  angleProp->setValue(gradientAngleDegrees());
  angleProp->setDisplayLabel(QStringLiteral("角度"));
  angleProp->setTooltip(QStringLiteral("Linear gradient angle in degrees"));
  solidGroup.addProperty(angleProp);

  auto reverseProp = persistentLayerProperty(QStringLiteral("solid.gradientReverse"),
                                             ArtifactCore::PropertyType::Boolean,
                                             gradientReverse(),
                                             -115);
  reverseProp->setValue(gradientReverse());
  reverseProp->setDisplayLabel(QStringLiteral("反転"));
  solidGroup.addProperty(reverseProp);

  auto centerXProp = persistentLayerProperty(QStringLiteral("solid.gradientCenterX"),
                                             ArtifactCore::PropertyType::Float,
                                             gradientCenterX(),
                                             -114);
  centerXProp->setValue(gradientCenterX());
  centerXProp->setDisplayLabel(QStringLiteral("中心X"));
  solidGroup.addProperty(centerXProp);

  auto centerYProp = persistentLayerProperty(QStringLiteral("solid.gradientCenterY"),
                                             ArtifactCore::PropertyType::Float,
                                             gradientCenterY(),
                                             -113);
  centerYProp->setValue(gradientCenterY());
  centerYProp->setDisplayLabel(QStringLiteral("中心Y"));
  solidGroup.addProperty(centerYProp);

  auto scaleProp = persistentLayerProperty(QStringLiteral("solid.gradientScale"),
                                           ArtifactCore::PropertyType::Float,
                                           gradientScale(),
                                           -112);
  scaleProp->setValue(gradientScale());
  scaleProp->setDisplayLabel(QStringLiteral("拡大率"));
  solidGroup.addProperty(scaleProp);

  auto offsetProp = persistentLayerProperty(QStringLiteral("solid.gradientOffset"),
                                            ArtifactCore::PropertyType::Float,
                                            gradientOffset(),
                                            -111);
  offsetProp->setValue(gradientOffset());
  offsetProp->setDisplayLabel(QStringLiteral("オフセット"));
  solidGroup.addProperty(offsetProp);

  groups.push_back(solidGroup);
  return groups;
 }

 bool ArtifactSolid2DLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
  if (propertyPath == QStringLiteral("solid.color")) {
   const auto c = value.value<QColor>();
   setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.fillType")) {
   const int type = value.toInt();
   setFillType(type <= 0 ? ArtifactSolidFillType::Solid
                         : type == 2 ? ArtifactSolidFillType::RadialGradient
                         : type == 3 ? ArtifactSolidFillType::ConicalGradient
                         : type == 4 ? ArtifactSolidFillType::RepeatingGradient
                         : type == 5 ? ArtifactSolidFillType::MirroredGradient
                                     : ArtifactSolidFillType::LinearGradient);
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientStartColor")) {
   const auto c = value.value<QColor>();
   setGradientStartColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientEndColor")) {
   const auto c = value.value<QColor>();
   setGradientEndColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientAngleDegrees")) {
   setGradientAngleDegrees(value.toFloat());
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientReverse")) {
   setGradientReverse(value.toBool());
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientCenterX")) {
   setGradientCenterX(value.toFloat());
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientCenterY")) {
   setGradientCenterY(value.toFloat());
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientScale")) {
   setGradientScale(value.toFloat());
   Q_EMIT changed();
   return true;
  }
  if (propertyPath == QStringLiteral("solid.gradientOffset")) {
   setGradientOffset(value.toFloat());
   Q_EMIT changed();
   return true;
  }
  return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
 }

void ArtifactSolid2DLayer::draw(ArtifactIRenderer* renderer)
{
 if (!renderer) return;
 auto size = this->sourceSize();
 const QMatrix4x4 baseTransform = getGlobalTransform4x4();
 drawWithClonerEffect(this, baseTransform, [renderer, size, this](const QMatrix4x4& transform, float weight) {
  if (impl_->fillType() != ArtifactSolidFillType::Solid) {
   const QImage gradientImage = ArtifactSolidGradientUtil::makeSolidGradientImage(
       QSize(size.width, size.height),
       QColor::fromRgbF(impl_->gradientStartColor().r(), impl_->gradientStartColor().g(),
                  impl_->gradientStartColor().b(),
                  impl_->gradientStartColor().a() * this->opacity() * weight),
       QColor::fromRgbF(impl_->gradientEndColor().r(), impl_->gradientEndColor().g(),
                  impl_->gradientEndColor().b(),
                  impl_->gradientEndColor().a() * this->opacity() * weight),
       static_cast<int>(impl_->fillType()),
       impl_->gradientAngleDegrees(),
       impl_->gradientReverse(),
       impl_->gradientCenterX(),
       impl_->gradientCenterY(),
       impl_->gradientScale(),
       impl_->gradientOffset());
   renderer->drawSpriteTransformed(0.0f, 0.0f,
                                   static_cast<float>(size.width),
                                   static_cast<float>(size.height),
                                   transform,
                                   gradientImage,
                                   1.0f);
   return;
  }
  const FloatColor src = impl_->color();
  const FloatColor color(src.r(), src.g(), src.b(), src.a() * this->opacity() * weight);
  renderer->drawSolidRectTransformed(0.0f, 0.0f,
                                     static_cast<float>(size.width),
                                     static_cast<float>(size.height),
                                     transform,
                                     color,
                                     1.0f);
 });
}

}
