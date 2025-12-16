module;

#include <wobjectcpp.h>
#include <wobjectimpl.h>
module Artifact.Layers.Abstract;

import std;
import Utils;
import Layer.State;
import Animation.Transform2D;
import Frame.Position;

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

 QString ArtifactAbstractLayer::className() const
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

 AnimatableTransform3D& ArtifactAbstractLayer::transform3D()
 {
  return impl_->transform_;
 }

};
