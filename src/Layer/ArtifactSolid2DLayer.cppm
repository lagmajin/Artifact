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
#include <algorithm>
#include <cmath>

module Artifact.Layer.Solid2D;

import Artifact.Layer.CloneEffectSupport;

import Artifact.Layers.Abstract._2D;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;
import Time.Rational;

namespace Artifact
{
namespace {
ArtifactCore::RationalTime solidTimelineTime(const ArtifactSolid2DLayer* layer) {
  if (auto* composition = layer
          ? static_cast<ArtifactAbstractComposition*>(layer->composition())
          : nullptr) {
    const double fps = composition->frameRate().framerate();
    return ArtifactCore::RationalTime(composition->framePosition().framePosition(),
                        std::max<int64_t>(
                            1, static_cast<int64_t>(std::llround(
                                   std::isfinite(fps) && fps > 0.0 ? fps
                                                                    : 30.0))));
  }
  return ArtifactCore::RationalTime(layer ? layer->currentFrame() : 0, 30);
}

QVariant animatedSolidGradientValue(const ArtifactSolid2DLayer* layer,
                                    const QString& path) {
  if (!layer) return {};
  const auto property = layer->getProperty(path);
  if (!property || property->getKeyFrames().empty()) return {};
  return property->interpolateValue(solidTimelineTime(layer));
}

FloatColor animatedSolidGradientColor(const ArtifactSolid2DLayer* layer,
                                      const QString& path,
                                      const FloatColor& fallback) {
  const QVariant value = animatedSolidGradientValue(layer, path);
  if (!value.canConvert<QColor>()) return fallback;
  const QColor color = value.value<QColor>();
  return FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

float animatedSolidGradientFloat(const ArtifactSolid2DLayer* layer,
                                 const QString& path, const float fallback) {
  const QVariant value = animatedSolidGradientValue(layer, path);
  const double result = value.isValid() ? value.toDouble() : fallback;
  return std::isfinite(result) ? static_cast<float>(result) : fallback;
}

bool animatedSolidGradientBool(const ArtifactSolid2DLayer* layer,
                               const QString& path, const bool fallback) {
  const QVariant value = animatedSolidGradientValue(layer, path);
  return value.isValid() ? value.toBool() : fallback;
}
} // namespace
 
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
  void setColor(const FloatColor& c) {
    const auto safe = [](float value, float fallback) {
      return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };
    color_ = FloatColor(safe(c.r(), 0.0f), safe(c.g(), 0.0f),
                        safe(c.b(), 0.0f), safe(c.a(), 1.0f));
  }
  ArtifactSolidFillType fillType() const { return fillType_; }
  void setFillType(const ArtifactSolidFillType v) {
    fillType_ = static_cast<ArtifactSolidFillType>(std::clamp(
        static_cast<int>(v), 0, 5));
  }
  FloatColor gradientStartColor() const { return gradientStartColor_; }
  void setGradientStartColor(const FloatColor& c) {
    const auto safe = [](float value, float fallback) {
      return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };
    gradientStartColor_ = FloatColor(safe(c.r(), 0.0f), safe(c.g(), 0.0f),
                                     safe(c.b(), 0.0f), safe(c.a(), 1.0f));
  }
  FloatColor gradientEndColor() const { return gradientEndColor_; }
  void setGradientEndColor(const FloatColor& c) {
    const auto safe = [](float value, float fallback) {
      return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };
    gradientEndColor_ = FloatColor(safe(c.r(), 0.0f), safe(c.g(), 0.0f),
                                   safe(c.b(), 0.0f), safe(c.a(), 1.0f));
  }
  float gradientAngleDegrees() const { return gradientAngleDegrees_; }
  void setGradientAngleDegrees(const float v) {
    gradientAngleDegrees_ = std::isfinite(v) ? v : 90.0f;
  }
  bool gradientReverse() const { return gradientReverse_; }
  void setGradientReverse(const bool v) { gradientReverse_ = v; }
  float gradientCenterX() const { return gradientCenterX_; }
  void setGradientCenterX(const float v) {
    gradientCenterX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
  }
  float gradientCenterY() const { return gradientCenterY_; }
  void setGradientCenterY(const float v) {
    gradientCenterY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
  }
  float gradientScale() const { return gradientScale_; }
  void setGradientScale(const float v) {
    gradientScale_ = std::isfinite(v)
        ? std::clamp(v, 0.0001f, 1000000.0f)
        : 1.0f;
  }
  float gradientOffset() const { return gradientOffset_; }
  void setGradientOffset(const float v) {
    gradientOffset_ = std::isfinite(v)
        ? std::clamp(v, -1000000.0f, 1000000.0f)
        : 0.0f;
  }
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
  return animatedSolidGradientColor(
      this, QStringLiteral("solid.gradientStartColor"),
      impl_->gradientStartColor());
 }

