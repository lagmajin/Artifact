module;

module Artifact.Layer.Composition;

import std;
import Artifact.Composition.Abstract;


namespace Artifact {

 class ArtifactCompositionLayer::Impl
 {
 private:
  CompositionID id_;
 public:
  Impl();
  ~Impl();
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

 }

};