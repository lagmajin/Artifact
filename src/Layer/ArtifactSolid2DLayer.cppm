module;
#include <utility>
#include <QList>
#include <QColor>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QVariant>
#include <Layer/ArtifactCloneEffectSupport.hpp>

module Artifact.Layer.Solid2D;

import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;

namespace Artifact
{


 class ArtifactSolid2DLayer::Impl
 {
 private:
  FloatColor color_;

 public:
  Impl();
  ~Impl();
  FloatColor color() const { return color_; }
  void setColor(const FloatColor& c) { color_ = c; }
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

 void ArtifactSolid2DLayer::setSize(int width, int height)
 {
  setSourceSize(Size_2D(width, height));
 }

 QJsonObject ArtifactSolid2DLayer::toJson() const
 {
  QJsonObject obj = ArtifactAbstractLayer::toJson();
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
  return obj;
 }

 void ArtifactSolid2DLayer::fromJsonProperties(const QJsonObject& obj)
 {
  ArtifactAbstractLayer::fromJsonProperties(obj);
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
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactSolid2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  const auto c = color();
  auto p = persistentLayerProperty(QStringLiteral("solid.color"),
                                   ArtifactCore::PropertyType::Color,
                                   QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()),
                                   -120);
  p->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  p->setValue(p->getColorValue());
  p->setAnimatable(true);
  solidGroup.addProperty(p);

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
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

void ArtifactSolid2DLayer::draw(ArtifactIRenderer* renderer)
{
 if (!renderer) return;
 auto size = this->sourceSize();
 const QMatrix4x4 baseTransform = getGlobalTransform4x4();
 drawWithClonerEffect(this, baseTransform, [renderer, size, this](const QMatrix4x4& transform, float weight) {
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
