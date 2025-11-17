module;


module Artifact.Layers.Null;
//import ArtifactNullLayer;


namespace Artifact {

 class ArtifactNullLayer::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactNullLayer::Impl::Impl()
 {

 }

 ArtifactNullLayer::Impl::~Impl()
 {

 }

 ArtifactNullLayer::ArtifactNullLayer():impl_(new Impl())
 {

 }

 ArtifactNullLayer::~ArtifactNullLayer()
 {
  delete impl_;
 }

};