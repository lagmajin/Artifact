module;

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QPointF>
#include <QRectF>
#include <wobjectcpp.h>
#include <wobjectimpl.h>
module Artifact.Layer.Abstract;

import std;
import Utils;
import Layer.State;
import Animation.Transform2D;
import Frame.Position;
import Artifact.Layer.Settings;


namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAbstractLayer)

  class ArtifactAbstractLayer::Impl {
  private:
   
   bool is3D_ = true;
   Id id;
   LayerState state_;
   //FramePosition framePosition_
   
  public:
   Impl();
   ~Impl();
   std::type_index type_index_ = typeid(void);
   void goToStartFrame();
   void goToEndFrame();
   void goToNextFrame();
   void goToPrevFrame();
 	
   bool is3D() const;
   AnimatableTransform3D transform_;
   AnimatableTransform2D transform2d_;
   Size_2D sourceSize_;
 };

  ArtifactAbstractLayer::Impl::Impl()
  {
  }

  ArtifactAbstractLayer::Impl::~Impl()
  {
  }

  void ArtifactAbstractLayer::Impl::goToStartFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToEndFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToNextFrame()
  {

  }

  void ArtifactAbstractLayer::Impl::goToPrevFrame()
  {

  }

  bool ArtifactAbstractLayer::Impl::is3D() const
  {
   return is3D_;
  }

  ArtifactAbstractLayer::ArtifactAbstractLayer():impl_(new Impl())
 {

 }

 ArtifactAbstractLayer::~ArtifactAbstractLayer()
 {
  delete impl_;
 }
 
 void ArtifactAbstractLayer::setVisible(bool visible/*=true*/)
 {
  //impl_->state_.setVisible(visible);
 }

 void ArtifactAbstractLayer::Show()
 {

 }

 void ArtifactAbstractLayer::Hide()
 {


 }

 LAYER_BLEND_TYPE ArtifactAbstractLayer::layerBlendType() const
 {

  return LAYER_BLEND_TYPE::BLEND_ADD;
 }

 void ArtifactAbstractLayer::setBlendMode(LAYER_BLEND_TYPE type)
 {

 }

 LayerID ArtifactAbstractLayer::id() const
 {

  return LayerID();
 }

 QString ArtifactAbstractLayer::layerName() const
 {

  return QString();
 }

 UniString ArtifactAbstractLayer::className() const
{
  return QString("");
 }

 void ArtifactAbstractLayer::setLayerName(const QString& name)
 {

 }

 std::type_index ArtifactAbstractLayer::type_index() const
 {
  return impl_->type_index_;
 }

 void ArtifactAbstractLayer::goToStartFrame()
 {

 }

 void ArtifactAbstractLayer::goToEndFrame()
 {

 }

 void ArtifactAbstractLayer::goToNextFrame()
 {

 }

 void ArtifactAbstractLayer::goToPrevFrame()
 {

 }

 void ArtifactAbstractLayer::goToFrame(int64_t frameNumber /*= 0*/)
 {

 }
 bool ArtifactAbstractLayer::isAdjustmentLayer() const
 {
  return true;
 }
 void ArtifactAbstractLayer::setAdjustmentLayer(bool isAdjustment)
 {
  //adjustmentLayer = isAdjustment;
 }

 bool ArtifactAbstractLayer::isVisible() const
 {
  return false;
 }

 void ArtifactAbstractLayer::setParentById(LayerID& id)
 {

 }

 bool ArtifactAbstractLayer::is3D() const
 {
  return false;
 }

 void ArtifactAbstractLayer::setTimeRemapEnabled(bool)
 {

 }

 void ArtifactAbstractLayer::setTimeRemapKey(int64_t compFrame, double sourceFrame)
 {

 }

 bool ArtifactAbstractLayer::isTimeRemapEnabled() const
 {
  return false;
 }

 bool ArtifactAbstractLayer::isNullLayer() const
 {
  return false;
 }

 bool ArtifactAbstractLayer::hasAudio() const
 {
  return true;
 }

 bool ArtifactAbstractLayer::hasVideo() const
 {
  return true;
 }

Size_2D ArtifactAbstractLayer::sourceSize() const
{
 return impl_->sourceSize_;
}

void ArtifactAbstractLayer::setSourceSize(const Size_2D& size)
{
 impl_->sourceSize_ = size;
}

Size_2D ArtifactAbstractLayer::aabb() const
{
 const auto bounds = transformedBoundingBox();
 if (bounds.width() <= 0 || bounds.height() <= 0) {
  return Size_2D();
 }
 Size_2D result;
 result.width = static_cast<int>(std::ceil(bounds.width()));
 result.height = static_cast<int>(std::ceil(bounds.height()));
 return result;
}

QRectF ArtifactAbstractLayer::transformedBoundingBox() const
{
 const auto size = sourceSize();
 if (size.isEmpty()) {
  return QRectF();
 }

 const float width = static_cast<float>(size.width);
 const float height = static_cast<float>(size.height);
 const float centerX = width * 0.5f;
 const float centerY = height * 0.5f;

 const float scaleX = transform3D().scaleX();
 const float scaleY = transform3D().scaleY();
 const float rotationDeg = transform3D().rotation();
 const float translateX = transform3D().positionX();
 const float translateY = transform3D().positionY();

 const float radians = rotationDeg * (3.14159265358979323846f / 180.0f);
 const float cosA = std::cos(radians);
 const float sinA = std::sin(radians);

 const std::array<QPointF, 4> corners = {
  QPointF(0.0f, 0.0f),
  QPointF(width, 0.0f),
  QPointF(width, height),
  QPointF(0.0f, height)
 };

 float minX = std::numeric_limits<float>::max();
 float minY = std::numeric_limits<float>::max();
 float maxX = std::numeric_limits<float>::lowest();
 float maxY = std::numeric_limits<float>::lowest();

 for (const auto& corner : corners) {
  QPointF pt = corner;
  pt -= QPointF(centerX, centerY);
  pt.setX(pt.x() * scaleX);
  pt.setY(pt.y() * scaleY);
  const float rotatedX = pt.x() * cosA - pt.y() * sinA;
  const float rotatedY = pt.x() * sinA + pt.y() * cosA;
  pt.setX(rotatedX + centerX + translateX);
  pt.setY(rotatedY + centerY + translateY);
  if (pt.x() < minX) {
   minX = pt.x();
  }
  if (pt.x() > maxX) {
   maxX = pt.x();
  }
  if (pt.y() < minY) {
   minY = pt.y();
  }
  if (pt.y() > maxY) {
   maxY = pt.y();
  }
 }

 return QRectF(minX, minY, maxX - minX, maxY - minY);
}

AnimatableTransform2D& ArtifactAbstractLayer::transform2D()
{
 return impl_->transform2d_;
}

const AnimatableTransform2D& ArtifactAbstractLayer::transform2D() const
{
 return impl_->transform2d_;
}

AnimatableTransform3D& ArtifactAbstractLayer::transform3D()
{
 return impl_->transform_;
}

const AnimatableTransform3D& ArtifactAbstractLayer::transform3D() const
{
 return impl_->transform_;
}

QJsonObject ArtifactAbstractLayer::toJson() const
{



 return QJsonObject();
}

 ArtifactAbstractLayerPtr ArtifactAbstractLayer::fromJson(const QJsonObject& obj)
 {
   return ArtifactAbstractLayerPtr();
 }

};
