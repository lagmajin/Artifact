module;

#include <QColor>
#include <QVariant>

module Artifact.Layers.SolidImage;

import std;
import Artifact.Render.IRenderer;
import Property.Abstract;
import Property.Group;

namespace Artifact
{
 ArtifactSolidImageLayerSettings::ArtifactSolidImageLayerSettings() = default;
 ArtifactSolidImageLayerSettings::~ArtifactSolidImageLayerSettings() = default;

 class ArtifactSolidImageLayer::Impl
 {
 public:
  FloatColor color_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
 };

 ArtifactSolidImageLayer::ArtifactSolidImageLayer()
  : impl_(new Impl())
 {
 }

 ArtifactSolidImageLayer::~ArtifactSolidImageLayer()
 {
  delete impl_;
 }

 FloatColor ArtifactSolidImageLayer::color() const
 {
  return impl_->color_;
 }

 void ArtifactSolidImageLayer::setColor(const FloatColor& color)
 {
  impl_->color_ = color;
 }

 void ArtifactSolidImageLayer::setSize(const int width, const int height)
 {
  setSourceSize(Size_2D(width, height));
 }

 std::vector<ArtifactCore::PropertyGroup> ArtifactSolidImageLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  auto property = std::make_shared<ArtifactCore::AbstractProperty>();
  property->setName(QStringLiteral("solid.color"));
  property->setType(ArtifactCore::PropertyType::Color);
  const auto c = color();
  property->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  property->setValue(property->getColorValue());
  property->setDisplayPriority(-120);
  property->setAnimatable(true);  // キーフレーム可能に設定
  solidGroup.addProperty(property);

  groups.push_back(solidGroup);
  return groups;
 }

 bool ArtifactSolidImageLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
 {
  if (propertyPath == QStringLiteral("solid.color")) {
   const auto c = value.value<QColor>();
   setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
   Q_EMIT changed();
   return true;
  }
  return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
 }

 void ArtifactSolidImageLayer::draw(ArtifactIRenderer* renderer)
 {
  const auto size = sourceSize();
  renderer->drawSolidRect(Detail::float2(0.0f, 0.0f),
                          Detail::float2(static_cast<float>(size.width),
                                         static_cast<float>(size.height)),
                          impl_->color_);
 }
}
