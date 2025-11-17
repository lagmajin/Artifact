module;

#include <wobjectcpp.h>
#include <wobjectimpl.h>
module Artifact.Layers.Abstract;

import Utils;
import Layer.State;

namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAbstractLayer)

  class ArtifactAbstractLayer::Impl {
  private:
   
   
   Id id;
   LayerState state_;
  public:
   Impl();
   ~Impl();
   std::type_index type_index_ = typeid(void);
 };

  ArtifactAbstractLayer::Impl::Impl()
  {
  }

  ArtifactAbstractLayer::Impl::~Impl()
  {
  }

  ArtifactAbstractLayer::ArtifactAbstractLayer():impl_(new Impl())
 {

 }

 ArtifactAbstractLayer::~ArtifactAbstractLayer()
 {
  delete impl_;
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

};