 void ArtifactSolid2DLayer::setGradientStartColor(const FloatColor& color)
 {
  impl_->setGradientStartColor(color);
 }

 FloatColor ArtifactSolid2DLayer::gradientEndColor() const
 {
  return animatedSolidGradientColor(
      this, QStringLiteral("solid.gradientEndColor"),
      impl_->gradientEndColor());
 }

 void ArtifactSolid2DLayer::setGradientEndColor(const FloatColor& color)
 {
  impl_->setGradientEndColor(color);
 }

 float ArtifactSolid2DLayer::gradientAngleDegrees() const
 {
  return animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientAngleDegrees"),
      impl_->gradientAngleDegrees());
 }

 void ArtifactSolid2DLayer::setGradientAngleDegrees(const float degrees)
 {
  impl_->setGradientAngleDegrees(degrees);
 }

 bool ArtifactSolid2DLayer::gradientReverse() const
 {
  return animatedSolidGradientBool(this, QStringLiteral("solid.gradientReverse"),
                                   impl_->gradientReverse());
 }

 void ArtifactSolid2DLayer::setGradientReverse(const bool reverse)
 {
  impl_->setGradientReverse(reverse);
 }

 float ArtifactSolid2DLayer::gradientCenterX() const
 {
  return animatedSolidGradientFloat(this, QStringLiteral("solid.gradientCenterX"),
                                    impl_->gradientCenterX());
 }

 void ArtifactSolid2DLayer::setGradientCenterX(const float value)
 {
  impl_->setGradientCenterX(value);
 }

 float ArtifactSolid2DLayer::gradientCenterY() const
 {
  return animatedSolidGradientFloat(this, QStringLiteral("solid.gradientCenterY"),
                                    impl_->gradientCenterY());
 }

 void ArtifactSolid2DLayer::setGradientCenterY(const float value)
 {
  impl_->setGradientCenterY(value);
 }

 float ArtifactSolid2DLayer::gradientScale() const
 {
  return animatedSolidGradientFloat(this, QStringLiteral("solid.gradientScale"),
                                    impl_->gradientScale());
 }

 void ArtifactSolid2DLayer::setGradientScale(const float value)
 {
  impl_->setGradientScale(value);
 }

 float ArtifactSolid2DLayer::gradientOffset() const
 {
  return animatedSolidGradientFloat(this, QStringLiteral("solid.gradientOffset"),
                                    impl_->gradientOffset());
 }

 void ArtifactSolid2DLayer::setGradientOffset(const float value)
 {
  impl_->setGradientOffset(value);
 }

