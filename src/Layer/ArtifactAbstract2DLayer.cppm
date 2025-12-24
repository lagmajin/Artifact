module;
module Artifact.Layers.Abstract._2D;

import std;
import Artifact.Layer.Abstract;
import Animation.Transform2D;


namespace Artifact {

 class ArtifactAbstract2DLayer::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactAbstract2DLayer::Impl::Impl()
 {

 }

 ArtifactAbstract2DLayer::Impl::~Impl()
 {

 }

 ArtifactAbstract2DLayer::ArtifactAbstract2DLayer() :impl_(new Impl())
 {

 }

 ArtifactAbstract2DLayer::~ArtifactAbstract2DLayer()
 {
  delete impl_;
 }

};