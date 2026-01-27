module;

module Artifact.Layer.Composition;

import std;
import Artifact.Composition.Abstract;


namespace Artifact {

 class ArtifactCompositionLayer::Impl
 {
 private:
  
 public:
  Impl();
  ~Impl();
  CompositionID id_;
 };

 ArtifactCompositionLayer::ArtifactCompositionLayer()
 {

 }

 ArtifactCompositionLayer::~ArtifactCompositionLayer()
 {

 }

 CompositionID ArtifactCompositionLayer::sourceCompositionId() const
 {
  return impl_->id_;
 }

 void ArtifactCompositionLayer::setCompositionId(const CompositionID& id)
 {
  impl_->id_ = id;
 }



};