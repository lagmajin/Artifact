module;

#include <wobjectcpp.h>

module Artifact.Layers.Abstract;
#include <wobjectimpl.h>

namespace Artifact {

 W_OBJECT_IMPL(ArtifactAbstractLayer)

  class ArtifactAbstractLayer::Impl {
  private:
   std::type_index index_ = typeid(void);
   Id id;

  public:

 };

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

 Id ArtifactAbstractLayer::layerId() const
 {

  return Id();
 }


};
