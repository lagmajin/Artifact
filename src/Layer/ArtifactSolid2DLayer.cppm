module;
#include <QList>

module Artifact.Layer.Solid2D;

import Artifact.Render.IRenderer;

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

void ArtifactSolid2DLayer::draw(ArtifactIRenderer* renderer)

 {
  auto size = sourceSize();
  renderer->drawSolidRect(Detail::float2(0.0f, 0.0f),
                          Detail::float2(static_cast<float>(size.width),
                                         static_cast<float>(size.height)),
                          impl_->color());
 }

}