void ArtifactSolid2DLayer::setSize(int width, int height)
{
  setSourceSize(Size_2D(std::clamp(width, 1, 16384),
                        std::clamp(height, 1, 16384)));
}

 QJsonObject ArtifactSolid2DLayer::toJson() const
 {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Solid);
  const auto safeSource = sourceSize();
  obj["solidWidth"] = std::clamp(safeSource.width, 1, 16384);
  obj["solidHeight"] = std::clamp(safeSource.height, 1, 16384);
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
  fillTypeProp->setHardRange(0.0, 5.0);
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
  startProp->setAnimatable(true);
  solidGroup.addProperty(startProp);

  const auto end = gradientEndColor();
  auto endProp = persistentLayerProperty(QStringLiteral("solid.gradientEndColor"),
                                         ArtifactCore::PropertyType::Color,
                                         QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()),
                                         -117);
  endProp->setColorValue(QColor::fromRgbF(end.r(), end.g(), end.b(), end.a()));
  endProp->setValue(endProp->getColorValue());
  endProp->setDisplayLabel(QStringLiteral("終了色"));
  endProp->setAnimatable(true);
  solidGroup.addProperty(endProp);

  auto angleProp = persistentLayerProperty(QStringLiteral("solid.gradientAngleDegrees"),
                                           ArtifactCore::PropertyType::Float,
                                           gradientAngleDegrees(),
                                           -116);
  angleProp->setValue(gradientAngleDegrees());
  angleProp->setDisplayLabel(QStringLiteral("角度"));
  angleProp->setTooltip(QStringLiteral("Linear gradient angle in degrees"));
  angleProp->setAnimatable(true);
  solidGroup.addProperty(angleProp);

  auto reverseProp = persistentLayerProperty(QStringLiteral("solid.gradientReverse"),
                                             ArtifactCore::PropertyType::Boolean,
                                             gradientReverse(),
                                             -115);
  reverseProp->setValue(gradientReverse());
  reverseProp->setDisplayLabel(QStringLiteral("反転"));
  reverseProp->setAnimatable(true);
  solidGroup.addProperty(reverseProp);

  auto centerXProp = persistentLayerProperty(QStringLiteral("solid.gradientCenterX"),
                                             ArtifactCore::PropertyType::Float,
                                             gradientCenterX(),
                                             -114);
  centerXProp->setHardRange(0.0, 1.0);
  centerXProp->setValue(gradientCenterX());
  centerXProp->setDisplayLabel(QStringLiteral("中心X"));
  centerXProp->setAnimatable(true);
  solidGroup.addProperty(centerXProp);

  auto centerYProp = persistentLayerProperty(QStringLiteral("solid.gradientCenterY"),
                                             ArtifactCore::PropertyType::Float,
                                             gradientCenterY(),
                                             -113);
  centerYProp->setHardRange(0.0, 1.0);
  centerYProp->setValue(gradientCenterY());
  centerYProp->setDisplayLabel(QStringLiteral("中心Y"));
  centerYProp->setAnimatable(true);
  solidGroup.addProperty(centerYProp);

  auto scaleProp = persistentLayerProperty(QStringLiteral("solid.gradientScale"),
                                           ArtifactCore::PropertyType::Float,
                                           gradientScale(),
                                           -112);
  scaleProp->setHardRange(0.0001, 1000000.0);
  scaleProp->setValue(gradientScale());
  scaleProp->setDisplayLabel(QStringLiteral("拡大率"));
  scaleProp->setAnimatable(true);
  solidGroup.addProperty(scaleProp);

  auto offsetProp = persistentLayerProperty(QStringLiteral("solid.gradientOffset"),
                                            ArtifactCore::PropertyType::Float,
                                            gradientOffset(),
                                            -111);
  offsetProp->setHardRange(-1000000.0, 1000000.0);
  offsetProp->setValue(gradientOffset());
  offsetProp->setDisplayLabel(QStringLiteral("オフセット"));
  offsetProp->setAnimatable(true);
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
 const auto sourceSize = this->sourceSize();
 const Size_2D size(std::clamp(sourceSize.width, 1, 16384),
                    std::clamp(sourceSize.height, 1, 16384));
 const QMatrix4x4 baseTransform = getGlobalTransform4x4();
  const FloatColor gradientStart = animatedSolidGradientColor(
      this, QStringLiteral("solid.gradientStartColor"),
      impl_->gradientStartColor());
  const FloatColor gradientEnd = animatedSolidGradientColor(
      this, QStringLiteral("solid.gradientEndColor"),
      impl_->gradientEndColor());
  const float gradientAngle = animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientAngleDegrees"),
      impl_->gradientAngleDegrees());
  const bool gradientReverse = animatedSolidGradientBool(
      this, QStringLiteral("solid.gradientReverse"),
      impl_->gradientReverse());
  const float gradientCenterX = animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientCenterX"), impl_->gradientCenterX());
  const float gradientCenterY = animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientCenterY"), impl_->gradientCenterY());
  const float gradientScale = animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientScale"), impl_->gradientScale());
  const float gradientOffset = animatedSolidGradientFloat(
      this, QStringLiteral("solid.gradientOffset"), impl_->gradientOffset());
  drawWithClonerEffect(this, baseTransform,
      [renderer, size, this, gradientStart, gradientEnd, gradientAngle,
       gradientReverse, gradientCenterX, gradientCenterY, gradientScale,
       gradientOffset](const QMatrix4x4& transform, float weight) {
   if (impl_->fillType() != ArtifactSolidFillType::Solid) {
   renderer->drawGradientRectTransformed(
       0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height),
       transform, gradientStart, gradientEnd, static_cast<int>(impl_->fillType()),
       gradientAngle, gradientReverse, gradientCenterX, gradientCenterY,
       gradientScale, gradientOffset, this->opacity() * weight);
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
