module;


module Artifact.Layer.Null;
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

 void ArtifactNullLayer::draw()
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 bool ArtifactNullLayer::isAdjustmentLayer() const
 {
  return false;
 }

 bool ArtifactNullLayer::isNullLayer() const
 {
 
  return true;
 }

};