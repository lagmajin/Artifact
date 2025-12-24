module;

module Artifact.Layer.Shape;



namespace Artifact
{
 class ArtifactShapeLayer::Impl {
 private:

 public:
  Impl();
  ~Impl();
  void addShape();
 };

 ArtifactShapeLayer::Impl::Impl()
 {

 }

 ArtifactShapeLayer::Impl::~Impl()
 {

 }


 ArtifactShapeLayer::ArtifactShapeLayer() :impl_(new Impl())
 {

 }

 ArtifactShapeLayer::~ArtifactShapeLayer()
 {
  delete impl_;
 }

 bool ArtifactShapeLayer::isShapeLayer() const
 {

  return true;
 }

	

};