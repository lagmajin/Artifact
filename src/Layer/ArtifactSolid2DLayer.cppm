module;
#include <QList>
#include <QColor>
#include <QVariant>

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

 std::vector<ArtifactCore::PropertyGroup> ArtifactSolid2DLayer::getLayerPropertyGroups() const
 {
  auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup solidGroup(QStringLiteral("Solid"));

  auto p = std::make_shared<ArtifactCore::AbstractProperty>();
  p->setName(QStringLiteral("solid.color"));
  p->setType(ArtifactCore::PropertyType::Color);
  const auto c = color();
  p->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  p->setValue(p->getColorValue());
  p->setDisplayPriority(-120);
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
 renderer->drawSolidRect(0.0f, 0.0f, 
                         static_cast<float>(size.width),
                         static_cast<float>(size.height),
                         impl_->color(),
                         this->opacity());
}

}
